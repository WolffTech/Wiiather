// Regional Forecast map framing, site selection and data shaping ported from
// ws4kp's regionalforecast.mjs and regionalforecast-utils.mjs. Pure logic with
// no I/O, so it can be tested on the host.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "forecast.h"
#include "obs.h"
#include "regional_cities_gen.h"

// Area-wise, no more non-overlapping 105x50 label boxes fit in the band
// their anchors are allowed in, so selection never drops a site.
#define REGIONAL_MAX_SITES 37

// The observation, then the next two forecast periods.
#define REGIONAL_SCREENS 3

typedef enum { REGIONAL_MAP_CONUS, REGIONAL_MAP_ALASKA, REGIONAL_MAP_HAWAII } RegionalMap;

// The part of the map shown for a location.
typedef struct {
	RegionalMap map;
	double origin_x, origin_y; // map pixel at the main area's top-left (ws4kp's sourceXY)
	double min_lat, max_lat, min_lon, max_lon; // cities strictly inside are candidates
} RegionalFrame;

// `state` is the NWS point's two-letter state; AK and HI have their own maps.
RegionalFrame regional_frame(double lat, double lon, const char *state);

// The map is drawn scaled from 480 map pixels to the 640-pixel screen width.
#define REGIONAL_MAP_SCALE (4.0 / 3.0)

typedef struct {
	char wfo[4];
	unsigned short x, y;
} Gridpoint;

typedef struct {
	char name[13]; // shortened to fit the map
	double lat, lon;
	bool has_gridpoint; // regional cities come with one; stations need a lookup
	Gridpoint gridpoint;
	double x, y; // label anchor in the main area
} RegionalSite;

// Regional cities nearest first, then stations filling the gaps, skipping any
// whose label would overlap one already placed. Returns the count.
size_t regional_select_sites(double lat, double lon, const RegionalFrame *frame, RegionalSite out[REGIONAL_MAX_SITES]);

// ws4kp's formatCity: up to the first , / ; \ or -, at most 12 characters.
void regional_city_name(const char *city, char out[13]);

typedef struct {
	int temperature; // degF
	char icon_url[160]; // the night icon is picked from the URL
	bool is_daytime;
	char weekday[4]; // forecast periods only, for the title
} RegionalPeriod;

typedef struct {
	RegionalPeriod periods[REGIONAL_SCREENS];
} RegionalData;

// Whether to ask MapClick for a better reading. `now` is UTC, 0 if unknown.
bool regional_obs_needs_mapclick(const ObsReading *api, time_t now);

// Whether the MapClick reading should replace the API's.
bool regional_obs_prefer_mapclick(const ObsReading *api, const ObsReading *mapclick);

// Fills the observation and the forecast's second and third periods. False if
// the reading lacks a temperature, description or icon, or the forecast has
// fewer than three periods. Expired periods must already be dropped.
bool regional_data(const ObsReading *obs, const Forecast *forecast, RegionalData *out);
