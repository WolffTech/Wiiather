// Port of ws4kp's Hourly Forecast (hourly.mjs, hourly.ejs, _hourly.scss). The
// 310px main area holds a 20px column header bar above 72px rows that scroll
// up under it, on a gradient that repeats every 338px from the list's bottom.
#include <grrlib.h>
#include <time.h>

#include "render/gfx.h"
#include "screens.h"

#define MAIN_TOP 90
#define MAIN_HEIGHT 310
#define COLUMN_HEADER_HEIGHT 20
#define COLUMN_LABEL_TOP (MAIN_TOP - 14)
#define LINES_TOP (MAIN_TOP + COLUMN_HEADER_HEIGHT)
#define LINES_PADDING 10
#define LINES_MIN_HEIGHT 338
#define ROW_HEIGHT 72
#define ROW_TEXT_TOP 8
#define ICON_LEFT 255
#define ICON_WIDTH 70
#define WIND_RIGHT (505 + 100)

#define RGBA_COLUMN_HEADER 0x200057FF // rgb(32, 0, 87)
#define RGBA_GRADIENT_1 0x102080FF
#define RGBA_GRADIENT_2 0x001040FF
#define RGBA_HEAT_INDEX 0xEE0000FF
#define RGBA_WIND_CHILL 0x8080FFFF

// The gradient's stops, from the bottom of each 338px period.
#define GRADIENT_PERIOD 338
#define GRADIENT_SOLID_START 136
#define GRADIENT_SOLID_END 202

static const TextStyle column_style = { FONT_STAR4000_SMALL_32, RGBA_YELLOW, true, false, 0 };
static const TextStyle row_style = { FONT_STAR4000_LARGE_32, RGBA_YELLOW, true, false, 0 };

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
			time(NULL) - snap->utc_offset, snap->utc_offset, out->rows);
	int rows_height = (int)out->count * ROW_HEIGHT;
	out->content_height = LINES_PADDING + (rows_height > LINES_MIN_HEIGHT ? rows_height : LINES_MIN_HEIGHT);
}

bool hourly_view_outdated(const HourlyView *view)
{
	return view->rtc_hour != rtc_hour();
}

u32 hourly_view_ms(const HourlyView *view)
{
	return scroll_timing_ms(SCROLL_TIMING_DEFAULT, view->content_height, MAIN_HEIGHT);
}

static void band(f32 top, f32 bottom, u32 top_color, u32 bottom_color)
{
	guVector v[4] = { { 0, top, 0 }, { SCREEN_WIDTH, top, 0 }, { SCREEN_WIDTH, bottom, 0 }, { 0, bottom, 0 } };
	u32 colors[4] = { top_color, top_color, bottom_color, bottom_color };
	GRRLIB_NGoneFilled(v, colors, 4);
}

// repeating-linear-gradient(0deg, ...) starts each period at the list's bottom.
static void draw_gradient(int lines_bottom)
{
	for (int bottom = lines_bottom; bottom > MAIN_TOP; bottom -= GRADIENT_PERIOD) {
		int top = bottom - GRADIENT_PERIOD;
		if (top >= MAIN_TOP + MAIN_HEIGHT)
			continue;
		int solid_top = bottom - GRADIENT_SOLID_END, solid_bottom = bottom - GRADIENT_SOLID_START;
		band(top, solid_top, RGBA_GRADIENT_2, RGBA_GRADIENT_1);
		band(solid_top, solid_bottom, RGBA_GRADIENT_1, RGBA_GRADIENT_1);
		band(solid_bottom, bottom, RGBA_GRADIENT_1, RGBA_GRADIENT_2);
	}
}

static void draw_row(const HourlyRow *row, f32 top, u32 time_ms)
{
	f32 text_top = top + ROW_TEXT_TOP;
	gfx_text(&row_style, 25, text_top, ALIGN_LEFT, row->hour);

	// The image sits on the baseline of a line no taller than itself, so its
	// top is the row's; text-align: center rounds to whole pixels.
	IconId icon = icons[row->icon];
	gfx_icon(icon, ICON_LEFT + (ICON_WIDTH - icon_assets[icon].w + 1) / 2, top, time_ms);

	gfx_text(&row_style, 355, text_top, ALIGN_LEFT, row->temp);
	TextStyle like_style = row_style;
	if (row->like_color == HOURLY_LIKE_WIND_CHILL)
		like_style.color = RGBA_WIND_CHILL;
	else if (row->like_color == HOURLY_LIKE_HEAT_INDEX)
		like_style.color = RGBA_HEAT_INDEX;
	gfx_text(&like_style, 425, text_top, ALIGN_LEFT, row->like);
	gfx_text(&row_style, WIND_RIGHT, text_top, ALIGN_RIGHT, row->wind);
}

void hourly_view_draw(const HourlyView *view, u32 elapsed_ms, u32 time_ms)
{
	int offset = scroll_timing_offset(SCROLL_TIMING_DEFAULT, view->content_height, MAIN_HEIGHT, elapsed_ms);
	int lines_top = LINES_TOP - offset;

	gfx_clip(0, MAIN_TOP, SCREEN_WIDTH, MAIN_HEIGHT);
	draw_gradient(lines_top + view->content_height);
	for (size_t i = 0; i < view->count; i++) {
		int top = lines_top + LINES_PADDING + (int)i * ROW_HEIGHT;
		if (top + ROW_HEIGHT > MAIN_TOP && top < MAIN_TOP + MAIN_HEIGHT)
			draw_row(&view->rows[i], top, time_ms);
	}

	// The sticky column headers stay on top of the scrolling rows.
	GRRLIB_Rectangle(0, MAIN_TOP, SCREEN_WIDTH, COLUMN_HEADER_HEIGHT, RGBA_COLUMN_HEADER, true);
	gfx_text(&column_style, 355, COLUMN_LABEL_TOP, ALIGN_LEFT, "TEMP");
	gfx_text(&column_style, 435, COLUMN_LABEL_TOP, ALIGN_LEFT, "LIKE");
	gfx_text(&column_style, 535, COLUMN_LABEL_TOP, ALIGN_LEFT, "WIND");
	gfx_clip_reset();
}
