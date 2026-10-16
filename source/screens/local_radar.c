// Port of ws4kp's Local Radar (radar.mjs, radar.ejs, _radar.scss): the
// newest composite images, stretched over the radar map and looped.
#include <grrlib.h>

#include "render/gfx.h"
#include "screens.h"

// .main.radar
#define MAIN_TOP 113
#define MAIN_HEIGHT 367
// GX textures are made of 4x4 blocks.
#define TEXTURE_HEIGHT ((RADAR_HEIGHT + 3) / 4 * 4)

// The header's precipitation scale, measured in Chrome. The table's collapsed
// 2px borders surround each box.
#define SCALE_CENTER 460
#define SCALE_LEFT 383
#define SCALE_TOP 60
#define SCALE_BORDER 2
#define SCALE_BOX_WIDTH 17
#define SCALE_BOX_HEIGHT 24
#define SCALE_BOXES 8

static const TextStyle title_style = { FONT_ARIAL_BOLD_38, RGBA_WHITE, true, false, 0 };
static const TextStyle precip_style = { FONT_STAR4000_24, RGBA_WHITE, true, true, 0 };
static const TextStyle time_style = { FONT_STAR4000_SMALL_32, RGBA_WHITE, true, false, 0 };

// radar.ejs repeats box-7 rather than using box-8.
static const u32 scale_colors[SCALE_BOXES] = { 0x31D216FF, 0x1C8A12FF, 0x145A0FFF, 0x0A280AFF, 0xC4B346FF,
	0xBE4813FF, 0xAB0E0EFF, 0xAB0E0EFF };

static GRRLIB_texImg *textures[RADAR_IMAGES];

void radar_view_prepare(const WeatherSnapshot *snap, RadarView *out)
{
	// The images themselves arrive through radar_view_update().
	out->has_utc_offset = snap->has_utc_offset;
	out->utc_offset = snap->utc_offset;
	radar_map_origin(snap->location.lat, snap->location.lon, &out->map_x, &out->map_y);
}

void radar_view_update(RadarView *view)
{
	static RadarImages images; // too large for the stack
	if (!weather_radar(&view->generation, &images))
		return;
	view->count = 0;
	for (size_t i = 0; i < images.count; i++) {
		if (!textures[i]) {
			textures[i] = GRRLIB_CreateEmptyTexture(RADAR_WIDTH, TEXTURE_HEIGHT);
			if (!textures[i])
				return;
			// A zero handle, so DrawPart scales about the image's middle.
			GRRLIB_SetHandle(textures[i], RADAR_WIDTH / 2, TEXTURE_HEIGHT / 2);
		}
		const RadarImage *image = &images.images[i];
		for (int y = 0; y < RADAR_HEIGHT; y++) {
			for (int x = 0; x < RADAR_WIDTH; x++)
				GRRLIB_SetPixelTotexImg(x, y, textures[i], image->colors[image->pixels[y][x]]);
		}
		GRRLIB_FlushTex(textures[i]);
		view->times[i] = image->time;
		view->count = i + 1;
	}
}

bool radar_view_ready(const RadarView *view)
{
	return view->count > 0 && view->has_utc_offset;
}

static void header(time_t local)
{
	gfx_image(IMG_LOGO_CORNER, 50, 30);
	gfx_text(&title_style, 155, 26, ALIGN_LEFT, "Local");
	gfx_text(&title_style, 155, 61, ALIGN_LEFT, "Radar");

	gfx_text(&precip_style, SCALE_CENTER, 32, ALIGN_CENTER, "PRECIP");
	gfx_text(&precip_style, 317, 59, ALIGN_LEFT, "Light");
	gfx_text(&precip_style, 548, 59, ALIGN_LEFT, "Heavy");
	f32 step = SCALE_BOX_WIDTH + SCALE_BORDER;
	GRRLIB_Rectangle(SCALE_LEFT, SCALE_TOP, SCALE_BOXES * step + SCALE_BORDER, SCALE_BOX_HEIGHT + 2 * SCALE_BORDER,
		RGBA_BLACK, true);
	for (int i = 0; i < SCALE_BOXES; i++)
		GRRLIB_Rectangle(SCALE_LEFT + SCALE_BORDER + i * step, SCALE_TOP + SCALE_BORDER, SCALE_BOX_WIDTH,
			SCALE_BOX_HEIGHT, scale_colors[i], true);

	char label[12];
	radar_time_label(local, label);
	gfx_text(&time_style, SCALE_CENTER, 78, ALIGN_CENTER, label);
}

void radar_view_draw(const RadarView *view, size_t image)
{
	gfx_image(IMG_BACKGROUND_4, 0, 0);
	gfx_clip(0, MAIN_TOP, SCREEN_WIDTH, MAIN_HEIGHT);
	gfx_map(MAP_RADAR, view->map_x, view->map_y, 1, 0, MAIN_TOP, SCREEN_WIDTH, MAIN_HEIGHT);
	// Stretched over the main area without smoothing, like ws4kp's canvas.
	f32 sx = (f32)SCREEN_WIDTH / RADAR_WIDTH, sy = (f32)MAIN_HEIGHT / RADAR_HEIGHT;
	GRRLIB_DrawPart(RADAR_WIDTH * (sx - 1) / 2, MAIN_TOP + RADAR_HEIGHT * (sy - 1) / 2, 0, 0, RADAR_WIDTH,
		RADAR_HEIGHT, textures[image], 0, sx, sy, RGBA_WHITE);
	gfx_map(MAP_RADAR_OVERLAY, view->map_x, view->map_y, 1, 0, MAIN_TOP, SCREEN_WIDTH, MAIN_HEIGHT);
	gfx_clip_reset();
	header(view->times[image] + view->utc_offset);
}
