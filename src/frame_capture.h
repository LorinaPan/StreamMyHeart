#ifndef STREAM_MY_HEART_FRAME_CAPTURE_H
#define STREAM_MY_HEART_FRAME_CAPTURE_H

#include "core/frame_data.h"

#include <memory>
#include <mutex>

#include <graphics/graphics.h>
#include <obs-source.h>

class FilterFrameCapture {
public:
	FilterFrameCapture() = default;
	~FilterFrameCapture() = default;

	void initialize();
	void destroy();
	bool capture(obs_source_t *filterSource);
	gs_texrender_t *texrender() const;
	std::shared_ptr<input_BGRA_data> sample() const;

private:
	gs_texrender_t *texrender_ = nullptr;
	gs_stagesurf_t *stageSurface_ = nullptr;
	std::shared_ptr<input_BGRA_data> bgraData_;
	mutable std::mutex bgraDataMutex_;
};

#endif
