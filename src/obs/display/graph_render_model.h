#ifndef STREAM_MY_HEART_GRAPH_RENDER_MODEL_H
#define STREAM_MY_HEART_GRAPH_RENDER_MODEL_H

#include "plugin_config.h"

#include <cstdint>
#include <utility>
#include <vector>

struct GraphPolyline {
	uint32_t colour = 0;
	std::vector<std::pair<float, float>> points;
};

struct GraphBackgroundBand {
	float top = 0.0f;
	float bottom = 0.0f;
	uint32_t colour = 0;
};

struct GraphRenderFrame {
	bool fillBackground = false;
	uint32_t backgroundColour = 0;
	std::vector<GraphBackgroundBand> backgroundBands;
	std::vector<GraphPolyline> polylines;
};

struct GraphRenderSettings {
	uint32_t width = 0;
	uint32_t height = 0;
	bool ecg = false;
	int graphSize = 0;
	GraphBackgroundMode backgroundMode = GraphBackgroundMode::CLEAR;
	int graphPlaneColour = 0xFFFFFFFF;
	int graphLineColour = 0xFF0000FF;
	int ecgLineColour = 0xFF0000FF;
	int ecgBackgroundColour = 0x00FFFFFF;
};

class GraphRenderState {
public:
	GraphRenderFrame update(const GraphRenderSettings &settings, int currentHeartRate);

private:
	std::vector<int> heartRateHistory_;
	std::vector<std::vector<float>> ecgWaves_;
	float waveOffset_ = 0.0f;
};

uint32_t abgrToArgb(uint32_t abgrColour);

#endif
