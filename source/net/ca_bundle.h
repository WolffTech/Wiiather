// Facts about curl's extract of Mozilla's CA bundle (cacert.pem).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The "Certificate data from Mozilla as of" date as YYYYMMDDhhmmss, so newer
// bundles compare greater. False if the header is missing or malformed.
bool ca_bundle_date(const char *pem, size_t len, uint64_t *out);
