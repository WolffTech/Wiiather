#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_DIR "sd:/apps/wiiather"

// Washington, DC until the location picker exists.
static const Config defaults = { .lat = 38.8951, .lon = -77.0364 };

static void parse_line(char *line, Config *out)
{
	char *eq = strchr(line, '=');
	if (!eq)
		return;
	*eq = '\0';
	const char *key = line, *value = eq + 1;
	if (strcmp(key, "lat") == 0)
		out->lat = strtod(value, NULL);
	else if (strcmp(key, "lon") == 0)
		out->lon = strtod(value, NULL);
}

bool config_load(const char *argv0, Config *out)
{
	*out = defaults;

	const char *slash = argv0 ? strrchr(argv0, '/') : NULL;
	if (slash)
		snprintf(out->dir, sizeof out->dir, "%.*s", (int)(slash - argv0), argv0);
	else
		snprintf(out->dir, sizeof out->dir, DEFAULT_DIR);

	char path[sizeof out->dir + 16];
	snprintf(path, sizeof path, "%s/settings.ini", out->dir);

	FILE *f = fopen(path, "r");
	if (!f)
		return false;
	char line[128];
	while (fgets(line, sizeof line, f)) {
		line[strcspn(line, "\r\n")] = '\0';
		parse_line(line, out);
	}
	fclose(f);
	return true;
}
