// The settings menu: one WeatherStar-style page per section, driven by the
// Wii Remote's D-pad, A and B.
#pragma once
#include <gctypes.h>

#include "config.h"
#include "location_picker.h"
#include "weather/weather.h"

typedef enum { PAGE_MAIN, PAGE_LOCATION, PAGE_ABOUT, PAGE_RESET } SettingsPage;

typedef struct {
	SettingsPage page;
	size_t main_row;
	LocationPicker picker; // while on PAGE_LOCATION
} SettingsMenu;

// Settings changed by one update, for the caller to apply.
typedef enum {
	SETTINGS_LOCATION = 1 << 0,
	SETTINGS_RESET = 1 << 1, // the config holds the defaults and no location
} SettingsChange;

typedef struct {
	unsigned changed; // SettingsChange flags
	bool closed;
} SettingsResult;

SettingsMenu settings_menu_make(void);

// Applies this frame's button presses to the menu and `config`.
SettingsResult settings_menu_update(SettingsMenu *m, u32 pressed, Config *config, const WeatherSnapshot *snap);

void settings_menu_draw(const SettingsMenu *m, const Config *config, const WeatherSnapshot *snap);
