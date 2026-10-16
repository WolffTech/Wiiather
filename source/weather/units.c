#include "units.h"

#include <math.h>

// JavaScript Math.round, which rounds .5 toward +infinity.
static int js_round(double value)
{
	return (int)floor(value + 0.5);
}

int units_temperature(double celsius, Units units)
{
	return js_round(units == UNITS_US ? celsius * 9 / 5 + 32 : celsius);
}

int units_temperature_convert(int value, Units from, Units to)
{
	if (from == to)
		return value;
	return to == UNITS_US ? js_round(value * 9 / 5.0 + 32) : js_round((value - 32) * 5 / 9.0);
}

int units_speed(double kph, Units units)
{
	return js_round(units == UNITS_US ? kph / 1.60934 : kph);
}

const char *units_temperature_name(Units units)
{
	return units == UNITS_US ? "F" : "C";
}

const char *units_speed_name(Units units)
{
	return units == UNITS_US ? "MPH" : "kph";
}
