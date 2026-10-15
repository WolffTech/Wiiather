#include "hourly.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sun_moon.h"
#include "util/utc.h"

#define HOUR 3600

// ISO 8601 durations as NWS writes them: "PT1H", "P1D", "P7DT5H". Years and
// months have no fixed length, so they're rejected.
static bool parse_duration(const char *s, long *seconds)
{
	if (*s++ != 'P')
		return false;
	long total = 0;
	bool time = false, any = false;
	while (*s) {
		if (*s == 'T' && !time) {
			time = true;
			s++;
			continue;
		}
		char *end;
		long n = strtol(s, &end, 10);
		if (end == s || n < 0)
			return false;
		char unit = *end;
		long scale = !time && unit == 'W' ? 7 * 86400
			: !time && unit == 'D'    ? 86400
			: time && unit == 'H'     ? HOUR
			: time && unit == 'M'     ? 60
			: time && unit == 'S'     ? 1
						  : 0;
		if (!scale)
			return false;
		total += n * scale;
		any = true;
		s = end + 1;
	}
	*seconds = total;
	return any;
}

bool hourly_parse_valid_time(const char *valid_time, time_t *start, time_t *end)
{
	const char *slash = valid_time ? strchr(valid_time, '/') : NULL;
	long duration;
	if (!slash || !utc_parse_iso8601(valid_time, start) || !parse_duration(slash + 1, &duration))
		return false;
	*end = *start + duration;
	return true;
}

size_t hourly_expand(time_t start, time_t end, time_t first_hour, size_t len, size_t max)
{
	// ws4kp's do-while adds the first hour even for an empty span.
	size_t added = 0;
	time_t t = start;
	do {
		if (t >= first_hour && len + added < max)
			added++;
		t += HOUR;
	} while (t < end && len + added < max);
	return added;
}

unsigned hourly_weather_flags(const char *weather)
{
	static const struct {
		const char *text;
		HourlyWeather flag;
	} phenomena[] = {
		{ "thunder", HOURLY_THUNDER }, { "snow", HOURLY_SNOW }, { "ice", HOURLY_ICE },
		{ "fog", HOURLY_FOG }, { "wind", HOURLY_WIND },
	};
	char lower[64];
	snprintf(lower, sizeof lower, "%s", weather ? weather : "");
	for (char *c = lower; *c; c++)
		*c = (char)tolower((unsigned char)*c);
	unsigned flags = 0;
	for (size_t i = 0; i < sizeof phenomena / sizeof phenomena[0]; i++)
		if (strstr(lower, phenomena[i].text))
			flags |= phenomena[i].flag;
	return flags;
}

// JavaScript's `value > limit`, where null is never greater.
static bool above(Measure m, double limit)
{
	return m.valid && m.value > limit;
}

HourlyIcon hourly_icon(const HourlyForecast *f, size_t hour, bool night)
{
	unsigned weather = f->weather[hour];
	bool snow = above(f->snowfall_amount[hour], 0) || (weather & HOURLY_SNOW);
	Measure pop = f->probability_of_precipitation[hour];
	Measure sky = f->sky_cover[hour];

	// First match wins; amounts are in mm and wind in km/h, like ws4kp.
	if (above(f->ice_accumulation[hour], 0) || (weather & HOURLY_ICE))
		return HOURLY_ICON_FREEZING_RAIN;
	if (above(f->snowfall_amount[hour], 10))
		return above(f->wind_speed[hour], 30) || (weather & HOURLY_WIND) ? HOURLY_ICON_BLOWING_SNOW
										 : HOURLY_ICON_HEAVY_SNOW;
	if (snow && (weather & HOURLY_THUNDER))
		return HOURLY_ICON_THUNDERSNOW;
	if (snow)
		return HOURLY_ICON_LIGHT_SNOW;
	if (weather & HOURLY_THUNDER)
		return HOURLY_ICON_THUNDERSTORM;
	if (above(pop, 70))
		return HOURLY_ICON_RAIN;
	if (above(pop, 30))
		return night ? HOURLY_ICON_SCATTERED_SHOWERS_NIGHT : HOURLY_ICON_SCATTERED_SHOWERS;
	if (weather & HOURLY_FOG)
		return HOURLY_ICON_FOG;
	if (above(sky, 70))
		return HOURLY_ICON_CLOUDY;
	if (above(sky, 50))
		return night ? HOURLY_ICON_PARTLY_CLEAR : HOURLY_ICON_MOSTLY_CLOUDY;
	if (above(sky, 30))
		return night ? HOURLY_ICON_PARTLY_CLOUDY_NIGHT : HOURLY_ICON_PARTLY_CLOUDY;
	return night ? HOURLY_ICON_CLEAR : HOURLY_ICON_SUNNY;
}

