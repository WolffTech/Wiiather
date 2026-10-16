// Pages share the displays' frame: background 1, the header with the clock,
// rows in the main area and button hints in the bottom bar.
#include "settings_menu.h"

#include <grrlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wiiuse/wpad.h>

#include "audio/music.h"
#include "render/gfx.h"
#include "screens.h"

#define MAIN_LEFT 64
#define MAIN_WIDTH 508
#define LABEL_LEFT 100
#define VALUE_RIGHT 556
#define ROW_TOP 110
#define ROW_PITCH 52
#define BAR_HEIGHT 42
#define POINTER_LEFT 76

// The Displays page's checklist: two columns of six.
#define CHECK_ROWS 6
#define CHECK_LEFT 78
#define CHECK_COLUMN_PITCH 252
#define CHECK_TOP 112
#define CHECK_PITCH 46
#define CHECK_BOX 20

// The Music page's volume bar, one segment per MUSIC_VOLUME_STEP.
#define VOLUME_SEGMENTS (100 / MUSIC_VOLUME_STEP)
#define SEGMENT_PITCH 10

// The Tracks page's scrolling checklist.
#define TRACK_ROWS 6
#define TRACK_LEFT 90
#define TRACK_TOP 110
#define TRACK_PITCH 40
#define TRACK_NAME_RIGHT 470
#define SCROLLBAR_LEFT 566

#define RGBA_DIM 0x9AA3C8FF
#define RGBA_BAR 0xFFFFFF1A
#define RGBA_BAR_TOP 0xFFFFFF2E
#define RGBA_BAR_BOTTOM 0x00000059

static const TextStyle label_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, 0 };
static const TextStyle label_selected_style = { FONT_STAR4000_32, RGBA_YELLOW, true, false, 0 };
static const TextStyle label_dim_style = { FONT_STAR4000_32, RGBA_DIM, true, false, 0 };
static const TextStyle value_style = { FONT_STAR4000_LARGE_28, RGBA_WHITE, true, false, 0 };
static const TextStyle value_selected_style = { FONT_STAR4000_LARGE_28, RGBA_YELLOW, true, false, 0 };
static const TextStyle check_style = { FONT_STAR4000_24, RGBA_WHITE, true, false, 0 };
static const TextStyle check_selected_style = { FONT_STAR4000_24, RGBA_YELLOW, true, false, 0 };
static const TextStyle check_dim_style = { FONT_STAR4000_24, RGBA_DIM, true, false, 0 };
static const TextStyle tag_style = { FONT_STAR4000_LARGE_20, RGBA_YELLOW, true, false, 0 };
static const TextStyle count_style = { FONT_STAR4000_LARGE_20, RGBA_DIM, true, false, 0 };
static const TextStyle playing_style = { FONT_STAR4000_LARGE_20, RGBA_YELLOW, true, true, 24 };
static const TextStyle prompt_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, 0 };
static const TextStyle note_style = { FONT_STAR4000_LARGE_20, RGBA_WHITE, true, true, 24 };

typedef enum { MAIN_LOCATION, MAIN_DISPLAYS, MAIN_MUSIC, MAIN_GENERAL, MAIN_ABOUT, MAIN_ROWS } MainRow;
static const char *const main_labels[MAIN_ROWS] = { "Location", "Displays", "Music", "General", "About" };

typedef enum { MUSIC_ON, MUSIC_ORDER, MUSIC_VOLUME, MUSIC_TRACKS, MUSIC_ROWS } MusicRow;

typedef enum { GENERAL_SPEED, GENERAL_CLOCK, GENERAL_BOTTOM_BAR, GENERAL_LOCATE, GENERAL_ROWS } GeneralRow;

static const char *const speed_names[SPEED_COUNT] = {
	[SPEED_VERY_FAST] = "Very Fast",
	[SPEED_FAST] = "Fast",
	[SPEED_NORMAL] = "Normal",
	[SPEED_SLOW] = "Slow",
	[SPEED_VERY_SLOW] = "Very Slow",
};

