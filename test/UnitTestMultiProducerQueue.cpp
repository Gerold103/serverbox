#include "mg/box/MultiProducerQueueIntrusive.h"
#include "mg/box/ThreadFunc.h"

#include "mg/test/Random.h"

#include "UnitTest.h"

#include <vector>

namespace mg {
namespace unittests {

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
			MG_BOX_ASSERT(queue.IsEmpty());
			MG_BOX_ASSERT(queue.PopAll(tail) == nullptr && tail == nullptr);
			Entry* garbage = (Entry*) &queue;
			e1.myNext = garbage;
			e2.myNext = garbage;
			e3.myNext = garbage;

			MG_BOX_ASSERT(queue.Push(&e1));
			MG_BOX_ASSERT(!queue.IsEmpty());
			Entry* res = queue.PopAll(tail);
			MG_BOX_ASSERT(queue.IsEmpty());
			MG_BOX_ASSERT(res == &e1);
			MG_BOX_ASSERT(res->myNext == nullptr);
			MG_BOX_ASSERT(tail == &e1);
			MG_BOX_ASSERT(queue.PopAll(tail) == nullptr && tail == nullptr);
			e1.myNext = garbage;

			MG_BOX_ASSERT(queue.Push(&e1));
			MG_BOX_ASSERT(!queue.IsEmpty());
			MG_BOX_ASSERT(!queue.Push(&e2));
			res = queue.PopAll(tail);
			MG_BOX_ASSERT(queue.IsEmpty());
			MG_BOX_ASSERT(res == &e1);
			MG_BOX_ASSERT(e1.myNext == &e2);
			MG_BOX_ASSERT(e2.myNext == nullptr);
			MG_BOX_ASSERT(tail == &e2);
			MG_BOX_ASSERT(queue.PopAll(tail) == nullptr && tail == nullptr);
			e1.myNext = garbage;
			e2.myNext = garbage;

			MG_BOX_ASSERT(queue.Push(&e1));
			MG_BOX_ASSERT(!queue.IsEmpty());
			MG_BOX_ASSERT(!queue.Push(&e2));
			MG_BOX_ASSERT(!queue.Push(&e3));
			res = queue.PopAll(tail);
			MG_BOX_ASSERT(queue.IsEmpty());
			MG_BOX_ASSERT(res == &e1);
			MG_BOX_ASSERT(e1.myNext == &e2);
			MG_BOX_ASSERT(e2.myNext == &e3);
			MG_BOX_ASSERT(e3.myNext == nullptr);
			MG_BOX_ASSERT(tail == &e3);
			MG_BOX_ASSERT(queue.PopAll(tail) == nullptr && tail == nullptr);

			// Push empty reversed.
			MG_BOX_ASSERT(queue.PushManyFastReversed(nullptr));
			MG_BOX_ASSERT(queue.IsEmpty());

			// Push one reversed.
			e1.myNext = nullptr;
			MG_BOX_ASSERT(queue.PushManyFastReversed(&e1));
			MG_BOX_ASSERT(!queue.IsEmpty());
			res = queue.PopAll();
			MG_BOX_ASSERT(res == &e1 && res->myNext == nullptr);
			MG_BOX_ASSERT(queue.IsEmpty());

			// Push 2 reversed.
			e2.myNext = &e1;
			e1.myNext = nullptr;
			MG_BOX_ASSERT(queue.PushManyFastReversed(&e2));
			MG_BOX_ASSERT(!queue.IsEmpty());
			res = queue.PopAll();
			MG_BOX_ASSERT(res == &e1);
			MG_BOX_ASSERT(e1.myNext == &e2);
			MG_BOX_ASSERT(e2.myNext == nullptr);

			// Push 3 reversed.
			e3.myNext = &e2;
			e2.myNext = &e1;
			e1.myNext = nullptr;
			MG_BOX_ASSERT(queue.PushManyFastReversed(&e3));
			MG_BOX_ASSERT(!queue.IsEmpty());
			res = queue.PopAll();
			MG_BOX_ASSERT(res == &e1);
			MG_BOX_ASSERT(e1.myNext == &e2);
			MG_BOX_ASSERT(e2.myNext == &e3);
			MG_BOX_ASSERT(e3.myNext == nullptr);

			// Push 4 reversed.
			Entry e4;
			e4.myValue = 4;
			e4.myNext = &e3;
			e3.myNext = &e2;
			e2.myNext = &e1;
			e1.myNext = nullptr;
			MG_BOX_ASSERT(queue.PushManyFastReversed(&e4));
			MG_BOX_ASSERT(!queue.IsEmpty());
			res = queue.PopAll();
			MG_BOX_ASSERT(res == &e1);
			MG_BOX_ASSERT(e1.myNext == &e2);
			MG_BOX_ASSERT(e2.myNext == &e3);
			MG_BOX_ASSERT(e3.myNext == &e4);
			MG_BOX_ASSERT(e4.myNext == nullptr);

			// Push empty reversed range.
			MG_BOX_ASSERT(queue.PushManyFastReversed(nullptr, nullptr));
			MG_BOX_ASSERT(queue.IsEmpty());

			// Push one item in a reversed range.
			e1.myNext = nullptr;
			MG_BOX_ASSERT(queue.PushManyFastReversed(&e1, &e1));
			MG_BOX_ASSERT(!queue.IsEmpty());
			res = queue.PopAll();
			MG_BOX_ASSERT(res == &e1 && res->myNext == nullptr);
			MG_BOX_ASSERT(queue.IsEmpty());

