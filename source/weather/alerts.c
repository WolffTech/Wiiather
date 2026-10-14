#include "alerts.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

AlertSeverity alert_severity(const char *name)
{
	static const char *const names[] = { "Unknown", "Minor", "Moderate", "Severe", "Extreme" };
	for (size_t i = 0; name && i < sizeof names / sizeof names[0]; i++)
		if (strcmp(name, names[i]) == 0)
			return (AlertSeverity)i;
	return SEVERITY_UNKNOWN;
}

static unsigned latin1(unsigned cp)
{
	if (cp < 0x100)
		return cp;
	switch (cp) {
	case 0x2018:
	case 0x2019:
		return '\'';
	case 0x201C:
	case 0x201D:
		return '"';
	case 0x2013:
	case 0x2014:
		return '-';
	default:
		return '?';
	}
}

void alert_copy_text(char *dst, size_t dst_len, const char *utf8)
{
	if (dst_len == 0)
		return;
	size_t n = 0;
	const unsigned char *p = (const unsigned char *)(utf8 ? utf8 : "");
	while (*p && n + 1 < dst_len) {
		unsigned cp = *p++;
		int extra = cp >= 0xF0 ? 3 : cp >= 0xE0 ? 2 : cp >= 0xC0 ? 1 : 0;
		bool bad = cp >= 0x80 && cp < 0xC0; // stray continuation byte
		if (extra)
			cp &= 0x3F >> extra;
		for (; extra > 0 && (*p & 0xC0) == 0x80; extra--)
			cp = cp << 6 | (*p++ & 0x3F);
		dst[n++] = (char)(bad || extra ? '?' : latin1(cp));
	}
	dst[n] = '\0';
}

// ws4kp calcSeverity(): base level plus a bump for the most dangerous warnings.
static int severity_score(const Alert *a)
{
	static const struct {
		const char *event;
		int bump;
	} bumps[] = { { "Hurricane Warning", 2 }, { "Tornado Warning", 3 }, { "Severe Thunderstorm Warning", 1 } };
	int score = a->severity == SEVERITY_EXTREME ? 10 : a->severity == SEVERITY_SEVERE ? 5 : 0;
	for (size_t i = 0; i < sizeof bumps / sizeof bumps[0]; i++)
		if (strcmp(a->event, bumps[i].event) == 0)
			score += bumps[i].bump;
	return score;
}

void alerts_select(Alerts *a)
{
	if (a->count > ALERTS_MAX)
		a->count = ALERTS_MAX;

	// ws4kp also counts "Unknown" alerts here, which then hides every alert
	// when the only immediate one has unknown severity; count shown ones only.
	bool has_immediate = false;
	for (size_t i = 0; i < a->count; i++)
		has_immediate |= a->alerts[i].immediate && a->alerts[i].severity != SEVERITY_UNKNOWN;

	// Stable insertion sort, most severe first. Alerts are large, so sort indexes.
	size_t order[ALERTS_MAX];
	for (size_t i = 0; i < a->count; i++) {
		size_t j = i;
		for (; j > 0 && severity_score(&a->alerts[order[j - 1]]) < severity_score(&a->alerts[i]); j--)
			order[j] = order[j - 1];
		order[j] = i;
	}

	static Alerts sorted;
	sorted.count = 0;
	for (size_t i = 0; i < a->count; i++) {
		const Alert *alert = &a->alerts[order[i]];
		if (alert->severity != SEVERITY_UNKNOWN && (!has_immediate || alert->immediate))
			sorted.alerts[sorted.count++] = *alert;
	}
	*a = sorted;
}

bool alerts_drop_expired(Alerts *a, time_t now)
{
	size_t kept = 0;
	for (size_t i = 0; i < a->count; i++)
		if (a->alerts[i].expires == 0 || a->alerts[i].expires > now) {
			if (kept != i)
				a->alerts[kept] = a->alerts[i];
			kept++;
		}
	bool dropped = kept != a->count;
	a->count = kept;
	return dropped;
}

typedef struct {
	char *s;
	size_t len, cap;
} Out;

static void put(Out *o, char c)
{
	if (o->len + 1 < o->cap)
		o->s[o->len++] = c;
	o->s[o->len] = '\0';
}

static void put_str(Out *o, const char *s)
{
	for (; *s; s++)
		put(o, *s);
}

// After ws4kp turns "\n\n" into <br/><br/>, the markup counts as non-space
// for its "(\S)\.\.\.(\S)" fix, so '\n' (a <br/>) does too.
static bool non_space(char c)
{
	return c && (c == '\n' || !strchr(" \t\r\f\v", c));
}

