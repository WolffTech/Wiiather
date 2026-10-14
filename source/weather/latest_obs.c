#include "latest_obs.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

double latest_obs_distance(double lat1, double lon1, double lat2, double lon2)
{
	return sqrt((lat2 - lat1) * (lat2 - lat1) + (lon2 - lon1) * (lon2 - lon1));
}

static int compare_distance(const void *a, const void *b)
{
	const StationDistance *x = a, *y = b;
	if (x->distance != y->distance)
		return x->distance < y->distance ? -1 : 1;
	return x->index < y->index ? -1 : x->index > y->index;
}

void latest_obs_sort_by_distance(StationDistance *stations, size_t count)
{
	qsort(stations, count, sizeof stations[0], compare_distance);
}

int latest_obs_missing(const ObsReading *r)
{
	return !r->temperature.valid + !r->wind_speed.valid + !r->wind_direction.valid + !r->text_description[0];
}

bool latest_obs_needs_mapclick(const ObsReading *api, time_t now)
{
	bool stale = now && api->timestamp && now - api->timestamp > OBS_STALE_SECONDS;
	return stale || latest_obs_missing(api) > 0;
}

bool latest_obs_prefer_mapclick(const ObsReading *api, const ObsReading *mapclick)
{
	int a = latest_obs_missing(api), m = latest_obs_missing(mapclick);
	bool fresher = mapclick->timestamp > api->timestamp;
	return m <= a && (fresher || m < a);
}

bool latest_obs_accept(const ObsReading *r, const char *city, LatestStation *out)
{
	if (latest_obs_missing(r) > 0)
		return false;
	*out = (LatestStation){
		.temperature = r->temperature.value,
		.wind_speed = r->wind_speed.value,
		.wind_direction = r->wind_direction.value,
	};
	snprintf(out->city, sizeof out->city, "%s", city);
	snprintf(out->text_description, sizeof out->text_description, "%s", r->text_description);
	return true;
}

// ---- Formatting -----------------------------------------------------------

// JavaScript Math.round, which rounds .5 toward +infinity.
static int js_round(double value)
{
	return (int)floor(value + 0.5);
}

static const char *direction_to_nsew(double degrees)
{
	static const char *const names[] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
		"S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
	int index = (int)floor(degrees / 22.5 + 0.5) % 16;
	return names[index < 0 ? index + 16 : index];
}

static bool only_letters_spaces(const char *start, const char *end)
{
	if (start == end)
		return false;
	for (const char *p = start; p < end; p++)
		if (!isalpha((unsigned char)*p) && *p != ' ')
			return false;
	return true;
}

// Port of ws4kp locationCleanup(): drops "Chicago / ", "Chicago/" and
// "Chicago, " prefixes, applied in that order.
static void location_cleanup(char *s)
{
	static const char *const separators[] = { " / ", "/", ", " };
	for (size_t i = 0; i < 3; i++) {
		const char *sep = strstr(s, separators[i]);
		if (sep && only_letters_spaces(s, sep)) {
			const char *rest = sep + strlen(separators[i]);
			memmove(s, rest, strlen(rest) + 1);
		}
	}
}

// JavaScript's String.replace with a plain pattern: first occurrence only.
static void replace_first(char *s, size_t cap, const char *from, const char *to)
{
	char *p = strstr(s, from);
	if (!p)
		return;
	size_t from_len = strlen(from), to_len = strlen(to), tail = strlen(p + from_len);
	if ((size_t)(p - s) + to_len + tail + 1 > cap)
		return;
	memmove(p + to_len, p + from_len, tail + 1);
	memcpy(p, to, to_len);
}

// Port of ws4kp's shortenCurrentConditions(); order matters.
static void shorten_conditions(char *s, size_t cap)
{
	static const char *const rules[][2] = {
		{ "Light", "L" }, { "Heavy", "H" }, { "Partly", "P" }, { "Mostly", "M" }, { "Few", "F" },
		{ "Thunderstorm", "T'storm" }, { " in ", "" }, { "Vicinity", "" }, { " and ", " " },
		{ "Freezing Rain", "Frz Rn" }, { "Freezing", "Frz" }, { "Unknown Precip", "" },
		{ "L Snow Fog", "L Snw/Fog" }, { " with ", "/" },
	};
	for (size_t i = 0; i < sizeof rules / sizeof rules[0]; i++)
		replace_first(s, cap, rules[i][0], rules[i][1]);
}

// Copies the first `limit` characters (JavaScript substr), then collapses
// spaces the way HTML renders them.
static void display_text(char *out, size_t out_len, const char *text, int limit)
{
	char buf[64];
	snprintf(buf, sizeof buf, "%.*s", limit, text);
	size_t n = 0;
	bool space = false;
	for (const char *p = buf; *p && n + 2 < out_len; p++) {
		if (*p == ' ') {
			space = n > 0;
			continue;
		}
		if (space)
			out[n++] = ' ';
		space = false;
		out[n++] = *p;
	}
	out[n] = '\0';
}

static void format_row(const LatestStation *s, LatestObsRow *row)
{
	char text[64];
	snprintf(text, sizeof text, "%s", s->city);
	location_cleanup(text);
	display_text(row->location, sizeof row->location, text, 14);

	snprintf(row->temperature, sizeof row->temperature, "%d", js_round(s->temperature * 9 / 5 + 32));

	snprintf(text, sizeof text, "%s", s->text_description);
	shorten_conditions(text, sizeof text);
	display_text(row->weather, sizeof row->weather, text, 9);

	// Direction and speed padded to five characters, e.g. "W   9" or "WNW14".
	int speed = js_round(s->wind_speed / 1.60934);
	if (speed > 0) {
		const char *dir = direction_to_nsew(s->wind_direction);
		char digits[12];
		int digits_len = snprintf(digits, sizeof digits, "%d", speed);
		int pad = 5 - (int)strlen(dir) - digits_len;
		snprintf(row->wind, sizeof row->wind, "%s%*s%s", dir, pad > 0 ? pad : 0, "", digits);
	} else {
		snprintf(row->wind, sizeof row->wind, "Calm");
	}
}

size_t latest_obs_format(const LatestObservations *obs, LatestObsRow *rows)
{
	size_t count = obs->count < LATEST_OBS_STATIONS ? obs->count : LATEST_OBS_STATIONS;
	// Stable insertion sort by location: ws4kp means to sort by station name,
	// but compares a property that doesn't exist, so it keeps distance order.
	for (size_t i = 0; i < count; i++) {
		LatestObsRow row;
		format_row(&obs->stations[i], &row);
		size_t j = i;
		for (; j > 0 && strcmp(rows[j - 1].location, row.location) > 0; j--)
			rows[j] = rows[j - 1];
		rows[j] = row;
	}
	return count;
}