static const char *const display_names[DISPLAY_COUNT] = {
	[DISPLAY_HAZARDS] = "Hazards",
	[DISPLAY_CURRENT] = "Current Conditions",
	[DISPLAY_LATEST_OBSERVATIONS] = "Latest Obs.",
	[DISPLAY_HOURLY] = "Hourly Forecast",
	[DISPLAY_HOURLY_GRAPH] = "Hourly Graph",
	[DISPLAY_TRAVEL] = "Travel Forecast",
	[DISPLAY_REGIONAL] = "Regional Forecast",
	[DISPLAY_LOCAL] = "Local Forecast",
	[DISPLAY_EXTENDED] = "Extended Forecast",
	[DISPLAY_ALMANAC] = "Almanac",
	[DISPLAY_SPC_OUTLOOK] = "SPC Outlook",
	[DISPLAY_RADAR] = "Local Radar",
};

typedef enum { ABOUT_VERSION, ABOUT_LOCATION, ABOUT_CERTIFICATES, ABOUT_WEATHER, ABOUT_RESET, ABOUT_ROWS } AboutRow;

const ZipGuess *settings_zip_guess(const Config *config, const WeatherSnapshot *snap)
{
	static const ZipGuess none = { .status = GUESS_NONE };
	return config->locate_by_ip ? &snap->zip_guess : &none;
}

SettingsMenu settings_menu_make(void)
{
	return (SettingsMenu){ .page = PAGE_MAIN };
}

// Moves a cursor up or down with wraparound.
static void move_cursor(size_t *cursor, size_t count, u32 pressed)
{
	if (pressed & WPAD_BUTTON_UP)
		*cursor = (*cursor + count - 1) % count;
	if (pressed & WPAD_BUTTON_DOWN)
		*cursor = (*cursor + 1) % count;
}

static void update_main(SettingsMenu *m, u32 pressed, const Config *config, SettingsResult *result)
{
	move_cursor(&m->main_row, MAIN_ROWS, pressed);
	if (pressed & WPAD_BUTTON_B) {
		result->closed = true;
		return;
	}
	if (!(pressed & WPAD_BUTTON_A))
		return;
	switch ((MainRow)m->main_row) {
	case MAIN_LOCATION:
		m->picker = location_picker_make(&config->location);
		m->page = PAGE_LOCATION;
		if (config->locate_by_ip)
			weather_request_zip_guess();
		break;
	case MAIN_DISPLAYS:
		m->page = PAGE_DISPLAYS;
		break;
	case MAIN_MUSIC:
		m->page = PAGE_MUSIC;
		break;
	case MAIN_GENERAL:
		m->page = PAGE_GENERAL;
		break;
	case MAIN_ABOUT:
		m->page = PAGE_ABOUT;
		break;
	case MAIN_ROWS:
		break;
	}
}

// Up and down move within a column, left and right between them.
static void update_displays(SettingsMenu *m, u32 pressed, Config *config, SettingsResult *result)
{
	size_t column = m->display_row / CHECK_ROWS, row = m->display_row % CHECK_ROWS;
	move_cursor(&row, CHECK_ROWS, pressed);
	if (pressed & (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT))
		column = !column;
	m->display_row = (DisplayId)(column * CHECK_ROWS + row);

	if (pressed & WPAD_BUTTON_A) {
		DisplayId id = m->display_row;
		if (config_set_display_shown(config, id, !config_display_shown(config, id)))
			result->changed |= SETTINGS_DISPLAYS;
	}
	if (pressed & WPAD_BUTTON_B)
		m->page = PAGE_MAIN;
}

static void music_changed(const Config *config, SettingsResult *result)
{
	music_configure(config);
	result->changed |= SETTINGS_MUSIC;
}

