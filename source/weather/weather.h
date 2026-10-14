#pragma once
#include <gctypes.h>

#include "nws.h"

// Status of the current observation, which every display depends on.
typedef enum {
	WEATHER_LOADING, // no data yet; `message` describes progress
	WEATHER_READY,
	WEATHER_FAILED, // no data; `message` holds the error, retry is scheduled
} WeatherStatus;

typedef struct {
	WeatherStatus status;
	char message[128];
	NwsPoint point;
	Observation observation;
	bool has_forecast; // kept from an earlier refresh if a later one fails
	Forecast forecast;
	bool has_latest_observations; // kept from an earlier refresh if a later one fails
	LatestObservations latest_observations;
	u32 generation; // increments whenever any data changes
} WeatherSnapshot;

// Starts the background thread that brings up the network and refreshes
// weather for the location periodically.
void weather_start(double lat, double lon);

// Copies the latest state. Safe to call every frame.
void weather_snapshot(WeatherSnapshot *out);
