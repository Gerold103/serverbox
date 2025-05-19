#include "TaskScheduler.h"

#include "mg/box/StringFunctions.h"
#include "mg/box/Time.h"

namespace mg {
namespace sch {

	thread_local TaskScheduler* TaskScheduler::ourCurrent = nullptr;

	TaskScheduler::TaskScheduler(
		const char* aName,
		uint32_t aSubQueueSize)
		: myExecBatchSize(aSubQueueSize)
		, mySchedBatchSize(myExecBatchSize)
		, myQueueReady(aSubQueueSize)
		, myName(aName)
	{
	}

	TaskScheduler::~TaskScheduler()
	{
		MG_BOX_ASSERT(WaitEmpty());
		Stop();
		MG_BOX_ASSERT(myThreads.empty());
		MG_BOX_ASSERT(myQueuePending.IsEmpty());
		MG_BOX_ASSERT(myQueueFront.PopAllFastReversed() == nullptr);
		MG_BOX_ASSERT(myQueueWaiting.Count() == 0);
		MG_BOX_ASSERT(myQueueReady.Count() == 0);
	}

	void
	TaskScheduler::Start(
		uint32_t aThreadCount)
	{
		PrivSchedulerLock();
		mySchedBatchSize = myExecBatchSize * aThreadCount;
		MG_BOX_ASSERT(myThreads.empty());
		myThreads.resize(aThreadCount);
		for (TaskSchedulerThread*& t : myThreads)
		{
			t = new TaskSchedulerThread(myName.c_str(), this);
			t->Start();
		}
		PrivSchedulerUnlock();
	}

	bool
	TaskScheduler::IsEmpty()
	{
		PrivSchedulerLock();
		for (TaskSchedulerThread* worker : myThreads)
		{
			// It is important to check the worker states. Even if all the queues are
			// empty, it doesn't mean there are no tasks and won't be new ones. Because
			// the currently running tasks might produce new tasks, and then the scheduler
			// isn't empty.
			if (worker->GetState() != TASK_SCHEDULER_WORKER_STATE_IDLE)
			{
				PrivSchedulerUnlock();
				return false;
			}
		}
		bool isEmpty =
			myQueueFront.IsEmpty() &&
			myQueueWaiting.Count() == 0 &&
			myQueuePending.IsEmpty() &&
			myQueueReady.Count() == 0;
		PrivSchedulerUnlock();
		return isEmpty;
	}

	bool
	TaskScheduler::WaitEmpty(
		mg::box::TimeLimit aTimeLimit)
	{
		mg::box::TimePoint deadline = aTimeLimit.ToPointFromNow();
		while (!IsEmpty())
		{
			// Polling is slow and stupid, but 'wait empty' is needed in such situations
			// where these milliseconds won't matter at all. And having it done via
			// polling allows to keep the place trivially simple.
			if (mg::box::GetMilliseconds() > deadline.myValue)
				return false;
			mg::box::Sleep(10);
		}
		return true;
	}

	void
	TaskScheduler::Stop()
	{
		PrivSchedulerLock();
		if (myThreads.empty())
		{
			PrivSchedulerUnlock();
			return;
		}
		mySchedBatchSize = myExecBatchSize;
		for (TaskSchedulerThread* t : myThreads)
			t->Stop();
		PrivSignalReady();
		// Yes, keep holding the lock while stopping the threads. They don't need to enter
		// the scheduler-role anyway.
		for (TaskSchedulerThread* t : myThreads)
			t->StopAndDelete();
		myThreads.clear();
		PrivSchedulerUnlock();
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
		MG_DEV_ASSERT(aTask->myScheduler == nullptr);
		aTask->myScheduler = this;
		PrivPost(aTask);
	}

	void
	TaskScheduler::PrivPost(
		Task* aTask)
	{
		MG_DEV_ASSERT(aTask->myScheduler == this);
		if (myQueueFront.Push(aTask))
			mySignalFront.Send();
	}

	inline void
	TaskScheduler::PrivSchedulerLock()
	{
		mySchedulerMutex.Lock([this]() { mySignalFront.Send(); });
	}

