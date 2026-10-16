// Host tests for Regional Forecast framing, site selection and data: make test
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "weather/regional.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)
#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)
#define CHECK_NEAR(a, b) CHECK(fabs((a) - (b)) < 0.01)

typedef struct {
	const char *name;
	double x, y;
	bool has_gridpoint;
} Expected;

// Expected values come from ws4kp's own regionalforecast-utils.mjs.
static void check_sites(double lat, double lon, const char *state, const Expected *expected, size_t count)
{
	RegionalFrame frame = regional_frame(lat, lon, state);
	RegionalSite sites[REGIONAL_MAX_SITES];
	size_t n = regional_select_sites(lat, lon, &frame, sites);
	CHECK(n == count);
	for (size_t i = 0; i < n && i < count; i++) {
		CHECK_STR(sites[i].name, expected[i].name);
		CHECK_NEAR(sites[i].x, expected[i].x);
		CHECK_NEAR(sites[i].y, expected[i].y);
		CHECK(sites[i].has_gridpoint == expected[i].has_gridpoint);
	}
}

static void test_frame(void)
{
	RegionalFrame f = regional_frame(42.3314, -83.0458, "MI");
	CHECK(f.map == REGIONAL_MAP_CONUS);
	CHECK_NEAR(f.origin_x, 1617.074);
	CHECK_NEAR(f.origin_y, 333.907);
	CHECK_NEAR(f.min_lat, 40.2118);
	CHECK_NEAR(f.max_lat, 44.4510);
	CHECK_NEAR(f.min_lon, -88.7909);
	CHECK_NEAR(f.max_lon, -77.3007);

	// Near the map's edge, the frame stops at the edge instead of centering.
	f = regional_frame(47.6062, -122.3321, "WA");
	CHECK(f.origin_x == 0);
	CHECK_NEAR(f.origin_y, 42.738);

	CHECK(regional_frame(61.2181, -149.9003, "AK").map == REGIONAL_MAP_ALASKA);
	f = regional_frame(21.3069, -157.8583, "HI");
	CHECK(f.map == REGIONAL_MAP_HAWAII);
	CHECK_NEAR(f.origin_x, 37.457);
	CHECK_NEAR(f.origin_y, 86.859);
}

static void test_sites(void)
{
	// Regional cities only: they're placed before stations and fill the map.
	static const Expected detroit[] = {
		{ "Detroit", 327.343, 148.349, true }, { "Saginaw", 276.129, 72.21, true },
		{ "Cleveland", 404.709, 206.239, true }, { "Fort Wayne", 208.196, 235.947, true },
		{ "Holland", 153.248, 117.416, true }, { "Pittsburgh", 501.233, 280.873, true },
		{ "Clarion", 536.111, 226.546, true }, { "Buffalo", 565.005, 109.59, true },
		{ "Chicago", 66.027, 179.667, true }, { "Sheboygan", 61.421, 49.082, true },
	};
	check_sites(42.3314, -83.0458, "MI", detroit, sizeof detroit / sizeof detroit[0]);

	// Stations fill gaps, with shortened names.
	static const Expected seattle[] = {
		{ "Seattle", 294.724, 148.552, true }, { "Yakima", 398.584, 218.482, true },
		{ "Hermiston", 467.998, 271.969, true }, { "Spokane", 575.039, 144.793, true },
		{ "Scappoose In", 264.272, 276.967, false },
	};
	check_sites(47.6062, -122.3321, "WA", seattle, sizeof seattle / sizeof seattle[0]);

	// ws4kp places Alaska's labels with the mainland scales, so Anchorage
	// itself falls outside and stations take its place.
	static const Expected anchorage[] = {
		{ "Merrill", 550.067, 146.35, false }, { "Homer", 456.967, 256.017, false },
		{ "Iliamna", 262.217, 249.017, false }, { "Sparrevohn", 224.217, 154.517, false },
		{ "Takotna", 200.467, 28.517, false },
	};
	check_sites(61.2181, -149.9003, "AK", anchorage, sizeof anchorage / sizeof anchorage[0]);

	static const Expected honolulu[] = {
		{ "Honolulu", 327.491, 148.51, true }, { "Hana", 432.569, 184.158, false },
	};
	check_sites(21.3069, -157.8583, "HI", honolulu, sizeof honolulu / sizeof honolulu[0]);
}

