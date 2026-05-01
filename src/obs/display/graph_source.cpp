#include <obs-module.h>
#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-source.h>
#include <obs-data.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include "plugin-support.h"
#include "graph_source.h"
#include "graph_source_info.h"
#include "heart_rate_source.h"
#include "plugin_config.h"

#define LINE_THICKNESS 3.0f
#define UPDATE_FREQUENCY 15

// Destroy function for graph source
void destroyGraphSource(void *data)
{
	struct graph_source *graph = reinterpret_cast<struct graph_source *>(data);

	if (graph) {
		graph->isDisabled = true;
		// Release the OBS source
		if (graph->source) {
			obs_source_release(graph->source);
			graph->source = nullptr;
		};

		// Free memory
		bfree(graph);
	}
}

static bool findHeartRateMonitorFilter(void *param, obs_source_t *source)
{
	const char *filterName = MONITOR_SOURCE_NAME;

	// Try to get the filter from the current source
	obs_source_t *filter = obs_source_get_filter_by_name(source, filterName);

	if (filter) {
		// Store the filter reference in param
		*(obs_source_t **)param = filter;
		return false; // Stop further enumeration
	}

	return true; // Continue searching
}

obs_source_t *getHeartRateMonitorFilter()
{
	obs_source_t *found_filter = nullptr;

	// Enumerate all sources to find the desired filter
	obs_enum_sources(findHeartRateMonitorFilter, &found_filter);

	return found_filter; // Return the filter reference (must be released later)
}

void graphSourceRender(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct graph_source *graphSource = reinterpret_cast<struct graph_source *>(data);
	if (!graphSource || !graphSource->source || graphSource->isDisabled) {
		return; // Ensure graphSource is valid
	}

	int curHeartRate = -1;

	if (graphSource->ecg || graphSource->frameCounter % UPDATE_FREQUENCY == 0) {
		// Retrieve OBS settings for the heart rate monitor source
		obs_source_t *heartRateSource = getHeartRateMonitorFilter();
		if (!heartRateSource) {
			return;
		}
		obs_data_t *hrsSettings = obs_source_get_settings(heartRateSource);

		if (obs_data_get_bool(hrsSettings, "is disabled")) {
			obs_data_release(hrsSettings);
			obs_source_release(heartRateSource);
			return;
		}
		curHeartRate = obs_data_get_int(hrsSettings, "heart rate"); // Retrieve heart rate
		obs_data_release(hrsSettings);
		obs_source_release(heartRateSource);
	}
	graphSource->frameCounter++;

	// Draw the graph using the retrieved heart rate
	drawGraph(graphSource, curHeartRate, graphSource->ecg);
}

static void thickenLines(const std::vector<std::pair<float, float>> &points)
{
	gs_render_start(GS_LINESTRIP);
	for (size_t i = 0; i < points.size() - 1; i++) {
		float x1 = points[i].first;
		float y1 = points[i].second;
		float x2 = points[i + 1].first;
		float y2 = points[i + 1].second;
		// Compute direction of the segment
		float dx = x2 - x1;
		float dy = y2 - y1;
		float length = std::sqrt(dx * dx + dy * dy);
		if (length == 0)
			continue;
		// Compute perpendicular vector (normal)
		float nx = -dy / length;
		float ny = dx / length;
		// Offset points perpendicular to the line
		for (float offset = -LINE_THICKNESS / 2; offset <= LINE_THICKNESS / 2; offset += 0.1f) {
			gs_vertex2f(x1 + nx * offset, y1 + ny * offset);
			gs_vertex2f(x2 + nx * offset, y2 + ny * offset);
		}
	}

	gs_render_stop(GS_LINESTRIP);
}

