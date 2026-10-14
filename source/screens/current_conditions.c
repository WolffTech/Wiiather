// Port of ws4kp's Current Conditions display. Positions come from
// _current-weather.scss: the main area starts at y=90 with a 64px side
// margin; the two 255px columns start 20px lower.
#include "render/gfx.h"
#include "screens.h"

#define MAIN_LEFT 64
#define MAIN_RIGHT 576
#define COL_WIDTH 255
#define COL_TOP 110
#define RIGHT_COL_LEFT (MAIN_RIGHT - COL_WIDTH)
#define LEFT_LINE 37 // "normal" line height for the 32px Star4000 fonts
#define ROW_PITCH 36 // 24px line plus 12px margin

static const TextStyle temp_style = { FONT_STAR4000_LARGE_32, RGBA_WHITE, true, false, 0 };
static const TextStyle left_style = { FONT_STAR4000_EXTENDED_32, RGBA_WHITE, true, false, 0 };
static const TextStyle location_style = { FONT_STAR4000_LARGE_20, RGBA_YELLOW, true, true, 24 };
static const TextStyle row_style = { FONT_STAR4000_LARGE_20, RGBA_WHITE, true, true, 24 };
static const TextStyle status_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, 0 };

static void row(int index, const char *label, const char *value)
{
	f32 top = 148 + index * ROW_PITCH;
	gfx_text(&row_style, RIGHT_COL_LEFT + 20, top, ALIGN_LEFT, label);
	gfx_text(&row_style, MAIN_RIGHT - 10, top, ALIGN_RIGHT, value);
}

void current_conditions_draw(const CurrentConditionsText *cc, u32 time_ms)
{
	f32 center = MAIN_LEFT + COL_WIDTH / 2.0f;
	f32 y = COL_TOP;
	gfx_text(&temp_style, center, y, ALIGN_CENTER, cc->temperature);
	y += LEFT_LINE;
	gfx_text(&left_style, center, y, ALIGN_CENTER, cc->condition);
	y += LEFT_LINE;

	const IconAsset *icon = &icon_assets[cc->icon];
	gfx_icon(cc->icon, MAIN_LEFT + (COL_WIDTH - icon->w) / 2, y, time_ms);
	y += icon->h;

	// The wind value floats right in a row with a 10px left margin.
	gfx_text(&left_style, MAIN_LEFT + 10, y, ALIGN_LEFT, "Wind:");
	gfx_text(&left_style, MAIN_LEFT + COL_WIDTH, y, ALIGN_RIGHT, cc->wind);
	y += LEFT_LINE;
	if (cc->gusts[0])
		gfx_text(&left_style, MAIN_LEFT + COL_WIDTH, y, ALIGN_RIGHT, cc->gusts);

	gfx_text(&location_style, RIGHT_COL_LEFT, COL_TOP + 4, ALIGN_LEFT, cc->location);
	int r = 0;
	row(r++, "Humidity:", cc->humidity);
	row(r++, "Dewpoint:", cc->dewpoint);
	row(r++, "Ceiling:", cc->ceiling);
	row(r++, "Visibility:", cc->visibility);
	if (cc->pressure[0])
		row(r++, "Pressure:", cc->pressure);
	if (cc->extra_label[0])
		row(r++, cc->extra_label, cc->extra_value);
}

void status_draw(const WeatherSnapshot *snap)
{
	const char *headline = snap->status == WEATHER_FAILED ? "Unable to load weather" : "Loading...";
	gfx_text(&status_style, 320, 200, ALIGN_CENTER, headline);
	gfx_text(&row_style, 320, 250, ALIGN_CENTER, snap->message);
}
