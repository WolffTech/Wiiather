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
	double lat, lon; // the configured location
	bool has_utc_offset;
	long utc_offset; // RTC (local time) minus UTC in seconds, a multiple of 15 minutes
	Observation observation;
	bool has_forecast; // kept from an earlier refresh if a later one fails
	Forecast forecast;
	bool has_latest_observations; // kept from an earlier refresh if a later one fails
	LatestObservations latest_observations;
	Alerts alerts; // kept from earlier fetches if later ones fail, minus expired ones
	u32 alert_fetches; // attempts so far; ws4kp re-shows viewed alerts every tenth
	u32 generation; // increments whenever any data changes
} WeatherSnapshot;

// Starts the background thread that brings up the network and refreshes
// weather for the location periodically.
void weather_start(double lat, double lon);

// Copies the latest state. Safe to call every frame.
void weather_snapshot(WeatherSnapshot *out);
