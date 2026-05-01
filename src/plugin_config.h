#ifndef STREAM_MY_HEART_PLUGIN_CONFIG_H
#define STREAM_MY_HEART_PLUGIN_CONFIG_H

#include "algorithm/face_detection/face_detection.h"
#include "core/heart_rate_pipeline.h"

#include <obs-data.h>
#include <obs-properties.h>

#include <string>

enum class GraphBackgroundMode {
	CLEAR = 0,
	COLOURED_TIERS = 1,
	CUSTOM_COLOUR = 2,
};

struct DisplaySceneConfig {
	bool enableTextSource = true;
	bool enableGraphSource = false;
	bool enableImageSource = false;
	bool enableMoodSource = false;
	bool enableEcgSource = false;
	std::string heartRateText = "Heart rate: {hr} BPM";
	int heartRate = -1;
	int heartRateGraphSize = 10;
	int graphLineColour = 0xFF0000FF;
	GraphBackgroundMode graphBackgroundMode = GraphBackgroundMode::CLEAR;
	int graphPlaneColour = 0xFFFFFFFF;
	int ecgLineColour = 0xFF0000FF;
	int ecgBackgroundColour = 0x00FFFFFF;
};

struct FaceDetectionConfig {
	FaceDetectionAlgorithm algorithm = FaceDetectionAlgorithm::DLIB;
	bool enableDebugBoxes = false;
	bool enableTracker = true;
	int frameUpdateInterval = 60;
};

struct MonitorRuntimeConfig {
	int fps = 30;
	bool isDisabled = false;
	HeartRatePipelineConfig pipeline;
	FaceDetectionConfig faceDetection;
	DisplaySceneConfig displayScene;
};

MonitorRuntimeConfig readMonitorRuntimeConfig(obs_data_t *settings);
void applyMonitorDefaults(obs_data_t *settings);
void updateDisplayPropertyVisibility(obs_properties_t *props, const DisplaySceneConfig &config);
void updateAlgorithmPropertyVisibility(obs_properties_t *props, const FaceDetectionConfig &config);

#endif
