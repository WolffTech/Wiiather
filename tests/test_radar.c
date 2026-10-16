// Host tests for the Local Radar's listing, crop, colors and timing: make test
#include <stdio.h>
#include <string.h>

#include "util/utc.h"
#include "weather/radar.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)

static time_t utc(int year, int month, int day, int hour, int minute)
{
	return (time_t)utc_days_from_civil(year, month, day) * 86400 + hour * 3600 + minute * 60;
}

static void test_listing(void)
{
	// Trimmed from the archive's index page.
	static const char today[] =
		"<a href=\"/archive/data/2026/10/16/GIS/\"> Parent Directory</a>\n"
		"<a href=\"n0r_202610160000.png\"> n0r_202610160000.png</a>\n"
		"<a href=\"n0r_202610160010.png\"> n0r_202610160010.png</a>\n"
		"<a href=\"n0r_202610160005.png\"> n0r_202610160005.png</a>\n"
		"<a href=\"n0r_2026101600.png\"> n0r_2026101600.png</a>\n"
		"<a href=\"n0r_202610160015.wld\"> n0r_202610160015.wld</a>\n";
	static const char yesterday[] =
		"<a href=\"n0r_202610152340.png\">x</a><a href=\"n0r_202610152345.png\">x</a>"
		"<a href=\"n0r_202610152350.png\">x</a><a href=\"n0r_202610152355.png\">x</a>";
	RadarTimes times = { 0 };
	CHECK(radar_add_listing(&times, today) == 3);
	CHECK(times.count == 3);
	CHECK(times.times[0] == utc(2026, 10, 16, 0, 0));
	CHECK(times.times[2] == utc(2026, 10, 16, 0, 10));

	// Merging the previous day keeps the newest six, oldest first.
	CHECK(radar_add_listing(&times, yesterday) == 4);
	CHECK(times.count == RADAR_IMAGES);
	CHECK(times.times[0] == utc(2026, 10, 15, 23, 45));
	CHECK(times.times[2] == utc(2026, 10, 15, 23, 55));
	CHECK(times.times[5] == utc(2026, 10, 16, 0, 10));

	// Repeats are ignored.
	CHECK(radar_add_listing(&times, today) == 3);
	CHECK(times.times[0] == utc(2026, 10, 15, 23, 45));
}

static void test_crop(void)
{
	RadarCrop crop;
	// Detroit: the crop starts at canvas (1704, 395) of the 6000x2600 composite.
	radar_crop(42.33, -83.05, 6000, 2600, &crop);
	CHECK(crop.src_x[0] == 4010);
	CHECK(crop.src_x[RADAR_WIDTH - 1] == 4572); // canvas 1943
	CHECK(crop.src_y[0] == 642);
	CHECK(crop.src_y[RADAR_HEIGHT - 1] == 905); // canvas 557

	// Key West runs off the bottom of ws4kp's canvas; the rest is transparent.
	radar_crop(24.55, -81.78, 6000, 2600, &crop);
	CHECK(crop.src_y[112] == 2599);
	CHECK(crop.src_y[113] == -1);
	CHECK(crop.src_y[RADAR_HEIGHT - 1] == -1);
}

static void test_colors(void)
{
	CHECK(radar_color(0, 0, 0) == 0);
	CHECK(radar_color(1, 160, 246) == 0);
	CHECK(radar_color(0, 255, 0) == 0x31D216FF);
	CHECK(radar_color(255, 0, 0) == radar_color(214, 0, 0));
	CHECK(radar_color(255, 0, 255) == 0x731F04FF);
	CHECK(radar_color(12, 34, 56) == 0x0C2238FF); // passed through

	static const uint8_t palette[][3] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 236, 236 }, { 0, 255, 0 },
		{ 255, 0, 0 }, { 214, 0, 0 }, { 192, 0, 0 } };
	static RadarImage image;
	uint8_t remap[256];
	CHECK(radar_palette(palette, sizeof palette / sizeof palette[0], &image, remap));
	CHECK(remap[0] == 0 && remap[1] == 0 && remap[2] == 0);
	CHECK(image.colors[0] == 0);
	CHECK(image.colors[remap[3]] == 0x31D216FF);
	CHECK(remap[4] == remap[5] && remap[4] != remap[3]);
	CHECK(image.colors[remap[6]] == 0x731F04FF);
	CHECK(remap[200] == 0); // past the palette

	// 256 distinct colors plus transparency don't fit.
	static uint8_t gray[256][3];
	for (int i = 0; i < 256; i++)
		gray[i][0] = gray[i][1] = gray[i][2] = (uint8_t)i;
	CHECK(radar_palette(gray, 256, &image, remap)); // black is transparent
	gray[0][2] = 7;
	CHECK(!radar_palette(gray, 256, &image, remap));
}

static void test_map_origin(void)
{
	int x, y;
	radar_map_origin(42.33, -83.05, &x, &y);
	CHECK(x == 4632 && y == 1014);
	// Clamped to the map.
	radar_map_origin(10, -60, &x, &y);
	CHECK(x == 6800 - 680 && y == 4255 - 387);
}

static void test_steps(void)
{
	uint32_t total = 0;
	for (size_t i = 0; i < RADAR_STEPS; i++)
		total += radar_step_ms(i);
	CHECK(total == 39 * 350);
	CHECK(radar_step_ms(0) == 1400);
	CHECK(radar_step_image(0, 6) == 5);
	CHECK(radar_step_image(1, 6) == 0);
	CHECK(radar_step_image(RADAR_STEPS - 1, 6) == 5);
	// Missing images drop off the old end.
	CHECK(radar_step_image(0, 4) == 3);
	CHECK(radar_step_image(1, 4) == 0);
	CHECK(radar_step_image(3, 4) == 0);
	CHECK(radar_step_image(4, 4) == 1);
	CHECK(radar_step_image(0, 1) == 0);
}

static void test_time_label(void)
{
	char label[12];
	radar_time_label(utc(2026, 10, 16, 15, 5), label);
	CHECK(strcmp(label, "\xa0" "3:05 PM") == 0);
	radar_time_label(utc(2026, 10, 16, 0, 30), label);
	CHECK(strcmp(label, "12:30 AM") == 0);
	radar_time_label(utc(2026, 10, 16, 12, 0), label);
	CHECK(strcmp(label, "12:00 PM") == 0);
	radar_time_label(utc(2026, 10, 16, 10, 45), label);
	CHECK(strcmp(label, "10:45 AM") == 0);
}

int main(void)
{
	test_listing();
	test_crop();
	test_colors();
	test_map_origin();
	test_steps();
	test_time_label();
	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
