// Port of ws4kp's Local Forecast (localforecast.mjs, _local-forecast.scss).
// Text fills a 492x280 box 15px into the main area, 40px per line.
#include <string.h>

#include "screens.h"

#define TEXT_LEFT (64 + 10)
#define TEXT_TOP (90 + 15)
#define TEXT_WIDTH 492
#define LINE_HEIGHT 40
#define BASE_PAGE_MS 5000

static const TextStyle text_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, LINE_HEIGHT };

void local_forecast_prepare(const Forecast *f, LocalForecastPages *out)
{
	memset(out, 0, sizeof *out);
	size_t periods = f->count < LOCAL_FORECAST_PERIODS ? f->count : LOCAL_FORECAST_PERIODS;
	for (size_t i = 0; i < periods; i++) {
		char text[640];
		forecast_local_text(&f->periods[i], text, sizeof text);
		static char lines[LOCAL_FORECAST_MAX_PAGES * LOCAL_FORECAST_PAGE_LINES][GFX_LINE_MAX];
		size_t max_lines = (LOCAL_FORECAST_MAX_PAGES - out->page_count) * LOCAL_FORECAST_PAGE_LINES;
		size_t n = gfx_wrap(FONT_STAR4000_32, text, TEXT_WIDTH, lines, max_lines);
		// Each period starts on a fresh page.
		for (size_t line = 0; line < n; line += LOCAL_FORECAST_PAGE_LINES) {
			size_t page = out->page_count++;
			size_t count = n - line < LOCAL_FORECAST_PAGE_LINES ? n - line : LOCAL_FORECAST_PAGE_LINES;
			memcpy(out->lines[page], lines[line], count * GFX_LINE_MAX);
			out->line_count[page] = count;
		}
	}
}

// ws4kp scales each page's time by how much text it holds.
u32 local_forecast_page_ms(const LocalForecastPages *pages, size_t page)
{
	size_t lines = pages->line_count[page];
	if (lines <= 1)
		return BASE_PAGE_MS * 6 / 10;
	if (lines == 2)
		return BASE_PAGE_MS * 8 / 10;
	if (lines >= 6)
		return BASE_PAGE_MS * 14 / 10;
	return BASE_PAGE_MS;
}

void local_forecast_draw(const LocalForecastPages *pages, size_t page)
{
	for (size_t i = 0; i < pages->line_count[page]; i++)
		gfx_text(&text_style, TEXT_LEFT, TEXT_TOP + i * LINE_HEIGHT, ALIGN_LEFT, pages->lines[page][i]);
}
