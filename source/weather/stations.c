#include "stations.h"

#include <stdlib.h>
#include <string.h>

static int compare_station(const void *key, const void *elem)
{
	return strcmp(key, ((const StationInfo *)elem)->id);
}

const StationInfo *station_find(const char *id)
{
	return bsearch(id, station_info, STATION_COUNT, sizeof station_info[0], compare_station);
}
