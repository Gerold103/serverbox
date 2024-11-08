#include "IOCore.h"

#include <poll.h>
#include <sys/eventfd.h>

namespace mg {
namespace aio {

	static int
	MakeEventFD(
		const char *name)
	{
		int fd = eventfd(0, EFD_NONBLOCK);
		MG_BOX_ASSERT_F(fd >= 0, "Failed to create %s eventfd: %s",
			name, mg::box::ErrorRaiseErrno()->myMessage.c_str());
		return fd;
	}

	void
	IOCore::PrivPlatformCreate()
	{
		MG_BOX_ASSERT(myRing.ring_fd == -1);
		myRingEventFd = MakeEventFD("ring");
		mySignalEventFd = MakeEventFD("signal");

		io_uring_params uringParams;
		memset(&uringParams, 0, sizeof(uringParams));
		int err = io_uring_queue_init_params(
			MG_IOCORE_IOURING_BATCH, &myRing, &uringParams);
		MG_BOX_ASSERT_F(err == 0,
			"Failed to create io_uring, error: %s",
			mg::box::ErrorRaiseErrno(-err)->myMessage.c_str());
		// Ridiculous, but without this "feature" io_uring can simply drop the cqes if
		// they don't fit into the completion queue. Since just silently loosing the
		// events is a bug in normal apps, need to check for this flag to make sure it
		// won't happen.
		MG_BOX_ASSERT(
			(uringParams.features & IORING_FEAT_NODROP) != 0 &&
			"io_uring doesn't support 'IORING_FEAT_NODROP' - need a newer kernel");

		err = io_uring_register_eventfd(&myRing, myRingEventFd);
		MG_BOX_ASSERT_F(err == 0,
			"Failed to register ring-event-fd in io_uring, error: %s",
			mg::box::ErrorRaiseErrno(-err)->myMessage.c_str());

		// The eventfd content is irrelevant. Just flush into this dummy global variable.
		static uint64_t theEventFdDummyBuf;

		// read() is accounted by the kernel as disk operations in /proc/<pid>/io. But
		// unfortunately io_uring has no any analogue of EPOLLET, and the eventfd can
		// only be consumed with read/readv(). No choice.
		mySignalEvent.myOpcode = MG_IO_URING_OP_READ;
		mySignalEvent.myParamsRead.myFd = mySignalEventFd;
		mySignalEvent.myParamsRead.mySize = sizeof(uint64_t);
		mySignalEvent.myParamsRead.myBuf = &theEventFdDummyBuf;
		mySignalEvent.Lock();
		myToSubmitEvents.Append(&mySignalEvent);
	}

	void
	IOCore::PrivPlatformDestroy()
	{
		MG_BOX_ASSERT(myRing.ring_fd >= 0 && myRingEventFd >= 0 && mySignalEventFd >= 0);
		if (!myToSubmitEvents.IsEmpty())
		{
			// Maybe before exit the core had submitted this last event for itself. But
			// it is internal and is ok.
			MG_BOX_ASSERT(myToSubmitEvents.PopFirst() == &mySignalEvent);
			MG_BOX_ASSERT(myToSubmitEvents.IsEmpty());
		}
		MG_BOX_ASSERT(io_uring_unregister_eventfd(&myRing) == 0);
		io_uring_queue_exit(&myRing);
		myRing.ring_fd = -1;
		MG_BOX_ASSERT(close(myRingEventFd) == 0);
		myRingEventFd = -1;
		MG_BOX_ASSERT(close(mySignalEventFd) == 0);
		mySignalEventFd = -1;
	}

	void
	IOCore::PrivPlatformSignal()
	{
		bool ok = eventfd_write(mySignalEventFd, 1) == 0;
		MG_BOX_ASSERT_F(ok, "Couldn't write to eventfd: %s",
			mg::box::ErrorRaiseErrno()->myMessage.c_str());
	}

