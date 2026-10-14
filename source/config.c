#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_DIR "sd:/apps/wiiather"

typedef struct {
	bool has_zip, has_lat, has_lon;
	unsigned long zip;
	double lat, lon;
} Settings;

static void parse_line(char *line, Settings *s)
{
	char *eq = strchr(line, '=');
	if (!eq)
		return;
	*eq = '\0';
	const char *key = line, *value = eq + 1;
	char *end;
	if (strcmp(key, "zip") == 0) {
		s->zip = strtoul(value, &end, 10);
		s->has_zip = end != value;
	} else if (strcmp(key, "lat") == 0) {
		s->lat = strtod(value, &end);
		s->has_lat = end != value;
	} else if (strcmp(key, "lon") == 0) {
		s->lon = strtod(value, &end);
		s->has_lon = end != value;
	}
}

static void settings_path(const Config *config, char *out, size_t out_len)
{
	snprintf(out, out_len, "%s/settings.ini", config->dir);
}

void config_load(const char *argv0, Config *out)
{
	*out = (Config){ 0 };
	const char *slash = argv0 ? strrchr(argv0, '/') : NULL;
	if (slash)
		snprintf(out->dir, sizeof out->dir, "%.*s", (int)(slash - argv0), argv0);
	else
		snprintf(out->dir, sizeof out->dir, DEFAULT_DIR);

	char path[sizeof out->dir + 16];
	settings_path(out, path, sizeof path);
	FILE *f = fopen(path, "r");
	if (!f)
		return;
	Settings s = { 0 };
	char line[128];
	while (fgets(line, sizeof line, f)) {
		line[strcspn(line, "\r\n")] = '\0';
		parse_line(line, &s);
	}
	fclose(f);

	if (s.has_lat && s.has_lon) {
		out->location = (Location){ s.has_zip ? s.zip : 0, s.lat, s.lon };
		out->has_location = true;
	} else if (s.has_zip) {
		out->has_location = location_from_zip(s.zip, &out->location);
	}
}

bool config_save(const Config *config)
{
	char path[sizeof config->dir + 16];
	settings_path(config, path, sizeof path);
	FILE *f = fopen(path, "w");
	if (!f)
		return false;
	const Location *loc = &config->location;
	if (loc->zip)
		fprintf(f, "zip=%05lu\n", (unsigned long)loc->zip);
	fprintf(f, "lat=%.6f\nlon=%.6f\n", loc->lat, loc->lon);
	return fclose(f) == 0;
}
