#include "sun_moon.h"

#include <math.h>
#include <stdio.h>

// SunCalc's formulas, kept in its operation order so doubles round the same.

#define PI 3.14159265358979323846
#define RAD (PI / 180)
#define DAY_MS 86400000.0
#define J1970 2440588.0
#define J2000 2451545.0
#define J0 0.0009
#define OBLIQUITY (RAD * 23.4397)

static double to_days(int64_t ms)
{
	return (double)ms / DAY_MS - 0.5 + J1970 - J2000;
}

// new Date() truncates toward zero.
static int64_t from_julian(double j)
{
	return (int64_t)((j + 0.5 - J1970) * DAY_MS);
}

static double right_ascension(double l, double b)
{
	return atan2(sin(l) * cos(OBLIQUITY) - tan(b) * sin(OBLIQUITY), cos(l));
}

static double declination(double l, double b)
{
	return asin(sin(b) * cos(OBLIQUITY) + cos(b) * sin(OBLIQUITY) * sin(l));
}

static double solar_mean_anomaly(double d)
{
	return RAD * (357.5291 + 0.98560028 * d);
}

static double ecliptic_longitude(double m)
{
	double c = RAD * (1.9148 * sin(m) + 0.02 * sin(2 * m) + 0.0003 * sin(3 * m)); // equation of center
	double p = RAD * 102.9372; // perihelion of the Earth
	return m + c + p + PI;
}

static double approx_transit(double ht, double lw, double n)
{
	return J0 + (ht + lw) / (2 * PI) + n;
}

static double solar_transit_j(double ds, double m, double l)
{
	return J2000 + ds + 0.0053 * sin(m) - 0.0069 * sin(2 * l);
}

bool suncalc_sun_times(int64_t ms, double lat, double lon, int64_t *sunrise, int64_t *sunset)
{
	double lw = RAD * -lon;
	double phi = RAD * lat;
	double d = to_days(ms);
	double n = floor(d - J0 - lw / (2 * PI) + 0.5); // JS Math.round
	double ds = approx_transit(0, lw, n);
	double m = solar_mean_anomaly(ds);
	double l = ecliptic_longitude(m);
	double dec = declination(l, 0);
	double j_noon = solar_transit_j(ds, m, l);

	double h0 = -0.833 * RAD;
	double w = acos((sin(h0) - sin(phi) * sin(dec)) / (cos(phi) * cos(dec)));
	if (isnan(w))
		return false;
	double j_set = solar_transit_j(approx_transit(w, lw, n), m, l);
	double j_rise = j_noon - (j_set - j_noon);
	*sunrise = from_julian(j_rise);
	*sunset = from_julian(j_set);
	return true;
}

double suncalc_moon_phase(int64_t ms)
{
	double d = to_days(ms);

	double sm = solar_mean_anomaly(d);
	double sl = ecliptic_longitude(sm);
	double s_dec = declination(sl, 0);
	double s_ra = right_ascension(sl, 0);

	double ml = RAD * (218.316 + 13.176396 * d); // ecliptic longitude
	double mm = RAD * (134.963 + 13.064993 * d); // mean anomaly
	double mf = RAD * (93.272 + 13.229350 * d); // mean distance
	double l = ml + RAD * 6.289 * sin(mm);
	double b = RAD * 5.128 * sin(mf);
	double m_dist = 385001 - 20905 * cos(mm);
	double m_ra = right_ascension(l, b);
	double m_dec = declination(l, b);

	double sdist = 149598000; // km from Earth to Sun
	double phi = acos(sin(s_dec) * sin(m_dec) + cos(s_dec) * cos(m_dec) * cos(s_ra - m_ra));
	double inc = atan2(sdist * sin(phi), m_dist - sdist * cos(phi));
	double angle = atan2(cos(s_dec) * sin(s_ra - m_ra),
		sin(s_dec) * cos(m_dec) - cos(s_dec) * sin(m_dec) * cos(s_ra - m_ra));
	return 0.5 + 0.5 * inc * (angle < 0 ? -1 : 1) / PI;
}

// ws4kp wraps phases above 0.9 so the new moon is a crossing of 0.
static double wrapped_phase(int64_t ms)
{
	double phase = suncalc_moon_phase(ms);
	return phase > 0.9 ? phase - 1.0 : phase;
}

