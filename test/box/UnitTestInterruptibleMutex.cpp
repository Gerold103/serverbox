#include "mg/box/InterruptibleMutex.h"

#include "mg/box/ThreadFunc.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace box {

	static void
	UnreachableWakeup()
	{
		TEST_CHECK(!"Reachable");
	}

	static void
	UnitTestInterruptibleMutexBasic()
	{
		mg::box::InterruptibleMutex mutex;
		mutex.Lock(UnreachableWakeup);
		mutex.Unlock();
		mutex.Lock(UnreachableWakeup);
		mutex.Unlock();
	}

	void
	UnitTestInterruptibleMutexWakeupOne()
	{
		mg::box::InterruptibleMutex mutex;
		mg::box::Signal signal;
		//
		// One thread takes the mutex and sleeps on something.
		//
		mg::box::Signal isBlockedOnSignal;
		mg::box::ThreadFunc* otherThread = new mg::box::ThreadFunc("mgtst",
			[&]() {
			TEST_CHECK(mutex.TryLock());
			isBlockedOnSignal.Send();
			signal.ReceiveBlocking();
			mutex.Unlock();
		});
		otherThread->Start();
		isBlockedOnSignal.ReceiveBlocking();
		//
		// Another thread wakes it up to take the mutex over.
		//
		mutex.Lock([&]() { signal.Send(); });
		delete otherThread;
		mutex.Unlock();
	}

	void
	UnitTestInterruptibleMutexWakeupTwo()
	{
		mg::box::InterruptibleMutex mutex;
		mg::box::Signal mainSignal;
		//
		// One thread takes the mutex and does something, not yet waiting on the main
		// signal.
		//
		mg::box::Signal isStarted;
		mg::box::Signal getTheMainSignal;
		mg::box::ThreadFunc* thread1 = new mg::box::ThreadFunc("mgtst1",
			[&]() {
			TEST_CHECK(mutex.TryLock());
			isStarted.Send();
			getTheMainSignal.ReceiveBlocking();
			mainSignal.ReceiveBlocking();
			mutex.Unlock();
		});
		thread1->Start();
		isStarted.ReceiveBlocking();
		//
		// Another thread tries to enter the mutex, but can't atm. Even the wakeup won't
		// help, because the first thread doesn't sleep on the main signal yet.
		//
		mg::box::ThreadFunc* thread2 = new mg::box::ThreadFunc("mgtst2",
			[&]() {
			mutex.Lock([&]() {
				isStarted.Send();
				mainSignal.Send();
			});
			mutex.Unlock();
		});
		thread2->Start();
		isStarted.ReceiveBlocking();
		//
		// Third thread enters eventually.
		//
		getTheMainSignal.Send();
		// No need to signal anything. The second thread must leave the lock
		// without waiting on anything.
		mutex.Lock([]() {});
		delete thread1;
		delete thread2;
		mutex.Unlock();
	}

	static void
	UnitTestInterruptibleMutexStress()
	{
		mg::box::InterruptibleMutex mutex;
		mg::box::Signal mainSignal;
		mg::box::AtomicU32 counter(0);
		constexpr uint32_t targetCount = 2000;
		mg::box::InterruptibleMutexWakeupCallback wakeupCallback = [&]() {
			mainSignal.Send();
		};

		mg::box::ThreadFunc mainThread("mgtstmai", [&]() {
			uint64_t yield = 0;
			while (true)
			{
				if (!mutex.TryLock())
				{
					if (++yield % 10000000 == 0)
						mg::box::Sleep(1);
					continue;
				}
				mainSignal.ReceiveBlocking();
				uint32_t old = counter.IncrementFetchRelaxed();
				mutex.Unlock();
				if (old >= targetCount)
					break;
			}
		});

		const uint32_t threadCount = 3;
		std::vector<mg::box::ThreadFunc*> threads;
		threads.reserve(threadCount);
		mg::box::AtomicBool isStopped(false);
		for (uint32_t i = 0; i < threadCount; ++i)
		{
			threads.push_back(new mg::box::ThreadFunc("mgtst", [&]() {
				uint64_t yield = 0;
				while (!isStopped.LoadRelaxed())
				{
					mutex.Lock(wakeupCallback);
					mutex.Unlock();
					if (++yield % 1000 == 0)
						mg::box::Sleep(1);
				}
			}));
		}
		for (mg::box::ThreadFunc* f : threads)
			f->Start();
		mainThread.Start();
		uint64_t printDeadline = 0;
		while (mainThread.IsRunning())
		{
			uint64_t now = mg::box::GetMilliseconds();
			if (now >= printDeadline)
			{
				printDeadline = now + 1000;
				uint32_t counterNow = counter.LoadRelaxed();
				int percent;
				if (counterNow >= targetCount)
					percent = 100;
				else
					percent = counterNow * 100 / targetCount;
				Report("Progress: %d (= %u)", percent, counterNow);
			}
			mg::box::Sleep(10);
		}
		Report("Progress: 100");
		isStopped.StoreRelaxed(true);
		for (mg::box::ThreadFunc* f : threads)
			delete f;
	}

	static void
	UnitTestInterruptibleMutexStressMany()
	{
		mg::box::InterruptibleMutex mutex;
		uint32_t counter = 0;
		mg::box::AtomicBool isLocked(false);
		mg::box::AtomicU32 trueCounter(0);
		uint32_t wakeups = 0;
		bool isInWakeup = false;
		mg::box::InterruptibleMutexWakeupCallback wakeupCallback = [&]() {
			TEST_CHECK(!isInWakeup);
			isInWakeup = true;
			++wakeups;
			TEST_CHECK(isInWakeup);
			isInWakeup = false;
		};

		const uint32_t threadCount = 10;
		std::vector<mg::box::ThreadFunc*> threads;
		threads.reserve(threadCount);
		for (uint32_t i = 0; i < threadCount; ++i)
		{
			threads.push_back(new mg::box::ThreadFunc("mgtst", [&]() {
				uint64_t deadline = mg::box::GetMilliseconds() + 2000;
				uint64_t yield = 0;
				uint32_t localCounter = 0;
				while (mg::box::GetMilliseconds() < deadline)
				{
					++localCounter;

					mutex.Lock(wakeupCallback);
					TEST_CHECK(!isLocked.ExchangeRelaxed(true));
					counter++;
					TEST_CHECK(isLocked.ExchangeRelaxed(false));
					mutex.Unlock();
					if (++yield % 1000 == 0)
						mg::box::Sleep(1);
				}
				trueCounter.AddRelaxed(localCounter);
			}));
		}
		for (mg::box::ThreadFunc* f : threads)
			f->Start();
		for (mg::box::ThreadFunc* f : threads)
			delete f;
		// Flush the caches. Make sure all the operations done inside the mutex-lock are
		// complete and visible in this thread.
		TEST_CHECK(mutex.TryLock());
		mutex.Unlock();

		TEST_CHECK(counter == trueCounter.LoadRelaxed());
		TEST_CHECK(wakeups <= counter);
		Report("wakeups = %u", wakeups);
	}

	void
	UnitTestInterruptibleMutex()
	{
		TestSuiteGuard suite("InterruptibleMutex");

		UnitTestInterruptibleMutexBasic();
		UnitTestInterruptibleMutexWakeupOne();
		UnitTestInterruptibleMutexWakeupTwo();
		UnitTestInterruptibleMutexStress();
		UnitTestInterruptibleMutexStressMany();
	}

}
}
}
