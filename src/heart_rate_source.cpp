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
#include <vector>
#include <sstream>

bool enableTiming = false;

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

	hrs->texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
	MonitorRuntimeConfig config = readMonitorRuntimeConfig(settings);
	reconcileDisplayScene(config.displayScene);

	hrs->faceDetection = FaceDetection::create(config.faceDetection.algorithm);
	hrs->frameCount = 0;

	return hrs;
}

// Destroy function
void heartRateSourceDestroy(void *data)
{
	struct heartRateSource *hrs = reinterpret_cast<struct heartRateSource *>(data);

	removeDisplaySceneSources();

	if (hrs) {
		hrs->isDisabled = true;
		obs_enter_graphics();
		gs_texrender_destroy(hrs->texrender);
		if (hrs->stagesurface) {
			gs_stagesurface_destroy(hrs->stagesurface);
		}
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
	obs_data_t *settings = obs_source_get_settings(hrs->source);
	obs_data_set_bool(settings, "is disabled", false);
	obs_data_release(settings);
}

void heartRateSourceDeactivate(void *data)
{
	struct heartRateSource *hrs = reinterpret_cast<heartRateSource *>(data);
	hrs->isDisabled = true;
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
}

static bool getBGRAFromStageSurface(struct heartRateSource *hrs)
{
	uint32_t width;
	uint32_t height;

	// Check if the source is enabled
	if (!obs_source_enabled(hrs->source)) {
		return false;
	}

	// Retrieve the target source of the filter
	obs_source_t *target;
	if (hrs->source) {
		target = obs_filter_get_target(hrs->source);
	} else {
		return false;
	}
	if (!target) {
		return false;
	}

	// Retrieve the base dimensions of the target source
	width = obs_source_get_base_width(target);
	height = obs_source_get_base_height(target);
	if (width == 0 || height == 0) {
		return false;
	}

	obs_enter_graphics();

	// Resets the texture renderer and begins rendering with the specified width and height
	gs_texrender_reset(hrs->texrender);
	if (!hrs->texrender) {
		obs_leave_graphics();
		return false;
	}
	if (!gs_texrender_begin(hrs->texrender, width, height)) {
		obs_leave_graphics();
		return false;
	}

	// Clear up and set up rendering
	// - Clears the rendering surface with a zeroed background
	// - Sets up the orthographic projection matrix
	// - Pushes the blend state and sets the blend function
	// - Renders the video of the target source
	// - Pops the blend state and ends the texture rendering

	// Declare a vec4 structure to hold the background colour
	struct vec4 background; // two component vector struct (x, y, z, w)

	// Set the background colour to zero (black)
	vec4_zero(&background); // zeroes a vector

	// Clear the rendering surface from the previous frame processing, with the specified background colour. The GS_CLEAR_COLOR flag indicates that the colour buffer should be cleared. This sets the colour to &background colour
	gs_clear(GS_CLEAR_COLOR, &background, 0.0f,
		 0); // Clears colour/depth/stencil buffers

	// Sets up an orthographic projection matrix. This matrix defines a 2D rendering space where objects are rendered without perspective distortion
	// Parameters:
	// - 0.0f: The left coordinate of the projection
	// static_cast<float>(width): The rigCan youht coordinate of the projection, set to the width of the target source
	// 0.0f: The bottom coordinate of the projection.
	// static_cast<float>(height): The top coordinate of the projection, set to the height of the target source
	// -100.0f: The near clipping plane. The near clipping plane is the closest plane to the camera. Objects closer to the camera than this plane are clipped (not rendered). It helps to avoid rendering artifacts and improves depth precision by discarding objects that are too close to the camera
	// 100.0f: The far clipping plane. The far clipping plane is the farthest plane from the camera. Objects farther from the camera than this plane are clipped (not rendered).It helps to limit the rendering distance and manage depth buffer precision by discarding objects that are too far away
	gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f, 100.0f);

	// This function saves the current blend state onto a stack. The blend state includes settings that control how colours from different sources are combined during rendering. By pushing the current blend state, you can make temporary changes to the blend settings and later restore the original settings by popping the blend state from the stack
	gs_blend_state_push();

	// This function sets the blend function for rendering. The blend function determines how the source (the new pixels being drawn) and the destination (the existing pixels in the framebuffer) colours are combined
	// Parameters:
	// - GS_BLEND_ONE: The source colour is used as-is. This means the source colour is multiplied by 1
	// - GS_BLEND_ZERO: The destination colour is ignored. This means the destination colour is multiplied by 0
	// Effect: The combination of GS_BLEND_ONE and GS_BLEND_ZERO results in the source colour completely replacing the destination colour. This is equivalent to disabling blending, where the new pixels overwrite the existing pixels
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	// This function renders the video frame of the target source onto the current rendering surface
	obs_source_video_render(target);

	// This function restores the previous blend state from the stack. The blend state that was saved by gs_blend_state_push is now restored. By popping the blend state, you undo the temporary changes made to the blend settings, ensuring that subsequent rendering operations use the original blend settings
	gs_blend_state_pop();

	// This function ends the texture rendering process. It finalizes the rendering operations and makes the rendered texture available for further processing. This function completes the rendering process, ensuring that the rendered texture is properly finalised and can be used for subsequent operations, such as extracting pixel data or further processing
	gs_texrender_end(hrs->texrender);

	// Retrieve the old existing stage surface
	if (hrs->stagesurface) {
		uint32_t stagesurfWidth = gs_stagesurface_get_width(hrs->stagesurface);
		uint32_t stagesurfHeight = gs_stagesurface_get_height(hrs->stagesurface);
		// If it still matches the new width and height, reuse it
		if (stagesurfWidth != width || stagesurfHeight != height) {
			// Destroy the old stage surface
			gs_stagesurface_destroy(hrs->stagesurface);
			hrs->stagesurface = nullptr;
		}
	}

	// Create a new stage surface if necessary
	if (!hrs->stagesurface) {
		hrs->stagesurface = gs_stagesurface_create(width, height, GS_BGRA);
	}

	// Use gs_stage_texture to stage the texture from the texture renderer (hrs->texrender) to the stage surface (hrs->stagesurface). This operation transfers the rendered texture to the stage surface for further processing
	gs_stage_texture(hrs->stagesurface, gs_texrender_get_texture(hrs->texrender));

	// Use gs_stagesurface_map to map the stage surface and retrieve the video data and line size. The video_data pointer will point to the BGRA data, and linesize will indicate the number of bytes per line
	uint8_t *video_data; // A pointer to the memory location where the BGRA data will be accessible
	uint32_t linesize;   // The number of bytes per line (or row) of the image data
	// The gs_stagesurface_map function creates a mapping between the GPU memory and the CPU memory. This allows the CPU to access the pixel data directly from the GPU memory
	if (!gs_stagesurface_map(hrs->stagesurface, &video_data, &linesize)) {
		obs_leave_graphics();
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(hrs->bgraDataMutex);
		std::shared_ptr<input_BGRA_data> bgraData(
			static_cast<input_BGRA_data *>(bzalloc(sizeof(input_BGRA_data))), [](input_BGRA_data *p) {
				if (p)
					bfree(p);
			});

		bgraData->width = width;
		bgraData->height = height;
		bgraData->linesize = linesize;
		bgraData->data = video_data;
		hrs->bgraData = bgraData;
	}

	// Use gs_stagesurface_unmap to unmap the stage surface, releasing the mapped memory.
	gs_stagesurface_unmap(hrs->stagesurface);

	obs_leave_graphics();
	return true;
}

