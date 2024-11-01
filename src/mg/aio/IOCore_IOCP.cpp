#include "IOCore.h"

namespace mg {
namespace aio {

	void
	IOCore::PrivPlatformCreate()
	{
		MG_BOX_ASSERT(myNativeCore == nullptr);
		myNativeCore = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		bool ok = myNativeCore != nullptr;
		MG_BOX_ASSERT_F(ok, "Failed to create IOCP, error: %d", GetLastError());
	}

	void
	IOCore::PrivPlatformDestroy()
	{
		MG_BOX_ASSERT(myNativeCore != nullptr);
		MG_BOX_ASSERT(CloseHandle(myNativeCore));
	}

	void
	IOCore::PrivPlatformSignal()
	{
		bool ok = PostQueuedCompletionStatus(myNativeCore, 0, (ULONG_PTR)nullptr,
			nullptr);
		MG_DEV_ASSERT_F(ok, "Couldn't post completion status: %d", GetLastError());
	}

	void
	IOCore::PrivKernelRegister(
		IOTask* aTask)
	{
		MG_BOX_ASSERT(aTask->myOperationCount == 0);
		MG_BOX_ASSERT(aTask->myReadyEventCount == 0);
		MG_BOX_ASSERT(aTask->myPendingEventCount == 0);
		MG_BOX_ASSERT(aTask->mySocket != mg::net::theInvalidSocket);
		MG_BOX_ASSERT(myNativeCore != nullptr);
		HANDLE handle = CreateIoCompletionPort((HANDLE)aTask->mySocket, myNativeCore,
			(ULONG_PTR)aTask, 0);
		if (handle != nullptr)
			return;
		// A valid scenario, when CreateIoCompletionPort could fail, does not exist. If it
		// does not work, something is seriously wrong.
		MG_BOX_ASSERT_F(false, "Couldn't add a socket to IOCP: %d\n", GetLastError());
	}

