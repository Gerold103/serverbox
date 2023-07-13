#include "IOTask.h"

#include "mg/box/IOVec.h"
#include "mg/box/Log.h"
#include "mg/box/Thread.h"

#include "mg/net/Buffer.h"
#include "mg/net/Socket.h"

namespace mg {
namespace asio {

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

	bool
	IOTask::IsInWorkerNow() const
	{
		return theCurrentIOTask == this;
	}

	bool
	IOTask::Send(
		const mg::net::Buffer* aHead,
		uint32_t aByteOffset,
		IOEvent& aEvent)
	{
		mg::box::IOVec bufs[theIOSendBatch];
		mg::box::IOVec* buf = bufs;
		mg::box::IOVec* bufEnd = bufs + theIOSendBatch;
		buf->myData = aHead->myWData + aByteOffset;
		MG_DEV_ASSERT(aHead->myPos >= aByteOffset);
		buf->mySize = aHead->myPos - aByteOffset;
		++buf;
		aHead = aHead->myNext.GetPointer();
		while (buf < bufEnd && aHead != nullptr)
		{
			buf->myData = aHead->myWData;
			buf->mySize = aHead->myPos;
			++buf;
			aHead = aHead->myNext.GetPointer();
		}
		return Send(bufs, (uint32_t)(buf - bufs), aEvent);
	}

	bool
	IOTask::Recv(
		mg::net::Buffer* aHead,
		IOEvent& aEvent)
	{
		mg::box::IOVec bufs[theIORecvBatch];
		mg::box::IOVec* buf = bufs;
		mg::box::IOVec* bufEnd = bufs + theIORecvBatch;
		buf->myData = aHead->myWData + aHead->myPos;
		MG_DEV_ASSERT(aHead->myCapacity >= aHead->myPos);
		buf->mySize = aHead->myCapacity - aHead->myPos;
		++buf;
		aHead = aHead->myNext.GetPointer();
		while (aHead != nullptr && buf < bufEnd)
		{
			MG_DEV_ASSERT(aHead->myPos == 0);
			buf->myData = aHead->myWData;
			buf->mySize = aHead->myCapacity;
			++buf;
			aHead = aHead->myNext.GetPointer();
		}
		return Recv(bufs, (uint32_t)(buf - bufs), aEvent);
	}

	bool
	IOTask::ConnectStart(
		const mg::net::Host& aHost,
		IOEvent& aEvent)
	{
		mg::box::Error::Ptr err;
		mg::net::Socket sock = mg::net::SocketCreate(aHost.GetAddressFamily(),
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
	IOBind(
		uint16_t aPort,
		mg::box::Error::Ptr& aOutErr)
	{
		mg::net::Socket sock = mg::net::SocketCreate(mg::net::ADDR_FAMILY_IPV6,
			mg::net::TRANSPORT_PROT_TCP, aOutErr);
		if (sock == mg::net::theInvalidSocket)
			return nullptr;
		mg::box::Error::Ptr nonCritErr;
		if (!mg::net::SocketSetFixReuseAddr(sock, nonCritErr))
		{
			MG_LOG_WARN("io_core_bind.05", "Couldn't fix reuseaddr - %s",
				nonCritErr->myMessage.c_str());
		}
		if (!mg::net::SocketSetDualStack(sock, true, nonCritErr))
		{
			MG_LOG_WARN("io_core_bind.05", "Couldn't enable dualstack - %s",
				nonCritErr->myMessage.c_str());
		}
		mg::net::Host host = mg::net::HostMakeAllIPV6(aPort);
		if (!mg::net::SocketBind(sock, host, aOutErr))
		{
			mg::net::SocketClose(sock);
			return nullptr;
		}
		IOServerSocket* res = new IOServerSocket();
		res->myPort = aPort;
		res->mySock = sock;
		return res;
	}

}
}
