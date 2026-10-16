#include "weather.h"

#include <math.h>
#include <network.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/mutex.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "net/ca_update.h"
#include "net/geoip.h"
#include "net/https.h"

#define REFRESH_SECONDS (10 * 60)
#define RETRY_SECONDS 60
#define ALERTS_SECONDS 60 // ws4kp refreshes hazards every minute
#define CA_CHECK_SECONDS (24 * 60 * 60)
#define CA_RESCUE_SECONDS (60 * 60) // between unverified fetches after trust failures
#define STACK_SIZE (128 * 1024)

static lwp_t thread;
static mutex_t lock;
static WeatherSnapshot shared;
static char app_dir[256];
// Counts weather_set_location calls (0 means no location yet), so results
// fetched for an earlier location are dropped.
static u32 location_version;

// Locks and returns true if `version` is still the current location;
// otherwise leaves the lock released.
static bool lock_current(u32 version)
{
	LWP_MutexLock(lock);
	if (location_version == version)
		return true;
	LWP_MutexUnlock(lock);
	return false;
}

static void set_status(u32 version, WeatherStatus status, const char *fmt, ...)
{
	if (!lock_current(version))
		return;
	// Keep showing old data while a refresh is in flight or fails.
	if (shared.status != WEATHER_READY)
		shared.status = status;
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(shared.message, sizeof shared.message, fmt, ap);
	va_end(ap);
	LWP_MutexUnlock(lock);
}

static u32 current_version(void)
{
	LWP_MutexLock(lock);
	u32 version = location_version;
	LWP_MutexUnlock(lock);
	return version;
}

static bool bring_up_network(void)
{
	char ip[16] = { 0 };
	set_status(current_version(), WEATHER_LOADING, "Connecting to network...");
	// if_config wraps net_init with DHCP and its own retries.
	s32 ret = if_config(ip, NULL, NULL, true, 20);
	if (ret < 0) {
		set_status(current_version(), WEATHER_FAILED, "Network unavailable (%d). Check Wii Internet settings.", ret);
		return false;
	}
	char err[128];
	set_status(current_version(), WEATHER_LOADING, "Loading certificates...");
	if (!https_init(err, sizeof err)) {
		set_status(current_version(), WEATHER_FAILED, "%s", err);
		return false;
	}
	ca_update_init(app_dir);
	return true;
}

// Call with the lock held.
static void update_utc_offset(void)
{
	time_t utc, rtc;
	if (!https_last_date(&utc, &rtc))
		return;
	// Zones are offset in multiples of 15 minutes, so rounding absorbs request
	// latency and RTC drift.
	shared.utc_offset = lround(difftime(rtc, utc) / 900) * 900;
	shared.has_utc_offset = true;
}

static bool refresh(const Location *loc, u32 version, NwsPoint *point, bool *have_point)
{
	char err[128];
	if (!*have_point) {
		set_status(version, WEATHER_LOADING, "Finding forecast office...");
		if (!nws_get_point(loc->lat, loc->lon, point, err, sizeof err)) {
			set_status(version, WEATHER_FAILED, "%s", err);
			return false;
		}
		*have_point = true;
	}

	set_status(version, WEATHER_LOADING, "Loading current conditions...");
	static Observation obs;
	if (!nws_get_current_observation(point, &obs, err, sizeof err)) {
		set_status(version, WEATHER_FAILED, "%s", err);
		return false;
	}
	if (!lock_current(version))
		return false;
	shared.status = WEATHER_READY;
	shared.message[0] = '\0';
	shared.point = *point;
	shared.observation = obs;
	update_utc_offset();
	shared.generation++;
	LWP_MutexUnlock(lock);

	// The forecast is optional: displays that need it are skipped without it.
	static Forecast forecast;
	if (nws_get_forecast(point, &forecast, err, sizeof err) && lock_current(version)) {
		shared.has_forecast = true;
		shared.forecast = forecast;
		shared.generation++;
		LWP_MutexUnlock(lock);
	}

	// So is the hourly forecast.
	static HourlyForecast hourly;
	if (nws_get_hourly(point, &hourly, err, sizeof err) && lock_current(version)) {
		shared.has_hourly = true;
		shared.hourly = hourly;
		shared.generation++;
		LWP_MutexUnlock(lock);
	}

	// Latest Observations are optional too.
	static LatestObservations latest;
	if (nws_get_latest_observations(loc->lat, loc->lon, &latest) && lock_current(version)) {
		shared.has_latest_observations = true;
		shared.latest_observations = latest;
		shared.generation++;
		LWP_MutexUnlock(lock);
	}

	// And the SPC Outlook.
	SpcOutlook spc;
	if (nws_get_spc_outlook(loc->lat, loc->lon, &spc) && lock_current(version)) {
		shared.has_spc_outlook = true;
		shared.spc_outlook = spc;
		shared.generation++;
		LWP_MutexUnlock(lock);
	}
	return true;
}

