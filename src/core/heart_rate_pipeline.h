#ifndef STREAM_MY_HEART_CORE_HEART_RATE_PIPELINE_H
#define STREAM_MY_HEART_CORE_HEART_RATE_PIPELINE_H

#include "../algorithm/heart_rate_algorithm.h"

#include <obs.h>
#include <vector>

enum class PreFilteringMethod {
	NONE = 0,
	BANDPASS = 1,
	DETREND = 2,
	ZERO_MEAN = 3,
};

enum class PpgAlgorithmMethod {
	GREEN = 0,
	PCA = 1,
	CHROM = 2,
};

enum class PostFilteringMethod {
	NONE = 0,
	BANDPASS = 1,
};

struct HeartRatePipelineConfig {
	int fps = 30;
	PreFilteringMethod preFiltering = PreFilteringMethod::ZERO_MEAN;
	PpgAlgorithmMethod ppgAlgorithm = PpgAlgorithmMethod::CHROM;
	PostFilteringMethod postFiltering = PostFilteringMethod::BANDPASS;
};

class HeartRatePipeline {
public:
	HeartRatePipeline() = default;

	double update(const std::vector<double_t> &averageRgb, const HeartRatePipelineConfig &config);
	void reset();

private:
	MovingAvg estimator_;
	PpgAlgorithmMethod currentAlgorithm_ = PpgAlgorithmMethod::CHROM;
	bool initialized_ = false;
};

bool hasFaceSample(const std::vector<double_t> &averageRgb);

#endif
