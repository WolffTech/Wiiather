// Host tests for settings.ini: make test
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)
#define CHECK_NEAR(a, b) CHECK((a) - (b) < 1e-4 && (b) - (a) < 1e-4)

static char dir[64], boot[96];

static void write_settings(const char *text)
{
	char path[128];
	snprintf(path, sizeof path, "%s/settings.ini", dir);
	FILE *f = fopen(path, "w");
	fputs(text, f);
	fclose(f);
}

static void test_load(void)
{
	Config c;
	config_load(boot, &c);
	CHECK(!c.has_location);
	CHECK(strcmp(c.dir, dir) == 0);

	write_settings("zip=73069\n");
	config_load(boot, &c);
	CHECK(c.has_location && c.location.zip == 73069);
	CHECK_NEAR(c.location.lat, 35.252521);

	// Coordinates win, for places without a ZIP.
	write_settings("zip=73069\r\nlat=38.8951\r\nlon=-77.0364\r\n");
	config_load(boot, &c);
	CHECK(c.has_location && c.location.zip == 73069);
	CHECK_NEAR(c.location.lat, 38.8951);
	CHECK_NEAR(c.location.lon, -77.0364);

	write_settings("zip=99999\nlat=38.8951\n");
	config_load(boot, &c);
	CHECK(!c.has_location);
}

static void test_save(void)
{
	Config c;
	config_load(boot, &c);
	CHECK(location_from_zip(601, &c.location));
	c.has_location = true;
	CHECK(config_save(&c));

	Config loaded;
	config_load(boot, &loaded);
	CHECK(loaded.has_location && loaded.location.zip == 601);
	CHECK_NEAR(loaded.location.lat, c.location.lat);
	CHECK_NEAR(loaded.location.lon, c.location.lon);
}

int main(void)
{
	snprintf(dir, sizeof dir, "/tmp/wiiather-test-XXXXXX");
	if (!mkdtemp(dir))
		return 1;
	snprintf(boot, sizeof boot, "%s/boot.dol", dir);
	test_load();
	test_save();

	char path[128];
	snprintf(path, sizeof path, "%s/settings.ini", dir);
	unlink(path);
	rmdir(dir);
	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
