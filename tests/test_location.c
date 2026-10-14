// Host tests for ZIP lookup and entry: make test
#include <stdio.h>
#include <string.h>

#include "weather/location.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)
#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)
#define CHECK_NEAR(a, b) CHECK((a) - (b) < 1e-4 && (b) - (a) < 1e-4)

static void test_lookup(void)
{
	Location loc;
	CHECK(location_from_zip(73069, &loc));
	CHECK(loc.zip == 73069);
	CHECK_NEAR(loc.lat, 35.252521);
	CHECK_NEAR(loc.lon, -97.464398);

	// First and last rows, so the search covers the table's ends.
	CHECK(location_from_zip(601, &loc));
	CHECK_NEAR(loc.lat, 18.180555);
	CHECK(location_from_zip(99929, &loc));

	CHECK(!location_from_zip(0, &loc));
	CHECK(!location_from_zip(99999, &loc));
	CHECK(!location_from_zip(600, &loc));
}

static void test_nearest_city(void)
{
	char city[64];
	Location norman;
	CHECK(location_from_zip(73069, &norman));
	location_nearest_city(&norman, city, sizeof city);
	CHECK_STR(city, "Norman, OK");
}

static void test_entry(void)
{
	ZipEntry e = zip_entry_make(601);
	CHECK(e.cursor == 0);
	CHECK(zip_entry_value(&e) == 601);
	CHECK(e.digits[0] == 0 && e.digits[2] == 6 && e.digits[4] == 1);

	zip_entry_step(&e, -1);
	CHECK(zip_entry_value(&e) == 90601);
	zip_entry_step(&e, 1);
	zip_entry_step(&e, 1);
	CHECK(zip_entry_value(&e) == 10601);

	zip_entry_move(&e, -1);
	CHECK(e.cursor == ZIP_DIGITS - 1);
	zip_entry_step(&e, 9);
	CHECK(zip_entry_value(&e) == 10600);
	zip_entry_move(&e, 1);
	CHECK(e.cursor == 0);
}

int main(void)
{
	test_lookup();
	test_nearest_city();
	test_entry();
	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
