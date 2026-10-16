#include "displays.h"

#define CURRENT_CONDITIONS_MS 9000 // ws4kp's default baseDelay
#define EXTENDED_SCREEN_MS 9000
#define LATEST_OBSERVATIONS_MS 9000
#define ALMANAC_MS 9000
#define HOURLY_GRAPH_MS 9000
#define REGIONAL_SCREEN_MS 9000
#define SPC_OUTLOOK_MS 9000

typedef struct {
	size_t (*screen_count)(const DisplayData *d);
	u32 (*screen_ms)(const DisplayData *d, size_t screen);
	void (*draw)(const DisplayData *d, size_t screen, u32 time_ms);
} Display;

// Rotation time since the screen started, for displays that animate from their
// start. It runs at the Speed setting's rate, unlike the icon animations.
static u32 screen_elapsed_ms;

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
	hazards_draw(&d->hazards, screen_elapsed_ms);
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
	hourly_view_draw(&d->hourly, screen_elapsed_ms, time_ms);
}

static size_t hourly_graph_count(const DisplayData *d)
{
	return d->hourly_graph.ready;
}

static u32 hourly_graph_ms(const DisplayData *d, size_t screen)
{
	return HOURLY_GRAPH_MS;
}

static void hourly_graph_draw(const DisplayData *d, size_t screen, u32 time_ms)
{
	gfx_image(IMG_BACKGROUND_1_CHART, 0, 0);
	header_title_draw("Hourly Graph", NULL);
	hourly_graph_view_draw(&d->hourly_graph);
}

static size_t travel_count(const DisplayData *d)
{
	return d->travel.count > 0;
}

static u32 travel_ms(const DisplayData *d, size_t screen)
{
	return travel_forecast_ms(&d->travel);
}

static void travel_draw(const DisplayData *d, size_t screen, u32 time_ms)
{
	gfx_image(IMG_BACKGROUND_1, 0, 0);
	header_draw("Travel Forecast", d->travel.title_bottom, false);
	travel_forecast_draw(&d->travel, screen_elapsed_ms, time_ms);
}

static size_t regional_count(const DisplayData *d)
{
	return d->regional.count ? REGIONAL_SCREENS : 0;
}

static u32 regional_ms(const DisplayData *d, size_t screen)
{
	return REGIONAL_SCREEN_MS;
}

