// The scrolling list Hourly and Travel Forecast share (_hourly.scss,
// _travel.scss). The 310px main area holds a 20px column header bar above 72px
// rows that scroll up under it, on a gradient that repeats every 338px from
// the list's bottom.
#include <grrlib.h>

#include "screens.h"

#define MAIN_TOP 90
#define COLUMN_HEADER_HEIGHT 20
#define LINES_TOP (MAIN_TOP + COLUMN_HEADER_HEIGHT)
#define LINES_PADDING 10
#define LINES_MIN_HEIGHT 338

#define RGBA_COLUMN_HEADER 0x200057FF // rgb(32, 0, 87)
#define RGBA_GRADIENT_1 0x102080FF
#define RGBA_GRADIENT_2 0x001040FF

// The gradient's stops, from the bottom of each 338px period.
#define GRADIENT_PERIOD 338
#define GRADIENT_SOLID_START 136
#define GRADIENT_SOLID_END 202

const TextStyle list_column_style = { FONT_STAR4000_SMALL_32, RGBA_YELLOW, true, false, 0 };
const TextStyle list_row_style = { FONT_STAR4000_LARGE_32, RGBA_YELLOW, true, false, 0 };

int list_content_height(size_t rows)
{
	int rows_height = (int)rows * LIST_ROW_HEIGHT;
	return LINES_PADDING + (rows_height > LINES_MIN_HEIGHT ? rows_height : LINES_MIN_HEIGHT);
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
		if (top >= MAIN_TOP + LIST_MAIN_HEIGHT)
			continue;
		int solid_top = bottom - GRADIENT_SOLID_END, solid_bottom = bottom - GRADIENT_SOLID_START;
		band(top, solid_top, RGBA_GRADIENT_2, RGBA_GRADIENT_1);
		band(solid_top, solid_bottom, RGBA_GRADIENT_1, RGBA_GRADIENT_1);
		band(solid_bottom, bottom, RGBA_GRADIENT_1, RGBA_GRADIENT_2);
	}
}

int list_begin(int content_height, int offset)
{
	int lines_top = LINES_TOP - offset;
	gfx_clip(0, MAIN_TOP, SCREEN_WIDTH, LIST_MAIN_HEIGHT);
	draw_gradient(lines_top + content_height);
	return lines_top + LINES_PADDING;
}

bool list_row_visible(int top)
{
	return top + LIST_ROW_HEIGHT > MAIN_TOP && top < MAIN_TOP + LIST_MAIN_HEIGHT;
}

void list_column_bar_draw(void)
{
	GRRLIB_Rectangle(0, MAIN_TOP, SCREEN_WIDTH, COLUMN_HEADER_HEIGHT, RGBA_COLUMN_HEADER, true);
}
