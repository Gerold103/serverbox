#pragma once

#include "mg/box/Definitions.h"

#include <atomic>
#include <vector>

namespace mg {
namespace tst {

	class MetricSpeed
	{
	 public:
		MetricSpeed() { Reset(0); }

		void Reset(
			uint32_t aBucketCount);
		void Add() { myCurSum.fetch_add(1, std::memory_order_relaxed); }
		uint64_t Get() const;
		void GoToNextBucket();

	 private:
		std::atomic_uint64_t myCurSum;
		std::vector<uint64_t> myBuckets;
		int myBucketIdx;
	};

}
}
