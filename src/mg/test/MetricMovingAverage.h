#pragma once

#include "mg/box/Definitions.h"

#include <atomic>
#include <vector>

namespace mg {
namespace tst {

	struct MetricMovingAverageBucket
	{
		MetricMovingAverageBucket() : mySum(0), myCount(0) {}

		uint64_t mySum;
		uint64_t myCount;
	};

	class MetricMovingAverage
	{
	 public:
		MetricMovingAverage() { Reset(0); }

		void Reset(
			uint32_t aBucketCount);
		void Add(
			uint64_t aValue);
		double Get() const;
		void GoToNextBucket();

	 private:
		std::atomic_uint64_t myCurSum;
		std::atomic_uint64_t myCurCount;
		std::vector<MetricMovingAverageBucket> myBuckets;
		int myBucketIdx;
	};

}
}
