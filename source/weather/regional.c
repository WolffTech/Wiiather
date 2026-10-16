#include "regional.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "latest_obs.h"
#include "stations_gen.h"

// ws4kp's mapOffsetXY: half the map pixels shown, so the location is centered.
#define OFFSET_X 240
#define OFFSET_Y 117

// Label anchors outside this band are dropped (ws4kp's available area).
#define MIN_X 33
#define MAX_X 580
#define MIN_Y 28
#define MAX_Y 282

#define LABEL_WIDTH 105
#define LABEL_HEIGHT 50
#define STATION_LABEL_FACTOR 1.7 // stations need more room, so cities win

// Map pixels: x = (lon - lon0) * lon_scale, y = (lat0 - lat) * lat_scale.
typedef struct {
	double lat0, lat_scale, lon0, lon_scale;
	int width, height;
} Projection;

static const Projection projections[] = {
	[REGIONAL_MAP_CONUS] = { 50.5, 55.2, -127.5, 41.775, 2550, 1600 },
	[REGIONAL_MAP_ALASKA] = { 73.0, 56, -175.0, 25.0, 1200, 1142 },
	[REGIONAL_MAP_HAWAII] = { 25, 55.2, -164.5, 41.775, 600, 571 },
};

static double clamp(double v, double max)
{
	return v > max ? max : v < 0 ? 0 : v;
}

RegionalFrame regional_frame(double lat, double lon, const char *state)
{
	RegionalMap map = strcmp(state, "AK") == 0 ? REGIONAL_MAP_ALASKA
			  : strcmp(state, "HI") == 0 ? REGIONAL_MAP_HAWAII
						     : REGIONAL_MAP_CONUS;
	const Projection *p = &projections[map];
	// Operations follow ws4kp's order so edge cases round the same way.
	double x = clamp((lon - p->lon0) * p->lon_scale - OFFSET_X, p->width - OFFSET_X * 2);
	double y = clamp((p->lat0 - lat) * p->lat_scale - OFFSET_Y, p->height - OFFSET_Y * 2);
	return (RegionalFrame){
		.map = map,
		.origin_x = x,
		.origin_y = y,
		.max_lat = -(y / p->lat_scale - p->lat0),
		.min_lat = -((y + OFFSET_Y * 2) / p->lat_scale - p->lat0),
		.min_lon = -(-x / p->lon_scale - p->lon0),
		.max_lon = -(-(x + OFFSET_X * 2) / p->lon_scale - p->lon0),
	};
}

// ws4kp's getXYForCity. Its Alaska and Hawaii variants are called but their
// results discarded, so every map places labels with these scales.
static bool label_anchor(const RegionalFrame *f, double lat, double lon, double *x, double *y)
{
	if (!(lat > f->min_lat && lat < f->max_lat && lon > f->min_lon && lon < f->max_lon))
		return false;
	*x = (lon - f->min_lon) * 57;
	*y = (f->max_lat - lat) * 70;
	return *y >= MIN_Y && *y <= MAX_Y && *x >= MIN_X && *x <= MAX_X;
}

void regional_city_name(const char *city, char out[13])
{
	size_t len = strcspn(city, ",/;\\-");
	snprintf(out, 13, "%.*s", (int)(len < 12 ? len : 12), city);
}

typedef struct {
	double x1, y1, x2, y2;
} Box;

static bool boxes_overlap(const Box *a, const Box *b)
{
	return !(a->x2 < b->x1 || a->x1 > b->x2 || a->y2 < b->y1 || a->y1 > b->y2);
}

typedef struct {
	const char *city;
	double lat, lon;
	const RegionalCity *regional; // NULL for stations
	double x, y;
} Candidate;

