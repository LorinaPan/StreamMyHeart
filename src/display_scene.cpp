#include "display_scene.h"

#include "heart_rate_source.h"
#include "obs_utils.h"
#include "plugin-support.h"

#include <obs-data.h>
#include <obs-frontend-api.h>
#include <obs-module.h>

namespace {
void applySceneItemTransform(obs_scene_t *scene, obs_source_t *source, const obs_transform_info &transformInfo)
{
	obs_sceneitem_t *sceneItem = getSceneItemFromSource(scene, source);
	if (sceneItem != nullptr) {
		obs_sceneitem_set_info2(sceneItem, &transformInfo);
		obs_sceneitem_release(sceneItem);
	}
}

void createTextLikeSource(obs_scene_t *scene, const char *sourceName, const char *defaultText, float posY,
			  bool extents)
{
	obs_source_t *source = obs_get_source_by_name(sourceName);
	if (source) {
		obs_source_release(source);
		return;
	}

	source = obs_source_create("text_ft2_source_v2", sourceName, nullptr, nullptr);
	if (!source) {
		return;
	}

	obs_scene_add(scene, source);
	obs_data_t *sourceSettings = obs_source_get_settings(source);
	obs_data_set_bool(sourceSettings, "word_wrap", true);
	obs_data_set_bool(sourceSettings, "extents", extents);
	obs_data_set_bool(sourceSettings, "outline", true);
	obs_data_set_int(sourceSettings, "outline_color", 4278190080);
	obs_data_set_int(sourceSettings, "outline_size", 7);
	obs_data_set_int(sourceSettings, "extents_cx", extents ? 1500 : 1200);
	obs_data_set_int(sourceSettings, "extents_cy", extents ? 230 : 200);

	obs_data_t *fontData = obs_data_create();
	obs_data_set_string(fontData, "face", "Verdana");
	obs_data_set_string(fontData, "style", "Bold");
	obs_data_set_int(fontData, "size", 64);
	obs_data_set_int(fontData, "flags", 0);
	obs_data_set_obj(sourceSettings, "font", fontData);
	obs_data_release(fontData);

	obs_data_set_string(sourceSettings, "text", defaultText);
	obs_source_update(source, sourceSettings);
	obs_data_release(sourceSettings);

	obs_transform_info transformInfo = {};
	transformInfo.pos.x = 260.0f;
	transformInfo.pos.y = posY;
	transformInfo.bounds.x = 500.0f;
	transformInfo.bounds.y = 145.0f;
	transformInfo.bounds_type = OBS_BOUNDS_SCALE_INNER;
	transformInfo.bounds_alignment = OBS_ALIGN_CENTER;
	transformInfo.alignment = OBS_ALIGN_CENTER;
	transformInfo.scale.x = 1.0f;
	transformInfo.scale.y = 1.0f;
	applySceneItemTransform(scene, source, transformInfo);
	obs_source_release(source);
}

void createImageSource(obs_scene_t *scene)
{
	obs_source_t *imageSource = obs_get_source_by_name(IMAGE_SOURCE_NAME);
	if (imageSource) {
		obs_source_release(imageSource);
		return;
	}

	obs_data_t *imageSettings = obs_data_create();
	char *imagePath = obs_module_file("heart_rate.gif");
	obs_data_set_string(imageSettings, "file", imagePath);
	bfree(imagePath);
	imageSource = obs_source_create("image_source", IMAGE_SOURCE_NAME, imageSettings, nullptr);
	obs_data_release(imageSettings);
	if (!imageSource) {
		return;
	}

	obs_scene_add(scene, imageSource);

	obs_transform_info transformInfo = {};
	transformInfo.pos.x = 460.0f;
	transformInfo.pos.y = 700.0f;
	transformInfo.bounds.x = 300.0f;
	transformInfo.bounds.y = 400.0f;
	transformInfo.bounds_type = OBS_BOUNDS_SCALE_INNER;
	transformInfo.bounds_alignment = OBS_ALIGN_CENTER;
	transformInfo.alignment = OBS_ALIGN_CENTER;
	transformInfo.scale.x = 0.1f;
	transformInfo.scale.y = 0.1f;
	applySceneItemTransform(scene, imageSource, transformInfo);
	obs_source_release(imageSource);
}

void createGraphLikeSource(obs_scene_t *scene, const char *sourceId, const char *sourceName, float posX)
{
	obs_source_t *source = obs_get_source_by_name(sourceName);
	if (source) {
		obs_source_release(source);
		return;
	}

	source = obs_source_create(sourceId, sourceName, nullptr, nullptr);
	if (!source) {
		return;
	}

	obs_scene_add(scene, source);

	obs_transform_info transformInfo = {};
	transformInfo.pos.x = posX;
	transformInfo.pos.y = 700.0f;
	transformInfo.bounds.x = 260.0f;
	transformInfo.bounds.y = 260.0f;
	transformInfo.bounds_type = OBS_BOUNDS_SCALE_INNER;
	transformInfo.bounds_alignment = OBS_ALIGN_CENTER;
	transformInfo.alignment = OBS_ALIGN_CENTER;
	transformInfo.scale.x = 1.0f;
	transformInfo.scale.y = 1.0f;
	applySceneItemTransform(scene, source, transformInfo);
	obs_source_release(source);
}

void reconcileSource(obs_scene_t *scene, bool enabled, const char *sourceName, void (*createFn)(obs_scene_t *))
{
	if (enabled) {
		createFn(scene);
	} else {
		removeSource(sourceName);
	}
}

void createGraphSource(obs_scene_t *scene) { createGraphLikeSource(scene, "heart_rate_graph", GRAPH_SOURCE_NAME, 260.0f); }
void createEcgSource(obs_scene_t *scene) { createGraphLikeSource(scene, "heart_rate_ecg", ECG_SOURCE_NAME, 360.0f); }
void createTextSource(obs_scene_t *scene) { createTextLikeSource(scene, TEXT_SOURCE_NAME, "Calibrating...", 990.0f, true); }
void createMoodSource(obs_scene_t *scene) { createTextLikeSource(scene, MOOD_SOURCE_NAME, "Calibrating...", 900.0f, false); }
} // namespace

