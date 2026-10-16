#pragma once
#include <stdint.h>

// ws4kp's calculateScrollTiming() (utils/scroll-timing.mjs): a pause, a
// 50 px/s scroll, then another pause, counted in 40 ms steps. Content that
// fits is shown without scrolling instead.
typedef struct {
	uint32_t initial_counts; // before scrolling starts
	uint32_t final_counts; // after it ends
	uint32_t static_counts; // for content that fits; ws4kp defaults to both pauses
} ScrollTiming;

// ws4kp's defaults: 3 s before and after.
#define SCROLL_TIMING_DEFAULT ((ScrollTiming){ 75, 75, 150 })

uint32_t scroll_timing_ms(ScrollTiming t, int content_height, int view_height);

// How far the content has scrolled up after `elapsed_ms`.
int scroll_timing_offset(ScrollTiming t, int content_height, int view_height, uint32_t elapsed_ms);
