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

// For displays that animate from their start rather than from boot.
static u32 screen_started_ms;

static size_t hazards_count(const DisplayData *d)
{
	return d->hazards.shown_count > 0;
}

static u32 hazards_screen_ms(const DisplayData *d, size_t screen)
{
	return hazards_ms(&d->hazards);
}

static void hazards_display_draw(const DisplayData *d, size_t screen, u32 time_ms)
{
	hazards_draw(&d->hazards, time_ms - screen_started_ms);
}

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

static size_t hourly_count(const DisplayData *d)
{
	return d->hourly.count > 0;
}

static u32 hourly_ms(const DisplayData *d, size_t screen)
{
	return hourly_view_ms(&d->hourly);
}

static void hourly_draw(const DisplayData *d, size_t screen, u32 time_ms)
{
	gfx_image(IMG_BACKGROUND_1, 0, 0);
	header_draw("Hourly Forecast", NULL, false);
	hourly_view_draw(&d->hourly, time_ms - screen_started_ms, time_ms);
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
	{ hazards_count, hazards_screen_ms, hazards_display_draw },
	{ current_count, current_ms, current_draw },
	{ latest_observations_count, latest_observations_ms, latest_observations_screen },
	{ hourly_count, hourly_ms, hourly_draw },
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
	hourly_view_prepare(snap, &out->hourly);
	if (snap->has_latest_observations)
		latest_observations_prepare(&snap->latest_observations, &out->latest_observations);
	else
		out->latest_observations.count = 0;
	almanac_view_prepare(snap, &out->almanac);
	hazards_prepare(snap, &out->hazards);
}

void displays_tick(const WeatherSnapshot *snap, DisplayData *data)
{
	if (almanac_view_outdated(&data->almanac))
		almanac_view_prepare(snap, &data->almanac);
	if (hourly_view_outdated(&data->hourly))
		hourly_view_prepare(snap, &data->hourly);
}

void rotation_update(Rotation *r, DisplayData *data, u32 now_ms)
{
	if (!r->running) {
		r->running = true;
		r->started_ms = now_ms;
	}
	const Display *d = &displays[r->display];
	size_t screens = d->screen_count(data);
	if (r->screen < screens && now_ms - r->started_ms < d->screen_ms(data, r->screen))
		return;
	r->started_ms = now_ms;
	if (r->screen + 1 < screens) {
		r->screen++;
		return;
	}
	// ws4kp shows alerts once, until they change or ten fetches pass.
	if (r->screen < screens && d->draw == hazards_display_draw)
		hazards_mark_viewed(&data->hazards);
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
	screen_started_ms = r->started_ms;
	if (r->screen < d->screen_count(data))
		d->draw(data, r->screen, now_ms);
}

bool displays_scroll_visible(const Rotation *r)
{
	// Hazards fills the screen; ws4kp sets okToDrawCurrentConditions = false.
	return displays[r->display].draw != hazards_display_draw;
}