// ws4kp's getMoonTransition: from a time just past the crossing, step back by
// hours until before it, forward by minutes until past it, and so on down to
// milliseconds. ws4kp skips the wrap for each pass's starting phase, which can
// derail new moons by up to 1000 steps; this wraps it.
static int64_t refine_crossing(double threshold, int64_t ms)
{
	static const int64_t steps[] = { -3600000, 60000, -1000, 1 };
	for (size_t i = 0; i < sizeof steps / sizeof steps[0]; i++) {
		int64_t step = steps[i];
		double phase = wrapped_phase(ms);
		bool crossed = false;
		for (int n = 0; n < 1000 && !crossed; n++) {
			double last = phase;
			ms += step;
			phase = wrapped_phase(ms);
			crossed = step < 0 ? last > threshold && phase <= threshold : last < threshold && phase >= threshold;
		}
		if (!crossed)
			break; // ws4kp gives up where it stopped
	}
	return ms;
}

size_t almanac_moon_events(int64_t now_ms, MoonEvent *out, size_t max)
{
	static const struct {
		MoonPhase phase;
		double threshold;
	} quarters[] = { { MOON_FIRST, 0.25 }, { MOON_FULL, 0.5 }, { MOON_LAST, 0.75 } };
	const int64_t day = 86400000;

	int64_t ms = now_ms - day;
	double phase = suncalc_moon_phase(ms);
	size_t count = 0;
	for (int i = 0; i <= 45 && count < max; i++) {
		double last = phase;
		ms += day;
		phase = suncalc_moon_phase(ms);
		for (size_t q = 0; q < sizeof quarters / sizeof quarters[0] && count < max; q++)
			if (last < quarters[q].threshold && phase >= quarters[q].threshold)
				out[count++] = (MoonEvent){ quarters[q].phase, refine_crossing(quarters[q].threshold, ms) };
		if (last > phase && count < max)
			out[count++] = (MoonEvent){ MOON_NEW, refine_crossing(0, ms) };
	}
	return count;
}

static void local_tm(int64_t ms, long utc_offset, struct tm *out)
{
	time_t t = (time_t)(ms / 1000) + utc_offset;
	gmtime_r(&t, out);
}

// Luxon's "h:mm a", with single-digit hours padded by a no-break space.
static void format_time(int64_t ms, long utc_offset, char *out, size_t out_len)
{
	struct tm tm;
	local_tm(ms, utc_offset, &tm);
	int hour = tm.tm_hour % 12 ? tm.tm_hour % 12 : 12;
	snprintf(out, out_len, "%s%d:%02d %s", hour < 10 ? "\xa0" : "", hour, tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
}

void almanac_format(double lat, double lon, time_t local_now, long utc_offset, AlmanacText *out)
{
	static const char *const weekdays[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
		"Saturday" };
	static const char *const months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
		"Nov", "Dec" };

	int64_t today = (int64_t)floor(local_now / 86400.0);
	for (int i = 0; i < ALMANAC_DAYS; i++) {
		AlmanacDay *day = &out->days[i];
		int64_t date = today + i;
		snprintf(day->weekday, sizeof day->weekday, "%s", weekdays[(date % 7 + 11) % 7]); // 1970-01-01 was a Thursday
		// ws4kp asks for the solar day nearest the current time, which shortly
		// after midnight can still be yesterday's; local noon always gives the
		// local date's.
		int64_t noon_ms = (date * 86400 + 12 * 3600 - utc_offset) * 1000;
		int64_t rise, set;
		if (suncalc_sun_times(noon_ms, lat, lon, &rise, &set)) {
			format_time(rise, utc_offset, day->sunrise, sizeof day->sunrise);
			format_time(set, utc_offset, day->sunset, sizeof day->sunset);
		} else {
			snprintf(day->sunrise, sizeof day->sunrise, "-");
			snprintf(day->sunset, sizeof day->sunset, "-");
		}
	}

	MoonEvent events[ALMANAC_MOON_EVENTS];
	out->moon_count = almanac_moon_events(((int64_t)local_now - utc_offset) * 1000, events, ALMANAC_MOON_EVENTS);
	for (size_t i = 0; i < out->moon_count; i++) {
		struct tm tm;
		local_tm(events[i].ms, utc_offset, &tm);
		out->moons[i].phase = events[i].phase;
		snprintf(out->moons[i].date, sizeof out->moons[i].date, "%s %d", months[tm.tm_mon], tm.tm_mday);
	}
}

const char *almanac_phase_name(MoonPhase phase)
{
	switch (phase) {
	case MOON_NEW:
		return "New";
	case MOON_FIRST:
		return "First";
	case MOON_FULL:
		return "Full";
	case MOON_LAST:
		return "Last";
	}
	return "";
}
