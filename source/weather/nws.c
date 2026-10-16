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
	copy_string(out->forecast_grid_data_url, sizeof out->forecast_grid_data_url,
		json_object_get(props, "forecastGridData"));
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

// `url` includes the units parameter.
static bool get_forecast(const char *url, Forecast *out, char *err, size_t err_len)
{
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

bool nws_get_forecast(const NwsPoint *point, Forecast *out, char *err, size_t err_len)
{
	char url[200];
	snprintf(url, sizeof url, "%s?units=us", point->forecast_url);
	return get_forecast(url, out, err, err_len);
}

bool nws_get_travel_day(const TravelCity *city, TravelDay *out)
{
	static Forecast forecast;
	char url[128], err[128];
	snprintf(url, sizeof url, API "/gridpoints/%s/%u,%u/forecast?units=us", city->wfo, city->x, city->y);
	return get_forecast(url, &forecast, err, sizeof err) && forecast_travel_day(&forecast, out);
}

static bool get_gridpoint(double lat, double lon, Gridpoint *out)
{
	char url[128], err[128];
	snprintf(url, sizeof url, API "/points/%.4f,%.4f", lat, lon);
	json_t *root = get_json(url, GEOJSON, NULL, err, sizeof err);
	json_t *props = json_object_get(root, "properties");
	const char *wfo = string_field(props, "gridId");
	json_t *x = json_object_get(props, "gridX"), *y = json_object_get(props, "gridY");
	// Offshore marine stations have no land grid, so these are null.
	bool ok = wfo && strlen(wfo) < sizeof out->wfo && json_is_integer(x) && json_is_integer(y);
	if (ok) {
		snprintf(out->wfo, sizeof out->wfo, "%s", wfo);
		out->x = json_integer_value(x);
		out->y = json_integer_value(y);
	}
	json_decref(root);
	return ok;
}

// ws4kp takes the first of the gridpoint's stations with a four-letter ID,
// which generally has good data.
static bool get_regional_station(const Gridpoint *g, char station_id[8])
{
	char url[128], err[128];
	snprintf(url, sizeof url, API "/gridpoints/%s/%u,%u/stations?limit=10", g->wfo, g->x, g->y);
	json_t *root = get_json(url, GEOJSON, NULL, err, sizeof err);
	json_t *features = json_object_get(root, "features");
	bool found = false;
	size_t i;
	json_t *station;
	json_array_foreach(features, i, station) {
		const char *id = string_field(json_object_get(station, "properties"), "stationIdentifier");
		if (id && strlen(id) == 4) {
			snprintf(station_id, 8, "%s", id);
			found = true;
			break;
		}
	}
	json_decref(root);
	return found;
}

static bool get_regional_observation(const char *station_id, ObsReading *out)
{
	char url[128], err[128];
	snprintf(url, sizeof url, API "/stations/%s/observations/latest", station_id);
	time_t now;
	json_t *root = get_json(url, GEOJSON, &now, err, sizeof err);
	if (!root)
		return false;
	json_t *props = json_object_get(root, "properties");
	*out = reading_from_json(props);
	obs_augment_with_metar(out, string_field(props, "rawMessage"));
	json_decref(root);

	ObsReading mapclick;
	if (regional_obs_needs_mapclick(out, now) && get_mapclick(station_id, now, &mapclick) &&
	    regional_obs_prefer_mapclick(out, &mapclick))
		*out = mapclick;
	return true;
}

bool nws_get_regional(RegionalSource *source, RegionalData *out)
{
	RegionalSite *site = &source->site;
	if (!site->has_gridpoint) {
		if (!get_gridpoint(site->lat, site->lon, &site->gridpoint))
			return false;
		site->has_gridpoint = true;
	}

	static Forecast forecast;
	char url[128], err[128];
	snprintf(url, sizeof url, API "/gridpoints/%s/%u,%u/forecast?units=us", site->gridpoint.wfo, site->gridpoint.x,
		site->gridpoint.y);
	if (!get_forecast(url, &forecast, err, sizeof err))
		return false;

	ObsReading obs;
	if (!source->station_id[0] && !get_regional_station(&site->gridpoint, source->station_id))
		return false;
	return get_regional_observation(source->station_id, &obs) && regional_data(&obs, &forecast, out);
}

// Expands one gridpoint series into `measures`, or into `weather` flags for
// the weather series. Returns the number of hours filled.
static size_t expand_series(json_t *props, const char *key, time_t first_hour, Measure *measures, unsigned *weather)
{
	json_t *values = json_object_get(json_object_get(props, key), "values");
	size_t len = 0, i;
	json_t *item;
	json_array_foreach(values, i, item) {
		time_t start, end;
		if (!hourly_parse_valid_time(string_field(item, "validTime"), &start, &end))
			continue;
		json_t *value = json_object_get(item, "value");
		Measure m = { json_is_number(value), json_number_value(value) };
		unsigned flags = 0;
		if (weather) {
			size_t j;
			json_t *phenomenon;
			json_array_foreach(value, j, phenomenon)
				flags |= hourly_weather_flags(string_field(phenomenon, "weather"));
		}
		size_t added = hourly_expand(start, end, first_hour, len, HOURLY_MAX_HOURS);
		for (size_t k = 0; k < added; k++, len++) {
			if (weather)
				weather[len] = flags;
			else
				measures[len] = m;
		}
	}
	return len;
}

bool nws_get_hourly(const NwsPoint *point, HourlyForecast *out, char *err, size_t err_len)
{
	if (!point->forecast_grid_data_url[0]) {
		snprintf(err, err_len, "No gridpoint data for this location");
		return false;
	}
	time_t now;
	json_t *root = get_json(point->forecast_grid_data_url, GEOJSON, &now, err, err_len);
	if (!root)
		return false;
	if (!now) {
		json_decref(root);
		snprintf(err, err_len, "Gridpoint data has no server time");
		return false;
	}
	json_t *props = json_object_get(root, "properties");
	*out = (HourlyForecast){ .start = now - now % 3600 };
	out->count = expand_series(props, "temperature", out->start, out->temperature, NULL);
	expand_series(props, "apparentTemperature", out->start, out->apparent_temperature, NULL);
	expand_series(props, "dewpoint", out->start, out->dewpoint, NULL);
	expand_series(props, "windSpeed", out->start, out->wind_speed, NULL);
	expand_series(props, "windDirection", out->start, out->wind_direction, NULL);
	expand_series(props, "skyCover", out->start, out->sky_cover, NULL);
	expand_series(props, "probabilityOfPrecipitation", out->start, out->probability_of_precipitation, NULL);
	expand_series(props, "snowfallAmount", out->start, out->snowfall_amount, NULL);
	expand_series(props, "iceAccumulation", out->start, out->ice_accumulation, NULL);
	expand_series(props, "weather", out->start, NULL, out->weather);
	json_decref(root);
	if (out->count == 0) {
		snprintf(err, err_len, "Gridpoint data has no current hours");
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

typedef struct {
	char name[32];
	int prob;
	bool night;
} IconCondition;

// Port of ws4kp icons-parse.mjs.
static bool parse_icon_url(const char *icon_url, IconCondition *out)
{
	// Icon URLs look like .../icons/land/{day|night}/{cond}[,prob][/{cond}[,prob]][?size=...]
	const char *p = strstr(icon_url, "/icons/");
	if (!p)
		return false;
	p = strchr(p + 7, '/'); // skip the icon set
	if (!p)
		return false;
	out->night = strncmp(p + 1, "night/", 6) == 0;
	if (!out->night && strncmp(p + 1, "day/", 4) != 0)
		return false;
	const char *cond = p + 1 + (out->night ? 6 : 4);
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
	bool use_second = slash && strcmp(first, second) != 0;
	snprintf(out->name, sizeof out->name, "%s", use_second ? second : first);
	out->prob = use_second ? second_prob : first_prob;
	return true;
}

// Port of ws4kp icons-large.mjs.
IconId nws_large_icon(const char *icon_url)
{
	IconCondition c;
	if (!parse_icon_url(icon_url, &c))
		return ICON_NO_DATA;
	const char *name = c.name;
	bool night = c.night;
	int prob = c.prob;

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

// Port of ws4kp icons-small.mjs.
IconId nws_small_icon(const char *icon_url)
{
	IconCondition c;
	if (!parse_icon_url(icon_url, &c))
		return ICON_SMALL_SUNNY;
	const char *name = c.name;
	bool night = c.night;

#define IS(s) (strcmp(name, s) == 0)
	if (IS("skc"))
		return night ? ICON_SMALL_CLEAR_1992 : ICON_SMALL_SUNNY;
	if (IS("few"))
		return night ? ICON_SMALL_PARTLY_CLEAR_1994 : ICON_SMALL_PARTLY_CLOUDY;
	if (IS("sct"))
		return night ? ICON_SMALL_PARTLY_CLOUDY_NIGHT : ICON_SMALL_PARTLY_CLOUDY;
	if (IS("bkn"))
		return night ? ICON_SMALL_PARTLY_CLEAR_1994 : ICON_SMALL_MOSTLY_CLOUDY_1994;
	if (IS("ovc"))
		return ICON_SMALL_CLOUDY;
	if (IS("fog"))
		return ICON_SMALL_FOG;
	if (IS("rain"))
		return ICON_SMALL_RAIN_1992;
	if (IS("rain_showers") || IS("rain_showers_hi"))
		return night ? ICON_SMALL_SCATTERED_SHOWERS_NIGHT_1994 : ICON_SMALL_SCATTERED_SHOWERS_1994;
	if (IS("snow"))
		return c.prob > 50 ? ICON_SMALL_HEAVY_SNOW_1994 : ICON_SMALL_LIGHT_SNOW;
	if (IS("rain_snow"))
		return ICON_SMALL_RAIN_SNOW_1992;
	// ws4kp has no night case for rain_sleet, so it falls back to Clear.
	if (IS("rain_sleet") && !night)
		return ICON_SMALL_RAIN_SLEET;
	if (IS("snow_sleet"))
		return ICON_SMALL_SNOW_SLEET;
	if (IS("sleet"))
		return ICON_SMALL_SLEET;
	if (IS("fzra") || IS("rain_fzra"))
		return ICON_SMALL_FREEZING_RAIN_1992;
	if (IS("snow_fzra"))
		return ICON_SMALL_FREEZING_RAIN_SNOW_1994;
	if (IS("tsra") || IS("tsra_sct"))
		return night ? ICON_SMALL_SCATTERED_TSTORMS_NIGHT_1994 : ICON_SMALL_SCATTERED_TSTORMS_1994;
	if (IS("tsra_hi") || IS("tornado") || IS("hurricane") || IS("tropical_storm"))
		return ICON_SMALL_THUNDERSTORM;
	if (IS("wind_skc"))
		return night ? ICON_SMALL_CLEAR_WIND_1994 : ICON_SMALL_SUNNY_WIND_1994;
	if (IS("wind_few") || IS("wind_"))
		return ICON_SMALL_WIND;
	if (IS("wind_sct"))
		return night ? ICON_SMALL_CLEAR_WIND_1994 : ICON_SMALL_WIND;
	if (IS("wind_bkn") || IS("wind_ovc"))
		return ICON_SMALL_CLOUDY_WIND;
	if (IS("dust") || IS("smoke"))
		return ICON_SMALL_SMOKE;
	if (IS("haze"))
		return ICON_SMALL_HAZE;
	if (IS("hot") && !night)
		return ICON_SMALL_HOT;
	if (IS("cold"))
		return ICON_SMALL_COLD;
	if (IS("blizzard"))
		return ICON_SMALL_BLOWING_SNOW;
#undef IS
	return night ? ICON_SMALL_CLEAR_1992 : ICON_SMALL_SUNNY;
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

// Whether a GeoJSON Polygon's rings, the area and then its holes, contain the point.
static bool polygon_contains(json_t *rings, double lon, double lat)
{
	size_t i;
	json_t *ring;
	json_array_foreach(rings, i, ring) {
		size_t n = json_array_size(ring);
		double (*points)[2] = malloc(n * sizeof *points);
		if (n && !points)
			return false;
		for (size_t k = 0; k < n; k++) {
			json_t *point = json_array_get(ring, k);
			points[k][0] = json_number_value(json_array_get(point, 0));
			points[k][1] = json_number_value(json_array_get(point, 1));
		}
		bool inside = spc_ring_contains(points, n, lon, lat);
		free(points);
		if (inside != (i == 0))
			return false;
	}
	return json_array_size(rings) > 0;
}

static bool geometry_contains(json_t *geometry, double lon, double lat)
{
	const char *type = string_field(geometry, "type");
	json_t *coordinates = json_object_get(geometry, "coordinates");
	if (type && strcmp(type, "Polygon") == 0)
		return polygon_contains(coordinates, lon, lat);
	if (type && strcmp(type, "MultiPolygon") == 0) {
		size_t i;
		json_t *polygon;
		json_array_foreach(coordinates, i, polygon)
			if (polygon_contains(polygon, lon, lat))
				return true;
	}
	return false;
}

bool nws_get_spc_outlook(double lat, double lon, SpcOutlook *out)
{
	bool any = false;
	for (int day = 0; day < SPC_DAYS; day++) {
		out->risk[day] = SPC_NONE;
		char url[96], err[128];
		snprintf(url, sizeof url, "https://www.spc.noaa.gov/products/outlook/day%dotlk_cat.nolyr.geojson", day + 1);
		json_t *root = get_json(url, GEOJSON, NULL, err, sizeof err);
		json_t *features = json_object_get(root, "features");
		any |= json_is_array(features);
		size_t i;
		json_t *feature;
		json_array_foreach(features, i, feature) {
			SpcRisk risk = spc_risk(string_field(json_object_get(feature, "properties"), "LABEL"));
			if (risk > out->risk[day] && geometry_contains(json_object_get(feature, "geometry"), lon, lat))
				out->risk[day] = risk;
		}
		json_decref(root);
	}
	return any;
}
