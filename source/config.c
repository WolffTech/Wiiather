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

#define DEFAULT_VOLUME 75 // ws4kp's default

// The raw values in settings.ini, before they are checked.
typedef struct {
	bool has_zip, has_lat, has_lon;
	unsigned long zip;
	double lat, lon;
} IniValues;

// The defaults, with no location.
static Config defaults(const char *dir)
{
	Config c = { .music = true, .music_order = MUSIC_SHUFFLE, .music_volume = DEFAULT_VOLUME };
	snprintf(c.dir, sizeof c.dir, "%s", dir);
	return c;
}

// Parses everything but the location, which needs every line first.
static void parse_line(char *line, IniValues *s, Config *out)
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
				out->hidden_displays |= (uint32_t)1 << i;
	} else if (strcmp(key, "music") == 0) {
		out->music = strcmp(value, "off") != 0;
	} else if (strcmp(key, "music_order") == 0) {
		out->music_order = strcmp(value, "in_order") == 0 ? MUSIC_IN_ORDER : MUSIC_SHUFFLE;
	} else if (strcmp(key, "music_volume") == 0) {
		unsigned long volume = strtoul(value, &end, 10);
		if (end != value && volume <= 100)
			out->music_volume = (uint8_t)(volume / MUSIC_VOLUME_STEP * MUSIC_VOLUME_STEP);
	} else if (strcmp(key, "skip_track") == 0) {
		config_set_track_enabled(out, value, false);
	}
}

static void settings_path(const Config *config, char *out, size_t out_len)
{
	snprintf(out, out_len, "%s/settings.ini", config->dir);
}

void config_load(const char *argv0, Config *out)
{
	char dir[sizeof out->dir];
	const char *slash = argv0 ? strrchr(argv0, '/') : NULL;
	if (slash)
		snprintf(dir, sizeof dir, "%.*s", (int)(slash - argv0), argv0);
	else
		snprintf(dir, sizeof dir, DEFAULT_DIR);
	*out = defaults(dir);

	char path[sizeof out->dir + 16];
	settings_path(out, path, sizeof path);
	FILE *f = fopen(path, "r");
	if (!f)
		return;
	IniValues s = { 0 };
	char line[160];
	while (fgets(line, sizeof line, f)) {
		line[strcspn(line, "\r\n")] = '\0';
		parse_line(line, &s, out);
	}
	fclose(f);

	uint32_t hidden = out->hidden_displays & ALL_BUT_HAZARDS;
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

bool config_track_enabled(const Config *config, const char *file)
{
	for (size_t i = 0; i < config->skipped_count; i++)
		if (strcmp(config->skipped_tracks[i], file) == 0)
			return false;
	return true;
}

bool config_set_track_enabled(Config *config, const char *file, bool enabled)
{
	for (size_t i = 0; i < config->skipped_count; i++) {
		if (strcmp(config->skipped_tracks[i], file) != 0)
			continue;
		if (enabled) {
			memmove(config->skipped_tracks[i], config->skipped_tracks[i + 1],
				(config->skipped_count - i - 1) * sizeof config->skipped_tracks[0]);
			config->skipped_count--;
		}
		return true;
	}
	if (enabled)
		return true;
	if (config->skipped_count == MAX_SKIPPED_TRACKS || strlen(file) >= sizeof config->skipped_tracks[0])
		return false;
	snprintf(config->skipped_tracks[config->skipped_count++], sizeof config->skipped_tracks[0], "%s", file);
	return true;
}

void config_reset(Config *config)
{
	char path[sizeof config->dir + 16];
	settings_path(config, path, sizeof path);
	remove(path);
	*config = defaults(config->dir);
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
	fprintf(f, "music=%s\nmusic_order=%s\nmusic_volume=%u\n", config->music ? "on" : "off",
		config->music_order == MUSIC_IN_ORDER ? "in_order" : "shuffle", (unsigned)config->music_volume);
	for (size_t i = 0; i < config->skipped_count; i++)
		fprintf(f, "skip_track=%s\n", config->skipped_tracks[i]);
	return fclose(f) == 0;
}
