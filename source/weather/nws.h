#pragma once
#include <stdbool.h>
#include <stddef.h>

#include "assets_gen.h"

// An NWS quantity; NWS reports missing readings as null.
typedef struct {
	bool valid;
	double value;
} Measure;

typedef struct {
	char city[48];
	char state[8];
	char observation_stations_url[160];
	char forecast_url[160];
	char time_zone[48];
} NwsPoint;

// Raw observation in the API's SI units (degC, km/h, Pa, m, %).
typedef struct {
	char station_id[8];
	char station_name[64];
	char text_description[64];
	char icon_url[160];
	Measure temperature;
	Measure dewpoint;
	Measure wind_direction;
	Measure wind_speed;
	Measure wind_gust;
	Measure pressure;
	Measure pressure_previous; // from the prior observation, for the trend
	Measure visibility;
	Measure relative_humidity;
	Measure heat_index;
	Measure wind_chill;
	Measure ceiling; // base of the lowest cloud layer
} Observation;

bool nws_get_point(double lat, double lon, NwsPoint *out, char *err, size_t err_len);

// Walks the nearby stations until one has usable data, like ws4kp.
bool nws_get_current_observation(const NwsPoint *point, Observation *out, char *err, size_t err_len);

IconId nws_large_icon(const char *icon_url);
