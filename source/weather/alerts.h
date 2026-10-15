// NWS active alerts and ws4kp's Hazards shaping (hazards.mjs and the hazard
// screen of currentweatherscroll.mjs). Pure logic, testable on the host.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "util/scroll_timing.h"

#define ALERTS_MAX 5 // ws4kp only considers the first five alerts
#define ALERT_ID_MAX 128
#define ALERT_EVENT_MAX 64
#define ALERT_DESCRIPTION_MAX 4096 // longer descriptions are truncated

typedef enum {
	SEVERITY_UNKNOWN,
	SEVERITY_MINOR,
	SEVERITY_MODERATE,
	SEVERITY_SEVERE,
	SEVERITY_EXTREME,
} AlertSeverity;

typedef struct {
	char id[ALERT_ID_MAX];
	char event[ALERT_EVENT_MAX]; // Latin-1
	AlertSeverity severity;
	bool immediate; // urgency is "Immediate"
	time_t expires; // UTC, 0 if unknown
	char description[ALERT_DESCRIPTION_MAX]; // Latin-1
} Alert;

typedef struct {
	size_t count;
	Alert alerts[ALERTS_MAX];
} Alerts;

AlertSeverity alert_severity(const char *name);

// Copies UTF-8 as Latin-1, truncating to fit. Typographic quotes and dashes
// become ASCII; other characters outside Latin-1 become '?'.
void alert_copy_text(char *dst, size_t dst_len, const char *utf8);

// ws4kp's ordering and filtering of the API's first ALERTS_MAX alerts: most
// severe first (stable), "Unknown" severity dropped, and only "Immediate"
// alerts kept when there are any.
void alerts_select(Alerts *a);

// Drops alerts that expired at or before `now` (UTC), keeping order.
// Returns whether any were dropped.
bool alerts_drop_expired(Alerts *a, time_t now);

// The Hazards display's text for one alert, before text-transform: uppercase.
// '\n' marks a <br/>: the event, a blank line, the description with its
// paragraph breaks, and three blank lines of padding.
void alert_hazard_text(const Alert *a, char *out, size_t out_len);

// The bottom bar's crawl text: "event description" with whitespace collapsed
// the way HTML renders it.
void alert_scroll_text(const Alert *a, char *out, size_t out_len);

// ws4kp marks alerts as viewed once the Hazards display has run through them
// and shows only unviewed ones, forgetting them every tenth alert fetch.
#define ALERTS_VIEWED_MAX 16
#define ALERTS_VIEWED_FETCHES 10

typedef struct {
	char ids[ALERTS_VIEWED_MAX][ALERT_ID_MAX];
	size_t count, next; // ring buffer once full
	uint32_t epoch;
} AlertsViewed;

// `fetches` counts alert fetches since startup, starting at 1.
void alerts_viewed_update(AlertsViewed *v, uint32_t fetches);
bool alerts_viewed_has(const AlertsViewed *v, const char *id);
void alerts_viewed_add(AlertsViewed *v, const char *id);

// Hazards pauses 3 s before scrolling and 2 s after.
#define HAZARDS_SCROLL_TIMING ((ScrollTiming){ 75, 50 })

// ws4kp's bottom-bar crawl: 1 s pause, 100 px/s (at least 2 s), 1 s pause,
// rounded up to the bar's 500 ms ticks.
uint32_t alert_scroll_ms(int text_width, int view_width);
int alert_scroll_offset(int text_width, int view_width, uint32_t elapsed_ms);
