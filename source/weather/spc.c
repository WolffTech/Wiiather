#include "spc.h"

#include <string.h>

SpcRisk spc_risk(const char *label)
{
	static const char *const labels[] = {
		[SPC_TSTM] = "TSTM", [SPC_MRGL] = "MRGL", [SPC_SLGT] = "SLGT",
		[SPC_ENH] = "ENH", [SPC_MDT] = "MDT", [SPC_HIGH] = "HIGH",
	};
	for (SpcRisk r = SPC_TSTM; r <= SPC_HIGH; r++)
		if (label && strcmp(label, labels[r]) == 0)
			return r;
	return SPC_NONE;
}

// From https://github.com/substack/point-in-polygon, as ws4kp uses it.
bool spc_ring_contains(const double (*ring)[2], size_t count, double lon, double lat)
{
	bool inside = false;
	for (size_t i = 0, j = count - 1; i < count; j = i++) {
		double xi = ring[i][0], yi = ring[i][1], xj = ring[j][0], yj = ring[j][1];
		if ((yi > lat) != (yj > lat) && lon < (xj - xi) * (lat - yi) / (yj - yi) + xi)
			inside = !inside;
	}
	return inside;
}

bool spc_outlook_has_risk(const SpcOutlook *outlook)
{
	for (size_t i = 0; i < SPC_DAYS; i++)
		if (outlook->risk[i] != SPC_NONE)
			return true;
	return false;
}
