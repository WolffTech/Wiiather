// Host tests for hourly forecast shaping: make test
#include <stdio.h>
#include <string.h>

#include "util/scroll_timing.h"
#include "util/utc.h"
#include "weather/hourly.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)
#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

#define HOUR 3600

static time_t iso(const char *s)
{
	time_t t = 0;
	utc_parse_iso8601(s, &t);
	return t;
}

static void test_valid_time(void)
{
	time_t start, end;
	CHECK(hourly_parse_valid_time("2026-10-14T20:00:00+00:00/PT1H", &start, &end));
	CHECK(start == iso("2026-10-14T20:00:00Z") && end == start + HOUR);
	CHECK(hourly_parse_valid_time("2026-10-14T20:00:00+00:00/P7DT5H", &start, &end));
	CHECK(end == start + 7 * 86400 + 5 * HOUR);
	CHECK(hourly_parse_valid_time("2026-10-14T20:00:00+00:00/P1D", &start, &end));
	CHECK(end == start + 86400);
	CHECK(!hourly_parse_valid_time("2026-10-14T20:00:00+00:00/P1M", &start, &end));
	CHECK(!hourly_parse_valid_time("2026-10-14T20:00:00+00:00", &start, &end));
	CHECK(!hourly_parse_valid_time(NULL, &start, &end));
}

static void test_expand(void)
{
	time_t first = iso("2026-10-15T04:00:00Z");
	// Only the hours from `first` on count.
	CHECK(hourly_expand(first - 2 * HOUR, first + HOUR, first, 0, 48) == 1);
	CHECK(hourly_expand(first - 3 * HOUR, first, first, 0, 48) == 0);
	CHECK(hourly_expand(first, first + 3 * HOUR, first, 5, 48) == 3);
	// The series stops at its maximum.
	CHECK(hourly_expand(first, first + 3 * HOUR, first, 47, 48) == 1);
	CHECK(hourly_expand(first, first + 3 * HOUR, first, 48, 48) == 0);
}

static void test_weather_flags(void)
{
	CHECK(hourly_weather_flags("thunderstorms") == HOURLY_THUNDER);
	CHECK(hourly_weather_flags("snow_showers") == HOURLY_SNOW);
	CHECK(hourly_weather_flags("freezing_fog") == HOURLY_FOG);
	CHECK(hourly_weather_flags("rain_showers") == 0);
	CHECK(hourly_weather_flags(NULL) == 0);
}

static Measure m(double value)
{
	return (Measure){ true, value };
}

static void test_icon(void)
{
	static HourlyForecast f;
	f = (HourlyForecast){ .count = 1 };
	CHECK(hourly_icon(&f, 0, false) == HOURLY_ICON_SUNNY);
	CHECK(hourly_icon(&f, 0, true) == HOURLY_ICON_CLEAR);
	f.sky_cover[0] = m(60);
	CHECK(hourly_icon(&f, 0, false) == HOURLY_ICON_MOSTLY_CLOUDY);
	CHECK(hourly_icon(&f, 0, true) == HOURLY_ICON_PARTLY_CLEAR);
	f.probability_of_precipitation[0] = m(40);
	CHECK(hourly_icon(&f, 0, true) == HOURLY_ICON_SCATTERED_SHOWERS_NIGHT);
	f.weather[0] = HOURLY_THUNDER;
	CHECK(hourly_icon(&f, 0, false) == HOURLY_ICON_THUNDERSTORM);
	f.snowfall_amount[0] = m(2);
	CHECK(hourly_icon(&f, 0, false) == HOURLY_ICON_THUNDERSNOW);
	// Heavy snow outranks thunder, and blows in wind over 30 km/h.
	f.snowfall_amount[0] = m(12);
	CHECK(hourly_icon(&f, 0, false) == HOURLY_ICON_HEAVY_SNOW);
	f.wind_speed[0] = m(31);
	CHECK(hourly_icon(&f, 0, false) == HOURLY_ICON_BLOWING_SNOW);
	f.ice_accumulation[0] = m(0.5);
	CHECK(hourly_icon(&f, 0, false) == HOURLY_ICON_FREEZING_RAIN);
}

