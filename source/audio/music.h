// Background music, like ws4kp's media player: every .mp3 in a folder plays
// in random order, reshuffled after each pass through the list.
#pragma once
#include <stdbool.h>

// Scans `dir` for .mp3 files and starts playing if any exist.
void music_start(const char *dir);

// Starts the next track when one ends. Call once per frame.
void music_update(void);

void music_toggle(void);
