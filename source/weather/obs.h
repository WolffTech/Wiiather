// Observation selection ported from ws4kp's currentweather.mjs, metar.mjs and
// mapclick.mjs. Pure logic with no I/O, so it can be tested on the host.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

// An NWS quantity; NWS reports missing readings as null.
typedef struct {
	bool valid;
	double value;
} Measure;

// One observation's properties in the API's SI units (degC, km/h, Pa, m, %).
typedef struct {
	time_t timestamp; // UTC, 0 if unknown
	char text_description[64];
	char icon_url[160];
	Measure temperature;
	Measure dewpoint;
	Measure wind_direction;
	Measure wind_speed;
	Measure wind_gust;
	Measure pressure;
	Measure visibility;
	Measure relative_humidity;
	Measure heat_index;
	Measure wind_chill;
	bool has_cloud_layer;
	Measure ceiling; // base of the first cloud layer
} ObsReading;

// The merged observation the display formats.
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
	Measure ceiling;
	bool stale; // older than OBS_STALE_SECONDS; ws4kp titles it "Recent"
} Observation;

// An hourly report plus ingest delay.
#define OBS_STALE_SECONDS (80 * 60)

// Fills fields the API left null from the raw METAR report.
void obs_augment_with_metar(ObsReading *r, const char *raw_message);

typedef struct {
	int missing_required; // temperature, description
	int missing_optional; // wind, dewpoint, pressure, visibility, humidity, ceiling
} ObsQuality;

ObsQuality obs_quality(const ObsReading *r);

// ws4kp accepts a station when nothing required is missing and at most one
// optional field is.
bool obs_quality_ok(ObsQuality q);

// Whether to ask MapClick for a better reading. `now` is UTC, 0 if unknown.
bool obs_needs_mapclick(const ObsReading *api, time_t now);

// Whether the MapClick reading should replace the API's latest one.
bool obs_prefer_mapclick(const ObsReading *api, const ObsReading *mapclick);

// Raw strings from MapClick's "currentobservation" object; NULL when absent.
typedef struct {
	const char *date; // "14 Oct 11:52 am EDT"
	const char *temp, *dewp, *relh; // degF, degF, %
	const char *winds, *windd, *gust; // mph, degrees, mph
	const char *weather, *weatherimage;
	const char *visibility, *slp; // miles, inHg
	const char *windchill; // degF
} MapClickObservation;

// Converts to API units. The timestamp falls back to `now` when the date
// can't be parsed, as in ws4kp.
void obs_from_mapclick(const MapClickObservation *m, time_t now, ObsReading *out);

// Parses MapClick's local date in a US zone. `year` supplies the missing year.
bool obs_parse_mapclick_date(const char *s, int year, time_t *out);

// Merges readings sorted newest first, backfilling missing values from older
// ones. Leaves the station fields empty.
void obs_merge(const ObsReading *readings, size_t count, time_t now, Observation *out);
