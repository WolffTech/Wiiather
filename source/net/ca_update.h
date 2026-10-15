// Keeps the trusted CA list current without a new release. The project
// publishes curl's extract of Mozilla's bundle with an ECDSA signature, and
// the app installs newer signed copies to the SD card.
#pragma once
#include <stdbool.h>

// Switches to the SD card copy in `dir` if it is signed and newer than the
// built-in bundle. Call after https_init.
void ca_update_init(const char *dir);

// Downloads the published bundle if it changed and installs it if newer.
// `unverified` skips the TLS certificate check, for when the current list
// can no longer verify the server; the signature still guards the content.
// Returns true if a newer bundle was installed.
bool ca_update_check(bool unverified);
