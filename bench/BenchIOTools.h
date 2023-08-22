#include "mg/box/Definitions.h"

F_DECLARE_CLASS(mg, tst, ReadMessage)
F_DECLARE_CLASS(mg, tst, WriteMessage)

namespace mg {
namespace bench {

	struct Message
	{
		Message();

		uint64_t myTimestamp;
		uint32_t myIntCount;
		uint32_t myIntValue;
		uint64_t myPayloadSize;
	};

	void BenchIOMetricsAddLatency(
		uint64_t aMsec);
	void BenchIOMetricsAddMessage();
	void BenchIOMetricsAddConnection();
	void BenchIOMetricsDropConnection();

	void BenchIOEncodeMessage(
		mg::tst::WriteMessage& aOut,
		const Message& aMessage);
	void BenchIODecodeMessage(
		mg::tst::ReadMessage& aMessage,
		Message& aOut);

	void BenchIORunMetrics();

}
}
