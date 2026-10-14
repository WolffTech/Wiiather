#include "obs.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "util/utc.h"

#define NONE ((Measure){ false, 0 })
#define SOME(v) ((Measure){ true, (v) })

// ---- METAR ----------------------------------------------------------------

// Only the main-body groups ws4kp uses. Unlike ws4kp (metar-taf-parser 9),
// visibility is stored in meters, the altimeter is converted correctly, and
// the ceiling comes from BKN/OVC layers; ws4kp's versions of those three are
// broken and never produce usable values.
typedef struct {
	Measure wind_direction, wind_speed, wind_gust; // degrees, knots, knots
	Measure temperature, dewpoint; // degC
	Measure pressure; // Pa
	Measure visibility_km;
	Measure ceiling_ft;
} Metar;

static bool all_digits(const char *s, size_t n)
{
	for (size_t i = 0; i < n; i++)
		if (!isdigit((unsigned char)s[i]))
			return false;
	return true;
}

static int digits_value(const char *s, size_t n)
{
	int v = 0;
	for (size_t i = 0; i < n; i++)
		v = v * 10 + (s[i] - '0');
	return v;
}

// dddssKT, dddssGggKT, VRBssKT
static bool parse_wind(const char *t, Metar *m)
{
	bool variable = strncmp(t, "VRB", 3) == 0;
	if ((!variable && !all_digits(t, 3)) || !all_digits(t + 3, 2))
		return false;
	const char *p = t + 5;
	Measure gust = NONE;
	if (*p == 'G') {
		size_t digits = strspn(p + 1, "0123456789");
		if (digits < 2 || digits > 3)
			return false;
		gust = SOME(digits_value(p + 1, digits));
		p += 1 + digits;
	}
	if (strcmp(p, "KT") != 0)
		return false;
	m->wind_direction = variable ? NONE : SOME(digits_value(t, 3));
	m->wind_speed = SOME(digits_value(t + 3, 2));
	m->wind_gust = gust;
	return true;
}

// "10SM", "1/2SM", "M1/4SM", "P6SM"; `whole` carries "1" from "1 1/2SM".
static bool parse_statute_visibility(const char *t, int whole, Metar *m)
{
	size_t n = strlen(t);
	if (n < 3 || strcmp(t + n - 2, "SM") != 0)
		return false;
	if (*t == 'M' || *t == 'P')
		t++;
	int a, b;
	double miles;
	if (sscanf(t, "%d/%d", &a, &b) == 2 && b > 0)
		miles = whole + (double)a / b;
	else if (isdigit((unsigned char)*t))
		miles = atof(t);
	else
		return false;
	m->visibility_km = SOME(miles * 1.609344);
	return true;
}

static int parse_temperature(const char *s, const char **end)
{
	bool negative = *s == 'M';
	int value = digits_value(s + negative, 2);
	*end = s + negative + 2;
	return negative ? -value : value;
}

// M?dd/M?dd
static bool parse_temperatures(const char *t, Metar *m)
{
	const char *p = t + (*t == 'M');
	if (!all_digits(p, 2) || p[2] != '/')
		return false;
	const char *q = p + 3 + (p[3] == 'M');
	if (!all_digits(q, 2) || q[2] != '\0')
		return false;
	const char *end;
	m->temperature = SOME(parse_temperature(t, &end));
	m->dewpoint = SOME(parse_temperature(end + 1, &end));
	return true;
}

static void parse_metar(const char *raw, Metar *m)
{
	*m = (Metar){ 0 };
	char buf[256];
	snprintf(buf, sizeof buf, "%s", raw);
	int whole_miles = 0;
	char *save;
	for (char *t = strtok_r(buf, " ", &save); t; t = strtok_r(NULL, " ", &save)) {
		if (strcmp(t, "RMK") == 0)
			break;
		int pending_whole = whole_miles;
		whole_miles = 0;
		size_t n = strlen(t);
		if (n >= 7 && parse_wind(t, m))
			continue;
		if (parse_statute_visibility(t, pending_whole, m))
			continue;
		if (n <= 2 && all_digits(t, n)) {
			whole_miles = atoi(t);
			continue;
		}
		if (n == 4 && all_digits(t, 4)) {
			m->visibility_km = SOME(digits_value(t, 4) / 1000.0);
			continue;
		}
		if (parse_temperatures(t, m))
			continue;
		if (n == 5 && (t[0] == 'A' || t[0] == 'Q') && all_digits(t + 1, 4)) {
			int v = digits_value(t + 1, 4);
			m->pressure = SOME(t[0] == 'A' ? round(v / 100.0 * 3386.39) : v * 100.0);
			continue;
		}
		if (n >= 6 && (strncmp(t, "BKN", 3) == 0 || strncmp(t, "OVC", 3) == 0) && all_digits(t + 3, 3)) {
			double feet = digits_value(t + 3, 3) * 100.0;
			if (!m->ceiling_ft.valid || feet < m->ceiling_ft.value)
				m->ceiling_ft = SOME(feet);
		}
	}
}

