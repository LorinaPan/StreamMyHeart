#include "algorithm/face_detection/face_detection.h"
#include "algorithm/face_detection/opencv_haarcascade.h"
#include "algorithm/face_detection/opencv_dlib_68_landmarks_face_tracker.h"
#include "core/heart_rate_pipeline.h"
#include "display_scene.h"
#include "heart_rate_source.h"
#include "plugin-support.h"
#include "plugin_config.h"

#include <obs-module.h>
#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-source.h>
#include <obs-data.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <util/platform.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

bool enableTiming = false;

namespace {
constexpr uint64_t kAnalysisCadenceNs = 1000000000ULL / 15ULL;

bool shouldUseAsyncDlib(const MonitorRuntimeConfig &config)
{
	return config.enableExperimentalAsyncAnalysis &&
	       config.faceDetection.algorithm == FaceDetectionAlgorithm::DLIB;
}

void publishIdleSnapshot(struct heartRateSource *hrs)
{
	std::lock_guard<std::mutex> lock(hrs->analysisMutex);
	hrs->analysisResult = {};
	hrs->analysisResult.state = AnalysisSnapshotState::Idle;
	hrs->analysisResult.heartRateText = "Calibrating...";
	hrs->analysisResult.moodText = "Calibrating...";
	hrs->analysisResult.publishedTimestampNs = os_gettime_ns();
}

void analysisWorkerLoop(struct heartRateSource *hrs)
{
	std::unique_lock<std::mutex> lock(hrs->analysisMutex);
	while (!hrs->stopAnalysisThread) {
		hrs->analysisCondition.wait(lock, [hrs] {
			return hrs->stopAnalysisThread || (!hrs->analysisPaused && hrs->hasPendingFrame);
		});
		if (hrs->stopAnalysisThread) {
			break;
		}

		// Stage shell only. Real async analysis lands in next stage.
		hrs->hasPendingFrame = false;
		hrs->workerBusy = false;
	}
}
} // namespace

#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
namespace {
constexpr uint64_t kPerfLogIntervalNs = 1000000000ULL;

void recordPerfSample(PerfAccumulator &accumulator, uint64_t durationNs)
{
	accumulator.sampleCount += 1;
	accumulator.totalNs += durationNs;
	accumulator.maxNs = std::max(accumulator.maxNs, durationNs);
}

double toMilliseconds(uint64_t durationNs)
{
	return static_cast<double>(durationNs) / 1000000.0;
}

void resetPerfStats(MonitorPerfStats &stats)
{
	stats.windowStartNs = 0;
	stats.renderCount = 0;
	stats.analysisCount = 0;
	stats.noFaceCount = 0;
	stats.render = {};
	stats.capture = {};
	stats.faceDetection = {};
	stats.pipeline = {};
}

void maybeLogPerfStats(struct heartRateSource *hrs, const MonitorRuntimeConfig &config)
{
	if (!config.enablePerfInstrumentation) {
		if (hrs->perfStats.wasEnabled) {
			resetPerfStats(hrs->perfStats);
			hrs->perfStats.wasEnabled = false;
		}
		return;
	}

	uint64_t nowNs = os_gettime_ns();
	MonitorPerfStats &stats = hrs->perfStats;
	if (stats.windowStartNs == 0) {
		stats.windowStartNs = nowNs;
		stats.wasEnabled = true;
		return;
	}

	uint64_t elapsedNs = nowNs - stats.windowStartNs;
	if (elapsedNs < kPerfLogIntervalNs) {
		stats.wasEnabled = true;
		return;
	}

	auto averageNs = [](const PerfAccumulator &accumulator) -> uint64_t {
		if (accumulator.sampleCount == 0) {
			return 0;
		}
		return accumulator.totalNs / accumulator.sampleCount;
	};

	double analysisHz = elapsedNs == 0 ? 0.0
					   : (static_cast<double>(stats.analysisCount) * 1000000000.0) /
						     static_cast<double>(elapsedNs);
	double renderHz = elapsedNs == 0 ? 0.0
					 : (static_cast<double>(stats.renderCount) * 1000000000.0) /
						   static_cast<double>(elapsedNs);
	obs_log(LOG_INFO,
		"[perf] render avg=%.3f ms max=%.3f ms samples=%llu | capture avg=%.3f ms max=%.3f ms "
		"samples=%llu | detect avg=%.3f ms max=%.3f ms samples=%llu | pipeline avg=%.3f ms "
		"max=%.3f ms samples=%llu | render_hz=%.2f | analysis_hz=%.2f | no_face=%llu",
		toMilliseconds(averageNs(stats.render)), toMilliseconds(stats.render.maxNs),
		static_cast<unsigned long long>(stats.render.sampleCount), toMilliseconds(averageNs(stats.capture)),
		toMilliseconds(stats.capture.maxNs), static_cast<unsigned long long>(stats.capture.sampleCount),
		toMilliseconds(averageNs(stats.faceDetection)), toMilliseconds(stats.faceDetection.maxNs),
		static_cast<unsigned long long>(stats.faceDetection.sampleCount),
		toMilliseconds(averageNs(stats.pipeline)), toMilliseconds(stats.pipeline.maxNs),
		static_cast<unsigned long long>(stats.pipeline.sampleCount), renderHz, analysisHz,
		static_cast<unsigned long long>(stats.noFaceCount));

	resetPerfStats(stats);
	stats.windowStartNs = nowNs;
	stats.wasEnabled = true;
}
} // namespace
#endif

