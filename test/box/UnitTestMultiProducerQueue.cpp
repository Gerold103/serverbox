#include "mg/box/MultiProducerQueueIntrusive.h"

#include "mg/box/ThreadFunc.h"
#include "mg/test/Random.h"

#include "UnitTest.h"

#include <vector>

namespace mg {
namespace unittests {
namespace box {

	static void
	UnitTestMPQIBasic()
	{
		{
			struct Entry
			{
				int myValue;
				Entry* myNext;
			};

			Entry e1;
			e1.myValue = 1;
			Entry e2;
			e2.myValue = 2;
			Entry e3;
			e3.myValue = 3;

			mg::box::MultiProducerQueueIntrusive<Entry> queue;
			Entry* tail;
			TEST_CHECK(queue.IsEmpty());
			TEST_CHECK(queue.PopAll(tail) == nullptr && tail == nullptr);
			Entry* garbage = (Entry*) &queue;
			e1.myNext = garbage;
			e2.myNext = garbage;
			e3.myNext = garbage;

			TEST_CHECK(queue.Push(&e1));
			TEST_CHECK(!queue.IsEmpty());
			Entry* res = queue.PopAll(tail);
			TEST_CHECK(queue.IsEmpty());
			TEST_CHECK(res == &e1);
			TEST_CHECK(res->myNext == nullptr);
			TEST_CHECK(tail == &e1);
			TEST_CHECK(queue.PopAll(tail) == nullptr && tail == nullptr);
			e1.myNext = garbage;

			TEST_CHECK(queue.Push(&e1));
			TEST_CHECK(!queue.IsEmpty());
			TEST_CHECK(!queue.Push(&e2));
			res = queue.PopAll(tail);
			TEST_CHECK(queue.IsEmpty());
			TEST_CHECK(res == &e1);
			TEST_CHECK(e1.myNext == &e2);
			TEST_CHECK(e2.myNext == nullptr);
			TEST_CHECK(tail == &e2);
			TEST_CHECK(queue.PopAll(tail) == nullptr && tail == nullptr);
			e1.myNext = garbage;
			e2.myNext = garbage;

			TEST_CHECK(queue.Push(&e1));
			TEST_CHECK(!queue.IsEmpty());
			TEST_CHECK(!queue.Push(&e2));
			TEST_CHECK(!queue.Push(&e3));
			res = queue.PopAll(tail);
			TEST_CHECK(queue.IsEmpty());
			TEST_CHECK(res == &e1);
			TEST_CHECK(e1.myNext == &e2);
			TEST_CHECK(e2.myNext == &e3);
			TEST_CHECK(e3.myNext == nullptr);
			TEST_CHECK(tail == &e3);
			TEST_CHECK(queue.PopAll(tail) == nullptr && tail == nullptr);

			// Push empty reversed.
			TEST_CHECK(queue.PushManyFastReversed(nullptr));
			TEST_CHECK(queue.IsEmpty());

			// Push one reversed.
			e1.myNext = nullptr;
			TEST_CHECK(queue.PushManyFastReversed(&e1));
			TEST_CHECK(!queue.IsEmpty());
			res = queue.PopAll();
			TEST_CHECK(res == &e1 && res->myNext == nullptr);
			TEST_CHECK(queue.IsEmpty());

			// Push 2 reversed.
			e2.myNext = &e1;
			e1.myNext = nullptr;
			TEST_CHECK(queue.PushManyFastReversed(&e2));
			TEST_CHECK(!queue.IsEmpty());
			res = queue.PopAll();
			TEST_CHECK(res == &e1);
			TEST_CHECK(e1.myNext == &e2);
			TEST_CHECK(e2.myNext == nullptr);

			// Push 3 reversed.
			e3.myNext = &e2;
			e2.myNext = &e1;
			e1.myNext = nullptr;
			TEST_CHECK(queue.PushManyFastReversed(&e3));
			TEST_CHECK(!queue.IsEmpty());
			res = queue.PopAll();
			TEST_CHECK(res == &e1);
			TEST_CHECK(e1.myNext == &e2);
			TEST_CHECK(e2.myNext == &e3);
			TEST_CHECK(e3.myNext == nullptr);

			// Push 4 reversed.
			Entry e4;
			e4.myValue = 4;
			e4.myNext = &e3;
			e3.myNext = &e2;
			e2.myNext = &e1;
			e1.myNext = nullptr;
			TEST_CHECK(queue.PushManyFastReversed(&e4));
			TEST_CHECK(!queue.IsEmpty());
			res = queue.PopAll();
			TEST_CHECK(res == &e1);
			TEST_CHECK(e1.myNext == &e2);
			TEST_CHECK(e2.myNext == &e3);
			TEST_CHECK(e3.myNext == &e4);
			TEST_CHECK(e4.myNext == nullptr);

			// Push empty reversed range.
			TEST_CHECK(queue.PushManyFastReversed(nullptr, nullptr));
			TEST_CHECK(queue.IsEmpty());

			// Push one item in a reversed range.
			e1.myNext = nullptr;
			TEST_CHECK(queue.PushManyFastReversed(&e1, &e1));
			TEST_CHECK(!queue.IsEmpty());
			res = queue.PopAll();
			TEST_CHECK(res == &e1 && res->myNext == nullptr);
			TEST_CHECK(queue.IsEmpty());

			// Push 2 in a reversed range.
			e2.myNext = &e1;
			e1.myNext = nullptr;
			TEST_CHECK(queue.PushManyFastReversed(&e2, &e1));
			TEST_CHECK(!queue.IsEmpty());
			res = queue.PopAll();
			TEST_CHECK(res == &e1);
			TEST_CHECK(e1.myNext == &e2);
			TEST_CHECK(e2.myNext == nullptr);

			// Make reversed range with non empty last next.
			e4.myNext = &e3;
			e3.myNext = &e2;
			e2.myNext = &e1;
			e1.myNext = nullptr;
			// Should cut e4.
			TEST_CHECK(queue.PushManyFastReversed(&e4, &e2));
			TEST_CHECK(!queue.IsEmpty());
			res = queue.PopAll();
			TEST_CHECK(res == &e2);
			TEST_CHECK(e2.myNext == &e3);
			TEST_CHECK(e3.myNext == &e4);
			TEST_CHECK(e4.myNext == nullptr);

			// Push many empty.
			TEST_CHECK(queue.PushMany(nullptr));
			TEST_CHECK(queue.IsEmpty());

			// Push 1 as many.
			e1.myNext = nullptr;
			TEST_CHECK(queue.PushMany(&e1));
			TEST_CHECK(!queue.IsEmpty());
			res = queue.PopAll();
			TEST_CHECK(res == &e1 && res->myNext == nullptr);
			TEST_CHECK(queue.IsEmpty());

			// Push 2 as many.
			e1.myNext = &e2;
			e2.myNext = nullptr;
			TEST_CHECK(queue.PushMany(&e1));
			TEST_CHECK(!queue.IsEmpty());
			res = queue.PopAll();
			TEST_CHECK(res == &e1);
			TEST_CHECK(e1.myNext == &e2);
			TEST_CHECK(e2.myNext == nullptr);

			// Push 3 as many.
			e1.myNext = &e2;
			e2.myNext = &e3;
			e3.myNext = nullptr;
			TEST_CHECK(queue.PushMany(&e1));
			TEST_CHECK(!queue.IsEmpty());
			res = queue.PopAll();
			TEST_CHECK(res == &e1);
			TEST_CHECK(e1.myNext == &e2);
			TEST_CHECK(e2.myNext == &e3);
			TEST_CHECK(e3.myNext == nullptr);

			// Push 4 as many.
			e1.myNext = &e2;
			e2.myNext = &e3;
			e3.myNext = &e4;
			e4.myNext = nullptr;
			TEST_CHECK(queue.PushMany(&e1));
			TEST_CHECK(!queue.IsEmpty());
			res = queue.PopAll();
			TEST_CHECK(res == &e1);
			TEST_CHECK(e1.myNext == &e2);
			TEST_CHECK(e2.myNext == &e3);
			TEST_CHECK(e3.myNext == &e4);
			TEST_CHECK(e4.myNext == nullptr);
		}
		{
			// Try non-default link name.
			struct Entry
			{
				int myValue;
				Entry* myNextInQueue;
			};

			Entry e1;
			e1.myValue = 1;
			Entry e2;
			e2.myValue = 2;

			mg::box::MultiProducerQueueIntrusive<Entry, &Entry::myNextInQueue> queue;
			Entry* tail;
			TEST_CHECK(queue.IsEmpty());
			TEST_CHECK(queue.PopAll(tail) == nullptr && tail == nullptr);
			Entry* garbage = (Entry*) &queue;
			e1.myNextInQueue = garbage;
			e2.myNextInQueue = garbage;

			TEST_CHECK(queue.Push(&e1));
			TEST_CHECK(!queue.IsEmpty());
			TEST_CHECK(!queue.Push(&e2));
			Entry* res = queue.PopAll(tail);
			TEST_CHECK(queue.IsEmpty());
			TEST_CHECK(res == &e1);
			TEST_CHECK(e1.myNextInQueue == &e2);
			TEST_CHECK(e2.myNextInQueue == nullptr);
			TEST_CHECK(tail == &e2);
			TEST_CHECK(queue.PopAll(tail) == nullptr && tail == nullptr);

			e1.myNextInQueue = &e2;
			e2.myNextInQueue = nullptr;
			TEST_CHECK(queue.PushMany(&e1));
			TEST_CHECK(!queue.IsEmpty());
			res = queue.PopAll(tail);
			TEST_CHECK(queue.IsEmpty());
			TEST_CHECK(res == &e1);
			TEST_CHECK(e1.myNextInQueue == &e2);
			TEST_CHECK(e2.myNextInQueue == nullptr);
		}
	}

