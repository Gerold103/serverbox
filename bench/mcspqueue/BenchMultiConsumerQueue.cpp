#include "Bench.h"

#include "mg/box/MultiConsumerQueue.h"

namespace mg {
namespace bench {

	struct BenchValue
	{
		BenchValue()
			: myValue(nullptr)
		{
		}

		void* myValue;
	};

	using BenchQueue = mg::box::MultiConsumerQueue<BenchValue>;
	using BenchQueueConsumer = mg::box::MultiConsumerQueueConsumer<BenchValue>;

}
}

#include "BenchMultiConsumerQueueTemplate.hpp"
