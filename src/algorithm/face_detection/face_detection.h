#ifndef FACE_DETECTION_H
#define FACE_DETECTION_H

#include "../../core/frame_data.h"
#include "face_detection_types.h"

#include <graphics/vec4.h>
#include <dlib/image_processing.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/shape_predictor.h>
#include <dlib/opencv.h>
#include <vector>

class FaceDetection {
public:
	virtual ~FaceDetection() = default;
	virtual std::vector<double_t> detectFace(std::shared_ptr<struct input_BGRA_data> bgraData,
						 std::vector<struct vec4> &faceCoordinates, bool enableDebugBoxes,
						 bool enableTracker, int frameUpdateInterval,
						 bool evaluation = false) = 0;

	static std::unique_ptr<FaceDetection> create(FaceDetectionAlgorithm algorithm);
};

#endif // FACE_DETECTION_H
