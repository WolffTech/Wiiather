#pragma once
#include <stdbool.h>
#include <time.h>

// The Wii RTC holds local wall-clock time with no zone, so absolute times come
// from server timestamps instead. These parsers return seconds since the Unix
// epoch in UTC without consulting the C library's time zone.

// Days since 1970-01-01 for a proleptic Gregorian date (month 1-12).
long utc_days_from_civil(int year, int month, int day);

// RFC 9110 IMF-fixdate, as in the HTTP Date header: "Wed, 14 Oct 2026 16:49:27 GMT".
bool utc_parse_http_date(const char *s, time_t *out);

// ISO 8601 with a numeric offset or Z: "2026-10-14T16:05:00+00:00".
bool utc_parse_iso8601(const char *s, time_t *out);

// Three-letter English month abbreviation to 1-12, or 0.
int utc_month_from_abbrev(const char *s);