const char *getHeartRateSourceName(void *)
{
	return obs_module_text("HeartRateMonitor");
}

// Create function
void *heartRateSourceCreate(obs_data_t *settings, obs_source_t *source)
{
	void *data = bmalloc(sizeof(struct heartRateSource));
	struct heartRateSource *hrs = new (data) heartRateSource();

	hrs->source = source;

	obs_enter_graphics();
	char *effectFile = obs_module_file("test.effect");

	hrs->testing = gs_effect_create_from_file(effectFile, NULL);

	bfree(effectFile);
	if (!hrs->testing) {
		heartRateSourceDestroy(hrs);
		hrs = NULL;
	}
	obs_leave_graphics();

	hrs->frameCapture.initialize();
	MonitorRuntimeConfig config = readMonitorRuntimeConfig(settings);
	reconcileDisplayScene(config.displayScene);

	hrs->faceDetection = FaceDetection::create(config.faceDetection.algorithm);
	hrs->asyncFaceDetection = FaceDetection::create(FaceDetectionAlgorithm::DLIB);
	hrs->frameCount = 0;
	hrs->analysisPaused = true;
	publishIdleSnapshot(hrs);
	hrs->analysisThread = std::thread(analysisWorkerLoop, hrs);

	return hrs;
}

// Destroy function
void heartRateSourceDestroy(void *data)
{
	struct heartRateSource *hrs = reinterpret_cast<struct heartRateSource *>(data);

	removeDisplaySceneSources();

	if (hrs) {
		hrs->isDisabled = true;
		{
			std::lock_guard<std::mutex> lock(hrs->analysisMutex);
			hrs->stopAnalysisThread = true;
			hrs->analysisPaused = true;
			hrs->hasPendingFrame = false;
			hrs->captureRequested = false;
		}
		hrs->analysisCondition.notify_all();
		if (hrs->analysisThread.joinable()) {
			hrs->analysisThread.join();
		}
		obs_enter_graphics();
		hrs->frameCapture.destroy();
		gs_effect_destroy(hrs->testing);
		obs_leave_graphics();
		hrs->~heartRateSource();
		bfree(hrs);
	}
}

void heartRateSourceDefaults(obs_data_t *settings)
{
	applyMonitorDefaults(settings);
}

static bool updateProperties(obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	MonitorRuntimeConfig config = readMonitorRuntimeConfig(settings);
	updateAlgorithmPropertyVisibility(props, config.faceDetection);
	updateDisplayPropertyVisibility(props, config.displayScene);
	reconcileDisplayScene(config.displayScene);

	if (config.displayScene.heartRate > 0) {
		std::string textFormat = config.displayScene.heartRateText;
		size_t pos = textFormat.find("{hr}");
		if (pos != std::string::npos) {
			textFormat.replace(pos, 4, std::to_string(config.displayScene.heartRate));
		}
		updateDisplaySceneText(textFormat, "");
	}

	return true; // Forces the UI to refresh
}

