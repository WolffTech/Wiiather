// ws4kp's Almanac data (almanac.mjs) and the parts of SunCalc it uses
// (vendor/auto/suncalc.js). Pure logic, testable on the host.
//
// Instants are JavaScript-style milliseconds since the Unix epoch in UTC, so
// results match SunCalc's to the millisecond.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// SunCalc.getTimes() sunrise and sunset for the solar day nearest `ms` at the
// location. Returns false when the sun doesn't cross the horizon that day.
bool suncalc_sun_times(int64_t ms, double lat, double lon, int64_t *sunrise, int64_t *sunset);

// SunCalc.getMoonIllumination().phase: 0 new, 0.25 first quarter, 0.5 full,
// 0.75 last quarter.
double suncalc_moon_phase(int64_t ms);

typedef enum { MOON_NEW, MOON_FIRST, MOON_FULL, MOON_LAST } MoonPhase;

typedef struct {
	MoonPhase phase;
	int64_t ms; // first millisecond at or past the phase
} MoonEvent;

// The first `max` quarter phases after `now_ms` minus a day, found like ws4kp:
// a daily scan from yesterday, then stepping by hours, minutes, seconds and
// milliseconds to the crossing. Scans at most 46 days.
size_t almanac_moon_events(int64_t now_ms, MoonEvent *out, size_t max);

// ws4kp's standard layout shows two days and four phases; widescreen adds one
// of each.
#define ALMANAC_DAYS 2
#define ALMANAC_MOON_EVENTS 4

typedef struct {
	char weekday[10]; // "Wednesday"
	char sunrise[16]; // "\xa07:17 AM" (NBSP-padded), "10:12 AM", or "-"
	char sunset[16];
} AlmanacDay;

typedef struct {
	MoonPhase phase;
	char date[8]; // "Oct 18"
} AlmanacMoon;

typedef struct {
	AlmanacDay days[ALMANAC_DAYS];
	size_t moon_count;
	AlmanacMoon moons[ALMANAC_MOON_EVENTS];
} AlmanacText;

// `local_now` is local wall-clock time counted as if it were UTC (the Wii
// RTC), and `utc_offset` is local minus UTC in seconds. Days are the location's
// local dates; every time is shown with that one offset.
void almanac_format(double lat, double lon, time_t local_now, long utc_offset, AlmanacText *out);

// "New", "First", "Full" or "Last".
const char *almanac_phase_name(MoonPhase phase);
