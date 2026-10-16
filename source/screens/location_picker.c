#include "location_picker.h"

#include <grrlib.h>
#include <stdio.h>
#include <wiiuse/wpad.h>

#include "render/gfx.h"
#include "screens.h"

#define DIGIT_PITCH 48
#define DIGITS_TOP 175
#define ARROW_HALF 12

static const TextStyle prompt_style = { FONT_STAR4000_32, RGBA_WHITE, true, false, 0 };
static const TextStyle digit_style = { FONT_STAR4000_LARGE_32, RGBA_WHITE, true, false, 0 };
static const TextStyle cursor_style = { FONT_STAR4000_LARGE_32, RGBA_YELLOW, true, false, 0 };
static const TextStyle preview_style = { FONT_STAR4000_LARGE_20, RGBA_YELLOW, true, true, 24 };
static const TextStyle note_style = { FONT_STAR4000_LARGE_20, RGBA_WHITE, true, true, 24 };

LocationPicker location_picker_make(const Location *current)
{
	return (LocationPicker){
		.entry = zip_entry_make(current ? current->zip : 0),
		.cancelable = current != NULL,
	};
}

PickerResult location_picker_update(LocationPicker *p, u32 pressed, const ZipGuess *guess, Location *chosen)
{
	if (!p->edited && !p->cancelable && guess->status == GUESS_FOUND)
		p->entry = zip_entry_make(guess->zip);

	if (pressed & (WPAD_BUTTON_UP | WPAD_BUTTON_DOWN | WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT))
		p->edited = true;
	if ((pressed & WPAD_BUTTON_1) && guess->status == GUESS_FOUND) {
		p->entry = zip_entry_make(guess->zip);
		p->edited = true;
	}
	if (pressed & WPAD_BUTTON_UP)
		zip_entry_step(&p->entry, 1);
	if (pressed & WPAD_BUTTON_DOWN)
		zip_entry_step(&p->entry, -1);
	if (pressed & WPAD_BUTTON_LEFT)
		zip_entry_move(&p->entry, -1);
	if (pressed & WPAD_BUTTON_RIGHT)
		zip_entry_move(&p->entry, 1);

	if ((pressed & WPAD_BUTTON_A) && location_from_zip(zip_entry_value(&p->entry), chosen))
		return PICKER_CHOSEN;
	if ((pressed & WPAD_BUTTON_B) && p->cancelable)
		return PICKER_CANCELED;
	return PICKER_OPEN;
}

static void arrow(f32 x, f32 tip_y, f32 base_y)
{
	guVector v[3] = { { x, tip_y, 0 }, { x + ARROW_HALF, base_y, 0 }, { x - ARROW_HALF, base_y, 0 } };
	u32 colors[3] = { RGBA_YELLOW, RGBA_YELLOW, RGBA_YELLOW };
	GRRLIB_NGoneFilled(v, colors, 3);
}

void location_picker_draw(const LocationPicker *p, const ZipGuess *guess)
{
	gfx_image(IMG_BACKGROUND_1, 0, 0);
	header_draw("Location", NULL, false);
	gfx_text(&prompt_style, 320, 110, ALIGN_CENTER, "Enter your ZIP code");

	const f32 line = gfx_line_height(FONT_STAR4000_LARGE_32);
	for (int i = 0; i < ZIP_DIGITS; i++) {
		f32 x = 320 + (i - ZIP_DIGITS / 2) * DIGIT_PITCH;
		char digit[2] = { (char)('0' + p->entry.digits[i]), '\0' };
		bool selected = i == p->entry.cursor;
		gfx_text(selected ? &cursor_style : &digit_style, x, DIGITS_TOP, ALIGN_CENTER, digit);
		if (selected) {
			arrow(x, DIGITS_TOP - 22, DIGITS_TOP - 8);
			arrow(x, DIGITS_TOP + line + 22, DIGITS_TOP + line + 8);
		}
	}

	Location loc;
	char preview[96];
	if (location_from_zip(zip_entry_value(&p->entry), &loc)) {
		char city[64];
		location_nearest_city(&loc, city, sizeof city);
		snprintf(preview, sizeof preview, "Near %s", city);
	} else {
		snprintf(preview, sizeof preview, "ZIP code not found");
	}
	gfx_text(&preview_style, 320, 285, ALIGN_CENTER, preview);

	char note[48] = "";
	if (!p->cancelable && !p->edited) {
		if (guess->status == GUESS_PENDING)
			snprintf(note, sizeof note, "Detecting your location...");
		else if (guess->status == GUESS_FOUND)
			snprintf(note, sizeof note, "Detected from your internet connection");
	} else if (guess->status == GUESS_FOUND && zip_entry_value(&p->entry) != guess->zip) {
		snprintf(note, sizeof note, "1: Use detected ZIP (%05lu)", (unsigned long)guess->zip);
	}
	if (note[0])
		gfx_text(&note_style, 320, 325, ALIGN_CENTER, note);

	scroll_draw(p->cancelable ? "D-Pad: Edit  A: Save  B: Back" : "D-Pad: Edit  A: Save");
}
