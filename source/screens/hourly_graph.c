// Port of ws4kp's Hourly Graph (hourly-graph.mjs, hourly-graph.ejs,
// _hourly-graph.scss) in its standard layout: four lines on a 532x285 chart,
// temperatures on the left, times below and a legend in the clock's place.
#include <grrlib.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "render/gfx.h"
#include "screens.h"

#define MAIN_TOP 90
#define MAIN_HEIGHT 310
#define CHART_LEFT 50
#define CHART_WIDTH 532
#define CHART_HEIGHT 285
#define PLOT_LEFT 5 // calcScale() puts the first hour here and the last at the chart's edge
#define PLOT_PADDING 10 // and keeps values this far from the top and bottom
#define LINE_WIDTH 3
#define SHADOW_WIDTH (LINE_WIDTH + 2)
#define SHADOW_DROP 2
#define LABEL_MARGIN_TOP -15
#define X_AXIS_LEFT 54
#define X_AXIS_HEIGHT 20
#define LEGEND_TOP 35
#define LEGEND_RIGHT (SCREEN_WIDTH - 60)
#define LEGEND_MARGIN_TOP -18

#define RGBA_RED 0xFF0000FF
#define RGBA_GREEN 0x008000FF
#define RGBA_LIGHT_GREY 0xD3D3D3FF
#define RGBA_AQUA 0x00FFFFFF

static const TextStyle label_style = { FONT_STAR4000_SMALL_32, RGBA_YELLOW, true, false, 0 };
static const TextStyle legend_style = { FONT_STAR4000_SMALL_28, RGBA_WHITE, true, false, 0 };

static long rtc_hour(void)
{
	return (long)(time(NULL) / 3600);
}

void hourly_graph_view_prepare(const WeatherSnapshot *snap, HourlyGraphView *out)
{
	out->rtc_hour = rtc_hour();
	// Labels are in local time, which needs the RTC's offset from UTC.
	out->ready = snap->has_hourly && snap->has_utc_offset &&
		     hourly_graph(&snap->hourly, time(NULL) - snap->utc_offset, snap->utc_offset, snap->units,
			&out->graph);
}

bool hourly_graph_view_outdated(const HourlyGraphView *view)
{
	return view->rtc_hour != rtc_hour();
}

// A canvas stroke segment with butt caps.
static void segment(f32 x0, f32 y0, f32 x1, f32 y1, f32 width, u32 color)
{
	f32 dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
	if (len == 0)
		return;
	f32 nx = -dy / len * width / 2, ny = dx / len * width / 2;
	guVector v[4] = { { x0 + nx, y0 + ny, 0 }, { x1 + nx, y1 + ny, 0 }, { x1 - nx, y1 - ny, 0 },
		{ x0 - nx, y0 - ny, 0 } };
	u32 colors[4] = { color, color, color, color };
	GRRLIB_NGoneFilled(v, colors, 4);
}

// ws4kp's drawPath(): a black shadow, whose points after the first drop 2px,
// then the colored line. Missing values break the line. Round joins stand in
// for the canvas's miter joins, which differ only at sharp corners.
static void draw_series(const Measure *values, size_t count, double min, double max, u32 color)
{
	if (count < 2 || max <= min)
		return;
	f32 x[HOURLY_GRAPH_HOURS], y[HOURLY_GRAPH_HOURS];
	for (size_t i = 0; i < count; i++) {
		x[i] = CHART_LEFT + PLOT_LEFT + (f32)i * (CHART_WIDTH - PLOT_LEFT) / (count - 1);
		y[i] = MAIN_TOP + CHART_HEIGHT - PLOT_PADDING -
		       (f32)((values[i].value - min) / (max - min)) * (CHART_HEIGHT - 2 * PLOT_PADDING);
	}
	for (int pass = 0; pass < 2; pass++) {
		bool shadow = pass == 0;
		f32 width = shadow ? SHADOW_WIDTH : LINE_WIDTH;
		u32 stroke = shadow ? RGBA_BLACK : color;
		for (size_t i = 1; i < count; i++) {
			if (!values[i - 1].valid || !values[i].valid)
				continue;
			f32 drop0 = shadow && i > 1 ? SHADOW_DROP : 0, drop1 = shadow ? SHADOW_DROP : 0;
			segment(x[i - 1], y[i - 1] + drop0, x[i], y[i] + drop1, width, stroke);
			if (i + 1 < count && values[i + 1].valid)
				GRRLIB_Circle(x[i], y[i] + drop1, width / 2, stroke, true);
		}
	}
}

