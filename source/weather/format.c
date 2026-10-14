#include "format.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stations.h"

#define DEGREE "\xB0"

int js_round(double value)
{
	return (int)floor(value + 0.5);
}

static int c_to_f(double c)
{
	return js_round(c * 9 / 5 + 32);
}

static int kph_to_mph(double kph)
{
	return js_round(kph / 1.60934);
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

// Replaces every occurrence of `from` with `to` in place.
static void replace_all(char *s, size_t cap, const char *from, const char *to)
{
	size_t from_len = strlen(from), to_len = strlen(to);
	char *p = s;
	while ((p = strstr(p, from))) {
		size_t tail = strlen(p + from_len);
		if ((size_t)(p - s) + to_len + tail + 1 > cap)
			return;
		memmove(p + to_len, p + from_len, tail + 1);
		memcpy(p, to, to_len);
		p += to_len;
	}
}

// Port of ws4kp shortConditions(); order matters.
static void short_conditions(char *s, size_t cap)
{
	static const char *const rules[][2] = {
		{ "Light", "L" }, { "Heavy", "H" }, { "Partly", "P" }, { "Mostly", "M" }, { "Few", "F" },
		{ "Thunderstorm", "T'storm" }, { " in ", "" }, { "Vicinity", "" }, { " and ", " " },
		{ "Freezing Rain", "Frz Rn" }, { "Freezing", "Frz" }, { "Unknown Precip", "" },
		{ "L Snow Fog", "L Snw/Fog" }, { " with ", "/" },
	};
	for (size_t i = 0; i < sizeof rules / sizeof rules[0]; i++)
		replace_all(s, cap, rules[i][0], rules[i][1]);
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

void format_station_location(const char *station_id, const char *nws_name, char *out, size_t out_len)
{
	const StationInfo *info = station_find(station_id);
	char name[64];
	if (info) {
		snprintf(name, sizeof name, "%s", info->city);
	} else {
		snprintf(name, sizeof name, "%s", nws_name);
		location_cleanup(name);
	}
	snprintf(out, out_len, "%.20s", name);
}

static void format_scroll(const Observation *obs, CurrentConditionsText *out)
{
	int n = 0;
	char (*line)[48] = out->scroll;
	int temp_f = c_to_f(obs->temperature.value);
	snprintf(line[n++], sizeof line[0], "Conditions at %s", out->location);

	int len = snprintf(line[n], sizeof line[0], "Temp: %d" DEGREE "F", temp_f);
	if (obs->heat_index.valid && obs->heat_index.value != 0)
		snprintf(line[n] + len, sizeof line[0] - len, " Heat Index: %d" DEGREE "F", c_to_f(obs->heat_index.value));
	else if (obs->wind_chill.valid && obs->wind_chill.value != 0)
		snprintf(line[n] + len, sizeof line[0] - len, " Wind Chill: %d" DEGREE "F", c_to_f(obs->wind_chill.value));
	n++;

	snprintf(line[n++], sizeof line[0], "Humidity: %s Dewpoint: %sF", out->humidity, out->dewpoint);

	snprintf(line[n++], sizeof line[0], "Barometric Pressure: %s", out->pressure[0] ? out->pressure : "-");

	int speed = obs->wind_speed.valid ? kph_to_mph(obs->wind_speed.value) : 0;
	len = speed > 0 ? snprintf(line[n], sizeof line[0], "Wind: %s %d MPH", direction_to_nsew(obs->wind_direction), speed)
			: snprintf(line[n], sizeof line[0], "Wind: Calm");
	int gust = obs->wind_gust.valid ? kph_to_mph(obs->wind_gust.value) : 0;
	if (gust > 0)
		snprintf(line[n] + len, sizeof line[0] - len, " Gusts to %d", gust);
	n++;

	char visibility[8];
	snprintf(visibility, sizeof visibility, "%.*s", (int)strcspn(out->visibility, " "), out->visibility);
	if (strcmp(out->ceiling, "Unlimited") == 0)
		snprintf(line[n++], sizeof line[0], "Visib: %s mi. Ceiling: Unlimited", visibility);
	else
		snprintf(line[n++], sizeof line[0], "Visib: %s mi. Ceiling: %.*s ft.", visibility,
			(int)strcspn(out->ceiling, "f"), out->ceiling);

	out->scroll_count = n;
}

void format_current_conditions(const Observation *obs, CurrentConditionsText *out)
{
	*out = (CurrentConditionsText){ 0 };
	int temp_f = c_to_f(obs->temperature.value);
	snprintf(out->temperature, sizeof out->temperature, "%d" DEGREE, temp_f);

	snprintf(out->condition, sizeof out->condition, "%s", obs->text_description);
	if (strlen(out->condition) > 15)
		short_conditions(out->condition, sizeof out->condition);

	if (!obs->wind_speed.valid)
		snprintf(out->wind, sizeof out->wind, "-");
	else if (kph_to_mph(obs->wind_speed.value) == 0)
		snprintf(out->wind, sizeof out->wind, "Calm");
	else
		snprintf(out->wind, sizeof out->wind, "%s %d", direction_to_nsew(obs->wind_direction),
			kph_to_mph(obs->wind_speed.value));

	if (obs->wind_gust.valid)
		snprintf(out->gusts, sizeof out->gusts, "Gusts to %d", kph_to_mph(obs->wind_gust.value));

	format_station_location(obs->station_id, obs->station_name, out->location, sizeof out->location);

	// Math.round(null) is 0 in ws4kp.
	snprintf(out->humidity, sizeof out->humidity, "%d%%",
		obs->relative_humidity.valid ? js_round(obs->relative_humidity.value) : 0);
	if (obs->dewpoint.valid)
		snprintf(out->dewpoint, sizeof out->dewpoint, "%d" DEGREE, c_to_f(obs->dewpoint.value));
	else
		snprintf(out->dewpoint, sizeof out->dewpoint, "-" DEGREE);

	// ws4kp rounds feet to the nearest hundred; no cloud layer is "Unlimited".
	int ceiling_ft = obs->ceiling.valid ? js_round(js_round(obs->ceiling.value / 0.3048) / 100.0) * 100 : 0;
	if (ceiling_ft == 0)
		snprintf(out->ceiling, sizeof out->ceiling, "Unlimited");
	else
		snprintf(out->ceiling, sizeof out->ceiling, "%dft.", ceiling_ft);

	if (obs->visibility.valid)
		snprintf(out->visibility, sizeof out->visibility, "%d mi.",
			js_round(js_round(obs->visibility.value / 1.60934) / 1000.0));
	else
		snprintf(out->visibility, sizeof out->visibility, "- mi.");

	if (obs->pressure.valid) {
		double in_hg = trunc(obs->pressure.value * 0.0002953 * 100) / 100;
		const char *trend = "";
		if (obs->pressure_previous.valid) {
			double diff = obs->pressure.value - obs->pressure_previous.value;
			trend = diff > 150 ? "R" : diff < -150 ? "F" : "";
		}
		// HTML drops the trailing space when there is no trend letter.
		snprintf(out->pressure, sizeof out->pressure, trend[0] ? "%.2f %s" : "%.2f", in_hg, trend);
	}

	// JS truthiness: a reading of exactly 0 counts as absent.
	if (obs->heat_index.valid && obs->heat_index.value != 0 && c_to_f(obs->heat_index.value) != temp_f) {
		snprintf(out->extra_label, sizeof out->extra_label, "Heat Index:");
		snprintf(out->extra_value, sizeof out->extra_value, "%d" DEGREE, c_to_f(obs->heat_index.value));
	} else if (obs->wind_chill.valid && obs->wind_chill.value != 0 && c_to_f(obs->wind_chill.value) < temp_f) {
		snprintf(out->extra_label, sizeof out->extra_label, "Wind Chill:");
		snprintf(out->extra_value, sizeof out->extra_value, "%d" DEGREE, c_to_f(obs->wind_chill.value));
	}

	out->icon = nws_large_icon(obs->icon_url);
	format_scroll(obs, out);
}
