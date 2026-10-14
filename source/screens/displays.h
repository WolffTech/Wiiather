// The display rotation: each display shows one or more screens for a set
// time, and displays without data are skipped, like ws4kp's navigation.
#pragma once
#include <gctypes.h>

#include "screens.h"

// Formatted data for every display, rebuilt when the weather changes.
typedef struct {
	bool has_current;
	bool stale;
	CurrentConditionsText current;
	LocalForecastPages local;
	ExtendedForecastView extended;
	LatestObservationsView latest_observations;
	AlmanacView almanac;
} DisplayData;

void displays_prepare(const WeatherSnapshot *snap, DisplayData *out);

// Refreshes displays that depend on the date as well as the data. Call every
// frame.
void displays_tick(const WeatherSnapshot *snap, DisplayData *data);

typedef struct {
	size_t display;
	size_t screen;
	u32 started_ms;
} Rotation;

// Advances to the next screen when the current one's time is up.
void rotation_update(Rotation *r, const DisplayData *data, u32 now_ms);

void displays_draw(const Rotation *r, const DisplayData *data, u32 now_ms);