// Magnus formula, as in ws4kp.
static double relative_humidity(double temperature, double dewpoint)
{
	const double a = 17.625, b = 243.04;
	double rh = exp(a * dewpoint / (b + dewpoint) - a * temperature / (b + temperature)) * 100;
	return round(fmax(0, fmin(100, rh)));
}

static void fill(Measure *dst, Measure src)
{
	if (!dst->valid && src.valid)
		*dst = src;
}

void obs_augment_with_metar(ObsReading *r, const char *raw_message)
{
	if (!raw_message || !raw_message[0])
		return;
	Metar m;
	parse_metar(raw_message, &m);
	if (m.wind_speed.valid)
		fill(&r->wind_speed, SOME(m.wind_speed.value * 1.852));
	fill(&r->wind_direction, m.wind_direction);
	if (m.wind_gust.valid)
		fill(&r->wind_gust, SOME(m.wind_gust.value * 1.852));
	fill(&r->temperature, m.temperature);
	fill(&r->dewpoint, m.dewpoint);
	fill(&r->pressure, m.pressure);
	if (r->temperature.valid && r->dewpoint.valid)
		fill(&r->relative_humidity, SOME(relative_humidity(r->temperature.value, r->dewpoint.value)));
	if (m.visibility_km.valid)
		fill(&r->visibility, SOME(round(m.visibility_km.value * 10) / 10 * 1000));
	// ws4kp only patches an existing layer whose base is null.
	if (r->has_cloud_layer && m.ceiling_ft.valid)
		fill(&r->ceiling, SOME(round(m.ceiling_ft.value * 0.3048)));
}

// ---- Quality --------------------------------------------------------------

ObsQuality obs_quality(const ObsReading *r)
{
	return (ObsQuality){
		.missing_required = !r->temperature.valid + !r->text_description[0],
		.missing_optional = !r->wind_speed.valid + !r->dewpoint.valid + !r->pressure.valid + !r->visibility.valid +
				    !r->relative_humidity.valid + (r->has_cloud_layer && !r->ceiling.valid),
	};
}

bool obs_quality_ok(ObsQuality q)
{
	return q.missing_required == 0 && q.missing_optional <= 1;
}

static bool is_stale(time_t timestamp, time_t now)
{
	return now && timestamp && now - timestamp > OBS_STALE_SECONDS;
}

bool obs_needs_mapclick(const ObsReading *api, time_t now)
{
	return is_stale(api->timestamp, now) || !obs_quality_ok(obs_quality(api));
}

bool obs_prefer_mapclick(const ObsReading *api, const ObsReading *mapclick)
{
	ObsQuality a = obs_quality(api), m = obs_quality(mapclick);
	bool fresher = mapclick->timestamp > api->timestamp;
	bool better = m.missing_required < a.missing_required || (a.missing_optional > 1 && m.missing_optional <= 1);
	return m.missing_required <= a.missing_required && (fresher || better);
}

// ---- MapClick -------------------------------------------------------------

// JS parseFloat: a leading number, else NaN.
static Measure parse_float(const char *s)
{
	if (!s)
		return NONE;
	char *end;
	double v = strtod(s, &end);
	return end != s && isfinite(v) ? SOME(v) : NONE;
}

static Measure scaled(Measure m, double factor)
{
	return m.valid ? SOME(m.value * factor) : NONE;
}

static Measure f_to_c(Measure m)
{
	return m.valid ? SOME((m.value - 32) * 5 / 9) : NONE;
}

// MapClick image names to API icon conditions. Night images carry an "n"
// prefix, stripped before lookup.
static const char *const mapclick_icons[][2] = {
	{ "skc", "skc" }, { "few", "few" }, { "sct", "sct" }, { "bkn", "bkn" }, { "ovc", "ovc" },
	{ "minus_ra", "rain" }, { "ra", "rain" }, { "sn", "snow" }, { "ra_sn", "rain_snow" },
	{ "rasn", "rain_snow" }, { "raip", "rain_sleet" }, { "ip", "sleet" }, { "ra_fzra", "rain_fzra" },
	{ "fzra", "fzra" }, { "fzra_sn", "snow_fzra" }, { "snip", "snow_sleet" },
	{ "hi_shwrs", "rain_showers_hi" }, { "shra", "rain_showers" }, { "tsra", "tsra" },
	{ "scttsra", "tsra_sct" }, { "hi_tsra", "tsra_hi" }, { "fg", "fog" }, { "wind_skc", "wind_skc" },
	{ "wind_few", "wind_few" }, { "wind_sct", "wind_sct" }, { "wind_bkn", "wind_bkn" },
	{ "wind_ovc", "wind_ovc" }, { "blizzard", "blizzard" }, { "cold", "cold" }, { "hot", "hot" },
	{ "du", "dust" }, { "fu", "smoke" }, { "hz", "haze" }, { "fc", "tornado" }, { "tor", "tornado" },
};

