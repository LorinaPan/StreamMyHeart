#ifndef HEART_RATE_SOURCE_H
#define HEART_RATE_SOURCE_H

#include <obs-module.h>
#include "core/frame_data.h"

#ifdef __cplusplus
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "algorithm/face_detection/face_detection.h"
#include "core/heart_rate_pipeline.h"
#include "frame_capture.h"
#else
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MONITOR_SOURCE_NAME obs_module_text("HeartRateMonitor")
#define TEXT_SOURCE_NAME obs_module_text("HeartRateDisplay")
#define GRAPH_SOURCE_NAME obs_module_text("HeartRateGraph")
#define IMAGE_SOURCE_NAME obs_module_text("HeartRateIcon")
#define MOOD_SOURCE_NAME obs_module_text("HeartRateMood")
#define ECG_SOURCE_NAME obs_module_text("HeartRateECG")

extern bool enableTiming;

#ifdef __cplusplus
enum class AnalysisSnapshotState {
	Idle,
	Calibrating,
	NoFace,
	Ready,
};

struct CapturedFrameSnapshot {
	std::vector<uint8_t> pixels;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t linesize = 0;
	uint64_t captureTimestampNs = 0;
	uint64_t frameId = 0;
};

struct AnalysisResultSnapshot {
	AnalysisSnapshotState state = AnalysisSnapshotState::Idle;
	int heartRate = -1;
	std::string heartRateText;
	std::string moodText;
	std::vector<struct vec4> faceCoordinates;
	uint64_t sourceFrameTimestampNs = 0;
	uint64_t publishedTimestampNs = 0;
	uint64_t frameId = 0;
};

#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
struct PerfAccumulator {
	uint64_t sampleCount = 0;
	uint64_t totalNs = 0;
	uint64_t maxNs = 0;
};

struct MonitorPerfStats {
	bool wasEnabled = false;
	uint64_t windowStartNs = 0;
	uint64_t renderCount = 0;
	uint64_t analysisCount = 0;
	uint64_t noFaceCount = 0;
	PerfAccumulator render;
	PerfAccumulator capture;
	PerfAccumulator faceDetection;
	PerfAccumulator pipeline;
};
#endif
#endif

struct heartRateSource {
	obs_source_t *source;
	gs_effect_t *testing;
#ifdef __cplusplus
	FilterFrameCapture frameCapture;
	std::unique_ptr<FaceDetection> faceDetection;
	HeartRatePipeline pipeline;
	std::unique_ptr<FaceDetection> asyncFaceDetection;
	HeartRatePipeline asyncPipeline;
	std::mutex analysisMutex;
	std::condition_variable analysisCondition;
	std::thread analysisThread;
	CapturedFrameSnapshot pendingFrame;
	AnalysisResultSnapshot analysisResult;
	bool stopAnalysisThread = false;
	bool analysisPaused = true;
	bool captureRequested = false;
	bool hasPendingFrame = false;
	bool workerBusy = false;
	uint64_t nextCaptureDueNs = 0;
	uint64_t nextFrameId = 0;
#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
	MonitorPerfStats perfStats;
#endif
#else
	void *frameCapture;
	void *faceDetection;
	void *pipeline;
#endif
	bool isDisabled;
	int frameCount;
};

// Function declarations
const char *getHeartRateSourceName(void *);
void *heartRateSourceCreate(obs_data_t *settings, obs_source_t *source);
void heartRateSourceDestroy(void *data);
void heartRateSourceDefaults(obs_data_t *settings);
obs_properties_t *heartRateSourceProperties(void *data);
void heartRateSourceActivate(void *data);
void heartRateSourceDeactivate(void *data);
void heartRateSourceTick(void *data, float seconds);
void heartRateSourceRender(void *data, gs_effect_t *effect);

#ifdef __cplusplus
}
#endif

#endif // HEART_RATE_SOURCE_H
