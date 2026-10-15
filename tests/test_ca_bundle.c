// Host tests for the CA bundle date: make test
#include <stdio.h>
#include <string.h>

#include "net/ca_bundle.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)

static bool date_of(const char *pem, uint64_t *out)
{
	return ca_bundle_date(pem, strlen(pem), out);
}

int main(void)
{
	uint64_t date = 0, later = 0;
	CHECK(date_of("##\n## Bundle of CA Root Certificates\n##\n"
		      "## Certificate data from Mozilla as of: Fri Sep 25 03:12:01 2026 GMT\n##\n",
		&date));
	CHECK(date == 20260925031201ULL);
	CHECK(date_of("## Certificate data from Mozilla as of: Tue Jan  5 23:00:00 2027 GMT\n", &later));
	CHECK(later == 20270105230000ULL && later > date);

	CHECK(!date_of("-----BEGIN CERTIFICATE-----\n", &date));
	CHECK(!date_of("## Certificate data from Mozilla as of: Fri Sxp 25 03:12:01 2026 GMT\n", &date));
	CHECK(!date_of("## Certificate data from Mozilla as of: Fri Sep 25 2026\n", &date));
	// Month names must match on a three-letter boundary.
	CHECK(!date_of("## Certificate data from Mozilla as of: Fri anF 25 03:12:01 2026 GMT\n", &date));

	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