void drawGraph(struct graph_source *graphSource, int curHeartRate, bool ecg)
{
	if (!graphSource || !graphSource->source)
		return; // Null check to avoid crashes

	// Retrieve source width and height
	uint32_t width = obs_source_get_width(graphSource->source);
	uint32_t height = obs_source_get_height(graphSource->source);
	obs_source_t *heartRateSource = getHeartRateMonitorFilter();
	if (!heartRateSource) {
		obs_log(LOG_INFO, "Failed to get heart rate source");
		return;
	}
	obs_data_t *hrsSettings = obs_source_get_settings(heartRateSource);
	MonitorRuntimeConfig config = readMonitorRuntimeConfig(hrsSettings);
	GraphRenderSettings renderSettings;
	renderSettings.width = width;
	renderSettings.height = height;
	renderSettings.ecg = ecg;
	renderSettings.graphSize = config.displayScene.heartRateGraphSize;
	renderSettings.backgroundMode = config.displayScene.graphBackgroundMode;
	renderSettings.graphPlaneColour = config.displayScene.graphPlaneColour;
	renderSettings.graphLineColour = config.displayScene.graphLineColour;
	renderSettings.ecgLineColour = config.displayScene.ecgLineColour;
	renderSettings.ecgBackgroundColour = config.displayScene.ecgBackgroundColour;

	if (width == 0 || height == 0 || renderSettings.graphSize == 0) {
		obs_data_release(hrsSettings);
		obs_source_release(heartRateSource);
		return; // Avoid division by zero
	}

	obs_enter_graphics();

	// Ensure no conflicting active effects
	gs_effect_t *activeEffect = gs_get_effect();
	if (activeEffect) {
		gs_technique_t *activeTechnique = gs_effect_get_current_technique(activeEffect);
		gs_technique_end(activeTechnique);
		gs_effect_destroy(activeEffect);
	}

	// Get base effect
	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_SOLID);
	GraphRenderFrame frame = graphSource->renderState.update(renderSettings, curHeartRate);
	while (gs_effect_loop(effect, "Solid")) {
		if (frame.fillBackground) {
			gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), frame.backgroundColour);
			gs_draw_sprite(nullptr, 0, width, height);
		}

		for (const GraphBackgroundBand &band : frame.backgroundBands) {
			gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), band.colour);
			gs_matrix_push();
			gs_matrix_translate3f(0, band.top, 0);
			gs_draw_sprite(nullptr, 0, width, band.bottom - band.top);
			gs_matrix_pop();
		}

		for (const GraphPolyline &polyline : frame.polylines) {
			gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), polyline.colour);
			thickenLines(polyline.points);
		}
	}

	obs_data_release(hrsSettings);
	obs_source_release(heartRateSource);

	obs_leave_graphics();
}

const char *getGraphSourceName(void *)
{
	return GRAPH_SOURCE_NAME;
}

const char *getECGSourceName(void *)
{
	return ECG_SOURCE_NAME;
}

static void *createGeneralGraphSourceInfo(obs_source_t *source, bool ecg)
{
	void *data = bmalloc(sizeof(struct graph_source));
	struct graph_source *graphSrc = new (data) graph_source();
	graphSrc->source = source;
	if (!source) {
		obs_log(LOG_INFO, "current source in create graph source is null");
		if (graphSrc) {
			bfree(graphSrc);
		}
		return nullptr;
	}

	graphSrc->isDisabled = false;
	graphSrc->ecg = ecg;
	graphSrc->frameCounter = 0;

	return graphSrc;
}

void *createGraphSourceInfo(obs_data_t *settings, obs_source_t *source)
{
	return createGeneralGraphSourceInfo(source, false);
}

void *createECGSourceInfo(obs_data_t *settings, obs_source_t *source)
{
	return createGeneralGraphSourceInfo(source, true);
}

uint32_t graphSourceInfoGetWidth(void *data)
{
	UNUSED_PARAMETER(data);
	obs_source_t *hrs = getHeartRateMonitorFilter();
	if (!hrs) {
		return 0;
	}
	obs_data_t *settings = obs_source_get_settings(hrs);
	int size = readMonitorRuntimeConfig(settings).displayScene.heartRateGraphSize;
	obs_data_release(settings);
	obs_source_release(hrs);
	if (size < 20) {
		return 260;
	} else if (size < 30) {
		return 520;
	} else {
		return 780;
	}
}
uint32_t graphSourceInfoGetHeight(void *data)
{
	UNUSED_PARAMETER(data);
	return 260;
}

uint32_t ecgSourceInfoGetWidth(void *data)
{
	UNUSED_PARAMETER(data);
	return 260;
}

void graphSourceActivate(void *data)
{
	struct graph_source *graphSource = reinterpret_cast<graph_source *>(data);
	graphSource->isDisabled = false;
}

void graphSourceDeactivate(void *data)
{
	struct graph_source *graphSource = reinterpret_cast<graph_source *>(data);
	graphSource->isDisabled = true;
}
