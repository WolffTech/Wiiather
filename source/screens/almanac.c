// Port of ws4kp's Almanac display (almanac.mjs, almanac.ejs, _almanac.scss) in
// its standard layout: a centered grid of sun times, then the moon phases in
// 132px inline blocks centered between 50px side paddings.
#include <ogc/lwp_watchdog.h>
#include <time.h>

#include "render/gfx.h"
#include "screens.h"

#define SUN_TOP (90 + 3)
#define SUN_ROW 30 // grid line-height
#define SUN_GAP 90 // grid column gap
#define MOON_TOP (SUN_TOP + 3 * SUN_ROW + 5 + 7) // grid margin-bottom, moon padding-top
#define MOON_ROW 36
#define MOON_LEFT 50
#define MOON_WIDTH (SCREEN_WIDTH - 2 * MOON_LEFT)
#define TITLE_LEFT (MOON_LEFT + 13)
#define DAY_WIDTH 132
#define ICON_PADDING 10 // left padding that offsets the image's shadow
// The image sits on the baseline of a 36px line, so the line box extends the
// font's 7px descent below it.
#define ICON_LINE (94 + 7)
#define DATE_SHIFT (-10)

static const TextStyle sun_header_style = { FONT_STAR4000_32, RGBA_YELLOW, true, false, SUN_ROW };
static const TextStyle sun_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, SUN_ROW };
static const TextStyle moon_title_style = { FONT_STAR4000_32, RGBA_YELLOW, true, false, MOON_ROW };
static const TextStyle moon_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, MOON_ROW };

// ws4kp shows a degraded full moon one time in 200, decided at startup.
static ImageId full_moon_image = IMG_COUNT;

// The RTC holds local time, so whole days since the epoch are local dates.
static long rtc_day(time_t rtc)
{
	return (long)(rtc / 86400);
}

void almanac_view_prepare(const WeatherSnapshot *snap, AlmanacView *out)
{
	if (full_moon_image == IMG_COUNT)
		full_moon_image = gettime() % 1000 >= 995 ? IMG_MOON_FULL_DEGRADED : IMG_MOON_FULL;
	out->ready = snap->has_utc_offset;
	if (!out->ready)
		return;
	time_t now = time(NULL);
	out->rtc_day = rtc_day(now);
	almanac_format(snap->lat, snap->lon, now, snap->utc_offset, &out->text);
}

bool almanac_view_outdated(const AlmanacView *view)
{
	return view->ready && view->rtc_day != rtc_day(time(NULL));
}

static f32 widest(f32 width, const char *text)
{
	f32 w = gfx_text_width(FONT_STAR4000_32, text);
	return w > width ? w : width;
}

// Three auto-width columns: row labels, then one per day.
static void draw_sun(const AlmanacText *t)
{
	f32 widths[1 + ALMANAC_DAYS];
	widths[0] = widest(widest(0, "Sunrise:"), "Sunset:");
	f32 total = widths[0];
	for (int i = 0; i < ALMANAC_DAYS; i++) {
		const AlmanacDay *day = &t->days[i];
		widths[1 + i] = widest(widest(widest(0, day->weekday), day->sunrise), day->sunset);
		total += SUN_GAP + widths[1 + i];
	}

	f32 x = (SCREEN_WIDTH - total) / 2;
	gfx_text(&sun_style, x + widths[0], SUN_TOP + SUN_ROW, ALIGN_RIGHT, "Sunrise:");
	gfx_text(&sun_style, x + widths[0], SUN_TOP + 2 * SUN_ROW, ALIGN_RIGHT, "Sunset:");
	x += widths[0];
	for (int i = 0; i < ALMANAC_DAYS; i++) {
		const AlmanacDay *day = &t->days[i];
		f32 center = x + SUN_GAP + widths[1 + i] / 2;
		gfx_text(&sun_header_style, center, SUN_TOP, ALIGN_CENTER, day->weekday);
		gfx_text(&sun_style, center, SUN_TOP + SUN_ROW, ALIGN_CENTER, day->sunrise);
		gfx_text(&sun_style, center, SUN_TOP + 2 * SUN_ROW, ALIGN_CENTER, day->sunset);
		x += SUN_GAP + widths[1 + i];
	}
}

static ImageId moon_image(MoonPhase phase)
{
	switch (phase) {
	case MOON_NEW:
		return IMG_MOON_NEW;
	case MOON_FIRST:
		return IMG_MOON_FIRST;
	case MOON_FULL:
		return full_moon_image;
	case MOON_LAST:
		return IMG_MOON_LAST;
	}
	return IMG_MOON_NEW;
}

static void draw_moon(const AlmanacText *t)
{
	gfx_text(&moon_title_style, TITLE_LEFT, MOON_TOP, ALIGN_LEFT, "Moon Data:");
	f32 top = MOON_TOP + MOON_ROW;
	f32 left = MOON_LEFT + (MOON_WIDTH - (f32)t->moon_count * DAY_WIDTH) / 2;
	for (size_t i = 0; i < t->moon_count; i++, left += DAY_WIDTH) {
		const AlmanacMoon *moon = &t->moons[i];
		f32 center = left + DAY_WIDTH / 2.0f;
		gfx_text(&moon_style, center, top, ALIGN_CENTER, almanac_phase_name(moon->phase));
		ImageId image = moon_image(moon->phase);
		f32 icon_center = center + ICON_PADDING / 2.0f;
		gfx_image(image, icon_center - image_assets[image].w / 2.0f, top + MOON_ROW);
		gfx_text(&moon_style, center, top + MOON_ROW + ICON_LINE + DATE_SHIFT, ALIGN_CENTER, moon->date);
	}
}

void almanac_view_draw(const AlmanacView *view)
{
	draw_sun(&view->text);
	draw_moon(&view->text);
}
