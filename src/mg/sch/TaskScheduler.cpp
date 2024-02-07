#include "TaskScheduler.h"

#include "mg/box/StringFunctions.h"
#include "mg/box/Time.h"

namespace mg {
namespace sch {

	TaskScheduler::TaskScheduler(
		const char* aName,
		uint32_t aThreadCount,
		uint32_t aSubQueueSize)
		: myQueueReady(aSubQueueSize)
		, myExecBatchSize(aSubQueueSize)
		, mySchedBatchSize(aSubQueueSize * aThreadCount)
		, myIsSchedulerWorking(0)
		, myIsStopped(0)
	{
		myThreads.resize(aThreadCount);
		for (TaskSchedulerThread*& t : myThreads)
		{
			t = new TaskSchedulerThread(aName, this);
			t->Start();
		}
	}

	TaskScheduler::~TaskScheduler()
	{
		myIsStopped.StoreRelease(true);
		for (TaskSchedulerThread* t : myThreads)
			t->Stop();
		// It is enough to wake the sched-thread. It will wakeup
		// another worker, and they will wakeup each other like
		// domino.
		mySignalFront.Send();
		for (TaskSchedulerThread* t : myThreads)
			t->StopAndDelete();
		MG_BOX_ASSERT(myQueuePending.IsEmpty());
		MG_BOX_ASSERT(myQueueFront.PopAllFastReversed() == nullptr);
		MG_BOX_ASSERT(myQueueWaiting.Count() == 0);
		MG_BOX_ASSERT(myQueueReady.Count() == 0);
	}

	void
	TaskScheduler::Reserve(
		uint32_t aCount)
	{
		myQueueReady.Reserve(aCount);
	}

	void
	TaskScheduler::Post(
		Task* aTask)
	{
		MG_DEV_ASSERT(!aTask->myIsInQueues);
		aTask->myIsInQueues = true;
		PrivPost(aTask);
	}

	void
	TaskScheduler::Wakeup(
		Task* aTask)
	{
		// Don't do the load inside of the cycle. It is needed
		// only first time. On next iterations the cmpxchg returns
		// the old value anyway.
		TaskStatus old = aTask->myStatus.LoadRelaxed();
		// Note, that the loop is not a busy loop nor a spin-lock.
		// Because it would mean the thread couldn't progress
		// until some other thread does something. Here, on the
		// contrary, the thread does progress always, and even
		// better if other threads don't do anything. Should be
		// one iteration in like 99.9999% cases.
		do
		{
			// Signal and ready mean the task will be executed
			// ASAP anyway. Also can't override the signal,
			// because it is stronger than a wakeup.
			if (old == TASK_STATUS_SIGNALED || old == TASK_STATUS_READY)
				return;
			// Relaxed is fine. The memory sync will happen via the scheduler
			// queues if the wakeup succeeds.
		} while (!aTask->myStatus.CmpExchgWeakRelaxed(
			old, TASK_STATUS_READY));

		// If the task was in the waiting queue. Need to re-push
		// it to let the scheduler know the task must be removed
		// from the queue earlier.
		if (old == TASK_STATUS_WAITING)
			PrivPost(aTask);
	}

	void
	TaskScheduler::Signal(
		Task* aTask)
	{
		// Release-barrier to sync with the acquire-barrier on the
		// signal receipt. Can't be relaxed, because the task might send
		// and receive the signal without the scheduler's participation
		// and can't count on synchronizing any memory via it.
		TaskStatus old = aTask->myStatus.ExchangeRelease(TASK_STATUS_SIGNALED);
		// WAITING - the task was in the waiting queue. Need to
		// re-push it to let the scheduler know the task must be
		// removed from the queue earlier.
		if (old == TASK_STATUS_WAITING)
			PrivPost(aTask);
	}

	inline void
	TaskScheduler::PrivPost(
		Task* aTask)
	{
		MG_DEV_ASSERT(aTask->myIsInQueues);
		if (myQueueFront.Push(aTask))
			mySignalFront.Send();
	}

