#include "radar.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "util/utc.h"

// Keeps the newest RADAR_IMAGES distinct times, oldest first.
static void add_time(RadarTimes *times, time_t t)
{
	size_t i = times->count;
	while (i > 0 && times->times[i - 1] > t)
		i--;
	if (i > 0 && times->times[i - 1] == t)
		return;
	if (times->count == RADAR_IMAGES) {
		if (i == 0)
			return; // older than all of them
		// Drop the oldest to make room.
		memmove(times->times, times->times + 1, (i - 1) * sizeof times->times[0]);
		times->times[i - 1] = t;
		return;
	}
	memmove(times->times + i + 1, times->times + i, (times->count - i) * sizeof times->times[0]);
	times->times[i] = t;
	times->count++;
}

static bool parse_digits(const char *s, size_t n, int *out)
{
	*out = 0;
	for (size_t i = 0; i < n; i++) {
		if (!isdigit((unsigned char)s[i]))
			return false;
		*out = *out * 10 + (s[i] - '0');
	}
	return true;
}

// "YYYYMMDDHHMM.png\"" to UTC.
static bool parse_name_time(const char *s, time_t *out)
{
	int year, month, day, hour, minute;
	if (!parse_digits(s, 4, &year) || !parse_digits(s + 4, 2, &month) || !parse_digits(s + 6, 2, &day) ||
		!parse_digits(s + 8, 2, &hour) || !parse_digits(s + 10, 2, &minute) || strncmp(s + 12, ".png\"", 5) != 0)
		return false;
	if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59)
		return false;
	*out = (time_t)utc_days_from_civil(year, month, day) * 86400 + hour * 3600 + minute * 60;
	return true;
}

size_t radar_add_listing(RadarTimes *times, const char *html)
{
	static const char prefix[] = "href=\"n0r_";
	size_t found = 0;
	for (const char *p = strstr(html, prefix); p; p = strstr(p, prefix)) {
		p += sizeof prefix - 1;
		time_t t;
		if (parse_name_time(p, &t)) {
			add_time(times, t);
			found++;
		}
	}
	return found;
}

static double coerce(double low, double value, double high)
{
	return fmax(fmin(value, high), low);
}

// Nearest neighbor scaling of `size` pixels to `canvas` pixels, as the
// browser draws with image smoothing off.
static int32_t nearest(int32_t canvas_pixel, uint32_t size, uint32_t canvas)
{
	return (int32_t)((canvas_pixel + 0.5) * size / canvas);
}

void radar_crop(double lat, double lon, uint32_t src_w, uint32_t src_h, RadarCrop *out)
{
	// getXYFromLatitudeLongitudeDoppler doubles its result and the processor
	// halves it again, then rounds like Math.round.
	double x = coerce(0, ((-129.138 - lon) * 42.1768) * -1 - 240, 2800);
	double y = coerce(0, (51 - lat) * 61.4481 - 138, 6000);
	int32_t left = (int32_t)floor(x + 0.5), top = (int32_t)floor(y + 0.5);
	for (int32_t i = 0; i < RADAR_WIDTH; i++) {
		int32_t cx = left + i;
		out->src_x[i] = cx < RADAR_CANVAS_WIDTH ? nearest(cx, src_w, RADAR_CANVAS_WIDTH) : -1;
	}
	for (int32_t j = 0; j < RADAR_HEIGHT; j++) {
		int32_t cy = top + j;
		out->src_y[j] = cy < RADAR_CANVAS_HEIGHT ? nearest(cy, src_h, RADAR_CANVAS_HEIGHT) : -1;
	}
}

#define RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define RGBA(r, g, b) ((RGB(r, g, b) << 8) | 0xFF)

uint32_t radar_color(uint8_t r, uint8_t g, uint8_t b)
{
	switch (RGB(r, g, b)) {
	case RGB(0, 0, 0):
	case RGB(0, 236, 236):
	case RGB(1, 160, 246):
	case RGB(0, 0, 246):
		return 0;
	case RGB(0, 255, 0):
		return RGBA(49, 210, 22);
	case RGB(0, 200, 0):
		return RGBA(0, 142, 0);
	case RGB(0, 144, 0):
		return RGBA(20, 90, 15);
	case RGB(255, 255, 0):
		return RGBA(10, 40, 10);
	case RGB(231, 192, 0):
		return RGBA(196, 179, 70);
	case RGB(255, 144, 0):
		return RGBA(190, 72, 19);
	case RGB(214, 0, 0):
	case RGB(255, 0, 0):
		return RGBA(171, 14, 14);
	case RGB(192, 0, 0):
	case RGB(255, 0, 255):
		return RGBA(115, 31, 4);
	default:
		return RGBA(r, g, b);
	}
}

bool radar_palette(const uint8_t (*rgb)[3], size_t count, RadarImage *image, uint8_t remap[256])
{
	memset(remap, 0, 256);
	memset(image->colors, 0, sizeof image->colors);
	size_t used = 1;
	for (size_t i = 0; i < count && i < 256; i++) {
		uint32_t color = radar_color(rgb[i][0], rgb[i][1], rgb[i][2]);
		size_t j = 0;
		while (j < used && image->colors[j] != color)
			j++;
		if (j == used) {
			if (used == 256)
				return false;
			image->colors[used++] = color;
		}
		remap[i] = (uint8_t)j;
	}
	return true;
}

void radar_map_origin(double lat, double lon, int *x, int *y)
{
	// getXYFromLatitudeLongitudeMap for the 680x387 view ws4kp centers on,
	// within its 6800x4255 map; radar-tiles.mjs then floors it.
	*x = (int)floor(coerce(0, (111.407 * lon + 14220.972) + 4 - 680 / 2.0, 6800 - 680));
	*y = (int)floor(coerce(0, (-145.095 * lat + 7377.117) - 27 - 387 / 2.0, 4255 - 387));
}

#define RADAR_BASE_MS 350

static const struct {
	uint8_t time; // in RADAR_BASE_MS units
	uint8_t image; // with RADAR_IMAGES images
} steps[RADAR_STEPS] = {
	{ 4, 5 }, { 1, 0 }, { 1, 1 }, { 1, 2 }, { 1, 3 }, { 1, 4 },
	{ 4, 5 }, { 1, 0 }, { 1, 1 }, { 1, 2 }, { 1, 3 }, { 1, 4 },
	{ 4, 5 }, { 1, 0 }, { 1, 1 }, { 1, 2 }, { 1, 3 }, { 1, 4 },
	{ 12, 5 },
};

uint32_t radar_step_ms(size_t step)
{
	return steps[step].time * RADAR_BASE_MS;
}

size_t radar_step_image(size_t step, size_t count)
{
	size_t missing = RADAR_IMAGES - count;
	return steps[step].image > missing ? steps[step].image - missing : 0;
}

void radar_time_label(time_t local, char out[12])
{
	struct tm tm;
	gmtime_r(&local, &tm);
	int hour = tm.tm_hour % 12 ? tm.tm_hour % 12 : 12;
	snprintf(out, 12, "%s%d:%02d %s", hour < 10 ? "\xa0" : "", hour, tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
}
