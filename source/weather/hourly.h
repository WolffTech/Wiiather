// ws4kp's Hourly Forecast data (hourly.mjs, icons-hourly.mjs): NWS gridpoint
// series expanded to one value per hour, then shaped into 24 display rows or
// the Hourly Graph's 36 hours (hourly-graph.mjs).
// Pure logic, testable on the host.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "obs.h"
#include "units.h"

#define HOURLY_MAX_HOURS 48 // like ws4kp's expand(); extra hours cover failed refreshes
#define HOURLY_ROWS 24

// Phenomena from the gridpoint `weather` series that pick the icon.
typedef enum {
	HOURLY_THUNDER = 1 << 0,
	HOURLY_SNOW = 1 << 1,
	HOURLY_ICE = 1 << 2,
	HOURLY_FOG = 1 << 3,
	HOURLY_WIND = 1 << 4,
} HourlyWeather;

// Gridpoint values in the API's SI units (degC, km/h, degrees, %, mm), one
// per hour from `start`.
typedef struct {
	time_t start; // UTC start of the first hour
	size_t count; // hours of temperature; other series may be shorter
	Measure temperature[HOURLY_MAX_HOURS];
	Measure apparent_temperature[HOURLY_MAX_HOURS];
	Measure dewpoint[HOURLY_MAX_HOURS];
	Measure wind_speed[HOURLY_MAX_HOURS];
	Measure wind_direction[HOURLY_MAX_HOURS];
	Measure sky_cover[HOURLY_MAX_HOURS];
	Measure probability_of_precipitation[HOURLY_MAX_HOURS];
	Measure snowfall_amount[HOURLY_MAX_HOURS];
	Measure ice_accumulation[HOURLY_MAX_HOURS];
	unsigned weather[HOURLY_MAX_HOURS]; // HourlyWeather flags
} HourlyForecast;

// Splits a gridpoint validTime such as "2026-10-14T20:00:00+00:00/PT3H" into
// UTC start and end times.
bool hourly_parse_valid_time(const char *valid_time, time_t *start, time_t *end);

// ws4kp's expand(): how many hourly slots a value spanning [start, end) adds
// to a series that already holds `len` of at most `max` values. Hours before
// `first_hour` are skipped, but gaps between values are not filled.
size_t hourly_expand(time_t start, time_t end, time_t first_hour, size_t len, size_t max);

// HourlyWeather flags for one gridpoint weather string, such as "thunderstorms".
unsigned hourly_weather_flags(const char *weather);

// The icons ws4kp's hourlyIcon() chooses from.
typedef enum {
	HOURLY_ICON_FREEZING_RAIN,
	HOURLY_ICON_BLOWING_SNOW,
	HOURLY_ICON_HEAVY_SNOW,
	HOURLY_ICON_THUNDERSNOW,
	HOURLY_ICON_LIGHT_SNOW,
	HOURLY_ICON_THUNDERSTORM,
	HOURLY_ICON_RAIN,
	HOURLY_ICON_SCATTERED_SHOWERS,
	HOURLY_ICON_SCATTERED_SHOWERS_NIGHT,
	HOURLY_ICON_FOG,
	HOURLY_ICON_CLOUDY,
	HOURLY_ICON_MOSTLY_CLOUDY,
	HOURLY_ICON_PARTLY_CLEAR,
	HOURLY_ICON_PARTLY_CLOUDY,
	HOURLY_ICON_PARTLY_CLOUDY_NIGHT,
	HOURLY_ICON_CLEAR,
	HOURLY_ICON_SUNNY,
	HOURLY_ICON_COUNT
} HourlyIcon;

HourlyIcon hourly_icon(const HourlyForecast *f, size_t hour, bool night);

// The feels-like column's color.
typedef enum { HOURLY_LIKE_PLAIN, HOURLY_LIKE_WIND_CHILL, HOURLY_LIKE_HEAT_INDEX } HourlyLike;

// One row's display strings, padded like ws4kp's white-space: pre columns.
typedef struct {
	char hour[12]; // "Thu 3 PM"
	char temp[8]; // " 72", or "  -" when missing
	char like[8];
	HourlyLike like_color;
	char wind[16]; // "SW  8" or "Calm"
	HourlyIcon icon;
} HourlyRow;

// Rows for the 24 hours starting with the hour containing `now` (UTC), fewer
// if the forecast runs out. `utc_offset` is local minus UTC in seconds, and
// night is between the location's sunset and sunrise.
size_t hourly_rows(const HourlyForecast *f, double lat, double lon, time_t now, long utc_offset, Units units,
	HourlyRow out[HOURLY_ROWS]);

#define HOURLY_GRAPH_HOURS 36
#define HOURLY_GRAPH_X_LABELS 5 // at the start, end and each quarter between
#define HOURLY_GRAPH_Y_LABELS 4

// The Hourly Graph's series and axis labels in ws4kp's standard layout.
typedef struct {
	size_t count; // hours of temperature from the current one; other series may be shorter
	Units units;
	// Whole degrees, as ws4kp rounds them before plotting.
	Measure temperature[HOURLY_GRAPH_HOURS];
	Measure dewpoint[HOURLY_GRAPH_HOURS];
	// Percentages.
	Measure sky_cover[HOURLY_GRAPH_HOURS];
	Measure probability_of_precipitation[HOURLY_GRAPH_HOURS];
	// The temperature axis spans both temperature series. Without either,
	// there's no axis and no labels.
	bool has_temperature_axis;
	int temperature_min, temperature_max;
	char y_labels[HOURLY_GRAPH_Y_LABELS][4]; // top to bottom: "72\xb0", or "100"
	char x_labels[HOURLY_GRAPH_X_LABELS][12]; // "6P", or "Fri 12A" when the day changes
} HourlyGraph;

// Fills `out` for the hours starting with the one containing `now` (UTC), and
// returns false if the forecast has none. `utc_offset` is local minus UTC in
// seconds.
bool hourly_graph(const HourlyForecast *f, time_t now, long utc_offset, Units units, HourlyGraph *out);
