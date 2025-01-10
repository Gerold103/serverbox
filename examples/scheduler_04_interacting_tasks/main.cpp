#include "mg/sch/TaskScheduler.h"

#include <iostream>

//
// It is assumed you have seen the previous examples in this section, and some previously
// explained things don't need another repetition.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// A realistic example how one task might submit some async work to be executed by another
// task, which in turn would wake the first one up, when the work is done.
//

static void
TaskSubmitRequest(
	mg::sch::Task& aSender)
{
	// A sub-task is created. But normally for such need one would have a pre-running task
	// or a thread or some other sort of async executor, which takes requests and executes
	// them.
	mg::sch::Task* worker = new mg::sch::Task();
	worker->SetCallback([](
		mg::sch::Task& aSelf,
		mg::sch::Task& aSender) -> mg::box::Coro {

		std::cout << "Worker: sent request\n";
		// Lets simulate like if the request takes 1 second to get done. For that the
		// task would yield for 1 second and then get executed again.
		uint64_t t1 = mg::box::GetMilliseconds();
		aSelf.SetDelay(1000);
		co_await aSelf.AsyncYield();

		// This flag means the task was woken up because its deadline was due.
		// Technically, it could wake up spuriously, but here we know it can't happen.
		// Proper production code should still be ready to that though.
		MG_BOX_ASSERT(aSelf.IsExpired());

		uint64_t t2 = mg::box::GetMilliseconds();
		std::cout << "Worker: received response, took " << t2 - t1 << " msec\n";

		// Wakeup the original task + let it know the request is actually done. The
		// original task would be able to tell that by seeing that it's got a signal, not
		// just a spurious wakeup.
		aSender.PostSignal();

		// 'delete self' + co_return wouldn't work here. Because deletion of the self
		// would destroy the C++ coroutine object. co_return would then fail with
		// use-after-free. For such 'delete and exit' case there is the special helper.
		co_await aSelf.AsyncExitDelete();
	}(*worker, aSender));
	mg::sch::TaskScheduler::This().Post(worker);
}

int
main()
{
	mg::sch::Task task;

	// The scheduler is defined after the task, so the task's destructor is not called
	// before the scheduler is terminated. It would cause the task to be destroyed while
	// in use.
	// Normally one would allocate tasks on the heap and make them delete themselves when
	// they are finished.
	mg::sch::TaskScheduler scheduler("tst",
		1, // Thread count.
		5  // Subqueue size.
	);

	task.SetCallback([](
		mg::sch::Task& aSelf) -> mg::box::Coro {

		// The task wants something to be done asynchronously. It would then submit the
		// work (could be another task, could be an HTTP client, or something alike) and
		// wait for a notification when it is done.
		std::cout << "Main: submit request\n";
		TaskSubmitRequest(aSelf);

		// The waiting is in a loop to protect the code from spurious wakeups.
		do {
			// Make sure to signalize that the task wants to wait (infinitely) until a
			// signal comes. Without this the task would be just re-scheduled immediately
			// and it would be a busy-loop.
			aSelf.SetWait();
		} while (!co_await aSelf.AsyncReceiveSignal());

		std::cout << "Main: finish\n";
		co_return;
	}(task));
	scheduler.Post(&task);
	return 0;
}
