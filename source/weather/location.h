// Where the weather is for, and the ZIP code entry that picks it.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZIP_DIGITS 5

typedef struct {
	uint32_t zip; // 0 when set by coordinates alone
	double lat, lon;
} Location;

// The location for a 5-digit ZIP code, from the Census ZCTA table. False for
// ZIPs without a tabulation area, such as PO box-only codes.
bool location_from_zip(uint32_t zip, Location *out);

// "City, ST" of the curated station nearest the location, for confirming a
// ZIP before it is saved.
void location_nearest_city(const Location *loc, char *out, size_t out_len);

// Digit-by-digit ZIP entry for the D-pad: left/right picks a digit,
// up/down changes it with wraparound.
typedef struct {
	uint8_t digits[ZIP_DIGITS];
	uint8_t cursor;
} ZipEntry;

ZipEntry zip_entry_make(uint32_t zip);
void zip_entry_move(ZipEntry *e, int delta);
void zip_entry_step(ZipEntry *e, int delta);
uint32_t zip_entry_value(const ZipEntry *e);