// Places candidates nearest first where their label box, scaled by `factor`,
// clears every placed one. Returns the new site count.
static size_t place_nearest(const Candidate *candidates, size_t n, double lat, double lon, double factor,
	RegionalSite *sites, Box *boxes, size_t count)
{
	static StationDistance ranked[STATION_COUNT];
	for (size_t i = 0; i < n; i++)
		ranked[i] = (StationDistance){ i, latest_obs_distance(candidates[i].lat, candidates[i].lon, lat, lon) };
	latest_obs_sort_by_distance(ranked, n);

	for (size_t i = 0; i < n && count < REGIONAL_MAX_SITES; i++) {
		const Candidate *c = &candidates[ranked[i].index];
		Box box = { c->x, c->y, c->x + LABEL_WIDTH * factor, c->y + LABEL_HEIGHT * factor };
		bool overlaps = false;
		for (size_t j = 0; j < count && !overlaps; j++)
			overlaps = boxes_overlap(&box, &boxes[j]);
		if (overlaps)
			continue;
		boxes[count] = box;
		RegionalSite *s = &sites[count++];
		*s = (RegionalSite){ .lat = c->lat, .lon = c->lon, .x = c->x, .y = c->y };
		regional_city_name(c->city, s->name);
		if (c->regional) {
			s->has_gridpoint = true;
			memcpy(s->gridpoint.wfo, c->regional->wfo, sizeof s->gridpoint.wfo);
			s->gridpoint.x = c->regional->x;
			s->gridpoint.y = c->regional->y;
		}
	}
	return count;
}

_Static_assert(STATION_COUNT >= REGIONAL_CITY_COUNT, "candidate buffers are sized for stations");

size_t regional_select_sites(double lat, double lon, const RegionalFrame *frame, RegionalSite out[REGIONAL_MAX_SITES])
{
	static Candidate candidates[STATION_COUNT];
	Box boxes[REGIONAL_MAX_SITES];
	size_t n = 0, count = 0;
	for (size_t i = 0; i < REGIONAL_CITY_COUNT; i++) {
		const RegionalCity *r = &regional_cities[i];
		Candidate c = { .city = r->name, .lat = r->lat, .lon = r->lon, .regional = r };
		if (label_anchor(frame, c.lat, c.lon, &c.x, &c.y))
			candidates[n++] = c;
	}
	count = place_nearest(candidates, n, lat, lon, 1, out, boxes, count);

	n = 0;
	for (size_t i = 0; i < STATION_COUNT; i++) {
		const StationInfo *s = &station_info[i];
		Candidate c = { .city = s->city, .lat = s->lat, .lon = s->lon };
		if (label_anchor(frame, c.lat, c.lon, &c.x, &c.y))
			candidates[n++] = c;
	}
	return place_nearest(candidates, n, lat, lon, STATION_LABEL_FACTOR, out, boxes, count);
}

// Required: temperature, description and icon.
static int obs_missing(const ObsReading *r)
{
	return !r->temperature.valid + !r->text_description[0] + !r->icon_url[0];
}

bool regional_obs_needs_mapclick(const ObsReading *api, time_t now)
{
	bool stale = now && api->timestamp && now - api->timestamp > OBS_STALE_SECONDS;
	return stale || obs_missing(api) > 0;
}

bool regional_obs_prefer_mapclick(const ObsReading *api, const ObsReading *mapclick)
{
	int a = obs_missing(api), m = obs_missing(mapclick);
	bool fresher = mapclick->timestamp > api->timestamp;
	return m <= a && (fresher || m < a);
}

bool regional_data(const ObsReading *obs, const Forecast *forecast, RegionalData *out)
{
	if (obs_missing(obs) > 0 || forecast->count < REGIONAL_SCREENS)
		return false;
	RegionalPeriod *p = &out->periods[0];
	*p = (RegionalPeriod){
		// JavaScript's Math.round
		.temperature = (int)floor(obs->temperature.value * 9 / 5 + 32 + 0.5),
		.is_daytime = strstr(obs->icon_url, "/day/") != NULL,
	};
	snprintf(p->icon_url, sizeof p->icon_url, "%s", obs->icon_url);
	for (size_t i = 1; i < REGIONAL_SCREENS; i++) {
		const ForecastPeriod *f = &forecast->periods[i];
		p = &out->periods[i];
		// A missing temperature reads as 0, as in ws4kp.
		*p = (RegionalPeriod){ .temperature = f->temperature, .is_daytime = f->is_daytime };
		snprintf(p->icon_url, sizeof p->icon_url, "%s", f->icon_url);
		memcpy(p->weekday, f->weekday, sizeof p->weekday);
	}
	return true;
}
