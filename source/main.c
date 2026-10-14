#include <fat.h>
#include <gccore.h>
#include <grrlib.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>

#include "config.h"
#include "render/gfx.h"
#include "screens/screens.h"
#include "weather/format.h"
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

	u64 start = gettime();
	CurrentConditionsText cc;
	u32 shown_generation = 0;
	while (!quit) {
		WPAD_ScanPads();
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
			break;

		u32 now_ms = ticks_to_millisecs(gettime() - start);
		WeatherSnapshot snap;
		weather_snapshot(&snap);
		if (snap.status == WEATHER_READY && snap.generation != shown_generation) {
			format_current_conditions(&snap.observation, &cc);
			shown_generation = snap.generation;
		}

		gfx_image(IMG_BACKGROUND_1, 0, 0);
		header_draw("Current", "Conditions");
		if (shown_generation) {
			current_conditions_draw(&cc, now_ms);
			scroll_draw(cc.scroll[(now_ms / SCROLL_LINE_MS) % cc.scroll_count]);
		} else {
			status_draw(&snap);
		}
		GRRLIB_Render();
	}

	GRRLIB_Exit();
	if (power_off)
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	return 0;
}
