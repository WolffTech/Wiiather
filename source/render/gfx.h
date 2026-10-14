#pragma once
#include <gctypes.h>
#include <stdbool.h>

#include "assets_gen.h"

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define RGBA_WHITE 0xFFFFFFFF
#define RGBA_YELLOW 0xFFFF00FF
#define RGBA_BLACK 0x000000FF

typedef enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT } TextAlign;

typedef struct {
	FontId font;
	u32 color;
	bool shadow; // ws4kp's text-shadow: 3px drop shadow plus 1.5px outline
	bool bold;   // browser-style synthetic bold
	f32 line_height; // CSS line-height in px; 0 means "normal"
} TextStyle;

// Call after GRRLIB_Init().
void gfx_init(void);

f32 gfx_text_width(FontId font, const char *text);

// Draws one line of Latin-1 text. `top` is the top of the CSS line box, so
// coordinates can be taken straight from ws4kp's layout. `x` is the left
// edge, center, or right edge depending on `align`.
void gfx_text(const TextStyle *style, f32 x, f32 top, TextAlign align, const char *text);

void gfx_image(ImageId id, f32 x, f32 y);

// Draws the animation frame for `time_ms`, with (x, y) as the top-left corner.
void gfx_icon(IconId id, f32 x, f32 y, u32 time_ms);
