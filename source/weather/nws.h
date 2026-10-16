#pragma once
#include <stdbool.h>
#include <stddef.h>

#include "alerts.h"
#include "assets_gen.h"
#include "forecast.h"
#include "hourly.h"
#include "latest_obs.h"
#include "obs.h"
#include "regional.h"
#include "spc.h"
#include "travel_cities_gen.h"

typedef struct {
	char city[48];
	char state[8];
	char observation_stations_url[160];
	char forecast_url[160];
	char forecast_grid_data_url[160];
	char time_zone[48];
} NwsPoint;

bool nws_get_point(double lat, double lon, NwsPoint *out, char *err, size_t err_len);

// Walks the nearby stations until one has usable data, like ws4kp, patching
// gaps from the METAR report and forecast.weather.gov's MapClick feed.
bool nws_get_current_observation(const NwsPoint *point, Observation *out, char *err, size_t err_len);

// The 12-hour forecast with expired periods removed.
bool nws_get_forecast(const NwsPoint *point, Forecast *out, char *err, size_t err_len);

// One Travel Forecast city's day, from its 12-hour forecast.
bool nws_get_travel_day(const TravelCity *city, TravelDay *out);

// A Regional Forecast site and the lookups its fetches need, cached because
// gridpoints and their stations don't move.
typedef struct {
	RegionalSite site; // its gridpoint is filled in once looked up
	char station_id[8]; // "" until looked up
} RegionalSource;

// One Regional Forecast site's latest observation and next two forecast periods.
bool nws_get_regional(RegionalSource *source, RegionalData *out);

// Gridpoint data expanded to hours from the server's current hour.
bool nws_get_hourly(const NwsPoint *point, HourlyForecast *out, char *err, size_t err_len);

// The nearest stations with complete latest observations, as ws4kp picks them.
bool nws_get_latest_observations(double lat, double lon, LatestObservations *out);

IconId nws_large_icon(const char *icon_url);
IconId nws_small_icon(const char *icon_url);

// Active alerts for the location, selected and ordered like ws4kp's Hazards,
// with expired ones dropped. `now` receives the server's clock (UTC, 0 if unknown).
bool nws_get_alerts(double lat, double lon, Alerts *out, time_t *now, char *err, size_t err_len);

// The Storm Prediction Center's categorical risk at the location for each
// day, from its outlook polygons like ws4kp. False if no day loaded.
bool nws_get_spc_outlook(double lat, double lon, SpcOutlook *out);
