#pragma once
#include <gctypes.h>

#include "location.h"
#include "nws.h"

// Status of the current observation, which every display depends on.
typedef enum {
	WEATHER_LOADING, // no data yet; `message` describes progress
	WEATHER_READY,
	WEATHER_FAILED, // no data; `message` holds the error, retry is scheduled
} WeatherStatus;

// The ZIP code guessed from the IP address when the app starts without a
// location.
typedef enum { GUESS_NONE, GUESS_PENDING, GUESS_FOUND, GUESS_FAILED } GuessStatus;

typedef struct {
	GuessStatus status;
	u32 zip; // valid when status is GUESS_FOUND
} ZipGuess;

typedef struct {
	WeatherStatus status;
	char message[128];
	NwsPoint point;
	Location location; // zeroed until one is set
	bool has_utc_offset;
	long utc_offset; // RTC (local time) minus UTC in seconds, a multiple of 15 minutes
	Observation observation;
	bool has_forecast; // kept from an earlier refresh if a later one fails
	Forecast forecast;
	bool has_latest_observations; // kept from an earlier refresh if a later one fails
	LatestObservations latest_observations;
	Alerts alerts; // kept from earlier fetches if later ones fail, minus expired ones
	u32 alert_fetches; // attempts so far; ws4kp re-shows viewed alerts every tenth
	ZipGuess zip_guess;
	u32 generation; // increments whenever any data changes
} WeatherSnapshot;

// Starts the background thread that brings up the network and refreshes
// weather for the location periodically. Without a location, it guesses a
// ZIP code from the IP address instead and waits for weather_set_location.
// `dir` is the app's folder, which holds the downloaded CA bundle.
void weather_start(const Location *location, const char *dir);

// Switches to a new location, dropping the old location's data.
void weather_set_location(const Location *location);

// Copies the latest state. Safe to call every frame.
void weather_snapshot(WeatherSnapshot *out);