// Alerts are optional and need only the location, so failures never block
// other data. A failed fetch keeps the old alerts until they expire, judged by
// the RTC's time since the last server clock reading.
static void refresh_alerts(const Location *loc, u32 version)
{
	static Alerts alerts;
	static time_t server_utc, local_at_server;
	char err[128];
	time_t now = 0;
	bool ok = nws_get_alerts(loc->lat, loc->lon, &alerts, &now, err, sizeof err);
	if (now) {
		server_utc = now;
		local_at_server = time(NULL);
	}
	if (!lock_current(version))
		return;
	Alerts before = shared.alerts;
	if (ok)
		shared.alerts = alerts;
	else if (server_utc)
		alerts_drop_expired(&shared.alerts, server_utc + (time(NULL) - local_at_server));
	shared.alert_fetches++;
	// Every display re-prepares on a new generation, so skip unchanged polls,
	// except when the viewed-alert reset is due.
	if (memcmp(&before, &shared.alerts, sizeof before) != 0 || shared.alert_fetches % ALERTS_VIEWED_FETCHES == 1)
		shared.generation++;
	LWP_MutexUnlock(lock);
}

static void refresh_travel_city(size_t i)
{
	TravelDay day;
	if (!nws_get_travel_day(&travel_cities[i], &day))
		return;
	LWP_MutexLock(lock);
	// Every display re-prepares on a new generation, so skip unchanged cities.
	TravelForecast *t = &shared.travel;
	if (!t->has_day[i] || memcmp(&t->days[i], &day, sizeof day) != 0) {
		t->has_day[i] = true;
		t->days[i] = day;
		shared.generation++;
	}
	LWP_MutexUnlock(lock);
}

// The worker's copies of the Regional Forecast sites, with cached lookups.
static RegionalSource regional_sources[REGIONAL_MAX_SITES];

// Picks the sites once the location's state is known. Returns the count.
static size_t select_regional(const Location *loc, const NwsPoint *point, u32 version)
{
	RegionalFrame frame = regional_frame(loc->lat, loc->lon, point->state);
	static RegionalSite sites[REGIONAL_MAX_SITES];
	size_t count = regional_select_sites(loc->lat, loc->lon, &frame, sites);
	for (size_t i = 0; i < count; i++)
		regional_sources[i] = (RegionalSource){ .site = sites[i] };
	if (!lock_current(version))
		return 0;
	RegionalForecast *r = &shared.regional;
	memset(r, 0, sizeof *r);
	r->frame = frame;
	r->count = count;
	memcpy(r->sites, sites, count * sizeof sites[0]);
	LWP_MutexUnlock(lock);
	return count;
}

static void refresh_regional_site(u32 version, size_t i)
{
	RegionalData data;
	if (!nws_get_regional(&regional_sources[i], &data) || !lock_current(version))
		return;
	// Every display re-prepares on a new generation, so skip unchanged sites.
	RegionalForecast *r = &shared.regional;
	if (!r->has_data[i] || memcmp(&r->data[i], &data, sizeof data) != 0) {
		r->has_data[i] = true;
		r->data[i] = data;
		shared.generation++;
	}
	LWP_MutexUnlock(lock);
}

static void guess_zip(void)
{
	LWP_MutexLock(lock);
	bool needed = shared.zip_guess.status == GUESS_PENDING;
	LWP_MutexUnlock(lock);
	if (!needed)
		return;
	u32 zip;
	ZipGuess guess = geoip_zip(&zip) ? (ZipGuess){ GUESS_FOUND, zip } : (ZipGuess){ GUESS_FAILED, 0 };
	LWP_MutexLock(lock);
	shared.zip_guess = guess;
	shared.generation++;
	LWP_MutexUnlock(lock);
}