static void test_rows(void)
{
	static HourlyForecast f;
	f = (HourlyForecast){ .start = iso("2026-10-15T03:00:00Z"), .count = 30 };
	for (size_t i = 0; i < f.count; i++) {
		f.temperature[i] = m(22.2); // 72 F
		f.apparent_temperature[i] = m(22.2);
	}
	f.apparent_temperature[1] = m(20); // 68 F
	f.apparent_temperature[2] = m(25); // 77 F
	f.temperature[3].valid = false;
	f.wind_speed[1] = m(12.964); // 8 mph
	f.wind_direction[1] = m(225);
	f.wind_speed[2] = m(19.3); // 12 mph
	f.wind_direction[2] = m(340);
	f.wind_speed[3] = m(0.5);

	// Washington, DC at 12:30 AM EDT: rows start with the current hour.
	HourlyRow rows[HOURLY_ROWS];
	time_t now = iso("2026-10-15T04:30:00Z");
	CHECK(hourly_rows(&f, 38.8951, -77.0364, now, -4 * HOUR, UNITS_US, rows) == HOURLY_ROWS);
	CHECK_STR(rows[0].hour, "Thu 12 AM");
	CHECK_STR(rows[0].temp, " 72");
	CHECK_STR(rows[0].like, " 68");
	CHECK(rows[0].like_color == HOURLY_LIKE_WIND_CHILL);
	CHECK_STR(rows[0].wind, "SW  8");
	CHECK(rows[0].icon == HOURLY_ICON_CLEAR);
	CHECK(rows[1].like_color == HOURLY_LIKE_HEAT_INDEX);
	CHECK_STR(rows[1].wind, "NNW12");
	CHECK_STR(rows[2].temp, "  -");
	CHECK(rows[2].like_color == HOURLY_LIKE_PLAIN);
	CHECK_STR(rows[2].wind, "Calm");
	CHECK_STR(rows[3].wind, "Calm");
	// Sunrise is about 7:15 AM and sunset about 6:25 PM.
	CHECK_STR(rows[6].hour, "Thu 6 AM");
	CHECK(rows[6].icon == HOURLY_ICON_CLEAR);
	CHECK_STR(rows[8].hour, "Thu 8 AM");
	CHECK(rows[8].icon == HOURLY_ICON_SUNNY);
	CHECK_STR(rows[12].hour, "Thu 12 PM");
	CHECK(rows[18].icon == HOURLY_ICON_SUNNY);
	CHECK(rows[19].icon == HOURLY_ICON_CLEAR);

	// Old data shows only the hours it still covers.
	CHECK(hourly_rows(&f, 38.8951, -77.0364, now + 10 * HOUR, -4 * HOUR, UNITS_US, rows) == 19);
	CHECK_STR(rows[0].hour, "Thu 10 AM");
	CHECK(hourly_rows(&f, 38.8951, -77.0364, now + 40 * HOUR, -4 * HOUR, UNITS_US, rows) == 0);

	CHECK(hourly_rows(&f, 38.8951, -77.0364, now, -4 * HOUR, UNITS_METRIC, rows) == HOURLY_ROWS);
	CHECK_STR(rows[0].temp, " 22");
	CHECK_STR(rows[0].like, " 20");
	CHECK_STR(rows[0].wind, "SW 13");
}

static void test_graph(void)
{
	static HourlyForecast f;
	f = (HourlyForecast){ .start = iso("2026-10-15T03:00:00Z"), .count = 48 };
	for (size_t i = 0; i < f.count; i++) {
		f.temperature[i] = m(20); // 68 F
		f.dewpoint[i] = m(10); // 50 F
	}
	f.temperature[5] = m(22.2); // 72 F
	f.dewpoint[6] = m(8.6); // 47.48 F, rounded before scaling
	f.dewpoint[7].valid = false;
	f.sky_cover[2] = m(45);

	// Washington, DC at 12:30 AM EDT on Thursday: the graph starts with the
	// current hour.
	static HourlyGraph g;
	time_t now = iso("2026-10-15T04:30:00Z");
	CHECK(hourly_graph(&f, now, -4 * HOUR, UNITS_US, &g));
	CHECK(g.count == HOURLY_GRAPH_HOURS);
	CHECK(g.temperature[4].value == 72);
	CHECK(g.dewpoint[5].value == 47);
	CHECK(!g.dewpoint[6].valid);
	CHECK(g.sky_cover[1].value == 45);
	CHECK(g.has_temperature_axis && g.temperature_min == 47 && g.temperature_max == 72);
	CHECK_STR(g.y_labels[0], "72\xb0");
	CHECK_STR(g.y_labels[1], "64\xb0"); // 47 + 2 * 25 / 3 = 63.7
	CHECK_STR(g.y_labels[2], "55\xb0");
	CHECK_STR(g.y_labels[3], "47\xb0");
	// Every 9 hours from 12 AM, with the weekday when it changes.
	CHECK_STR(g.x_labels[0], "12A");
	CHECK_STR(g.x_labels[1], "9A");
	CHECK_STR(g.x_labels[2], "6P");
	CHECK_STR(g.x_labels[3], "Fri 3A");
	CHECK_STR(g.x_labels[4], "12P");

	// Three-digit temperatures lose the degree sign; quarters of fewer hours
	// fall between them.
	f.temperature[47] = m(40); // 104 F
	CHECK(hourly_graph(&f, now + 29 * HOUR, -4 * HOUR, UNITS_US, &g));
	CHECK(g.count == 18);
	CHECK_STR(g.y_labels[0], "104");
	CHECK_STR(g.x_labels[0], "5A");
	CHECK_STR(g.x_labels[1], "9A"); // 9:30, 4.5 hours later
	CHECK_STR(g.x_labels[4], "11P");

	CHECK(!hourly_graph(&f, now + 50 * HOUR, -4 * HOUR, UNITS_US, &g));
}

static void test_timing(void)
{
	// 24 rows are 1738 px in ws4kp's 310 px main area: 3 s, 714 counts at
	// 2 px each, then 3 s.
	CHECK(scroll_timing_ms(SCROLL_TIMING_DEFAULT, 1738, 310) == (75 + 714 + 75) * 40);
	CHECK(scroll_timing_offset(SCROLL_TIMING_DEFAULT, 1738, 310, 3040) == 2);
	CHECK(scroll_timing_offset(SCROLL_TIMING_DEFAULT, 1738, 310, 100000) == 1428);
	CHECK(scroll_timing_ms(SCROLL_TIMING_DEFAULT, 300, 310) == 6000);
}

int main(void)
{
	test_valid_time();
	test_expand();
	test_weather_flags();
	test_icon();
	test_rows();
	test_graph();
	test_timing();
	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
