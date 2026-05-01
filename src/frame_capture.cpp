#include "frame_capture.h"

#include <graphics/matrix4.h>
#include <obs-module.h>

#include "plugin-support.h"

void FilterFrameCapture::initialize()
{
	texrender_ = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
}

void FilterFrameCapture::destroy()
{
	if (texrender_) {
		gs_texrender_destroy(texrender_);
		texrender_ = nullptr;
	}
	if (stageSurface_) {
		gs_stagesurface_destroy(stageSurface_);
		stageSurface_ = nullptr;
	}
	std::lock_guard<std::mutex> lock(bgraDataMutex_);
	bgraData_.reset();
}

bool FilterFrameCapture::capture(obs_source_t *filterSource)
{
	if (!filterSource || !obs_source_enabled(filterSource)) {
		return false;
	}

	obs_source_t *target = obs_filter_get_target(filterSource);
	if (!target) {
		return false;
	}

	uint32_t width = obs_source_get_base_width(target);
	uint32_t height = obs_source_get_base_height(target);
	if (width == 0 || height == 0) {
		return false;
	}

	obs_enter_graphics();

	gs_texrender_reset(texrender_);
	if (!texrender_ || !gs_texrender_begin(texrender_, width, height)) {
		obs_leave_graphics();
		return false;
	}

	struct vec4 background;
	vec4_zero(&background);
	gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
	gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f, 100.0f);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
	obs_source_video_render(target);
	gs_blend_state_pop();
	gs_texrender_end(texrender_);

	if (stageSurface_) {
		uint32_t stageWidth = gs_stagesurface_get_width(stageSurface_);
		uint32_t stageHeight = gs_stagesurface_get_height(stageSurface_);
		if (stageWidth != width || stageHeight != height) {
			gs_stagesurface_destroy(stageSurface_);
			stageSurface_ = nullptr;
		}
	}

	if (!stageSurface_) {
		stageSurface_ = gs_stagesurface_create(width, height, GS_BGRA);
	}

	gs_stage_texture(stageSurface_, gs_texrender_get_texture(texrender_));

	uint8_t *videoData;
	uint32_t lineSize;
	if (!gs_stagesurface_map(stageSurface_, &videoData, &lineSize)) {
		obs_leave_graphics();
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(bgraDataMutex_);
		std::shared_ptr<input_BGRA_data> bgraData(
			static_cast<input_BGRA_data *>(bzalloc(sizeof(input_BGRA_data))), [](input_BGRA_data *data) {
				if (data) {
					bfree(data);
				}
			});
		bgraData->width = width;
		bgraData->height = height;
		bgraData->linesize = lineSize;
		bgraData->data = videoData;
		bgraData_ = bgraData;
	}

	gs_stagesurface_unmap(stageSurface_);
	obs_leave_graphics();
	return true;
}

gs_texrender_t *FilterFrameCapture::texrender() const
{
	return texrender_;
}

std::shared_ptr<input_BGRA_data> FilterFrameCapture::sample() const
{
	std::lock_guard<std::mutex> lock(bgraDataMutex_);
	return bgraData_;
}
