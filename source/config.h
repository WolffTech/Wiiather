#pragma once
#include <stdbool.h>

typedef struct {
	double lat, lon;
	char dir[256]; // the app's folder, which holds settings.ini and music/
} Config;

// Reads settings.ini next to boot.dol, or from sd:/apps/wiiather/ when the
// loader passes no path.
// Missing files or keys keep the defaults. Returns true if a file was read.
bool config_load(const char *argv0, Config *out);
