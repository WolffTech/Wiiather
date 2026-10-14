#pragma once
#include <gctypes.h>

#include "render/gfx.h"
#include "weather/format.h"
#include "weather/sun_moon.h"
#include "weather/weather.h"

// Header shared by most displays: logo, two-line title (or one line when
// `title_bottom` is NULL), clock, and optionally the NOAA logo.
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

// Almanac: today's and tomorrow's sun times and the next four moon phases.
typedef struct {
	bool ready; // false until the RTC's UTC offset is known
	long rtc_day; // the RTC date `text` is for, in days since the epoch
	AlmanacText text;
} AlmanacView;

void almanac_view_prepare(const WeatherSnapshot *snap, AlmanacView *out);
// Whether the RTC's date has changed since the view was prepared.
bool almanac_view_outdated(const AlmanacView *view);
void almanac_view_draw(const AlmanacView *view);

// Hazards: unviewed alerts wrapped for a full-screen vertical scroll, plus
// the most severe alert's crawl for the bottom bar.
#define HAZARDS_MAX_LINES 640

typedef struct {
	AlertsViewed viewed; // persists across prepares
	size_t shown_count; // unviewed alerts on the display
	char shown_ids[ALERTS_MAX][ALERT_ID_MAX];
	size_t line_count;
	int line_top[HAZARDS_MAX_LINES]; // relative to the top of the scrolling content
	char lines[HAZARDS_MAX_LINES][GFX_LINE_MAX];
	int content_height;
	char crawl_event[ALERT_EVENT_MAX]; // empty when there are no alerts
	char crawl[ALERT_EVENT_MAX + ALERT_DESCRIPTION_MAX];
	int crawl_width;
} HazardsView;

void hazards_prepare(const WeatherSnapshot *snap, HazardsView *view);
u32 hazards_ms(const HazardsView *view);
void hazards_draw(const HazardsView *view, u32 elapsed_ms);
// Called once the display has run its course, like ws4kp's viewedAlerts.
void hazards_mark_viewed(HazardsView *view);
