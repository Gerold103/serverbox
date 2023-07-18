#include "IOTask.h"

#include "mg/aio/IOCore.h"

#include "mg/box/IOVec.h"
#include "mg/box/Log.h"
#include "mg/box/Thread.h"

#include "mg/net/Buffer.h"

namespace mg {
namespace aio {

	// For IO worker threads it is the task which is being executed right now. It allows
	// to do some actions right away without going through the scheduler again.
	//
	// In particular it is useful on Windows. Because even if a WSA operation is finished
	// immediately, its completion is delivered asynchronously. Therefore it can't lead to
	// a starvation, when one socket would not let an IO worker go and would infinitely
	// start and complete some operations. In addition the optimization is necessary,
	// because without it the latency would suffer due to too much time needed to complete
	// an operation. For any read/write it would be necessary to re-schedule the task (one
	// async event) + call a WSA function (whose completion is another async event). With
	// the optimization most of the time it is only the latter.
	//
	// On Unix complete means complete right now. This optimization would allow to
	// abuse this and take an IO worker for one socket for too long time when data stream
	// is aggressive. It could be protected with some quota, but according to benches it
	// does not change much anyway. On Linux the ability to check if the task is in a
	// worker is used for sanity checks and to support the feature of fast re-scheduling.
	static MG_THREADLOCAL IOTask* theCurrentIOTask = nullptr;

	IOTask::IOTask()
		: myStatus(IOTASK_STATUS_PENDING)
		, mySocket(mg::net::theInvalidSocket)
		, myNext(nullptr)
		, myDeadline(MG_DEADLINE_INFINITE)
		, myIndex(-1)
		, myCloseGuard(false)
		, myIsClosed(false)
		, myIsInQueues(false)
		, myIsExpired(false)
		, myCore(nullptr)
	{
		PrivConstructPlatform();
	}

	IOTask::~IOTask()
	{
		PrivTouch();
		// Can't destroy the task which is being kept by the worker. In case it would be
		// freed and another task would be allocated in the same memory, it might think it
		// was being executed, which would be false.
		MG_DEV_ASSERT(!IsInWorkerNow());
#if IS_BUILD_DEBUG
		IOTaskStatus status = myStatus.LoadRelaxed();
		MG_DEV_ASSERT(status == IOTASK_STATUS_CLOSED || status == IOTASK_STATUS_PENDING);
#endif
		MG_DEV_ASSERT(mySocket == mg::net::theInvalidSocket);
		MG_DEV_ASSERT(myNext == nullptr);
		PrivDestructPlatform();
	}

	void
	IOTask::Post(
		IOCore& aCore,
		mg::net::Socket aSocket,
		IOSubscription* aSub)
	{
		MG_DEV_ASSERT(myCore == nullptr);
		MG_DEV_ASSERT(aSocket != mg::net::theInvalidSocket);
		PrivAttach(aSub, aSocket);
		aCore.PrivPostFirst(this);
	}

	void
	IOTask::Post(
		IOCore& aCore,
		IOServerSocket* aSocket,
		IOSubscription* aSub)
	{
		MG_DEV_ASSERT(myCore == nullptr);
		MG_DEV_ASSERT(aSocket->mySock != mg::net::theInvalidSocket);
		// Move the socket ownership to the task.
		PrivAttach(aSub, aSocket->mySock);
		aSocket->mySock = mg::net::theInvalidSocket;
#if MG_IOCORE_USE_IOCP
		// Only IOCP with its weird WSA API needs the context to be kept. Unix-based
		// implementations are simpler in this sense. Can just delete its context.
		MG_DEV_ASSERT(myServerSock == nullptr);
		myServerSock = aSocket;
#else
		delete aSocket;
#endif
		aCore.PrivPostFirst(this);
	}

	void
	IOTask::Post(
		IOCore& aCore,
		IOSubscription* aSub)
	{
		MG_DEV_ASSERT(myCore == nullptr);
		PrivAttach(aSub, mg::net::theInvalidSocket);
		aCore.PrivPostFirst(this);
	}

