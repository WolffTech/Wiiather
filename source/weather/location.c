#include "location.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "stations_gen.h"
#include "zips_gen.h"

static int compare_zip(const void *key, const void *elem)
{
	uint32_t a = *(const uint32_t *)key, b = ((const ZipInfo *)elem)->zip;
	return (a > b) - (a < b);
}

bool location_from_zip(uint32_t zip, Location *out)
{
	const ZipInfo *info = bsearch(&zip, zip_info, ZIP_COUNT, sizeof zip_info[0], compare_zip);
	if (!info)
		return false;
	*out = (Location){ zip, info->lat, info->lon };
	return true;
}

void location_nearest_city(const Location *loc, char *out, size_t out_len)
{
	// Equirectangular distance is plenty to rank stations this close.
	double cos_lat = cos(loc->lat * M_PI / 180), best = INFINITY;
	const StationInfo *nearest = NULL;
	for (size_t i = 0; i < STATION_COUNT; i++) {
		double dy = station_info[i].lat - loc->lat;
		double dx = (station_info[i].lon - loc->lon) * cos_lat;
		double d = dx * dx + dy * dy;
		if (d < best) {
			best = d;
			nearest = &station_info[i];
		}
	}
	snprintf(out, out_len, "%s, %s", nearest->city, nearest->state);
}

ZipEntry zip_entry_make(uint32_t zip)
{
	ZipEntry e = { 0 };
	for (int i = ZIP_DIGITS - 1; i >= 0; i--, zip /= 10)
		e.digits[i] = zip % 10;
	return e;
}

static int wrap(int value, int count)
{
	return ((value % count) + count) % count;
}

void zip_entry_move(ZipEntry *e, int delta)
{
	e->cursor = wrap(e->cursor + delta, ZIP_DIGITS);
}

void zip_entry_step(ZipEntry *e, int delta)
{
	e->digits[e->cursor] = wrap(e->digits[e->cursor] + delta, 10);
}

uint32_t zip_entry_value(const ZipEntry *e)
{
	uint32_t zip = 0;
	for (int i = 0; i < ZIP_DIGITS; i++)
		zip = zip * 10 + e->digits[i];
	return zip;
}
