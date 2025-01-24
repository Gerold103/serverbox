#include "mg/box/ThreadLocalPool.h"

#include "mg/box/Assert.h"
#include "mg/box/ThreadFunc.h"
#include "mg/test/Random.h"

#include "UnitTest.h"

#include <algorithm>
#include <vector>

namespace mg {
namespace unittests {
namespace box {
namespace threadlocalpool {

	template<uint32_t PaddigSize>
	struct ValueBase
	{
		static constexpr uint32_t ourPaddingSize = PaddigSize;
		static constexpr uint32_t ourBatchSize = 0;
		static constexpr bool ourIsPooled = false;

		ValueBase(
			int aValue,
			int aOwner)
			: myValue(aValue)
			, myOwner(aOwner)
		{
		}

		ValueBase(
			int aValue)
			: ValueBase(aValue, 0)
		{
		}

		int myValue;
		int myOwner;
		uint8_t myPadding[PaddigSize];
	};

	template<uint32_t PaddigSize, uint32_t BatchSize>
	struct Value
		: public ValueBase<PaddigSize>
		, public mg::box::ThreadPooled<Value<PaddigSize, BatchSize>, BatchSize>
	{
		static constexpr uint32_t ourBatchSize = BatchSize;
		static constexpr bool ourIsPooled = true;

		Value(
			int aValue,
			int aOwner)
			: ValueBase<PaddigSize>(aValue, aOwner)
		{
		}

		Value(
			int aValue)
			: ValueBase<PaddigSize>(aValue)
		{
		}
	};

	//////////////////////////////////////////////////////////////////////////////////////

	template<uint32_t PaddigSize, uint32_t BatchSize>
	static void
	UnitTestThreadLocalPoolBasic()
	{
		TestCaseGuard guard("Basic, padding = %u, batch = %u",
			(unsigned)PaddigSize, (unsigned)BatchSize);

		using ValueT = Value<PaddigSize, BatchSize>;

		ValueT* v1 = new ValueT(1);
		delete v1;
		ValueT* v2 = new ValueT(2);
		TEST_CHECK(v1 == v2);
		delete v2;

		uint32_t count = mg::box::theThreadLocalBatchSize * 3 + 10;
		std::vector<ValueT*> valuePtrs;
		valuePtrs.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
			valuePtrs.push_back(new ValueT(i));
		for (ValueT* v : valuePtrs)
			delete v;

		std::vector<ValueT*> values;
		values.reserve(count);
		for (uint32_t retryi = 0; retryi < 3; ++retryi)
		{
			for (uint32_t i = 0; i < count; ++i)
			{
				ValueT* v = new ValueT(i);
				TEST_CHECK(std::find(values.begin(), values.end(), v) == values.end());
				TEST_CHECK(std::find(valuePtrs.begin(), valuePtrs.end(), v) !=
					valuePtrs.end());
				values.push_back(v);
			}
			for (ValueT* v : values)
				delete v;
			values.resize(0);
		}
	}

	template<typename ValueT>
	static void
	UnitTestThreadLocalPoolStress()
	{
		TestCaseGuard guard("Stress, pooled = %u, padding = %u, batch = %u",
			ValueT::ourIsPooled ? 1 : 0, (unsigned)ValueT::ourPaddingSize,
			(unsigned)ValueT::ourBatchSize);

		constexpr uint32_t threadCount = 5;
		constexpr uint32_t valueCount = 100000;
#if MG_IS_CI
		// The default workload runs for 10 minutes in GitHub CI on Windows Debug. Twice
		// longer than complete job of any other run config. Lets make it smaller.
		constexpr uint32_t iterCount = 20;
#else
		constexpr uint32_t iterCount = 200;
#endif
		std::vector<mg::box::Thread*> threads;
		threads.reserve(threadCount);
		mg::box::ConditionVariable condVar;
		mg::box::Mutex mutex;
		bool doStart = false;
		mg::box::Atomic<int> threadIdCounter(0);
		for (uint32_t threadI = 0; threadI < threadCount; ++threadI)
		{
			threads.push_back(new mg::box::ThreadFunc("mgtst", [&]() {
				int threadId = threadIdCounter.IncrementFetchRelaxed();
				std::vector<ValueT*> values;
				values.resize(valueCount);

				mutex.Lock();
				while (!doStart)
					condVar.Wait(mutex);
				mutex.Unlock();

				for (uint32_t iterI = 0; iterI < iterCount; ++iterI)
				{
					for (uint32_t valI = 0; valI < valueCount; ++valI)
						values[valI] = new ValueT(valI, threadId);
					for (uint32_t valI = 0; valI < valueCount; ++valI)
					{
						uint32_t lastIdx = valueCount - valI - 1;
						uint32_t idx = mg::tst::RandomUniformUInt32(0, lastIdx);
						ValueT* v = values[idx];
						TEST_CHECK(v->myValue == (int)idx);
						TEST_CHECK(v->myOwner == threadId);
						delete v;
						if (idx == lastIdx)
							continue;
						// Cyclic removal.
						v = values[lastIdx];
						TEST_CHECK(v->myValue == (int)lastIdx);
						TEST_CHECK(v->myOwner == threadId);
						v->myValue = idx;
						values[idx] = v;
					}
				}
			}));
			threads.back()->Start();
		}

		mutex.Lock();
		doStart = true;
		condVar.Broadcast();
		mutex.Unlock();

		for (mg::box::Thread* t : threads)
			t->StopAndDelete();
	}
}

	void
	UnitTestThreadLocalPool()
	{
		using namespace threadlocalpool;
		TestSuiteGuard suite("ThreadLocalPool");

		UnitTestThreadLocalPoolBasic<64, mg::box::theThreadLocalBatchSize>();
		UnitTestThreadLocalPoolBasic<1,  mg::box::theThreadLocalBatchSize>();
		UnitTestThreadLocalPoolBasic<1,  1>();
		UnitTestThreadLocalPoolBasic<1,  10>();

		UnitTestThreadLocalPoolStress<ValueBase<1>>();
		UnitTestThreadLocalPoolStress<Value<1, mg::box::theThreadLocalBatchSize>>();

		UnitTestThreadLocalPoolStress<ValueBase<512>>();
		UnitTestThreadLocalPoolStress<Value<512, mg::box::theThreadLocalBatchSize>>();

		UnitTestThreadLocalPoolStress<ValueBase<1024>>();
		UnitTestThreadLocalPoolStress<Value<1024, mg::box::theThreadLocalBatchSize>>();
	}

}
}
}
