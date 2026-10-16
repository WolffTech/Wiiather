#pragma once
#include <gctypes.h>

#include "render/gfx.h"
#include "weather/format.h"
#include "weather/sun_moon.h"
#include "weather/weather.h"

// Header shared by most displays: logo, two-line title (or one line when
// `title_bottom` is NULL), clock, and optionally the NOAA logo.
void header_draw(const char *title_top, const char *title_bottom, bool noaa_logo);
// Just the logo and title, for displays with something else in the clock's place.
void header_title_draw(const char *title_top, const char *title_bottom);

// Bottom bar text (ws4kp's "scroll" area).
void scroll_draw(const char *text);

void current_conditions_draw(const CurrentConditionsText *cc, u32 time_ms);

// Placeholder body while data loads or after a failure.
void status_draw(const WeatherSnapshot *snap);

// The scrolling list Hourly and Travel Forecast share: rows scroll up under a
// sticky column header bar in the main area.
#define LIST_MAIN_HEIGHT 310
#define LIST_COLUMN_LABEL_TOP (90 - 14)
#define LIST_ROW_HEIGHT 72
#define LIST_ROW_TEXT_TOP 8

extern const TextStyle list_column_style, list_row_style;

int list_content_height(size_t rows);
// Clips to the main area and draws the background for content scrolled up by
// `offset`. Returns the top of the first row.
int list_begin(int content_height, int offset);
bool list_row_visible(int top);
// Draws the column header bar over the rows. Label it with list_column_style
// at LIST_COLUMN_LABEL_TOP, then call gfx_clip_reset().
void list_column_bar_draw(void);

// Hourly Forecast: the next 24 hours, scrolling up under column headers.
typedef struct {
	long rtc_hour; // the RTC hour the rows start from, in hours since the epoch
	size_t count; // 0 without data
	HourlyRow rows[HOURLY_ROWS];
	int content_height;
} HourlyView;

void hourly_view_prepare(const WeatherSnapshot *snap, HourlyView *out);
// Whether the hour has changed since the view was prepared.
bool hourly_view_outdated(const HourlyView *view);
u32 hourly_view_ms(const HourlyView *view);
void hourly_view_draw(const HourlyView *view, u32 elapsed_ms, u32 time_ms);

// Hourly Graph: the next 36 hours of temperature, dewpoint, cloud cover and
// chance of precipitation.
typedef struct {
	long rtc_hour; // the RTC hour the graph starts from, in hours since the epoch
	bool ready; // false without data
	HourlyGraph graph;
} HourlyGraphView;

void hourly_graph_view_prepare(const WeatherSnapshot *snap, HourlyGraphView *out);
// Whether the hour has changed since the view was prepared.
bool hourly_graph_view_outdated(const HourlyGraphView *view);
void hourly_graph_view_draw(const HourlyGraphView *view);

// Travel Forecast: the day's high and low for each city with data.
typedef struct {
	const char *city;
	IconId icon;
	char low[8], high[8];
} TravelRow;

typedef struct {
	size_t count; // 0 without data
	TravelRow rows[TRAVEL_CITY_COUNT];
	char title_bottom[16]; // "For Thursday"
	int content_height;
} TravelForecastView;

void travel_forecast_prepare(const TravelForecast *travel, TravelForecastView *out);
u32 travel_forecast_ms(const TravelForecastView *view);
void travel_forecast_draw(const TravelForecastView *view, u32 elapsed_ms, u32 time_ms);

// Regional Forecast: the map around the location with each site's
// observation, then its next two forecast periods, one screen each.
typedef struct {
	char name[13];
	f32 x, y; // anchor in the main area
	IconId icons[REGIONAL_SCREENS];
	char temperatures[REGIONAL_SCREENS][8];
} RegionalLabel;

typedef struct {
	size_t count; // 0 without data
	MapId map;
	f32 origin_x, origin_y; // map pixel at the main area's top-left
	RegionalLabel labels[REGIONAL_MAX_SITES];
	char title_bottom[REGIONAL_SCREENS][24]; // "Observations", "Thursday Night"
} RegionalForecastView;

void regional_forecast_prepare(const RegionalForecast *regional, RegionalForecastView *out);
const char *regional_forecast_title_top(size_t screen);
void regional_forecast_draw(const RegionalForecastView *view, size_t screen, u32 time_ms);

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

// SPC Outlook: a bar for each day's categorical risk at the location.
typedef struct {
	bool ready; // false without a risk on any day
	long rtc_day; // the RTC date `weekdays` start from, in days since the epoch
	char weekdays[SPC_DAYS][10]; // "Wednesday"
	SpcRisk risk[SPC_DAYS];
} SpcOutlookView;

void spc_outlook_view_prepare(const WeatherSnapshot *snap, SpcOutlookView *out);
// Whether the RTC's date has changed since the view was prepared.
bool spc_outlook_view_outdated(const SpcOutlookView *view);
void spc_outlook_view_draw(const SpcOutlookView *view);

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
