#include "BenchIOTools.h"

#include "Bench.h"
#include "mg/aio/IOCore.h"
#include "mg/box/Thread.h"
#include "mg/test/Message.h"
#include "mg/test/MetricMovingAverage.h"
#include "mg/test/MetricSpeed.h"

#include <algorithm>

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

	void
	throw_exception(std::exception const& e, boost::source_location const&)
	{
		throw_exception(e);
	}
}
#endif

#endif

namespace mg {
namespace bench {
namespace io {

	static constexpr int theSampleInterval = 100;
	static constexpr int theSampleDuration = 2000;
	static constexpr int theMomentInterval = 1000;
	static constexpr int theBucketCount = theSampleDuration / theSampleInterval;
	static constexpr int theMagicNumber = 0xdeadbeef;

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

	Reporter::Reporter()
		: myIsRunning(false)
		, myHasReport(false)
		, myMode(REPORT_MODE_ONLINE)
	{
	}

	Reporter::~Reporter()
	{
		Stop();
	}

	void
	Reporter::Start(
		ReportMode aMode)
	{
		MG_BOX_ASSERT(!myIsRunning);
		myIsRunning = true;
		myHasReport = false;
		myLatencyAvg.Reset(theBucketCount);
		mySpeed.Reset(theBucketCount);
		myConnectionCount.StoreRelaxed(0);
		myMessageCount.StoreRelaxed(0);
		myDuration = 0;
		myMomentDeadline = 0;
		myMode = aMode;
		myMoments = {};
		Thread::Start();
	}

	void
	Reporter::Stop()
	{
		if (!myIsRunning)
			return;
		BlockingStop();
		myIsRunning = false;
		myHasReport = true;
	}

	void
	Reporter::Print()
	{
		MG_BOX_ASSERT(!myIsRunning);
		MG_BOX_ASSERT(myHasReport);
		Report("");
		Report("================ Network summary =================");
		Report("   Messages(total): %llu", (long long)myMessageCount.LoadRelaxed());
		Report("     Duration(sec): %lf", myDuration / 1000.0);
		Report("Message/sec(total): %llu",
			(long long)(myMessageCount.LoadRelaxed() * 1000 / myDuration));
		std::vector<MetricMoment> moments;
		moments.reserve(myMoments.size());
		while (!myMoments.empty())
		{
			moments.push_back(std::move(myMoments.front()));
			myMoments.pop();
		}

		std::sort(moments.begin(), moments.end(),
			[](const MetricMoment& m1, const MetricMoment &m2) -> bool {
				return m1.mySpeed < m2.mySpeed;
			});
		Report("  Message/sec(med): %llu",
			(long long)moments[moments.size() / 2].mySpeed);
		Report("  Message/sec(max): %llu",
			(long long)moments.back().mySpeed);

		std::sort(moments.begin(), moments.end(),
			[](const MetricMoment& m1, const MetricMoment &m2) -> bool {
				return m1.myLatency < m2.myLatency;
			});
		Report("   Latency ms(med): %lf",
			moments[moments.size() / 2].myLatency);
		Report("   Latency ms(max): %lf",
			moments.back().myLatency);
	}

	void
	Reporter::StatAddLatency(
		uint64_t aMsec)
	{
		myLatencyAvg.Add(aMsec);
	}

	void
	Reporter::StatAddMessage()
	{
		mySpeed.Add();
		myMessageCount.IncrementRelaxed();
	}

	void
	Reporter::StatAddConnection()
	{
		myConnectionCount.IncrementRelaxed();
	}

	void
	Reporter::StatDelConnection()
	{
		myConnectionCount.DecrementRelaxed();
	}

	void
	Reporter::Run()
	{
		uint64_t startTime = mg::box::GetMilliseconds();
		while (!StopRequested())
		{
			uint64_t ts1 = mg::box::GetMilliseconds();

			double latency = myLatencyAvg.Get();
			myLatencyAvg.GoToNextBucket();

			uint64_t speed = mySpeed.Get() * 1000 / theMomentInterval;
			mySpeed.GoToNextBucket();

			if (myMode == REPORT_MODE_ONLINE)
			{
				printf(
R"--(
=================== Main board ===================
  Connect count: %u
        Latency: %lf ms
    Message/sec: %llu
       Time sec: %lf
)--",
					myConnectionCount.LoadRelaxed(),
					latency,
					(long long)speed,
					(ts1 - startTime) / 1000.0
				);
			}
			if (ts1 >= myMomentDeadline)
			{
				MetricMoment moment;
				moment.myLatency = latency;
				moment.mySpeed = speed;
				myMoments.push(moment);
				myMomentDeadline = ts1 + theMomentInterval;
			}
			uint64_t ts2 = mg::box::GetMilliseconds();
			int64_t toSleep = (int64_t)theSampleInterval - (ts2 - ts1);
			if (toSleep > 0)
				mg::box::Sleep(toSleep);
		}
		myDuration = mg::box::GetMilliseconds() - startTime;
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

}
}
}
