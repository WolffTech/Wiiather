// Host tests for forecast shaping: make test
#include <stdio.h>
#include <string.h>

#include "weather/forecast.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)
#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

static const char *shorten(const char *text)
{
	static char out[32];
	forecast_shorten(text, out, sizeof out);
	return out;
}

static void test_shorten(void)
{
	CHECK_STR(shorten("Sunny"), "Sunny");
	CHECK_STR(shorten("Chance Showers And Thunderstorms"), "Showers T'Storms");
	CHECK_STR(shorten("Mostly Sunny then Slight Chance Rain Showers"), "Rain Showers");
	CHECK_STR(shorten("Patchy Fog then Mostly Sunny"), "Mostly Sunny");
	CHECK_STR(shorten("Snow And Blowing Snow"), "Snow");
	CHECK_STR(shorten("Rain. Windy"), "Rain");
	CHECK_STR(shorten("Scattered Rain Showers"), "Scattered Rain");
	CHECK_STR(shorten("Thunderstorms Likely"), "T'Storms Likely");
}

static ForecastPeriod period(const char *name, const char *weekday, bool day, int temperature, time_t end)
{
	ForecastPeriod p = { .is_daytime = day, .has_temperature = true, .temperature = temperature, .end = end };
	snprintf(p.name, sizeof p.name, "%s", name);
	snprintf(p.weekday, sizeof p.weekday, "%s", weekday);
	snprintf(p.short_forecast, sizeof p.short_forecast, "Partly Cloudy");
	return p;
}

static void test_periods(void)
{
	char weekday[4];
	forecast_weekday("2026-10-14T06:00:00-04:00", weekday);
	CHECK_STR(weekday, "Wed");
	forecast_weekday("1969-12-31T23:00:00Z", weekday);
	CHECK_STR(weekday, "Wed");
	CHECK_STR(forecast_weekday_name(weekday), "Wednesday");
	CHECK_STR(forecast_weekday_name(""), "");

	Forecast f = { .count = 5 };
	f.periods[0] = period("This Afternoon", "Wed", true, 84, 100);
	f.periods[1] = period("Tonight", "Wed", false, 60, 200);
	f.periods[2] = period("Thursday", "Thu", true, 80, 300);
	f.periods[3] = period("Thursday Night", "Thu", false, 62, 400);
	f.periods[4] = period("Friday", "Fri", true, 82, 500);
	forecast_drop_expired(&f, 100);
	CHECK(f.count == 4);
	CHECK_STR(f.periods[0].name, "Tonight");

	// A leading night period is skipped; a trailing day has no low.
	ExtendedDay days[6];
	CHECK(forecast_extended_days(&f, days, 6) == 2);
	CHECK_STR(days[0].weekday, "Thu");
	CHECK(days[0].has_high && days[0].high == 80 && days[0].has_low && days[0].low == 62);
	CHECK_STR(days[0].text, "Partly Cloudy");
	CHECK_STR(days[1].weekday, "Fri");
	CHECK(days[1].has_high && !days[1].has_low);

	snprintf(f.periods[0].detailed_forecast, sizeof f.periods[0].detailed_forecast,
		"Clear...then cloudy. Low around 60...not as warm.");
	char text[256];
	forecast_local_text(&f.periods[0], text, sizeof text);
	CHECK_STR(text, "Tonight...Clear then cloudy. Low around 60...not as warm.");
}

static void test_travel(void)
{
	Forecast f = { .count = 3 };
	f.periods[0] = period("Today", "Wed", true, 84, 100);
	f.periods[1] = period("Tonight", "Wed", false, 60, 200);
	f.periods[2] = period("Thursday", "Thu", true, 80, 300);
	snprintf(f.periods[0].icon_url, sizeof f.periods[0].icon_url, "/icons/land/day/sct");
	TravelDay day;
	CHECK(forecast_travel_day(&f, &day));
	CHECK_STR(day.weekday, "Wed");
	CHECK(day.high == 84 && day.low == 60);
	CHECK_STR(day.icon_url, "/icons/land/day/sct");

	// Starting at night, the next day's high pairs with the night after it.
	forecast_drop_expired(&f, 100);
	CHECK(!forecast_travel_day(&f, &day));
	f.periods[f.count++] = period("Thursday Night", "Thu", false, 62, 400);
	CHECK(forecast_travel_day(&f, &day));
	CHECK_STR(day.weekday, "Thu");
	CHECK(day.high == 80 && day.low == 62);

	f.periods[2].has_temperature = false;
	CHECK(!forecast_travel_day(&f, &day));

	CHECK(scroll_timing_ms(TRAVEL_SCROLL_TIMING, 1738, 310) == (75 + 714 + 75) * 40);
	CHECK(scroll_timing_ms(TRAVEL_SCROLL_TIMING, 300, 310) == 5000);
}

int main(void)
{
	test_shorten();
	test_periods();
	test_travel();
	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
