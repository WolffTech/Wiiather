// Local Radar, ported from ws4kp's radar.mjs, radar-utils.mjs and
// radar-processor.mjs. Pure logic with no I/O, so it can be tested on the host.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// ws4kp's dopplerRadarImageMax.
#define RADAR_IMAGES 6

// The part of Iowa Environmental Mesonet's national composite around the
// location (RADAR_SOURCE_SIZE), which ws4kp stretches over the main area.
#define RADAR_WIDTH 240
#define RADAR_HEIGHT 163

// ws4kp draws the whole composite into a canvas of this size before cropping.
#define RADAR_CANVAS_WIDTH 2550
#define RADAR_CANVAS_HEIGHT 1600

// One composite image's crop, with colors already converted for display.
typedef struct {
	time_t time; // UTC
	uint8_t pixels[RADAR_HEIGHT][RADAR_WIDTH]; // indexes into `colors`
	uint32_t colors[256]; // RGBA; index 0 is transparent
} RadarImage;

typedef struct {
	size_t count; // oldest first
	RadarImage images[RADAR_IMAGES];
} RadarImages;

// The newest image times in directory listings, oldest first.
typedef struct {
	size_t count;
	time_t times[RADAR_IMAGES];
} RadarTimes;

// Adds the time of each n0r_YYYYMMDDHHMM.png linked from a directory
// listing. Returns how many links it found.
size_t radar_add_listing(RadarTimes *times, const char *html);

// Which composite pixel each crop pixel shows, -1 past the edge of ws4kp's
// canvas. Rows only increase, so the composite can be decoded top down.
typedef struct {
	int32_t src_x[RADAR_WIDTH];
	int32_t src_y[RADAR_HEIGHT];
} RadarCrop;

// The crop around the location from a composite of src_w x src_h pixels.
void radar_crop(double lat, double lon, uint32_t src_w, uint32_t src_h, RadarCrop *out);

// ws4kp's removeDopplerRadarImageNoise: a composite color in the WeatherStar's
// colors as RGBA, or 0 for the background and the lightest returns.
uint32_t radar_color(uint8_t r, uint8_t g, uint8_t b);

// Fills image->colors from the composite's palette and `remap` from each
// palette index to its entry in image->colors. False if the palette has too
// many distinct colors.
bool radar_palette(const uint8_t (*rgb)[3], size_t count, RadarImage *image, uint8_t remap[256]);

// The radar maps' pixel at the main area's top-left corner.
void radar_map_origin(double lat, double lon, int *x, int *y);

// ws4kp's animation: three loops of the images, each holding on the newest,
// then a longer hold on it.
#define RADAR_STEPS 19
uint32_t radar_step_ms(size_t step);
// The image shown at `step` out of `count`. ws4kp always has six; with fewer,
// the sequence keeps ending on the newest.
size_t radar_step_image(size_t step, size_t count);

// An image's local time as ws4kp shows it: "12:05 PM", or " 3:05 PM" with a
// leading no-break space.
void radar_time_label(time_t local, char out[12]);