	static void
	UnitTestMPQIStress()
	{
		struct Entry
		{
			Entry(
				uint32_t aThreadId,
				uint32_t aId)
				: myThreadId(aThreadId)
				, myId(aId)
				, myNext(nullptr)
			{
			}

			uint32_t myThreadId;
			uint32_t myId;
			Entry* myNext;
		};

		const uint32_t itemCount = 100000;
		const uint32_t threadCount = 10;

		mg::box::MultiProducerQueueIntrusive<Entry> queue;
		std::vector<Entry*> data;
		data.reserve(threadCount * itemCount);

		std::vector<mg::box::ThreadFunc*> threads;
		threads.reserve(threadCount);
		mg::box::AtomicU32 readyCount(0);
		for (uint32_t ti = 0; ti < threadCount; ++ti)
		{
			threads.push_back(new mg::box::ThreadFunc("mgtst", [&]() {

				const uint32_t packMaxSize = 5;

				uint32_t threadId = readyCount.FetchIncrementRelaxed();
				while (readyCount.LoadRelaxed() != threadCount)
					mg::box::Sleep(1);

				uint32_t i = 0;
				while (i < itemCount)
				{
					uint32_t packSize = mg::tst::RandomUInt32() % packMaxSize + 1;
					if (i + packSize > itemCount)
						packSize = itemCount - i;

					Entry* head = new Entry(threadId, i++);
					Entry* pos = head;
					for (uint32_t j = 1; j < packSize; ++j)
					{
						pos->myNext = new Entry(threadId, i++);
						pos = pos->myNext;
					}
					if (packSize == 1)
						queue.Push(head);
					else
						queue.PushMany(head);
				}
			}));
			threads.back()->Start();
		}

		bool done = false;
		uint64_t yield = 0;
		while (!done)
		{
			done = true;
			for (mg::box::ThreadFunc* f : threads)
				done &= !f->IsRunning();
			Entry* head = queue.PopAll();
			while (head != nullptr)
			{
				data.push_back(head);
				head = head->myNext;
			}
			if (++yield % 1000 == 0)
				mg::box::Sleep(1);
		}
		TEST_CHECK(data.size() == itemCount * threadCount);
		for (mg::box::ThreadFunc* f : threads)
			delete f;
		std::vector<uint32_t> counters;
		counters.reserve(threadCount);
		for (uint32_t i = 0; i < threadCount; ++i)
			counters.push_back(0);
		for (Entry* e : data)
			TEST_CHECK(e->myId == counters[e->myThreadId]++);
		for (uint32_t count : counters)
			TEST_CHECK(count == itemCount);
	}

	void
	UnitTestMultiProducerQueue()
	{
		TestSuiteGuard suite("MultiProducerQueue");

		UnitTestMPQIBasic();
		UnitTestMPQIStress();
	}

}
}
}
