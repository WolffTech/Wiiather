#pragma once
#include "stations_gen.h"

// ws4kp's curated station entry for an ICAO id, or NULL.
const StationInfo *station_find(const char *id);
