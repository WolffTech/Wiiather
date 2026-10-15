// Port of ws4kp's Hazards (hazards.mjs, _hazards.scss). The display has no
// header: a red 640x480 main area whose text scrolls up from the top.
#include <grrlib.h>
#include <stdio.h>
#include <string.h>

#include "screens.h"

#define RGBA_HAZARD_RED 0x702323FF // rgb(112, 35, 35)
#define VIEW_HEIGHT 480
#define TEXT_LEFT 80
#define TEXT_WIDTH (SCREEN_WIDTH - 2 * 80)
#define LIST_PADDING_TOP 10
#define HAZARD_MARGIN_TOP 10
#define HAZARD_PADDING_BOTTOM 10

static const TextStyle text_style = { FONT_STAR4000_32, RGBA_WHITE, false, false, 0 };
static const TextStyle outline_style = { FONT_STAR4000_32, RGBA_BLACK, false, false, 0 };

// text-transform: uppercase for Latin-1.
static void upcase(char *s)
{
	for (u8 *p = (u8 *)s; *p; p++)
		if ((*p >= 'a' && *p <= 'z') || (*p >= 0xE0 && *p <= 0xFE && *p != 0xF7))
			*p -= 0x20;
}

// Appends one alert's lines; returns the height it adds to the content.
static int add_alert(const Alert *a, HazardsView *view, int top)
{
	static char text[ALERT_EVENT_MAX + ALERT_DESCRIPTION_MAX * 2];
	alert_hazard_text(a, text, sizeof text);
	upcase(text);

	const int line_height = gfx_line_height(FONT_STAR4000_32);
	int y = top;
	// Each '\n' is a <br/>; a trailing one doesn't start another line.
	for (char *seg = text; *seg;) {
		char *end = strchr(seg, '\n');
		if (end)
			*end = '\0';
		size_t room = HAZARDS_MAX_LINES - view->line_count;
		if (room == 0)
			break; // truncated
		size_t n = gfx_wrap(FONT_STAR4000_32, seg, TEXT_WIDTH, &view->lines[view->line_count], room);
		for (size_t i = 0; i < n; i++)
			view->line_top[view->line_count++] = y + i * line_height;
		y += (n ? n : 1) * line_height;
		if (!end)
			break;
		seg = end + 1;
	}
	return y - top;
}

void hazards_prepare(const WeatherSnapshot *snap, HazardsView *view)
{
	const Alerts *alerts = &snap->alerts;
	alerts_viewed_update(&view->viewed, snap->alert_fetches);

	view->shown_count = 0;
	view->line_count = 0;
	int y = LIST_PADDING_TOP;
	for (size_t i = 0; i < alerts->count; i++) {
		const Alert *a = &alerts->alerts[i];
		if (alerts_viewed_has(&view->viewed, a->id))
			continue;
		snprintf(view->shown_ids[view->shown_count++], ALERT_ID_MAX, "%s", a->id);
		y += HAZARD_MARGIN_TOP;
		y += add_alert(a, view, y);
		y += HAZARD_PADDING_BOTTOM;
	}
	view->content_height = y;

	// The bottom bar always crawls the most severe alert, viewed or not.
	view->crawl_event[0] = view->crawl[0] = '\0';
	view->crawl_width = 0;
	if (alerts->count > 0) {
		snprintf(view->crawl_event, sizeof view->crawl_event, "%s", alerts->alerts[0].event);
		alert_scroll_text(&alerts->alerts[0], view->crawl, sizeof view->crawl);
		view->crawl_width = gfx_text_width(FONT_STAR4000_32, view->crawl);
	}
}

u32 hazards_ms(const HazardsView *view)
{
	return scroll_timing_ms(HAZARDS_SCROLL_TIMING, view->content_height, VIEW_HEIGHT);
}

void hazards_mark_viewed(HazardsView *view)
{
	for (size_t i = 0; i < view->shown_count; i++)
		alerts_viewed_add(&view->viewed, view->shown_ids[i]);
	view->shown_count = 0;
	view->line_count = 0;
	view->content_height = 0;
}

void hazards_draw(const HazardsView *view, u32 elapsed_ms)
{
	GRRLIB_Rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBA_HAZARD_RED, true);
	int offset = scroll_timing_offset(HAZARDS_SCROLL_TIMING, view->content_height, VIEW_HEIGHT, elapsed_ms);
	const int line_height = gfx_line_height(FONT_STAR4000_32);
	// text-shadow with a zero offset: just the 1.5px outline.
	static const f32 outline[][2] = {
		{ 0, 0 }, { -1.5f, -1.5f }, { 0, -1.5f }, { 1.5f, -1.5f }, { 1.5f, 0 },
		{ 1.5f, 1.5f }, { 0, 1.5f }, { -1.5f, 1.5f }, { -1.5f, 0 },
	};
	for (size_t i = 0; i < view->line_count; i++) {
		int top = view->line_top[i] - offset;
		if (top + line_height < 0 || top >= VIEW_HEIGHT)
			continue;
		for (size_t j = 0; j < sizeof outline / sizeof outline[0]; j++)
			gfx_text(&outline_style, TEXT_LEFT + outline[j][0], top + outline[j][1], ALIGN_LEFT, view->lines[i]);
		gfx_text(&text_style, TEXT_LEFT, top, ALIGN_LEFT, view->lines[i]);
	}
}
