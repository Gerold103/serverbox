#include "MetricSpeed.h"

namespace mg {
namespace tst {

	void
	MetricSpeed::Reset(
		uint32_t aBucketCount)
	{
		myCurSum.store(0, std::memory_order_relaxed);
		myBucketIdx = 0;
		myBuckets.clear();
		myBuckets.resize(aBucketCount, 0);
	}

	uint64_t
	MetricSpeed::Get() const
	{
		uint64_t sum = 0;
		for (const uint64_t& b : myBuckets)
		{
			sum += b;
		}
		return sum;
	}

	void
	MetricSpeed::GoToNextBucket()
	{
		myBuckets[myBucketIdx] = myCurSum.exchange(0, std::memory_order_relaxed);
		myBucketIdx = (myBucketIdx + 1) % myBuckets.size();
	}

}
}
