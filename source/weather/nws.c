#include "nws.h"

#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net/https.h"
#include "stations.h"
#include "util/utc.h"

#define API "https://api.weather.gov"
#define GEOJSON "application/geo+json"
#define MAX_STATIONS 6
#define OBSERVATION_HISTORY 5

// `date`, if given, receives the server's clock in UTC (0 if unknown).
static json_t *get_json(const char *url, const char *accept, time_t *date, char *err, size_t err_len)
{
	HttpResponse res;
	if (!https_get(url, accept, &res, err, err_len))
		return NULL;
	if (date)
		*date = res.date;
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
	json_t *root = get_json(url, GEOJSON, NULL, err, err_len);
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

static ObsReading reading_from_json(json_t *props)
{
	ObsReading r = { 0 };
	const char *timestamp = json_string_value(json_object_get(props, "timestamp"));
	if (!timestamp || !utc_parse_iso8601(timestamp, &r.timestamp))
		r.timestamp = 0;
	copy_string(r.text_description, sizeof r.text_description, json_object_get(props, "textDescription"));
	copy_string(r.icon_url, sizeof r.icon_url, json_object_get(props, "icon"));
	r.temperature = measure(props, "temperature");
	r.dewpoint = measure(props, "dewpoint");
	r.wind_direction = measure(props, "windDirection");
	r.wind_speed = measure(props, "windSpeed");
	r.wind_gust = measure(props, "windGust");
	r.pressure = measure(props, "barometricPressure");
	r.visibility = measure(props, "visibility");
	r.relative_humidity = measure(props, "relativeHumidity");
	r.heat_index = measure(props, "heatIndex");
	r.wind_chill = measure(props, "windChill");
	// No layers means clear skies, not missing data.
	json_t *layers = json_object_get(props, "cloudLayers");
	r.has_cloud_layer = json_array_size(layers) > 0;
	r.ceiling = measure(json_array_get(layers, 0), "base");
	return r;
}

static const char *string_field(json_t *obj, const char *key)
{
	return json_string_value(json_object_get(obj, key));
}

// MapClick serves airport stations from a faster feed than api.weather.gov.
static bool get_mapclick(const char *station_id, time_t now, ObsReading *out)
{
	const StationInfo *station = station_find(station_id);
	if (!station)
		return false;
	char url[200], err[128];
	snprintf(url, sizeof url, "https://forecast.weather.gov/MapClick.php?FcstType=json&lat=%.4f&lon=%.4f&station=%s",
		station->lat, station->lon, station_id);
	json_t *root = get_json(url, "application/json", NULL, err, sizeof err);
	json_t *cur = json_object_get(root, "currentobservation");
	if (!json_is_object(cur)) {
		json_decref(root);
		return false;
	}
	MapClickObservation m = {
		.date = string_field(cur, "Date"),
		.temp = string_field(cur, "Temp"),
		.dewp = string_field(cur, "Dewp"),
		.relh = string_field(cur, "Relh"),
		.winds = string_field(cur, "Winds"),
		.windd = string_field(cur, "Windd"),
		.gust = string_field(cur, "Gust"),
		.weather = string_field(cur, "Weather"),
		.weatherimage = string_field(cur, "Weatherimage"),
		.visibility = string_field(cur, "Visibility"),
		.slp = string_field(cur, "SLP"),
		.windchill = string_field(cur, "WindChill"),
	};
	obs_from_mapclick(&m, now, out);
	json_decref(root);
	return true;
}

static bool select_observation(json_t *root, const char *station_id, time_t now, Observation *out)
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

	ObsReading readings[OBSERVATION_HISTORY];
	for (size_t i = 0; i < count; i++)
		readings[i] = reading_from_json(json_object_get(obs[i], "properties"));

	ObsReading *latest = &readings[0];
	obs_augment_with_metar(latest, string_field(json_object_get(obs[0], "properties"), "rawMessage"));
	ObsReading mapclick;
	if (obs_needs_mapclick(latest, now) && get_mapclick(station_id, now, &mapclick) &&
	    obs_prefer_mapclick(latest, &mapclick)) {
		// ws4kp keeps the API timestamp, so stale data still reads "Recent".
		mapclick.timestamp = latest->timestamp;
		*latest = mapclick;
	}
	if (!obs_quality_ok(obs_quality(latest)))
		return false;
	obs_merge(readings, count, now, out);
	return true;
}