	void
	IOTask::PostClose()
	{
		if (!PrivCloseStart())
			return;
		// It is important to have close start and set of CLOSING state separated. Between
		// them can provide close parameters like whether it should be immediate or
		// graceful. Otherwise if CLOSING would be set right away, then it could be
		// immediately noticed by the scheduler (if the task was already in the front
		// queue) and would be actually closed + deleted in some worker thread even before
		// this function ends.
		IOTaskStatus oldStatus = myStatus.ExchangeRelaxed(IOTASK_STATUS_CLOSING);
		MG_DEV_ASSERT_F(
			oldStatus == IOTASK_STATUS_PENDING ||
			oldStatus == IOTASK_STATUS_READY ||
			oldStatus == IOTASK_STATUS_WAITING, "status: %d", (int)oldStatus);

		if (oldStatus == IOTASK_STATUS_WAITING)
			return myCore->PrivRePost(this);
	}

	void
	IOTask::PostWakeup()
	{
		// Fast path.
		IOTaskStatus oldStatus = IOTASK_STATUS_WAITING;
		if (myStatus.CmpExchgStrongRelaxed(oldStatus, IOTASK_STATUS_READY))
			return myCore->PrivRePost(this);
		// Fail, but the task was awake anyway.
		if (oldStatus == IOTASK_STATUS_READY)
			return;

		// Slow path. If meet CLOSED then nothing to wakeup anymore. If meet CLOSING, then
		// it is same as READY - the task is already awake.
		while (oldStatus == IOTASK_STATUS_PENDING ||
			oldStatus == IOTASK_STATUS_WAITING)
		{
			IOTaskStatus newStatus = oldStatus;
			if (!myStatus.CmpExchgStrongRelaxed(newStatus, IOTASK_STATUS_READY))
			{
				// Fail. Retry.
				oldStatus = newStatus;
				continue;
			}
			// Success.
			if (oldStatus == IOTASK_STATUS_PENDING)
				return;
			if (oldStatus == IOTASK_STATUS_WAITING)
				return myCore->PrivRePost(this);
			MG_DEV_ASSERT_F(false, "status: %d", (int)oldStatus);
		}
	}

	bool
	IOTask::IsInWorkerNow() const
	{
		return theCurrentIOTask == this;
	}

	void
	IOTask::AttachSocket(
		mg::net::Socket aSocket)
	{
		MG_DEV_ASSERT(IsInWorkerNow());
		MG_DEV_ASSERT(!IsClosed());
		MG_DEV_ASSERT(mySocket == mg::net::theInvalidSocket);
		mySocket = aSocket;
		myCore->PrivKernelRegister(this);
	}

	bool
	IOTask::Send(
		const mg::net::Buffer* aHead,
		uint32_t aByteOffset,
		IOEvent& aEvent)
	{
		mg::box::IOVec bufs[mg::box::theIOVecMaxCount];
		uint32_t count = mg::net::BuffersToIOVecsForWrite(
			aHead, aByteOffset, bufs, mg::box::theIOVecMaxCount);
		return Send(bufs, count, aEvent);
	}

	bool
	IOTask::Recv(
		mg::net::Buffer* aHead,
		IOEvent& aEvent)
	{
		mg::box::IOVec bufs[mg::box::theIOVecMaxCount];
		uint32_t count = mg::net::BuffersToIOVecsForRead(
			aHead, bufs, mg::box::theIOVecMaxCount);
		return Recv(bufs, count, aEvent);
	}

	bool
	IOTask::ConnectStart(
		const mg::net::Host& aHost,
		IOEvent& aEvent)
	{
		mg::box::Error::Ptr err;
		mg::net::Socket sock = SocketCreate(aHost.GetAddressFamily(),
			mg::net::TRANSPORT_PROT_DEFAULT, err);
		if (sock == mg::net::theInvalidSocket)
			return aEvent.ReturnError(err->myCode);
		if (ConnectStart(sock, aHost, aEvent))
			return true;
		MG_DEV_ASSERT(aEvent.IsError());
		if (!HasSocket())
			mg::net::SocketClose(sock);
		return false;
	}

