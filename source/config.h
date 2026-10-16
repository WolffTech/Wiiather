#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "weather/location.h"

// The displays in rotation order.
typedef enum {
	DISPLAY_HAZARDS, // always shown, since it only appears for active alerts
	DISPLAY_CURRENT,
	DISPLAY_LATEST_OBSERVATIONS,
	DISPLAY_HOURLY,
	DISPLAY_HOURLY_GRAPH,
	DISPLAY_TRAVEL,
	DISPLAY_REGIONAL,
	DISPLAY_LOCAL,
	DISPLAY_EXTENDED,
	DISPLAY_ALMANAC,
	DISPLAY_SPC_OUTLOOK,
	DISPLAY_RADAR,
	DISPLAY_COUNT
} DisplayId;

typedef struct {
	bool has_location; // false until the user picks one
	Location location;
	uint32_t hidden_displays; // a bit per DisplayId, never Hazards or every other display
	char dir[256]; // the app's folder, which holds settings.ini and music/
} Config;

// Reads settings.ini next to boot.dol, or from sd:/apps/wiiather/ when the
// loader passes no path. `lat` and `lon` take precedence over `zip`, so
// locations without a ZIP can be set by hand.
void config_load(const char *argv0, Config *out);

bool config_display_shown(const Config *config, DisplayId id);

// Shows or hides a display. Returns false, changing nothing, for Hazards or
// for hiding the last display shown.
bool config_set_display_shown(Config *config, DisplayId id, bool shown);

// Writes the settings to settings.ini. Returns false if the SD card can't be
// written.
bool config_save(const Config *config);

// Restores the defaults, with no location, and deletes settings.ini.
void config_reset(Config *config);
