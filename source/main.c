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
#include "screens/screens.h"
#include "screens/settings_menu.h"
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
	music_start(music_dir, &config);
	header_set_clock_24h(config.clock_24h);
	if (!config.has_location && config.locate_by_ip)
		weather_request_zip_guess();

	u64 start = gettime();
	static WeatherSnapshot snap;
	static DisplayData data;
	Rotation rotation = { 0 };
	ScrollBar scroll_bar = { 0 };
	u32 shown_generation = 0;
	bool picking = !config.has_location;
	LocationPicker picker = location_picker_make(NULL);
	bool in_menu = false;
	SettingsMenu menu;
	unsigned menu_changes = 0;
	// The rotation runs on its own clock, which runs at the Speed setting's
	// rate and stops while the menu is open. `scaled_ms` carries the remainder.
	u32 rotation_ms = 0, last_ms = 0;
	u64 scaled_ms = 0;
	while (!quit) {
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME)
			break;
		music_update();

		u32 now_ms = ticks_to_millisecs(gettime() - start);
		if (!in_menu) {
			scaled_ms += (u64)(now_ms - last_ms) * 100;
			unsigned percent = config_speed_percent(config.speed);
			rotation_ms += (u32)(scaled_ms / percent);
			scaled_ms %= percent;
		}
		last_ms = now_ms;
		weather_snapshot(&snap);
		if (snap.generation != shown_generation) {
			displays_prepare(&snap, &data);
			shown_generation = snap.generation;
		}
		displays_tick(&snap, &data);

		if (picking) {
			Location chosen;
			PickerResult result = location_picker_update(&picker, pressed, settings_zip_guess(&config, &snap), &chosen);
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
			location_picker_draw(&picker, settings_zip_guess(&config, &snap));
			gfx_present();
			continue;
		}

		if (in_menu) {
			SettingsResult result = settings_menu_update(&menu, pressed, &config, &snap);
			menu_changes |= result.changed;
			if (result.changed & SETTINGS_LOCATION) {
				weather_set_location(&config.location);
				rotation = (Rotation){ 0 };
				scroll_bar = (ScrollBar){ 0 };
			}
			if (result.closed) {
				in_menu = false;
				header_set_clock_24h(config.clock_24h);
				// Don't let the B that closed the menu toggle the music.
				pressed = 0;
				if (menu_changes & SETTINGS_RESET) {
					// config_reset removed settings.ini; the next location
					// chosen saves a new one.
					picker = location_picker_make(NULL);
					picking = true;
					if (config.locate_by_ip)
						weather_request_zip_guess();
					music_configure(&config);
				} else if (menu_changes) {
					config_save(&config);
				}
			} else {
				settings_menu_draw(&menu, &config, &snap);
				gfx_present();
				continue;
			}
		} else if ((pressed & WPAD_BUTTON_A) && config.has_location) {
			menu = settings_menu_make();
			menu_changes = 0;
			in_menu = true;
			settings_menu_draw(&menu, &config, &snap);
			gfx_present();
			continue;
		}

		if (pressed & WPAD_BUTTON_B) {
			config.music = !config.music;
			music_configure(&config);
			config_save(&config);
		}

		if (data.has_current) {
			rotation_update(&rotation, &data, &config, rotation_ms);
			displays_draw(&rotation, &data, rotation_ms, now_ms);
			scroll_bar_draw(&scroll_bar, &data, displays_scroll_visible(&rotation), config.bottom_bar, rotation_ms);
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
