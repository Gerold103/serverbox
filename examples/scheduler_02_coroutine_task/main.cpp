#include "mg/sch/TaskScheduler.h"

#include <iostream>

//
// It is assumed you have seen the previous examples in this section, and some previously
// explained things don't need another repetition.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// A simple example of how to use C++ stack-less coroutines with TaskScheduler.
//

int
main()
{
	mg::sch::Task task;

	// Do not use lambda capture-params! The thing is that C++ coroutines are not
	// functions. They are **results** of functions. When a lambda returns a C++
	// corotuine, it needs to be invoked to return this coroutine. And after invocation
	// the lambda is destroyed in this case. Which makes it not possible to use any lambda
	// data, like captures. See for further explanation:
	// https://stackoverflow.com/questions/60592174/lambda-lifetime-explanation-for-c20-coroutines
	//
	// Luckily, this problem is trivial to workaround. Just pass your values as arguments.
	// Then the coroutine object captures them.
	task.SetCallback([](
		mg::sch::Task& aSelf) -> mg::box::Coro {

		// Imagine the task sends a request to some async network client or alike.
		std::cout << "Sending request ...\n";

		// After sending, the task would wait for a response. Here it is simplified, so
		// the task just yields and gets continued right away.
		// Yield with no deadline or delay set simply re-schedules the task.
		co_await aSelf.AsyncYield();

		std::cout << "Received response!\n";
		co_await aSelf.AsyncYield();
		std::cout << "Finish\n";
		co_return;
	}(task));

	// The scheduler is defined after the task, so the task's destructor is not called
	// before the scheduler is terminated. It would cause the task to be destroyed while
	// in use.
	// Normally one would allocate tasks on the heap and make them delete themselves when
	// they are finished.
	mg::sch::TaskScheduler scheduler("tst",
		1, // Thread count.
		5  // Subqueue size.
	);
	scheduler.Post(&task);
	return 0;
}
