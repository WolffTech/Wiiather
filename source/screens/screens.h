#pragma once
#include <gctypes.h>

#include "weather/format.h"
#include "weather/weather.h"

// Header shared by most displays: logo, two-line title, NOAA logo, clock.
void header_draw(const char *title_top, const char *title_bottom);

// Bottom bar text (ws4kp's "scroll" area).
void scroll_draw(const char *text);

void current_conditions_draw(const CurrentConditionsText *cc, u32 time_ms);

// Placeholder body while data loads or after a failure.
void status_draw(const WeatherSnapshot *snap);
