// Storm Prediction Center categorical outlook, ported from ws4kp's
// spc-outlook.mjs and utils/polygon.mjs. Pure logic with no I/O, so it can be
// tested on the host.
#pragma once
#include <stdbool.h>
#include <stddef.h>

// Today, tomorrow and the day after.
#define SPC_DAYS 3

// Categorical risk levels, least to most severe.
typedef enum { SPC_NONE, SPC_TSTM, SPC_MRGL, SPC_SLGT, SPC_ENH, SPC_MDT, SPC_HIGH } SpcRisk;

typedef struct {
	SpcRisk risk[SPC_DAYS]; // SPC_NONE where the day's fetch failed, like ws4kp
} SpcOutlook;

// A categorical feature's LABEL property, or SPC_NONE if unknown.
SpcRisk spc_risk(const char *label);

// Ray casting test of a GeoJSON ring of lon, lat pairs.
bool spc_ring_contains(const double (*ring)[2], size_t count, double lon, double lat);

// Whether any day has a risk; ws4kp skips the display otherwise.
bool spc_outlook_has_risk(const SpcOutlook *outlook);
