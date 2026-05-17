#ifndef HEART_RATE_ALGO_H
#define HEART_RATE_ALGO_H

#include <Eigen/Dense>
#include <cmath>
#include <complex>
#include <numeric>
#include <vector>

#include <obs.h>

class MovingAvg {
private:
	int windowSize;
	int windowStride = 1;
	int maxNumWindows = 8;
	int calibrationTime = 5;
	int fps;

	std::vector<std::vector<std::vector<double_t>>> windows;

	std::vector<std::vector<bool>> latestSkinKey;
	bool detectFace = false;

	std::vector<double_t> heartRates;
	int numHeartRates = 8;
	int startupWarmupHeartRates = 4;

	double uiHeartRate = -1.0;
	int uiUpdateInterval;
	double uiUpdateAmount = 0.0;
	int framesSincePPG = 0;
	int NUM_UPDATES = 8;
	double prevHr;

	void updateWindows(std::vector<double_t> frameAvg);

	double welch(std::vector<double_t> ppgSignal);

	double smoothHeartRate(double hr);

public:
	double calculateHeartRate(std::vector<double_t> avg, int preFilter = 1, int ppgAlgorithm = 1,
				  int postFilter = 0, bool smooth = true, int Fps = 30, int sampleRate = 1);
};
#endif
