#include "nws.h"

#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net/https.h"

#define API "https://api.weather.gov"
#define GEOJSON "application/geo+json"
#define MAX_STATIONS 6
#define OBSERVATION_HISTORY 5

static json_t *get_json(const char *url, char *err, size_t err_len)
{
	HttpResponse res;
	if (!https_get(url, GEOJSON, &res, err, err_len))
		return NULL;
	if (res.status != 200) {
		snprintf(err, err_len, "HTTP %d from %.80s", res.status, url);
		http_response_free(&res);
		return NULL;
	}
	json_error_t jerr;
	json_t *root = json_loadb(res.body, res.len, 0, &jerr);
	http_response_free(&res);
	if (!root)
		snprintf(err, err_len, "bad JSON: %s", jerr.text);
	return root;
}

static void copy_string(char *dst, size_t dst_len, json_t *value)
{
	snprintf(dst, dst_len, "%s", json_is_string(value) ? json_string_value(value) : "");
}

// Reads {"value": n} quantities.
static Measure measure(json_t *props, const char *key)
{
	json_t *value = json_object_get(json_object_get(props, key), "value");
	if (json_is_number(value))
		return (Measure){ true, json_number_value(value) };
	return (Measure){ false, 0 };
}

bool nws_get_point(double lat, double lon, NwsPoint *out, char *err, size_t err_len)
{
	// NWS redirects coordinates with more than 4 decimal places.
	char url[128];
	snprintf(url, sizeof url, API "/points/%.4f,%.4f", lat, lon);
	json_t *root = get_json(url, err, err_len);
	if (!root)
		return false;

	json_t *props = json_object_get(root, "properties");
	json_t *where = json_object_get(json_object_get(props, "relativeLocation"), "properties");
	*out = (NwsPoint){ 0 };
	copy_string(out->city, sizeof out->city, json_object_get(where, "city"));
	copy_string(out->state, sizeof out->state, json_object_get(where, "state"));
	copy_string(out->observation_stations_url, sizeof out->observation_stations_url,
		json_object_get(props, "observationStations"));
	copy_string(out->forecast_url, sizeof out->forecast_url, json_object_get(props, "forecast"));
	copy_string(out->time_zone, sizeof out->time_zone, json_object_get(props, "timeZone"));
	json_decref(root);

	if (!out->observation_stations_url[0]) {
		snprintf(err, err_len, "Location not covered by the NWS");
		return false;
	}
	return true;
}

static int compare_timestamp_desc(const void *a, const void *b)
{
	const char *ta = json_string_value(json_object_get(json_object_get(*(json_t **)a, "properties"), "timestamp"));
	const char *tb = json_string_value(json_object_get(json_object_get(*(json_t **)b, "properties"), "timestamp"));
	return strcmp(tb ? tb : "", ta ? ta : "");
}

// Latest non-null value across observations sorted newest first (ws4kp backfill).
static Measure backfill(json_t **obs, size_t count, const char *key)
{
	for (size_t i = 0; i < count; i++) {
		Measure m = measure(json_object_get(obs[i], "properties"), key);
		if (m.valid)
			return m;
	}
	return (Measure){ false, 0 };
}

static bool parse_observations(json_t *root, Observation *out)
{
	json_t *features = json_object_get(root, "features");
	size_t count = json_array_size(features);
	if (count == 0)
		return false;
	if (count > OBSERVATION_HISTORY)
		count = OBSERVATION_HISTORY;
	json_t *obs[OBSERVATION_HISTORY];
	for (size_t i = 0; i < count; i++)
		obs[i] = json_array_get(features, i);
	qsort(obs, count, sizeof obs[0], compare_timestamp_desc);

	json_t *latest = json_object_get(obs[0], "properties");
	copy_string(out->text_description, sizeof out->text_description, json_object_get(latest, "textDescription"));
	copy_string(out->icon_url, sizeof out->icon_url, json_object_get(latest, "icon"));
	out->temperature = backfill(obs, count, "temperature");
	out->dewpoint = backfill(obs, count, "dewpoint");
	out->wind_direction = backfill(obs, count, "windDirection");
	out->wind_speed = backfill(obs, count, "windSpeed");
	out->wind_gust = backfill(obs, count, "windGust");
	out->pressure = backfill(obs, count, "barometricPressure");
	out->visibility = backfill(obs, count, "visibility");
	out->relative_humidity = backfill(obs, count, "relativeHumidity");
	out->heat_index = backfill(obs, count, "heatIndex");
	out->wind_chill = backfill(obs, count, "windChill");
	// cloudLayers has no "value" key, so ws4kp takes it from the latest
	// observation only. No layers means clear skies, not missing data.
	json_t *layers = json_object_get(latest, "cloudLayers");
	out->ceiling = measure(json_array_get(layers, 0), "base");
	bool ceiling_missing = json_array_size(layers) > 0 && !out->ceiling.valid;
	out->pressure_previous = count > 1 ? measure(json_object_get(obs[1], "properties"), "barometricPressure")
					   : (Measure){ false, 0 };

	// ws4kp requires temperature and a description, and tolerates one
	// missing optional field.
	if (!out->temperature.valid || !out->text_description[0])
		return false;
	int missing = !out->wind_speed.valid + !out->dewpoint.valid + !out->pressure.valid + !out->visibility.valid +
		      !out->relative_humidity.valid + ceiling_missing;
	return missing <= 1;
}