bool nws_get_current_observation(const NwsPoint *point, Observation *out, char *err, size_t err_len)
{
	json_t *stations = get_json(point->observation_stations_url, GEOJSON, NULL, err, err_len);
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
		const char *station_id = string_field(props, "stationIdentifier");
		if (!station_url || !station_id)
			continue;

		char url[200];
		snprintf(url, sizeof url, "%s/observations?limit=%d", station_url, OBSERVATION_HISTORY);
		time_t now;
		json_t *obs = get_json(url, GEOJSON, &now, err, err_len);
		if (!obs)
			continue;
		found = select_observation(obs, station_id, now, out);
		json_decref(obs);
		if (found) {
			snprintf(out->station_id, sizeof out->station_id, "%s", station_id);
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

static int number_or_value(json_t *v)
{
	// Newer API responses wrap numbers as {"value": n}.
	if (json_is_object(v))
		v = json_object_get(v, "value");
	return (int)json_number_value(v);
}

bool nws_get_forecast(const NwsPoint *point, Forecast *out, char *err, size_t err_len)
{
	char url[200];
	snprintf(url, sizeof url, "%s?units=us", point->forecast_url);
	time_t now;
	json_t *root = get_json(url, GEOJSON, &now, err, err_len);
	if (!root)
		return false;
	json_t *periods = json_object_get(json_object_get(root, "properties"), "periods");
	*out = (Forecast){ 0 };
	size_t i;
	json_t *period;
	json_array_foreach(periods, i, period) {
		if (out->count == FORECAST_MAX_PERIODS)
			break;
		ForecastPeriod *p = &out->periods[out->count++];
		copy_string(p->name, sizeof p->name, json_object_get(period, "name"));
		forecast_weekday(string_field(period, "startTime"), p->weekday);
		const char *end = string_field(period, "endTime");
		if (!end || !utc_parse_iso8601(end, &p->end))
			p->end = 0;
		p->is_daytime = json_is_true(json_object_get(period, "isDaytime"));
		json_t *temperature = json_object_get(period, "temperature");
		p->has_temperature = json_is_number(temperature) ||
				     json_is_number(json_object_get(temperature, "value"));
		p->temperature = p->has_temperature ? number_or_value(temperature) : 0;
		copy_string(p->short_forecast, sizeof p->short_forecast, json_object_get(period, "shortForecast"));
		copy_string(p->detailed_forecast, sizeof p->detailed_forecast,
			json_object_get(period, "detailedForecast"));
		copy_string(p->icon_url, sizeof p->icon_url, json_object_get(period, "icon"));
	}
	json_decref(root);
	if (now)
		forecast_drop_expired(out, now);
	if (out->count == 0) {
		snprintf(err, err_len, "Forecast has no current periods");
		return false;
	}
	return true;
}

static bool get_latest_station(const StationInfo *station, LatestStation *out)
{
	char url[128], err[128];
	snprintf(url, sizeof url, API "/stations/%s/observations/latest", station->id);
	time_t now;
	json_t *root = get_json(url, GEOJSON, &now, err, sizeof err);
	if (!root)
		return false;
	json_t *props = json_object_get(root, "properties");
	ObsReading r = reading_from_json(props);
	obs_augment_with_metar(&r, string_field(props, "rawMessage"));
	json_decref(root);

	ObsReading mapclick;
	if (latest_obs_needs_mapclick(&r, now) && get_mapclick(station->id, now, &mapclick) &&
	    latest_obs_prefer_mapclick(&r, &mapclick)) {
		mapclick.timestamp = r.timestamp;
		r = mapclick;
	}
	return latest_obs_accept(&r, station->city, out);
}

bool nws_get_latest_observations(double lat, double lon, LatestObservations *out)
{
	static StationDistance ranked[STATION_COUNT];
	for (size_t i = 0; i < STATION_COUNT; i++)
		ranked[i] = (StationDistance){ i, latest_obs_distance(station_info[i].lat, station_info[i].lon, lat, lon) };
	latest_obs_sort_by_distance(ranked, STATION_COUNT);

	// ws4kp fetches the nearest in batches only as large as the shortfall, so
	// trying one at a time picks the same stations.
	*out = (LatestObservations){ 0 };
	for (size_t i = 0; i < LATEST_OBS_CANDIDATES && out->count < LATEST_OBS_STATIONS; i++) {
		if (get_latest_station(&station_info[ranked[i].index], &out->stations[out->count]))
			out->count++;
	}
	return out->count > 0;
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

bool nws_get_alerts(double lat, double lon, Alerts *out, time_t *now, char *err, size_t err_len)
{
	char url[128];
	snprintf(url, sizeof url, API "/alerts/active?point=%.4f,%.4f&status=actual", lat, lon);
	json_t *root = get_json(url, GEOJSON, now, err, err_len);
	if (!root)
		return false;
	json_t *features = json_object_get(root, "features");
	if (!json_is_array(features)) {
		snprintf(err, err_len, "Alerts response has no features");
		json_decref(root);
		return false;
	}
	out->count = 0;
	size_t i;
	json_t *feature;
	json_array_foreach(features, i, feature) {
		if (out->count == ALERTS_MAX)
			break;
		json_t *props = json_object_get(feature, "properties");
		Alert *a = &out->alerts[out->count++];
		copy_string(a->id, sizeof a->id, json_object_get(feature, "id"));
		alert_copy_text(a->event, sizeof a->event, string_field(props, "event"));
		alert_copy_text(a->description, sizeof a->description, string_field(props, "description"));
		a->severity = alert_severity(string_field(props, "severity"));
		const char *urgency = string_field(props, "urgency");
		a->immediate = urgency && strcmp(urgency, "Immediate") == 0;
		const char *expires = string_field(props, "expires");
		if (!expires || !utc_parse_iso8601(expires, &a->expires))
			a->expires = 0;
	}
	json_decref(root);
	alerts_select(out);
	if (*now)
		alerts_drop_expired(out, *now);
	return true;
}
