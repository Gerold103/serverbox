#include "mg/box/MultiConsumerQueue.h"

#include "mg/box/Thread.h"
#include "mg/box/Time.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace box {

	struct UTMCQValue
	{
		UTMCQValue(
			int aValue)
			: myValue(aValue)
		{
		}

		UTMCQValue()
			: myValue(-1)
		{
		}

		~UTMCQValue()
		{
			myValue = -2;
		}

		int myValue;
		UTMCQValue* myNext;
	};

	// Multi-Consumer-Multi-Producer queue, but with a mutex lock
	// on each push/pop. The queue is used to compare its speed
	// with the Multi-Consumer queue from mg.
	// This implementation is the slowest possible, and anything
	// else shall be faster. Otherwise something is very wrong.
	//
	class UTMCQueueLocked
	{
	public:
		UTMCQueueLocked()
			: myHead(nullptr)
			, myTail(nullptr)
		{
		}

		bool
		Push(
			UTMCQValue* aObject)
		{
			myLock.Lock();
			bool wasEmpty = myHead == nullptr;
			if (wasEmpty)
				myHead = aObject;
			else
				myTail->myNext = aObject;
			myTail = aObject;
			aObject->myNext = nullptr;
			myLock.Unlock();
			return wasEmpty;
		}

		UTMCQValue*
		Pop()
		{
			myLock.Lock();
			UTMCQValue* res = myHead;
			if (res != nullptr)
				myHead = myHead->myNext;
			myLock.Unlock();
			return res;
		}

	private:
		mg::box::Mutex myLock;
		UTMCQValue* myHead;
		UTMCQValue* myTail;
	};

	using UTMCQueue = mg::box::MultiConsumerQueue<UTMCQValue>;
	using UTMCQueueConsumer = mg::box::MultiConsumerQueueConsumer<UTMCQValue>;

	static void
	UnitTestMCQLockedBasic()
	{
		UTMCQueueLocked queue;

		UTMCQValue v1;
		UTMCQValue v2;
		UTMCQValue v3;

		TEST_CHECK(queue.Pop() == nullptr);

		TEST_CHECK(queue.Push(&v1));
		TEST_CHECK(queue.Pop() == &v1);
		TEST_CHECK(queue.Pop() == nullptr);

		TEST_CHECK(queue.Push(&v1));
		TEST_CHECK(!queue.Push(&v2));
		TEST_CHECK(queue.Pop() == &v1);
		TEST_CHECK(queue.Pop() == &v2);
		TEST_CHECK(queue.Pop() == nullptr);

		TEST_CHECK(queue.Push(&v1));
		TEST_CHECK(!queue.Push(&v2));
		TEST_CHECK(!queue.Push(&v3));
		TEST_CHECK(queue.Pop() == &v1);
		TEST_CHECK(queue.Pop() == &v2);
		TEST_CHECK(queue.Pop() == &v3);
		TEST_CHECK(queue.Pop() == nullptr);

		TEST_CHECK(queue.Push(&v1));
		TEST_CHECK(!queue.Push(&v2));
		TEST_CHECK(queue.Pop() == &v1);
		TEST_CHECK(!queue.Push(&v3));
		TEST_CHECK(queue.Pop() == &v2);
		TEST_CHECK(queue.Pop() == &v3);
		TEST_CHECK(queue.Pop() == nullptr);
	}

	static void
	UnitTestMCQBasic()
	{
		UTMCQueue queue(5);
		TEST_CHECK(queue.Count() == 0);
		TEST_CHECK(queue.ConsumerCount() == 0);
		TEST_CHECK(queue.SubQueueCount() == 1);

		{
			// Ensure empty consumer is destroyed fine.
			UTMCQueueConsumer c2;
		}

		UTMCQueueConsumer c1;
		c1.Attach(&queue);
		TEST_CHECK(queue.ConsumerCount() == 1);

		UTMCQValue v1;
		UTMCQValue v2;
		UTMCQValue v3;

		TEST_CHECK(c1.Pop() == nullptr);

		// Push-pop single.
		TEST_CHECK(queue.Push(&v1));
		TEST_CHECK(queue.Count() == 1);

		TEST_CHECK(c1.Pop() == &v1);
		TEST_CHECK(queue.Count() == 0);

		TEST_CHECK(c1.Pop() == nullptr);

		// Push-pop 2 elements.
		TEST_CHECK(queue.Push(&v1));
		TEST_CHECK(queue.Count() == 1);

		TEST_CHECK(!queue.Push(&v2));
		TEST_CHECK(queue.Count() == 2);

		TEST_CHECK(c1.Pop() == &v1);
		TEST_CHECK(queue.Count() == 1);

		TEST_CHECK(c1.Pop() == &v2);
		TEST_CHECK(queue.Count() == 0);

		TEST_CHECK(c1.Pop() == nullptr);

		// Push-pop 3 elements. Number of sub-queues becomes 2,
		// because the first one was of size 5, and next 2 pushes
		// fill it.
		TEST_CHECK(queue.Push(&v1));
		TEST_CHECK(queue.Count() == 1);

		TEST_CHECK(!queue.Push(&v2));
		TEST_CHECK(queue.Count() == 2);
		TEST_CHECK(queue.SubQueueCount() == 1);

		TEST_CHECK(!queue.Push(&v3));
		TEST_CHECK(queue.Count() == 3);
		TEST_CHECK(queue.SubQueueCount() == 2);

		TEST_CHECK(c1.Pop() == &v1);
		TEST_CHECK(queue.Count() == 2);

		TEST_CHECK(c1.Pop() == &v2);
		TEST_CHECK(queue.Count() == 1);

		TEST_CHECK(c1.Pop() == &v3);
		TEST_CHECK(queue.Count() == 0);
		TEST_CHECK(queue.SubQueueCount() == 2);

		TEST_CHECK(c1.Pop() == nullptr);

		// Ensure the sub-queues are recycled. Push-pop 5 elements
		// 2 times - the sub-queue count does not change.
		const int count = 5;
		UTMCQValue vs[count];
		for (int i = 0; i < 2; ++i)
		{
			for (int j = 0; j < count; ++j)
				queue.Push(&vs[j]);
			TEST_CHECK(queue.Count() == count);
			for (int j = 0; j < count; ++j)
				TEST_CHECK(c1.Pop() == &vs[j]);
			TEST_CHECK(queue.Count() == 0);
		}
		TEST_CHECK(queue.SubQueueCount() == 2);
	}

	static void
	UnitTestMCQPending()
	{
		UTMCQValue v[10];
		{
			UTMCQueue queue(5);
			UTMCQueueConsumer c;
			c.Attach(&queue);

			// Push one.
			TEST_CHECK(!queue.PushPending(&v[0]));
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(c.Pop() == nullptr);

			TEST_CHECK(queue.FlushPending());
			TEST_CHECK(queue.Count() == 1);
			TEST_CHECK(c.Pop() == &v[0]);
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(c.Pop() == nullptr);
		}
		{
			UTMCQueue queue(5);
			UTMCQueueConsumer c;
			c.Attach(&queue);

			// Flush on empty queue does not do anything.
			TEST_CHECK(!queue.FlushPending());
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(c.Pop() == nullptr);

			// Push 2 so the first one is saved, and the second is
			// written but not available yet.
			TEST_CHECK(!queue.PushPending(&v[0]));
			TEST_CHECK(!queue.PushPending(&v[1]));
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(c.Pop() == nullptr);

			TEST_CHECK(queue.FlushPending());
			TEST_CHECK(queue.Count() == 2);
			TEST_CHECK(c.Pop() == &v[0]);
			TEST_CHECK(c.Pop() == &v[1]);
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(c.Pop() == nullptr);
		}
		{
			UTMCQueue queue(5);
			UTMCQueueConsumer c;
			c.Attach(&queue);

			// Pending push does the flush when wpos is switched.
			for (int i = 0; i < 5; ++i)
				TEST_CHECK(!queue.PushPending(&v[i]));
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(c.Pop() == nullptr);

			TEST_CHECK(queue.PushPending(&v[5]));
			TEST_CHECK(queue.Count() == 5);
			for (int i = 0; i < 5; ++i)
				TEST_CHECK(c.Pop() == &v[i]);
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(c.Pop() == nullptr);

			TEST_CHECK(queue.FlushPending());
			TEST_CHECK(queue.Count() == 1);
			TEST_CHECK(c.Pop() == &v[5]);
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(c.Pop() == nullptr);
		}
		{
			UTMCQueue queue(5);
			UTMCQueueConsumer c;
			c.Attach(&queue);

			// Double-flush does not change anything.
			TEST_CHECK(!queue.PushPending(&v[0]));
			TEST_CHECK(queue.FlushPending());
			TEST_CHECK(!queue.FlushPending());
			TEST_CHECK(c.Pop() == &v[0]);
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(c.Pop() == nullptr);
		}
		{
			UTMCQueue queue(5);
			UTMCQueueConsumer c;
			c.Attach(&queue);

			// Normal push does the flush.
			TEST_CHECK(!queue.PushPending(&v[0]));
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(queue.Push(&v[1]));
			TEST_CHECK(queue.Count() == 2);
			TEST_CHECK(c.Pop() == &v[0]);
			TEST_CHECK(c.Pop() == &v[1]);
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(c.Pop() == nullptr);
		}
		{
			UTMCQueue queue(5);
			UTMCQueueConsumer c;
			c.Attach(&queue);

			// Second flush-via-push won't return that the queue
			// was empty.
			TEST_CHECK(queue.Push(&v[0]));
			TEST_CHECK(!queue.PushPending(&v[1]));
			TEST_CHECK(!queue.Push(&v[2]));
			TEST_CHECK(queue.Count() == 3);
			TEST_CHECK(c.Pop() == &v[0]);
			TEST_CHECK(c.Pop() == &v[1]);
			TEST_CHECK(c.Pop() == &v[2]);
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(c.Pop() == nullptr);
		}
		{
			UTMCQueue queue(5);
			UTMCQueueConsumer c;
			c.Attach(&queue);

			// Sub-queue switch when normal push happens, but
			// there is a pending item.
			for (int i = 0; i < 5; ++i)
				TEST_CHECK(!queue.PushPending(&v[i]));
			TEST_CHECK(queue.Push(&v[5]));
			TEST_CHECK(queue.Count() == 6);
			for (int i = 0; i < 6; ++i)
				TEST_CHECK(c.Pop() == &v[i]);
			TEST_CHECK(queue.Count() == 0);
			TEST_CHECK(c.Pop() == nullptr);
		}
	}

	static void
	UnitTestMCQSingleElement()
	{
		// Ensure the queue still works even with sub-queues of
		// size 1.
		UTMCQueue queue(1);
		TEST_CHECK(queue.Count() == 0);
		TEST_CHECK(queue.ConsumerCount() == 0);
		TEST_CHECK(queue.SubQueueCount() == 1);

		UTMCQValue v1;
		UTMCQValue v2;
		UTMCQValue v3;
		queue.Push(&v1);
		queue.Push(&v2);
		queue.Push(&v3);
		TEST_CHECK(queue.Count() == 3);
		TEST_CHECK(queue.ConsumerCount() == 0);
		TEST_CHECK(queue.SubQueueCount() == 3);

		UTMCQueueConsumer c1(&queue);
		TEST_CHECK(c1.Pop() == &v1);
		TEST_CHECK(queue.Count() == 2);
		TEST_CHECK(c1.Pop() == &v2);
		TEST_CHECK(queue.Count() == 1);
		TEST_CHECK(c1.Pop() == &v3);
		TEST_CHECK(queue.Count() == 0);
		TEST_CHECK(c1.Pop() == nullptr);
	}

	static void
	UnitTestMCQConsumerGap()
	{
		// See what happens when consumers have a gap between
		// each other. So one of them is beyond the sub-queue list
		// head.
		UTMCQueue queue(2);

		UTMCQValue vs[8];
		UTMCQueueConsumer c1(&queue);
		UTMCQueueConsumer c2(&queue);

		for (int i = 0; i < 8; ++i)
			queue.Push(&vs[i]);

		// First 2 sub-queues are fully consumed, one is garbage collected.
		//
		// [0, 1] -> [2, 3] -> [4, 5] -> [6, 7]
		//  c1
		//  c2
		TEST_CHECK(c1.Pop() == &vs[0]);
		TEST_CHECK(queue.Count() == 7);
		TEST_CHECK(c1.Pop() == &vs[1]);
		TEST_CHECK(queue.Count() == 6);
		// [x, x] -> [2, 3] -> [4, 5] -> [6, 7]
		//  c2        c1
		TEST_CHECK(c1.Pop() == &vs[2]);
		TEST_CHECK(queue.Count() == 5);
		TEST_CHECK(c1.Pop() == &vs[3]);
		TEST_CHECK(queue.Count() == 4);
		// [x, x] -> [x, x] -> [4, 5] -> [6, 7]
		//  c2           c1
		TEST_CHECK(c1.Pop() == &vs[4]);
		TEST_CHECK(queue.Count() == 3);
		// [x, x] -> [x, 5] -> [6, 7]
		//  c2        c1

		// But still is referenced by one of the consumers. And it
		// should recycle it and the next sub-queue now.
		TEST_CHECK(c2.Pop() == &vs[5]);
		TEST_CHECK(queue.Count() == 2);
		//  [x, x] -> [6, 7]
		//   c1 c2
		TEST_CHECK(c1.Pop() == &vs[6]);
		TEST_CHECK(queue.Count() == 1);
		// [x, x] -> [x, 7]
		//     c2     c1
		TEST_CHECK(c2.Pop() == &vs[7]);
		TEST_CHECK(queue.Count() == 0);
		// [x, x]
		//  c1 c2
		TEST_CHECK(c1.Pop() == nullptr);
		TEST_CHECK(c2.Pop() == nullptr);

		// Ensure it is still functional.
		queue.Push(&vs[0]);
		queue.Push(&vs[1]);
		TEST_CHECK(c1.Pop() == &vs[0]);
		TEST_CHECK(queue.Count() == 1);
		TEST_CHECK(c2.Pop() == &vs[1]);
		TEST_CHECK(queue.Count() == 0);

		TEST_CHECK(c1.Pop() == nullptr);
		TEST_CHECK(c2.Pop() == nullptr);
	}

	static void
	UnitTestMCQConsumerDetach()
	{
		// See what happens when detach happens for the consumer
		// keeping the head sub-queue, when it is already empty.
		UTMCQValue vs[8];

		// Detach can happen when the head is empty, but some next
		// sub-queue is used by another consumer. The recycling
		// should stop on it regardless of read position.
		{
			UTMCQueue queue(2);

			UTMCQueueConsumer c1(&queue);
			UTMCQueueConsumer c2(&queue);

			for (int i = 0; i < 6; ++i)
				queue.Push(&vs[i]);

			// The head is fully consumed.
			TEST_CHECK(c1.Pop() == &vs[0]);
			TEST_CHECK(queue.Count() == 5);
			TEST_CHECK(c1.Pop() == &vs[1]);
			TEST_CHECK(queue.Count() == 4);
			TEST_CHECK(c1.Pop() == &vs[2]);
			TEST_CHECK(queue.Count() == 3);
			TEST_CHECK(c1.Pop() == &vs[3]);
			TEST_CHECK(queue.Count() == 2);

			// The consumer, keeping the head is detached. The
			// head still should be recycled. Either at detach, or
			// by another consumer later.
			c2.Detach();

			TEST_CHECK(c1.Pop() == &vs[4]);
			TEST_CHECK(queue.Count() == 1);
			TEST_CHECK(c1.Pop() == &vs[5]);
			TEST_CHECK(queue.Count() == 0);

			queue.Push(&vs[6]);
			queue.Push(&vs[7]);

			TEST_CHECK(c1.Pop() == &vs[6]);
			TEST_CHECK(queue.Count() == 1);
			TEST_CHECK(c1.Pop() == &vs[7]);
			TEST_CHECK(queue.Count() == 0);

			// The head was recycled at detach. So 3 sub-queues,
			// not 4.
			TEST_CHECK(queue.SubQueueCount() == 3);
		}

		// Detach can happen when the head is empty, and no more
		// consumers. So the recycling should stop at wpos.
		{
			UTMCQueue queue(2);

			UTMCQueueConsumer c1(&queue);
			UTMCQueueConsumer c2(&queue);

			for (int i = 0; i < 6; ++i)
				queue.Push(&vs[i]);

			// The head is fully consumed.
			TEST_CHECK(c1.Pop() == &vs[0]);
			TEST_CHECK(queue.Count() == 5);
			TEST_CHECK(c1.Pop() == &vs[1]);
			TEST_CHECK(queue.Count() == 4);
			TEST_CHECK(c1.Pop() == &vs[2]);
			TEST_CHECK(queue.Count() == 3);
			TEST_CHECK(c1.Pop() == &vs[3]);
			TEST_CHECK(queue.Count() == 2);

			// The leading consumer is detached. It can't recycle
			// anything because there is a second consumer staying
			// on the head.
			c1.Detach();

			// The head consumer should recycle until wpos.
			c2.Detach();

			// The head was recycled at detach. So 3 subqueues,
			// not 4.
			queue.Push(&vs[6]);
			queue.Push(&vs[7]);
			TEST_CHECK(queue.SubQueueCount() == 3);

			// Validate.
			c1.Attach(&queue);
			TEST_CHECK(c1.Pop() == &vs[4]);
			TEST_CHECK(queue.Count() == 3);
			TEST_CHECK(c1.Pop() == &vs[5]);
			TEST_CHECK(queue.Count() == 2);
			TEST_CHECK(c1.Pop() == &vs[6]);
			TEST_CHECK(queue.Count() == 1);
			TEST_CHECK(c1.Pop() == &vs[7]);
			TEST_CHECK(queue.Count() == 0);
		}

		// Recycle at detach should not do anything with
		// sub-queues which are not fully consumed. Even if they
		// are not referenced.
		{
			UTMCQueue queue(2);

			UTMCQueueConsumer c1(&queue);
			UTMCQueueConsumer c2(&queue);

			for (int i = 0; i < 4; ++i)
				queue.Push(&vs[i]);

			// The head is fully consumed, and another sub-queue
			// is consumed in half.
			TEST_CHECK(c1.Pop() == &vs[0]);
			TEST_CHECK(queue.Count() == 3);
			TEST_CHECK(c1.Pop() == &vs[1]);
			TEST_CHECK(queue.Count() == 2);
			TEST_CHECK(c1.Pop() == &vs[2]);
			TEST_CHECK(queue.Count() == 1);
			TEST_CHECK(queue.SubQueueCount() == 2);

			// The leading consumer is detached. It can't recycle
			// anything because there is a second consumer staying
			// on the head.
			c1.Detach();

			queue.Push(&vs[4]);
			queue.Push(&vs[5]);
			TEST_CHECK(queue.SubQueueCount() == 3);

			// The head consumer should recycle *not* until wpos,
			// but until the last non-consumed sub-queue.
			c2.Detach();

			// The head was recycled at detach. So 3 subqueues,
			// not 4.
			queue.Push(&vs[6]);
			queue.Push(&vs[7]);
			TEST_CHECK(queue.SubQueueCount() == 3);

			// Validate.
			c1.Attach(&queue);
			TEST_CHECK(c1.Pop() == &vs[3]);
			TEST_CHECK(queue.Count() == 4);
			TEST_CHECK(c1.Pop() == &vs[4]);
			TEST_CHECK(queue.Count() == 3);
			TEST_CHECK(c1.Pop() == &vs[5]);
			TEST_CHECK(queue.Count() == 2);
			TEST_CHECK(c1.Pop() == &vs[6]);
			TEST_CHECK(queue.Count() == 1);
			TEST_CHECK(c1.Pop() == &vs[7]);
			TEST_CHECK(queue.Count() == 0);
		}
	}

	class UTMCQLockedConsumerThread
		: public mg::box::Thread
	{
	public:
		UTMCQLockedConsumerThread()
			: myQueue(nullptr)
		{
		}

		void
		Create(
			UTMCQueueLocked* aQueue,
			mg::box::AtomicU32* aTotalPopCount)
		{
			myQueue = aQueue;
			myPopCount = 0;
			myTotalPopCount = aTotalPopCount;
		}

		int
		GetPopCount()
		{
			return myPopCount;
		}

	private:
		void
		Run() override
		{
			UTMCQValue* v;
			uint64_t yield = 0;
			while (!StopRequested())
			{
				while ((v = myQueue->Pop()) != nullptr)
				{
					TEST_CHECK(v->myValue >= 0);
					v->myValue = -v->myValue;
					++myPopCount;
					myTotalPopCount->IncrementRelaxed();
					if (++yield % 10000 == 0)
						mg::box::Sleep(1);
				}
				if (++yield % 10000 == 0)
					mg::box::Sleep(1);
			}
		}

		UTMCQueueLocked* myQueue;
		uint32_t myPopCount;
		mg::box::AtomicU32* myTotalPopCount;
	};

	static void
	UnitTestMCQLockedStress(
		int aElementCount,
		int aThreadCount)
	{
		UTMCQueueLocked queue;
		mg::box::AtomicU32 popCount(0);
		UTMCQValue* values = new UTMCQValue[aElementCount];
		for (int i = 0; i < aElementCount; ++i)
			values[i].myValue = i;

		UTMCQLockedConsumerThread* threads = new UTMCQLockedConsumerThread[aThreadCount];
		for (int i = 0; i < aThreadCount; ++i)
		{
			threads[i].Create(&queue, &popCount);
			threads[i].Start();
		}

		double startMs = mg::box::GetMillisecondsPrecise();

		uint64_t yield = 0;
		for (int i = 0; i < aElementCount; ++i)
		{
			queue.Push(&values[i]);
			if (++yield % 10000 == 0)
				mg::box::Sleep(1);
		}

		while (popCount.LoadRelaxed() != (uint32_t)aElementCount)
			mg::box::Sleep(1);

		double duration = mg::box::GetMillisecondsPrecise() - startMs;

		for (int i = 0; i < aThreadCount; ++i)
			threads[i].BlockingStop();

		for (int i = 0; i < aElementCount; ++i)
			TEST_CHECK(values[i].myValue == -i);

		Report(
			"Locked queue, elements = %d, threads = %d, duration = %lf ms",
			aElementCount, aThreadCount, duration
		);
		for (int i = 0; i < aThreadCount; ++i)
			Report("Thread %d did %d pops", i + 1, threads[i].GetPopCount());

		delete[] threads;
		delete[] values;
	}

	class UTMCQConsumerThread
		: public mg::box::Thread
	{
	public:
		void
		Create(
			UTMCQueue* aQueue,
			mg::box::AtomicU32* aTotalPopCount)
		{
			myConsumer.Attach(aQueue);
			myPopCount = 0;
			myTotalPopCount = aTotalPopCount;
		}

		int
		GetPopCount()
		{
			return myPopCount;
		}

	private:
		void
		Run() override
		{
			UTMCQValue* v;
			uint64_t yield = 0;
			while (!StopRequested())
			{
				while ((v = myConsumer.Pop()) != nullptr)
				{
					TEST_CHECK(v->myValue >= 0);
					v->myValue = -v->myValue;
					++myPopCount;
					myTotalPopCount->IncrementRelaxed();
					if (++yield % 10000 == 0)
						mg::box::Sleep(1);
				}
				if (++yield % 10000 == 0)
					mg::box::Sleep(1);
			}
		}

		UTMCQueueConsumer myConsumer;
		uint32_t myPopCount;
		mg::box::AtomicU32* myTotalPopCount;
	};

	static void
	UnitTestMCQStress(
		bool aIsPushPending,
		bool aReserve,
		int aSubQueueSize,
		int aElementCount,
		int aThreadCount)
	{
		UTMCQueue queue(aSubQueueSize);
		if (aReserve)
			queue.Reserve(aElementCount);

		mg::box::AtomicU32 popCount(0);
		UTMCQValue* values = new UTMCQValue[aElementCount];
		for (int i = 0; i < aElementCount; ++i)
			values[i].myValue = i;

		UTMCQConsumerThread* threads = new UTMCQConsumerThread[aThreadCount];
		for (int i = 0; i < aThreadCount; ++i)
		{
			threads[i].Create(&queue, &popCount);
			threads[i].Start();
		}

		double startMs = mg::box::GetMillisecondsPrecise();

		if (aIsPushPending)
		{
			for (int i = 0; i < aElementCount; ++i)
				queue.PushPending(&values[i]);
			queue.FlushPending();
		}
		else
		{
			for (int i = 0; i < aElementCount; ++i)
				queue.Push(&values[i]);
		}
		double endMs = mg::box::GetMillisecondsPrecise();
		double durationPush = endMs - startMs;

		startMs = endMs;
		while (popCount.LoadRelaxed() != (uint32_t)aElementCount)
			mg::box::Sleep(1);

		double duration = mg::box::GetMillisecondsPrecise() - startMs;

		for (int i = 0; i < aThreadCount; ++i)
			threads[i].BlockingStop();

		for (int i = 0; i < aElementCount; ++i)
			TEST_CHECK(values[i].myValue == -i);

		uint32_t subQueueCountMax =
			aElementCount / aSubQueueSize + (aElementCount % aSubQueueSize != 0);
		TEST_CHECK(queue.SubQueueCount() <= subQueueCountMax);

		Report(
			"Special queue, elements = %d, threads = %d, duration = %lf ms, "
			"push = %lf ms, subqueue count = %u, reserve = %d, pending = %d, "
			"subqueue size = %d", aElementCount, aThreadCount, duration, durationPush,
			queue.SubQueueCount(), (int)aReserve, (int)aIsPushPending, aSubQueueSize
		);
		for (int i = 0; i < aThreadCount; ++i)
			Report("Thread %d did %d pops", i + 1, threads[i].GetPopCount());

		delete[] threads;
		delete[] values;
	}

	void
	UnitTestMultiConsumerQueue()
	{
		TestSuiteGuard suite("MultiConsumerQueue");

		UnitTestMCQLockedBasic();
		UnitTestMCQLockedStress(100, 2);
		UnitTestMCQLockedStress(1000, 5);
		UnitTestMCQLockedStress(1000000, 6);

		UnitTestMCQBasic();
		UnitTestMCQSingleElement();
		UnitTestMCQConsumerGap();
		UnitTestMCQConsumerDetach();
		UnitTestMCQPending();

		UnitTestMCQStress(false, false, 100, 100, 2);
		UnitTestMCQStress(false, true, 100, 100, 2);

		UnitTestMCQStress(false, false, 1000, 1000, 5);
		UnitTestMCQStress(false, true, 1000, 1000, 5);

		UnitTestMCQStress(false, false, 1000, 1000000, 6);
		UnitTestMCQStress(false, true, 1000, 1000000, 6);

		UnitTestMCQStress(true, false, 1000, 1000000, 6);
		UnitTestMCQStress(true, true, 1000, 1000000, 6);
	}

}
}
}
