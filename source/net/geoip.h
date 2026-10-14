#pragma once
#include <stdbool.h>
#include <stdint.h>

// Guesses the ZIP code from the console's public IP address with ipinfo.io,
// which sees that address. False outside the US or when the lookup fails.
// Uses https_get, so call it from the weather thread only.
bool geoip_zip(uint32_t *zip);
