#include "scroll_timing.h"

#include <math.h>

#define COUNT_MS 40
#define SPEED 50 // px/s

static uint32_t scroll_counts(int scrollable)
{
	// ceil(seconds * 1000 / 40) with seconds = px / 50
	return (uint32_t)(scrollable * 1000 + SPEED * COUNT_MS - 1) / (SPEED * COUNT_MS);
}

uint32_t scroll_timing_ms(ScrollTiming t, int content_height, int view_height)
{
	int scrollable = content_height > view_height ? content_height - view_height : 0;
	uint32_t counts = t.initial_counts + t.final_counts;
	if (scrollable > 0)
		counts += scroll_counts(scrollable);
	return counts * COUNT_MS;
}

int scroll_timing_offset(ScrollTiming t, int content_height, int view_height, uint32_t elapsed_ms)
{
	int scrollable = content_height > view_height ? content_height - view_height : 0;
	if (scrollable == 0)
		return 0;
	// ws4kp moves in whole 40 ms counts, by scrollable / counts pixels each.
	double per_count = (double)scrollable / scroll_counts(scrollable);
	double offset = ((double)(elapsed_ms / COUNT_MS) - t.initial_counts) * per_count;
	if (offset > scrollable)
		offset = scrollable;
	return offset < 0 ? 0 : (int)floor(offset + 0.5);
}