	bool
	IOTask::PrivExecute()
	{
		MG_DEV_ASSERT(theCurrentIOTask == nullptr);
		MG_DEV_ASSERT(myCore != nullptr);
		IOArgs args;
		PrivDumpReadyEvents(args);
		// Need to reset the deadline to prevent infinite rescheduling. It is safe to do
		// here, because the value is always owned by one thread, and can only be changed
		// by Execute() call below.
		myDeadline = MG_DEADLINE_INFINITE;
		theCurrentIOTask = this;
		if (myIsClosed)
		{
			// Pop the listener so as to free the place for another one in case the task
			// will be reused.
			IOSubscription::Ptr listener = std::move(mySub);
			PrivCloseEndPlatform();
			myCore = nullptr;
			listener->OnEvent(args);
			// Pop the task from the global before its deletion. Helps to assert that the
			// task is never deleted while in work.
			theCurrentIOTask = nullptr;
			listener.Clear();
			return false;
		}
		mySub->OnEvent(args);
		theCurrentIOTask = nullptr;
		return true;
	}

	bool
	IOTask::PrivCloseStart()
	{
		return !myCloseGuard.ExchangeRelaxed(true);
	}

	void
	IOTask::PrivCloseDo()
	{
		MG_DEV_ASSERT(myCloseGuard.LoadRelaxed());
		MG_DEV_ASSERT(!myIsClosed);
		MG_DEV_ASSERT(myNext == nullptr);
		MG_DEV_ASSERT(myStatus.LoadRelaxed() == IOTASK_STATUS_CLOSING);
		// Closed flag is ok to update non-atomically. Close is done in the scheduler, so
		// the task is not executed in any worker now and they can't see this flag before
		// the task's socket is finally removed from the kernel.
		myIsClosed = true;
		// Socket can be invalid if the task wasn't attached to a socket.
		if (mySocket == mg::net::theInvalidSocket)
			return;
		mg::net::SocketClose(mySocket);
		mySocket = mg::net::theInvalidSocket;
	}

	void
	IOTask::PrivAttach(
		IOSubscription* aSub,
		mg::net::Socket aSocket)
	{
		MG_DEV_ASSERT(!mySub.IsSet());
		MG_DEV_ASSERT(mySocket == mg::net::theInvalidSocket);
		MG_DEV_ASSERT(myNext == nullptr);
		MG_DEV_ASSERT(aSocket != mg::net::theInvalidSocket);
		IOTaskStatus oldStatus = IOTASK_STATUS_CLOSED;
		if (myStatus.CmpExchgStrongRelaxed(oldStatus, IOTASK_STATUS_PENDING))
		{
			// The task is reused.
			MG_DEV_ASSERT(IsInWorkerNow());
			MG_DEV_ASSERT(myIsClosed);
			myIsClosed = false;
			// Clear the guard in the end to prevent a new closure while the task is being
			// re-created.
			MG_DEV_ASSERT(myCloseGuard.ExchangeRelaxed(false));
		}
		else
		{
			MG_DEV_ASSERT(!myIsClosed);
		}
		mySub.Set(aSub);
		mySocket = aSocket;
	}

	IOServerSocket*
	SocketBind(
		const mg::net::Host& aHost,
		mg::box::Error::Ptr& aOutErr)
	{
		mg::net::Socket sock = SocketCreate(aHost.GetAddressFamily(),
			mg::net::TRANSPORT_PROT_TCP, aOutErr);
		if (sock == mg::net::theInvalidSocket)
			return nullptr;
		mg::box::Error::Ptr nonCritErr;
		if (!mg::net::SocketSetFixReuseAddr(sock, nonCritErr))
		{
			MG_LOG_WARN("io_core_bind.05", "Couldn't fix reuseaddr - %s",
				nonCritErr->myMessage.c_str());
		}
		if (aHost.IsIPV6() && !mg::net::SocketSetDualStack(sock, true, nonCritErr))
		{
			MG_LOG_WARN("io_core_bind.05", "Couldn't enable dualstack - %s",
				nonCritErr->myMessage.c_str());
		}
		if (!mg::net::SocketBind(sock, aHost, aOutErr))
		{
			mg::net::SocketClose(sock);
			return nullptr;
		}
		IOServerSocket* res = new IOServerSocket();
		res->mySock = sock;
		return res;
	}

}
}
