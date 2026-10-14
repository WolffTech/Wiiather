// NWS 12-hour forecast periods and ws4kp's Local/Extended Forecast shaping
// (localforecast.mjs, extendedforecast.mjs). Pure logic, testable on the host.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define FORECAST_MAX_PERIODS 14

typedef struct {
	char name[32]; // "This Afternoon"
	char weekday[4]; // "Thu", from the start time's local date
	time_t end; // UTC
	bool is_daytime;
	bool has_temperature;
	int temperature; // degF
	char short_forecast[64];
	char detailed_forecast[512];
	char icon_url[160];
} ForecastPeriod;

typedef struct {
	size_t count;
	ForecastPeriod periods[FORECAST_MAX_PERIODS];
} Forecast;

// Drops periods that ended at or before `now` (UTC), keeping order.
void forecast_drop_expired(Forecast *f, time_t now);

// "Thu" for an ISO 8601 timestamp's own calendar date, or "" if unparseable.
void forecast_weekday(const char *iso_time, char out[4]);

// Local Forecast shows the first six periods as "Name...Text".
#define LOCAL_FORECAST_PERIODS 6
void forecast_local_text(const ForecastPeriod *p, char *out, size_t out_len);

typedef struct {
	char weekday[4];
	char icon_url[160];
	char text[24];
	bool has_low, has_high;
	int low, high;
} ExtendedDay;

// Pairs day and night periods into days, skipping a leading night period.
size_t forecast_extended_days(const Forecast *f, ExtendedDay *out, size_t max);

// ws4kp's two-word summary: "Chance Showers And Thunderstorms" -> "Showers T'Storms".
void forecast_shorten(const char *text, char *out, size_t out_len);
