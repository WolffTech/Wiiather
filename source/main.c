#include <fat.h>
#include <gccore.h>
#include <grrlib.h>
#include <ogc/lwp_watchdog.h>
#include <stdio.h>
#include <wiiuse/wpad.h>

#include "audio/music.h"
#include "config.h"
#include "render/gfx.h"
#include "screens/displays.h"
#include "weather/weather.h"

#define SCROLL_LINE_MS 4000

static volatile bool quit, power_off;

static void on_reset(u32 irq, void *ctx)
{
	quit = true;
}

static void on_power(void)
{
	quit = power_off = true;
}

int main(int argc, char **argv)
{
	GRRLIB_Init();
	WPAD_Init();
	SYS_SetResetCallback(on_reset);
	SYS_SetPowerCallback(on_power);
	fatInitDefault();
	gfx_init();

	Config config;
	config_load(argc > 0 ? argv[0] : NULL, &config);
	weather_start(config.lat, config.lon);
	char music_dir[sizeof config.dir + 8];
	snprintf(music_dir, sizeof music_dir, "%s/music", config.dir);
	music_start(music_dir);

	u64 start = gettime();
	static WeatherSnapshot snap;
	static DisplayData data;
	Rotation rotation = { 0 };
	u32 shown_generation = 0;
	while (!quit) {
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME)
			break;
		if (pressed & WPAD_BUTTON_A)
			music_toggle();
		music_update();

		u32 now_ms = ticks_to_millisecs(gettime() - start);
		weather_snapshot(&snap);
		if (snap.generation != shown_generation) {
			displays_prepare(&snap, &data);
			shown_generation = snap.generation;
		}

		if (data.has_current) {
			rotation_update(&rotation, &data, now_ms);
			displays_draw(&rotation, &data, now_ms);
			scroll_draw(data.current.scroll[(now_ms / SCROLL_LINE_MS) % data.current.scroll_count]);
		} else {
			gfx_image(IMG_BACKGROUND_1, 0, 0);
			header_draw("Current", "Conditions", true);
			status_draw(&snap);
		}
		gfx_present();
	}

	GRRLIB_Exit();
	if (power_off)
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	return 0;
}
