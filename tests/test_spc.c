// Host tests for the SPC Outlook's risk labels and polygon test: make test
#include <stdio.h>
#include <string.h>

#include "weather/spc.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)

static void test_risk(void)
{
	CHECK(spc_risk("TSTM") == SPC_TSTM);
	CHECK(spc_risk("MRGL") == SPC_MRGL);
	CHECK(spc_risk("SLGT") == SPC_SLGT);
	CHECK(spc_risk("ENH") == SPC_ENH);
	CHECK(spc_risk("MDT") == SPC_MDT);
	CHECK(spc_risk("HIGH") == SPC_HIGH);
	CHECK(spc_risk("SIGN") == SPC_NONE);
	CHECK(spc_risk(NULL) == SPC_NONE);
}

static void test_ring(void)
{
	// A closed GeoJSON ring shaped like a C, open to the east.
	static const double ring[][2] = { { -100, 30 }, { -90, 30 }, { -90, 33 }, { -97, 33 }, { -97, 37 },
		{ -90, 37 }, { -90, 40 }, { -100, 40 }, { -100, 30 } };
	size_t n = sizeof ring / sizeof ring[0];
	CHECK(spc_ring_contains(ring, n, -93, 31));
	CHECK(spc_ring_contains(ring, n, -98, 35));
	CHECK(!spc_ring_contains(ring, n, -93, 35)); // in the opening
	CHECK(!spc_ring_contains(ring, n, -101, 35));
	CHECK(!spc_ring_contains(ring, n, -95, 41));
}

static void test_has_risk(void)
{
	SpcOutlook outlook = { { SPC_NONE, SPC_NONE, SPC_NONE } };
	CHECK(!spc_outlook_has_risk(&outlook));
	outlook.risk[2] = SPC_TSTM;
	CHECK(spc_outlook_has_risk(&outlook));
}

int main(void)
{
	test_risk();
	test_ring();
	test_has_risk();
	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
