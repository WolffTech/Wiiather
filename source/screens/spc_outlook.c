// Port of ws4kp's SPC Outlook (spc-outlook.mjs, spc-outlook.ejs,
// _spc-outlook.scss) in its standard layout: risk level names stepped up the
// background's columns, then a bar per day reaching the day's risk.
#include <grrlib.h>
#include <time.h>

#include "render/gfx.h"
#include "screens.h"

// .container-categories: 10px side margins, 300px tall with overflow hidden,
// which trims the last bar's bottom border.
#define LEFT 10
#define TOP 90
#define WIDTH (SCREEN_WIDTH - 2 * LEFT)
#define HEIGHT 300
#define LEVEL_LEFT (LEFT + 206)
#define LEVEL_TOP (TOP - 14)
#define LEVEL_STEP 20
#define DAYS_TOP (TOP + 6 * LEVEL_STEP)
#define DAY_HEIGHT 60
#define DAY_NAME_RIGHT (LEFT + 200)
#define DAY_PADDING_TOP 20
#define BAR_LEFT (LEFT + 210)
#define BAR_HEIGHT 40
#define BAR_BORDER 3

// The outset border's light and dark sides, as Chrome shades hsl(0, 0%, 70%),
// and the fill's vertical gradient from hsl(0, 0%, 40%) to 60% and back.
#define RGBA_BORDER_LIGHT 0xFFFFFFFF
#define RGBA_BORDER_DARK 0x5E5E5EFF
#define RGBA_FILL_EDGE 0x666666FF
#define RGBA_FILL_MIDDLE 0x999999FF

static const TextStyle level_style = { FONT_STAR4000_SMALL_32, RGBA_WHITE, true, false, 0 };
static const TextStyle day_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, 0 };

// Most severe first, each a step down and to the left of the one before.
static const char *const levels[] = { "High", "Moderate", "Enhanced", "Slight", "Marginal", "T'Storm" };

// Bar content widths in px.
static const int bar_widths[] = {
	[SPC_TSTM] = 60, [SPC_MRGL] = 150, [SPC_SLGT] = 210, [SPC_ENH] = 270, [SPC_MDT] = 330, [SPC_HIGH] = 390,
};

// The RTC holds local time, so whole days since the epoch are local dates.
static long rtc_day(time_t rtc)
{
	return (long)(rtc / 86400);
}

void spc_outlook_view_prepare(const WeatherSnapshot *snap, SpcOutlookView *out)
{
	out->ready = snap->has_spc_outlook && spc_outlook_has_risk(&snap->spc_outlook);
	if (!out->ready)
		return;
	time_t now = time(NULL);
	out->rtc_day = rtc_day(now);
	for (size_t i = 0; i < SPC_DAYS; i++) {
		time_t day = now + (time_t)i * 86400;
		struct tm tm;
		localtime_r(&day, &tm);
		strftime(out->weekdays[i], sizeof out->weekdays[i], "%A", &tm);
		out->risk[i] = snap->spc_outlook.risk[i];
	}
}

bool spc_outlook_view_outdated(const SpcOutlookView *view)
{
	return view->ready && view->rtc_day != rtc_day(time(NULL));
}

static void quad(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, u32 c0, u32 c1, u32 c2, u32 c3)
{
	guVector v[4] = { { x0, y0, 0 }, { x1, y1, 0 }, { x2, y2, 0 }, { x3, y3, 0 } };
	u32 colors[4] = { c0, c1, c2, c3 };
	GRRLIB_NGoneFilled(v, colors, 4);
}

// A bar with its top-left at (x, y) and `width` px of content.
static void draw_bar(f32 x, f32 y, f32 width)
{
	f32 w = width + 2 * BAR_BORDER, h = BAR_HEIGHT + 2 * BAR_BORDER, b = BAR_BORDER;
	f32 mid = y + b + BAR_HEIGHT / 2;
	f32 left = x + b, right = x + w - b;
	quad(left, y + b, right, y + b, right, mid, left, mid, RGBA_FILL_EDGE, RGBA_FILL_EDGE, RGBA_FILL_MIDDLE,
		RGBA_FILL_MIDDLE);
	quad(left, mid, right, mid, right, y + h - b, left, y + h - b, RGBA_FILL_MIDDLE, RGBA_FILL_MIDDLE,
		RGBA_FILL_EDGE, RGBA_FILL_EDGE);

	u32 l = RGBA_BORDER_LIGHT, d = RGBA_BORDER_DARK;
	quad(x, y, x + w, y, right, y + b, left, y + b, l, l, l, l);
	quad(x, y, left, y + b, left, y + h - b, x, y + h, l, l, l, l);
	quad(x, y + h, left, y + h - b, right, y + h - b, x + w, y + h, d, d, d, d);
	quad(x + w, y, x + w, y + h, right, y + h - b, right, y + b, d, d, d, d);
}

void spc_outlook_view_draw(const SpcOutlookView *view)
{
	gfx_clip(LEFT, TOP, WIDTH, HEIGHT);
	size_t level_count = sizeof levels / sizeof levels[0];
	for (size_t i = 0; i < level_count; i++)
		gfx_text(&level_style, LEVEL_LEFT + (level_count - 1 - i) * LEVEL_STEP, LEVEL_TOP + i * LEVEL_STEP,
			ALIGN_LEFT, levels[i]);
	for (size_t i = 0; i < SPC_DAYS; i++) {
		f32 top = DAYS_TOP + i * DAY_HEIGHT + DAY_PADDING_TOP;
		gfx_text(&day_style, DAY_NAME_RIGHT, top, ALIGN_RIGHT, view->weekdays[i]);
		if (view->risk[i] != SPC_NONE)
			draw_bar(BAR_LEFT, top, bar_widths[view->risk[i]]);
	}
	gfx_clip_reset();
}
