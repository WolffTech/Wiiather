// Port of ws4kp's Latest Observations (latestobservations.mjs,
// _latest-observations.scss). Column headers sit 14px above the main area;
// rows are 40px apart after 10px of padding, with text 8px into each row.
#include <stdio.h>

#include "screens.h"

#define MAIN_LEFT 64
#define MAIN_TOP 90
#define HEADER_TOP (MAIN_TOP - 14)
#define ROW_TOP (MAIN_TOP + 10 + 8)
#define ROW_PITCH 40
#define TEMP_LEFT (MAIN_LEFT + 230)
#define WEATHER_LEFT (MAIN_LEFT + 280)
#define WIND_LEFT (MAIN_LEFT + 430)

static const TextStyle header_style = { FONT_STAR4000_SMALL_32, RGBA_WHITE, true, false, 0 };
static const TextStyle row_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, 0 };

void latest_observations_prepare(const LatestObservations *obs, Units units, LatestObservationsView *out)
{
	out->units = units;
	out->count = latest_obs_format(obs, units, out->rows);
}

void latest_observations_draw(const LatestObservationsView *view)
{
	char degrees[3];
	snprintf(degrees, sizeof degrees, "\xB0%s", units_temperature_name(view->units));
	gfx_text(&header_style, TEMP_LEFT, HEADER_TOP, ALIGN_LEFT, degrees);
	gfx_text(&header_style, WEATHER_LEFT, HEADER_TOP, ALIGN_LEFT, "Weather");
	gfx_text(&header_style, WIND_LEFT, HEADER_TOP, ALIGN_LEFT, "Wind");

	for (size_t i = 0; i < view->count; i++) {
		const LatestObsRow *row = &view->rows[i];
		f32 top = ROW_TOP + i * ROW_PITCH;
		gfx_text(&row_style, MAIN_LEFT, top, ALIGN_LEFT, row->location);
		gfx_text(&row_style, TEMP_LEFT, top, ALIGN_LEFT, row->temperature);
		gfx_text(&row_style, WEATHER_LEFT, top, ALIGN_LEFT, row->weather);
		gfx_text(&row_style, WIND_LEFT, top, ALIGN_LEFT, row->wind);
	}
}