	bool
	TaskScheduler::PrivSchedule()
	{
		if (myIsSchedulerWorking.ExchangeAcqRel(true))
			return false;

		// Task status operations can all be relaxed inside the
		// scheduler. Syncing writes and reads between producers and
		// workers anyway happens via acquire-release of the front
		// queue - all tasks go through it.
		TaskStatus old;
		Task* t;
		Task* next;
		Task* tail;
		TaskSchedulerQueuePending ready;
		uint64_t deadline;
		uint64_t timestamp = mg::box::GetMilliseconds();
		uint32_t batch;
		uint32_t maxBatch = mySchedBatchSize;

	retry:
		if (PrivIsStopped())
			goto end;

		// -------------------------------------------------------
		// Handle waiting tasks. They are older than the ones in
		// the front queue, so must be handled first.

		batch = 0;
		while (myQueueWaiting.Count() > 0 && ++batch < maxBatch)
		{
			t = myQueueWaiting.GetTop();
			if (t->myDeadline > timestamp)
				break;
			myQueueWaiting.RemoveTop();
			t->myIsExpired = true;

			old = TASK_STATUS_WAITING;
			if (!t->myStatus.CmpExchgStrongRelaxed(old, TASK_STATUS_READY))
			{
				MG_DEV_ASSERT(old == TASK_STATUS_READY ||
					old == TASK_STATUS_SIGNALED);
				// The task is woken up explicitly or signaled. It
				// means the waker thread saw the task in WAITING
				// state.
				//
				// Protocol for wakers in that case is to push the
				// task to the front queue.
				//
				// It means the task can't be added to the ready
				// queue until it is collected from the front
				// queue. Otherwise it is possible to end up with
				// the task added to the ready queue below and at
				// the same time to the front queue by the waker
				// thread.
				continue;
			}
			MG_DEV_ASSERT(old == TASK_STATUS_WAITING);
			ready.Append(t);
		}

		// -------------------------------------------------------
		// Handle front tasks.

		mySignalFront.Receive();
		// Popping the front queue takes linear time due to how the multi-producer queue
		// is implemented. It is not batched so far, but even for millions of tasks it is
		// a few milliseconds tops.
		t = myQueueFront.PopAll(tail);
		myQueuePending.Append(t, tail);
		batch = 0;
		while (!myQueuePending.IsEmpty() && ++batch < maxBatch)
		{
			t = myQueuePending.PopFirst();
			t->myNext = nullptr;
			if (timestamp < t->myDeadline)
			{
				t->myIsExpired = false;
				old = TASK_STATUS_PENDING;
				if (t->myStatus.CmpExchgStrongRelaxed(old, TASK_STATUS_WAITING))
				{
					// The task is not added to the heap in case
					// it is put to sleep until explicit wakeup or
					// a signal. Because anyway it won't be popped
					// ever. And signal/wakeup work fine even if
					// the task is waiting but not in the waiting
					// queue. Only status matters.
					if (t->myDeadline != MG_TIME_INFINITE)
						myQueueWaiting.Push(t);
					else
						MG_DEV_ASSERT(t->myIndex == -1);
					// Even if the task is woken right now, it is
					// ok to add it to the waiting queue. Because
					// it is also added to the front queue by the
					// wakeup, and the scheduler handles this case
					// below.
					continue;
				}
				MG_DEV_ASSERT(
					// The task was woken up or signaled
					// specifically to ignore the deadline.
					old == TASK_STATUS_READY ||
					old == TASK_STATUS_SIGNALED);
			}
			else
			{
				t->myIsExpired = true;
				old = TASK_STATUS_PENDING;
				t->myStatus.CmpExchgStrongRelaxed(old, TASK_STATUS_READY);
				MG_DEV_ASSERT(
					// Normal task reached its dispatch.
					old == TASK_STATUS_PENDING ||
					// The task was woken up or signaled
					// explicitly.
					old == TASK_STATUS_READY ||
					old == TASK_STATUS_SIGNALED);
			}
			// The task can be also stored in the waiting queue
			// and then pushed to the front queue to wake it up or
			// signal earlier. In this case it must be removed
			// from the waiting queue. A task never should be in
			// two queues simultaneously.
			if (t->myIndex >= 0)
				myQueueWaiting.Remove(t);
			ready.Append(t);
		}

		// End of tasks polling.
		// -------------------------------------------------------

		t = ready.PopAll();
		while (t != nullptr)
		{
			next = t->myNext;
			t->myNext = nullptr;
			myQueueReady.PushPending(t);
			t = next;
		}
		myQueueReady.FlushPending();

		if (myQueueReady.Count() == 0 && myQueuePending.IsEmpty())
		{
			// No ready tasks means the other workers already
			// sleep on ready-signal. Or are going to start
			// sleeping any moment. So the sched can't quit. It
			// must retry until either a front task appears, or
			// one of the waiting tasks' deadline is expired.
			if (myQueueWaiting.Count() == 0)
			{
				mySignalFront.ReceiveBlocking();
			}
			else
			{
				deadline = myQueueWaiting.GetTop()->myDeadline;
				timestamp = mg::box::GetMilliseconds();
				if (deadline > timestamp)
				{
					mySignalFront.ReceiveTimed(
						mg::box::TimeDuration(deadline - timestamp));
				}
			}
			goto retry;
		}

	end:
		myIsSchedulerWorking.StoreRelease(false);
		// The signal is absolutely vital to have exactly here.
		// If the signal would not be emitted here, all the
		// workers could block on ready tasks in their loops.
		//
		// It could easily happen like this. Assume there are 2
		// threads. One becomes the scheduler, schedules one task
		// above, and is paused by the OS right *before* the
		// scheduler flag is freed.
		//
		// Then the second thread starts working, executes the
		// ready task, fails to become a sched (the flag is still
		// taken), and goes to infinite sleep waiting for ready
		// tasks.
		//
		// Then the first thread continues execution, frees the
		// flag, also fails to execute anything and also goes to
		// infinite sleep on the same condition.
		//
		// The signal below fixes it so if a thread finished the
		// scheduling, it will either go and execute tasks, or
		// will go sleep, but the signal will wake it again or
		// some another thread to elect a new sched-thread.
		//
		// On the other hand it does not lead to busy-loop. If
		// there are no new expired tasks, eventually one worker
		// will stuck in scheduler role on waiting for new tasks,
		// and other workers will sleep on waiting for ready
		// tasks.
		PrivSignalReady();
		return true;
	}