static void *worker(void *arg)
{
	while (!bring_up_network())
		sleep(RETRY_SECONDS);
	guess_zip();

	NwsPoint point;
	bool have_point = false;
	u32 seen_version = 0;
	u64 next_alerts = 0, next_refresh = 0, next_ca_rescue = 0, next_travel = 0;
	size_t travel_city = TRAVEL_CITY_COUNT; // the next to fetch, or TRAVEL_CITY_COUNT between rounds
	// Regional sites for `regional_version`, fetched in rounds like the travel cities.
	u32 regional_version = 0;
	size_t regional_count = 0, regional_site = 0;
	u64 next_regional = 0;
	// Let the weather load before the first CA check.
	u64 next_ca_check = gettime() + secs_to_ticks(RETRY_SECONDS);
	// Poll every second so a new location loads promptly.
	for (;; sleep(1)) {
		u64 now = gettime();
		if (https_take_trust_failure() && now >= next_ca_rescue) {
			// The CA list may be too old to verify the servers. Fetch the
			// signed list without TLS verification, and retry right away if
			// it was newer.
			next_ca_rescue = now + secs_to_ticks(CA_RESCUE_SECONDS);
			if (ca_update_check(true))
				next_alerts = next_refresh = now;
		} else if (now >= next_ca_check) {
			ca_update_check(false);
			next_ca_check = gettime() + secs_to_ticks(CA_CHECK_SECONDS);
		}

		LWP_MutexLock(lock);
		u32 version = location_version;
		Location loc = shared.location;
		LWP_MutexUnlock(lock);
		if (version == 0)
			continue;
		if (version != seen_version) {
			seen_version = version;
			have_point = false;
			regional_count = regional_site = 0;
			next_alerts = next_refresh = gettime();
		}
		// Alerts first so the Hazards display can lead the first rotation.
		if (gettime() >= next_alerts) {
			refresh_alerts(&loc, version);
			next_alerts = gettime() + secs_to_ticks(ALERTS_SECONDS);
		}
		if (gettime() >= next_refresh) {
			bool ok = refresh(&loc, version, &point, &have_point);
			next_refresh = gettime() + secs_to_ticks(ok ? REFRESH_SECONDS : RETRY_SECONDS);
		}
		// Travel cities refresh as often as the rest, but one per pass so
		// alerts and location changes aren't held up behind them.
		if (travel_city == TRAVEL_CITY_COUNT && gettime() >= next_travel) {
			travel_city = 0;
			next_travel = gettime() + secs_to_ticks(REFRESH_SECONDS);
		}
		if (travel_city < TRAVEL_CITY_COUNT)
			refresh_travel_city(travel_city++);

		// Regional sites likewise, once the point gives the location's state.
		if (have_point && regional_version != version) {
			regional_version = version;
			regional_count = select_regional(&loc, &point, version);
			regional_site = 0;
			next_regional = gettime() + secs_to_ticks(REFRESH_SECONDS);
		}
		if (regional_site == regional_count && gettime() >= next_regional) {
			regional_site = 0;
			next_regional = gettime() + secs_to_ticks(REFRESH_SECONDS);
		}
		if (regional_site < regional_count)
			refresh_regional_site(regional_version, regional_site++);
	}
	return NULL;
}

void weather_start(const Location *location, const char *dir)
{
	snprintf(app_dir, sizeof app_dir, "%s", dir);
	LWP_MutexInit(&lock, false);
	shared.status = WEATHER_LOADING;
	snprintf(shared.message, sizeof shared.message, "Starting...");
	if (location) {
		shared.location = *location;
		location_version = 1;
	} else {
		shared.zip_guess.status = GUESS_PENDING;
	}
	LWP_CreateThread(&thread, worker, NULL, NULL, STACK_SIZE, 48);
}

void weather_set_location(const Location *location)
{
	LWP_MutexLock(lock);
	location_version++;
	// The clock reading, guess and travel cities don't depend on the location.
	WeatherSnapshot *s = &shared;
	bool has_utc_offset = s->has_utc_offset;
	long utc_offset = s->utc_offset;
	ZipGuess zip_guess = s->zip_guess;
	u32 generation = s->generation;
	TravelForecast travel = s->travel;
	memset(s, 0, sizeof *s);
	s->status = WEATHER_LOADING;
	snprintf(s->message, sizeof s->message, "Loading...");
	s->location = *location;
	s->has_utc_offset = has_utc_offset;
	s->utc_offset = utc_offset;
	s->zip_guess = zip_guess;
	s->travel = travel;
	s->generation = generation + 1;
	LWP_MutexUnlock(lock);
}

void weather_snapshot(WeatherSnapshot *out)
{
	LWP_MutexLock(lock);
	*out = shared;
	LWP_MutexUnlock(lock);
}