// Left and right change the selected value; A also flips the on/off ones.
static void update_music(SettingsMenu *m, u32 pressed, Config *config, SettingsResult *result)
{
	move_cursor(&m->music_row, MUSIC_ROWS, pressed);
	bool flip = pressed & (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT | WPAD_BUTTON_A);
	switch ((MusicRow)m->music_row) {
	case MUSIC_ON:
		if (flip) {
			config->music = !config->music;
			music_changed(config, result);
		}
		break;
	case MUSIC_ORDER:
		if (flip) {
			config->music_order = config->music_order == MUSIC_SHUFFLE ? MUSIC_IN_ORDER : MUSIC_SHUFFLE;
			music_changed(config, result);
		}
		break;
	case MUSIC_VOLUME: {
		int volume = config->music_volume;
		if (pressed & WPAD_BUTTON_LEFT)
			volume -= MUSIC_VOLUME_STEP;
		if (pressed & WPAD_BUTTON_RIGHT)
			volume += MUSIC_VOLUME_STEP;
		volume = volume < 0 ? 0 : volume > 100 ? 100 : volume;
		if (volume != config->music_volume) {
			config->music_volume = (uint8_t)volume;
			music_changed(config, result);
		}
		break;
	}
	case MUSIC_TRACKS:
		if ((pressed & WPAD_BUTTON_A) && music_track_count() > 0)
			m->page = PAGE_TRACKS;
		break;
	case MUSIC_ROWS:
		break;
	}
	if (pressed & WPAD_BUTTON_B)
		m->page = PAGE_MAIN;
}

static void update_tracks(SettingsMenu *m, u32 pressed, Config *config, SettingsResult *result)
{
	size_t count = music_track_count();
	move_cursor(&m->track_row, count, pressed);
	if (m->track_row < m->track_scroll)
		m->track_scroll = m->track_row;
	if (m->track_row >= m->track_scroll + TRACK_ROWS)
		m->track_scroll = m->track_row - TRACK_ROWS + 1;

	const char *file = music_track_file(m->track_row);
	if ((pressed & WPAD_BUTTON_A)
		&& config_set_track_enabled(config, file, !config_track_enabled(config, file)))
		music_changed(config, result);
	if (pressed & WPAD_BUTTON_1) {
		if (!config->music) {
			config->music = true;
			music_changed(config, result);
		}
		music_play(m->track_row);
	}
	if (pressed & WPAD_BUTTON_B)
		m->page = PAGE_MUSIC;
}

// Left and right change the selected value; A also flips the on/off ones.
static void update_general(SettingsMenu *m, u32 pressed, Config *config, SettingsResult *result)
{
	move_cursor(&m->general_row, GENERAL_ROWS, pressed);
	bool flip = pressed & (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT | WPAD_BUTTON_A);
	bool *toggle = NULL;
	switch ((GeneralRow)m->general_row) {
	case GENERAL_SPEED: {
		int speed = config->speed;
		if ((pressed & WPAD_BUTTON_LEFT) && speed > 0)
			speed--;
		if ((pressed & WPAD_BUTTON_RIGHT) && speed < SPEED_COUNT - 1)
			speed++;
		if (speed != (int)config->speed) {
			config->speed = (Speed)speed;
			result->changed |= SETTINGS_GENERAL;
		}
		break;
	}
	case GENERAL_CLOCK:
		toggle = &config->clock_24h;
		break;
	case GENERAL_BOTTOM_BAR:
		toggle = &config->bottom_bar;
		break;
	case GENERAL_LOCATE:
		toggle = &config->locate_by_ip;
		break;
	case GENERAL_ROWS:
		break;
	}
	if (toggle && flip) {
		*toggle = !*toggle;
		result->changed |= SETTINGS_GENERAL;
	}
	if (pressed & WPAD_BUTTON_B)
		m->page = PAGE_MAIN;
}

SettingsResult settings_menu_update(SettingsMenu *m, u32 pressed, Config *config, const WeatherSnapshot *snap)
{
	SettingsResult result = { 0 };
	switch (m->page) {
	case PAGE_MAIN:
		update_main(m, pressed, config, &result);
		break;
	case PAGE_LOCATION: {
		Location chosen;
		PickerResult picked = location_picker_update(&m->picker, pressed, settings_zip_guess(config, snap), &chosen);
		if (picked == PICKER_CHOSEN) {
			config->location = chosen;
			config->has_location = true;
			result.changed |= SETTINGS_LOCATION;
		}
		if (picked != PICKER_OPEN)
			m->page = PAGE_MAIN;
		break;
	}
	case PAGE_DISPLAYS:
		update_displays(m, pressed, config, &result);
		break;
	case PAGE_MUSIC:
		update_music(m, pressed, config, &result);
		break;
	case PAGE_TRACKS:
		update_tracks(m, pressed, config, &result);
		break;
	case PAGE_GENERAL:
		update_general(m, pressed, config, &result);
		break;
	case PAGE_ABOUT:
		if (pressed & WPAD_BUTTON_A)
			m->page = PAGE_RESET;
		else if (pressed & WPAD_BUTTON_B)
			m->page = PAGE_MAIN;
		break;
	case PAGE_RESET:
		if (pressed & WPAD_BUTTON_A) {
			config_reset(config);
			result.changed |= SETTINGS_RESET;
			result.closed = true;
		} else if (pressed & WPAD_BUTTON_B) {
			m->page = PAGE_ABOUT;
		}
		break;
	}
	return result;
}

