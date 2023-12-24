#include "Task.h"

#include "mg/box/Assert.h"
#include "mg/box/Time.h"
#include "mg/sch/TaskScheduler.h"

namespace mg {
namespace sch {

	void
	Task::PostWakeup()
	{
		// Don't do the load inside of the loop. It is needed only first time. On next
		// iterations the cmpxchg returns the old value anyway.
		TaskStatus old = myStatus.LoadRelaxed();
		// Note, that the loop is not a busy loop nor a spin-lock. Because it would mean
		// the thread couldn't progress until some other thread does something. Here, on
		// the contrary, the thread does progress always, and even better if other threads
		// don't do anything. Should be one iteration in like 99.9999% cases.
		do
		{
			// Signal and ready mean the task will be executed ASAP anyway. Also can't
			// override the signal, because it is stronger than a wakeup.
			if (old == TASK_STATUS_SIGNALED || old == TASK_STATUS_READY)
				return;
			// Relaxed is fine. The memory sync will happen via the scheduler queues if
			// the wakeup succeeds.
		} while (!myStatus.CmpExchgWeakRelaxed(old, TASK_STATUS_READY));

		// If the task was in the waiting queue. Need to re-push it to let the scheduler
		// know the task must be removed from the queue earlier.
		if (old == TASK_STATUS_WAITING)
			myScheduler->PrivPost(this);
	}

	void
	Task::PostSignal()
	{
		// Release-barrier to sync with the acquire-barrier on the signal receipt. Can't
		// be relaxed, because the task might send and receive the signal without the
		// scheduler's participation and can't count on synchronizing any memory via it.
		TaskStatus old = myStatus.ExchangeRelease(TASK_STATUS_SIGNALED);
		// WAITING - the task was in the waiting queue. Need to re-push it to let the
		// scheduler know the task must be removed from the queue earlier.
		if (old == TASK_STATUS_WAITING)
			myScheduler->PrivPost(this);
	}

	void
	Task::SetDelay(
		uint32_t aDelay)
	{
		PrivTouch();
		myDeadline = mg::box::GetMilliseconds() + aDelay;
	}

	void
	Task::AdjustDelay(
		uint32_t aDelay)
	{
		AdjustDeadline(mg::box::GetMilliseconds() + aDelay);
	}

	void
	Task::SetDeadline(
		uint64_t aDeadline)
	{
		PrivTouch();
		myDeadline = aDeadline;
	}

	void
	Task::AdjustDeadline(
		uint64_t aDeadline)
	{
		PrivTouch();
		if (aDeadline < myDeadline)
			myDeadline = aDeadline;
	}

	bool
	Task::IsExpired() const
	{
		PrivTouch();
		return myIsExpired;
	}

	uint64_t
	Task::GetDeadline() const
	{
		PrivTouch();
		return myDeadline;
	}

	bool
	Task::ReceiveSignal()
	{
		PrivTouch();
		TaskStatus oldStatus = TASK_STATUS_SIGNALED;
		// Acquire-barrier to sync with the release-barrier on the
		// signal sending. Can't be relaxed, because the task might send
		// and receive the signal without the scheduler's participation
		// and can't count on synchronizing any memory via it.
		return myStatus.CmpExchgStrongAcquire(oldStatus, TASK_STATUS_PENDING);
	}

	void
	Task::PrivExecute()
	{
		PrivTouch();
		// Deadline is reset so as the task wouldn't use it again
		// if re-posted. Next post should specify a new deadline
		// or omit it. In that way when Post, you can always be
		// sure the old task deadline won't affect the next
		// execution.
		myDeadline = 0;
		myCallback(this);
	}

	void
	Task::PrivCreate()
	{
		myNext = nullptr;
		myIndex = -1;
		myStatus.StoreRelease(TASK_STATUS_PENDING);
		myScheduler = nullptr;
		myDeadline = 0;
		myIsExpired = false;
	}

	void
	Task::PrivTouch() const
	{
		MG_DEV_ASSERT_F(myScheduler == nullptr,
			"An attempt to modify a task while it is in scheduler");
	}

}
}
