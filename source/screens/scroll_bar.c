// Port of ws4kp's bottom bar timing (currentweatherscroll.mjs). Layout is
// #container>.scroll in _weather-display.scss: a 77px bar at y 403 whose
// 26px header row starts 10px above it and is clipped by the bar.
#include <grrlib.h>
#include <string.h>

#include "displays.h"

#define LINE_MS 4000
#define RGBA_HAZARD_RED 0x702323FF // rgb(112, 35, 35)
#define BAR_TOP 403
#define TEXT_LEFT 55
#define TEXT_WIDTH (SCREEN_WIDTH - 2 * 55)
#define HEADER_TOP 393
#define HEADER_HEIGHT 26
#define CRAWL_TOP (HEADER_TOP + HEADER_HEIGHT)
#define CRAWL_HEIGHT 37 // .fixed's line box
#define CRAWL_MAX_CHARS 64 // more than fit in the bar

static const TextStyle header_style = { FONT_STAR4000_SMALL_26, RGBA_WHITE, true, false, 0 };
static const TextStyle crawl_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, 0 };

// Item 0 is the hazard crawl; 0 ms means the item has nothing to show.
static u32 item_ms(size_t item, const DisplayData *d)
{
	if (item == 0)
		return d->hazards.crawl[0] ? alert_scroll_ms(d->hazards.crawl_width, TEXT_WIDTH) : 0;
	return item <= (size_t)d->current.scroll_count ? LINE_MS : 0;
}

static void update(ScrollBar *bar, const DisplayData *d, bool visible, u32 now_ms)
{
	if (!visible) {
		if (bar->shown) {
			bar->shown = false;
			bar->paused_elapsed_ms = now_ms - bar->started_ms;
			// ws4kp abandons a crawl that was cut off rather than replay it.
			if (item_ms(bar->item, d) > LINE_MS) {
				bar->item++;
				bar->paused_elapsed_ms = 0;
			}
		}
		return;
	}
	if (!bar->shown) {
		bar->shown = true;
		bar->started_ms = now_ms - bar->paused_elapsed_ms;
	}
	u32 ms = item_ms(bar->item, d);
	if (ms && now_ms - bar->started_ms < ms)
		return;
	// Next item with something to show, like ws4kp's forced increments.
	size_t items = 1 + d->current.scroll_count;
	bar->started_ms = now_ms;
	for (size_t i = 0; i < items; i++) {
		bar->item = (bar->item + 1) % items;
		if (item_ms(bar->item, d))
			break;
	}
}

// Draws only the part of the crawl near the bar to keep the vertex count low.
static void draw_crawl(const char *text, f32 x)
{
	const FontAsset *f = &font_assets[crawl_style.font];
	const f32 margin = 32; // room for glyph overhang and the shadow
	const u8 *p = (const u8 *)text;
	while (*p && x + f->glyphs[*p].advance < TEXT_LEFT - margin)
		x += f->glyphs[*p++].advance;
	char visible[CRAWL_MAX_CHARS + 1];
	size_t n = 0;
	for (f32 end = x; p[n] && n < CRAWL_MAX_CHARS && end < TEXT_LEFT + TEXT_WIDTH + margin; n++)
		end += f->glyphs[p[n]].advance;
	memcpy(visible, p, n);
	visible[n] = '\0';
	gfx_text(&crawl_style, x, CRAWL_TOP, ALIGN_LEFT, visible);
}

static void draw_hazard(const HazardsView *h, u32 elapsed_ms)
{
	GRRLIB_Rectangle(0, BAR_TOP, SCREEN_WIDTH, SCREEN_HEIGHT - BAR_TOP, RGBA_HAZARD_RED, true);
	gfx_clip(TEXT_LEFT, BAR_TOP, TEXT_WIDTH, CRAWL_TOP - BAR_TOP);
	gfx_text(&header_style, TEXT_LEFT, HEADER_TOP, ALIGN_LEFT, h->crawl_event);
	gfx_clip(TEXT_LEFT, CRAWL_TOP, TEXT_WIDTH, CRAWL_HEIGHT);
	draw_crawl(h->crawl, TEXT_LEFT - alert_scroll_offset(h->crawl_width, TEXT_WIDTH, elapsed_ms));
	gfx_clip_reset();
}

void scroll_bar_draw(ScrollBar *bar, const DisplayData *d, bool visible, u32 now_ms)
{
	update(bar, d, visible, now_ms);
	if (!bar->shown)
		return;
	if (bar->item == 0)
		draw_hazard(&d->hazards, now_ms - bar->started_ms);
	else
		scroll_draw(d->current.scroll[bar->item - 1]);
}