	bool
	IOCore::PrivScheduleDo()
	{
		MG_DEV_ASSERT(myIsSchedulerWorking.LoadRelaxed());
		OVERLAPPED_ENTRY overs[MG_IOCORE_IOCP_BATCH];
		ULONG overCount;
		DWORD byteCount;
		bool didWait = false;
		DWORD timeout = 0;
		bool isExpired;
		bool ok;
		IOTaskStatus oldState;
		// Don't push ready elements to the queue right away. It is possible that the same
		// task is both in the iocp output and in the front queue output. Firstly, can
		// merge them to avoid double wakeups. Secondly, a task might need to be closed
		// but if it would be pushed to a worker after iocp processing, it wouldn't be
		// able to close the socket here because it would be in use in the worker.
		IOTaskForwardList ready;
		IOTask* nextTask;
		IOTask* tail;
		IOTask* task;
		IOEvent* event;
		OVERLAPPED_ENTRY* over;
		OVERLAPPED_ENTRY* overEnd;
		uint64_t timestamp;
		uint32_t batch;
		uint32_t maxBatch = mySchedBatchSize;

	retry:
		// It is important that the kernel events are dispatched on each scheduling step.
		// Not only when the front queue becomes empty. Because otherwise there will be a
		// starvation - front queue tasks may re-post themselves and the scheduler might
		// not touch iocp ever. It leads to unfairness towards not so active tasks. This
		// is why iocp poll is done on each schedule step.
		//
		// It uses 0 timeout at first iteration in order to return immediately because
		// otherwise the scheduler would block if there are no kernel events, even if the
		// front queue is not empty.
		//
		// At second iteration the timeout becomes non-zero when the scheduler sees there
		// are no ready tasks and therefore it must try to wait.
		overCount = 0;
		ok = GetQueuedCompletionStatusEx(myNativeCore, overs, MG_IOCORE_IOCP_BATCH,
			&overCount, timeout, false);
		if (!ok)
		{
			int err = GetLastError();
			MG_BOX_ASSERT_F(err == WAIT_TIMEOUT, "Unexpected error from IOCP: %d\n", err);
			overCount = 0;
		}
		timestamp = mg::box::GetMilliseconds();
		// Popping the front queue takes linear time due to how the multi-producer queue
		// is implemented. It is not batched so far, but even for millions of tasks it is
		// a few milliseconds tops.
		task = myFrontQueue.PopAll(tail);
		myPendingQueue.Append(task, tail);

		//////////////////////////////////////////////////////////////////////////////////
		//
		// Handle kernel events. They are handled before front queue elements because this
		// allows to improve batching of the pending events. If a task appears both from
		// the kernel and the front queue, it gets events from both.
		//
		over = overs;
		overEnd = over + overCount;
		for (; over < overEnd; ++over)
		{
			// When overlapped is null, it corresponds to the special event defined by
			// IOCore and used for signaling IOCP, and serves just to wakeup this thread.
			// For example, to let it know, that it is time to stop.
			if (over->lpOverlapped == nullptr)
				continue;
			event = CONTAINING_RECORD(over->lpOverlapped, IOEvent, myOverlap);
			task = (IOTask*)over->lpCompletionKey;
			MG_DEV_ASSERT(&event->myOverlap == over->lpOverlapped);
			DWORD unusedFlags = 0;
			if (WSAGetOverlappedResult(task->mySocket, &event->myOverlap, &byteCount,
				false, &unusedFlags))
				event->ReturnBytes(byteCount);
			else
				event->ReturnError(mg::box::ErrorCodeWSA());
			task->myPendingEvents.Append(event);
			++task->myPendingEventCount;
			oldState = IOTASK_STATUS_WAITING;
			if (!task->myStatus.CmpExchgStrongRelaxed(oldState, IOTASK_STATUS_READY))
			{
				// Is already in a queue somewhere. Let it return to the scheduler via the
				// front queue to decide if need to execute it again.
				if (oldState != IOTASK_STATUS_CLOSED)
					continue;
				// Closed means it couldn't be removed from the scheduler right away via
				// the front queue - when its socket was closed, it still had not finished
				// operations inside of iocp.
				MG_BOX_ASSERT(task->myReadyEventCount == 0);
				MG_BOX_ASSERT(task->myPendingEventCount <= task->myOperationCount);
				if (task->myPendingEventCount != task->myOperationCount)
					continue;
			}
			MG_DEV_ASSERT(task->myReadyEventCount == 0);
			MG_DEV_ASSERT(task->myReadyEvents.IsEmpty());
			task->myReadyEvents = std::move(task->myPendingEvents);
			task->myReadyEventCount = task->myPendingEventCount;
			task->myPendingEventCount = 0;
			task->myIsExpired = timestamp >= task->myDeadline;

			if (task->myIndex >= 0)
				myWaitingQueue.Remove(task);
			ready.Append(task);
		}

		//////////////////////////////////////////////////////////////////////////////////
		//
		// Handle the front queue. Do it after kernel events. Because this way some of the
		// front queue tasks could piggyback their kernel events from the code above.
		//
		batch = 0;
		while (!myPendingQueue.IsEmpty() && ++batch < maxBatch)
		{
			task = myPendingQueue.PopFirst();
			task->myNext = nullptr;
			// The task is either new (didn't have any events yet), or has returned from a
			// worker thread (it must have consumed all the events).
			MG_DEV_ASSERT(task->myReadyEventCount == 0);
			MG_DEV_ASSERT(task->myReadyEvents.IsEmpty());
			// Pending events belong to the scheduler, and are never updated or read by
			// other threads. Safe to check the deadline non-atomically.
			isExpired = timestamp >= task->myDeadline;
			if (task->myPendingEventCount != 0 || isExpired)
			{
				// Has events already. It could happen if they were added while the task
				// was being executed in a worker thread when new events arrived from the
				// kernel and were added to the task.
				oldState = IOTASK_STATUS_PENDING;
				task->myStatus.CmpExchgStrongRelaxed(oldState, IOTASK_STATUS_READY);
				MG_BOX_ASSERT(oldState != IOTASK_STATUS_CLOSED);
			}
			else
			{
				// Try to wait for new kernel events. But it could happen, that the task
				// was woken up while was in the front queue. So its status not
				// necessarily is PENDING.
				oldState = IOTASK_STATUS_PENDING;
				if (task->myStatus.CmpExchgStrongRelaxed(oldState, IOTASK_STATUS_WAITING))
				{
					// The task entered WAITING state successfully. It means it doesn't
					// have anything to do and should not be added to the ready queue. In
					// future it can be woken by an external async event; or by the kernel
					// if iocp will return something for the task's socket; or by the
					// deadline. If the deadline is infinite, don't even need to store it
					// in any real queue.
					if (task->myDeadline != MG_TIME_INFINITE)
						myWaitingQueue.Push(task);
					else
						MG_DEV_ASSERT(task->myIndex == -1);
					continue;
				}
			}

			// Going to the ready queue anyway. So must be removed from the waiting queue.
			if (task->myIndex >= 0)
				myWaitingQueue.Remove(task);

			if (oldState == IOTASK_STATUS_CLOSING)
			{
				// Safe to close. The task can't be used in an external thread now
				// (because it is just popped from the front queue).
				task->PrivCloseDo();
				oldState = task->myStatus.ExchangeRelaxed(IOTASK_STATUS_CLOSED);
				MG_BOX_ASSERT_F(oldState == IOTASK_STATUS_CLOSING, "status: %d",
					(int)oldState);
				if (task->myOperationCount != task->myPendingEventCount)
				{
					// Closed, but not all operations has returned from iocp yet. Must
					// wait for them before can return the task to the external code.
					MG_BOX_ASSERT(task->myOperationCount > task->myPendingEventCount);
					continue;
				}
				// Closed and no unfinished operations. Can safely return the task to its
				// owner via the ready queue last time.
			}
			else
			{
				MG_DEV_ASSERT_F(
					// Closed tasks never return to the scheduler. That is the whole idea
					// of CLOSED status.
					oldState != IOTASK_STATUS_CLOSED &&
					// Waiting status is always replaced before push to the front queue.
					oldState != IOTASK_STATUS_WAITING, "status: %d", (int)oldState);
			}
			// Ready events can be accessed by a worker thread and by the scheduler.
			// But only by one thread at a time. Since the task is here (in front
			// queue), it can't be in a worker thread. Safe to update non-atomically.
			task->myReadyEvents = std::move(task->myPendingEvents);
			task->myReadyEventCount = task->myPendingEventCount;
			task->myPendingEventCount = 0;
			task->myIsExpired = isExpired;

			ready.Append(task);
		}

		//////////////////////////////////////////////////////////////////////////////////
		//
		// Handle expired tasks. Do it after the front queue dispatch. Because consider
		// what happens when a task was waiting and then woken up + pushed to the front
		// queue. And it is also expired.
		//
		// If the waiting queue would be dispatched first, it would fail the atomic
		// cmpxchg of setting state to PENDING if it was WAITING. Because it is already
		// PENDING. So total cost of such task handling would be at least 2 atomics: 1
		// during waiting queue and 1 during front queue handling.
		//
		// If the waiting queue is dispatched last, the task would be removed from it
		// during the front queue handling. So its handling would cost at least 1 atomic.
		// This is cheaper. Even though the case is a bit specific. But there is no
		// example when it would be more worse.
		//
		batch = 0;
		while (myWaitingQueue.Count() > 0 && ++batch < maxBatch)
		{
			task = myWaitingQueue.GetTop();
			// Sorted by deadline. The top is not expired = nothing is expired.
			if (task->myDeadline > timestamp)
				break;
			myWaitingQueue.RemoveTop();

			oldState = IOTASK_STATUS_WAITING;
			if (task->myStatus.CmpExchgStrongRelaxed(oldState, IOTASK_STATUS_READY))
			{
				// If it has pending events, it wouldn't be waiting.
				MG_DEV_ASSERT(task->myPendingEventCount == 0);
				task->myIsExpired = true;
				ready.Append(task);
			}
			else
			{
				// Can be READY. In case the task was woken up after the front queue was
				// dispatched, but before this moment. Then the waker thread will push the
				// task to the front queue, can't touch it now.
				//
				// Can be CLOSING by the same reasons as READY.
				MG_DEV_ASSERT_F(
					// Can't be CLOSED - it would have been removed earlier then, during
					// kernel and front queues dispatch.
					oldState != IOTASK_STATUS_CLOSED &&
					// PENDING task can be in the front queue, if it comes back from an IO
					// worker. Then it can't be in the waiting queue. Also it could become
					// PENDING during the front and kernel queues dispatch, but then it
					// would be removed from the waiting queue.
					oldState != IOTASK_STATUS_PENDING, "status: %d", (int)oldState);
			}
		}
		//
		// End of processing
		//
		//////////////////////////////////////////////////////////////////////////////////

		task = ready.PopAll();
		while (task != nullptr)
		{
			nextTask = task->myNext;
			task->myNext = nullptr;
			MG_DEV_ASSERT(task->myIndex == -1);
			myReadyQueue.PushPending(task);
			task = nextTask;
		}
		myReadyQueue.FlushPending();
		if (myReadyQueue.Count() > 0 || !myPendingQueue.IsEmpty())
			return true;

		// No ready tasks and they won't appear - only the scheduler populates the
		// ready-queue. Need to wait for any events.
		//
		// Waiting is done at most once to make this function iterative and to be able to
		// exit from here when the thread is shutting down.
		if (didWait)
			return false;

		if (myWaitingQueue.Count() != 0)
		{
			uint64_t deadline = myWaitingQueue.GetTop()->myDeadline;
			timestamp = mg::box::GetMilliseconds();
			if (timestamp >= deadline)
				goto retry;
			uint64_t duration = deadline - timestamp;
			// Subtract 1 because this 'infinite' isn't really infinite. It is just 4
			// bytes unsigned number which is < 50 days. Need to wakeup at when they
			// expire. Despite it sounds unrealistic, this scenario is theoretically
			// possible.
			if (duration > INFINITE)
				duration = INFINITE - 1;
			timeout = (DWORD)duration;
		}
		else if (myState.LoadRelaxed() != IOCORE_STATE_RUNNING)
		{
			// The scheduler might be woken up by the special event for shutting down.
			// Need to exit instead of going into the infinite sleep.
			return false;
		}
		else
		{
			timeout = INFINITE;
		}
		didWait = true;
		goto retry;
	}

}
}
