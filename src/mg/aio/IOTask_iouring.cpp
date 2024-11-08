#include "IOTask.h"

#include "mg/box/Algorithm.h"
#include "mg/box/IOVec.h"

namespace mg {
namespace aio {

	IOServerSocket::IOServerSocket()
		: mySock(mg::net::theInvalidSocket)
	{
	}

	IOServerSocket::~IOServerSocket()
	{
		if (mySock != mg::net::theInvalidSocket)
			mg::net::SocketClose(mySock);
	}

	//////////////////////////////////////////////////////////////////////////////////////

	void
	IOTask::PrivConstructPlatform()
	{
		myReadyEventCount = 0;
		myPendingEventCount = 0;
		myOperationCount = 0;
	}

	void
	IOTask::PrivDestructPlatform()
	{
		MG_BOX_ASSERT(myReadyEvents.IsEmpty());
		MG_BOX_ASSERT(myReadyEventCount == 0);
		MG_BOX_ASSERT(myPendingEvents.IsEmpty());
		MG_BOX_ASSERT(myPendingEventCount == 0);
		MG_BOX_ASSERT(myOperationCount == 0);
		MG_BOX_ASSERT(myToSubmitEvents.IsEmpty());
	}

	void
	IOTask::OperationStart()
	{
		PrivTouch();
		++myOperationCount;
	}

	bool
	IOTask::Send(
		const mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		IOEvent& aEvent)
	{
		MG_BOX_ASSERT(!aEvent.IsLocked());
		MG_BOX_ASSERT(mySocket != mg::net::theInvalidSocket);
		IOUringParamsIOMsg& params = aEvent.myParamsIOMsg;

		aEvent.myOpcode = MG_IO_URING_OP_SENDMSG;
		aEvent.myTask = this;
		params.myFd = mySocket;
		memset(&params.myMsg, 0, sizeof(params.myMsg));
		params.myMsg.msg_iovlen = mg::box::Min(aBufferCount, theIOUringMaxBufCount);
		params.myMsg.msg_iov = mg::box::IOVecToNative(params.myData);
		memcpy(params.myData, aBuffers,
			params.myMsg.msg_iovlen * sizeof(aBuffers[0]));

		OperationStart();
		myToSubmitEvents.Append(&aEvent);
		aEvent.Lock();
		return true;
	}

	bool
	IOTask::Recv(
		mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		IOEvent& aEvent)
	{
		MG_BOX_ASSERT(!aEvent.IsLocked());
		MG_BOX_ASSERT(mySocket != mg::net::theInvalidSocket);
		IOUringParamsIOMsg& params = aEvent.myParamsIOMsg;

		aEvent.myOpcode = MG_IO_URING_OP_RECVMSG;
		aEvent.myTask = this;
		params.myFd = mySocket;
		memset(&params.myMsg, 0, sizeof(params.myMsg));
		params.myMsg.msg_iovlen = mg::box::Min(aBufferCount, theIOUringMaxBufCount);
		params.myMsg.msg_iov = mg::box::IOVecToNative(params.myData);
		memcpy(params.myData, aBuffers,
			params.myMsg.msg_iovlen * sizeof(aBuffers[0]));

		OperationStart();
		myToSubmitEvents.Append(&aEvent);
		aEvent.Lock();
		return true;
	}

	bool
	IOTask::ConnectStart(
		mg::net::Socket aSocket,
		const mg::net::Host& aHost,
		IOEvent& aEvent)
	{
		MG_BOX_ASSERT(!aEvent.IsLocked());
		MG_BOX_ASSERT(mySocket == mg::net::theInvalidSocket &&
			aSocket != mg::net::theInvalidSocket);

		AttachSocket(aSocket);
		MG_BOX_ASSERT(mySocket == aSocket);
		IOUringParamsConnect& params = aEvent.myParamsConnect;

		aEvent.myOpcode = MG_IO_URING_OP_CONNECT;
		aEvent.myTask = this;
		params.myFd = mySocket;
		params.myAddrLen = aHost.GetSockaddrSize();
		MG_BOX_ASSERT(sizeof(params.myAddr) >= params.myAddrLen);
		memcpy(&params.myAddr, &aHost.myAddr, params.myAddrLen);

		OperationStart();
		myToSubmitEvents.Append(&aEvent);
		aEvent.Lock();
		return true;
	}

	bool
	IOTask::ConnectUpdate(
		IOEvent& aEvent)
	{
		MG_BOX_ASSERT(IsInWorkerNow());
		MG_BOX_ASSERT(mySocket != mg::net::theInvalidSocket);
		if (aEvent.IsLocked())
			return true;
		if (aEvent.IsError())
		{
			// There was merged a patch into the kernel, which makes io_uring not let
			// EINPROGRESS into the userspace. It is handled internally. No need to handle
			// it like for the other schedulers (epoll, kqueue).
			// https://lore.kernel.org/io-uring/36e61a79-8e7c-e869-99d3-eaafdd6ad63d@kernel.dk/T/#u
			return false;
		}
		MG_BOX_ASSERT(aEvent.GetBytes() == 0);
		return true;
	}

