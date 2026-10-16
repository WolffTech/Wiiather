// Background music, like ws4kp's media player: every .mp3 in a folder plays,
// shuffled after each pass through the list or in file name order.
#pragma once
#include <stdbool.h>
#include <stddef.h>

#include "config.h"

// Scans `dir` for .mp3 files and plays them as `config` says.
void music_start(const char *dir, const Config *config);

// Applies changed music settings: on or off, order, volume and skipped tracks.
void music_configure(const Config *config);

// Starts the next track when one ends. Call once per frame.
void music_update(void);

// The tracks, sorted by file name.
size_t music_track_count(void);
const char *music_track_file(size_t track);

// Plays `track` now, even when skipped, and continues from it. Needs music on.
void music_play(size_t track);

// The track playing, or -1 for none.
int music_playing(void);
