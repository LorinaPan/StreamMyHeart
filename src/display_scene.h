#ifndef STREAM_MY_HEART_DISPLAY_SCENE_H
#define STREAM_MY_HEART_DISPLAY_SCENE_H

#include "plugin_config.h"

#include <obs-data.h>
#include <obs.h>

#include <string>

void reconcileDisplayScene(const DisplaySceneConfig &config);
void removeDisplaySceneSources();
void updateDisplaySceneText(const std::string &heartRateText, const std::string &moodText);

#endif
