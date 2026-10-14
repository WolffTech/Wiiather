#pragma once
#include <stdbool.h>
#include <stddef.h>

#include "assets_gen.h"
#include "obs.h"

typedef struct {
	char city[48];
	char state[8];
	char observation_stations_url[160];
	char forecast_url[160];
	char time_zone[48];
} NwsPoint;

bool nws_get_point(double lat, double lon, NwsPoint *out, char *err, size_t err_len);

// Walks the nearby stations until one has usable data, like ws4kp, patching
// gaps from the METAR report and forecast.weather.gov's MapClick feed.
bool nws_get_current_observation(const NwsPoint *point, Observation *out, char *err, size_t err_len);

IconId nws_large_icon(const char *icon_url);