	mg::net::Socket
	IOTask::Accept(
		IOEvent& aEvent,
		mg::net::Host& aOutPeer)
	{
		MG_BOX_ASSERT(IsInWorkerNow());
		// Lock = spurious wakeup. Keep waiting then.
		if (aEvent.IsLocked())
			return mg::net::theInvalidSocket;
		if (aEvent.IsEmpty())
		{
			MG_BOX_ASSERT(mySocket != mg::net::theInvalidSocket);
			aEvent.myOpcode = MG_IO_URING_OP_ACCEPT;
			aEvent.myTask = this;
			aEvent.myParamsAccept.myAddrLen = sizeof(aEvent.myParamsAccept.myAddr);
			aEvent.myParamsAccept.myFd = mySocket;
			OperationStart();
			myToSubmitEvents.Append(&aEvent);
			aEvent.Lock();
			return mg::net::theInvalidSocket;
		}
		if (aEvent.IsError())
		{
			// Non-critical error means the socket was closed remotely before was
			// accepted. Then the only way is to retry.
			if (!mg::net::SocketIsAcceptErrorCritical(aEvent.GetError()))
			{
				aEvent.PopError();
				Reschedule();
			}
			return mg::net::theInvalidSocket;
		}
		aOutPeer.Set(&aEvent.myParamsAccept.myAddr.base);
		return aEvent.PopBytes();
	}

	bool
	IOTask::ProcessArgs(
		const IOArgs& aArgs,
		mg::box::Error::Ptr& /* aOutErr */)
	{
		MG_DEV_ASSERT(!IsClosed());
		MG_DEV_ASSERT(IsInWorkerNow());

		IOEvent* event = aArgs.myEvents;
		while (event != nullptr)
		{
			event->Unlock();
			event = event->myNext;
		}
		return true;
	}

	void
	IOTask::PrivDumpReadyEvents(
		IOArgs& aOutArgs)
	{
		PrivTouch();
		MG_DEV_ASSERT(myReadyEventCount >= 0);
		MG_DEV_ASSERT(myReadyEventCount <= myOperationCount);
		// Ready events can be accessed by a worker thread and by the scheduler. But only
		// by one thread at a time. Since the task is here (in the worker thread), it
		// can't be in the scheduler. Safe to read and update non-atomically.
		aOutArgs.myEvents = myReadyEvents.PopAll();
		myOperationCount -= myReadyEventCount;
		myReadyEventCount = 0;
	}

	void
	IOTask::PrivCloseEnd()
	{
		MG_BOX_ASSERT(myStatus.LoadRelaxed() == IOTASK_STATUS_CLOSED);
		// Task couldn't return to the scheduler and get new pending events.
		MG_BOX_ASSERT(myPendingEventCount == 0);
		MG_BOX_ASSERT(myPendingEvents.IsEmpty());
		// Ready events are already dumped into IOArgs.
		MG_BOX_ASSERT(myReadyEventCount == 0);
		MG_BOX_ASSERT(myReadyEvents.IsEmpty());
		// IOCP and io_uring can report task as closed only after all its operations are
		// complete.
		MG_BOX_ASSERT(myOperationCount == 0);
		// Otherwise the operation count wouldn't be zero.
		MG_BOX_ASSERT(myToSubmitEvents.IsEmpty());
		// When a task is closed, its latest events aren't supposed to be processed and
		// hence the events won't be unlocked on the last wakeup. Need to do it manually
		// for the internal events if want to reuse the task for another socket. The
		// events would be still locked, because a closed task doesn't do ProcessArgs().
		myCancelEvent.Reset();
	}

	mg::net::Socket
	SocketCreate(
		mg::net::SockAddrFamily aAddrFamily,
		mg::net::TransportProtocol aProtocol,
		mg::box::Error::Ptr& aOutErr)
	{
		int flags = SOCK_NONBLOCK;
		switch(aProtocol)
		{
		case mg::net::TRANSPORT_PROT_DEFAULT:
		case mg::net::TRANSPORT_PROT_TCP:
			flags |= SOCK_STREAM;
			break;
		default:
			MG_BOX_ASSERT(!"Unknown protocol");
			break;
		}
		mg::net::Socket res = socket(aAddrFamily, flags, 0);
		if (res < 0)
		{
			aOutErr = mg::box::ErrorRaiseErrno("socket()");
			return mg::net::theInvalidSocket;
		}
		return res;
	}

	bool
	SocketListen(
		IOServerSocket* aSock,
		uint32_t aBacklog,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_BOX_ASSERT(aSock != nullptr && aSock->mySock != mg::net::theInvalidSocket &&
			"First bind, then listen");
		bool ok = listen(aSock->mySock, aBacklog) == 0;
		if (!ok)
			aOutErr = mg::box::ErrorRaiseErrno("listen()");
		return ok;
	}

}
}