void alert_hazard_text(const Alert *a, char *out, size_t out_len)
{
	if (out_len == 0)
		return;
	Out o = { out, 0, out_len };
	out[0] = '\0';
	put_str(&o, a->event);
	put_str(&o, "\n\n");

	// description.replaceAll('\n\n', '<br/><br/>').replaceAll('\n', ' ')
	static char desc[ALERT_DESCRIPTION_MAX * 2];
	size_t n = 0;
	for (const char *p = a->description; *p && n + 2 < sizeof desc;) {
		if (p[0] == '\n' && p[1] == '\n') {
			desc[n++] = '\n';
			desc[n++] = '\n';
			p += 2;
		} else {
			desc[n++] = *p == '\n' ? ' ' : *p;
			p++;
		}
	}
	desc[n] = '\0';

	// .replace(/(\S)\.\.\.(\S)/g, '$1... $2') so the text can wrap after "...".
	for (size_t i = 0; i < n;) {
		if (non_space(desc[i]) && strncmp(&desc[i + 1], "...", 3) == 0 && non_space(desc[i + 4])) {
			put(&o, desc[i]);
			put_str(&o, "... ");
			put(&o, desc[i + 4]);
			i += 5;
		} else {
			put(&o, desc[i++]);
		}
	}
	put_str(&o, "\n\n\n\n");
}

void alert_scroll_text(const Alert *a, char *out, size_t out_len)
{
	if (out_len == 0)
		return;
	Out o = { out, 0, out_len };
	out[0] = '\0';
	bool space = false;
	const char *parts[] = { a->event, " ", a->description };
	for (size_t i = 0; i < 3; i++)
		for (const char *p = parts[i]; *p; p++) {
			if (strchr(" \t\r\n\f\v", *p)) {
				space = o.len > 0;
				continue;
			}
			if (space)
				put(&o, ' ');
			space = false;
			put(&o, *p);
		}
}

void alerts_viewed_update(AlertsViewed *v, uint32_t fetches)
{
	// ws4kp clears its viewed set on fetches 11, 21, 31...
	uint32_t epoch = fetches > 0 ? (fetches - 1) / ALERTS_VIEWED_FETCHES : 0;
	if (epoch != v->epoch) {
		v->count = v->next = 0;
		v->epoch = epoch;
	}
}

bool alerts_viewed_has(const AlertsViewed *v, const char *id)
{
	for (size_t i = 0; i < v->count; i++)
		if (strcmp(v->ids[i], id) == 0)
			return true;
	return false;
}

void alerts_viewed_add(AlertsViewed *v, const char *id)
{
	if (alerts_viewed_has(v, id))
		return;
	snprintf(v->ids[v->next], sizeof v->ids[0], "%s", id);
	v->next = (v->next + 1) % ALERTS_VIEWED_MAX;
	if (v->count < ALERTS_VIEWED_MAX)
		v->count++;
}

#define COUNT_MS 40
#define HAZARDS_SPEED 50 // px/s
#define HAZARDS_INITIAL_COUNTS 75 // 3 s
#define HAZARDS_FINAL_COUNTS 50 // 2 s
#define HAZARDS_STATIC_COUNTS 125 // 5 s

static uint32_t hazards_scroll_counts(int scrollable)
{
	// ceil(seconds * 1000 / 40) with seconds = px / 50
	return (uint32_t)(scrollable * 1000 + HAZARDS_SPEED * COUNT_MS - 1) / (HAZARDS_SPEED * COUNT_MS);
}

uint32_t hazards_display_ms(int content_height, int view_height)
{
	int scrollable = content_height > view_height ? content_height - view_height : 0;
	if (scrollable == 0)
		return HAZARDS_STATIC_COUNTS * COUNT_MS;
	return (HAZARDS_INITIAL_COUNTS + hazards_scroll_counts(scrollable) + HAZARDS_FINAL_COUNTS) * COUNT_MS;
}

int hazards_scroll_offset(int content_height, int view_height, uint32_t elapsed_ms)
{
	int scrollable = content_height > view_height ? content_height - view_height : 0;
	if (scrollable == 0)
		return 0;
	// ws4kp moves in whole 40 ms counts, by scrollable / counts pixels each.
	double per_count = (double)scrollable / hazards_scroll_counts(scrollable);
	double offset = ((double)(elapsed_ms / COUNT_MS) - HAZARDS_INITIAL_COUNTS) * per_count;
	if (offset > scrollable)
		offset = scrollable;
	return offset < 0 ? 0 : (int)floor(offset + 0.5);
}

#define TICK_MS 500
#define CRAWL_SPEED 100 // px/s
#define CRAWL_PAUSE_MS 1000 // before and after

static double crawl_seconds(int distance)
{
	double seconds = (double)distance / CRAWL_SPEED;
	return seconds < 2 ? 2 : seconds;
}

uint32_t alert_scroll_ms(int text_width, int view_width)
{
	int distance = text_width > view_width ? text_width - view_width : 0;
	double ms = crawl_seconds(distance) * 1000 + 2 * CRAWL_PAUSE_MS;
	return (uint32_t)ceil(ms / TICK_MS) * TICK_MS;
}

int alert_scroll_offset(int text_width, int view_width, uint32_t elapsed_ms)
{
	int distance = text_width > view_width ? text_width - view_width : 0;
	if (distance == 0 || elapsed_ms <= CRAWL_PAUSE_MS)
		return 0;
	// The CSS transition's duration is written with toFixed(1).
	double duration_ms = floor(crawl_seconds(distance) * 10 + 0.5) * 100;
	double offset = distance * (elapsed_ms - CRAWL_PAUSE_MS) / duration_ms;
	return offset >= distance ? distance : (int)floor(offset + 0.5);
}
