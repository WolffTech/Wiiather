#pragma once
#include <gctypes.h>
#include <time.h>

#include "location.h"
#include "nws.h"
#include "radar.h"

// Status of the current observation, which every display depends on.
typedef enum {
	WEATHER_LOADING, // no data yet; `message` describes progress
	WEATHER_READY,
	WEATHER_FAILED, // no data; `message` holds the error, retry is scheduled
} WeatherStatus;

// The ZIP code guessed from the IP address, when the app starts without a
// location or the location picker asks for one.
typedef enum { GUESS_NONE, GUESS_PENDING, GUESS_FOUND, GUESS_FAILED } GuessStatus;

typedef struct {
	GuessStatus status;
	u32 zip; // valid when status is GUESS_FOUND
} ZipGuess;

// Travel Forecast cities, each kept from an earlier fetch if a later one fails.
typedef struct {
	bool has_day[TRAVEL_CITY_COUNT];
	TravelDay days[TRAVEL_CITY_COUNT];
} TravelForecast;

// Regional Forecast sites around the location, each kept from an earlier
// fetch if a later one fails.
typedef struct {
	RegionalFrame frame;
	size_t count; // 0 until the location's state is known
	RegionalSite sites[REGIONAL_MAX_SITES];
	bool has_data[REGIONAL_MAX_SITES];
	RegionalData data[REGIONAL_MAX_SITES];
} RegionalForecast;

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
	bool has_hourly; // kept from an earlier refresh if a later one fails
	HourlyForecast hourly;
	bool has_latest_observations; // kept from an earlier refresh if a later one fails
	LatestObservations latest_observations;
	bool has_spc_outlook; // kept from an earlier refresh if a later one fails
	SpcOutlook spc_outlook;
	TravelForecast travel; // doesn't depend on the location, so kept when it changes
	RegionalForecast regional;
	Alerts alerts; // kept from earlier fetches if later ones fail, minus expired ones
	u32 alert_fetches; // attempts so far; ws4kp re-shows viewed alerts every tenth
	ZipGuess zip_guess;
	time_t updated; // RTC time of the last current conditions, 0 before any
	u64 ca_date; // the CA bundle in use as YYYYMMDDhhmmss, 0 until the network is up
	u32 generation; // increments whenever any data changes
} WeatherSnapshot;

// Starts the background thread that brings up the network and refreshes
// weather for the location periodically. Without a location, it guesses a
// ZIP code from the IP address instead and waits for weather_set_location.
// `dir` is the app's folder, which holds the downloaded CA bundle.
void weather_start(const Location *location, const char *dir);

// Switches to a new location, dropping the old location's data.
void weather_set_location(const Location *location);

// Starts guessing the ZIP code from the IP address unless a guess is under way
// or has succeeded.
void weather_request_zip_guess(void);

// Copies the latest state. Safe to call every frame.
void weather_snapshot(WeatherSnapshot *out);

// Copies the Local Radar's images for the current location if they changed
// since `*generation`, and updates it. They're too large to copy with every
// snapshot.
bool weather_radar(u32 *generation, RadarImages *out);
