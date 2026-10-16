// Host tests for unit conversions: make test
#include <stdio.h>

#include "weather/units.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)

int main(void)
{
	CHECK(units_temperature(20, UNITS_US) == 68);
	CHECK(units_temperature(-40, UNITS_US) == -40);
	CHECK(units_temperature(12.5, UNITS_METRIC) == 13); // rounds .5 up, like Math.round
	CHECK(units_temperature(-2.5, UNITS_METRIC) == -2);

	CHECK(units_temperature_convert(54, UNITS_US, UNITS_US) == 54);
	CHECK(units_temperature_convert(54, UNITS_US, UNITS_METRIC) == 12);
	CHECK(units_temperature_convert(32, UNITS_US, UNITS_METRIC) == 0);
	CHECK(units_temperature_convert(-4, UNITS_US, UNITS_METRIC) == -20);
	CHECK(units_temperature_convert(12, UNITS_METRIC, UNITS_US) == 54);

	CHECK(units_speed(25.9, UNITS_US) == 16);
	CHECK(units_speed(25.9, UNITS_METRIC) == 26);
	CHECK(units_speed(0.4, UNITS_US) == 0);

	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
