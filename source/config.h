#pragma once
#include <stdbool.h>

#include "weather/location.h"

typedef struct {
	bool has_location; // false until the user picks one
	Location location;
	char dir[256]; // the app's folder, which holds settings.ini and music/
} Config;

// Reads settings.ini next to boot.dol, or from sd:/apps/wiiather/ when the
// loader passes no path. `lat` and `lon` take precedence over `zip`, so
// locations without a ZIP can be set by hand.
void config_load(const char *argv0, Config *out);

// Writes the location to settings.ini. Returns false if the SD card can't be
// written.
bool config_save(const Config *config);