			// Push 2 in a reversed range.
			e2.myNext = &e1;
			e1.myNext = nullptr;
			MG_BOX_ASSERT(queue.PushManyFastReversed(&e2, &e1));
			MG_BOX_ASSERT(!queue.IsEmpty());
			res = queue.PopAll();
			MG_BOX_ASSERT(res == &e1);
			MG_BOX_ASSERT(e1.myNext == &e2);
			MG_BOX_ASSERT(e2.myNext == nullptr);

			// Make reversed range with non empty last next.
			e4.myNext = &e3;
			e3.myNext = &e2;
			e2.myNext = &e1;
			e1.myNext = nullptr;
			// Should cut e4.
			MG_BOX_ASSERT(queue.PushManyFastReversed(&e4, &e2));
			MG_BOX_ASSERT(!queue.IsEmpty());
			res = queue.PopAll();
			MG_BOX_ASSERT(res == &e2);
			MG_BOX_ASSERT(e2.myNext == &e3);
			MG_BOX_ASSERT(e3.myNext == &e4);
			MG_BOX_ASSERT(e4.myNext == nullptr);

			// Push many empty.
			MG_BOX_ASSERT(queue.PushMany(nullptr));
			MG_BOX_ASSERT(queue.IsEmpty());

			// Push 1 as many.
			e1.myNext = nullptr;
			MG_BOX_ASSERT(queue.PushMany(&e1));
			MG_BOX_ASSERT(!queue.IsEmpty());
			res = queue.PopAll();
			MG_BOX_ASSERT(res == &e1 && res->myNext == nullptr);
			MG_BOX_ASSERT(queue.IsEmpty());

			// Push 2 as many.
			e1.myNext = &e2;
			e2.myNext = nullptr;
			MG_BOX_ASSERT(queue.PushMany(&e1));
			MG_BOX_ASSERT(!queue.IsEmpty());
			res = queue.PopAll();
			MG_BOX_ASSERT(res == &e1);
			MG_BOX_ASSERT(e1.myNext == &e2);
			MG_BOX_ASSERT(e2.myNext == nullptr);

			// Push 3 as many.
			e1.myNext = &e2;
			e2.myNext = &e3;
			e3.myNext = nullptr;
			MG_BOX_ASSERT(queue.PushMany(&e1));
			MG_BOX_ASSERT(!queue.IsEmpty());
			res = queue.PopAll();
			MG_BOX_ASSERT(res == &e1);
			MG_BOX_ASSERT(e1.myNext == &e2);
			MG_BOX_ASSERT(e2.myNext == &e3);
			MG_BOX_ASSERT(e3.myNext == nullptr);

			// Push 4 as many.
			e1.myNext = &e2;
			e2.myNext = &e3;
			e3.myNext = &e4;
			e4.myNext = nullptr;
			MG_BOX_ASSERT(queue.PushMany(&e1));
			MG_BOX_ASSERT(!queue.IsEmpty());
			res = queue.PopAll();
			MG_BOX_ASSERT(res == &e1);
			MG_BOX_ASSERT(e1.myNext == &e2);
			MG_BOX_ASSERT(e2.myNext == &e3);
			MG_BOX_ASSERT(e3.myNext == &e4);
			MG_BOX_ASSERT(e4.myNext == nullptr);
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
			MG_BOX_ASSERT(queue.IsEmpty());
			MG_BOX_ASSERT(queue.PopAll(tail) == nullptr && tail == nullptr);
			Entry* garbage = (Entry*) &queue;
			e1.myNextInQueue = garbage;
			e2.myNextInQueue = garbage;

			MG_BOX_ASSERT(queue.Push(&e1));
			MG_BOX_ASSERT(!queue.IsEmpty());
			MG_BOX_ASSERT(!queue.Push(&e2));
			Entry* res = queue.PopAll(tail);
			MG_BOX_ASSERT(queue.IsEmpty());
			MG_BOX_ASSERT(res == &e1);
			MG_BOX_ASSERT(e1.myNextInQueue == &e2);
			MG_BOX_ASSERT(e2.myNextInQueue == nullptr);
			MG_BOX_ASSERT(tail == &e2);
			MG_BOX_ASSERT(queue.PopAll(tail) == nullptr && tail == nullptr);

			e1.myNextInQueue = &e2;
			e2.myNextInQueue = nullptr;
			MG_BOX_ASSERT(queue.PushMany(&e1));
			MG_BOX_ASSERT(!queue.IsEmpty());
			res = queue.PopAll(tail);
			MG_BOX_ASSERT(queue.IsEmpty());
			MG_BOX_ASSERT(res == &e1);
			MG_BOX_ASSERT(e1.myNextInQueue == &e2);
			MG_BOX_ASSERT(e2.myNextInQueue == nullptr);
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
			threads.push_back(new mg::box::ThreadFunc([&]() {

				const uint32_t packMaxSize = 5;

				uint32_t threadId = readyCount.FetchIncrementRelaxed();
				while (readyCount.LoadRelaxed() != threadCount)
					mg::box::Sleep(1);

				uint32_t i = 0;
				while (i < itemCount)
				{
					uint32_t packSize = mg::test::RandomUInt32() % packMaxSize + 1;
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
		MG_BOX_ASSERT(data.size() == itemCount * threadCount);
		for (mg::box::ThreadFunc* f : threads)
			delete f;
		std::vector<uint32_t> counters;
		counters.reserve(threadCount);
		for (uint32_t i = 0; i < threadCount; ++i)
			counters.push_back(0);
		for (Entry* e : data)
			MG_BOX_ASSERT(e->myId == counters[e->myThreadId]++);
		for (uint32_t count : counters)
			MG_BOX_ASSERT(count == itemCount);
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
