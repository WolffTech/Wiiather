#include "gfx.h"

#include <grrlib.h>
#include <math.h>

static GRRLIB_texImg *font_tex[FONT_COUNT];
static GRRLIB_texImg *icon_tex[ICON_COUNT];
static GRRLIB_texImg *image_tex[IMG_COUNT];

void gfx_init(void)
{
	// Layout is authored at 640x480 like ws4kp. PAL 50Hz modes have a taller
	// framebuffer, so map the layout onto whatever size the video mode uses.
	Mtx44 projection;
	guOrtho(projection, 0, SCREEN_HEIGHT, 0, SCREEN_WIDTH, 0, 1000.0f);
	GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);

	// Pixel fonts and GIF art are drawn 1:1, so nearest sampling keeps them crisp.
	GRRLIB_SetAntiAliasing(false);
	for (int i = 0; i < FONT_COUNT; i++)
		font_tex[i] = GRRLIB_LoadTexturePNG(font_assets[i].png);
	for (int i = 0; i < ICON_COUNT; i++)
		icon_tex[i] = GRRLIB_LoadTexturePNG(icon_assets[i].png);
	for (int i = 0; i < IMG_COUNT; i++)
		image_tex[i] = GRRLIB_LoadTexturePNG(image_assets[i].png);
}

f32 gfx_text_width(FontId font, const char *text)
{
	const FontAsset *f = &font_assets[font];
	int width = 0;
	for (const u8 *p = (const u8 *)text; *p; p++)
		width += f->glyphs[*p].advance;
	return width;
}

static void draw_run(FontId font, f32 x, f32 baseline, u32 color, const char *text)
{
	const FontAsset *f = &font_assets[font];
	f32 pen = x;
	for (const u8 *p = (const u8 *)text; *p; p++) {
		const GlyphInfo *g = &f->glyphs[*p];
		if (g->w && g->h) {
			GRRLIB_DrawPart(floorf(pen + g->xoff + 0.5f), floorf(baseline + g->yoff + 0.5f), g->x, g->y,
				g->w, g->h, font_tex[font], 0, 1, 1, color);
		}
		pen += g->advance;
	}
}

// Chrome fakes bold by thickening outlines without changing advances;
// overdrawing one pixel to the right approximates that.
static void draw_weighted(const TextStyle *style, f32 x, f32 baseline, u32 color, const char *text)
{
	draw_run(style->font, x, baseline, color, text);
	if (style->bold)
		draw_run(style->font, x + 1, baseline, color, text);
}

void gfx_text(const TextStyle *style, f32 x, f32 top, TextAlign align, const char *text)
{
	const FontAsset *f = &font_assets[style->font];
	f32 content = f->ascent + f->descent;
	f32 line_height = style->line_height > 0 ? style->line_height : content;
	f32 baseline = top + floorf((line_height - content) / 2) + f->ascent;

	f32 width = gfx_text_width(style->font, text);
	if (align == ALIGN_CENTER)
		x -= width / 2;
	else if (align == ALIGN_RIGHT)
		x -= width;

	if (style->shadow) {
		static const f32 outline[][2] = {
			{ 3, 3 }, { -1.5f, -1.5f }, { 0, -1.5f }, { 1.5f, -1.5f }, { 1.5f, 0 },
			{ 1.5f, 1.5f }, { 0, 1.5f }, { -1.5f, 1.5f }, { -1.5f, 0 },
		};
		for (size_t i = 0; i < sizeof outline / sizeof outline[0]; i++)
			draw_weighted(style, x + outline[i][0], baseline + outline[i][1], RGBA_BLACK, text);
	}
	draw_weighted(style, x, baseline, style->color, text);
}

void gfx_image(ImageId id, f32 x, f32 y)
{
	GRRLIB_DrawImg(x, y, image_tex[id], 0, 1, 1, RGBA_WHITE);
}

void gfx_icon(IconId id, f32 x, f32 y, u32 time_ms)
{
	const IconAsset *icon = &icon_assets[id];
	u32 frame = icon->delay_ms ? (time_ms / icon->delay_ms) % icon->frames : 0;
	GRRLIB_DrawPart(x, y, (frame % icon->cols) * icon->cell_w, (frame / icon->cols) * icon->cell_h, icon->w,
		icon->h, icon_tex[id], 0, 1, 1, RGBA_WHITE);
}