// `title_bottom` may be NULL for a one-line title.
static void frame_draw(const char *title_top, const char *title_bottom, const char *hints)
{
	gfx_image(IMG_BACKGROUND_1, 0, 0);
	header_draw(title_top, title_bottom, false);
	scroll_draw(hints);
}

#define POINTER_WIDTH 13

// A yellow pointer with the text's drop shadow, from its left edge at `x`.
static void pointer_draw(f32 x, f32 y, bool left)
{
	const f32 shadow = 2;
	f32 tip = left ? x : x + POINTER_WIDTH, base = left ? x + POINTER_WIDTH : x;
	for (int pass = 0; pass < 2; pass++) {
		f32 d = pass ? 0 : shadow;
		u32 color = pass ? RGBA_YELLOW : RGBA_BLACK;
		guVector v[3] = { { base + d, y + d, 0 }, { tip + d, y + 9 + d, 0 }, { base + d, y + 18 + d, 0 } };
		u32 colors[3] = { color, color, color };
		GRRLIB_NGoneFilled(v, colors, 3);
	}
}

// ROW_CHOICE is selected with arrows for changing the value.
typedef enum { ROW_PLAIN, ROW_SELECTED, ROW_CHOICE, ROW_READ_ONLY } RowLook;

// A label on the left and its value right-aligned; `value` may be NULL.
static void row_draw(size_t index, RowLook look, const char *label, const char *value)
{
	f32 top = ROW_TOP + index * ROW_PITCH;
	bool selected = look == ROW_SELECTED || look == ROW_CHOICE;
	if (selected) {
		GRRLIB_Rectangle(MAIN_LEFT, top - 8, MAIN_WIDTH, BAR_HEIGHT, RGBA_BAR, true);
		GRRLIB_Rectangle(MAIN_LEFT, top - 8, MAIN_WIDTH, 1, RGBA_BAR_TOP, true);
		GRRLIB_Rectangle(MAIN_LEFT, top - 8 + BAR_HEIGHT - 1, MAIN_WIDTH, 1, RGBA_BAR_BOTTOM, true);
		pointer_draw(POINTER_LEFT, top + 2, false);
	}
	const TextStyle *ls = selected ? &label_selected_style
		: look == ROW_READ_ONLY ? &label_dim_style
		: &label_style;
	gfx_text(ls, LABEL_LEFT, top, ALIGN_LEFT, label);
	if (!value)
		return;
	f32 right = VALUE_RIGHT;
	if (look == ROW_CHOICE) {
		const f32 gap = 10;
		right -= POINTER_WIDTH + gap;
		pointer_draw(right + gap, top + 5, false);
		pointer_draw(right - gfx_text_width(value_style.font, value) - gap - POINTER_WIDTH, top + 5, true);
	}
	gfx_text(selected ? &value_selected_style : &value_style, right, top + 3, ALIGN_RIGHT, value);
}

// "Grand Rapids, MI", or coordinates for a location set without a ZIP.
static void location_name(const Location *loc, char *out, size_t out_len)
{
	if (loc->zip)
		location_nearest_city(loc, out, out_len);
	else
		snprintf(out, out_len, "%.2f%c %.2f%c", fabs(loc->lat), loc->lat < 0 ? 'S' : 'N', fabs(loc->lon),
			loc->lon < 0 ? 'W' : 'E');
}

static size_t enabled_tracks(const Config *config)
{
	size_t enabled = 0;
	for (size_t i = 0; i < music_track_count(); i++)
		enabled += config_track_enabled(config, music_track_file(i));
	return enabled;
}