static void test_city_name(void)
{
	char name[13];
	regional_city_name("Scappoose Industrial Airpark", name);
	CHECK_STR(name, "Scappoose In");
	regional_city_name("Winston-Salem", name);
	CHECK_STR(name, "Winston");
	regional_city_name("St. Louis, MO", name);
	CHECK_STR(name, "St. Louis");
	regional_city_name("A/B", name);
	CHECK_STR(name, "A");
}

static ObsReading reading(time_t timestamp, bool has_temperature)
{
	ObsReading r = { .timestamp = timestamp, .temperature = { has_temperature, 21.4 } };
	snprintf(r.text_description, sizeof r.text_description, "Cloudy");
	snprintf(r.icon_url, sizeof r.icon_url, "https://api.weather.gov/icons/land/night/ovc?size=medium");
	return r;
}

static void test_mapclick(void)
{
	ObsReading fresh = reading(10000, true);
	CHECK(!regional_obs_needs_mapclick(&fresh, 10000 + 60));
	CHECK(regional_obs_needs_mapclick(&fresh, 10000 + 81 * 60));
	ObsReading gap = reading(10000, false);
	CHECK(regional_obs_needs_mapclick(&gap, 10000));

	// MapClick wins when it fills a gap or is newer without losing fields.
	CHECK(regional_obs_prefer_mapclick(&gap, &fresh));
	ObsReading newer = reading(20000, true);
	CHECK(regional_obs_prefer_mapclick(&fresh, &newer));
	CHECK(!regional_obs_prefer_mapclick(&fresh, &fresh));
	ObsReading newer_gap = reading(20000, false);
	CHECK(!regional_obs_prefer_mapclick(&fresh, &newer_gap));
}

static void test_data(void)
{
	Forecast f = { .count = 3 };
	const char *weekdays[] = { "Wed", "Wed", "Thu" };
	for (int i = 0; i < 3; i++) {
		ForecastPeriod *p = &f.periods[i];
		p->is_daytime = i != 1;
		p->has_temperature = i != 2;
		p->temperature = p->has_temperature ? 50 + i : 0;
		snprintf(p->weekday, sizeof p->weekday, "%s", weekdays[i]);
		snprintf(p->icon_url, sizeof p->icon_url, "https://api.weather.gov/icons/land/day/few?size=medium#%d", i);
	}
	ObsReading obs = reading(10000, true);
	RegionalData d;
	CHECK(regional_data(&obs, &f, &d));
	CHECK(d.periods[0].temperature == 71); // 21.4 degC
	CHECK(!d.periods[0].is_daytime);
	CHECK_STR(d.periods[0].icon_url, obs.icon_url);
	CHECK(d.periods[1].temperature == 51 && !d.periods[1].is_daytime);
	CHECK_STR(d.periods[1].weekday, "Wed");
	CHECK(d.periods[2].temperature == 0 && d.periods[2].is_daytime);
	CHECK_STR(d.periods[2].weekday, "Thu");
	CHECK_STR(d.periods[2].icon_url, f.periods[2].icon_url);

	// Every required observation field and three periods are needed.
	obs.icon_url[0] = '\0';
	CHECK(!regional_data(&obs, &f, &d));
	obs = reading(10000, true);
	f.count = 2;
	CHECK(!regional_data(&obs, &f, &d));
}

int main(void)
{
	test_frame();
	test_sites();
	test_city_name();
	test_mapclick();
	test_data();
	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
