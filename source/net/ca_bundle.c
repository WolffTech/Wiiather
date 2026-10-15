#include "ca_bundle.h"

#include <stdio.h>
#include <string.h>

bool ca_bundle_date(const char *pem, size_t len, uint64_t *out)
{
	static const char marker[] = "## Certificate data from Mozilla as of: ";
	static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	// The header is in the first few lines; don't scan the whole bundle.
	char head[1024];
	size_t n = len < sizeof head - 1 ? len : sizeof head - 1;
	memcpy(head, pem, n);
	head[n] = '\0';
	const char *line = strstr(head, marker);
	if (!line)
		return false;

	char month[4];
	unsigned day, hour, minute, second, year;
	if (sscanf(line + sizeof marker - 1, "%*3s %3s %u %u:%u:%u %u GMT", month, &day, &hour, &minute, &second,
		    &year) != 6)
		return false;
	const char *m = strlen(month) == 3 ? strstr(months, month) : NULL;
	if (!m || (m - months) % 3 != 0 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60)
		return false;
	unsigned mon = (unsigned)(m - months) / 3 + 1;
	*out = ((((year * 100ULL + mon) * 100 + day) * 100 + hour) * 100 + minute) * 100 + second;
	return true;
}