	void
	IOCore::PrivKernelRegister(
		IOTask* aTask)
	{
		MG_BOX_ASSERT(aTask->myOperationCount == 0);
		MG_BOX_ASSERT(aTask->myReadyEventCount == 0);
		MG_BOX_ASSERT(aTask->myPendingEventCount == 0);
		MG_BOX_ASSERT(aTask->mySocket != mg::net::theInvalidSocket);
		MG_BOX_ASSERT(myRing.ring_fd >= 0);
		// Yes, io_uring doesn't require any explicit registration of the descriptors.
	}

	bool
	IOCore::PrivScheduleDo()
	{
		MG_DEV_ASSERT(myIsSchedulerWorking.LoadRelaxed());
		io_uring_cqe* cqes[MG_IOCORE_IOURING_BATCH];
		// It is important that the kernel events are dispatched on each scheduling step.
		// Not only when the front queue becomes empty. Because otherwise there will be a
		// starvation - front queue tasks may re-post themselves and the scheduler might
		// not touch io_uring ever. It leads to unfairness towards not so active tasks.
		// This is why the completion entries are collected on each schedule step.
		unsigned evCount = io_uring_peek_batch_cqe(
			&myRing, cqes, MG_IOCORE_IOURING_BATCH);
		bool isExpired;
		IOTaskStatus oldState;
		uint32_t batch;
		uint32_t maxBatch = mySchedBatchSize;
		uint64_t timestamp = mg::box::GetMilliseconds();
		// Don't push ready elements to the queue right away. It is possible that the same
		// task is both in the io_uring output and in the front queue output. Firstly, can
		// merge them to avoid double wakeups. Secondly, a task might need to be closed
		// but if it would be pushed to a worker after io_uring processing, it wouldn't be
		// able to close the socket here because it would be in use in the worker.
		IOTaskForwardList ready;
		// Popping the front queue takes linear time due to how the multi-producer queue
		// is implemented. It is not batched so far, but even for millions of tasks it is
		// a few milliseconds tops.
		IOTask* next;
		IOTask* tail;
		IOEvent* event;
		IOTask* task = myFrontQueue.PopAll(tail);
		myPendingQueue.Append(task, tail);

		//////////////////////////////////////////////////////////////////////////////////
		//
		// Handle kernel events. They are handled before front queue elements because this
		// allows to improve batching of the pending events. If a task appears both from
		// the kernel and the front queue, it gets events from both.
		//
		for (unsigned i = 0; i < evCount; ++i)
		{
			io_uring_cqe* cqe = cqes[i];
			event = (IOEvent*)io_uring_cqe_get_data(cqe);
			task = event->myTask;
			// When task is null, it is the eventfd descriptor, and serves just to wakeup
			// the scheduler. For example, to let it know, that it is time to stop or
			// that the front queue isn't empty anymore.
			if (task == nullptr)
			{
				MG_BOX_ASSERT(event == &mySignalEvent);
				MG_BOX_ASSERT(event->IsLocked());
				MG_BOX_ASSERT(cqe->res == sizeof(uint64_t));
				// The eventfd reading must always be watched. Put it first, out of order.
				myToSubmitEvents.Prepend(&mySignalEvent);
				continue;
			}
			if (cqe->res >= 0)
				event->ReturnBytes(cqe->res);
			else
				event->ReturnError(mg::box::ErrorCodeFromErrno(-cqe->res));
			task->myPendingEvents.Append(event);
			++task->myPendingEventCount;
			oldState = IOTASK_STATUS_WAITING;
			if (!task->myStatus.CmpExchgStrongRelaxed(oldState, IOTASK_STATUS_READY))
			{
				// Is already in a queue somewhere. Let it return to the scheduler via the
				// front queue to decide if need to execute it again.
				if (oldState == IOTASK_STATUS_PENDING || oldState == IOTASK_STATUS_READY)
				{
					// If a cancel event would be completed, it means it was already sent.
					// And tasks having cancel operation submitted aren't supposed to
					// leave the scheduler until the cancellation completes.
					MG_DEV_ASSERT(event != &task->myCancelEvent);
					continue;
				}
				MG_BOX_ASSERT(oldState == IOTASK_STATUS_CLOSING);
				if (!task->myCancelEvent.IsLocked())
				{
					// Cancellation isn't submitted yet. Means that the task still didn't
					// reach the scheduler through the front queue after the user
					// requested its closure. Wait until it comes via the front.
					MG_DEV_ASSERT(event != &task->myCancelEvent);
					continue;
				}
				if (event == &task->myCancelEvent)
				{
					MG_BOX_ASSERT(
						// Successful cancellation.
						cqe->res >= 0 ||
						// Nothing is found.
						cqe->res == -ENOENT ||
						// Too late to cancel. Need to wait for normal completion.
						cqe->res == -EALREADY);
				}
				if (task->myOperationCount > task->myPendingEventCount)
				{
					// Not all operations are cancelled/complete yet. Wait for all of
					// them to come though.
					continue;
				}
				MG_BOX_ASSERT(task->myOperationCount == task->myPendingEventCount);
				task->PrivCloseDo();
				oldState = task->myStatus.ExchangeRelaxed(IOTASK_STATUS_CLOSED);
				MG_BOX_ASSERT_F(oldState == IOTASK_STATUS_CLOSING, "status: %d",
					(int)oldState);
				// Closed and no unfinished operations. Can safely return the task to its
				// owner via the ready queue last time.
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
		io_uring_cq_advance(&myRing, evCount);

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

			// Do not submit the events into the ring right away. There are no any
			// guarantees about the ring's submission queue capacity and how many of the
			// events get flushed with each io_uring-enter. Instead, build a queue of all
			// the events which need submission, and submit them separately in as big
			// batches as possible.
			myToSubmitEvents.Append(std::move(task->myToSubmitEvents));

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
					// future it can be woken up by an external async event; or by the
					// kernel if io_uring will return something for the task's socket; or
					// by the deadline. If the deadline is infinite, don't even need to
					// store it in any real queue.
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
				MG_BOX_ASSERT(!task->myCancelEvent.IsLocked());
				if (task->myPendingEventCount == task->myOperationCount)
				{
					task->PrivCloseDo();
					oldState = task->myStatus.ExchangeRelaxed(IOTASK_STATUS_CLOSED);
					MG_BOX_ASSERT_F(oldState == IOTASK_STATUS_CLOSING, "status: %d",
						(int)oldState);
					// Closed and no unfinished operations. Can safely return the task to
					// its owner via the ready queue last time.
				}
				else
				{
					MG_BOX_ASSERT(task->mySocket != mg::net::theInvalidSocket);
					task->myCancelEvent.myOpcode = MG_IO_URING_OP_CANCEL_FD;
					task->myCancelEvent.myTask = task;
					task->myCancelEvent.myParamsCancelFd.myFd = task->mySocket;
					task->myCancelEvent.Lock();
					++task->myOperationCount;
					myToSubmitEvents.Append(&task->myCancelEvent);
					continue;
				}
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
		// example when it would be worse.
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
			next = task->myNext;
			task->myNext = nullptr;
			MG_DEV_ASSERT(task->myIndex == -1);
			myReadyQueue.PushPending(task);
			task = next;
		}
		myReadyQueue.FlushPending();

		//////////////////////////////////////////////////////////////////////////////////
		//
		// Flush the operations into io_uring. They are scrapped from the incoming front
		// tasks to be submitted in as large batches as possible.
		//
		batch = 0;
		while (!myToSubmitEvents.IsEmpty()) {
			// Sadly, io_uring has no API for batch-get of multiple submission entries.
			// Have to take them one by one.
			io_uring_sqe* sqe = io_uring_get_sqe(&myRing);
			if (sqe == nullptr)
			{
				// It shouldn't be possible that the submission queue is full to the brim.
				// It is supposed to be consumed by the kernel on each io_uring_submit().
				// Otherwise how would the io_uring users be supposed to wait for the
				// submission queue to have space? No way.
				MG_BOX_ASSERT(batch > 0);
				break;
			}
			event = myToSubmitEvents.PopFirst();
			++batch;
			switch(event->myOpcode)
			{
			case MG_IO_URING_OP_CONNECT:
				io_uring_prep_connect(sqe, event->myParamsConnect.myFd,
					&event->myParamsConnect.myAddr.base,
					event->myParamsConnect.myAddrLen);
				break;
			case MG_IO_URING_OP_ACCEPT:
				io_uring_prep_accept(sqe, event->myParamsAccept.myFd,
					&event->myParamsAccept.myAddr.base,
					&event->myParamsAccept.myAddrLen, SOCK_NONBLOCK);
				break;
			case MG_IO_URING_OP_RECVMSG:
				io_uring_prep_recvmsg(sqe, event->myParamsIOMsg.myFd,
					&event->myParamsIOMsg.myMsg, 0);
				break;
			case MG_IO_URING_OP_SENDMSG:
				io_uring_prep_sendmsg(sqe, event->myParamsIOMsg.myFd,
					&event->myParamsIOMsg.myMsg, 0);
				break;
			case MG_IO_URING_OP_CANCEL_FD:
				// Can't use io_uring_prep_cancel_fd(), because it is too new. Not so
				// old Linux distros in their liburing packages easily don't have this
				// function available.
				io_uring_prep_rw(IORING_OP_ASYNC_CANCEL, sqe,
					event->myParamsCancelFd.myFd, nullptr, 0, 0);
				sqe->cancel_flags =
					1 /* IORING_ASYNC_CANCEL_ALL */ |
					2 /* IORING_ASYNC_CANCEL_FD */;
				break;
			case MG_IO_URING_OP_READ:
				io_uring_prep_read(sqe, event->myParamsRead.myFd,
					event->myParamsRead.myBuf, event->myParamsRead.mySize, 0);
				break;
			case MG_IO_URING_OP_NOP:
				MG_BOX_ASSERT(!"Unsupported IO Uring operation");
				break;
			}
			io_uring_sqe_set_data(sqe, event);
		}
		if (batch > 0)
		{
			int rc = io_uring_submit(&myRing);
			MG_BOX_ASSERT(rc > 0 && (uint32_t)rc == batch);
		}

		if (
			// Ready-queue isn't empty? - go help to process it.
			myReadyQueue.Count() > 0 ||
			// Front-queue isn't fully dispatched? - re-enter the scheduler to continue
			// the dispatching.
			!myPendingQueue.IsEmpty() ||
			// Not all io operations are submitted yet? - re-enter the scheduler to
			// submit the rest.
			!myToSubmitEvents.IsEmpty())
			return true;

		// No work to do anywhere. Need to wait for any events from the front or the
		// kernel.
		//
		// On the other hand, the wait must not change the io_uring state. That is, the
		// scheduler shouldn't pop any events. Just wait until they appear. Otherwise
		// would need to handle those events right away instead of just letting them be
		// handled on the next scheduling step. Unfortunately, io_uring methods have no
		// proper way of waiting for the events without consuming them.
		//
		// However, io_uring allows to signal an eventfd when it gets any events. And that
		// eventfd can be waited on using poll().
		//
		// If poll() returns, it means either a timeout, or that the next
		// io_uring_peek_batch_cqe() is going to return some new events.

		pollfd pfd;
		memset(&pfd, 0, sizeof(pfd));
		pfd.fd = myRingEventFd;
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
		else if (myState.LoadRelaxed() != IOCORE_STATE_RUNNING)
		{
			// The scheduler might be woken up by the special event for shutting down.
			// Need to exit instead of going into the infinite sleep.
			return false;
		}
		if (poll(&pfd, 1, pollTimeout) > 0)
		{
			uint64_t num;
			ssize_t rc = read(myRingEventFd, &num, sizeof(num));
			MG_BOX_ASSERT(rc == sizeof(num));
			MG_BOX_ASSERT(num > 0);
		}
		return false;
	}

}
}
