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
#define VOLUME 191 // ws4kp's default 75%

static char folder[256];
static char tracks[MAX_TRACKS][128];
static size_t track_count, next_track;
static FILE *playing_file;
static bool enabled;

static void shuffle(void)
{
	for (size_t i = track_count - 1; i > 0; i--) {
		size_t j = (size_t)rand() % (i + 1);
		char tmp[sizeof tracks[0]];
		snprintf(tmp, sizeof tmp, "%s", tracks[i]);
		snprintf(tracks[i], sizeof tracks[i], "%s", tracks[j]);
		snprintf(tracks[j], sizeof tracks[j], "%s", tmp);
	}
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
}

static void play_next(void)
{
	stop();
	// Give up after one pass of unreadable files rather than spinning.
	for (size_t tries = 0; tries < track_count; tries++) {
		if (next_track == track_count) {
			shuffle();
			next_track = 0;
		}
		char path[sizeof folder + sizeof tracks[0] + 1];
		snprintf(path, sizeof path, "%.255s/%.127s", folder, tracks[next_track++]);
		playing_file = fopen(path, "rb");
		if (playing_file) {
			MP3Player_PlayFile(playing_file, read_file, NULL);
			return;
		}
	}
	enabled = false;
}

void music_start(const char *dir)
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

	srand((unsigned)time(NULL));
	next_track = track_count; // shuffle before the first track
	ASND_Init();
	MP3Player_Init();
	MP3Player_Volume(VOLUME);
	enabled = true;
	play_next();
}

void music_update(void)
{
	if (enabled && !MP3Player_IsPlaying())
		play_next();
}

void music_toggle(void)
{
	if (track_count == 0)
		return;
	enabled = !enabled;
	if (enabled)
		play_next();
	else
		stop();
}
