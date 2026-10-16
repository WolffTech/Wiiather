// Latest Observations station selection and formatting ported from ws4kp's
// latestobservations.mjs. Pure logic with no I/O, so it can be tested on the host.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "obs.h"
#include "units.h"

#define LATEST_OBS_STATIONS 7    // ws4kp's MaximumRegionalStations
#define LATEST_OBS_CANDIDATES 30 // nearest stations tried before giving up

// ws4kp ranks stations by planar distance in degrees.
typedef struct {
	size_t index;
	double distance;
} StationDistance;

double latest_obs_distance(double lat1, double lon1, double lat2, double lon2);

// Sorts nearest first; ties keep index order, like JavaScript's stable sort.
void latest_obs_sort_by_distance(StationDistance *stations, size_t count);

// Required fields missing from a reading: temperature, wind speed, wind
// direction and description. A station is shown only when none are.
int latest_obs_missing(const ObsReading *r);

// Whether to ask MapClick for a better reading. `now` is UTC, 0 if unknown.
bool latest_obs_needs_mapclick(const ObsReading *api, time_t now);

// Whether the MapClick reading should replace the API's.
bool latest_obs_prefer_mapclick(const ObsReading *api, const ObsReading *mapclick);

// An accepted station: every field the display needs is present.
typedef struct {
	char city[40]; // ws4kp's curated station city
	char text_description[64];
	double temperature;    // degC
	double wind_speed;     // km/h
	double wind_direction; // degrees
} LatestStation;

typedef struct {
	size_t count;
	LatestStation stations[LATEST_OBS_STATIONS]; // nearest first
} LatestObservations;

// Fills `out` and returns true when the reading has every required field.
bool latest_obs_accept(const ObsReading *r, const char *city, LatestStation *out);

// One display row. Location and weather are collapsed the way
// HTML renders them; wind keeps its padding (white-space: pre).
typedef struct {
	char location[16];
	char temperature[8];
	char weather[16];
	char wind[24];
} LatestObsRow;

// Formats every station, nearest first. Returns the row count.
size_t latest_obs_format(const LatestObservations *obs, Units units, LatestObsRow *rows);