// A track's file name without ".mp3".
static void track_name(size_t track, char *out, size_t out_len)
{
	const char *file = music_track_file(track);
	snprintf(out, out_len, "%.*s", (int)(strlen(file) - 4), file);
}

static void main_draw(const SettingsMenu *m, const Config *config)
{
	frame_draw("Settings", NULL, "A: Select  B: Close");
	for (size_t i = 0; i < MAIN_ROWS; i++) {
		char value[48];
		switch ((MainRow)i) {
		case MAIN_LOCATION:
			location_name(&config->location, value, sizeof value);
			break;
		case MAIN_DISPLAYS: {
			int shown = 0;
			for (int id = 0; id < DISPLAY_COUNT; id++)
				shown += config_display_shown(config, id);
			snprintf(value, sizeof value, "%d of %d on", shown, DISPLAY_COUNT);
			break;
		}
		case MAIN_MUSIC:
			if (music_track_count() == 0)
				snprintf(value, sizeof value, "No tracks");
			else
				snprintf(value, sizeof value, "%s, %zu tracks", config->music ? "On" : "Off", enabled_tracks(config));
			break;
		case MAIN_GENERAL:
			snprintf(value, sizeof value, ">");
			break;
		case MAIN_ABOUT:
			snprintf(value, sizeof value, "%s", WIIATHER_VERSION);
			break;
		case MAIN_ROWS:
			break;
		}
		row_draw(i, i == m->main_row ? ROW_SELECTED : ROW_PLAIN, main_labels[i], value);
	}
}

// A checkbox with the text's drop shadow, filled in when `checked`.
static void checkbox_draw(f32 x, f32 y, bool checked, bool selected)
{
	GRRLIB_Rectangle(x + 2, y + 2, CHECK_BOX, CHECK_BOX, RGBA_BLACK, true);
	GRRLIB_Rectangle(x, y, CHECK_BOX, CHECK_BOX, selected ? RGBA_YELLOW : RGBA_WHITE, true);
	GRRLIB_Rectangle(x + 2, y + 2, CHECK_BOX - 4, CHECK_BOX - 4, 0x000000FF, true);
	GRRLIB_Rectangle(x + 3, y + 3, CHECK_BOX - 6, CHECK_BOX - 6, 0x1F2A5CFF, true);
	if (checked)
		GRRLIB_Rectangle(x + 5, y + 5, CHECK_BOX - 10, CHECK_BOX - 10, RGBA_YELLOW, true);
}

static void displays_draw(const SettingsMenu *m, const Config *config)
{
	frame_draw("Displays", NULL, "A: Toggle  B: Back");
	for (int id = 0; id < DISPLAY_COUNT; id++) {
		f32 x = CHECK_LEFT + id / CHECK_ROWS * CHECK_COLUMN_PITCH;
		f32 top = CHECK_TOP + id % CHECK_ROWS * CHECK_PITCH;
		bool selected = (int)m->display_row == id;
		checkbox_draw(x, top + 2, config_display_shown(config, id), selected);
		// Hazards can't be turned off.
		const TextStyle *style = selected ? &check_selected_style
			: id == DISPLAY_HAZARDS ? &check_dim_style
			: &check_style;
		gfx_text(style, x + 30, top, ALIGN_LEFT, display_names[id]);
	}
}

// A bar of segments, filled up to `volume`, ending at VALUE_RIGHT.
static void volume_draw(f32 top, unsigned volume, bool selected)
{
	const f32 w = SEGMENT_PITCH - 2, h = 18;
	f32 left = VALUE_RIGHT - VOLUME_SEGMENTS * SEGMENT_PITCH + 2;
	for (unsigned i = 0; i < VOLUME_SEGMENTS; i++) {
		f32 x = left + i * SEGMENT_PITCH;
		bool on = i < volume / MUSIC_VOLUME_STEP;
		GRRLIB_Rectangle(x + 2, top + 2, w, h, RGBA_BLACK, true);
		GRRLIB_Rectangle(x, top, w, h, on || selected ? RGBA_YELLOW : RGBA_WHITE, true);
		if (!on)
			GRRLIB_Rectangle(x + 2, top + 2, w - 4, h - 4, 0x1F2A5CFF, true);
	}
}

