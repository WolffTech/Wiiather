// Host tests for Latest Observations selection and formatting: make test
#include <stdio.h>
#include <string.h>

#include "weather/latest_obs.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)
#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

static void test_sort_by_distance(void)
{
	StationDistance s[] = { { 0, 2.0 }, { 1, 0.5 }, { 2, 1.0 }, { 3, 0.5 } };
	latest_obs_sort_by_distance(s, 4);
	CHECK(s[0].index == 1 && s[1].index == 3 && s[2].index == 2 && s[3].index == 0);
	CHECK(latest_obs_distance(38, -77, 41, -73) == 5);
}

static ObsReading complete(void)
{
	ObsReading r = { .timestamp = 1000000,
		.temperature = { true, 20 },
		.wind_speed = { true, 10 },
		.wind_direction = { true, 180 } };
	snprintf(r.text_description, sizeof r.text_description, "Clear");
	return r;
}

static void test_quality(void)
{
	ObsReading r = complete();
	LatestStation s;
	CHECK(latest_obs_missing(&r) == 0);
	CHECK(!latest_obs_needs_mapclick(&r, r.timestamp + OBS_STALE_SECONDS));
	CHECK(latest_obs_needs_mapclick(&r, r.timestamp + OBS_STALE_SECONDS + 1));
	CHECK(latest_obs_accept(&r, "Washington", &s));
	CHECK_STR(s.city, "Washington");

	// Variable wind has no direction, so ws4kp drops the station.
	r.wind_direction.valid = false;
	CHECK(latest_obs_missing(&r) == 1);
	CHECK(latest_obs_needs_mapclick(&r, 0));
	CHECK(!latest_obs_accept(&r, "Washington", &s));

	// MapClick wins when it fills a gap, or is newer without losing fields.
	ObsReading m = complete();
	m.timestamp = r.timestamp - 60;
	CHECK(latest_obs_prefer_mapclick(&r, &m));
	ObsReading api = complete();
	CHECK(!latest_obs_prefer_mapclick(&api, &m));
	m.timestamp = api.timestamp + 60;
	CHECK(latest_obs_prefer_mapclick(&api, &m));
	m.text_description[0] = '\0';
	CHECK(!latest_obs_prefer_mapclick(&api, &m));
}

static LatestStation station(const char *city, const char *text, double temp_c, double kph, double dir)
{
	LatestStation s = { .temperature = temp_c, .wind_speed = kph, .wind_direction = dir };
	snprintf(s.city, sizeof s.city, "%s", city);
	snprintf(s.text_description, sizeof s.text_description, "%s", text);
	return s;
}

static void test_format(void)
{
	LatestObservations obs = { .count = 6,
		.stations = {
			station("Washington", "Mostly Cloudy", 29.4, 14.5, 180),
			station("Chicago / West Chicago", "A Few Clouds", 27.8, 22.5, 292.5),
			station("Camp Springs", "Light Rain and Fog/Mist", -0.3, 0.5, 0),
			station("Dallas, Love Field", "Thunderstorm in Vicinity", 30, 160, 22.5),
			station("Gaithersburg Montgomery", "Light Snow Fog", 0, 1.0, 270),
			station("Chicago/Waukegan", "Partly Cloudy", 20, 100, 337),
		} };
	LatestObsRow rows[LATEST_OBS_STATIONS];
	CHECK(latest_obs_format(&obs, rows) == 6);

	// Kept in distance order, like the live site.
	CHECK_STR(rows[0].location, "Washington");
	CHECK_STR(rows[0].temperature, "85");
	CHECK_STR(rows[0].weather, "M Cloudy");
	CHECK_STR(rows[0].wind, "S   9");

	CHECK_STR(rows[1].location, "West Chicago");
	CHECK_STR(rows[1].temperature, "82");
	CHECK_STR(rows[1].weather, "A F Cloud");
	CHECK_STR(rows[1].wind, "WNW14");

	CHECK_STR(rows[2].location, "Camp Springs");
	CHECK_STR(rows[2].temperature, "31");
	CHECK_STR(rows[2].weather, "L Rain Fo");
	CHECK_STR(rows[2].wind, "Calm");

	CHECK_STR(rows[3].location, "Love Field");
	CHECK_STR(rows[3].weather, "T'storm");
	CHECK_STR(rows[3].wind, "NNE99");

	CHECK_STR(rows[4].location, "Gaithersburg M"); // 14 characters
	CHECK_STR(rows[4].temperature, "32");
	CHECK_STR(rows[4].weather, "L Snw/Fog");
	CHECK_STR(rows[4].wind, "W   1");

	CHECK_STR(rows[5].location, "Waukegan");
	CHECK_STR(rows[5].weather, "P Cloudy");
	CHECK_STR(rows[5].wind, "NNW62");
}

int main(void)
{
	test_sort_by_distance();
	test_quality();
	test_format();
	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
