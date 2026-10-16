#include "forecast.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "util/utc.h"

void forecast_drop_expired(Forecast *f, time_t now)
{
	size_t kept = 0;
	for (size_t i = 0; i < f->count; i++)
		if (f->periods[i].end > now)
			f->periods[kept++] = f->periods[i];
	f->count = kept;
}

static const char *const weekdays[][2] = {
	{ "Sun", "Sunday" }, { "Mon", "Monday" }, { "Tue", "Tuesday" }, { "Wed", "Wednesday" },
	{ "Thu", "Thursday" }, { "Fri", "Friday" }, { "Sat", "Saturday" },
};

void forecast_weekday(const char *iso_time, char out[4])
{
	int year, month, day;
	out[0] = '\0';
	if (!iso_time || sscanf(iso_time, "%d-%d-%d", &year, &month, &day) != 3)
		return;
	long days = utc_days_from_civil(year, month, day);
	int weekday = (int)((days % 7 + 11) % 7); // 1970-01-01 was a Thursday
	snprintf(out, 4, "%s", weekdays[weekday][0]);
}

const char *forecast_weekday_name(const char *weekday)
{
	for (size_t i = 0; i < sizeof weekdays / sizeof weekdays[0]; i++)
		if (strcmp(weekday, weekdays[i][0]) == 0)
			return weekdays[i][1];
	return "";
}

void forecast_local_text(const ForecastPeriod *p, char *out, size_t out_len)
{
	// ws4kp replaces only the first "..." in the text.
	const char *dots = strstr(p->detailed_forecast, "...");
	if (dots)
		snprintf(out, out_len, "%s...%.*s %s", p->name, (int)(dots - p->detailed_forecast), p->detailed_forecast,
			dots + 3);
	else
		snprintf(out, out_len, "%s...%s", p->name, p->detailed_forecast);
}

// JS String.replace with a global regex of literal text.
static void replace_all(char *s, size_t cap, const char *from, const char *to, bool ignore_case)
{
	char result[256];
	size_t from_len = strlen(from), to_len = strlen(to), n = 0;
	for (const char *p = s; *p && n < sizeof result - 1;) {
		bool match = ignore_case ? strncasecmp(p, from, from_len) == 0 : strncmp(p, from, from_len) == 0;
		if (match && n + to_len < sizeof result) {
			memcpy(result + n, to, to_len);
			n += to_len;
			p += from_len;
		} else {
			result[n++] = *p++;
		}
	}
	result[n] = '\0';
	snprintf(s, cap, "%s", result);
}

// Copies up to the first space (or NUL), at most 10 characters like substr(0, 10).
static size_t word(const char *s, char *out, size_t out_len)
{
	size_t len = strcspn(s, " ");
	snprintf(out, out_len, "%.*s", (int)(len < 10 ? len : 10), s);
	return len;
}

void forecast_shorten(const char *text, char *out, size_t out_len)
{
	static const struct {
		const char *from, *to;
		bool ignore_case;
	} rules[] = {
		{ " and ", " ", true }, { "slight ", "", true }, { "chance ", "", true }, { "very ", "", true },
		{ "patchy ", "", true }, { "Areas Of ", "", true }, { "areas ", "", true }, { "dense ", "", true },
		{ "Thunderstorm", "T'Storm", false },
	};
	char s[256];
	snprintf(s, sizeof s, "%s", text);
	for (size_t i = 0; i < sizeof rules / sizeof rules[0]; i++)
		replace_all(s, sizeof s, rules[i].from, rules[i].to, rules[i].ignore_case);

	// With "A then B", only B is summarized.
	const char *conditions = s;
	const char *then = strstr(s, " then ");
	if (strstr(s, "then") && then)
		conditions = then + 6;

	char first[16], second[16] = "";
	size_t len = word(conditions, first, sizeof first);
	if (conditions[len] == ' ' && conditions[len + 1] && conditions[len + 1] != ' ') {
		size_t first_len = strlen(first);
		if (first_len && first[first_len - 1] == '.') {
			char *dot = strchr(first, '.');
			memmove(dot, dot + 1, strlen(dot));
		} else {
			word(conditions + len + 1, second, sizeof second);
		}
		if (strcmp(second, "Blowing") == 0)
			second[0] = '\0';
	}
	if (second[0])
		snprintf(out, out_len, "%s %s", first, second);
	else
		snprintf(out, out_len, "%s", first);
}

size_t forecast_extended_days(const Forecast *f, ExtendedDay *out, size_t max)
{
	size_t start = f->count > 0 && !f->periods[0].is_daytime ? 1 : 0;
	size_t n = 0;
	bool open = false; // out[n] has been started
	for (size_t i = start; i < f->count && n < max; i++) {
		const ForecastPeriod *p = &f->periods[i];
		if (!open) {
			out[n] = (ExtendedDay){ 0 };
			open = true;
		}
		ExtendedDay *day = &out[n];
		if (p->is_daytime) {
			day->has_high = p->has_temperature;
			day->high = p->temperature;
			snprintf(day->icon_url, sizeof day->icon_url, "%s", p->icon_url);
			forecast_shorten(p->short_forecast, day->text, sizeof day->text);
			snprintf(day->weekday, sizeof day->weekday, "%s", p->weekday);
		} else {
			day->has_low = p->has_temperature;
			day->low = p->temperature;
			n++;
			open = false;
		}
	}
	return open ? n + 1 : n;
}

bool forecast_travel_day(const Forecast *f, TravelDay *out)
{
	size_t day = f->count > 0 && !f->periods[0].is_daytime ? 1 : 0;
	if (day + 1 >= f->count)
		return false;
	const ForecastPeriod *p = &f->periods[day], *night = &f->periods[day + 1];
	if (!p->has_temperature || !night->has_temperature)
		return false;
	*out = (TravelDay){ .high = p->temperature, .low = night->temperature };
	snprintf(out->weekday, sizeof out->weekday, "%s", p->weekday);
	snprintf(out->icon_url, sizeof out->icon_url, "%s", p->icon_url);
	return true;
}