	bool
	TaskScheduler::PrivExecute(
		Task* aTask)
	{
		if (aTask == nullptr)
			return false;
		MG_DEV_ASSERT(aTask->myIsInQueues);
		aTask->myIsInQueues = false;
		TaskStatus old = TASK_STATUS_READY;
		aTask->myStatus.CmpExchgStrongRelaxed(old, TASK_STATUS_PENDING);
		MG_DEV_ASSERT(old == TASK_STATUS_READY || old == TASK_STATUS_SIGNALED);
		// The task object shall not be accessed anyhow after
		// execution. It may be deleted inside.
		aTask->PrivExecute();
		return true;
	}

	void
	TaskScheduler::PrivWaitReady()
	{
		mySignalReady.ReceiveBlocking();
	}

	void
	TaskScheduler::PrivSignalReady()
	{
		mySignalReady.Send();
	}

	bool
	TaskScheduler::PrivIsStopped()
	{
		return myIsStopped.LoadAcquire();
	}

	TaskSchedulerThread::TaskSchedulerThread(
		const char* aSchedulerName,
		TaskScheduler* aScheduler)
		: Thread(mg::box::StringFormat(
			"mgsch.wrk%s", aSchedulerName).c_str())
		, myScheduler(aScheduler)
		, myExecuteCount(0)
		, myScheduleCount(0)
	{
		myConsumer.Attach(&myScheduler->myQueueReady);
	}

	void
	TaskSchedulerThread::Run()
	{
		uint64_t maxBatch = myScheduler->myExecBatchSize;
		uint64_t batch;
		while (!myScheduler->PrivIsStopped())
		{
			do
			{
				if (myScheduler->PrivSchedule())
					myScheduleCount.IncrementRelaxed();
				batch = 0;
				while (myScheduler->PrivExecute(myConsumer.Pop()) && ++batch < maxBatch);
				myExecuteCount.AddRelaxed(batch);
			} while (batch == maxBatch);
			MG_DEV_ASSERT(batch < maxBatch);

			myScheduler->PrivWaitReady();
		}
		myScheduler->PrivSignalReady();
	}

	void
	TaskOneShot::Execute(
		Task* aTask)
	{
		MG_DEV_ASSERT(aTask == this);
		myCallback();
		delete this;
	}

}
}
