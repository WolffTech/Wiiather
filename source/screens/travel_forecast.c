// Port of ws4kp's Travel Forecast (travelforecast.mjs, travel.ejs, _travel.scss):
// each city's high and low for the day, in its scrolling list.
#include <stdio.h>
#include <string.h>

#include "render/gfx.h"
#include "screens.h"
#include "weather/nws.h"

#define CITY_LEFT 80
#define ICON_LEFT 330
#define ICON_WIDTH 70
#define ICON_MAX_WIDTH 47
#define LOW_LEFT 455
#define LOW_WIDTH 50
#define HIGH_LEFT 510
#define HIGH_WIDTH 60

void travel_forecast_prepare(const TravelForecast *travel, Units units, TravelForecastView *out)
{
	// ws4kp drops cities without data, so the list closes up around them.
	out->count = 0;
	const TravelDay *first = NULL;
	for (size_t i = 0; i < TRAVEL_CITY_COUNT; i++) {
		if (!travel->has_day[i])
			continue;
		const TravelDay *day = &travel->days[i];
		if (!first)
			first = day;
		TravelRow *row = &out->rows[out->count++];
		row->city = travel_cities[i].name;
		row->icon = nws_small_icon(day->icon_url);
		snprintf(row->low, sizeof row->low, "%d", units_temperature_convert(day->low, UNITS_US, units));
		snprintf(row->high, sizeof row->high, "%d", units_temperature_convert(day->high, UNITS_US, units));
	}
	out->content_height = list_content_height(out->count);

	// ws4kp names today, or tomorrow if the first city's forecast starts at
	// night. The day's own date is the same except after midnight, when ws4kp
	// would name the day after.
	snprintf(out->title_bottom, sizeof out->title_bottom, "For %s", first ? forecast_weekday_name(first->weekday) : "");
}

u32 travel_forecast_ms(const TravelForecastView *view)
{
	return scroll_timing_ms(TRAVEL_SCROLL_TIMING, view->content_height, LIST_MAIN_HEIGHT);
}

// text-align: center in a box `width` wide. Text too wide for the box starts
// at its left edge and overflows to the right.
static void centered_text(const TextStyle *style, f32 left, f32 width, f32 top, const char *text)
{
	f32 w = gfx_text_width(style->font, text);
	gfx_text(style, left + (w < width ? (width - w) / 2 : 0), top, ALIGN_LEFT, text);
}

static void draw_row(const TravelRow *row, f32 top, u32 time_ms)
{
	f32 text_top = top + LIST_ROW_TEXT_TOP;
	gfx_text(&list_row_style, CITY_LEFT, text_top, ALIGN_LEFT, row->city);

	// img { max-width: 47px } keeps the aspect ratio. The image sits on the
	// baseline of a line no taller than itself (within a pixel), so its top is
	// the row's; text-align: center rounds to whole pixels.
	const IconAsset *icon = &icon_assets[row->icon];
	int w = icon->w > ICON_MAX_WIDTH ? ICON_MAX_WIDTH : icon->w;
	f32 h = (f32)icon->h * w / icon->w;
	gfx_icon_scaled(row->icon, ICON_LEFT + (ICON_WIDTH - w + 1) / 2, top, w, h, time_ms);

	centered_text(&list_row_style, LOW_LEFT, LOW_WIDTH, text_top, row->low);
	centered_text(&list_row_style, HIGH_LEFT, HIGH_WIDTH, text_top, row->high);
}

void travel_forecast_draw(const TravelForecastView *view, u32 elapsed_ms, u32 time_ms)
{
	int offset = scroll_timing_offset(TRAVEL_SCROLL_TIMING, view->content_height, LIST_MAIN_HEIGHT, elapsed_ms);
	int rows_top = list_begin(view->content_height, offset);
	for (size_t i = 0; i < view->count; i++) {
		int top = rows_top + (int)i * LIST_ROW_HEIGHT;
		if (list_row_visible(top))
			draw_row(&view->rows[i], top, time_ms);
	}

	list_column_bar_draw();
	centered_text(&list_column_style, LOW_LEFT, LOW_WIDTH, LIST_COLUMN_LABEL_TOP, "LOW");
	centered_text(&list_column_style, HIGH_LEFT, HIGH_WIDTH, LIST_COLUMN_LABEL_TOP, "HIGH");
	gfx_clip_reset();
}