static void draw_legend(Units units)
{
	char temperature[16], dewpoint[16];
	snprintf(temperature, sizeof temperature, "Temperature \xb0%s", units_temperature_name(units));
	snprintf(dewpoint, sizeof dewpoint, "Dewpoint \xb0%s", units_temperature_name(units));
	const struct {
		const char *text;
		u32 color;
	} lines[] = {
		{ temperature, RGBA_RED },
		{ dewpoint, RGBA_GREEN },
		{ "Cloud %", RGBA_LIGHT_GREY },
		{ "Precip %", RGBA_AQUA },
	};
	f32 line_height = gfx_line_height(legend_style.font);
	for (size_t i = 0; i < sizeof lines / sizeof lines[0]; i++) {
		TextStyle style = legend_style;
		style.color = lines[i].color;
		f32 top = LEGEND_TOP + LEGEND_MARGIN_TOP + i * (line_height + LEGEND_MARGIN_TOP);
		gfx_text(&style, LEGEND_RIGHT, top, ALIGN_RIGHT, lines[i].text);
	}
}

static void draw_labels(const HourlyGraph *g)
{
	f32 line_height = gfx_line_height(label_style.font);
	if (g->has_temperature_axis) {
		// The first two labels are placed by their tops, the others by their
		// bottoms, so margin-top moves only the first two.
		const f32 tops[HOURLY_GRAPH_Y_LABELS] = {
			MAIN_TOP + LABEL_MARGIN_TOP,
			MAIN_TOP + 280.0f / 3 + LABEL_MARGIN_TOP,
			MAIN_TOP + CHART_HEIGHT - (280.0f / 3 - 11) - line_height,
			MAIN_TOP + CHART_HEIGHT - line_height,
		};
		for (size_t i = 0; i < HOURLY_GRAPH_Y_LABELS; i++)
			gfx_text(&label_style, CHART_LEFT, roundf(tops[i]), ALIGN_RIGHT, g->y_labels[i]);
	}
	f32 x_top = MAIN_TOP + MAIN_HEIGHT - X_AXIS_HEIGHT + LABEL_MARGIN_TOP;
	for (size_t i = 0; i < HOURLY_GRAPH_X_LABELS; i++) {
		f32 x = X_AXIS_LEFT + (f32)CHART_WIDTH * i / (HOURLY_GRAPH_X_LABELS - 1);
		gfx_text(&label_style, x, x_top, ALIGN_CENTER, g->x_labels[i]);
	}
}

void hourly_graph_view_draw(const HourlyGraphView *view)
{
	const HourlyGraph *g = &view->graph;
	draw_legend(g->units);

	// Last drawn is on top, as in ws4kp.
	gfx_clip(CHART_LEFT, MAIN_TOP, CHART_WIDTH, CHART_HEIGHT);
	draw_series(g->sky_cover, g->count, 0, 100, RGBA_LIGHT_GREY);
	draw_series(g->probability_of_precipitation, g->count, 0, 100, RGBA_AQUA);
	if (g->has_temperature_axis) {
		draw_series(g->dewpoint, g->count, g->temperature_min, g->temperature_max, RGBA_GREEN);
		draw_series(g->temperature, g->count, g->temperature_min, g->temperature_max, RGBA_RED);
	}

	// The main area hides what its labels' negative margins push above it.
	gfx_clip(0, MAIN_TOP, SCREEN_WIDTH, MAIN_HEIGHT);
	draw_labels(g);
	gfx_clip_reset();
}
