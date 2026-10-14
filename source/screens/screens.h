#pragma once
#include <gctypes.h>

#include "render/gfx.h"
#include "weather/format.h"
#include "weather/weather.h"

// Header shared by most displays: logo, two-line title, clock, and
// optionally the NOAA logo.
void header_draw(const char *title_top, const char *title_bottom, bool noaa_logo);

// Bottom bar text (ws4kp's "scroll" area).
void scroll_draw(const char *text);

void current_conditions_draw(const CurrentConditionsText *cc, u32 time_ms);

// Placeholder body while data loads or after a failure.
void status_draw(const WeatherSnapshot *snap);

// Local Forecast: each period's text wrapped and split into 7-line pages.
#define LOCAL_FORECAST_PAGE_LINES 7
#define LOCAL_FORECAST_MAX_PAGES 18

typedef struct {
	size_t line_count[LOCAL_FORECAST_MAX_PAGES];
	char lines[LOCAL_FORECAST_MAX_PAGES][LOCAL_FORECAST_PAGE_LINES][GFX_LINE_MAX];
	size_t page_count;
} LocalForecastPages;

void local_forecast_prepare(const Forecast *f, LocalForecastPages *out);
u32 local_forecast_page_ms(const LocalForecastPages *pages, size_t page);
void local_forecast_draw(const LocalForecastPages *pages, size_t page);

// Extended Forecast: three days per screen.
#define EXTENDED_DAYS_PER_SCREEN 3
#define EXTENDED_SCREENS 2

typedef struct {
	size_t count;
	ExtendedDay days[EXTENDED_DAYS_PER_SCREEN * EXTENDED_SCREENS];
	IconId icons[EXTENDED_DAYS_PER_SCREEN * EXTENDED_SCREENS];
} ExtendedForecastView;

void extended_forecast_prepare(const Forecast *f, ExtendedForecastView *out);
void extended_forecast_draw(const ExtendedForecastView *view, size_t screen, u32 time_ms);

// Latest Observations: one row per nearby station.
typedef struct {
	size_t count;
	LatestObsRow rows[LATEST_OBS_STATIONS];
} LatestObservationsView;

void latest_observations_prepare(const LatestObservations *obs, LatestObservationsView *out);
void latest_observations_draw(const LatestObservationsView *view);
