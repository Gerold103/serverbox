#include "mg/box/ConditionVariable.h"

#include "mg/box/Atomic.h"
#include "mg/box/ThreadFunc.h"

#include "UnitTest.h"

#define UNIT_TEST_CONDVAR_TIMEOUT 10000

namespace mg {
namespace unittests {
namespace box {

	// The tests require some synchronization between threads to
	// make the tests stable and reproducible. And it should not
	// be cond vars, since they are being tested here. This is a
	// simple ping-pong a couple of functions to sync threads
	// using only CPU means.

	static inline void
	UnitTestCondVarSend(
		mg::box::AtomicU32& aCounter,
		uint32_t& aOutNextCounter)
	{
		aOutNextCounter = aCounter.IncrementFetchRelaxed() + 1;
	}

	static inline void
	UnitTestCondVarReceive(
		mg::box::AtomicU32& aCounter,
		uint32_t& aNextCounter)
	{
		while (aCounter.LoadRelaxed() != aNextCounter)
			continue;
		++aNextCounter;
	}

	static void
	UnitTestConditionVariableBasic()
	{
		mg::box::ConditionVariable var;
		mg::box::Mutex mutex;
		mg::box::AtomicU32 stepCounter(0);
		uint32_t next = 0;
		mg::box::ThreadFunc worker("mgtst", [&]() {
			uint32_t workerNext = 1;

			// Test that simple lock/unlock work correct.
			UnitTestCondVarReceive(stepCounter, workerNext);

			mutex.Lock();
			UnitTestCondVarSend(stepCounter, workerNext);
			var.Wait(mutex);
			TEST_CHECK(mutex.IsOwnedByThisThread());
			mutex.Unlock();
			mutex.Lock();
			UnitTestCondVarSend(stepCounter, workerNext);

			// Test that timed wait correctly returns a timeout
			// error.
			UnitTestCondVarReceive(stepCounter, workerNext);

			var.TimedWait(mutex, mg::box::TimeDuration(100));
			TEST_CHECK(mutex.IsOwnedByThisThread());
			UnitTestCondVarSend(stepCounter, workerNext);

			// Test that timed wait does not set the flag if
			// there was no a timeout.
			UnitTestCondVarReceive(stepCounter, workerNext);

			UnitTestCondVarSend(stepCounter, workerNext);
			// Wait signal.
			TEST_CHECK(var.TimedWait(mutex,
				mg::box::TimeDuration(UNIT_TEST_CONDVAR_TIMEOUT)));
			TEST_CHECK(mutex.IsOwnedByThisThread());

			UnitTestCondVarReceive(stepCounter, workerNext);
			UnitTestCondVarSend(stepCounter, workerNext);
			// Wait broadcast.
			TEST_CHECK(var.TimedWait(mutex,
				mg::box::TimeDuration(UNIT_TEST_CONDVAR_TIMEOUT)));
			TEST_CHECK(mutex.IsOwnedByThisThread());

			mutex.Unlock();
		});
		worker.Start();

		// Test that simple lock/unlock work correct.
		UnitTestCondVarSend(stepCounter, next);
		UnitTestCondVarReceive(stepCounter, next);
		mutex.Lock();
		var.Signal();
		mutex.Unlock();
		UnitTestCondVarReceive(stepCounter, next);

		// Test that timed wait correctly returns a timeout
		// error.
		UnitTestCondVarSend(stepCounter, next);
		UnitTestCondVarReceive(stepCounter, next);

		// Test that timed wait does not set the flag if
		// was no a timeout.
		UnitTestCondVarSend(stepCounter, next);
		UnitTestCondVarReceive(stepCounter, next);
		mutex.Lock();
		var.Signal();
		mutex.Unlock();

		UnitTestCondVarSend(stepCounter, next);
		UnitTestCondVarReceive(stepCounter, next);
		mutex.Lock();
		var.Broadcast();
		mutex.Unlock();

		worker.BlockingStop();
	}

	void
	UnitTestConditionVariable()
	{
		TestSuiteGuard suite("ConditionVariable");

		UnitTestConditionVariableBasic();
	}

}
}
}