	inline bool
	TaskScheduler::PrivSchedulerTryLock()
	{
		return mySchedulerMutex.TryLock();
	}

	bool
	TaskScheduler::PrivSchedule()
	{
		if (!PrivSchedulerTryLock())
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

		// Popping the front queue takes linear time due to how the multi-producer queue
		// is implemented. It is not batched so far, but even for millions of tasks it is
		// a few milliseconds tops.
		// Note, the front signal is not received, because under a high load the queue is
		// rarely expected to be empty. So receiving this signal would be pointless. At
		// the same time, if the queue does become empty for a while, the front signal is
		// received when the scheduler has nothing to do and goes to sleep on that signal.
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
			// No ready tasks means the other workers already sleep on ready-signal. Or
			// are going to start sleeping any moment. So the sched can't quit. It must
			// try to wait until something new happens which would require processing.
			if (myQueueWaiting.Count() > 0)
			{
				deadline = myQueueWaiting.GetTop()->myDeadline;
				timestamp = mg::box::GetMilliseconds();
				if (deadline > timestamp)
				{
					mySignalFront.ReceiveTimed(
						mg::box::TimeDuration(deadline - timestamp));
				}
			}
			else
			{
				mySignalFront.ReceiveBlocking();
			}
		}

		PrivSchedulerUnlock();
		return true;
	}

	inline void
	TaskScheduler::PrivSchedulerUnlock()
	{
		mySchedulerMutex.Unlock();
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
	}

	bool
	TaskScheduler::PrivExecute(
		Task* aTask)
	{
		if (aTask == nullptr)
			return false;
		MG_DEV_ASSERT(aTask->myScheduler == this);
		aTask->myScheduler = nullptr;
		TaskStatus old = TASK_STATUS_READY;
		aTask->myStatus.CmpExchgStrongRelaxed(old, TASK_STATUS_PENDING);
		MG_DEV_ASSERT(old == TASK_STATUS_READY || old == TASK_STATUS_SIGNALED);
		// The task object shall not be accessed anyhow after
		// execution. It may be deleted inside.
		aTask->PrivExecute();
		return true;
	}

	inline void
	TaskScheduler::PrivWaitReady()
	{
		mySignalReady.ReceiveBlocking();
	}

	inline void
	TaskScheduler::PrivSignalReady()
	{
		mySignalReady.Send();
	}

	TaskSchedulerThread::TaskSchedulerThread(
		const char* aSchedulerName,
		TaskScheduler* aScheduler)
		: Thread(mg::box::StringFormat(
			"mgsch.wrk%s", aSchedulerName).c_str())
		, myScheduler(aScheduler)
		, myState(TASK_SCHEDULER_WORKER_STATE_IDLE)
		, myExecuteCount(0)
		, myScheduleCount(0)
	{
		myConsumer.Attach(&myScheduler->myQueueReady);
	}

	inline TaskSchedulerWorkerState
	TaskSchedulerThread::GetState() const
	{
		return myState.LoadRelaxed();
	}

	void
	TaskSchedulerThread::Run()
	{
		TaskScheduler::ourCurrent = myScheduler;
		uint64_t maxBatch = myScheduler->myExecBatchSize;
		uint64_t batch;
		while (!StopRequested())
		{
			myState.StoreRelaxed(TASK_SCHEDULER_WORKER_STATE_RUNNING);
			do
			{
				if (myScheduler->PrivSchedule())
					myScheduleCount.IncrementRelaxed();
				batch = 0;
				while (myScheduler->PrivExecute(myConsumer.Pop()) && ++batch < maxBatch);
				myExecuteCount.AddRelaxed(batch);
			} while (batch == maxBatch);
			MG_DEV_ASSERT(batch < maxBatch);
			myState.StoreRelaxed(TASK_SCHEDULER_WORKER_STATE_IDLE);
			myScheduler->PrivWaitReady();
		}
		myState.StoreRelaxed(TASK_SCHEDULER_WORKER_STATE_IDLE);
		myScheduler->PrivSignalReady();
		MG_BOX_ASSERT(TaskScheduler::ourCurrent == myScheduler);
		TaskScheduler::ourCurrent = nullptr;
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
