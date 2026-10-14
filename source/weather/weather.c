#include "weather.h"

#include <math.h>
#include <network.h>
#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "net/https.h"

#define REFRESH_SECONDS (10 * 60)
#define RETRY_SECONDS 60
#define ALERTS_SECONDS 60 // ws4kp refreshes hazards every minute
#define STACK_SIZE (128 * 1024)

static lwp_t thread;
static mutex_t lock;
static WeatherSnapshot shared;
static double location_lat, location_lon;

static void set_status(WeatherStatus status, const char *fmt, ...)
{
	LWP_MutexLock(lock);
	// Keep showing old data while a refresh is in flight or fails.
	if (shared.status != WEATHER_READY)
		shared.status = status;
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(shared.message, sizeof shared.message, fmt, ap);
	va_end(ap);
	LWP_MutexUnlock(lock);
}

static bool bring_up_network(void)
{
	char ip[16] = { 0 };
	set_status(WEATHER_LOADING, "Connecting to network...");
	// if_config wraps net_init with DHCP and its own retries.
	s32 ret = if_config(ip, NULL, NULL, true, 20);
	if (ret < 0) {
		set_status(WEATHER_FAILED, "Network unavailable (%d). Check Wii Internet settings.", ret);
		return false;
	}
	char err[128];
	set_status(WEATHER_LOADING, "Loading certificates...");
	if (!https_init(err, sizeof err)) {
		set_status(WEATHER_FAILED, "%s", err);
		return false;
	}
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

static bool refresh(NwsPoint *point, bool *have_point)
{
	char err[128];
	if (!*have_point) {
		set_status(WEATHER_LOADING, "Finding forecast office...");
		if (!nws_get_point(location_lat, location_lon, point, err, sizeof err)) {
			set_status(WEATHER_FAILED, "%s", err);
			return false;
		}
		*have_point = true;
	}

	set_status(WEATHER_LOADING, "Loading current conditions...");
	static Observation obs;
	if (!nws_get_current_observation(point, &obs, err, sizeof err)) {
		set_status(WEATHER_FAILED, "%s", err);
		return false;
	}
	LWP_MutexLock(lock);
	shared.status = WEATHER_READY;
	shared.message[0] = '\0';
	shared.point = *point;
	shared.observation = obs;
	update_utc_offset();
	shared.generation++;
	LWP_MutexUnlock(lock);

	// The forecast is optional: displays that need it are skipped without it.
	static Forecast forecast;
	if (nws_get_forecast(point, &forecast, err, sizeof err)) {
		LWP_MutexLock(lock);
		shared.has_forecast = true;
		shared.forecast = forecast;
		shared.generation++;
		LWP_MutexUnlock(lock);
	}

	// Latest Observations are optional too.
	static LatestObservations latest;
	if (nws_get_latest_observations(location_lat, location_lon, &latest)) {
		LWP_MutexLock(lock);
		shared.has_latest_observations = true;
		shared.latest_observations = latest;
		shared.generation++;
		LWP_MutexUnlock(lock);
	}
	return true;
}

// Alerts are optional and need only the location, so failures never block
// other data. A failed fetch keeps the old alerts until they expire, judged by
// the RTC's time since the last server clock reading.
static void refresh_alerts(void)
{
	static Alerts alerts;
	static time_t server_utc, local_at_server;
	char err[128];
	time_t now = 0;
	bool ok = nws_get_alerts(location_lat, location_lon, &alerts, &now, err, sizeof err);
	if (now) {
		server_utc = now;
		local_at_server = time(NULL);
	}
	LWP_MutexLock(lock);
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

static void *worker(void *arg)
{
	while (!bring_up_network())
		sleep(RETRY_SECONDS);

	NwsPoint point;
	bool have_point = false;
	int until_refresh = 0;
	for (;;) {
		// Alerts first so the Hazards display can lead the first rotation.
		refresh_alerts();
		if (until_refresh <= 0)
			until_refresh = refresh(&point, &have_point) ? REFRESH_SECONDS : RETRY_SECONDS;
		sleep(ALERTS_SECONDS);
		until_refresh -= ALERTS_SECONDS;
	}
	return NULL;
}

void weather_start(double lat, double lon)
{
	location_lat = lat;
	location_lon = lon;
	shared.status = WEATHER_LOADING;
	shared.lat = lat;
	shared.lon = lon;
	snprintf(shared.message, sizeof shared.message, "Starting...");
	LWP_MutexInit(&lock, false);
	LWP_CreateThread(&thread, worker, NULL, NULL, STACK_SIZE, 48);
}

void weather_snapshot(WeatherSnapshot *out)
{
	LWP_MutexLock(lock);
	*out = shared;
	LWP_MutexUnlock(lock);
}
