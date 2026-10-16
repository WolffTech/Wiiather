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

static void test_displays(void)
{
	Config c;
	write_settings("zip=73069\nhide_display=almanac\nhide_display=hazards\nhide_display=bogus\n");
	config_load(boot, &c);
	CHECK(!config_display_shown(&c, DISPLAY_ALMANAC));
	CHECK(config_display_shown(&c, DISPLAY_HAZARDS));
	CHECK(config_display_shown(&c, DISPLAY_RADAR));

	CHECK(!config_set_display_shown(&c, DISPLAY_HAZARDS, false));
	CHECK(config_set_display_shown(&c, DISPLAY_RADAR, false));
	CHECK(config_save(&c));
	Config loaded;
	config_load(boot, &loaded);
	CHECK(loaded.has_location && loaded.location.zip == 73069);
	CHECK(loaded.hidden_displays == c.hidden_displays);

	// The last display shown can't be hidden.
	CHECK(config_set_display_shown(&c, DISPLAY_RADAR, true));
	for (int i = DISPLAY_CURRENT; i < DISPLAY_RADAR; i++)
		CHECK(config_set_display_shown(&c, i, false));
	CHECK(!config_set_display_shown(&c, DISPLAY_RADAR, false));
	CHECK(config_display_shown(&c, DISPLAY_RADAR));

	// Nor can a hand-edited file hide them all.
	write_settings("hide_display=current_conditions\nhide_display=latest_observations\n"
		"hide_display=hourly_forecast\nhide_display=hourly_graph\nhide_display=travel_forecast\n"
		"hide_display=regional_forecast\nhide_display=local_forecast\nhide_display=extended_forecast\n"
		"hide_display=almanac\nhide_display=spc_outlook\nhide_display=local_radar\n");
	config_load(boot, &c);
	CHECK(c.hidden_displays == 0);
}

static void test_music(void)
{
	Config c;
	write_settings("zip=73069\n");
	config_load(boot, &c);
	CHECK(c.music && c.music_order == MUSIC_SHUFFLE && c.music_volume == 75);

	write_settings("music=off\nmusic_order=in_order\nmusic_volume=42\nskip_track=Night Drive.mp3\n");
	config_load(boot, &c);
	CHECK(!c.music && c.music_order == MUSIC_IN_ORDER && c.music_volume == 40);
	CHECK(!config_track_enabled(&c, "Night Drive.mp3"));
	CHECK(config_track_enabled(&c, "Crisp Day.mp3"));

	CHECK(config_set_track_enabled(&c, "Crisp Day.mp3", false));
	CHECK(config_set_track_enabled(&c, "Night Drive.mp3", true));
	CHECK(config_set_track_enabled(&c, "Crisp Day.mp3", false));
	CHECK(c.skipped_count == 1);
	CHECK(config_save(&c));
	Config loaded;
	config_load(boot, &loaded);
	CHECK(!loaded.music && loaded.music_order == MUSIC_IN_ORDER && loaded.music_volume == 40);
	CHECK(loaded.skipped_count == 1 && !config_track_enabled(&loaded, "Crisp Day.mp3"));

	// Out-of-range volumes keep the default.
	write_settings("music_volume=250\n");
	config_load(boot, &c);
	CHECK(c.music_volume == 75);
}

static void test_general(void)
{
	Config c;
	write_settings("zip=73069\n");
	config_load(boot, &c);
	CHECK(c.units == UNITS_US && c.speed == SPEED_NORMAL && !c.clock_24h && c.bottom_bar && c.locate_by_ip);
	CHECK(config_speed_percent(c.speed) == 100);

	write_settings("units=metric\nspeed=very_slow\nclock=24\nbottom_bar=off\nlocate_by_ip=off\n");
	config_load(boot, &c);
	CHECK(c.units == UNITS_METRIC && c.speed == SPEED_VERY_SLOW && c.clock_24h && !c.bottom_bar && !c.locate_by_ip);
	CHECK(config_speed_percent(SPEED_VERY_FAST) == 50 && config_speed_percent(SPEED_VERY_SLOW) == 150);

	c.speed = SPEED_FAST;
	CHECK(config_save(&c));
	Config loaded;
	config_load(boot, &loaded);
	CHECK(loaded.units == UNITS_METRIC && loaded.speed == SPEED_FAST && loaded.clock_24h && !loaded.bottom_bar && !loaded.locate_by_ip);
}

static void test_reset(void)
{
	Config c;
	config_load(boot, &c);
	CHECK(c.has_location);
	config_reset(&c);
	CHECK(!c.has_location);
	CHECK(strcmp(c.dir, dir) == 0);
	CHECK(c.music && c.music_volume == 75);

	Config loaded;
	config_load(boot, &loaded);
	CHECK(!loaded.has_location);
}

int main(void)
{
	snprintf(dir, sizeof dir, "/tmp/wiiather-test-XXXXXX");
	if (!mkdtemp(dir))
		return 1;
	snprintf(boot, sizeof boot, "%s/boot.dol", dir);
	test_load();
	test_save();
	test_reset();
	test_displays();
	test_music();
	test_general();

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
