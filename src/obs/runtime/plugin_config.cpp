#include "plugin_config.h"

namespace {
constexpr const char *kFps = "fps";
constexpr const char *kFaceDetectionAlgorithm = "face detection algorithm";
constexpr const char *kFaceDetectionDebugBoxes = "face detection debug boxes";
constexpr const char *kEnableFaceTracking = "enable face tracking";
constexpr const char *kFrameUpdateInterval = "frame update interval";
constexpr const char *kPpgAlgorithm = "ppg algorithm";
constexpr const char *kPreFilteringMethod = "pre-filtering method";
constexpr const char *kPostFiltering = "post-filtering";
constexpr const char *kIsDisabled = "is disabled";
constexpr const char *kHeartRate = "heart rate";
constexpr const char *kHeartRateText = "heart rate text";
constexpr const char *kEnableTextSource = "enable text source";
constexpr const char *kEnableGraphSource = "enable graph source";
constexpr const char *kEnableImageSource = "enable image source";
constexpr const char *kEnableMoodSource = "enable mood source";
constexpr const char *kEnableEcgSource = "enable ecg source";
constexpr const char *kGraphLineColour = "graph line colour";
constexpr const char *kGraphPlaneDropdown = "graph plane dropdown";
constexpr const char *kGraphPlaneColour = "graph plane colour";
constexpr const char *kEcgLineColour = "ecg line colour";
constexpr const char *kEcgBackgroundColour = "ecg background colour";
constexpr const char *kHeartRateGraphSize = "heart rate graph size";
constexpr const char *kEnablePerfInstrumentation = "enable perf instrumentation";
constexpr const char *kEnableExperimentalAsyncAnalysis = "enable experimental async analysis";
} // namespace

MonitorRuntimeConfig readMonitorRuntimeConfig(obs_data_t *settings)
{
	MonitorRuntimeConfig config;
	config.fps = static_cast<int>(obs_data_get_int(settings, kFps));
	config.isDisabled = obs_data_get_bool(settings, kIsDisabled);
	config.enablePerfInstrumentation = obs_data_get_bool(settings, kEnablePerfInstrumentation);
	config.enableExperimentalAsyncAnalysis = obs_data_get_bool(settings, kEnableExperimentalAsyncAnalysis);

	config.pipeline.fps = config.fps;
	config.pipeline.ppgAlgorithm = static_cast<PpgAlgorithmMethod>(obs_data_get_int(settings, kPpgAlgorithm));
	config.pipeline.preFiltering =
		static_cast<PreFilteringMethod>(obs_data_get_int(settings, kPreFilteringMethod));
	config.pipeline.postFiltering =
		obs_data_get_bool(settings, kPostFiltering) ? PostFilteringMethod::BANDPASS
							    : PostFilteringMethod::NONE;

	config.faceDetection.algorithm =
		static_cast<FaceDetectionAlgorithm>(obs_data_get_int(settings, kFaceDetectionAlgorithm));
	config.faceDetection.enableDebugBoxes = obs_data_get_bool(settings, kFaceDetectionDebugBoxes);
	config.faceDetection.enableTracker = obs_data_get_bool(settings, kEnableFaceTracking);
	config.faceDetection.frameUpdateInterval =
		static_cast<int>(obs_data_get_int(settings, kFrameUpdateInterval));

	config.displayScene.enableTextSource = obs_data_get_bool(settings, kEnableTextSource);
	config.displayScene.enableGraphSource = obs_data_get_bool(settings, kEnableGraphSource);
	config.displayScene.enableImageSource = obs_data_get_bool(settings, kEnableImageSource);
	config.displayScene.enableMoodSource = obs_data_get_bool(settings, kEnableMoodSource);
	config.displayScene.enableEcgSource = obs_data_get_bool(settings, kEnableEcgSource);
	config.displayScene.heartRateText = obs_data_get_string(settings, kHeartRateText);
	config.displayScene.heartRate = static_cast<int>(obs_data_get_int(settings, kHeartRate));
	config.displayScene.heartRateGraphSize =
		static_cast<int>(obs_data_get_int(settings, kHeartRateGraphSize));
	config.displayScene.graphLineColour = static_cast<int>(obs_data_get_int(settings, kGraphLineColour));
	config.displayScene.graphBackgroundMode =
		static_cast<GraphBackgroundMode>(obs_data_get_int(settings, kGraphPlaneDropdown));
	config.displayScene.graphPlaneColour = static_cast<int>(obs_data_get_int(settings, kGraphPlaneColour));
	config.displayScene.ecgLineColour = static_cast<int>(obs_data_get_int(settings, kEcgLineColour));
	config.displayScene.ecgBackgroundColour =
		static_cast<int>(obs_data_get_int(settings, kEcgBackgroundColour));

	return config;
}

void applyMonitorDefaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, kFps, 30);
	obs_data_set_default_int(settings, kFaceDetectionAlgorithm, 1);
	obs_data_set_default_bool(settings, kFaceDetectionDebugBoxes, true);
	obs_data_set_default_bool(settings, kEnableFaceTracking, true);
	obs_data_set_default_int(settings, kFrameUpdateInterval, 60);
	obs_data_set_default_int(settings, kPpgAlgorithm, 2);
	obs_data_set_default_int(settings, kHeartRate, -1);
	obs_data_set_default_string(settings, kHeartRateText, "Heart rate: {hr} BPM");
	obs_data_set_default_bool(settings, kEnableTextSource, true);
	obs_data_set_default_bool(settings, kEnableGraphSource, false);
	obs_data_set_default_bool(settings, kEnableImageSource, false);
	obs_data_set_default_bool(settings, kEnableMoodSource, false);
	obs_data_set_default_bool(settings, kEnableEcgSource, false);
	obs_data_set_default_int(settings, kEcgLineColour, 0xFF0000FF);
	obs_data_set_default_int(settings, kEcgBackgroundColour, 0x00FFFFFF);
	obs_data_set_default_int(settings, kGraphLineColour, 0xFF0000FF);
	obs_data_set_default_int(settings, kGraphPlaneDropdown, 0);
	obs_data_set_default_int(settings, kGraphPlaneColour, 0xFFFFFFFF);
	obs_data_set_default_int(settings, kPreFilteringMethod, 3);
	obs_data_set_default_bool(settings, kPostFiltering, true);
	obs_data_set_default_bool(settings, kIsDisabled, false);
	obs_data_set_default_int(settings, kHeartRateGraphSize, 10);
#ifdef STREAM_MY_HEART_ENABLE_DEBUG_FEATURES
	obs_data_set_default_bool(settings, kEnablePerfInstrumentation, true);
	obs_data_set_default_bool(settings, kEnableExperimentalAsyncAnalysis, false);
#else
	obs_data_set_default_bool(settings, kEnablePerfInstrumentation, false);
	obs_data_set_default_bool(settings, kEnableExperimentalAsyncAnalysis, false);
#endif
}

void updateDisplayPropertyVisibility(obs_properties_t *props, const DisplaySceneConfig &config)
{
	obs_property_set_visible(obs_properties_get(props, kHeartRateText), config.enableTextSource);
	obs_property_set_visible(obs_properties_get(props, "heart rate text explain"), config.enableTextSource);
	obs_property_set_visible(obs_properties_get(props, kGraphLineColour), config.enableGraphSource);
	obs_property_set_visible(obs_properties_get(props, kGraphPlaneDropdown), config.enableGraphSource);
	obs_property_set_visible(obs_properties_get(props, kGraphPlaneColour),
				 config.enableGraphSource &&
					 config.graphBackgroundMode == GraphBackgroundMode::CUSTOM_COLOUR);
	obs_property_set_visible(obs_properties_get(props, kHeartRateGraphSize), config.enableGraphSource);
	obs_property_set_visible(obs_properties_get(props, "heart rate graph explain"),
				 config.enableGraphSource);
	obs_property_set_visible(obs_properties_get(props, kEcgLineColour), config.enableEcgSource);
	obs_property_set_visible(obs_properties_get(props, kEcgBackgroundColour), config.enableEcgSource);
}

void updateAlgorithmPropertyVisibility(obs_properties_t *props, const FaceDetectionConfig &config)
{
	bool isDlibSelected = config.algorithm == FaceDetectionAlgorithm::DLIB;
	obs_property_set_visible(obs_properties_get(props, kEnableFaceTracking), isDlibSelected);
	obs_property_set_visible(obs_properties_get(props, "face tracking explain"), isDlibSelected);
	obs_property_set_visible(obs_properties_get(props, kFrameUpdateInterval),
				 isDlibSelected && config.enableTracker);
	obs_property_set_visible(obs_properties_get(props, "frame update interval explain"),
				 isDlibSelected && config.enableTracker);
}