// JavaScript Math.round, which rounds .5 toward +infinity.
static int js_round(double value)
{
	return (int)floor(value + 0.5);
}

static const char *direction_to_nsew(Measure dir)
{
	static const char *const names[] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
		"S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
	if (!dir.valid)
		return "VAR";
	int index = (int)floor(dir.value / 22.5 + 0.5) % 16;
	return names[index < 0 ? index + 16 : index];
}

static void format_temperature(Measure c, char *out, size_t out_len)
{
	if (c.valid)
		snprintf(out, out_len, "%3d", js_round(c.value * 9 / 5 + 32));
	else
		snprintf(out, out_len, "  -");
}

// ws4kp pads the wind to five characters between direction and speed.
static void format_wind(Measure speed_kph, Measure direction, char *out, size_t out_len)
{
	int mph = speed_kph.valid ? js_round(speed_kph.value / 1.60934) : 0;
	if (mph <= 0) {
		snprintf(out, out_len, "Calm");
		return;
	}
	const char *dir = direction_to_nsew(direction);
	char speed[8];
	int speed_len = snprintf(speed, sizeof speed, "%d", mph);
	int spaces = 5 - (int)strlen(dir) - speed_len;
	snprintf(out, out_len, "%s%*s%s", dir, spaces > 0 ? spaces : 0, "", speed);
}

// Luxon's { weekday: 'short', hour: 'numeric' } in en-US.
static void format_hour(time_t utc, long utc_offset, char *out, size_t out_len)
{
	static const char *const weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	time_t local = utc + utc_offset;
	struct tm tm;
	gmtime_r(&local, &tm);
	int hour = tm.tm_hour % 12 ? tm.tm_hour % 12 : 12;
	snprintf(out, out_len, "%s %d %s", weekdays[tm.tm_wday], hour, tm.tm_hour < 12 ? "AM" : "PM");
}

static int64_t floor_div(int64_t a, int64_t b)
{
	return a / b - (a % b < 0);
}

// Sun times for the location's local date, cached since rows span two dates.
typedef struct {
	bool cached, has_times;
	int64_t day;
	int64_t sunrise, sunset;
} SunCache;

// ws4kp counts an hour as night when it starts between a sunset and the next
// sunrise. Polar days and nights count as day, as they do there.
static bool is_night(SunCache *cache, time_t hour, double lat, double lon, long utc_offset)
{
	int64_t day = floor_div((int64_t)hour + utc_offset, 86400);
	if (!cache->cached || cache->day != day) {
		int64_t noon_ms = (day * 86400 + 12 * HOUR - utc_offset) * 1000;
		cache->cached = true;
		cache->day = day;
		cache->has_times = suncalc_sun_times(noon_ms, lat, lon, &cache->sunrise, &cache->sunset);
	}
	int64_t ms = (int64_t)hour * 1000;
	return cache->has_times && (ms < cache->sunrise || ms >= cache->sunset);
}

size_t hourly_rows(const HourlyForecast *f, double lat, double lon, time_t now, long utc_offset,
	HourlyRow out[HOURLY_ROWS])
{
	time_t current_hour = (time_t)(floor_div(now, HOUR) * HOUR);
	size_t first = current_hour > f->start ? (size_t)((current_hour - f->start) / HOUR) : 0;
	SunCache sun = { 0 };
	size_t n = 0;
	for (size_t i = first; i < f->count && n < HOURLY_ROWS; i++, n++) {
		HourlyRow *row = &out[n];
		time_t hour = f->start + (time_t)i * HOUR;
		format_hour(hour, utc_offset, row->hour, sizeof row->hour);
		format_temperature(f->temperature[i], row->temp, sizeof row->temp);
		format_temperature(f->apparent_temperature[i], row->like, sizeof row->like);
		row->like_color = HOURLY_LIKE_PLAIN;
		if (f->temperature[i].valid && f->apparent_temperature[i].valid) {
			int temp = atoi(row->temp), like = atoi(row->like);
			if (like < temp)
				row->like_color = HOURLY_LIKE_WIND_CHILL;
			else if (like > temp)
				row->like_color = HOURLY_LIKE_HEAT_INDEX;
		}
		format_wind(f->wind_speed[i], f->wind_direction[i], row->wind, sizeof row->wind);
		row->icon = hourly_icon(f, i, is_night(&sun, hour, lat, lon, utc_offset));
	}
	return n;
}
