#ifndef STREAM_MY_HEART_CORE_FRAME_DATA_H
#define STREAM_MY_HEART_CORE_FRAME_DATA_H

#include <cstdint>

struct input_BGRA_data {
	uint8_t *data;
	uint32_t width;
	uint32_t height;
	uint32_t linesize;
};

#endif
