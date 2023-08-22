#include "MetricMovingAverage.h"

namespace mg {
namespace tst {

	void
	MetricMovingAverage::Reset(
		uint32_t aBucketCount)
	{
		myBuckets.clear();
		myBuckets.resize(aBucketCount);
		myBucketIdx = 0;
		myCurSum.store(0, std::memory_order_relaxed);
		myCurCount = 0;
	}

	void
	MetricMovingAverage::Add(
		uint64_t aValue)
	{
		myCurSum.fetch_add(aValue, std::memory_order_relaxed);
		myCurCount.fetch_add(1, std::memory_order_relaxed);
	}

	double
	MetricMovingAverage::Get() const
	{
		uint64_t count = 0;
		uint64_t sum = 0;
		for (const MetricMovingAverageBucket& b : myBuckets)
		{
			count += b.myCount;
			sum += b.mySum;
		}
		if (count > 0)
			return (double)sum / count;
		return 0;
	}

	void
	MetricMovingAverage::GoToNextBucket()
	{
		MetricMovingAverageBucket& b = myBuckets[myBucketIdx];
		b.mySum = myCurSum.exchange(0, std::memory_order_relaxed);
		b.myCount = myCurCount.exchange(0, std::memory_order_relaxed);
		myBucketIdx = (myBucketIdx + 1) % myBuckets.size();
	}

}
}
