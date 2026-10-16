// Port of ws4kp's Regional Forecast (regionalforecast.mjs, regional-forecast.ejs,
// _regional-forecast.scss): the map around the location with each site's
// observation, then its next two forecast periods.
#include <stdio.h>
#include <string.h>

#include "render/gfx.h"
#include "screens.h"
#include "weather/nws.h"

#define MAIN_TOP 90
#define MAIN_HEIGHT 310

// Offsets from each site's anchor, measured in Chrome.
#define NAME_LEFT (-40)
#define NAME_TOP (-35)
#define ICON_LEFT 4
#define ICON_TOP (-9)
#define ICON_MAX_HEIGHT 32
#define TEMP_LEFT (-40)
#define TEMP_TOP (-5)
#define TEMP_WIDTH 40

static const TextStyle name_style = { FONT_STAR4000_20, RGBA_WHITE, true, false, 0 };
static const TextStyle temp_style = { FONT_STAR4000_LARGE_28, RGBA_YELLOW, true, false, 0 };

static MapId map_id(RegionalMap map)
{
	switch (map) {
	case REGIONAL_MAP_ALASKA:
		return MAP_ALASKA;
	case REGIONAL_MAP_HAWAII:
		return MAP_HAWAII;
	case REGIONAL_MAP_CONUS:
		break;
	}
	return MAP_CONUS;
}

void regional_forecast_prepare(const RegionalForecast *regional, Units units, RegionalForecastView *out)
{
	out->map = map_id(regional->frame.map);
	out->origin_x = regional->frame.origin_x;
	out->origin_y = regional->frame.origin_y;
	// Sites without data are left off the map.
	out->count = 0;
	const RegionalData *first = NULL;
	for (size_t i = 0; i < regional->count; i++) {
		if (!regional->has_data[i])
			continue;
		const RegionalData *data = &regional->data[i];
		if (!first)
			first = data;
		const RegionalSite *site = &regional->sites[i];
		RegionalLabel *label = &out->labels[out->count++];
		memcpy(label->name, site->name, sizeof label->name);
		label->x = site->x;
		label->y = site->y;
		for (size_t s = 0; s < REGIONAL_SCREENS; s++) {
			label->icons[s] = nws_small_icon(data->periods[s].icon_url);
			snprintf(label->temperatures[s], sizeof label->temperatures[s], "%d",
				units_temperature_convert(data->periods[s].temperature, UNITS_US, units));
		}
	}

	// ws4kp titles the forecasts after the first site's periods.
	snprintf(out->title_bottom[0], sizeof out->title_bottom[0], "Observations");
	for (size_t s = 1; s < REGIONAL_SCREENS; s++) {
		const RegionalPeriod *p = first ? &first->periods[s] : NULL;
		snprintf(out->title_bottom[s], sizeof out->title_bottom[s], "%s%s",
			p ? forecast_weekday_name(p->weekday) : "", p && !p->is_daytime ? " Night" : "");
	}
}

const char *regional_forecast_title_top(size_t screen)
{
	return screen == 0 ? "Regional" : "Forecast for";
}

static void draw_label(const RegionalLabel *label, size_t screen, u32 time_ms)
{
	f32 x = label->x, y = MAIN_TOP + label->y;

	// img { max-height: 32px } keeps the aspect ratio.
	IconId icon = label->icons[screen];
	const IconAsset *a = &icon_assets[icon];
	f32 h = a->h > ICON_MAX_HEIGHT ? ICON_MAX_HEIGHT : a->h;
	gfx_icon_scaled(icon, x + ICON_LEFT, y + ICON_TOP, a->w * h / a->h, h, time_ms);

	gfx_text(&name_style, x + NAME_LEFT, y + NAME_TOP, ALIGN_LEFT, label->name);

	// Right-aligned in its box; text too wide starts at the box's left edge
	// and overflows to the right.
	const char *temp = label->temperatures[screen];
	f32 w = gfx_text_width(temp_style.font, temp);
	gfx_text(&temp_style, x + TEMP_LEFT + (w < TEMP_WIDTH ? TEMP_WIDTH - w : 0), y + TEMP_TOP, ALIGN_LEFT, temp);
}

void regional_forecast_draw(const RegionalForecastView *view, size_t screen, u32 time_ms)
{
	gfx_clip(0, MAIN_TOP, SCREEN_WIDTH, MAIN_HEIGHT);
	gfx_map(view->map, view->origin_x, view->origin_y, REGIONAL_MAP_SCALE, 0, MAIN_TOP, SCREEN_WIDTH, MAIN_HEIGHT);
	for (size_t i = 0; i < view->count; i++)
		draw_label(&view->labels[i], screen, time_ms);
	gfx_clip_reset();
}
