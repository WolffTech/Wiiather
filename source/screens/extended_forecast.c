// Port of ws4kp's Extended Forecast (extendedforecast.mjs, _extended-forecast.scss).
// Each day is a 155px column: weekday, a 75px icon row, a two-line
// condition, then adjacent Lo/Hi blocks 44% wide (the served HTML is minified,
// so no whitespace separates the inline blocks).
#include <ctype.h>
#include <stdio.h>

#include "screens.h"
#include "weather/nws.h"

#define DAY_LEFT 47 // 27px container margin + 15px day margin + 5px padding
#define DAY_PITCH 195 // 155px + 2 * 5px padding + 2 * 15px margin
#define DAY_WIDTH 155
#define DAY_TOP (90 + 16 + 5)
#define ICON_HEIGHT 75
#define CONDITION_GAP 5
#define CONDITION_HEIGHT 74
#define TEMP_BLOCK_WIDTH (DAY_WIDTH * 0.44f)
#define VALUE_GAP 4

static const TextStyle date_style = { FONT_STAR4000_32, RGBA_YELLOW, true, false, 0 };
static const TextStyle text_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, 0 };
static const TextStyle low_style = { FONT_STAR4000_32, RGBA_EXTENDED_LOW, true, false, 0 };
static const TextStyle hi_style = { FONT_STAR4000_32, RGBA_YELLOW, true, false, 0 };
static const TextStyle value_style = { FONT_STAR4000_LARGE_32, RGBA_WHITE, true, false, 0 };

void extended_forecast_prepare(const Forecast *f, Units units, ExtendedForecastView *out)
{
	out->count = forecast_extended_days(f, out->days, EXTENDED_DAYS_PER_SCREEN * EXTENDED_SCREENS);
	for (size_t i = 0; i < out->count; i++) {
		ExtendedDay *day = &out->days[i];
		out->icons[i] = nws_large_icon(day->icon_url);
		// The forecast may still be in the old units until it's refetched.
		day->low = units_temperature_convert(day->low, f->units, units);
		day->high = units_temperature_convert(day->high, f->units, units);
	}
}

static void temperature_block(f32 left, f32 top, const TextStyle *label_style, const char *label, bool valid,
	int value)
{
	f32 center = left + TEMP_BLOCK_WIDTH / 2;
	gfx_text(label_style, center, top, ALIGN_CENTER, label);
	if (!valid)
		return;
	char text[8];
	snprintf(text, sizeof text, "%d", value);
	gfx_text(&value_style, center, top + gfx_line_height(FONT_STAR4000_32) + VALUE_GAP, ALIGN_CENTER, text);
}

static void draw_day(const ExtendedDay *day, IconId icon_id, f32 left, u32 time_ms)
{
	f32 center = left + DAY_WIDTH / 2.0f;
	f32 line = gfx_line_height(FONT_STAR4000_32);
	f32 top = DAY_TOP;

	char weekday[4];
	snprintf(weekday, sizeof weekday, "%s", day->weekday);
	for (char *c = weekday; *c; c++)
		*c = (char)toupper((unsigned char)*c);
	gfx_text(&date_style, center, top, ALIGN_CENTER, weekday);
	top += line;

	// img { max-height: 75px } keeps the aspect ratio.
	const IconAsset *icon = &icon_assets[icon_id];
	f32 h = icon->h > ICON_HEIGHT ? ICON_HEIGHT : icon->h;
	f32 w = icon->w * h / icon->h;
	gfx_icon_scaled(icon_id, center - w / 2, top, w, h, time_ms);
	top += ICON_HEIGHT + CONDITION_GAP;

	char lines[3][GFX_LINE_MAX];
	size_t n = gfx_wrap(FONT_STAR4000_32, day->text, DAY_WIDTH, lines, 3);
	for (size_t i = 0; i < n; i++)
		gfx_text(&text_style, center, top + i * line, ALIGN_CENTER, lines[i]);
	top += CONDITION_HEIGHT;

	temperature_block(left, top, &low_style, "Lo", day->has_low, day->low);
	temperature_block(left + TEMP_BLOCK_WIDTH, top, &hi_style, "Hi", day->has_high, day->high);
}

void extended_forecast_draw(const ExtendedForecastView *view, size_t screen, u32 time_ms)
{
	size_t first = screen * EXTENDED_DAYS_PER_SCREEN;
	for (size_t i = 0; i < EXTENDED_DAYS_PER_SCREEN && first + i < view->count; i++)
		draw_day(&view->days[first + i], view->icons[first + i], DAY_LEFT + i * DAY_PITCH, time_ms);
}