static void music_draw(const SettingsMenu *m, const Config *config)
{
	bool on_tracks = m->music_row == MUSIC_TRACKS;
	frame_draw("Music", NULL, on_tracks ? "A: Select  B: Back" : "D-Pad: Change  B: Back");
	for (size_t i = 0; i < MUSIC_ROWS; i++) {
		bool selected = i == m->music_row;
		RowLook choice = selected ? ROW_CHOICE : ROW_PLAIN;
		char value[48];
		switch ((MusicRow)i) {
		case MUSIC_ON:
			row_draw(i, choice, "Music", config->music ? "On" : "Off");
			break;
		case MUSIC_ORDER:
			row_draw(i, choice, "Order", config->music_order == MUSIC_SHUFFLE ? "Shuffle" : "In Order");
			break;
		case MUSIC_VOLUME:
			row_draw(i, selected ? ROW_SELECTED : ROW_PLAIN, "Volume", NULL);
			volume_draw(ROW_TOP + i * ROW_PITCH + 6, config->music_volume, selected);
			break;
		case MUSIC_TRACKS:
			if (music_track_count() == 0)
				snprintf(value, sizeof value, "None");
			else
				snprintf(value, sizeof value, "%zu of %zu", enabled_tracks(config), music_track_count());
			row_draw(i, selected ? ROW_SELECTED : ROW_PLAIN, "Tracks", value);
			break;
		case MUSIC_ROWS:
			break;
		}
	}

	char note[160];
	int playing = music_playing();
	if (music_track_count() == 0) {
		snprintf(note, sizeof note, "Copy MP3s to apps/wiiather/music");
	} else if (playing >= 0) {
		char name[128];
		track_name((size_t)playing, name, sizeof name);
		snprintf(note, sizeof note, "Now playing: %s", name);
	} else {
		note[0] = '\0';
	}
	gfx_text(&playing_style, 320, 336, ALIGN_CENTER, note);
}

static void general_draw(const SettingsMenu *m, const Config *config)
{
	frame_draw("General", NULL, "D-Pad: Change  B: Back");
	for (size_t i = 0; i < GENERAL_ROWS; i++) {
		RowLook look = i == m->general_row ? ROW_CHOICE : ROW_PLAIN;
		switch ((GeneralRow)i) {
		case GENERAL_SPEED:
			row_draw(i, look, "Speed", speed_names[config->speed]);
			break;
		case GENERAL_CLOCK:
			row_draw(i, look, "Clock", config->clock_24h ? "24-Hour" : "12-Hour");
			break;
		case GENERAL_BOTTOM_BAR:
			row_draw(i, look, "Bottom Bar", config->bottom_bar ? "On" : "Off");
			break;
		case GENERAL_LOCATE:
			row_draw(i, look, "Locate by IP", config->locate_by_ip ? "On" : "Off");
			break;
		case GENERAL_ROWS:
			break;
		}
	}
	gfx_text(&playing_style, 320, 336, ALIGN_CENTER,
		m->general_row == GENERAL_BOTTOM_BAR ? "Weather alerts still show when off" : "");
}

