// Layout numbers are ws4kp's CSS positions (_weather-display.scss) at 640x480.
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include "render/gfx.h"
#include "screens.h"

static const TextStyle title_style = { FONT_STAR4000_32, RGBA_YELLOW, true, false, 0 };
static const TextStyle clock_style = { FONT_STAR4000_SMALL_32, RGBA_WHITE, true, false, 0 };
static const TextStyle scroll_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, 0 };

static bool clock_24h;

void header_set_clock_24h(bool on)
{
	clock_24h = on;
}

void clock_format(const struct tm *tm, bool hours_24, bool seconds, char *out, size_t size)
{
	int hour = hours_24 ? tm->tm_hour : tm->tm_hour % 12 ? tm->tm_hour % 12 : 12;
	int n = snprintf(out, size, hours_24 ? "%02d:%02d" : "%d:%02d", hour, tm->tm_min);
	if (seconds && n >= 0 && (size_t)n < size)
		n += snprintf(out + n, size - (size_t)n, ":%02d", tm->tm_sec);
	if (!hours_24 && n >= 0 && (size_t)n < size)
		snprintf(out + n, size - (size_t)n, " %s", tm->tm_hour < 12 ? "AM" : "PM");
}

static void upcase(char *s)
{
	for (; *s; s++)
		*s = (char)toupper((unsigned char)*s);
}

void header_title_draw(const char *title_top, const char *title_bottom)
{
	gfx_image(IMG_LOGO_CORNER, 50, 30);
	if (title_bottom) {
		gfx_text(&title_style, 170, 27, ALIGN_LEFT, title_top);
		gfx_text(&title_style, 170, 56, ALIGN_LEFT, title_bottom);
	} else {
		gfx_text(&title_style, 170, 40, ALIGN_LEFT, title_top);
	}
}

void header_draw(const char *title_top, const char *title_bottom, bool noaa_logo)
{
	header_title_draw(title_top, title_bottom);
	if (noaa_logo)
		gfx_image(IMG_NOAA, 356, 39);

	// The Wii RTC holds local time, so localtime() with no TZ set yields the
	// wall clock the user configured in the System Menu.
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char clock[16], date[16];
	clock_format(&tm, clock_24h, true, clock, sizeof clock);
	strftime(date, sizeof date, " %a %b ", &tm);
	snprintf(date + 9, sizeof date - 9, "%2d", tm.tm_mday);
	upcase(date);
	gfx_text(&clock_style, 585, 30, ALIGN_RIGHT, clock);
	gfx_text(&clock_style, 585, 52, ALIGN_RIGHT, date);
}

void scroll_draw(const char *text)
{
	gfx_text(&scroll_style, 55, 419, ALIGN_LEFT, text);
}
