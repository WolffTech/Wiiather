#include "utc.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

long utc_days_from_civil(int year, int month, int day)
{
	// Howard Hinnant's days_from_civil.
	year -= month <= 2;
	long era = (year >= 0 ? year : year - 399) / 400;
	long yoe = year - era * 400;
	long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}

static time_t to_epoch(int year, int month, int day, int hour, int min, int sec)
{
	return (time_t)utc_days_from_civil(year, month, day) * 86400 + hour * 3600 + min * 60 + sec;
}

int utc_month_from_abbrev(const char *s)
{
	static const char *const months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
		"Nov", "Dec" };
	for (int i = 0; i < 12; i++)
		if (strncasecmp(s, months[i], 3) == 0)
			return i + 1;
	return 0;
}

bool utc_parse_http_date(const char *s, time_t *out)
{
	int day, year, hour, min, sec;
	char month[4];
	if (sscanf(s, "%*3s, %d %3s %d %d:%d:%d", &day, month, &year, &hour, &min, &sec) != 6)
		return false;
	int m = utc_month_from_abbrev(month);
	if (!m)
		return false;
	*out = to_epoch(year, m, day, hour, min, sec);
	return true;
}

bool utc_parse_iso8601(const char *s, time_t *out)
{
	int year, month, day, hour, min, sec, used = 0;
	if (sscanf(s, "%d-%d-%dT%d:%d:%d%n", &year, &month, &day, &hour, &min, &sec, &used) != 6)
		return false;
	const char *zone = s + used;
	if (*zone == '.') // fractional seconds
		zone += 1 + strspn(zone + 1, "0123456789");
	int offset = 0;
	if (*zone == '+' || *zone == '-') {
		int oh, om;
		if (sscanf(zone + 1, "%2d:%2d", &oh, &om) != 2)
			return false;
		offset = (oh * 60 + om) * 60 * (*zone == '-' ? -1 : 1);
	} else if (*zone != 'Z') {
		return false;
	}
	*out = to_epoch(year, month, day, hour, min, sec) - offset;
	return true;
}
