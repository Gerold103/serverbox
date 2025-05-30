#include "mg/sch/TaskScheduler.h"

#include "mg/box/Time.h"
#include "mg/test/Random.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace sch {

	struct TestScopeGuard
	{
		template<typename Func>
		TestScopeGuard(
			Func&& aFunc)
			: myFunc(std::forward<Func>(aFunc))
		{
		}

		~TestScopeGuard()
		{
			myFunc();
		}

		std::function<void()> myFunc;
	};

	static void
	UnitTestTaskSchedulerBasic()
	{
		TestCaseGuard guard("Basic");

		mg::sch::TaskScheduler sched("tst", 5);
		sched.Start(1);
		mg::sch::TaskCallback cb;
		mg::sch::Task* tp;
		mg::box::AtomicU32 progress;

		// Simple test for a task being executed 3 times.
		progress.StoreRelaxed(0);
		cb = [&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask == tp);
			TEST_CHECK(aTask->IsExpired());
			if (progress.IncrementFetchRelaxed() < 3)
				sched.Post(aTask);
		};
		mg::sch::Task t(cb);
		tp = &t;
		sched.Post(&t);
		while (progress.LoadRelaxed() != 3)
			mg::box::Sleep(1);

		// Delay should be respected.
		uint64_t timestamp = mg::box::GetMilliseconds();
		progress.StoreRelaxed(false);
		cb = [&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask == tp);
			TEST_CHECK(aTask->IsExpired());
			TEST_CHECK(mg::box::GetMilliseconds() >= timestamp + 5);
			progress.StoreRelaxed(true);
		};
		t.SetCallback(cb);
		t.SetDelay(5);
		sched.Post(&t);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Deadline should be respected.
		timestamp = 0;
		progress.StoreRelaxed(false);
		cb = [&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask == tp);
			TEST_CHECK(aTask->IsExpired());
			if (timestamp == 0)
			{
				timestamp = mg::box::GetMilliseconds() + 5;
				return sched.PostDeadline(aTask, timestamp);
			}
			TEST_CHECK(mg::box::GetMilliseconds() >= timestamp);
			progress.StoreRelaxed(true);
		};
		t.SetCallback(cb);
		sched.Post(&t);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Task can delete itself before return.
		progress.StoreRelaxed(false);
		tp = new mg::sch::Task();
		cb = [&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask == tp);
			TEST_CHECK(aTask->IsExpired());
			progress.StoreRelaxed(true);
			delete aTask;
		};
		tp->SetCallback(cb);
		sched.Post(tp);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Task's callback can be created in-place.
		progress.StoreRelaxed(false);
		tp = new mg::sch::Task([&](mg::sch::Task* aTask) {
			cb(aTask);
		});
		sched.Post(tp);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);
	}

	static void
	UnitTestTaskSchedulerDestroyWithFront()
	{
		TestCaseGuard guard("Destroy with front");

		mg::box::AtomicU32 doneCount(0);
		mg::sch::Task task2([&](mg::sch::Task*) {
			doneCount.IncrementRelaxed();
		});
		mg::sch::Task task1([&](mg::sch::Task*) {
			// Task posts another one. To cover the case how does the scheduler behave,
			// when a new task was submitted after shutdown is started.
			mg::sch::TaskScheduler::This().Post(&task2);
			doneCount.IncrementRelaxed();
		});
		{
			mg::sch::TaskScheduler sched("tst", 5);
			sched.Start(1);
			sched.Post(&task1);
		}
		TEST_CHECK(doneCount.LoadRelaxed() == 2);
	}

	static void
	UnitTestTaskSchedulerDestroyWithWaiting()
	{
		TestCaseGuard guard("Destroy with waiting");

		mg::box::AtomicU32 doneCount(0);
		mg::sch::Task task([&](mg::sch::Task*) {
			doneCount.IncrementRelaxed();
		});
		task.SetDelay(50);
		{
			mg::sch::TaskScheduler sched("tst", 5);
			sched.Start(1);
			sched.Post(&task);
		}
		TEST_CHECK(doneCount.LoadRelaxed() == 1);
	}

	static void
	UnitTestTaskSchedulerOrder()
	{
		TestCaseGuard guard("Order");

		// Order is never guaranteed in a multi-thread system. But
		// at least it should be correct when the thread is just
		// one.
		mg::sch::TaskScheduler sched("tst", 5);
		sched.Start(1);
		mg::sch::TaskCallback cb;
		mg::sch::Task t1;
		mg::sch::Task t2;
		mg::sch::Task t3;
		mg::box::AtomicU32 progress(0);
		cb = [&](mg::sch::Task* aTask) {
			TEST_CHECK(progress.LoadRelaxed() == 0);
			TEST_CHECK(aTask->IsExpired());
			progress.IncrementRelaxed();
		};
		t1.SetCallback(cb);

		cb = [&](mg::sch::Task* aTask) {
			TEST_CHECK(progress.LoadRelaxed() == 1);
			TEST_CHECK(aTask->IsExpired());
			progress.IncrementRelaxed();
		};
		t2.SetCallback(cb);

		cb = [&](mg::sch::Task* aTask) {
			TEST_CHECK(progress.LoadRelaxed() == 2);
			TEST_CHECK(aTask->IsExpired());
			progress.IncrementRelaxed();
		};
		t3.SetCallback(cb);

		sched.Post(&t1);
		sched.PostDelay(&t2, 3);
		sched.PostDelay(&t3, 5);

		while (progress.LoadRelaxed() != 3)
			mg::box::Sleep(1);
	}

	static void
	UnitTestTaskSchedulerDomino()
	{
		TestCaseGuard guard("Domino");

		// Ensure all the workers wakeup each other if necessary.
		// The test is called 'domino', because the worker threads
		// are supposed to wake each other on demand.
		mg::sch::TaskScheduler sched("tst", 5);
		sched.Start(3);
		mg::sch::TaskCallback cb;
		mg::sch::Task t1;
		mg::sch::Task t2;
		mg::sch::Task t3;
		mg::box::AtomicU32 progress(0);
		mg::box::AtomicBool finish(false);
		cb = [&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask->IsExpired());
			progress.IncrementRelaxed();
			while (!finish.LoadAcquire())
				mg::box::Sleep(1);
			progress.IncrementRelaxed();
		};
		t1.SetCallback(cb);
		t2.SetCallback(cb);
		t3.SetCallback(cb);

		sched.Post(&t1);
		sched.Post(&t2);
		sched.Post(&t3);

		while (progress.LoadRelaxed() != 3)
			mg::box::Sleep(1);
		progress.StoreRelaxed(0);
		finish.StoreRelease(true);
		while (progress.LoadRelaxed() != 3)
			mg::box::Sleep(1);
	}

	static void
	UnitTestTaskSchedulerWakeup()
	{
		TestCaseGuard guard("Wakeup");

		mg::sch::TaskScheduler sched("tst", 5);
		sched.Start(1);
		mg::sch::TaskCallback cb;
		mg::sch::Task t1;
		mg::box::AtomicU32 progress(false);
		cb = [&](mg::sch::Task*) {
			progress.StoreRelaxed(true);
		};
		t1.SetCallback(cb);

		// Wakeup while the task is in the front queue. Should be
		// dispatched immediately to the ready queue.
		sched.Post(&t1);
		t1.PostWakeup();
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Works for deadlined tasks too.
		progress.StoreRelaxed(false);
		sched.PostDeadline(&t1, MG_TIME_INFINITE - 1);
		t1.PostWakeup();
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// If a task was woken up, it should not reuse the old
		// deadline after rescheduling.
		progress.StoreRelaxed(0);
		cb = [&](mg::sch::Task* aTask) {
			if (progress.IncrementFetchRelaxed() == 1)
				sched.Post(aTask);
		};
		t1.SetCallback(cb);
		sched.PostWait(&t1);
		t1.PostWakeup();
		while (progress.LoadRelaxed() != 2)
			mg::box::Sleep(1);

		// Wakeup works even if the task is being executed now.
		progress.StoreRelaxed(0);
		cb = [&](mg::sch::Task* aTask) {
			if (progress.LoadRelaxed() == 0)
			{
				progress.StoreRelaxed(1);
				// Wakeup will happen here, and will make the task
				// executed early despite PostWait() below.
				while (progress.LoadRelaxed() != 2)
					mg::box::Sleep(1);
				return sched.PostWait(aTask);
			}
			progress.StoreRelaxed(3);
		};
		t1.SetCallback(cb);
		sched.Post(&t1);
		while (progress.LoadRelaxed() != 1)
			mg::box::Sleep(1);
		t1.PostWakeup();
		progress.StoreRelaxed(2);
		while (progress.LoadRelaxed() != 3)
			mg::box::Sleep(1);

		// Wakeup for signaled task does not work.
		progress.StoreRelaxed(0);
		cb = [&](mg::sch::Task* aTask) {
			if (aTask->IsSignaled())
			{
				progress.StoreRelaxed(1);
				// Wakeup will happen here, and will not affect
				// PostWait(), because the signal is still active.
				while (progress.LoadRelaxed() != 2)
					mg::box::Sleep(1);
				TEST_CHECK(aTask->ReceiveSignal());
				return sched.PostWait(aTask);
			}
			progress.StoreRelaxed(3);
		};
		t1.SetCallback(cb);
		sched.PostWait(&t1);
		t1.PostSignal();
		while (progress.LoadRelaxed() != 1)
			mg::box::Sleep(1);

		t1.PostWakeup();
		progress.StoreRelaxed(2);
		mg::box::Sleep(1);
		TEST_CHECK(progress.LoadRelaxed() == 2);
		while (t1.IsSignaled())
			mg::box::Sleep(1);

		t1.PostWakeup();
		while (progress.LoadRelaxed() != 3)
			mg::box::Sleep(1);
	}

	static void
	UnitTestTaskSchedulerExpiration()
	{
		TestCaseGuard guard("Expiration");

		mg::sch::TaskScheduler sched("tst", 100);
		sched.Start(1);
		mg::sch::Task t1;
		mg::box::AtomicU32 progress;

		// Expiration check for non-woken task not having a
		// deadline.
		progress.StoreRelaxed(false);
		t1.SetCallback([&](mg::sch::Task* aTask) {
			TEST_CHECK(!aTask->ReceiveSignal());
			TEST_CHECK(aTask->IsExpired());
			progress.StoreRelaxed(true);
		});
		sched.Post(&t1);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Expiration check for woken task not having a deadline.
		progress.StoreRelaxed(false);
		t1.PostWakeup();
		sched.Post(&t1);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Expiration check for non-woken task having a deadline.
		progress.StoreRelaxed(false);
		sched.PostDelay(&t1, 1);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Expiration check for woken task having a deadline.
		progress.StoreRelaxed(false);
		t1.SetCallback([&](mg::sch::Task* aTask) {
			TEST_CHECK(!aTask->ReceiveSignal());
			TEST_CHECK(!aTask->IsExpired());
			progress.StoreRelaxed(true);
		});
		t1.PostWakeup();
		sched.PostDeadline(&t1, MG_TIME_INFINITE - 1);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Expiration check for signaled task not having a
		// deadline.
		progress.StoreRelaxed(false);
		t1.SetCallback([&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask->ReceiveSignal());
			TEST_CHECK(aTask->IsExpired());
			progress.StoreRelaxed(true);
		});
		t1.PostSignal();
		sched.Post(&t1);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Expiration check for signaled task having a deadline.
		progress.StoreRelaxed(false);
		t1.SetCallback([&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask->ReceiveSignal());
			TEST_CHECK(!aTask->IsExpired());
			progress.StoreRelaxed(true);
		});
		t1.PostSignal();
		sched.PostDeadline(&t1, MG_TIME_INFINITE - 1);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Expiration check for woken task which was in infinite
		// wait.
		progress.StoreRelaxed(false);
		t1.SetCallback([&](mg::sch::Task* aTask) {
			TEST_CHECK(!aTask->ReceiveSignal());
			TEST_CHECK(!aTask->IsExpired());
			progress.StoreRelaxed(true);
		});
		t1.PostWakeup();
		sched.PostWait(&t1);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Expiration check for signaled task which was in
		// infinite wait.
		progress.StoreRelaxed(false);
		t1.SetCallback([&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask->ReceiveSignal());
			TEST_CHECK(!aTask->IsExpired());
			progress.StoreRelaxed(true);
		});
		t1.PostSignal();
		sched.PostWait(&t1);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Deadline adjustment.
		progress.StoreRelaxed(false);
		t1.SetDeadline(MG_TIME_INFINITE);
		TEST_CHECK(t1.GetDeadline() == MG_TIME_INFINITE);
		t1.AdjustDeadline(1);
		TEST_CHECK(t1.GetDeadline() == 1);
		t1.AdjustDeadline(MG_TIME_INFINITE);
		TEST_CHECK(t1.GetDeadline() == 1);
		t1.AdjustDelay(1000000);
		// Does not adjust to a bigger value.
		TEST_CHECK(t1.GetDeadline() == 1);
		t1.SetCallback([&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask->IsExpired());
			progress.StoreRelaxed(true);
		});
		sched.Post(&t1);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);
	}

	static void
	UnitTestTaskSchedulerReschedule()
	{
		TestCaseGuard guard("Reschedule");

		mg::sch::TaskScheduler sched1("tst1", 100);
		sched1.Start(2);
		mg::sch::TaskScheduler sched2("tst2", 100);
		sched2.Start(2);
		mg::sch::TaskCallback cb;
		mg::sch::Task t1;
		mg::sch::Task t2;
		mg::sch::Task t3;
		mg::box::AtomicU32 progress(0);
		// A task can schedule another task into the same or
		// different scheduler.
		cb = [&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask == &t1);
			TEST_CHECK(aTask->IsExpired());
			sched1.Post(&t2);
			progress.IncrementRelaxed();
		};
		t1.SetCallback(cb);

		cb = [&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask == &t2);
			TEST_CHECK(aTask->IsExpired());
			sched2.Post(&t3);
			progress.IncrementRelaxed();
		};
		t2.SetCallback(cb);
		t2.SetDelay(3);

		cb = [&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask == &t3);
			TEST_CHECK(aTask->IsExpired());
			progress.IncrementRelaxed();
		};
		t3.SetCallback(cb);
		t3.SetDelay(5);

		sched1.Post(&t1);

		while (progress.LoadRelaxed() != 3)
			mg::box::Sleep(1);

		// A task can be signaled while not scheduled. But the
		// signal won't be consumed until the task is posted
		// again.
		progress.StoreRelaxed(false);
		t1.SetCallback([&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask == &t1);
			TEST_CHECK(aTask->IsExpired());
			if (progress.ExchangeRelaxed(true))
				TEST_CHECK(aTask->ReceiveSignal());
		});
		sched1.Post(&t1);
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		t1.PostSignal();
		mg::box::Sleep(1);
		TEST_CHECK(t1.IsSignaled());
		sched1.Post(&t1);
		while (t1.IsSignaled())
			mg::box::Sleep(1);

		// A task can be woken up while not scheduled. But the
		// task is not executed until posted again.
		progress.StoreRelaxed(0);
		t1.SetCallback([&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask == &t1);
			if (progress.IncrementFetchRelaxed() != 1)
				TEST_CHECK(!aTask->IsExpired());
			else
				TEST_CHECK(aTask->IsExpired());
		});
		sched1.Post(&t1);
		while (progress.LoadRelaxed() != 1)
			mg::box::Sleep(1);

		t1.PostWakeup();
		mg::box::Sleep(1);
		TEST_CHECK(progress.LoadRelaxed() == 1);
		sched1.PostWait(&t1);
		while (progress.LoadRelaxed() != 2)
			mg::box::Sleep(1);
	}

	static void
	UnitTestTaskSchedulerSignal()
	{
		TestCaseGuard guard("Signal");

		mg::sch::TaskScheduler sched("tst", 2);
		sched.Start(1);

		// Signal works during execution.
		mg::box::AtomicU32 progress(0);
		mg::sch::Task t([&](mg::sch::Task* aTask) {
			bool isSignaled = aTask->ReceiveSignal();
			progress.IncrementRelaxed();
			if (isSignaled)
				return;
			// Inject a sleep to ensure the signal works
			// during execution.
			while (progress.LoadRelaxed() != 2)
				mg::box::Sleep(1);
			sched.PostWait(aTask);
		});
		sched.Post(&t);
		while (progress.LoadRelaxed() != 1)
			mg::box::Sleep(1);
		t.PostSignal();
		progress.IncrementRelaxed();
		while (progress.LoadRelaxed() != 3)
			mg::box::Sleep(1);

		// Signal works for tasks in the front queue.
		progress.StoreRelaxed(false);
		t.SetCallback([&](mg::sch::Task*) {
			TEST_CHECK(t.ReceiveSignal());
			progress.StoreRelaxed(true);
		});
		sched.PostDeadline(&t, MG_TIME_INFINITE);
		t.PostSignal();
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Signal works for tasks in the waiting queue. Without
		// -1 the task won't be saved to the waiting queue.
		progress.StoreRelaxed(false);
		sched.PostDeadline(&t, MG_TIME_INFINITE - 1);
		mg::box::Sleep(1);
		t.PostSignal();
		while (!progress.LoadRelaxed())
			mg::box::Sleep(1);

		// Signal can be checked before receipt.
		t.SetCallback([&](mg::sch::Task* aTask) {
			if (!aTask->IsSignaled())
				return sched.PostWait(aTask);
			TEST_CHECK(aTask->ReceiveSignal());
		});
		sched.Post(&t);
		t.PostSignal();
		while (t.IsSignaled())
			mg::box::Sleep(1);

		// Double signal works.
		progress.StoreRelaxed(false);
		t.SetCallback([&](mg::sch::Task* aTask) {
			while (!progress.LoadRelaxed())
				mg::box::Sleep(1);
			TEST_CHECK(aTask->IsSignaled());
			TEST_CHECK(aTask->ReceiveSignal());
		});
		sched.Post(&t);
		t.PostSignal();
		t.PostSignal();
		progress.StoreRelaxed(true);
		while (t.IsSignaled())
			mg::box::Sleep(1);

		// Task is rescheduled until signal is received.
		progress.StoreRelaxed(0);
		t.SetCallback([&](mg::sch::Task* aTask) {
			TEST_CHECK(aTask->IsSignaled());
			uint32_t p = progress.IncrementFetchRelaxed();
			if (p == 1)
				return sched.PostWait(aTask);
			if (p == 2)
				return sched.PostDeadline(aTask, MG_TIME_INFINITE - 1);
			if (p == 3)
				return sched.PostDelay(aTask, 1000000);
			if (p == 4)
				return sched.Post(aTask);
			TEST_CHECK(aTask->ReceiveSignal());
		});
		t.PostSignal();
		sched.Post(&t);
		while (t.IsSignaled())
			mg::box::Sleep(1);
		TEST_CHECK(progress.LoadRelaxed() == 5);
	}

	static void
	UnitTestTaskSchedulerCoroutineBasic()
	{
#if MG_CORO_IS_ENABLED
		TestCaseGuard guard("Coroutine basic");
		mg::sch::TaskScheduler sched("tst", 100);
		sched.Start(2);
		mg::box::Signal s;
		mg::sch::Task t;
		//
		// Async yield.
		//
		t.SetCallback([](mg::sch::Task& t, mg::box::Signal& s) -> mg::box::Coro {
			s.Send();
			t.SetWait();
			co_await t.AsyncYield();

			s.Send();
			t.SetWait();
			co_await t.AsyncYield();

			co_await t.AsyncExitSendSignal(s);
			TEST_CHECK(!"Unreachable");
			co_return;
		}(t, s));
		sched.Post(&t);
		for (int i = 0; i < 2; ++i)
		{
			s.ReceiveBlocking();
			t.PostWakeup();
		}
		s.ReceiveBlocking();
		//
		// Async wait with a deadline.
		//
		t.SetCallback([](mg::sch::Task& t, mg::box::Signal& s) -> mg::box::Coro {
			t.SetDelay(10);
			co_await t.AsyncYield();
			TEST_CHECK(t.IsExpired());

			t.SetDelay(10);
			TEST_CHECK(!co_await t.AsyncReceiveSignal());
			TEST_CHECK(t.IsExpired());

			s.Send();
			t.SetWait();
			TEST_CHECK(co_await t.AsyncReceiveSignal());

			co_await t.AsyncExitSendSignal(s);
			TEST_CHECK(!"Unreachable");
			co_return;
		}(t, s));
		sched.Post(&t);
		s.ReceiveBlocking();
		t.PostSignal();
		s.ReceiveBlocking();
#endif
	}

	static void
	UnitTestTaskSchedulerCoroutineAsyncReceiveSignal()
	{
#if MG_CORO_IS_ENABLED
		TestCaseGuard guard("Coroutine AsyncReceiveSignal()");
		mg::sch::TaskScheduler sched("tst", 100);
		sched.Start(2);
		mg::box::Signal s;
		mg::box::AtomicBool isGuardDone(false);
		mg::sch::Task t;
		t.SetCallback([](
			mg::sch::Task& t,
			mg::box::Signal& s,
			mg::box::AtomicBool& isGuardDone) -> mg::box::Coro {

			for (int i = 0; i < 2; ++i)
			{
				s.Send();
				do {
					t.SetWait();
				} while (!co_await t.AsyncReceiveSignal());
			}
			TestScopeGuard guard([&]() { isGuardDone.StoreRelaxed(true); });
			co_await t.AsyncExitSendSignal(s);
			TEST_CHECK(!"Unreachable");
			co_return;
		}(t, s, isGuardDone));
		sched.Post(&t);
		for (int i = 0; i < 2; ++i)
		{
			s.ReceiveBlocking();
			t.PostWakeup();
			t.PostWakeup();
			mg::box::Sleep(10);
			t.PostSignal();
		}
		s.ReceiveBlocking();
		TEST_CHECK(isGuardDone.LoadRelaxed());
		//
		// The signal already available.
		//
		t.PostSignal();
		t.SetCallback([](mg::sch::Task& t, mg::box::Signal& s) -> mg::box::Coro {
			t.SetWait();
			TEST_CHECK(co_await t.AsyncReceiveSignal());
			co_await t.AsyncExitSendSignal(s);
			TEST_CHECK(!"Unreachable");
			co_return;
		}(t, s));
		sched.Post(&t);
		s.ReceiveBlocking();
		//
		// Connect 2 tasks with a signal.
		//
		mg::sch::Task t2;
		t.SetCallback([](mg::sch::Task& t, mg::sch::Task& t2) -> mg::box::Coro {
			t.SetWait();
			TEST_CHECK(co_await t.AsyncReceiveSignal());
			co_await t.AsyncExitExec([&t2](mg::sch::Task*) { t2.PostSignal(); });
			TEST_CHECK(!"Unreachable");
			co_return;
		}(t, t2));
		sched.Post(&t);

		t2.SetCallback([](
			mg::sch::Task& t,
			mg::sch::Task& t2,
			mg::box::Signal& s) -> mg::box::Coro {

			t.PostSignal();
			t2.SetWait();
			TEST_CHECK(co_await t2.AsyncReceiveSignal());
			co_await t2.AsyncExitSendSignal(s);
			TEST_CHECK(!"Unreachable");
			co_return;
		}(t, t2, s));
		sched.Post(&t2);

		s.ReceiveBlocking();
#endif
	}

	static void
	UnitTestTaskSchedulerCoroutineAsyncExitDelete()
	{
#if MG_CORO_IS_ENABLED
		TestCaseGuard guard("Coroutine AsyncExitDelete()");
		mg::sch::TaskScheduler sched("tst", 100);
		sched.Start(2);
		mg::box::Signal s;
		mg::sch::Task* t = new mg::sch::Task();

		t->SetCallback([](mg::sch::Task* t, mg::box::Signal& s) -> mg::box::Coro {
			s.Send();
			t->SetWait();
			co_await t->AsyncYield();

			TestScopeGuard guard([&]() { s.Send(); });
			co_await t->AsyncExitDelete();

			TEST_CHECK(!"Unreachable");
			co_return;
		}(t, s));
		sched.Post(t);
		s.ReceiveBlocking();
		t->PostWakeup();
		s.ReceiveBlocking();
#endif
	}

	static void
	UnitTestTaskSchedulerCoroutineAsyncExitExec()
	{
#if MG_CORO_IS_ENABLED
		TestCaseGuard guard("Coroutine AsyncExitExec()");
		mg::sch::TaskScheduler sched("tst", 100);
		sched.Start(2);
		mg::box::Signal s;
		mg::sch::Task t;
		t.SetCallback([](
			mg::sch::Task& t,
			mg::box::Signal& s,
			mg::sch::TaskScheduler& sched) -> mg::box::Coro {

			s.Send();
			t.SetWait();
			co_await t.AsyncYield();
			TestScopeGuard guard([&]() { s.Send(); });

			co_await t.AsyncExitExec([&s, &sched](mg::sch::Task* t) {
				// Save it on stack, because the '&sched' is destroyed when the new
				// callback is set below.
				mg::sch::TaskScheduler& tmp = sched;
				t->SetCallback([&s](mg::sch::Task*) {
					s.Send();
				});
				tmp.PostWait(t);
				return;
			});
			TEST_CHECK(!"Unreachable");
			co_return;
		}(t, s, sched));
		sched.Post(&t);
		s.ReceiveBlocking();
		t.PostWakeup();
		s.ReceiveBlocking();
		t.PostWakeup();
		s.ReceiveBlocking();
		//
		// Delete self inside exec.
		//
		mg::sch::Task* th = new mg::sch::Task();
		th->SetCallback([](mg::sch::Task* t, mg::box::Signal& s) -> mg::box::Coro {
			s.Send();
			t->SetWait();
			co_await t->AsyncYield();
			co_await t->AsyncExitExec([&s](mg::sch::Task* t) {
				mg::box::Signal& tmp = s;
				delete t;
				tmp.Send();
				return;
			});
			TEST_CHECK(!"Unreachable");
			co_return;
		}(th, s));
		sched.Post(th);
		s.ReceiveBlocking();
		th->PostWakeup();
		s.ReceiveBlocking();
#endif
	}

	static void
	UnitTestTaskSchedulerCoroutineNested()
	{
#if MG_CORO_IS_ENABLED
		TestCaseGuard guard("Coroutine nested");
		mg::sch::TaskScheduler sched("tst", 100);
		sched.Start(2);
		mg::box::Signal s;
		mg::sch::Task t;
		t.SetCallback([](
			mg::sch::Task& t, mg::box::Signal& s) -> mg::box::Coro {

			co_await mg::box::CoroCall([](
				mg::sch::Task& t, mg::box::Signal& s) -> mg::box::Coro {

				co_await mg::box::CoroCall([](
					mg::sch::Task& t, mg::box::Signal& s) -> mg::box::Coro {

					co_await mg::box::CoroCall([](
						mg::sch::Task& t, mg::box::Signal& s) -> mg::box::Coro {

						co_await mg::box::CoroCall([](
							mg::sch::Task& t, mg::box::Signal& s) -> mg::box::Coro {

							s.Send();
							t.SetWait();
							co_await t.AsyncYield();
						}(t, s));

						s.Send();
						t.SetWait();
						co_await t.AsyncYield();
					}(t, s));

					s.Send();
					t.SetWait();
					co_await t.AsyncYield();
				}(t, s));

				s.Send();
				t.SetWait();
				co_await t.AsyncYield();
			}(t, s));

			s.Send();
			t.SetWait();
			co_await t.AsyncYield();
			co_await t.AsyncExitSendSignal(s);
			TEST_CHECK(!"Unreachable");
			co_return;
		}(t, s));

		sched.Post(&t);
		for (int i = 0; i < 5; ++i)
		{
			s.ReceiveBlocking();
			t.PostWakeup();
		}
		s.ReceiveBlocking();
		//
		// Exit-delete inside nested coroutine stack.
		//
		mg::sch::Task* th = new mg::sch::Task();
		mg::box::AtomicU32 value(0);
		th->SetCallback([](
			mg::sch::Task* t,
			mg::box::Signal& s,
			mg::box::AtomicU32& value) -> mg::box::Coro {

			TestScopeGuard scope([&value, &s]() {
				TEST_CHECK(value.ExchangeRelaxed(3) == 2);
				s.Send();
			});
			co_await mg::box::CoroCall([](
				mg::sch::Task* t,
				mg::box::AtomicU32& value) -> mg::box::Coro {

				TestScopeGuard scope([&value]() {
					TEST_CHECK(value.ExchangeRelaxed(2) == 1);
				});
				co_await mg::box::CoroCall([](
					mg::sch::Task* t,
					mg::box::AtomicU32& value) -> mg::box::Coro {

					TestScopeGuard scope([&value]() {
						TEST_CHECK(value.ExchangeRelaxed(1) == 0);
					});
					co_await t->AsyncExitDelete();
					TEST_CHECK(!"Unreachable");
				}(t, value));
				TEST_CHECK(!"Unreachable");
			}(t, value));
			TEST_CHECK(!"Unreachable");
			co_return;
		}(th, s, value));
		sched.Post(th);
		s.ReceiveBlocking();
		TEST_CHECK(value.LoadRelaxed() == 3);
#endif
	}

	static void
	UnitTestTaskSchedulerCoroutineDifferentSchedulers()
	{
#if MG_CORO_IS_ENABLED
		TestCaseGuard guard("Coroutine different schedulers");
		mg::sch::TaskScheduler sched1("tst1", 100);
		sched1.Start(1);
		mg::sch::TaskScheduler sched2("tst2", 100);
		sched2.Start(1);
		mg::box::Signal s;
		mg::sch::Task t;
		t.SetCallback([](
			mg::sch::Task& t,
			mg::box::Signal& s,
			mg::sch::TaskScheduler& sched1,
			mg::sch::TaskScheduler& sched2) -> mg::box::Coro {

			s.Send();
			t.SetWait();
			co_await t.AsyncYield(sched1);
			mg::box::ThreadId tid1 = mg::box::GetCurrentThreadId();

			s.Send();
			t.SetWait();
			co_await t.AsyncYield(sched2);
			mg::box::ThreadId tid2 = mg::box::GetCurrentThreadId();

			TEST_CHECK(tid1 != tid2);

			s.Send();
			t.SetWait();
			TEST_CHECK(co_await t.AsyncReceiveSignal(sched1));
			TEST_CHECK(mg::box::GetCurrentThreadId() == tid1);

			s.Send();
			t.SetWait();
			TEST_CHECK(co_await t.AsyncReceiveSignal(sched2));
			TEST_CHECK(mg::box::GetCurrentThreadId() == tid2);

			s.Send();
			co_return;
		}(t, s, sched1, sched2));
		sched1.Post(&t);
		for (int i = 0; i < 2; ++i)
		{
			s.ReceiveBlocking();
			t.PostWakeup();
		}
		for (int i = 0; i < 2; ++i)
		{
			s.ReceiveBlocking();
			t.PostSignal();
		}
		s.ReceiveBlocking();
#endif
	}

	static void
	UnitTestTaskSchedulerCoroutineStress()
	{
#if MG_CORO_IS_ENABLED
		TestCaseGuard guard("Coroutine stress");
		mg::sch::TaskScheduler sched("tst", 100);
		sched.Start(5);
		mg::box::Signal s;
		const uint32_t taskCount = 1000;
		const uint32_t signalCount = 10;
		struct Context
		{
			mg::sch::Task myTask;
			mg::box::Signal mySignal;
		};
		std::vector<std::unique_ptr<Context>> ctxs;
		ctxs.resize(taskCount);
		for (std::unique_ptr<Context>& ctx : ctxs)
			ctx = std::unique_ptr<Context>(new Context());
		for (uint32_t i = 0; i < taskCount; ++i)
		{
			Context& ctx = *ctxs[i];
			Context& nextCtx = *ctxs[(i + 1) % taskCount];
			ctx.myTask.SetCallback([](
				Context& aCtx,
				Context& aNextCtx,
				uint32_t aSigCount) -> mg::box::Coro {

				for (uint32_t i = 0; i < aSigCount; ++i)
				{
					do {
						aCtx.myTask.SetWait();
					} while (!co_await aCtx.myTask.AsyncReceiveSignal());
					aNextCtx.myTask.PostSignal();
				}
				co_await aCtx.myTask.AsyncExitSendSignal(aCtx.mySignal);
				TEST_CHECK(!"Unreachable");
			}(ctx, nextCtx, signalCount));

			sched.Post(&ctx.myTask);
		}
		ctxs.front()->myTask.PostSignal();
		for (std::unique_ptr<Context>& ctx : ctxs)
			ctx->mySignal.ReceiveBlocking();
#endif
	}

	struct UTTSchedulerTask;

	struct UTTSchedulerTaskCtx
	{
		UTTSchedulerTaskCtx(
			uint32_t aTaskCount,
			uint32_t aExecuteCount,
			mg::sch::TaskScheduler* aScheduler);

		void CreateHeavy();

		void CreateMicro();

		void CreateSignaled();

		void WaitAllExecuted();

		void WaitExecuteCount(
			uint64_t aCount);

		void WaitAllStopped();

		void PostAll();

		~UTTSchedulerTaskCtx();

		UTTSchedulerTask* myTasks;
		const uint32_t myTaskCount;
		const uint32_t myExecuteCount;
		mg::box::AtomicU32 myCurrentParallel;
		mg::box::AtomicU32 myMaxParallel;
		mg::box::AtomicU32 myStopCount;
		mg::box::AtomicU64 myTotalExecuteCount;
		mg::sch::TaskScheduler* myScheduler;
	};

	struct UTTSchedulerTask
		: public mg::sch::Task
	{
		UTTSchedulerTask()
			: myCtx(nullptr)
		{
		}

		void
		CreateHeavy(
			UTTSchedulerTaskCtx* aCtx)
		{
			myExecuteCount = 0;
			myCtx = aCtx;
			SetCallback(std::bind(
				&UTTSchedulerTask::ExecuteHeavy,
				this, std::placeholders::_1));
		}

		void
		CreateMicro(
			UTTSchedulerTaskCtx* aCtx)
		{
			myExecuteCount = 0;
			myCtx = aCtx;
			SetCallback(std::bind(
				&UTTSchedulerTask::ExecuteMicro,
				this, std::placeholders::_1));
		}

		void
		CreateSignaled(
			UTTSchedulerTaskCtx* aCtx)
		{
			myExecuteCount = 0;
			myCtx = aCtx;
			SetCallback(std::bind(
				&UTTSchedulerTask::ExecuteSignaled, this,
				std::placeholders::_1));
			SetWait();
		}

		void
		ExecuteHeavy(
			mg::sch::Task* aTask)
		{
			TEST_CHECK(aTask == this);
			aTask->ReceiveSignal();
			++myExecuteCount;
			myCtx->myTotalExecuteCount.IncrementRelaxed();

			uint32_t i;
			// Tracking max parallel helps to ensure the tasks
			// really use all the worker threads.
			uint32_t old = myCtx->myCurrentParallel.IncrementFetchRelaxed();
			uint32_t max = myCtx->myMaxParallel.LoadRelaxed();
			if (old > max)
				myCtx->myMaxParallel.CmpExchgStrong(max, old);
			// Simulate heavy work.
			for (i = 0; i < 100; ++i)
				mg::tst::RandomBool();
			myCtx->myCurrentParallel.DecrementRelaxed();

			bool isLast = myExecuteCount >= myCtx->myExecuteCount;
			if (myExecuteCount % 10 == 0)
			{
				i = mg::tst::RandomUniformUInt32(0, myCtx->myTaskCount - 1);
				myCtx->myTasks[i].PostWakeup();
				i = mg::tst::RandomUniformUInt32(0, myCtx->myTaskCount - 1);
				myCtx->myTasks[i].PostSignal();
				i = mg::tst::RandomUniformUInt32(0, myCtx->myTaskCount - 1);
				myCtx->myTasks[i].PostWakeup();
				i = mg::tst::RandomUniformUInt32(0, myCtx->myTaskCount - 1);
				myCtx->myTasks[i].PostSignal();
				return isLast ? Stop() : myCtx->myScheduler->Post(aTask);
			}
			if (myExecuteCount % 3 == 0)
				return isLast ? Stop() : myCtx->myScheduler->PostDelay(aTask, 2);
			if (myExecuteCount % 2 == 0)
				return isLast ? Stop() : myCtx->myScheduler->PostDelay(aTask, 1);
			return isLast ? Stop() : myCtx->myScheduler->Post(aTask);
		}

		void
		ExecuteMicro(
			mg::sch::Task* aTask)
		{
			TEST_CHECK(aTask == this);
			++myExecuteCount;
			myCtx->myTotalExecuteCount.IncrementRelaxed();
			if (myExecuteCount >= myCtx->myExecuteCount)
				return Stop();
			return myCtx->myScheduler->Post(aTask);
		}

		void
		ExecuteSignaled(
			mg::sch::Task* aTask)
		{
			TEST_CHECK(aTask == this);
			TEST_CHECK(!aTask->IsExpired());
			TEST_CHECK(aTask->ReceiveSignal());
			++myExecuteCount;
			myCtx->myTotalExecuteCount.IncrementRelaxed();
			if (myExecuteCount >= myCtx->myExecuteCount)
				return Stop();
			return myCtx->myScheduler->PostWait(aTask);
		}

		void
		Stop()
		{
			myCtx->myStopCount.IncrementRelaxed();
		}

		uint32_t myExecuteCount;
		UTTSchedulerTaskCtx* myCtx;
	};

	UTTSchedulerTaskCtx::UTTSchedulerTaskCtx(
		uint32_t aTaskCount,
		uint32_t aExecuteCount,
		mg::sch::TaskScheduler* aScheduler)
		: myTasks(new UTTSchedulerTask[aTaskCount])
		, myTaskCount(aTaskCount)
		, myExecuteCount(aExecuteCount)
		, myCurrentParallel(0)
		, myMaxParallel(0)
		, myStopCount(0)
		, myTotalExecuteCount(0)
		, myScheduler(aScheduler)
	{
	}

	void
	UTTSchedulerTaskCtx::CreateHeavy()
	{
		for (uint32_t i = 0; i < myTaskCount; ++i)
			myTasks[i].CreateHeavy(this);
	}

	void
	UTTSchedulerTaskCtx::CreateMicro()
	{
		for (uint32_t i = 0; i < myTaskCount; ++i)
			myTasks[i].CreateMicro(this);
	}

	void
	UTTSchedulerTaskCtx::CreateSignaled()
	{
		for (uint32_t i = 0; i < myTaskCount; ++i)
			myTasks[i].CreateSignaled(this);
	}

	void
	UTTSchedulerTaskCtx::WaitAllExecuted()
	{
		uint64_t total = myExecuteCount * myTaskCount;
		WaitExecuteCount(total);
		TEST_CHECK(total == myTotalExecuteCount.LoadRelaxed());
	}

	void
	UTTSchedulerTaskCtx::WaitExecuteCount(
		uint64_t aCount)
	{
		while (myTotalExecuteCount.LoadRelaxed() < aCount)
			mg::box::Sleep(1);
	}

	void
	UTTSchedulerTaskCtx::WaitAllStopped()
	{
		while (myStopCount.LoadRelaxed() != myTaskCount)
			mg::box::Sleep(1);
		for (uint32_t i = 0; i < myTaskCount; ++i)
			TEST_CHECK(myTasks[i].myExecuteCount == myExecuteCount);
	}

	void
	UTTSchedulerTaskCtx::PostAll()
	{
		for (uint32_t i = 0; i < myTaskCount; ++i)
			myScheduler->Post(&myTasks[i]);
	}

	UTTSchedulerTaskCtx::~UTTSchedulerTaskCtx()
	{
		delete[] myTasks;
	}

	static void
	UnitTestTaskSchedulerPrintStat(
		const mg::sch::TaskScheduler* aSched)
	{
		uint32_t count;
		mg::sch::TaskSchedulerThread*const* threads = aSched->GetThreads(count);
		Report("Scheduler stat:");
		for (uint32_t i = 0; i < count; ++i)
		{
			uint64_t execCount = threads[i]->StatPopExecuteCount();
			uint64_t schedCount = threads[i]->StatPopScheduleCount();
			Report("Thread %2u: exec: %12llu, sched: %9llu", i, (unsigned long long)execCount,
				(unsigned long long)schedCount);
		}
		Report("");
	}

	static void
	UnitTestTaskSchedulerBatch(
		uint32_t aThreadCount,
		uint32_t aTaskCount,
		uint32_t aExecuteCount)
	{
		TestCaseGuard guard("Batch");

		Report("Batch test: %u threads, %u tasks, %u executes", aThreadCount, aTaskCount,
			aExecuteCount);
		mg::sch::TaskScheduler sched("tst", 5000);
		sched.Start(aThreadCount);
		UTTSchedulerTaskCtx ctx(aTaskCount, aExecuteCount, &sched);

		ctx.CreateHeavy();
		ctx.PostAll();
		ctx.WaitAllStopped();

		UnitTestTaskSchedulerPrintStat(&sched);
	}

	static void
	UnitTestTaskSchedulerMicro(
		uint32_t aThreadCount,
		uint32_t aTaskCount)
	{
		TestCaseGuard guard("Micro");

		// Micro test uses super lightweight tasks to check how
		// fast is the scheduler itself, almost not affected by
		// the task bodies.
		Report("Micro test: %u threads, %u tasks", aThreadCount, aTaskCount);
		mg::sch::TaskScheduler sched("tst", 5000);
		sched.Start(aThreadCount);
		UTTSchedulerTaskCtx ctx(aTaskCount, 1, &sched);

		ctx.CreateMicro();
		double startMs = mg::box::GetMillisecondsPrecise();
		ctx.PostAll();
		ctx.WaitAllExecuted();
		double duration = mg::box::GetMillisecondsPrecise() - startMs;
		Report("Duration: %lf ms", duration);
		ctx.WaitAllStopped();

		UnitTestTaskSchedulerPrintStat(&sched);
	}

	static void
	UnitTestTaskSchedulerMicroNew(
		uint32_t aThreadCount,
		uint32_t aTaskCount)
	{
		TestCaseGuard guard("Micro new");

		// Checkout speed of one-shot tasks allocated manually, so
		// it is -1 virtual call compared to automatic one-shot
		// tasks.
		Report("Micro new test: %u threads, %u tasks", aThreadCount, aTaskCount);
		mg::sch::TaskScheduler sched("tst", 5000);
		sched.Start(aThreadCount);
		mg::box::AtomicU64 executeCount(0);
		mg::sch::TaskCallback cb(
			[&](mg::sch::Task* aTask) {
				executeCount.IncrementRelaxed();
				delete aTask;
			}
		);

		double startMs = mg::box::GetMillisecondsPrecise();
		for (uint32_t i = 0; i < aTaskCount; ++i)
			sched.Post(new mg::sch::Task(cb));
		while (executeCount.LoadRelaxed() != aTaskCount)
			mg::box::Sleep(1);
		double duration = mg::box::GetMillisecondsPrecise() - startMs;
		Report("Duration: %lf ms", duration);

		UnitTestTaskSchedulerPrintStat(&sched);
	}

	static void
	UnitTestTaskSchedulerMicroOneShot(
		uint32_t aThreadCount,
		uint32_t aTaskCount)
	{
		TestCaseGuard guard("Micro one shot");

		// Checkout speed of one-shot tasks, which are allocated
		// automatically inside of the scheduler.
		Report("Micro one shot test: %u threads, %u tasks", aThreadCount, aTaskCount);
		mg::sch::TaskScheduler sched("tst", 5000);
		sched.Start(aThreadCount);
		mg::box::AtomicU64 executeCount(0);
		mg::sch::TaskCallbackOneShot cb([&](void) -> void {
			executeCount.IncrementRelaxed();
		});

		double startMs = mg::box::GetMillisecondsPrecise();
		for (uint32_t i = 0; i < aTaskCount; ++i)
			sched.PostOneShot(cb);
		while (executeCount.LoadRelaxed() != aTaskCount)
			mg::box::Sleep(1);
		double duration = mg::box::GetMillisecondsPrecise() - startMs;
		Report("Duration: %lf ms", duration);

		UnitTestTaskSchedulerPrintStat(&sched);
	}

	static void
	UnitTestTaskSchedulerPortions(
		uint32_t aThreadCount,
		uint32_t aTaskCount,
		uint32_t aExecuteCount)
	{
		TestCaseGuard guard("Portions");

		// See what happens when tasks are added in multiple
		// steps, not in a single sleep-less loop.
		Report("Portions test: %u threads, %u tasks, %u executes", aThreadCount,
			aTaskCount, aExecuteCount);
		mg::sch::TaskScheduler sched("tst", 5000);
		sched.Start(aThreadCount);
		UTTSchedulerTaskCtx ctx(aTaskCount, aExecuteCount, &sched);

		ctx.CreateHeavy();
		for (uint32_t i = 0; i < aTaskCount; ++i)
		{
			sched.Post(&ctx.myTasks[i]);
			if (i % 10000 == 0)
				mg::box::Sleep(5);
		}
		ctx.WaitAllStopped();

		Report("Max parallel: %u", ctx.myMaxParallel.LoadRelaxed());
		UnitTestTaskSchedulerPrintStat(&sched);
	}

	static void
	UnitTestTaskSchedulerMildLoad(
		uint32_t aThreadCount,
		uint32_t aTaskCount,
		uint32_t aExecuteCount,
		uint32_t aDuration)
	{
		TestCaseGuard guard("Mild load");

		// Try to simulate load close to reality, when RPS may be
		// relatively stable, and not millions.
		TEST_CHECK(aTaskCount % aDuration == 0);
		uint32_t tasksPer50ms = aTaskCount / aDuration * 50;
		Report("Mild load test: %u threads, %u tasks, %u executes, %u per 50ms",
			aThreadCount, aTaskCount, aExecuteCount, tasksPer50ms);
		mg::sch::TaskScheduler sched("tst", 5000);
		sched.Start(aThreadCount);
		UTTSchedulerTaskCtx ctx(aTaskCount, aExecuteCount, &sched);

		ctx.CreateHeavy();
		for (uint32_t i = 0; i < aTaskCount; ++i)
		{
			sched.Post(&ctx.myTasks[i]);
			if (i % tasksPer50ms == 0)
				mg::box::Sleep(50);
		}
		ctx.WaitAllStopped();

		Report("Max parallel: %u", ctx.myMaxParallel.LoadRelaxed());
		UnitTestTaskSchedulerPrintStat(&sched);
	}

	static void
	UnitTestTaskSchedulerTimeouts(
		uint32_t aTaskCount)
	{
		TestCaseGuard guard("Timeouts");

		// The test checks how slow is the scheduler on the
		// slowest case - when tons of tasks are woken up in the
		// order reversed from their insertion. It would cost
		// logarithmic time to remove each of them from the
		// waiting tasks queue's root, which is a binary heap.
		Report("Timeouts test: %u tasks", aTaskCount);
		mg::sch::TaskScheduler sched("tst", 5000);
		sched.Start(1);
		UTTSchedulerTaskCtx ctx(aTaskCount, 1, &sched);

		ctx.CreateMicro();
		for (uint32_t i = 0; i < aTaskCount; ++i)
			ctx.myTasks[i].SetDelay(1000000 + i);

		// Create one special task pushed last, whose execution
		// would mean all the previous tasks surely ended up in
		// the waiting queue.
		mg::box::Atomic<mg::sch::Task*> t;
		t.StoreRelaxed(new mg::sch::Task(
			[&](mg::sch::Task* aTask) {
				t.StoreRelaxed(nullptr);
				delete aTask;
			}
		));

		double startMs = mg::box::GetMillisecondsPrecise();
		ctx.PostAll();
		sched.Post(t.LoadRelaxed());

		while (t.LoadRelaxed() != nullptr)
			mg::box::Sleep(1);

		// Wakeup so as the scheduler would remove tasks from the
		// binary heap from its root - the slowest case. For that
		// wakeup from the end, because the front queue is
		// reversed.
		for (int i = aTaskCount - 1; i >= 0; --i)
			ctx.myTasks[i].PostWakeup();
		ctx.WaitAllExecuted();

		double duration = mg::box::GetMillisecondsPrecise() - startMs;
		Report("Duration: %lf ms", duration);

		ctx.WaitAllStopped();
		UnitTestTaskSchedulerPrintStat(&sched);
	}

	static void
	UnitTestTaskSchedulerSignalStress(
		uint32_t aThreadCount,
		uint32_t aTaskCount,
		uint32_t aExecuteCount)
	{
		TestCaseGuard guard("Signal stress");

		// Ensure the tasks never stuck when signals are used.
		Report("Signal stress test: %u threads, %u tasks, %u executes", aThreadCount,
			aTaskCount, aExecuteCount);
		mg::sch::TaskScheduler sched("tst", 5000);
		sched.Start(aThreadCount);
		UTTSchedulerTaskCtx ctx(aTaskCount, aExecuteCount, &sched);

		ctx.CreateSignaled();
		ctx.PostAll();
		for (uint32_t i = 0; i < aExecuteCount; ++i)
		{
			for (uint32_t j = 0; j < aTaskCount; ++j)
				ctx.myTasks[j].PostSignal();
			uint64_t total = aTaskCount * (i + 1);
			ctx.WaitExecuteCount(total);
		}
		ctx.WaitAllStopped();

		UnitTestTaskSchedulerPrintStat(&sched);
	}

	void
	UnitTestTaskScheduler()
	{
		TestSuiteGuard suite("TaskScheduler");

		UnitTestTaskSchedulerBasic();
		UnitTestTaskSchedulerDestroyWithFront();
		UnitTestTaskSchedulerDestroyWithWaiting();
		UnitTestTaskSchedulerOrder();
		UnitTestTaskSchedulerDomino();
		UnitTestTaskSchedulerWakeup();
		UnitTestTaskSchedulerExpiration();
		UnitTestTaskSchedulerReschedule();
		UnitTestTaskSchedulerSignal();
		UnitTestTaskSchedulerCoroutineBasic();
		UnitTestTaskSchedulerCoroutineAsyncReceiveSignal();
		UnitTestTaskSchedulerCoroutineAsyncExitDelete();
		UnitTestTaskSchedulerCoroutineAsyncExitExec();
		UnitTestTaskSchedulerCoroutineNested();
		UnitTestTaskSchedulerCoroutineDifferentSchedulers();
		UnitTestTaskSchedulerCoroutineStress();
		UnitTestTaskSchedulerMicro(5, 10000000);
		UnitTestTaskSchedulerMicroNew(5, 10000000);
		UnitTestTaskSchedulerMicroOneShot(5, 10000000);
		UnitTestTaskSchedulerBatch(5, 100000, 100);
		UnitTestTaskSchedulerPortions(5, 100000, 100);
		UnitTestTaskSchedulerMildLoad(5, 100000, 1, 10000);
		UnitTestTaskSchedulerTimeouts(1000000);
		UnitTestTaskSchedulerSignalStress(5, 1000000, 5);
	}

}
}
}
