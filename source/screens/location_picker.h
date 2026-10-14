// ZIP code entry with the Wii Remote's D-pad, in the WeatherStar style.
#pragma once
#include <gctypes.h>

#include "weather/location.h"
#include "weather/weather.h"

typedef struct {
	ZipEntry entry;
	bool edited; // false until a digit changes, so a late IP guess can still fill it in
	bool cancelable; // false on first run, when there is no location to go back to
} LocationPicker;

typedef enum { PICKER_OPEN, PICKER_CHOSEN, PICKER_CANCELED } PickerResult;

// Starts at `current`'s ZIP, or with a blank entry when `current` is NULL.
LocationPicker location_picker_make(const Location *current);

// Applies this frame's button presses. On PICKER_CHOSEN, `chosen` receives
// the location.
PickerResult location_picker_update(LocationPicker *p, u32 pressed, const ZipGuess *guess, Location *chosen);

void location_picker_draw(const LocationPicker *p, const ZipGuess *guess);
