// Pages share the displays' frame: background 1, the header with the clock,
// rows in the main area and button hints in the bottom bar.
#include "settings_menu.h"

#include <grrlib.h>
#include <math.h>
#include <stdio.h>
#include <wiiuse/wpad.h>

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
static const TextStyle prompt_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, 0 };
static const TextStyle note_style = { FONT_STAR4000_LARGE_20, RGBA_WHITE, true, true, 24 };

typedef enum { MAIN_LOCATION, MAIN_DISPLAYS, MAIN_ABOUT, MAIN_ROWS } MainRow;
static const char *const main_labels[MAIN_ROWS] = { "Location", "Displays", "About" };

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
		weather_request_zip_guess();
		break;
	case MAIN_DISPLAYS:
		m->page = PAGE_DISPLAYS;
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

SettingsResult settings_menu_update(SettingsMenu *m, u32 pressed, Config *config, const WeatherSnapshot *snap)
{
	SettingsResult result = { 0 };
	switch (m->page) {
	case PAGE_MAIN:
		update_main(m, pressed, config, &result);
		break;
	case PAGE_LOCATION: {
		Location chosen;
		PickerResult picked = location_picker_update(&m->picker, pressed, &snap->zip_guess, &chosen);
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

static void frame_draw(const char *title, const char *hints)
{
	gfx_image(IMG_BACKGROUND_1, 0, 0);
	header_draw(title, NULL, false);
	scroll_draw(hints);
}

// A yellow pointer with the text's drop shadow.
static void pointer_draw(f32 x, f32 y)
{
	const f32 shadow = 2;
	for (int pass = 0; pass < 2; pass++) {
		f32 dx = pass ? 0 : shadow;
		u32 color = pass ? RGBA_YELLOW : RGBA_BLACK;
		guVector v[3] = { { x + dx, y + dx, 0 }, { x + 13 + dx, y + 9 + dx, 0 }, { x + dx, y + 18 + dx, 0 } };
		u32 colors[3] = { color, color, color };
		GRRLIB_NGoneFilled(v, colors, 3);
	}
}

typedef enum { ROW_PLAIN, ROW_SELECTED, ROW_READ_ONLY } RowLook;

// A label on the left and its value right-aligned; `value` may be NULL.
static void row_draw(size_t index, RowLook look, const char *label, const char *value)
{
	f32 top = ROW_TOP + index * ROW_PITCH;
	if (look == ROW_SELECTED) {
		GRRLIB_Rectangle(MAIN_LEFT, top - 8, MAIN_WIDTH, BAR_HEIGHT, RGBA_BAR, true);
		GRRLIB_Rectangle(MAIN_LEFT, top - 8, MAIN_WIDTH, 1, RGBA_BAR_TOP, true);
		GRRLIB_Rectangle(MAIN_LEFT, top - 8 + BAR_HEIGHT - 1, MAIN_WIDTH, 1, RGBA_BAR_BOTTOM, true);
		pointer_draw(POINTER_LEFT, top + 2);
	}
	const TextStyle *ls = look == ROW_SELECTED ? &label_selected_style
		: look == ROW_READ_ONLY ? &label_dim_style
		: &label_style;
	gfx_text(ls, LABEL_LEFT, top, ALIGN_LEFT, label);
	if (value)
		gfx_text(look == ROW_SELECTED ? &value_selected_style : &value_style, VALUE_RIGHT, top + 3, ALIGN_RIGHT, value);
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

static void main_draw(const SettingsMenu *m, const Config *config)
{
	frame_draw("Settings", "A: Select  B: Close");
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
	frame_draw("Displays", "A: Toggle  B: Back");
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
	frame_draw("About", "A: Select  B: Back");
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
		int hour = tm.tm_hour % 12 ? tm.tm_hour % 12 : 12;
		snprintf(value, sizeof value, "%d:%02d %s", hour, tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
	} else {
		snprintf(value, sizeof value, "-");
	}
	row_draw(ABOUT_WEATHER, ROW_READ_ONLY, "Weather Data", value);

	row_draw(ABOUT_RESET, ROW_SELECTED, "Reset Settings", NULL);
}

static void reset_draw(void)
{
	frame_draw("Reset", "A: Reset  B: Cancel");
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
		location_picker_draw(&m->picker, &snap->zip_guess);
		break;
	case PAGE_DISPLAYS:
		displays_draw(m, config);
		break;
	case PAGE_ABOUT:
		about_draw(config, snap);
		break;
	case PAGE_RESET:
		reset_draw();
		break;
	}
}
