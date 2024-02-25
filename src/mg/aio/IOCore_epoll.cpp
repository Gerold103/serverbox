#include "IOCore.h"

#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace mg {
namespace aio {

	enum
	{
		// A mask of events each socket should listen for.
		MG_AIO_EPOLL_EVENTS =
			EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLPRI | EPOLLET | EPOLLOUT,
	};

	void
	IOCore::PrivPlatformCreate()
	{
		MG_BOX_ASSERT(myNativeCore < 0 && myEventFd < 0);
		myNativeCore = epoll_create1(0);
		MG_BOX_ASSERT_F(myNativeCore >= 0, "Failed to create epoll, error: %s",
			mg::box::ErrorRaiseErrno()->myMessage.c_str());

		myEventFd = eventfd(0, EFD_NONBLOCK);
		MG_BOX_ASSERT_F(myEventFd >= 0, "Failed to create eventfd: %s",
			mg::box::ErrorRaiseErrno()->myMessage.c_str());

		epoll_event event;
		memset(&event, 0, sizeof(event));
		event.events = EPOLLIN | EPOLLET;
		bool ok = epoll_ctl(myNativeCore, EPOLL_CTL_ADD, myEventFd, &event) == 0;
		MG_BOX_ASSERT_F(ok, "Failed to register eventfd in epoll: %s",
			mg::box::ErrorRaiseErrno()->myMessage.c_str());
	}

	void
	IOCore::PrivPlatformDestroy()
	{
		MG_BOX_ASSERT(myNativeCore >= 0 && myEventFd >= 0);
		MG_BOX_ASSERT(close(myNativeCore) == 0);
		myNativeCore = -1;
		MG_BOX_ASSERT(close(myEventFd) == 0);
		myEventFd = -1;
	}

	void
	IOCore::PrivPlatformSignal()
	{
		bool ok = eventfd_write(myEventFd, 1) == 0;
		MG_BOX_ASSERT_F(ok, "Couldn't write to eventfd: %s",
			mg::box::ErrorRaiseErrno()->myMessage.c_str());
	}

	void
	IOCore::PrivKernelRegister(
		IOTask* aTask)
	{
		MG_BOX_ASSERT(aTask->myReadyEvents == 0);
		MG_BOX_ASSERT(aTask->myPendingEvents == 0);
		MG_BOX_ASSERT(aTask->mySocket != mg::net::theInvalidSocket);
		MG_BOX_ASSERT(myNativeCore >= 0);
		epoll_event event;
		memset(&event, 0, sizeof(event));
		event.data.ptr = aTask;
		event.events = MG_AIO_EPOLL_EVENTS;
		bool ok = epoll_ctl(myNativeCore, EPOLL_CTL_ADD, aTask->mySocket, &event) == 0;
		// A valid scenario, when EPOLL_CTL_ADD could fail, does not exist. If it does not
		// work, something is seriously wrong.
		MG_BOX_ASSERT_F(ok, "Couldn't add a socket to epoll: %s",
			mg::box::ErrorRaiseErrno()->myMessage.c_str());
	}

	void
	IOCore::PrivKernelUnregister(
		IOTask* aTask)
	{
		MG_BOX_ASSERT(myNativeCore >= 0);
		if (aTask->mySocket == mg::net::theInvalidSocket)
			return;

		epoll_event event;
		memset(&event, 0, sizeof(event));
		bool ok = epoll_ctl(myNativeCore, EPOLL_CTL_DEL, aTask->mySocket, &event) == 0;
		// A valid scenario, when EPOLL_CTL_DEL could fail, does not exist. If it does not
		// work, something is seriously wrong.
		MG_BOX_ASSERT_F(ok, "Couldn't remove a socket from epoll: %s",
			mg::box::ErrorRaiseErrno()->myMessage.c_str());
	}