static void regional_draw(const DisplayData *d, size_t screen, u32 time_ms)
{
	gfx_image(IMG_BACKGROUND_1, 0, 0);
	header_draw(regional_forecast_title_top(screen), d->regional.title_bottom[screen], false);
	regional_forecast_draw(&d->regional, screen, time_ms);
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

static size_t spc_outlook_count(const DisplayData *d)
{
	return d->spc_outlook.ready;
}

static u32 spc_outlook_ms(const DisplayData *d, size_t screen)
{
	return SPC_OUTLOOK_MS;
}

static void spc_outlook_draw(const DisplayData *d, size_t screen, u32 time_ms)
{
	gfx_image(IMG_BACKGROUND_6, 0, 0);
	header_draw("Storm Prediction", "Center Outlook", false);
	spc_outlook_view_draw(&d->spc_outlook);
}

static size_t radar_count(const DisplayData *d)
{
	return radar_view_ready(&d->radar) ? RADAR_STEPS : 0;
}

static u32 radar_ms(const DisplayData *d, size_t screen)
{
	return radar_step_ms(screen);
}

static void radar_draw(const DisplayData *d, size_t screen, u32 time_ms)
{
	radar_view_draw(&d->radar, radar_step_image(screen, d->radar.count));
}

// ws4kp's order, limited to the displays ported so far.
static const Display displays[DISPLAY_COUNT] = {
	[DISPLAY_HAZARDS] = { hazards_count, hazards_screen_ms, hazards_display_draw },
	[DISPLAY_CURRENT] = { current_count, current_ms, current_draw },
	[DISPLAY_LATEST_OBSERVATIONS] = { latest_observations_count, latest_observations_ms, latest_observations_screen },
	[DISPLAY_HOURLY] = { hourly_count, hourly_ms, hourly_draw },
	[DISPLAY_HOURLY_GRAPH] = { hourly_graph_count, hourly_graph_ms, hourly_graph_draw },
	[DISPLAY_TRAVEL] = { travel_count, travel_ms, travel_draw },
	[DISPLAY_REGIONAL] = { regional_count, regional_ms, regional_draw },
	[DISPLAY_LOCAL] = { local_count, local_ms, local_draw },
	[DISPLAY_EXTENDED] = { extended_count, extended_ms, extended_draw },
	[DISPLAY_ALMANAC] = { almanac_count, almanac_ms, almanac_draw },
	[DISPLAY_SPC_OUTLOOK] = { spc_outlook_count, spc_outlook_ms, spc_outlook_draw },
	[DISPLAY_RADAR] = { radar_count, radar_ms, radar_draw },
};

// Hidden displays have no screens, so the rotation skips them like
// displays without data.
static size_t screen_count(const DisplayData *data, DisplayId id, const Config *config)
{
	return config_display_shown(config, id) ? displays[id].screen_count(data) : 0;
}

void displays_prepare(const WeatherSnapshot *snap, DisplayData *out)
{
	out->has_current = snap->status == WEATHER_READY;
	out->stale = snap->observation.stale;
	if (out->has_current)
		format_current_conditions(&snap->observation, snap->units, &out->current);
	if (snap->has_forecast) {
		local_forecast_prepare(&snap->forecast, &out->local);
		extended_forecast_prepare(&snap->forecast, snap->units, &out->extended);
	} else {
		out->local.page_count = 0;
		out->extended.count = 0;
	}
	hourly_view_prepare(snap, &out->hourly);
	hourly_graph_view_prepare(snap, &out->hourly_graph);
	travel_forecast_prepare(&snap->travel, snap->units, &out->travel);
	regional_forecast_prepare(&snap->regional, snap->units, &out->regional);
	if (snap->has_latest_observations)
		latest_observations_prepare(&snap->latest_observations, snap->units, &out->latest_observations);
	else
		out->latest_observations.count = 0;
	almanac_view_prepare(snap, &out->almanac);
	spc_outlook_view_prepare(snap, &out->spc_outlook);
	radar_view_prepare(snap, &out->radar);
	hazards_prepare(snap, &out->hazards);
}

void displays_tick(const WeatherSnapshot *snap, DisplayData *data)
{
	if (almanac_view_outdated(&data->almanac))
		almanac_view_prepare(snap, &data->almanac);
	if (spc_outlook_view_outdated(&data->spc_outlook))
		spc_outlook_view_prepare(snap, &data->spc_outlook);
	if (hourly_view_outdated(&data->hourly))
		hourly_view_prepare(snap, &data->hourly);
	if (hourly_graph_view_outdated(&data->hourly_graph))
		hourly_graph_view_prepare(snap, &data->hourly_graph);
	radar_view_update(&data->radar);
}

void rotation_update(Rotation *r, DisplayData *data, const Config *config, u32 now_ms)
{
	if (!r->running) {
		r->running = true;
		r->started_ms = now_ms;
	}
	const Display *d = &displays[r->display];
	size_t screens = screen_count(data, r->display, config);
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
		if (screen_count(data, next, config) > 0) {
			r->display = (DisplayId)next;
			return;
		}
	}
}

void displays_draw(const Rotation *r, const DisplayData *data, u32 rotation_ms, u32 now_ms)
{
	const Display *d = &displays[r->display];
	screen_elapsed_ms = rotation_ms - r->started_ms;
	if (r->screen < d->screen_count(data)) {
		d->draw(data, r->screen, now_ms);
		// Hazards fill the screen without a header.
		if (d->draw != hazards_display_draw)
			header_logo_draw();
	}
}

bool displays_scroll_visible(const Rotation *r)
{
	// Hazards and the radar fill the screen; ws4kp sets
	// okToDrawCurrentConditions = false.
	const Display *d = &displays[r->display];
	return d->draw != hazards_display_draw && d->draw != radar_draw;
}
