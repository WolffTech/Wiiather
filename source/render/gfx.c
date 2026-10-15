#include "gfx.h"

#include <grrlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

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
	for (int i = 0; i < ICON_COUNT; i++) {
		icon_tex[i] = GRRLIB_LoadTexturePNG(icon_assets[i].png);
		// The default handle is the sheet's center, which shifts scaled frames
		// sideways; a zero handle makes DrawPart scale about the frame's center.
		GRRLIB_SetHandle(icon_tex[i], icon_tex[i]->w / 2, icon_tex[i]->h / 2);
	}
	for (int i = 0; i < IMG_COUNT; i++)
		image_tex[i] = GRRLIB_LoadTexturePNG(image_assets[i].png);
}

void gfx_present(void)
{
	// GRRLIB enables the deflicker copy filter whenever it draws with
	// antialiasing on; keep pixel art sharp regardless of the last draw.
	GX_SetCopyFilter(GX_FALSE, rmode->sample_pattern, GX_FALSE, rmode->vfilter);
	GRRLIB_Render();
}

f32 gfx_line_height(FontId font)
{
	const FontAsset *f = &font_assets[font];
	return f->ascent + f->descent + f->line_gap;
}

static int advance(const FontAsset *f, char c)
{
	return f->glyphs[(u8)c].advance;
}

// A break is allowed after a hyphen between letters ("north-northwest").
static bool hyphen_break(const char *p)
{
	return p[0] == '-' && p[1] && isalpha((u8)p[1]);
}

size_t gfx_wrap(FontId font, const char *text, f32 width, char (*lines)[GFX_LINE_MAX], size_t max_lines)
{
	const FontAsset *f = &font_assets[font];
	size_t count = 0, len = 0;
	int line_width = 0;
	bool space_pending = false;
	const char *p = text;
	while (*p && count < max_lines) {
		if (*p == ' ') {
			space_pending = len > 0;
			p++;
			continue;
		}
		// The next unbreakable piece: up to a space or just past a hyphen break.
		const char *end = p;
		int piece_width = 0;
		while (*end && *end != ' ') {
			piece_width += advance(f, *end);
			if (hyphen_break(end++) && end > p + 1)
				break;
		}
		size_t piece_len = end - p;
		int space = space_pending ? advance(f, ' ') : 0;
		bool fits = len == 0 || line_width + space + piece_width <= width;
		if (!fits) {
			lines[count][len] = '\0';
			if (++count == max_lines)
				return count;
			len = 0;
			line_width = 0;
			space = 0;
		}
		if (space && len + 1 < GFX_LINE_MAX)
			lines[count][len++] = ' ';
		size_t copy = piece_len < GFX_LINE_MAX - 1 - len ? piece_len : GFX_LINE_MAX - 1 - len;
		memcpy(&lines[count][len], p, copy);
		len += copy;
		line_width += space + piece_width;
		space_pending = false;
		p = end;
	}
	if (len > 0 && count < max_lines) {
		lines[count][len] = '\0';
		count++;
	}
	return count;
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
	f32 line_height = style->line_height > 0 ? style->line_height : gfx_line_height(style->font);
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

void gfx_clip(f32 x, f32 y, f32 w, f32 h)
{
	// The scissor works in framebuffer pixels, and the framebuffer may be taller than 480.
	f32 sx = rmode->fbWidth / (f32)SCREEN_WIDTH, sy = rmode->efbHeight / (f32)SCREEN_HEIGHT;
	GRRLIB_ClipDrawing(x * sx, y * sy, w * sx, h * sy);
}

void gfx_clip_reset(void)
{
	GRRLIB_ClipReset();
}

void gfx_icon_scaled(IconId id, f32 x, f32 y, f32 w, f32 h, u32 time_ms)
{
	const IconAsset *icon = &icon_assets[id];
	u32 frame = icon->delay_ms ? (time_ms / icon->delay_ms) % icon->frames : 0;
	bool scaled = w != icon->w || h != icon->h;
	// GRRLIB scales about the middle of the part.
	f32 left = x + (w - icon->w) / 2, top = y + (h - icon->h) / 2;
	GRRLIB_SetAntiAliasing(scaled);
	GRRLIB_DrawPart(left, top, (frame % icon->cols) * icon->cell_w, (frame / icon->cols) * icon->cell_h, icon->w,
		icon->h, icon_tex[id], 0, w / icon->w, h / icon->h, RGBA_WHITE);
	GRRLIB_SetAntiAliasing(false);
}

void gfx_icon(IconId id, f32 x, f32 y, u32 time_ms)
{
	const IconAsset *icon = &icon_assets[id];
	gfx_icon_scaled(id, x, y, icon->w, icon->h, time_ms);
}
