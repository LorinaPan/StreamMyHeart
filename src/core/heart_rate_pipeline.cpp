#include "heart_rate_pipeline.h"

#include <algorithm>

double HeartRatePipeline::update(const std::vector<double_t> &averageRgb, const HeartRatePipelineConfig &config)
{
	if (!initialized_ || currentAlgorithm_ != config.ppgAlgorithm) {
		estimator_ = MovingAvg();
		currentAlgorithm_ = config.ppgAlgorithm;
		initialized_ = true;
	}

	return estimator_.calculateHeartRate(averageRgb, static_cast<int>(config.preFiltering),
					     static_cast<int>(config.ppgAlgorithm),
					     static_cast<int>(config.postFiltering), true, config.fps,
					     config.sampleWindowSeconds);
}

void HeartRatePipeline::reset()
{
	estimator_ = MovingAvg();
	initialized_ = false;
}

bool hasFaceSample(const std::vector<double_t> &averageRgb)
{
	return !std::all_of(averageRgb.begin(), averageRgb.end(), [](double_t value) { return value == 0.0; });
}
