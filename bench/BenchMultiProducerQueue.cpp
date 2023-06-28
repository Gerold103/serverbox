#include "Bench.h"

#include "mg/box/MultiProducerQueueIntrusive.h"

namespace mg {
namespace bench {

	struct BenchValue
	{
		BenchValue();

		BenchValue* myNext;
	};

	using MultiProducerQueue = mg::box::MultiProducerQueueIntrusive<BenchValue>;

	inline
	BenchValue::BenchValue()
		: myNext(nullptr)
	{
	}

}
}

#include "BenchMultiProducerQueueTemplate.hpp"
