#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "weather/location.h"
#include "weather/units.h"

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

typedef enum { MUSIC_SHUFFLE, MUSIC_IN_ORDER } MusicOrder;

// ws4kp's speed setting, which scales how long each screen shows.
typedef enum { SPEED_VERY_FAST, SPEED_FAST, SPEED_NORMAL, SPEED_SLOW, SPEED_VERY_SLOW, SPEED_COUNT } Speed;

#define MUSIC_VOLUME_STEP 5 // percent
#define MAX_SKIPPED_TRACKS 64

typedef struct {
	bool has_location; // false until the user picks one
	Location location;
	uint32_t hidden_displays; // a bit per DisplayId, never Hazards or every other display
	bool music;
	MusicOrder music_order;
	uint8_t music_volume; // percent, a multiple of MUSIC_VOLUME_STEP
	// File names left out of playback, kept by name so new files play.
	char skipped_tracks[MAX_SKIPPED_TRACKS][128];
	size_t skipped_count;
	Units units;
	Speed speed;
	bool clock_24h;
	bool bottom_bar; // the conditions lines; hazard crawls show regardless
	bool locate_by_ip; // suggest a ZIP code from the public IP address
	char dir[256]; // the app's folder, which holds settings.ini and music/
} Config;

// Reads settings.ini next to boot.dol, or from sd:/apps/wiiather/ when the
// loader passes no path. `lat` and `lon` take precedence over `zip`, so
// locations without a ZIP can be set by hand.
void config_load(const char *argv0, Config *out);

// The screen time multiplier as a percentage: 50 for Very Fast to 150 for
// Very Slow.
unsigned config_speed_percent(Speed speed);

bool config_display_shown(const Config *config, DisplayId id);

// Shows or hides a display. Returns false, changing nothing, for Hazards or
// for hiding the last display shown.
bool config_set_display_shown(Config *config, DisplayId id, bool shown);

bool config_track_enabled(const Config *config, const char *file);

// Returns false, changing nothing, when the skip list is full.
bool config_set_track_enabled(Config *config, const char *file, bool enabled);

// Writes the settings to settings.ini. Returns false if the SD card can't be
// written.
bool config_save(const Config *config);

// Restores the defaults, with no location, and deletes settings.ini.
void config_reset(Config *config);
