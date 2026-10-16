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
	HourlyView hourly;
	HourlyGraphView hourly_graph;
	TravelForecastView travel;
	RegionalForecastView regional;
	LocalForecastPages local;
	ExtendedForecastView extended;
	LatestObservationsView latest_observations;
	AlmanacView almanac;
	SpcOutlookView spc_outlook;
	RadarView radar;
	HazardsView hazards;
} DisplayData;

void displays_prepare(const WeatherSnapshot *snap, DisplayData *out);

// Refreshes displays that depend on the date as well as the data. Call every
// frame.
void displays_tick(const WeatherSnapshot *snap, DisplayData *data);

typedef struct {
	size_t display;
	size_t screen;
	u32 started_ms;
	bool running; // false until the first update, so the first screen gets its full time
} Rotation;

// Advances to the next screen when the current one's time is up.
void rotation_update(Rotation *r, DisplayData *data, u32 now_ms);

void displays_draw(const Rotation *r, const DisplayData *data, u32 now_ms);

// Whether the current display shows the bottom bar (ws4kp's okToDrawCurrentConditions).
bool displays_scroll_visible(const Rotation *r);

// The bottom bar (ws4kp's currentweatherscroll): the most severe alert's
// crawl, then each current conditions line for 4 s, paused while hidden.
typedef struct {
	size_t item; // 0 is the hazard crawl, then the conditions lines
	bool shown;
	u32 started_ms;
	u32 paused_elapsed_ms;
} ScrollBar;

// Advances the bar, and draws it if `visible`.
void scroll_bar_draw(ScrollBar *bar, const DisplayData *d, bool visible, u32 now_ms);