	bool
	IOCore::PrivScheduleDo()
	{
		MG_DEV_ASSERT(myIsSchedulerWorking.LoadRelaxed());
		epoll_event evs[MG_IOCORE_EPOLL_BATCH];
		// It is important that the kernel events are dispatched on each scheduling step.
		// Not only when the front queue becomes empty. Because otherwise there will be a
		// starvation - front queue tasks may re-post themselves and the scheduler might
		// not touch epoll ever. It leads to unfairness towards not so active tasks. This
		// is why epoll_wait() is done on each schedule step.
		//
		// It uses 0 timeout in order to return immediately because otherwise the
		// scheduler would block if there are no kernel events, even if the front queue is
		// not empty.
		int evCount = epoll_wait(myNativeCore, evs, MG_IOCORE_EPOLL_BATCH, 0);
		if (evCount < 0)
		{
			MG_BOX_ASSERT(errno == EINTR);
			evCount = 0;
		}
		bool isExpired;
		IOTaskStatus oldState;
		uint32_t batch;
		uint32_t maxBatch = mySchedBatchSize;
		uint64_t timestamp = mg::box::GetMilliseconds();
		// Don't push ready elements to the queue right away. It is possible that the same
		// task is both in the epoll output and in the front queue output. Firstly, can
		// merge them to avoid double wakeups. Secondly, a task might need to be closed
		// but if it would be pushed to a worker after epoll processing, it wouldn't be
		// able to close the socket here because it would be in use in the worker.
		IOTaskForwardList ready;
		// Popping the front queue takes linear time due to how the multi-producer queue
		// is implemented. It is not batched so far, but even for millions of tasks it is
		// a few milliseconds tops.
		IOTask* next;
		IOTask* tail;
		IOTask* t = myFrontQueue.PopAll(tail);
		myPendingQueue.Append(t, tail);

		//////////////////////////////////////////////////////////////////////////////////
		//
		// Handle kernel events. They are handled before front queue elements because this
		// allows to improve batching of the pending events. If a task appears both from
		// the kernel and the front queue, it gets events from both.
		//
		for (int i = 0; i < evCount; ++i)
		{
			t = (IOTask*)evs[i].data.ptr;
			// When d is null, it corresponds to an eventfd descriptor, and serves just to
			// wakeup this thread. For example, to let it know, that it is time to stop.
			if (t == nullptr)
				continue;
			// It is not documented if epoll will return the full event mask on each
			// edge-triggered update (EPOLLET), if only some of the events has changed.
			// Therefore the only safe action here is to merge them in userspace using OR.
			t->myPendingEvents |= evs[i].events;
			oldState = IOTASK_STATUS_WAITING;
			if (!t->myStatus.CmpExchgStrongRelaxed(oldState, IOTASK_STATUS_READY))
			{
				MG_DEV_ASSERT(oldState != IOTASK_STATUS_CLOSED);
				// Is already in a queue somewhere. Let it return to the scheduler via the
				// front queue to decide if need to execute it again.
				continue;
			}
			if (t->myIndex >= 0)
				myWaitingQueue.Remove(t);

			t->myReadyEvents |= t->myPendingEvents;
			t->myPendingEvents = 0;
			t->myIsExpired = timestamp >= t->myDeadline;
			ready.Append(t);
		}

		//////////////////////////////////////////////////////////////////////////////////
		//
		// Handle the front queue. Do it after kernel events. Because this way some of the
		// front queue tasks could piggyback their kernel events from the code above.
		//
		batch = 0;
		while (!myPendingQueue.IsEmpty() && ++batch < maxBatch)
		{
			t = myPendingQueue.PopFirst();
			t->myNext = nullptr;
			// Pending events belong to the scheduler, and are never updated or read by
			// other threads. Safe to check the deadline non-atomically.
			isExpired = timestamp >= t->myDeadline;
			if (t->myPendingEvents != 0 || isExpired)
			{
				// Has events already. It could happen if they were added while the task
				// was being executed in a worker thread when new events arrived from the
				// kernel and were merged into the mask.
				oldState = IOTASK_STATUS_PENDING;
				t->myStatus.CmpExchgStrongRelaxed(oldState, IOTASK_STATUS_READY);
				MG_DEV_ASSERT(oldState != IOTASK_STATUS_CLOSED);
			}
			else
			{
				// Try to wait for new kernel events. But it could happen, that the task
				// was woken up while was in the front queue. So its status not
				// necessarily is PENDING.
				oldState = IOTASK_STATUS_PENDING;
				if (t->myStatus.CmpExchgStrongRelaxed(oldState, IOTASK_STATUS_WAITING))
				{
					// The task entered WAITING state successfully. It means it doesn't
					// have anything to do and should not be added to the ready queue. In
					// future it can be woken by an external async event; or by the kernel
					// if epoll_wait() will return something for the task's socket; or by
					// the deadline.
					// If the deadline is infinite, don't even need to store it in any
					// real queue.
					if (t->myDeadline != MG_TIME_INFINITE)
						myWaitingQueue.Push(t);
					else
						MG_DEV_ASSERT(t->myIndex == -1);
					continue;
				}
			}

			// Going to the ready queue anyway. So must be removed from the waiting queue.
			if (t->myIndex >= 0)
				myWaitingQueue.Remove(t);

			if (oldState == IOTASK_STATUS_CLOSING)
			{
				PrivKernelUnregister(t);
				// After close the task is added to the ready queue one last time to be
				// destroyed in one of the worker threads, and to deliver the close event.
				t->PrivCloseDo();
				oldState = t->myStatus.ExchangeRelaxed(IOTASK_STATUS_CLOSED);
				MG_BOX_ASSERT_F(oldState == IOTASK_STATUS_CLOSING, "status: %d",
					(int)oldState);
				// IO events are not delivered if the task is closed. Because its socket
				// is already closed and the descriptor is not valid. The task owner must
				// release all the operations waiting for IO when sees the task is closed.
				t->myReadyEvents = 0;
			}
			else
			{
				MG_DEV_ASSERT_F(
					// Closed tasks never return to the scheduler. That is the whole idea
					// of CLOSED status.
					oldState != IOTASK_STATUS_CLOSED &&
					// WAITING status is always replaced before push to the front queue.
					oldState != IOTASK_STATUS_WAITING, "status: %d", (int)oldState);
				// Ready events can be accessed by a worker thread and by the scheduler.
				// But only by one thread at a time. Since the task is here (in front
				// queue), it can't be in a worker thread. Safe to update non-atomically.
				// Need to preserve the old non-consumed ready-events, hence OR is used.
				t->myReadyEvents |= t->myPendingEvents;
			}
			t->myPendingEvents = 0;
			t->myIsExpired = isExpired;

			ready.Append(t);
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
			t = myWaitingQueue.GetTop();
			// Sorted by deadline. The top is not expired = nothing is expired.
			if (t->myDeadline > timestamp)
				break;
			myWaitingQueue.RemoveTop();

			oldState = IOTASK_STATUS_WAITING;
			if (t->myStatus.CmpExchgStrongRelaxed(oldState, IOTASK_STATUS_READY))
			{
				// If it has pending events, it wouldn't be waiting.
				MG_DEV_ASSERT(t->myPendingEvents == 0);
				t->myIsExpired = true;
				ready.Append(t);
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

		t = ready.PopAll();
		while (t != nullptr)
		{
			next = t->myNext;
			t->myNext = nullptr;
			MG_DEV_ASSERT(t->myIndex == -1);
			myReadyQueue.PushPending(t);
			t = next;
		}
		myReadyQueue.FlushPending();
		if (myReadyQueue.Count() > 0 || !myPendingQueue.IsEmpty())
			return true;

		// No ready tasks and they won't appear - only the scheduler populates the
		// ready-queue. Need to wait for any events.
		//
		// On the other hand, the wait must not change the epoll queue state. Otherwise
		// will need to handle them right away instead of just letting these events be
		// handled on the next schedule step. Unfortunately, epoll_wait() can't be used
		// with 0 array of events to just wait.
		//
		// The waiting is done using a tricky, but totally legal and documented feature of
		// epoll - *it can be nested*. Epoll file descriptor can be used as any other file
		// descriptor in any polling mechanism to wait until epoll has some events to
		// return from epoll_wait(). Here it is done using poll(), because it is the
		// simplest and fastest option when need to wait for a single fd.
		//
		// If poll() returns, it means either a timeout, or that the next epoll_wait()
		// will return something immediately.

		pollfd pfd;
		memset(&pfd, 0, sizeof(pfd));
		pfd.fd = myNativeCore;
		pfd.events = POLLIN;
		int pollTimeout = -1;
		if (myWaitingQueue.Count() != 0)
		{
			uint64_t deadline = myWaitingQueue.GetTop()->myDeadline;
			timestamp = mg::box::GetMilliseconds();
			if (timestamp >= deadline)
				return false;
			uint64_t duration = deadline - timestamp;
			if (duration > INT_MAX)
				duration = INT_MAX;
			pollTimeout = (int)duration;
		}
		poll(&pfd, 1, pollTimeout);
		return false;
	}

}
}
