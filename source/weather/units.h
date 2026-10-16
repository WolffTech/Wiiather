// ws4kp's US and metric units (utils/units.mjs). NWS observations and
// gridpoint data come in SI units; forecasts come in the units requested.
#pragma once

typedef enum { UNITS_US, UNITS_METRIC } Units;

// Degrees, rounded, from Celsius.
int units_temperature(double celsius, Units units);
// Converts a whole-degree temperature, such as a forecast's, between units.
int units_temperature_convert(int value, Units from, Units to);
// MPH or kph, rounded, from km/h.
int units_speed(double kph, Units units);

// "F" or "C".
const char *units_temperature_name(Units units);
// "MPH" or "kph".
const char *units_speed_name(Units units);
