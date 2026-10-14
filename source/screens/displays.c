#include "displays.h"

#define CURRENT_CONDITIONS_MS 9000 // ws4kp's default baseDelay
#define EXTENDED_SCREEN_MS 9000
#define LATEST_OBSERVATIONS_MS 9000
#define ALMANAC_MS 9000

typedef struct {
	size_t (*screen_count)(const DisplayData *d);
	u32 (*screen_ms)(const DisplayData *d, size_t screen);
	void (*draw)(const DisplayData *d, size_t screen, u32 time_ms);
} Display;

static size_t current_count(const DisplayData *d)
{
	return d->has_current;
}

static u32 current_ms(const DisplayData *d, size_t screen)
{
	return CURRENT_CONDITIONS_MS;
}

static void current_draw(const DisplayData *d, size_t screen, u32 time_ms)
{
	gfx_image(IMG_BACKGROUND_1, 0, 0);
	header_draw(d->stale ? "Recent" : "Current", "Conditions", true);
	current_conditions_draw(&d->current, time_ms);
}

static size_t latest_observations_count(const DisplayData *d)
{
	return d->latest_observations.count > 0;
}

static u32 latest_observations_ms(const DisplayData *d, size_t screen)
{
	return LATEST_OBSERVATIONS_MS;
}

static void latest_observations_screen(const DisplayData *d, size_t screen, u32 time_ms)
{
	gfx_image(IMG_BACKGROUND_1, 0, 0);
	header_draw("Latest", "Observations", true);
	latest_observations_draw(&d->latest_observations);
}

static size_t local_count(const DisplayData *d)
{
	return d->local.page_count;
}

static u32 local_ms(const DisplayData *d, size_t screen)
{
	return local_forecast_page_ms(&d->local, screen);
}

static void local_draw(const DisplayData *d, size_t screen, u32 time_ms)
{
	gfx_image(IMG_BACKGROUND_1, 0, 0);
	header_draw("Local", "Forecast", true);
	local_forecast_draw(&d->local, screen);
}

static size_t extended_count(const DisplayData *d)
{
	return d->extended.count ? EXTENDED_SCREENS : 0;
}

static u32 extended_ms(const DisplayData *d, size_t screen)
{
	return EXTENDED_SCREEN_MS;
}

static void extended_draw(const DisplayData *d, size_t screen, u32 time_ms)
{
	gfx_image(IMG_BACKGROUND_2, 0, 0);
	header_draw("Extended", "Forecast", false);
	extended_forecast_draw(&d->extended, screen, time_ms);
}

static size_t almanac_count(const DisplayData *d)
{
	return d->almanac.ready;
}

static u32 almanac_ms(const DisplayData *d, size_t screen)
{
	return ALMANAC_MS;
}

static void almanac_draw(const DisplayData *d, size_t screen, u32 time_ms)
{
	gfx_image(IMG_BACKGROUND_3, 0, 0);
	header_draw("Almanac", NULL, false);
	almanac_view_draw(&d->almanac);
}

// ws4kp's order, limited to the displays ported so far.
static const Display displays[] = {
	{ current_count, current_ms, current_draw },
	{ latest_observations_count, latest_observations_ms, latest_observations_screen },
	{ local_count, local_ms, local_draw },
	{ extended_count, extended_ms, extended_draw },
	{ almanac_count, almanac_ms, almanac_draw },
};
#define DISPLAY_COUNT (sizeof displays / sizeof displays[0])

void displays_prepare(const WeatherSnapshot *snap, DisplayData *out)
{
	out->has_current = snap->status == WEATHER_READY;
	out->stale = snap->observation.stale;
	if (out->has_current)
		format_current_conditions(&snap->observation, &out->current);
	if (snap->has_forecast) {
		local_forecast_prepare(&snap->forecast, &out->local);
		extended_forecast_prepare(&snap->forecast, &out->extended);
	} else {
		out->local.page_count = 0;
		out->extended.count = 0;
	}
	if (snap->has_latest_observations)
		latest_observations_prepare(&snap->latest_observations, &out->latest_observations);
	else
		out->latest_observations.count = 0;
	almanac_view_prepare(snap, &out->almanac);
}

void displays_tick(const WeatherSnapshot *snap, DisplayData *data)
{
	if (almanac_view_outdated(&data->almanac))
		almanac_view_prepare(snap, &data->almanac);
}

void rotation_update(Rotation *r, const DisplayData *data, u32 now_ms)
{
	const Display *d = &displays[r->display];
	size_t screens = d->screen_count(data);
	if (r->screen < screens && now_ms - r->started_ms < d->screen_ms(data, r->screen))
		return;
	r->started_ms = now_ms;
	if (r->screen + 1 < screens) {
		r->screen++;
		return;
	}
	r->screen = 0;
	for (size_t i = 1; i <= DISPLAY_COUNT; i++) {
		size_t next = (r->display + i) % DISPLAY_COUNT;
		if (displays[next].screen_count(data) > 0) {
			r->display = next;
			return;
		}
	}
}

void displays_draw(const Rotation *r, const DisplayData *data, u32 now_ms)
{
	const Display *d = &displays[r->display];
	if (r->screen < d->screen_count(data))
		d->draw(data, r->screen, now_ms);
}
