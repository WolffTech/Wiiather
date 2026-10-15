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
#include "screens/location_picker.h"
#include "weather/weather.h"

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
	weather_start(config.has_location ? &config.location : NULL, config.dir);
	char music_dir[sizeof config.dir + 8];
	snprintf(music_dir, sizeof music_dir, "%s/music", config.dir);
	music_start(music_dir);

	u64 start = gettime();
	static WeatherSnapshot snap;
	static DisplayData data;
	Rotation rotation = { 0 };
	ScrollBar scroll_bar = { 0 };
	u32 shown_generation = 0;
	bool picking = !config.has_location;
	LocationPicker picker = location_picker_make(NULL);
	while (!quit) {
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME)
			break;
		music_update();

		u32 now_ms = ticks_to_millisecs(gettime() - start);
		weather_snapshot(&snap);
		if (snap.generation != shown_generation) {
			displays_prepare(&snap, &data);
			shown_generation = snap.generation;
		}
		displays_tick(&snap, &data);

		if (picking) {
			Location chosen;
			PickerResult result = location_picker_update(&picker, pressed, &snap.zip_guess, &chosen);
			if (result == PICKER_CHOSEN) {
				config.location = chosen;
				config.has_location = true;
				// A failed save, such as on a locked SD card, still applies
				// the location until the app exits.
				config_save(&config);
				weather_set_location(&chosen);
				rotation = (Rotation){ 0 };
				scroll_bar = (ScrollBar){ 0 };
			}
			picking = result == PICKER_OPEN;
			// Also hold the picker on screen for the frame a ZIP is chosen,
			// while `data` still holds the old location.
			if (result != PICKER_CANCELED) {
				location_picker_draw(&picker, &snap.zip_guess);
				gfx_present();
				continue;
			}
			// Don't let the B press fall through.
			pressed = 0;
		}

		if (pressed & WPAD_BUTTON_A)
			music_toggle();
		if (pressed & WPAD_BUTTON_PLUS) {
			picker = location_picker_make(&config.location);
			picking = true;
		}

		if (data.has_current) {
			rotation_update(&rotation, &data, now_ms);
			displays_draw(&rotation, &data, now_ms);
			scroll_bar_draw(&scroll_bar, &data, displays_scroll_visible(&rotation), now_ms);
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
