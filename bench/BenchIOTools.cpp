#include "BenchIOTools.h"

#include "mg/aio/IOCore.h"
#include "mg/box/Thread.h"
#include "mg/test/Message.h"
#include "mg/test/MetricMovingAverage.h"
#include "mg/test/MetricSpeed.h"

#if MG_BENCH_IO_HAS_BOOST
#include <boost/asio.hpp>

#ifdef BOOST_NO_EXCEPTIONS
namespace boost {
	void
	throw_exception(std::exception const& e)
	{
		MG_BOX_ASSERT_F(false, "exception - %s", e.what());
		while (true)
			abort();
	}
}
#endif

#endif

namespace mg {
namespace bench {

	static constexpr int theSampleInterval = 100;
	static constexpr int theSampleDuration = 2000;
	static constexpr int theBucketCount = theSampleDuration / theSampleInterval;
	static constexpr int theMagicNumber = 0xdeadbeef;

	struct IOMetrics
	{
		IOMetrics()
			: myConnectionCount(0)
		{
			myLatencyAvg.Reset(theBucketCount);
			mySpeed.Reset(theBucketCount);
		}

		mg::tst::MetricMovingAverage myLatencyAvg;
		mg::tst::MetricSpeed mySpeed;
		mg::box::AtomicI32 myConnectionCount;
	};

	static IOMetrics theMetrics;

	struct PayloadBuffer
	{
		PayloadBuffer()
			: myData(nullptr)
			, mySize(0)
		{
		}

		void
		Reserve(
			uint64_t aSize)
		{
			if (mySize >= aSize)
				return;
			delete[] myData;
			myData = new uint8_t[aSize];
			mySize = aSize;
		}

		uint8_t* myData;
		uint64_t mySize;
	};

	static PayloadBuffer&
	GetThreadLocalPayloadBuffer(
		uint64_t aSize)
	{
		static MG_THREADLOCAL PayloadBuffer* theBuffer = nullptr;
		if (theBuffer == nullptr)
			theBuffer = new PayloadBuffer();
		theBuffer->Reserve(aSize);
		return *theBuffer;
	}

	Message::Message()
		: myTimestamp(0)
		, myIntCount(0)
		, myIntValue(0)
		, myPayloadSize(0)
	{
	}

	void
	BenchIOMetricsAddLatency(
		uint64_t aMsec)
	{
		theMetrics.myLatencyAvg.Add(aMsec);
	}

	static double
	BenchIOMetricsGetAndSwitchLatency()
	{
		double res = theMetrics.myLatencyAvg.Get();
		theMetrics.myLatencyAvg.GoToNextBucket();
		return res;
	}

	void
	BenchIOMetricsAddMessage()
	{
		theMetrics.mySpeed.Add();
	}

	static uint64_t
	BenchIOMetricsGetAndSwitchSpeed()
	{
		uint64_t res = theMetrics.mySpeed.Get();
		theMetrics.mySpeed.GoToNextBucket();
		return res;
	}

	void
	BenchIOMetricsAddConnection()
	{
		theMetrics.myConnectionCount.IncrementRelaxed();
	}

	void
	BenchIOMetricsDropConnection()
	{
		theMetrics.myConnectionCount.DecrementRelaxed();
	}

	static inline uint32_t
	BenchIOMetricsGetConnectionCount()
	{
		int32_t count = theMetrics.myConnectionCount.LoadRelaxed();
		MG_BOX_ASSERT(count >= 0);
		return count;
	}

	void
	BenchIOEncodeMessage(
		mg::tst::WriteMessage& aOut,
		const Message& aMessage)
	{
		aOut.Open();
		aOut.WriteUInt64(aMessage.myTimestamp);
		aOut.WriteUInt32(aMessage.myIntCount);
		for (uint32_t i = 0; i < aMessage.myIntCount; ++i)
			aOut.WriteUInt32(aMessage.myIntValue);

		uint64_t size = aMessage.myPayloadSize;
		PayloadBuffer& buf = GetThreadLocalPayloadBuffer(size);
		uint8_t* pos = buf.myData;
		uint64_t tail = size % sizeof(theMagicNumber);
		uint8_t* end = pos + size;
		uint8_t* safeEnd = end - tail;
		for (; pos < safeEnd; pos += sizeof(theMagicNumber))
			memcpy(pos, &theMagicNumber, sizeof(theMagicNumber));
		memcpy(end - tail, &theMagicNumber, tail);
		aOut.WriteUInt64(size);
		aOut.WriteData(buf.myData, size);
		aOut.Close();
	}

	void
	BenchIODecodeMessage(
		mg::tst::ReadMessage& aMessage,
		Message& aOut)
	{
		MG_BOX_ASSERT(aMessage.IsComplete());
		aMessage.Open();
		aMessage.ReadUInt64(aOut.myTimestamp);
		aMessage.ReadUInt32(aOut.myIntCount);
		if (aOut.myIntCount > 0)
		{
			uint32_t value = 0;
			aMessage.ReadUInt32(value);
			aOut.myIntValue = value;
			for (uint32_t i = 1; i < aOut.myIntCount; ++i)
			{
				aMessage.ReadUInt32(value);
				MG_BOX_ASSERT(value == aOut.myIntValue);
			}
		}
		else
		{
			aOut.myIntValue = 0;
		}
		uint64_t size = 0;
		aMessage.ReadUInt64(size);
		aOut.myPayloadSize = size;
		PayloadBuffer& buf = GetThreadLocalPayloadBuffer(size);
		uint8_t* pos = buf.myData;
		aMessage.ReadData(pos, size);

		uint64_t tail = size % sizeof(theMagicNumber);
		uint8_t* end = pos + size;
		uint8_t* safeEnd = end - tail;
		for (; pos < safeEnd; pos += sizeof(theMagicNumber))
		{
			MG_BOX_ASSERT(memcmp(pos, &theMagicNumber,
				sizeof(theMagicNumber)) == 0);
		}
		MG_BOX_ASSERT(memcmp(end - tail, &theMagicNumber, tail) == 0);
		aMessage.Close();
	}

	void
	BenchIORunMetrics()
	{
		uint64_t startTime = mg::box::GetMilliseconds();
		while (true)
		{
			uint64_t ts1 = mg::box::GetMilliseconds();
			double latency = BenchIOMetricsGetAndSwitchLatency();
			uint64_t speed = BenchIOMetricsGetAndSwitchSpeed();
			printf(
R"--(
################### Main board ###################
  Connect count: %u
        Latency: %lf ms
    Message/sec: %llu
       Time sec: %lf
)--",
				BenchIOMetricsGetConnectionCount(),
				latency,
				(long long)(speed * 1000 / theSampleDuration),
				(ts1 - startTime) / 1000.0
			);
			uint64_t ts2 = mg::box::GetMilliseconds();
			int64_t toSleep = (int64_t)theSampleInterval - (ts2 - ts1);
			if (toSleep > 0)
				mg::box::Sleep(toSleep);
		}
	}

}
}
