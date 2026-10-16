// Port of ws4kp's Hourly Forecast (hourly.mjs, hourly.ejs, _hourly.scss).
#include <time.h>

#include "render/gfx.h"
#include "screens.h"

#define ICON_LEFT 255
#define ICON_WIDTH 70
#define WIND_RIGHT (505 + 100)

#define RGBA_HEAT_INDEX 0xEE0000FF
#define RGBA_WIND_CHILL 0x8080FFFF

static const IconId icons[HOURLY_ICON_COUNT] = {
	[HOURLY_ICON_FREEZING_RAIN] = ICON_SMALL_FREEZING_RAIN_1992,
	[HOURLY_ICON_BLOWING_SNOW] = ICON_SMALL_BLOWING_SNOW,
	[HOURLY_ICON_HEAVY_SNOW] = ICON_SMALL_HEAVY_SNOW_1994,
	[HOURLY_ICON_THUNDERSNOW] = ICON_SMALL_THUNDERSNOW,
	[HOURLY_ICON_LIGHT_SNOW] = ICON_SMALL_LIGHT_SNOW,
	[HOURLY_ICON_THUNDERSTORM] = ICON_SMALL_THUNDERSTORM,
	[HOURLY_ICON_RAIN] = ICON_SMALL_RAIN_1992,
	[HOURLY_ICON_SCATTERED_SHOWERS] = ICON_SMALL_SCATTERED_SHOWERS_1994,
	[HOURLY_ICON_SCATTERED_SHOWERS_NIGHT] = ICON_SMALL_SCATTERED_SHOWERS_NIGHT_1994,
	[HOURLY_ICON_FOG] = ICON_SMALL_FOG,
	[HOURLY_ICON_CLOUDY] = ICON_SMALL_CLOUDY,
	[HOURLY_ICON_MOSTLY_CLOUDY] = ICON_SMALL_MOSTLY_CLOUDY_1994,
	[HOURLY_ICON_PARTLY_CLEAR] = ICON_SMALL_PARTLY_CLEAR_1994,
	[HOURLY_ICON_PARTLY_CLOUDY] = ICON_SMALL_PARTLY_CLOUDY,
	[HOURLY_ICON_PARTLY_CLOUDY_NIGHT] = ICON_SMALL_PARTLY_CLOUDY_NIGHT,
	[HOURLY_ICON_CLEAR] = ICON_SMALL_CLEAR_1992,
	[HOURLY_ICON_SUNNY] = ICON_SMALL_SUNNY,
};

static long rtc_hour(void)
{
	return (long)(time(NULL) / 3600);
}

void hourly_view_prepare(const WeatherSnapshot *snap, HourlyView *out)
{
	out->count = 0;
	out->rtc_hour = rtc_hour();
	// Rows are labeled in local time, which needs the RTC's offset from UTC.
	if (snap->has_hourly && snap->has_utc_offset)
		out->count = hourly_rows(&snap->hourly, snap->location.lat, snap->location.lon,
			time(NULL) - snap->utc_offset, snap->utc_offset, snap->units, out->rows);
	out->content_height = list_content_height(out->count);
}

bool hourly_view_outdated(const HourlyView *view)
{
	return view->rtc_hour != rtc_hour();
}

u32 hourly_view_ms(const HourlyView *view)
{
	return scroll_timing_ms(SCROLL_TIMING_DEFAULT, view->content_height, LIST_MAIN_HEIGHT);
}

static void draw_row(const HourlyRow *row, f32 top, u32 time_ms)
{
	f32 text_top = top + LIST_ROW_TEXT_TOP;
	gfx_text(&list_row_style, 25, text_top, ALIGN_LEFT, row->hour);

	// The image sits on the baseline of a line no taller than itself, so its
	// top is the row's; text-align: center rounds to whole pixels.
	IconId icon = icons[row->icon];
	gfx_icon(icon, ICON_LEFT + (ICON_WIDTH - icon_assets[icon].w + 1) / 2, top, time_ms);

	gfx_text(&list_row_style, 355, text_top, ALIGN_LEFT, row->temp);
	TextStyle like_style = list_row_style;
	if (row->like_color == HOURLY_LIKE_WIND_CHILL)
		like_style.color = RGBA_WIND_CHILL;
	else if (row->like_color == HOURLY_LIKE_HEAT_INDEX)
		like_style.color = RGBA_HEAT_INDEX;
	gfx_text(&like_style, 425, text_top, ALIGN_LEFT, row->like);
	gfx_text(&list_row_style, WIND_RIGHT, text_top, ALIGN_RIGHT, row->wind);
}

void hourly_view_draw(const HourlyView *view, u32 elapsed_ms, u32 time_ms)
{
	int offset = scroll_timing_offset(SCROLL_TIMING_DEFAULT, view->content_height, LIST_MAIN_HEIGHT, elapsed_ms);
	int rows_top = list_begin(view->content_height, offset);
	for (size_t i = 0; i < view->count; i++) {
		int top = rows_top + (int)i * LIST_ROW_HEIGHT;
		if (list_row_visible(top))
			draw_row(&view->rows[i], top, time_ms);
	}

	// The sticky column headers stay on top of the scrolling rows.
	list_column_bar_draw();
	gfx_text(&list_column_style, 355, LIST_COLUMN_LABEL_TOP, ALIGN_LEFT, "TEMP");
	gfx_text(&list_column_style, 435, LIST_COLUMN_LABEL_TOP, ALIGN_LEFT, "LIKE");
	gfx_text(&list_column_style, 535, LIST_COLUMN_LABEL_TOP, ALIGN_LEFT, "WIND");
	gfx_clip_reset();
}