bool nws_get_current_observation(const NwsPoint *point, Observation *out, char *err, size_t err_len)
{
	json_t *stations = get_json(point->observation_stations_url, err, err_len);
	if (!stations)
		return false;

	json_t *features = json_object_get(stations, "features");
	size_t count = json_array_size(features);
	if (count > MAX_STATIONS)
		count = MAX_STATIONS;

	bool found = false;
	for (size_t i = 0; i < count && !found; i++) {
		json_t *station = json_array_get(features, i);
		json_t *props = json_object_get(station, "properties");
		const char *station_url = json_string_value(json_object_get(station, "id"));
		if (!station_url)
			continue;

		char url[200];
		snprintf(url, sizeof url, "%s/observations?limit=%d", station_url, OBSERVATION_HISTORY);
		json_t *obs = get_json(url, err, err_len);
		if (!obs)
			continue;
		*out = (Observation){ 0 };
		found = parse_observations(obs, out);
		json_decref(obs);
		if (found) {
			copy_string(out->station_id, sizeof out->station_id, json_object_get(props, "stationIdentifier"));
			copy_string(out->station_name, sizeof out->station_name, json_object_get(props, "name"));
		}
	}
	json_decref(stations);

	if (!found && count > 0)
		snprintf(err, err_len, "No nearby station has current data");
	else if (!found)
		snprintf(err, err_len, "No observation stations found");
	return found;
}

// Port of ws4kp icons-parse.mjs + icons-large.mjs.
IconId nws_large_icon(const char *icon_url)
{
	// Icon URLs look like .../icons/land/{day|night}/{cond}[,prob][/{cond}[,prob]][?size=...]
	const char *p = strstr(icon_url, "/icons/");
	if (!p)
		return ICON_NO_DATA;
	p = strchr(p + 7, '/'); // skip the icon set
	if (!p)
		return ICON_NO_DATA;
	bool night = strncmp(p + 1, "night/", 6) == 0;
	if (!night && strncmp(p + 1, "day/", 4) != 0)
		return ICON_NO_DATA;
	const char *cond = p + 1 + (night ? 6 : 4);
	size_t cond_len = strcspn(cond, "?");

	char first[32] = "", second[32] = "";
	int first_prob = 100, second_prob = 100;
	const char *slash = memchr(cond, '/', cond_len);
	size_t first_len = slash ? (size_t)(slash - cond) : cond_len;
	snprintf(first, sizeof first, "%.*s", (int)first_len, cond);
	if (slash)
		snprintf(second, sizeof second, "%.*s", (int)(cond_len - first_len - 1), slash + 1);

	char *comma;
	if ((comma = strchr(first, ','))) {
		*comma = '\0';
		first_prob = atoi(comma + 1) ?: 100;
	}
	if ((comma = strchr(second, ','))) {
		*comma = '\0';
		second_prob = atoi(comma + 1) ?: 100;
	}

	// With two different conditions, ws4kp uses the second one.
	const char *name = first;
	int prob = first_prob;
	if (slash && strcmp(first, second) != 0) {
		name = second;
		prob = second_prob;
	}

#define IS(s) (strcmp(name, s) == 0)
	if (IS("skc") || IS("hot") || IS("haze") || IS("cold"))
		return night ? ICON_CLEAR : ICON_SUNNY;
	if (IS("dust") || IS("smoke"))
		return ICON_SMOKE;
	if (IS("few") || IS("sct") || IS("bkn"))
		return night ? ICON_MOSTLY_CLEAR : ICON_PARTLY_CLOUDY;
	if (IS("ovc"))
		return ICON_CLOUDY;
	if (IS("fog"))
		return ICON_FOG;
	if (IS("rain_sleet"))
		return ICON_RAIN_SLEET;
	if (IS("sleet"))
		return ICON_SLEET;
	if (IS("rain_showers") || IS("rain_showers_hi") || IS("rain_showers_high"))
		return ICON_SHOWER;
	if (IS("rain"))
		return ICON_RAIN;
	if (IS("snow"))
		return prob > 50 ? ICON_HEAVY_SNOW : ICON_LIGHT_SNOW;
	if (IS("rain_snow"))
		return ICON_RAIN_SNOW;
	if (IS("snow_fzra") || IS("winter_mix"))
		return ICON_FREEZING_RAIN_SNOW;
	if (IS("fzra") || IS("rain_fzra"))
		return ICON_FREEZING_RAIN;
	if (IS("snow_sleet"))
		return ICON_SNOW_SLEET;
	if (IS("tsra_sct") || IS("tsra"))
		return night ? ICON_SCATTERED_THUNDERSTORMS_NIGHT : ICON_SCATTERED_THUNDERSTORMS_DAY;
	if (IS("tsra_hi") || IS("tornado") || IS("hurricane") || IS("tropical_storm"))
		return ICON_THUNDERSTORM;
	if (strncmp(name, "wind_", 5) == 0)
		return ICON_WINDY;
	if (IS("blizzard"))
		return ICON_BLOWING_SNOW;
#undef IS
	return ICON_NO_DATA;
}
