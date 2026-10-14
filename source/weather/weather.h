#pragma once
#include <gctypes.h>

#include "nws.h"

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
	u32 generation; // increments on each successful refresh
} WeatherSnapshot;

// Starts the background thread that brings up the network and refreshes
// weather for the location periodically.
void weather_start(double lat, double lon);

// Copies the latest state. Safe to call every frame.
void weather_snapshot(WeatherSnapshot *out);