static void mapclick_icon(const char *image, char *out, size_t out_len)
{
	out[0] = '\0';
	if (!image || !image[0] || strcmp(image, "NULL") == 0 || strcmp(image, "NA") == 0)
		return;
	char name[32];
	snprintf(name, sizeof name, "%s", image);
	char *ext = strstr(name, ".png");
	if (ext)
		memmove(ext, ext + 4, strlen(ext + 4) + 1);

	// hi_nshwrs and hi_ntsra put the night marker after "hi_".
	bool night = false;
	char base[32];
	if (strncmp(name, "hi_n", 4) == 0) {
		night = true;
		snprintf(base, sizeof base, "hi_%s", name + 4);
	} else if (name[0] == 'n') {
		night = true;
		snprintf(base, sizeof base, "%s", name + 1);
	} else {
		snprintf(base, sizeof base, "%s", name);
	}

	for (size_t i = 0; i < sizeof mapclick_icons / sizeof mapclick_icons[0]; i++) {
		if (strcmp(base, mapclick_icons[i][0]) == 0) {
			snprintf(out, out_len, "/icons/land/%s/%s?size=medium", night ? "night" : "day", mapclick_icons[i][1]);
			return;
		}
	}
}

// The zone names JavaScript's Date parser understands.
static bool zone_offset_hours(const char *zone, int *out)
{
	static const struct {
		const char *name;
		int offset;
	} zones[] = { { "EST", -5 }, { "EDT", -4 }, { "CST", -6 }, { "CDT", -5 }, { "MST", -7 }, { "MDT", -6 },
		{ "PST", -8 }, { "PDT", -7 }, { "GMT", 0 }, { "UTC", 0 }, { "UT", 0 } };
	for (size_t i = 0; i < sizeof zones / sizeof zones[0]; i++) {
		if (strcasecmp(zone, zones[i].name) == 0) {
			*out = zones[i].offset;
			return true;
		}
	}
	return false;
}

bool obs_parse_mapclick_date(const char *s, int year, time_t *out)
{
	int day, hour, minute;
	char month[4], ampm[3], zone[4];
	if (!s || sscanf(s, "%d %3s %d:%d %2s %3s", &day, month, &hour, &minute, ampm, zone) != 6)
		return false;
	int m = utc_month_from_abbrev(month);
	bool pm = strcasecmp(ampm, "pm") == 0;
	int offset;
	if (!m || (!pm && strcasecmp(ampm, "am") != 0) || !zone_offset_hours(zone, &offset))
		return false;
	// MapClick sometimes pairs a 24-hour time with am/pm ("23:53 pm").
	if (hour > 12)
		hour -= 12;
	hour = hour % 12 + (pm ? 12 : 0);
	*out = (time_t)utc_days_from_civil(year, m, day) * 86400 + (hour - offset) * 3600 + minute * 60;
	return true;
}

void obs_from_mapclick(const MapClickObservation *m, time_t now, ObsReading *out)
{
	*out = (ObsReading){ 0 };
	struct tm utc;
	gmtime_r(&now, &utc);
	if (!obs_parse_mapclick_date(m->date, utc.tm_year + 1900, &out->timestamp))
		out->timestamp = now;
	snprintf(out->text_description, sizeof out->text_description, "%s", m->weather ? m->weather : "");
	mapclick_icon(m->weatherimage, out->icon_url, sizeof out->icon_url);
	out->temperature = f_to_c(parse_float(m->temp));
	out->dewpoint = f_to_c(parse_float(m->dewp));
	out->wind_direction = parse_float(m->windd);
	out->wind_speed = scaled(parse_float(m->winds), 1.60934);
	out->wind_gust = scaled(parse_float(m->gust), 1.60934);
	out->pressure = scaled(parse_float(m->slp), 3386.39);
	out->visibility = scaled(parse_float(m->visibility), 1609.34);
	out->relative_humidity = parse_float(m->relh);
	out->wind_chill = f_to_c(parse_float(m->windchill));
}

// ---- Merge ----------------------------------------------------------------

#define BACKFILL(field)                                     \
	do {                                                \
		out->field = NONE;                          \
		for (size_t i = 0; i < count; i++) {        \
			if (readings[i].field.valid) {      \
				out->field = readings[i].field; \
				break;                      \
			}                                   \
		}                                           \
	} while (0)

void obs_merge(const ObsReading *readings, size_t count, time_t now, Observation *out)
{
	*out = (Observation){ 0 };
	if (count == 0)
		return;
	const ObsReading *latest = &readings[0];
	snprintf(out->text_description, sizeof out->text_description, "%s", latest->text_description);
	snprintf(out->icon_url, sizeof out->icon_url, "%s", latest->icon_url);
	BACKFILL(temperature);
	BACKFILL(dewpoint);
	BACKFILL(wind_direction);
	BACKFILL(wind_speed);
	BACKFILL(wind_gust);
	BACKFILL(pressure);
	BACKFILL(visibility);
	BACKFILL(relative_humidity);
	BACKFILL(heat_index);
	BACKFILL(wind_chill);
	// cloudLayers has no "value" key, so ws4kp takes it from the latest only.
	out->ceiling = latest->ceiling;
	out->pressure_previous = count > 1 ? readings[1].pressure : NONE;
	out->stale = is_stale(latest->timestamp, now);
}