static obs_properties_t *algorithmProperties()
{
	obs_properties_t *props = obs_properties_create();
	// Set the face detection algorithm
	obs_property_t *dropdown = obs_properties_add_list(props, "face detection algorithm",
							   obs_module_text("FaceDetectionAlgorithm"),
							   OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(dropdown, obs_module_text("HaarCascade"), 0);
	obs_property_list_add_int(dropdown, obs_module_text("Dlib"), 1);

	// Allow user to disable face detection boxes drawing
	obs_properties_add_bool(props, "face detection debug boxes", obs_module_text("FaceDetectionDebugBoxes"));

	// Set if enable face tracking
	obs_property_t *enableTracker =
		obs_properties_add_bool(props, "enable face tracking", obs_module_text("FaceTrackerEnable"));
	obs_properties_add_text(props, "face tracking explain", obs_module_text("FaceTrackerExplain"), OBS_TEXT_INFO);

	obs_properties_add_int(props, "frame update interval", obs_module_text("FrameUpdateInterval"), 1, 120, 1);
	obs_properties_add_text(props, "frame update interval explain", obs_module_text("FrameUpdateIntervalExplain"),
				OBS_TEXT_INFO);

	// Add dropdown for selecting PPG algorithm
	obs_property_t *ppgDropdown = obs_properties_add_list(props, "ppg algorithm", obs_module_text("PPGAlgorithm"),
							      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(ppgDropdown, obs_module_text("GreenChannel"), 0);
	obs_property_list_add_int(ppgDropdown, obs_module_text("PCA"), 1);
	obs_property_list_add_int(ppgDropdown, obs_module_text("Chrom"), 2);

	// Add dropdown for pre-filtering methods
	obs_property_t *preFilterDropdown = obs_properties_add_list(props, "pre-filtering method",
								    obs_module_text("PreFilteringAlgorithm"),
								    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(preFilterDropdown, obs_module_text("None"), 0);
	obs_property_list_add_int(preFilterDropdown, obs_module_text("Bandpass"), 1);
	obs_property_list_add_int(preFilterDropdown, obs_module_text("Detrend"), 2);
	obs_property_list_add_int(preFilterDropdown, obs_module_text("ZeroMean"), 3);

	// Add boolean tick box for post-filtering
	obs_properties_add_bool(props, "post-filtering", obs_module_text("PostFilteringAlgorithm"));
#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
	obs_properties_add_bool(props, "enable perf instrumentation", "Enable perf instrumentation");
	obs_properties_add_bool(props, "enable experimental async analysis", "Enable experimental async analysis");
#endif
	obs_property_set_modified_callback(dropdown, updateProperties);
	obs_property_set_modified_callback(enableTracker, updateProperties);
	obs_property_set_modified_callback(ppgDropdown, updateProperties);
	return props;
}

obs_properties_t *heartRateSourceProperties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_int(props, "fps", obs_module_text("fps"), 1, 120, 1);

	obs_property_t *enableText =
		obs_properties_add_bool(props, "enable text source", obs_module_text("TextSourceEnable"));
	obs_property_t *heartRateText =
		obs_properties_add_text(props, "heart rate text", obs_module_text("HeartRateText"), OBS_TEXT_DEFAULT);
	obs_property_t *heartRateTextExplain = obs_properties_add_text(
		props, "heart rate text explain", obs_module_text("HeartRateTextExplain"), OBS_TEXT_INFO);

	obs_property_t *enableImage =
		obs_properties_add_bool(props, "enable image source", obs_module_text("ImageSourceEnable"));
	obs_property_t *enableMood =
		obs_properties_add_bool(props, "enable mood source", obs_module_text("MoodSourceEnable"));

	obs_property_t *enableGraph =
		obs_properties_add_bool(props, "enable graph source", obs_module_text("GraphSourceEnable"));
	obs_property_t *graphLineColour =
		obs_properties_add_color_alpha(props, "graph line colour", obs_module_text("GraphLineColour"));
	obs_property_t *graphPlaneDropdown = obs_properties_add_list(props, "graph plane dropdown",
								     obs_module_text("GraphPlaneDropdown"),
								     OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(graphPlaneDropdown, obs_module_text("Clear"), 0);
	obs_property_list_add_int(graphPlaneDropdown, obs_module_text("ColouredTiers"), 1);
	obs_property_list_add_int(graphPlaneDropdown, obs_module_text("CustomColour"), 2);
	obs_property_t *graphPlaneColour = obs_properties_add_color_alpha(props, "graph plane colour", "");
	obs_property_t *heartRateGraphSize = obs_properties_add_int(
		props, "heart rate graph size", obs_module_text("HeartRateHistoryLength"), 10, 30, 1);
	obs_property_t *heartRateGraphSizeExplain = obs_properties_add_text(
		props, "heart rate graph explain", obs_module_text("HeartRateHistoryLengthExplain"), OBS_TEXT_INFO);

	obs_property_t *enableECG =
		obs_properties_add_bool(props, "enable ecg source", obs_module_text("ECGSourceEnable"));
	obs_property_t *ecgLineColour =
		obs_properties_add_color_alpha(props, "ecg line colour", obs_module_text("ECGLineColour"));
	obs_property_t *ecgBackgroundColour =
		obs_properties_add_color_alpha(props, "ecg background colour", obs_module_text("ECGBackgroundColour"));

	obs_properties_t *algorithmSettings = algorithmProperties();
	obs_properties_add_group(props, "algorithm settings", obs_module_text("AdvanceSettings"), OBS_GROUP_NORMAL,
				 algorithmSettings);

	obs_data_t *settings = obs_source_get_settings((obs_source_t *)data);

	obs_property_set_modified_callback(heartRateText, updateProperties);
	obs_property_set_modified_callback(heartRateTextExplain, updateProperties);
	obs_property_set_modified_callback(enableText, updateProperties);
	obs_property_set_modified_callback(enableGraph, updateProperties);
	obs_property_set_modified_callback(graphPlaneDropdown, updateProperties);
	obs_property_set_modified_callback(graphPlaneColour, updateProperties);
	obs_property_set_modified_callback(graphLineColour, updateProperties);
	obs_property_set_modified_callback(enableImage, updateProperties);
	obs_property_set_modified_callback(enableMood, updateProperties);
	obs_property_set_modified_callback(enableECG, updateProperties);
	obs_property_set_modified_callback(ecgLineColour, updateProperties);
	obs_property_set_modified_callback(ecgBackgroundColour, updateProperties);
	obs_property_set_modified_callback(heartRateGraphSize, updateProperties);
	obs_property_set_modified_callback(heartRateGraphSizeExplain, updateProperties);

	obs_data_release(settings);

	return props;
}

void heartRateSourceActivate(void *data)
{
	struct heartRateSource *hrs = reinterpret_cast<heartRateSource *>(data);
	hrs->isDisabled = false;
	{
		std::lock_guard<std::mutex> lock(hrs->analysisMutex);
		hrs->analysisPaused = false;
		hrs->nextCaptureDueNs = os_gettime_ns();
	}
	hrs->analysisCondition.notify_all();
	obs_data_t *settings = obs_source_get_settings(hrs->source);
	obs_data_set_bool(settings, "is disabled", false);
	obs_data_release(settings);
}

void heartRateSourceDeactivate(void *data)
{
	struct heartRateSource *hrs = reinterpret_cast<heartRateSource *>(data);
	hrs->isDisabled = true;
	{
		std::lock_guard<std::mutex> lock(hrs->analysisMutex);
		hrs->analysisPaused = true;
		hrs->hasPendingFrame = false;
		hrs->captureRequested = false;
	}
	publishIdleSnapshot(hrs);
	obs_data_t *settings = obs_source_get_settings(hrs->source);
	obs_data_set_bool(settings, "is disabled", true);
	obs_data_release(settings);
}

// Tick function
void heartRateSourceTick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	struct heartRateSource *hrs = reinterpret_cast<struct heartRateSource *>(data);

	if (hrs->isDisabled) {
		return;
	}

	if (!obs_source_enabled(hrs->source)) {
		return;
	}

	obs_data_t *settings = obs_source_get_settings(hrs->source);
	MonitorRuntimeConfig config = readMonitorRuntimeConfig(settings);
	obs_data_release(settings);

	if (!shouldUseAsyncDlib(config)) {
		std::lock_guard<std::mutex> lock(hrs->analysisMutex);
		hrs->analysisPaused = true;
		hrs->captureRequested = false;
		return;
	}

	uint64_t nowNs = os_gettime_ns();
	{
		std::lock_guard<std::mutex> lock(hrs->analysisMutex);
		hrs->analysisPaused = false;
		if (hrs->nextCaptureDueNs == 0 || nowNs >= hrs->nextCaptureDueNs) {
			hrs->captureRequested = true;
			hrs->nextCaptureDueNs = nowNs + kAnalysisCadenceNs;
		}
	}
	hrs->analysisCondition.notify_all();
}

static gs_texture_t *drawRectangle(struct heartRateSource *hrs, uint32_t width, uint32_t height,
				   std::vector<struct vec4> &faceCoordinates)
{
	gs_texture_t *blurredTexture = gs_texture_create(width, height, GS_BGRA, 1, nullptr, 0);
	gs_copy_texture(blurredTexture, gs_texrender_get_texture(hrs->frameCapture.texrender()));

	gs_texrender_reset(hrs->frameCapture.texrender());
	if (!gs_texrender_begin(hrs->frameCapture.texrender(), width, height)) {
		obs_log(LOG_INFO, "Could not open background texrender!");
		return blurredTexture;
	}

	gs_effect_set_texture(gs_effect_get_param_by_name(hrs->testing, "image"), blurredTexture);

	std::vector<std::string> params = {"face", "eye_1", "eye_2", "mouth", "detected"};

	for (size_t i = 0; i < std::min(params.size(), faceCoordinates.size()); i++) {
		gs_effect_set_vec4(gs_effect_get_param_by_name(hrs->testing, params[static_cast<int>(i)].c_str()),
				   &faceCoordinates[i]);
	}

	struct vec4 background;
	vec4_zero(&background);
	gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
	gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f, 100.0f);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

	gs_blend_state_pop();
	gs_texrender_end(hrs->frameCapture.texrender());
	gs_copy_texture(blurredTexture, gs_texrender_get_texture(hrs->frameCapture.texrender()));
	return blurredTexture;
}

std::string getMood(int heart_rate)
{
	std::string mood;
	if (heart_rate > 150) {
		mood = "Extremely hyped";
	} else if (heart_rate > 120) {
		mood = "Very Intense";
	} else if (heart_rate > 90) {
		mood = "Excited";
	} else if (heart_rate > 60) {
		mood = "Normal";
	} else {
		mood = "Extremely calm";
	}
	return mood;
}

// Render function
void heartRateSourceRender(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct heartRateSource *hrs = reinterpret_cast<struct heartRateSource *>(data);
	uint64_t renderStartNs = 0;
	uint64_t captureStartNs = 0;
#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
	renderStartNs = os_gettime_ns();
	captureStartNs = renderStartNs;
#endif

	if (!hrs->source) {
		return;
	}

	if (hrs->isDisabled) {
		skipVideoFilterIfSafe(hrs->source);
		return;
	}

	if (!hrs->frameCapture.capture(hrs->source)) {
		skipVideoFilterIfSafe(hrs->source);
		return;
	}

	if (!hrs->testing) {
		obs_log(LOG_INFO, "Effect not loaded");
		// Effect failed to load, skip rendering
		skipVideoFilterIfSafe(hrs->source);
		return;
	}

	obs_data_t *hrsSettings = obs_source_get_settings(hrs->source);
	MonitorRuntimeConfig config = readMonitorRuntimeConfig(hrsSettings);

#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
	hrs->perfStats.renderCount += 1;
#endif

	std::vector<struct vec4> faceCoordinates;
	std::vector<double_t> avg;
	std::shared_ptr<input_BGRA_data> bgraData = hrs->frameCapture.sample();

#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
	recordPerfSample(hrs->perfStats.capture, os_gettime_ns() - captureStartNs);
#endif

	// User has changed face detection algorithm, recreate the face detection object
	if ((config.faceDetection.algorithm == FaceDetectionAlgorithm::HAAR_CASCADE &&
	     dynamic_cast<DlibFaceDetection *>(hrs->faceDetection.get())) ||
	    (config.faceDetection.algorithm == FaceDetectionAlgorithm::DLIB &&
	     dynamic_cast<HaarCascadeFaceDetection *>(hrs->faceDetection.get()))) {
		hrs->faceDetection = FaceDetection::create(config.faceDetection.algorithm);
	}

	if (hrs->faceDetection) {
		uint64_t start_face_detection, end_face_detection;
		if (enableTiming) {
			start_face_detection = os_gettime_ns();
		}
#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
		uint64_t faceDetectionStartNs = os_gettime_ns();
#endif
		avg = hrs->faceDetection->detectFace(bgraData, faceCoordinates,
						     config.faceDetection.enableDebugBoxes,
						     config.faceDetection.enableTracker,
						     config.faceDetection.frameUpdateInterval);
		if (enableTiming) {
			end_face_detection = os_gettime_ns();
			obs_log(LOG_INFO, "Face detection took: %lu ns", end_face_detection - start_face_detection);
		}
#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
		recordPerfSample(hrs->perfStats.faceDetection, os_gettime_ns() - faceDetectionStartNs);
		hrs->perfStats.analysisCount += 1;
#endif
	}

	double heartRate = -1.0;
	bool noFaceDetected = false;
	if (hasFaceSample(avg)) { // face detected
		hrs->frameCount = 0; // reset frame count

#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
		uint64_t pipelineStartNs = os_gettime_ns();
#endif
		heartRate = hrs->pipeline.update(avg, config.pipeline);
#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
		recordPerfSample(hrs->perfStats.pipeline, os_gettime_ns() - pipelineStartNs);
#endif
	} else { // no face detected
		hrs->frameCount += 1;
#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
		hrs->perfStats.noFaceCount += 1;
#endif
		if (hrs->frameCount >= config.fps) { // if no face detected more than 1 second
			noFaceDetected = true;
		}
	}

	std::string heartRateText;
	std::string moodText;

	obs_data_set_int(hrsSettings, "heart rate", static_cast<int>(std::round(heartRate)));
	if (heartRate > 0.0) {

		heartRateText = config.displayScene.heartRateText;
		size_t pos = heartRateText.find("{hr}");
		if (pos != std::string::npos) {
			heartRateText.replace(pos, 4, std::to_string(static_cast<int>(std::round(heartRate))));
		} else {
			heartRateText =
				"Heart rate: " + std::to_string(static_cast<int>(std::round(heartRate))) + " BPM";
		}
		moodText = "Mood: " + getMood(heartRate);
	} else if (noFaceDetected) { // output "No Face Detected"
		heartRateText = "No Face Detected";
		moodText = "No Face Detected";
	} else if (heartRate == -1.0) { // output "Calibrating..."
		heartRateText = "Calibrating...";
		moodText = "Calibrating...";
	}

	if (noFaceDetected || heartRate != 0.0) {
		updateDisplaySceneText(heartRateText, moodText);
	}

	obs_data_release(hrsSettings);

	if (config.faceDetection.enableDebugBoxes) {
		gs_texture_t *testingTexture =
			drawRectangle(hrs, bgraData->width, bgraData->height, faceCoordinates);

		if (!obs_source_process_filter_begin(hrs->source, GS_BGRA, OBS_ALLOW_DIRECT_RENDERING)) {
			skipVideoFilterIfSafe(hrs->source);
			gs_texture_destroy(testingTexture);
			return;
		}
		gs_effect_set_texture(gs_effect_get_param_by_name(hrs->testing, "image"), testingTexture);

		gs_blend_state_push();
		gs_reset_blend_state();

		if (hrs->source) {
			obs_source_process_filter_tech_end(hrs->source, hrs->testing, bgraData->width,
							   bgraData->height, "Draw");
		}

		gs_blend_state_pop();

		gs_texture_destroy(testingTexture);

	} else {
		skipVideoFilterIfSafe(hrs->source);
	}

#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
	recordPerfSample(hrs->perfStats.render, os_gettime_ns() - renderStartNs);
	maybeLogPerfStats(hrs, config);
#endif
}
