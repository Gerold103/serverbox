#include "mg/sch/TaskScheduler.h"

#include <iostream>

//
// It is assumed you have seen the previous examples in this section, and some previously
// explained things don't need another repetition.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// An example how a task can consist of multiple untrivial steps, with yields in between,
// and with context used by those steps.
//

class MyTask
	: public mg::sch::Task
{
public:
	MyTask()
		: Task([this](mg::sch::Task* aSelf) {
			TaskSendRequest(aSelf);
		}) {}

private:
	// Step 1.
	void
	TaskSendRequest(
		mg::sch::Task* aSelf)
	{
		// The scheduler always gives the "self" as an argument. This is not needed when
		// the task is inherited. But quite handy when a task is just a lambda.
		MG_BOX_ASSERT(aSelf == this);

		// Imagine that the task sends an HTTP request via some other module, and is woken
		// up, when the request ends. To execute the next step. For that the task changes
		// its callback which will be executed when the task is woken up next time.
		std::cout << "Sending request ...\n";
		aSelf->SetCallback([this](mg::sch::Task* aSelf) {
			TaskRecvResponse(aSelf);
		});
		mg::sch::TaskScheduler::This().Post(aSelf);
	}

	// Step 2.
	void
	TaskRecvResponse(
		mg::sch::Task* aSelf)
	{
		MG_BOX_ASSERT(aSelf == this);

		// Lets make another step, with a third callback which would be the final one.
		std::cout << "Received response!\n";
		aSelf->SetCallback([this](mg::sch::Task *aSelf) {
			TaskFinish(aSelf);
		});
		mg::sch::TaskScheduler::This().Post(aSelf);
	}

	// Step 3.
	void
	TaskFinish(
		mg::sch::Task* aSelf)
	{
		MG_BOX_ASSERT(aSelf == this);
		std::cout << "Finish\n";
	}

	// Here one would normally put various members needed by that task for its context.
	// For example, request and user data related to this task.
};

int
main()
{
	MyTask task;

	// The scheduler is defined after the task, so the task's destructor is not called
	// before the scheduler is terminated. It would cause the task to be destroyed while
	// in use.
	// Normally one would allocate tasks on the heap and make them delete themselves when
	// they are finished.
	mg::sch::TaskScheduler scheduler("tst",
		5  // Subqueue size.
	);
	scheduler.Start(1);
	scheduler.Post(&task);
	return 0;
}
