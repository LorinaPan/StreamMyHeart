#include "graph_render_model.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPixelPerHr = 2.0f;

std::vector<float> generateEcgWaveform(int heartRate, int width)
{
	std::vector<float> waveform(width, 0.0f);
	int numCycles = (heartRate - 50) / 20 + 1;
	float cycleLength = width / static_cast<float>(numCycles);

	for (int i = 0; i < width; i++) {
		float pos = fmod(i, cycleLength) / cycleLength;
		if (pos > 0.1f && pos < 0.2f) {
			waveform[i] = 0.05f * sin((pos - 0.15f) * M_PI * 10);
		} else if (pos > 0.3f && pos < 0.32f) {
			waveform[i] = -0.1f;
		} else if (pos > 0.35f && pos < 0.37f) {
			waveform[i] = 0.6f;
		} else if (pos > 0.4f && pos < 0.42f) {
			waveform[i] = -0.2f;
		} else if (pos > 0.6f && pos < 0.8f) {
			waveform[i] = 0.1f * sin((pos - 0.7f) * M_PI * 5);
		}
	}

	return waveform;
}

void appendHeartRate(std::vector<int> &history, int graphSize, int currentHeartRate)
{
	if (currentHeartRate <= 0 || graphSize <= 0) {
		return;
	}

	while (history.size() >= static_cast<size_t>(graphSize)) {
		history.erase(history.begin());
	}
	history.push_back(currentHeartRate);
}

GraphPolyline makePolyline(uint32_t colour, std::vector<std::pair<float, float>> points)
{
	GraphPolyline polyline;
	polyline.colour = colour;
	polyline.points = std::move(points);
	return polyline;
}
} // namespace

GraphRenderFrame GraphRenderState::update(const GraphRenderSettings &settings, int currentHeartRate)
{
	GraphRenderFrame frame;
	if (settings.width == 0 || settings.height == 0 || settings.graphSize == 0) {
		return frame;
	}

	appendHeartRate(heartRateHistory_, settings.graphSize, currentHeartRate);

	if (settings.ecg) {
		frame.fillBackground = true;
		frame.backgroundColour = abgrToArgb(settings.ecgBackgroundColour);

		float baseHeight = settings.height / 2.0f;
		waveOffset_ += 6.0f;

		if (heartRateHistory_.size() >= 2) {
			if (ecgWaves_.empty()) {
				ecgWaves_.push_back(generateEcgWaveform(
					heartRateHistory_[heartRateHistory_.size() - 2], static_cast<int>(settings.width)));
				ecgWaves_.push_back(generateEcgWaveform(
					heartRateHistory_[heartRateHistory_.size() - 1], static_cast<int>(settings.width)));
			} else if (waveOffset_ >= settings.width) {
				waveOffset_ -= settings.width;
				ecgWaves_.erase(ecgWaves_.begin());
				ecgWaves_.push_back(generateEcgWaveform(
					heartRateHistory_[heartRateHistory_.size() - 1], static_cast<int>(settings.width)));
			}
		}

		if (ecgWaves_.size() < 2) {
			return frame;
		}

		std::vector<std::pair<float, float>> points;
		for (size_t i = 0; i < settings.width; i++) {
			float value;
			if (i + static_cast<size_t>(waveOffset_) < static_cast<size_t>(settings.width)) {
				value = ecgWaves_[0][i + static_cast<size_t>(waveOffset_)];
			} else {
				size_t shiftedIndex = i + static_cast<size_t>(waveOffset_) - settings.width;
				value = ecgWaves_[1][shiftedIndex];
			}

			float y = baseHeight - (value * settings.height * 0.4f);
			points.push_back({static_cast<float>(i), y});
		}

		frame.polylines.push_back(makePolyline(abgrToArgb(settings.ecgLineColour), std::move(points)));
		return frame;
	}

	if (settings.backgroundMode == GraphBackgroundMode::COLOURED_TIERS) {
		struct Zone {
			int minHr;
			int maxHr;
			uint32_t colour;
		};
		Zone heartRateZones[] = {
			{50, 90, 0xFF00FF00},
			{90, 120, 0xFFFFFF00},
			{120, 150, 0xFFFFA500},
			{150, 180, 0xFFFF0000},
		};

		for (const Zone &zone : heartRateZones) {
			float top =
				settings.height - (static_cast<float>(zone.maxHr - 50) / 260.0f) * settings.height * 2;
			float bottom =
				settings.height - (static_cast<float>(zone.minHr - 50) / 260.0f) * settings.height * 2;
			frame.backgroundBands.push_back({top, bottom, zone.colour});
		}
	} else if (settings.backgroundMode == GraphBackgroundMode::CUSTOM_COLOUR) {
		frame.fillBackground = true;
		frame.backgroundColour = abgrToArgb(settings.graphPlaneColour);
	}

	if (heartRateHistory_.size() < 3) {
		return frame;
	}

	std::vector<std::pair<float, float>> linePoints;
	for (size_t i = 0; i < heartRateHistory_.size() - 1; i++) {
		float x = (static_cast<float>(i) / (settings.graphSize - 1)) * settings.width;
		float y = settings.height - std::clamp(std::round((static_cast<float>(heartRateHistory_[i] - 50)) *
								  kPixelPerHr),
					  0.0f, static_cast<float>(settings.height));
		linePoints.push_back({x, y});
		if (i == heartRateHistory_.size() - 2) {
			linePoints.push_back({static_cast<float>(settings.width), y});
		}
	}
	frame.polylines.push_back(makePolyline(abgrToArgb(settings.graphLineColour), std::move(linePoints)));

	frame.polylines.push_back(makePolyline(abgrToArgb(settings.graphLineColour),
						       {{0.0f, static_cast<float>(settings.height)},
							{static_cast<float>(settings.width),
							 static_cast<float>(settings.height)}}));
	frame.polylines.push_back(makePolyline(abgrToArgb(settings.graphLineColour),
						       {{0.0f, 0.0f}, {0.0f, static_cast<float>(settings.height)}}));

	for (float i = 10; i <= std::min(130.0f, settings.height / kPixelPerHr); i += 20) {
		float y = settings.height - i * kPixelPerHr;
		frame.polylines.push_back(makePolyline(
			abgrToArgb(settings.graphLineColour), {{0.0f, y}, {5.0f, y}}));
	}

	return frame;
}

uint32_t abgrToArgb(uint32_t abgrColour)
{
	return (abgrColour & 0xFF000000) | ((abgrColour & 0xFF) << 16) | (abgrColour & 0xFF00) |
	       ((abgrColour & 0xFF0000) >> 16);
}