static void tracks_draw(const SettingsMenu *m, const Config *config)
{
	frame_draw("Music", "Tracks", "A: Toggle  1: Play Now  B: Back");
	size_t count = music_track_count();
	int playing = music_playing();
	for (size_t row = 0; row < TRACK_ROWS && m->track_scroll + row < count; row++) {
		size_t track = m->track_scroll + row;
		f32 top = TRACK_TOP + row * TRACK_PITCH;
		bool selected = track == m->track_row;
		checkbox_draw(TRACK_LEFT, top + 2, config_track_enabled(config, music_track_file(track)), selected);
		char name[128];
		track_name(track, name, sizeof name);
		gfx_clip(TRACK_LEFT, top - 8, TRACK_NAME_RIGHT - TRACK_LEFT, TRACK_PITCH);
		gfx_text(selected ? &check_selected_style : &check_style, TRACK_LEFT + 30, top, ALIGN_LEFT, name);
		gfx_clip_reset();
		if ((int)track == playing)
			gfx_text(&tag_style, VALUE_RIGHT, top + 2, ALIGN_RIGHT, "PLAYING");
	}

	char shown[32];
	snprintf(shown, sizeof shown, "%zu of %zu on", enabled_tracks(config), count);
	gfx_text(&count_style, TRACK_LEFT, TRACK_TOP + TRACK_ROWS * TRACK_PITCH + 4, ALIGN_LEFT, shown);

	if (count > TRACK_ROWS) {
		const f32 height = TRACK_ROWS * TRACK_PITCH - 8;
		f32 thumb = height * TRACK_ROWS / count, offset = height * m->track_scroll / count;
		GRRLIB_Rectangle(SCROLLBAR_LEFT, TRACK_TOP, 6, height, 0x00000059, true);
		GRRLIB_Rectangle(SCROLLBAR_LEFT + 2, TRACK_TOP + offset + 2, 6, thumb, RGBA_BLACK, true);
		GRRLIB_Rectangle(SCROLLBAR_LEFT, TRACK_TOP + offset, 6, thumb, RGBA_YELLOW, true);
	}
}

// "Oct 2, 2026" from YYYYMMDDhhmmss.
static void format_ca_date(u64 date, char *out, size_t out_len)
{
	static const char *const months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	unsigned year = (unsigned)(date / 10000000000ULL);
	unsigned month = (unsigned)(date / 100000000ULL % 100), day = (unsigned)(date / 1000000ULL % 100);
	if (date == 0 || month < 1 || month > 12)
		snprintf(out, out_len, "-");
	else
		snprintf(out, out_len, "%s %u, %u", months[month - 1], day, year);
}

static void about_draw(const Config *config, const WeatherSnapshot *snap)
{
	frame_draw("About", NULL, "A: Select  B: Back");
	char value[48];
	row_draw(ABOUT_VERSION, ROW_READ_ONLY, "Version", WIIATHER_VERSION);

	snprintf(value, sizeof value, "%.2f%c %.2f%c", fabs(config->location.lat), config->location.lat < 0 ? 'S' : 'N',
		fabs(config->location.lon), config->location.lon < 0 ? 'W' : 'E');
	row_draw(ABOUT_LOCATION, ROW_READ_ONLY, "Location", value);

	format_ca_date(snap->ca_date, value, sizeof value);
	row_draw(ABOUT_CERTIFICATES, ROW_READ_ONLY, "Certificates", value);

	// The RTC holds local time, as in header_draw.
	if (snap->updated) {
		struct tm tm;
		localtime_r(&snap->updated, &tm);
		clock_format(&tm, config->clock_24h, false, value, sizeof value);
	} else {
		snprintf(value, sizeof value, "-");
	}
	row_draw(ABOUT_WEATHER, ROW_READ_ONLY, "Weather Data", value);

	row_draw(ABOUT_RESET, ROW_SELECTED, "Reset Settings", NULL);
}

static void reset_draw(void)
{
	frame_draw("Reset", NULL, "A: Reset  B: Cancel");
	gfx_text(&prompt_style, 320, 150, ALIGN_CENTER, "Reset all settings?");
	gfx_text(&note_style, 320, 215, ALIGN_CENTER, "Your location and choices go");
	gfx_text(&note_style, 320, 245, ALIGN_CENTER, "back to their defaults.");
}

void settings_menu_draw(const SettingsMenu *m, const Config *config, const WeatherSnapshot *snap)
{
	switch (m->page) {
	case PAGE_MAIN:
		main_draw(m, config);
		break;
	case PAGE_LOCATION:
		location_picker_draw(&m->picker, settings_zip_guess(config, snap));
		break;
	case PAGE_DISPLAYS:
		displays_draw(m, config);
		break;
	case PAGE_MUSIC:
		music_draw(m, config);
		break;
	case PAGE_TRACKS:
		tracks_draw(m, config);
		break;
	case PAGE_GENERAL:
		general_draw(m, config);
		break;
	case PAGE_ABOUT:
		about_draw(config, snap);
		break;
	case PAGE_RESET:
		reset_draw();
		break;
	}
}