void reconcileDisplayScene(const DisplaySceneConfig &config)
{
	obs_source_t *sceneAsSource = obs_frontend_get_current_scene();
	if (!sceneAsSource) {
		return;
	}

	obs_scene_t *scene = obs_scene_from_source(sceneAsSource);
	if (!scene) {
		obs_source_release(sceneAsSource);
		return;
	}

	reconcileSource(scene, config.enableTextSource, TEXT_SOURCE_NAME, createTextSource);
	reconcileSource(scene, config.enableGraphSource, GRAPH_SOURCE_NAME, createGraphSource);
	reconcileSource(scene, config.enableImageSource, IMAGE_SOURCE_NAME, createImageSource);
	reconcileSource(scene, config.enableMoodSource, MOOD_SOURCE_NAME, createMoodSource);
	reconcileSource(scene, config.enableEcgSource, ECG_SOURCE_NAME, createEcgSource);

	obs_source_release(sceneAsSource);
}

void removeDisplaySceneSources()
{
	removeSource(TEXT_SOURCE_NAME);
	removeSource(GRAPH_SOURCE_NAME);
	removeSource(IMAGE_SOURCE_NAME);
	removeSource(MOOD_SOURCE_NAME);
	removeSource(ECG_SOURCE_NAME);
}

void updateDisplaySceneText(const std::string &heartRateText, const std::string &moodText)
{
	obs_source_t *source = obs_get_source_by_name(TEXT_SOURCE_NAME);
	if (source) {
		obs_data_t *sourceSettings = obs_source_get_settings(source);
		obs_data_set_string(sourceSettings, "text", heartRateText.c_str());
		obs_source_update(source, sourceSettings);
		obs_data_release(sourceSettings);
		obs_source_release(source);
	}

	source = obs_get_source_by_name(MOOD_SOURCE_NAME);
	if (source && !moodText.empty()) {
		obs_data_t *sourceSettings = obs_source_get_settings(source);
		obs_data_set_string(sourceSettings, "text", moodText.c_str());
		obs_source_update(source, sourceSettings);
		obs_data_release(sourceSettings);
		obs_source_release(source);
	}
}
