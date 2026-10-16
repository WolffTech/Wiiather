#pragma once
#include <gctypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "assets_gen.h"

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define RGBA_WHITE 0xFFFFFFFF
#define RGBA_YELLOW 0xFFFF00FF
#define RGBA_BLACK 0x000000FF
#define RGBA_EXTENDED_LOW 0x8080FFFF

#define GFX_LINE_MAX 128 // bytes per wrapped line, including the NUL

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

// Finishes the frame. Use instead of GRRLIB_Render().
void gfx_present(void);

f32 gfx_text_width(FontId font, const char *text);

// Height of a CSS line-height: normal line box.
f32 gfx_line_height(FontId font);

// Wraps text like CSS white-space: normal at spaces and after hyphens.
// Words wider than `width` overflow on their own line. Returns the line count.
size_t gfx_wrap(FontId font, const char *text, f32 width, char (*lines)[GFX_LINE_MAX], size_t max_lines);

// Draws one line of Latin-1 text. `top` is the top of the CSS line box, so
// coordinates can be taken straight from ws4kp's layout. `x` is the left
// edge, center, or right edge depending on `align`.
void gfx_text(const TextStyle *style, f32 x, f32 top, TextAlign align, const char *text);

void gfx_image(ImageId id, f32 x, f32 y);

// Limits drawing to a rectangle in layout coordinates until gfx_clip_reset().
void gfx_clip(f32 x, f32 y, f32 w, f32 h);
void gfx_clip_reset(void);

// Draws the animation frame for `time_ms`, with (x, y) as the top-left corner.
void gfx_icon(IconId id, f32 x, f32 y, u32 time_ms);

// Same, resized to w x h with bilinear filtering.
void gfx_icon_scaled(IconId id, f32 x, f32 y, f32 w, f32 h, u32 time_ms);

// Draws the map scaled by `scale`, with map pixel (src_x, src_y) at (x, y),
// filling w x h with bilinear filtering. Tiles are decoded on first use, so
// the first frame of a new view can take a moment.
void gfx_map(MapId map, f32 src_x, f32 src_y, f32 scale, f32 x, f32 y, f32 w, f32 h);
