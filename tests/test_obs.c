// Host tests for observation selection and time parsing: make test
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "util/utc.h"
#include "weather/obs.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)
#define CHECK_NEAR(a, b) CHECK(fabs((a) - (b)) < 0.01)
#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

static Measure some(double v)
{
	return (Measure){ true, v };
}

// A complete API reading at `timestamp`.
static ObsReading full_reading(time_t timestamp)
{
	return (ObsReading){
		.timestamp = timestamp,
		.text_description = "Clear",
		.temperature = some(29),
		.dewpoint = some(18),
		.wind_direction = some(270),
		.wind_speed = some(14.8),
		.pressure = some(101456),
		.visibility = some(16093),
		.relative_humidity = some(51.5),
	};
}

static void test_time_parsing(void)
{
	time_t t;
	CHECK(utc_parse_http_date("Wed, 14 Oct 2026 16:49:27 GMT", &t) && t == 1791996567);
	CHECK(utc_parse_iso8601("2026-10-14T16:05:00+00:00", &t) && t == 1791993900);
	CHECK(utc_parse_iso8601("2026-10-14T12:05:00-04:00", &t) && t == 1791993900);
	CHECK(utc_parse_iso8601("2026-10-14T16:05:00.250Z", &t) && t == 1791993900);
	CHECK(!utc_parse_iso8601("2026-10-14", &t));

	CHECK(obs_parse_mapclick_date("14 Oct 11:52 am EDT", 2026, &t) && t == 1791993120);
	// 24-hour times with a meridiem, as MapClick sometimes sends.
	CHECK(obs_parse_mapclick_date("18 Jun 23:53 pm EDT", 2026, &t) && t == 1781841180);
	CHECK(obs_parse_mapclick_date("5 Jan 12:05 am EST", 2026, &t) && t == 1767589500);
	CHECK(!obs_parse_mapclick_date("14 Oct 11:52 am HST", 2026, &t));
}

static void test_metar_fills_only_missing_fields(void)
{
	ObsReading r = { .has_cloud_layer = true, .temperature = some(30) };
	obs_augment_with_metar(&r, "KDCA 141552Z VRB06KT 1 1/2SM BKN150 OVC200 29/18 A2996 RMK AO2 T02940183");
	CHECK_NEAR(r.temperature.value, 30); // the API value wins
	CHECK_NEAR(r.dewpoint.value, 18);
	CHECK_NEAR(r.wind_speed.value, 6 * 1.852);
	CHECK(!r.wind_direction.valid); // variable
	CHECK(!r.wind_gust.valid);
	CHECK_NEAR(r.visibility.value, 2400); // 1.5 mi, rounded to 0.1 km
	CHECK_NEAR(r.pressure.value, 101456);
	CHECK_NEAR(r.relative_humidity.value, 49); // from 30/18
	CHECK_NEAR(r.ceiling.value, 4572); // lowest BKN/OVC layer

	ObsReading g = { 0 };
	obs_augment_with_metar(&g, "KORD 141551Z 27015G25KT 10SM FEW040 M05/M10 Q1013");
	CHECK_NEAR(g.wind_direction.value, 270);
	CHECK_NEAR(g.wind_gust.value, 25 * 1.852);
	CHECK_NEAR(g.temperature.value, -5);
	CHECK_NEAR(g.dewpoint.value, -10);
	CHECK_NEAR(g.pressure.value, 101300);
	CHECK(!g.ceiling.valid); // no layer to patch
}

static void test_quality_rules(void)
{
	ObsReading r = full_reading(1000);
	CHECK(obs_quality_ok(obs_quality(&r)));
	r.visibility.valid = false;
	CHECK(obs_quality_ok(obs_quality(&r))); // one optional gap is fine
	r.has_cloud_layer = true; // with a null base
	CHECK(!obs_quality_ok(obs_quality(&r)));
	r = full_reading(1000);
	r.text_description[0] = '\0';
	CHECK(!obs_quality_ok(obs_quality(&r)));
}

static void test_mapclick_selection(void)
{
	time_t now = 1791996567;
	ObsReading api = full_reading(now - 600);
	CHECK(!obs_needs_mapclick(&api, now));
	CHECK(obs_needs_mapclick(&api, now + OBS_STALE_SECONDS)); // stale

	MapClickObservation m = {
		.date = "14 Oct 11:52 am EDT", .temp = "85", .dewp = "65", .relh = "51", .winds = "7", .windd = "999",
		.gust = "NA", .weather = "Mostly Cloudy", .weatherimage = "bkn.png", .visibility = "10.00",
		.slp = "29.96", .windchill = "NA",
	};
	ObsReading mc;
	obs_from_mapclick(&m, now, &mc);
	CHECK(mc.timestamp == 1791993120);
	CHECK_STR(mc.text_description, "Mostly Cloudy");
	CHECK_STR(mc.icon_url, "/icons/land/day/bkn?size=medium");
	CHECK_NEAR(mc.temperature.value, (85 - 32) * 5 / 9.0);
	CHECK_NEAR(mc.wind_direction.value, 999);
	CHECK_NEAR(mc.wind_speed.value, 7 * 1.60934);
	CHECK(!mc.wind_gust.valid && !mc.wind_chill.valid && !mc.heat_index.valid);
	CHECK(!mc.has_cloud_layer);

	// The KDCA case: the newest API reading has no description.
	api.text_description[0] = '\0';
	api.timestamp = 1791993900; // newer than MapClick
	CHECK(obs_needs_mapclick(&api, now));
	CHECK(obs_prefer_mapclick(&api, &mc));

	// Fresher MapClick data is rejected if it loses a required field.
	ObsReading good = full_reading(1000);
	mc.timestamp = 2000;
	mc.temperature.valid = false;
	CHECK(!obs_prefer_mapclick(&good, &mc));

	MapClickObservation night = { .weatherimage = "hi_ntsra.png" };
	obs_from_mapclick(&night, now, &mc);
	CHECK_STR(mc.icon_url, "/icons/land/night/tsra_hi?size=medium");
	CHECK(mc.timestamp == now); // unparseable date falls back to now
	night.weatherimage = "nskc.png";
	obs_from_mapclick(&night, now, &mc);
	CHECK_STR(mc.icon_url, "/icons/land/night/skc?size=medium");
}

static void test_merge_backfills(void)
{
	ObsReading readings[2] = { full_reading(5000), full_reading(4000) };
	readings[0].dewpoint.valid = false;
	readings[1].dewpoint = some(17);
	readings[0].ceiling.valid = false;
	readings[1].has_cloud_layer = true;
	readings[1].ceiling = some(3810);
	readings[1].pressure = some(101300);

	Observation o;
	obs_merge(readings, 2, 5000 + OBS_STALE_SECONDS, &o);
	CHECK_NEAR(o.dewpoint.value, 17);
	CHECK(!o.ceiling.valid); // never backfilled
	CHECK_NEAR(o.pressure.value, 101456);
	CHECK_NEAR(o.pressure_previous.value, 101300);
	CHECK(!o.stale);
	obs_merge(readings, 2, 5001 + OBS_STALE_SECONDS, &o);
	CHECK(o.stale);
}

int main(void)
{
	test_time_parsing();
	test_metar_fills_only_missing_fields();
	test_quality_rules();
	test_mapclick_selection();
	test_merge_backfills();
	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
