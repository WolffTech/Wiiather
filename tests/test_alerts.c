// Host tests for alert selection and Hazards text and timing: make test
#include <stdio.h>
#include <string.h>

#include "weather/alerts.h"

static int failures;

#define CHECK(cond)                                                          \
	do {                                                                 \
		if (!(cond)) {                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			failures++;                                          \
		}                                                            \
	} while (0)
#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

static Alert alert(const char *id, const char *event, const char *severity, bool immediate)
{
	Alert a = { .severity = alert_severity(severity), .immediate = immediate };
	snprintf(a.id, sizeof a.id, "%s", id);
	snprintf(a.event, sizeof a.event, "%s", event);
	return a;
}

static Alerts alerts;

static void test_select(void)
{
	alerts = (Alerts){ .count = 5 };
	alerts.alerts[0] = alert("a", "Flood Advisory", "Minor", false);
	alerts.alerts[1] = alert("b", "Flood Warning", "Severe", false);
	alerts.alerts[2] = alert("c", "Air Quality Alert", "Unknown", false);
	alerts.alerts[3] = alert("d", "Flood Watch", "Severe", false);
	alerts.alerts[4] = alert("e", "Extreme Heat Warning", "Extreme", false);
	alerts_select(&alerts);
	// Most severe first, ties in API order, "Unknown" dropped.
	CHECK(alerts.count == 4);
	CHECK_STR(alerts.alerts[0].id, "e");
	CHECK_STR(alerts.alerts[1].id, "b");
	CHECK_STR(alerts.alerts[2].id, "d");
	CHECK_STR(alerts.alerts[3].id, "a");

	// Specific warnings outrank their severity level.
	alerts = (Alerts){ .count = 3 };
	alerts.alerts[0] = alert("a", "Severe Thunderstorm Warning", "Severe", false);
	alerts.alerts[1] = alert("b", "Tornado Warning", "Severe", false);
	alerts.alerts[2] = alert("c", "Flood Warning", "Severe", false);
	alerts_select(&alerts);
	CHECK_STR(alerts.alerts[0].id, "b");
	CHECK_STR(alerts.alerts[1].id, "a");
	CHECK_STR(alerts.alerts[2].id, "c");

	// Any immediate alert hides the rest.
	alerts = (Alerts){ .count = 3 };
	alerts.alerts[0] = alert("a", "Flood Watch", "Severe", false);
	alerts.alerts[1] = alert("b", "Flash Flood Warning", "Severe", true);
	alerts.alerts[2] = alert("c", "Special Weather Statement", "Moderate", true);
	alerts_select(&alerts);
	CHECK(alerts.count == 2);
	CHECK_STR(alerts.alerts[0].id, "b");
	CHECK_STR(alerts.alerts[1].id, "c");

	// An immediate alert that is never shown doesn't hide the others.
	alerts = (Alerts){ .count = 2 };
	alerts.alerts[0] = alert("a", "Flood Watch", "Severe", false);
	alerts.alerts[1] = alert("b", "Test Message", "Unknown", true);
	alerts_select(&alerts);
	CHECK(alerts.count == 1);
	CHECK_STR(alerts.alerts[0].id, "a");
}

static void test_expired(void)
{
	alerts = (Alerts){ .count = 3 };
	alerts.alerts[0] = alert("a", "A", "Severe", false);
	alerts.alerts[1] = alert("b", "B", "Severe", false);
	alerts.alerts[2] = alert("c", "C", "Severe", false);
	alerts.alerts[0].expires = 100;
	alerts.alerts[1].expires = 200;
	CHECK(!alerts_drop_expired(&alerts, 50));
	CHECK(alerts_drop_expired(&alerts, 100));
	CHECK(alerts.count == 2);
	CHECK_STR(alerts.alerts[0].id, "b");
	CHECK_STR(alerts.alerts[1].id, "c"); // unknown expiry is kept
}

static void test_text(void)
{
	char out[256];
	alert_copy_text(out, sizeof out, "caf\xC3\xA9 \xE2\x80\x9Cquoted\xE2\x80\x9D \xE2\x80\x94 \xF0\x9F\x8C\x80 \x80x");
	CHECK_STR(out, "caf\xE9 \"quoted\" - ? ?x");
	alert_copy_text(out, 5, "abcdefg");
	CHECK_STR(out, "abcd");

	static Alert a;
	a = alert("a", "Flood Warning", "Severe", false);
	snprintf(a.description, sizeof a.description,
		"* WHAT...Flooding caused by\nrain.\n\n* WHERE...Norman...Moore.\n\n\nA...B...C ...D");
	alert_hazard_text(&a, out, sizeof out);
	CHECK_STR(out, "Flood Warning\n\n* WHAT... Flooding caused by rain.\n\n* WHERE... Norman... Moore.\n\n A... B...C "
		       "...D\n\n\n\n");

	alert_scroll_text(&a, out, sizeof out);
	CHECK_STR(out, "Flood Warning * WHAT...Flooding caused by rain. * WHERE...Norman...Moore. A...B...C ...D");
}

static void test_viewed(void)
{
	static AlertsViewed v;
	alerts_viewed_update(&v, 1);
	alerts_viewed_add(&v, "a");
	alerts_viewed_add(&v, "a");
	CHECK(v.count == 1);
	alerts_viewed_update(&v, 10);
	CHECK(alerts_viewed_has(&v, "a"));
	alerts_viewed_update(&v, 11);
	CHECK(!alerts_viewed_has(&v, "a"));

	char id[8];
	for (int i = 0; i < ALERTS_VIEWED_MAX + 1; i++) {
		snprintf(id, sizeof id, "%d", i);
		alerts_viewed_add(&v, id);
	}
	CHECK(!alerts_viewed_has(&v, "0"));
	CHECK(alerts_viewed_has(&v, "1"));
	CHECK(alerts_viewed_has(&v, id));
}

static void test_timing(void)
{
	// Measured on ws4kp: 5583 px of hazards in a 480 px view ran 107.08 s.
	CHECK(scroll_timing_ms(HAZARDS_SCROLL_TIMING, 5583, 480) == 2677 * 40);
	CHECK(scroll_timing_ms(HAZARDS_SCROLL_TIMING, 300, 480) == 5000);
	CHECK(scroll_timing_offset(HAZARDS_SCROLL_TIMING, 5583, 480, 2999) == 0);
	CHECK(scroll_timing_offset(HAZARDS_SCROLL_TIMING, 5583, 480, 3040) == 2);
	CHECK(scroll_timing_offset(HAZARDS_SCROLL_TIMING, 5583, 480, 200000) == 5103);

	// ws4kp: scrollWidth 16680 in a 530 px bar gives "transform 161.5s linear".
	CHECK(alert_scroll_ms(16680, 530) == 163500);
	CHECK(alert_scroll_offset(16680, 530, 1000) == 0);
	CHECK(alert_scroll_offset(16680, 530, 2000) == 100);
	CHECK(alert_scroll_offset(16680, 530, 162500) == 16150);
	CHECK(alert_scroll_ms(400, 530) == 4000);
	CHECK(alert_scroll_offset(400, 530, 3000) == 0);
	CHECK(alert_scroll_ms(630, 530) == 4000); // 1 s of travel stretched to 2 s
	CHECK(alert_scroll_offset(630, 530, 2000) == 50);
}

int main(void)
{
	test_select();
	test_expired();
	test_text();
	test_viewed();
	test_timing();
	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	puts("all tests passed");
	return 0;
}
