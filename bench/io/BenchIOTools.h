#pragma once

#include "mg/box/Definitions.h"
#include "mg/box/Thread.h"
#include "mg/test/MetricMovingAverage.h"
#include "mg/test/MetricSpeed.h"

#include <queue>

F_DECLARE_CLASS(mg, tst, ReadMessage)
F_DECLARE_CLASS(mg, tst, WriteMessage)

namespace mg {
namespace bench {
namespace io {

	struct Message
	{
		Message();

		uint64_t myTimestamp;
		uint32_t myIntCount;
		uint32_t myIntValue;
		uint64_t myPayloadSize;
	};

	enum ReportMode
	{
		REPORT_MODE_ONLINE,
		REPORT_MODE_SUMMARY,
	};

	struct MetricMoment
	{
		double myLatency;
		uint64_t mySpeed;
	};

	class Reporter final : private mg::box::Thread
	{
	public:
		Reporter();
		~Reporter() final;

		void Start(
			ReportMode aMode);

		void Stop();

		void Print();

		void StatAddLatency(
			uint64_t aMsec);
		void StatAddMessage();
		void StatAddConnection();
		void StatDelConnection();

	private:
		void Run() final;

		bool myIsRunning;
		bool myHasReport;
		mg::tst::MetricMovingAverage myLatencyAvg;
		mg::tst::MetricSpeed mySpeed;
		mg::box::AtomicU32 myConnectionCount;
		mg::box::AtomicU64 myMessageCount;
		uint64_t myDuration;
		uint64_t myMomentDeadline;
		ReportMode myMode;
		std::queue<MetricMoment> myMoments;
	};

	void BenchIOEncodeMessage(
		mg::tst::WriteMessage& aOut,
		const Message& aMessage);
	void BenchIODecodeMessage(
		mg::tst::ReadMessage& aMessage,
		Message& aOut);

}
}
}
