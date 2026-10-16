#include "radar_fetch.h"

#include <png.h>
#include <stdio.h>
#include <string.h>

#include "net/https.h"

#define ARCHIVE "https://mesonet.agron.iastate.edu/archive/data"
// ws4kp also reads the previous day's listing this soon after midnight, when
// the newest images may still be filed under it.
#define PREVIOUS_DAY_SECONDS (RADAR_IMAGES * 5 * 60)
// The composite is 6000 pixels wide.
#define MAX_SOURCE_WIDTH 8192

static bool get(const char *url, const char *accept, HttpResponse *res)
{
	char err[128];
	if (!https_get(url, accept, res, err, sizeof err))
		return false;
	if (res->status == 200)
		return true;
	http_response_free(res);
	return false;
}

// Adds the day's images to `times`. Returns how many the day has, 0 if its
// listing failed to load.
static size_t add_day(RadarTimes *times, time_t day)
{
	struct tm tm;
	gmtime_r(&day, &tm);
	char url[128];
	snprintf(url, sizeof url, ARCHIVE "/%04d/%02d/%02d/GIS/uscomp/?F=0&P=n0r*.png", tm.tm_year + 1900,
		tm.tm_mon + 1, tm.tm_mday);
	HttpResponse res;
	if (!get(url, "text/html", &res))
		return 0;
	size_t found = radar_add_listing(times, res.body);
	http_response_free(&res);
	return found;
}

typedef struct {
	const unsigned char *data;
	size_t len, pos;
} PngSource;

static void read_png(png_structp png, png_bytep out, size_t len)
{
	PngSource *src = png_get_io_ptr(png);
	if (len > src->len - src->pos)
		png_error(png, "truncated");
	memcpy(out, src->data + src->pos, len);
	src->pos += len;
}

// Decodes only the rows the crop needs, so the 6000x2600 composite is never
// held whole. Errors longjmp out to decode().
static bool decode_rows(png_structp png, png_infop info, double lat, double lon, RadarImage *out)
{
	png_read_info(png, info);
	if (png_get_color_type(png, info) != PNG_COLOR_TYPE_PALETTE ||
		png_get_interlace_type(png, info) != PNG_INTERLACE_NONE)
		return false;
	png_colorp palette;
	int count;
	if (!png_get_PLTE(png, info, &palette, &count))
		return false;
	uint8_t rgb[256][3];
	for (int i = 0; i < count && i < 256; i++) {
		rgb[i][0] = palette[i].red;
		rgb[i][1] = palette[i].green;
		rgb[i][2] = palette[i].blue;
	}
	uint8_t remap[256];
	if (!radar_palette(rgb, count, out, remap))
		return false;

	png_set_packing(png); // one byte per pixel at any bit depth
	png_read_update_info(png, info);
	static png_byte row[MAX_SOURCE_WIDTH];
	if (png_get_rowbytes(png, info) > sizeof row)
		return false;
	static RadarCrop crop;
	radar_crop(lat, lon, png_get_image_width(png, info), png_get_image_height(png, info), &crop);
	size_t j = 0;
	for (int32_t y = 0; j < RADAR_HEIGHT && crop.src_y[j] >= 0; y++) {
		png_read_row(png, row, NULL);
		for (; j < RADAR_HEIGHT && crop.src_y[j] == y; j++) {
			for (size_t i = 0; i < RADAR_WIDTH; i++)
				out->pixels[j][i] = crop.src_x[i] < 0 ? 0 : remap[row[crop.src_x[i]]];
		}
	}
	// Past the bottom of ws4kp's canvas.
	memset(out->pixels[j], 0, (RADAR_HEIGHT - j) * sizeof out->pixels[0]);
	return true;
}

static bool decode(const HttpResponse *res, double lat, double lon, RadarImage *out)
{
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info = png ? png_create_info_struct(png) : NULL;
	PngSource src = { (const unsigned char *)res->body, res->len, 0 };
	bool ok = false;
	if (info && !setjmp(png_jmpbuf(png))) {
		png_set_read_fn(png, &src, read_png);
		ok = decode_rows(png, info, lat, lon, out);
	}
	png_destroy_read_struct(&png, &info, NULL);
	return ok;
}

static bool get_image(double lat, double lon, time_t time, RadarImage *out)
{
	struct tm tm;
	gmtime_r(&time, &tm);
	char url[128];
	snprintf(url, sizeof url, ARCHIVE "/%04d/%02d/%02d/GIS/uscomp/n0r_%04d%02d%02d%02d%02d.png", tm.tm_year + 1900,
		tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
	HttpResponse res;
	if (!get(url, "image/png", &res))
		return false;
	bool ok = decode(&res, lat, lon, out);
	http_response_free(&res);
	out->time = time;
	return ok;
}

bool radar_fetch(double lat, double lon, time_t now, RadarImages *images)
{
	RadarTimes times = { 0 };
	time_t today = now - now % 86400;
	if (add_day(&times, today) < RADAR_IMAGES || now - today < PREVIOUS_DAY_SECONDS)
		add_day(&times, today - 86400);
	if (times.count == 0)
		return false;

	static RadarImages next;
	next.count = times.count;
	for (size_t i = 0; i < times.count; i++) {
		size_t old = 0;
		while (old < images->count && images->images[old].time != times.times[i])
			old++;
		// ws4kp gives up on the display if any image fails.
		if (old < images->count)
			next.images[i] = images->images[old];
		else if (!get_image(lat, lon, times.times[i], &next.images[i]))
			return false;
	}
	*images = next;
	return true;
}
