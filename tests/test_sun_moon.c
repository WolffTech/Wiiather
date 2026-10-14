// Host tests for the almanac's sun and moon logic: make test
// Expected values come from running ws4kp's almanac.mjs logic with its
// vendored suncalc.js and luxon in Node for the same instants and locations.
#include <stdio.h>
#include <string.h>

#include "weather/sun_moon.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)
#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)
// libm and V8 may differ in the last bit, which can move a crossing by 1 ms.
#define CHECK_MS(a, b) CHECK((a) - (b) <= 2 && (b) - (a) <= 2)
#define CHECK_NEAR(a, b) CHECK((a) - (b) < 1e-12 && (b) - (a) < 1e-12)

#define NBSP "\xa0"
#define HOUR 3600

static void test_suncalc(void)
{
	// Washington, DC at 2026-10-14T17:51:00Z.
	int64_t rise, set;
	CHECK(suncalc_sun_times(1792000260000, 38.8951, -77.0364, &rise, &set));
	CHECK_MS(rise, 1791976651678);
	CHECK_MS(set, 1792017183911);
	CHECK(suncalc_sun_times(1792000260000 + 86400000, 38.8951, -77.0364, &rise, &set));
	CHECK_MS(rise, 1792063111446);
	CHECK_MS(set, 1792103496727);
	// Utqiagvik, AK in polar night.
	CHECK(!suncalc_sun_times(1797804000000, 71.2906, -156.7886, &rise, &set));

	CHECK_NEAR(suncalc_moon_phase(1792000260000), 0.13456078681443567);
	CHECK_NEAR(suncalc_moon_phase(1792285380000), 0.2330533215262488);
	CHECK_NEAR(suncalc_moon_phase(1793011140000), 0.5155767776594127);
	CHECK_NEAR(suncalc_moon_phase(1768654080000), 0.9554913911796601);
}

static void test_moon_events(void)
{
	MoonEvent e[5];
	CHECK(almanac_moon_events(1792000260000, e, 5) == 5);
	CHECK(e[0].phase == MOON_FIRST);
	CHECK_MS(e[0].ms, 1792334095646);
	CHECK(e[1].phase == MOON_FULL);
	CHECK_MS(e[1].ms, 1793005118551);
	CHECK(e[2].phase == MOON_LAST);
	CHECK_MS(e[2].ms, 1793572524168);
	CHECK(e[3].phase == MOON_NEW);
	CHECK_MS(e[3].ms, 1794207973648);
	CHECK(e[4].phase == MOON_FIRST);
	CHECK_MS(e[4].ms, 1794906314652);

	// ws4kp reports this new moon at 2023-11-14T05:40:00Z because its minute
	// pass starts from an unwrapped phase; the crossing is 2023-11-13T13:00:39Z.
	CHECK(almanac_moon_events(1699963200000 - 2 * 86400000, e, 1) == 1);
	CHECK(e[0].phase == MOON_NEW);
	CHECK_MS(e[0].ms, 1699880439169);
}

static void check_day(const AlmanacDay *d, const char *weekday, const char *rise, const char *set)
{
	CHECK_STR(d->weekday, weekday);
	CHECK_STR(d->sunrise, rise);
	CHECK_STR(d->sunset, set);
}

static void check_moons(const AlmanacText *t, const MoonPhase phases[4], const char *const dates[4])
{
	CHECK(t->moon_count == 4);
	for (int i = 0; i < 4; i++) {
		CHECK(t->moons[i].phase == phases[i]);
		CHECK_STR(t->moons[i].date, dates[i]);
	}
}

static void test_format(void)
{
	AlmanacText t;
	// Washington, DC, 1:51 PM EDT.
	almanac_format(38.8951, -77.0364, 1792000260 - 4 * HOUR, -4 * HOUR, &t);
	check_day(&t.days[0], "Wednesday", NBSP "7:17 AM", NBSP "6:33 PM");
	check_day(&t.days[1], "Thursday", NBSP "7:18 AM", NBSP "6:31 PM");
	check_moons(&t, (MoonPhase[]){ MOON_FIRST, MOON_FULL, MOON_LAST, MOON_NEW },
		(const char *[]){ "Oct 18", "Oct 26", "Nov 1", "Nov 9" });

	// Anchorage at 12:30 AM AKST Monday. ws4kp would show Sunday's sun times
	// until about 1 AM; these are ws4kp's values later on Monday.
	almanac_format(61.2181, -149.9003, 1767605400 - 9 * HOUR, -9 * HOUR, &t);
	check_day(&t.days[0], "Monday", "10:11 AM", NBSP "4:00 PM");
	check_day(&t.days[1], "Tuesday", "10:10 AM", NBSP "4:02 PM");
	check_moons(&t, (MoonPhase[]){ MOON_LAST, MOON_NEW, MOON_FIRST, MOON_FULL },
		(const char *[]){ "Jan 10", "Jan 18", "Jan 25", "Feb 1" });

	// Utqiagvik in polar night, across the new year.
	almanac_format(71.2906, -156.7886, 1797804000 - 9 * HOUR, -9 * HOUR, &t);
	check_day(&t.days[0], "Sunday", "-", "-");
	check_day(&t.days[1], "Monday", "-", "-");
	check_moons(&t, (MoonPhase[]){ MOON_FULL, MOON_LAST, MOON_NEW, MOON_FIRST },
		(const char *[]){ "Dec 23", "Dec 30", "Jan 7", "Jan 15" });

	// Honolulu, 6:30 PM HST.
	almanac_format(21.3069, -157.8583, 1772944200 - 10 * HOUR, -10 * HOUR, &t);
	check_day(&t.days[0], "Saturday", NBSP "6:48 AM", NBSP "6:39 PM");
	check_day(&t.days[1], "Sunday", NBSP "6:47 AM", NBSP "6:39 PM");
	check_moons(&t, (MoonPhase[]){ MOON_LAST, MOON_NEW, MOON_FIRST, MOON_FULL },
		(const char *[]){ "Mar 10", "Mar 18", "Mar 25", "Apr 1" });
}

int main(void)
{
	test_suncalc();
	test_moon_events();
	test_format();
	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
