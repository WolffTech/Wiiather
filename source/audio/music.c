#include "music.h"

#include <asndlib.h>
#include <dirent.h>
#include <mp3player.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define MAX_TRACKS 64

static char folder[256];
static char tracks[MAX_TRACKS][128];
static bool skipped[MAX_TRACKS];
static size_t track_count;
// The play order for this pass through the list, and the next position in it.
static size_t order[MAX_TRACKS], next_position;
static MusicOrder current_order;
static FILE *playing_file;
static int playing = -1;
static bool enabled;
// Set when no track would play, so music_update doesn't retry every frame.
static bool stalled;

static int compare_names(const void *a, const void *b)
{
	return strcasecmp(a, b);
}

// Starts a new pass, shuffled unless playing in order.
static void new_pass(void)
{
	for (size_t i = 0; i < track_count; i++)
		order[i] = i;
	if (current_order == MUSIC_SHUFFLE) {
		for (size_t i = track_count - 1; i > 0; i--) {
			size_t j = (size_t)rand() % (i + 1);
			size_t tmp = order[i];
			order[i] = order[j];
			order[j] = tmp;
		}
	}
	next_position = 0;
}

// Starts a new pass that continues after `track`, or from the top for -1.
static void continue_after(int track)
{
	new_pass();
	for (size_t i = 0; track >= 0 && i < track_count; i++)
		if (order[i] == (size_t)track)
			next_position = i + 1;
}

static s32 read_file(void *file, void *dst, s32 size)
{
	return (s32)fread(dst, 1, (size_t)size, file);
}

static void stop(void)
{
	MP3Player_Stop();
	if (playing_file) {
		fclose(playing_file);
		playing_file = NULL;
	}
	playing = -1;
}

static bool play_track(size_t track)
{
	stop();
	char path[sizeof folder + sizeof tracks[0] + 1];
	snprintf(path, sizeof path, "%.255s/%.127s", folder, tracks[track]);
	playing_file = fopen(path, "rb");
	if (!playing_file)
		return false;
	MP3Player_PlayFile(playing_file, read_file, NULL);
	playing = (int)track;
	return true;
}

static void play_next(void)
{
	stop();
	// Two passes' worth of tries, since a new pass can reorder the tracks.
	for (size_t tries = 0; tries < 2 * track_count; tries++) {
		if (next_position == track_count)
			new_pass();
		size_t track = order[next_position++];
		if (!skipped[track] && play_track(track))
			return;
	}
	stalled = true;
}

void music_start(const char *dir, const Config *config)
{
	snprintf(folder, sizeof folder, "%s", dir);
	DIR *d = opendir(dir);
	if (!d)
		return;
	struct dirent *e;
	while ((e = readdir(d)) && track_count < MAX_TRACKS) {
		size_t len = strlen(e->d_name);
		if (len > 4 && len < sizeof tracks[0] && strcasecmp(e->d_name + len - 4, ".mp3") == 0)
			snprintf(tracks[track_count++], sizeof tracks[0], "%s", e->d_name);
	}
	closedir(d);
	if (track_count == 0)
		return;
	qsort(tracks, track_count, sizeof tracks[0], compare_names);

	srand((unsigned)time(NULL));
	ASND_Init();
	MP3Player_Init();
	current_order = config->music_order;
	new_pass();
	music_configure(config);
}

void music_configure(const Config *config)
{
	if (track_count == 0)
		return;
	for (size_t i = 0; i < track_count; i++)
		skipped[i] = !config_track_enabled(config, tracks[i]);
	MP3Player_Volume(255 * config->music_volume / 100);
	if (config->music_order != current_order) {
		current_order = config->music_order;
		continue_after(playing);
	}
	stalled = false;
	if (config->music && !enabled) {
		enabled = true;
		play_next();
	} else if (!config->music && enabled) {
		enabled = false;
		stop();
	}
}

void music_update(void)
{
	if (enabled && !stalled && !MP3Player_IsPlaying())
		play_next();
}

size_t music_track_count(void)
{
	return track_count;
}

const char *music_track_file(size_t track)
{
	return tracks[track];
}

void music_play(size_t track)
{
	if (!enabled || track >= track_count)
		return;
	if (play_track(track))
		continue_after((int)track);
	else
		play_next();
}

int music_playing(void)
{
	return playing;
}
