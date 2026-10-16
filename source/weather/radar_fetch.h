// Fetches the Local Radar's images from Iowa Environmental Mesonet's archive
// of national composites, like ws4kp's radar.mjs.
#pragma once
#include <stdbool.h>
#include <time.h>

#include "radar.h"

// Updates `images` to the newest composites cropped around the location,
// reusing images it already holds for the same location. `now` is UTC. On
// failure, `images` is left unchanged.
bool radar_fetch(double lat, double lon, time_t now, RadarImages *images);