static gs_texture_t *drawRectangle(struct heartRateSource *hrs, uint32_t width, uint32_t height,
				   std::vector<struct vec4> &faceCoordinates)
{
	gs_texture_t *blurredTexture = gs_texture_create(width, height, GS_BGRA, 1, nullptr, 0);
	gs_copy_texture(blurredTexture, gs_texrender_get_texture(hrs->texrender));

	gs_texrender_reset(hrs->texrender);
	if (!gs_texrender_begin(hrs->texrender, width, height)) {
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
	gs_texrender_end(hrs->texrender);
	gs_copy_texture(blurredTexture, gs_texrender_get_texture(hrs->texrender));
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

	if (!hrs->source) {
		return;
	}

	if (hrs->isDisabled) {
		skipVideoFilterIfSafe(hrs->source);
		return;
	}

	if (!getBGRAFromStageSurface(hrs)) {
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

	std::vector<struct vec4> faceCoordinates;
	std::vector<double_t> avg;

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
		avg = hrs->faceDetection->detectFace(hrs->bgraData, faceCoordinates,
						     config.faceDetection.enableDebugBoxes,
						     config.faceDetection.enableTracker,
						     config.faceDetection.frameUpdateInterval);
		if (enableTiming) {
			end_face_detection = os_gettime_ns();
			obs_log(LOG_INFO, "Face detection took: %lu ns", end_face_detection - start_face_detection);
		}
	}

	double heartRate = -1.0;
	bool noFaceDetected = false;
	if (hasFaceSample(avg)) { // face detected
		hrs->frameCount = 0; // reset frame count

		heartRate = hrs->pipeline.update(avg, config.pipeline);
	} else { // no face detected
		hrs->frameCount += 1;
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
			drawRectangle(hrs, hrs->bgraData->width, hrs->bgraData->height, faceCoordinates);

		if (!obs_source_process_filter_begin(hrs->source, GS_BGRA, OBS_ALLOW_DIRECT_RENDERING)) {
			skipVideoFilterIfSafe(hrs->source);
			gs_texture_destroy(testingTexture);
			return;
		}
		gs_effect_set_texture(gs_effect_get_param_by_name(hrs->testing, "image"), testingTexture);

		gs_blend_state_push();
		gs_reset_blend_state();

		if (hrs->source) {
			obs_source_process_filter_tech_end(hrs->source, hrs->testing, hrs->bgraData->width,
							   hrs->bgraData->height, "Draw");
		}

		gs_blend_state_pop();

		gs_texture_destroy(testingTexture);

	} else {
		skipVideoFilterIfSafe(hrs->source);
	}
}
