#pragma once
#include "nws.h"
#include "units.h"

// Display strings for the Current Conditions screen, in either units, as ws4kp
// formats them. Strings are Latin-1; "\xB0" is the degree sign. Runs of
// spaces are already collapsed the way HTML renders them.
typedef struct {
	char temperature[8];
	char condition[64];
	char wind[16];
	char gusts[24];       // empty when there is no gust
	char location[24];
	char humidity[8];
	char dewpoint[8];
	char ceiling[16];
	char visibility[16];
	char pressure[16];    // empty when pressure is unavailable
	char extra_label[16]; // "Heat Index:", "Wind Chill:", or empty
	char extra_value[16];
	IconId icon;
	// Bottom-bar lines that cycle every few seconds (ws4kp currentweatherscroll).
	char scroll[6][48];
	int scroll_count;
} CurrentConditionsText;

void format_current_conditions(const Observation *obs, Units units, CurrentConditionsText *out);

// Matches JavaScript Math.round, which rounds .5 toward +infinity.
int js_round(double value);

// Station display name: ws4kp's curated city name, else the cleaned NWS name.
void format_station_location(const char *station_id, const char *nws_name, char *out, size_t out_len);
