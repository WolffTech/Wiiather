#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_DIR "sd:/apps/wiiather"

// settings.ini names, for hide_display lines.
static const char *const display_keys[DISPLAY_COUNT] = {
	[DISPLAY_HAZARDS] = "hazards",
	[DISPLAY_CURRENT] = "current_conditions",
	[DISPLAY_LATEST_OBSERVATIONS] = "latest_observations",
	[DISPLAY_HOURLY] = "hourly_forecast",
	[DISPLAY_HOURLY_GRAPH] = "hourly_graph",
	[DISPLAY_TRAVEL] = "travel_forecast",
	[DISPLAY_REGIONAL] = "regional_forecast",
	[DISPLAY_LOCAL] = "local_forecast",
	[DISPLAY_EXTENDED] = "extended_forecast",
	[DISPLAY_ALMANAC] = "almanac",
	[DISPLAY_SPC_OUTLOOK] = "spc_outlook",
	[DISPLAY_RADAR] = "local_radar",
};

#define ALL_BUT_HAZARDS (((uint32_t)1 << DISPLAY_COUNT) - 1 - (1 << DISPLAY_HAZARDS))

// The raw values in settings.ini, before they are checked.
typedef struct {
	bool has_zip, has_lat, has_lon;
	unsigned long zip;
	double lat, lon;
	uint32_t hidden_displays;
} IniValues;

static void parse_line(char *line, IniValues *s)
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
	} else if (strcmp(key, "hide_display") == 0) {
		for (int i = 0; i < DISPLAY_COUNT; i++)
			if (strcmp(value, display_keys[i]) == 0)
				s->hidden_displays |= (uint32_t)1 << i;
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
	IniValues s = { 0 };
	char line[128];
	while (fgets(line, sizeof line, f)) {
		line[strcspn(line, "\r\n")] = '\0';
		parse_line(line, &s);
	}
	fclose(f);

	uint32_t hidden = s.hidden_displays & ALL_BUT_HAZARDS;
	out->hidden_displays = hidden == ALL_BUT_HAZARDS ? 0 : hidden;

	if (s.has_lat && s.has_lon) {
		out->location = (Location){ s.has_zip ? s.zip : 0, s.lat, s.lon };
		out->has_location = true;
	} else if (s.has_zip) {
		out->has_location = location_from_zip(s.zip, &out->location);
	}
}

bool config_display_shown(const Config *config, DisplayId id)
{
	return !(config->hidden_displays & (uint32_t)1 << id);
}

bool config_set_display_shown(Config *config, DisplayId id, bool shown)
{
	uint32_t bit = (uint32_t)1 << id;
	uint32_t hidden = shown ? config->hidden_displays & ~bit : config->hidden_displays | bit;
	if (id == DISPLAY_HAZARDS || (hidden & ALL_BUT_HAZARDS) == ALL_BUT_HAZARDS)
		return false;
	config->hidden_displays = hidden;
	return true;
}

void config_reset(Config *config)
{
	char path[sizeof config->dir + 16];
	settings_path(config, path, sizeof path);
	remove(path);
	Config reset = { 0 };
	snprintf(reset.dir, sizeof reset.dir, "%s", config->dir);
	*config = reset;
}

bool config_save(const Config *config)
{
	char path[sizeof config->dir + 16];
	settings_path(config, path, sizeof path);
	FILE *f = fopen(path, "w");
	if (!f)
		return false;
	const Location *loc = &config->location;
	if (config->has_location) {
		if (loc->zip)
			fprintf(f, "zip=%05lu\n", (unsigned long)loc->zip);
		fprintf(f, "lat=%.6f\nlon=%.6f\n", loc->lat, loc->lon);
	}
	for (int i = 0; i < DISPLAY_COUNT; i++)
		if (!config_display_shown(config, i))
			fprintf(f, "hide_display=%s\n", display_keys[i]);
	return fclose(f) == 0;
}
