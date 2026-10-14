#include "weather.h"

#include <network.h>
#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "net/https.h"

#define REFRESH_SECONDS (10 * 60)
#define RETRY_SECONDS 60
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
	Observation obs;
	if (!nws_get_current_observation(point, &obs, err, sizeof err)) {
		set_status(WEATHER_FAILED, "%s", err);
		return false;
	}

	LWP_MutexLock(lock);
	shared.status = WEATHER_READY;
	shared.message[0] = '\0';
	shared.point = *point;
	shared.observation = obs;
	shared.generation++;
	LWP_MutexUnlock(lock);
	return true;
}

static void *worker(void *arg)
{
	while (!bring_up_network())
		sleep(RETRY_SECONDS);

	NwsPoint point;
	bool have_point = false;
	for (;;) {
		bool ok = refresh(&point, &have_point);
		sleep(ok ? REFRESH_SECONDS : RETRY_SECONDS);
	}
	return NULL;
}

void weather_start(double lat, double lon)
{
	location_lat = lat;
	location_lon = lon;
	shared.status = WEATHER_LOADING;
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
