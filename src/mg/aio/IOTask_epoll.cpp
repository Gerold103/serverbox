#include "IOTask.h"

#include "mg/box/IOVec.h"

#include <sys/epoll.h>

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
		myReadyEvents = 0;
		myPendingEvents = 0;
		myInEvent = nullptr;
		myOutEvent = nullptr;
	}

	void
	IOTask::PrivDestructPlatform()
	{
		MG_BOX_ASSERT(myReadyEvents == 0);
		MG_BOX_ASSERT(myPendingEvents == 0);
	}

	void
	IOTask::SaveEventWritable()
	{
		PrivTouch();
		myReadyEvents |= EPOLLOUT;
	}

	void
	IOTask::SaveEventReadable()
	{
		PrivTouch();
		myReadyEvents |= EPOLLIN;
	}

	bool
	IOTask::Send(
		const mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		IOEvent& aEvent)
	{
		MG_BOX_ASSERT(!aEvent.IsLocked());
		// sendmsg() is preferred over writev() because write*() calls are accounted like
		// disk operations in IO monitoring in the kernel.
		msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = mg::box::IOVecToNative(aBuffers);
		msg.msg_iovlen = aBufferCount;
		ssize_t rc = sendmsg(GetSocket(), &msg, 0);
		if (rc > 0)
		{
			// Write is done. But in edge-triggered (EPOLLET) epoll must write until would
			// block. Or save the event to finish its consumption later.
			SaveEventWritable();
			// Writes are not retried in a loop, because
			//
			// 1) usually it is not need (all is written first time), but complicates the
			//    code notably;
			//
			// 2) it may lead to unfair CPU usage if some sockets would retry io
			//    operations. If they had more data to read/write, they could occupy the
			//    worker threads for too long.
			//
			// Instead, let the other tasks work. 'Cooperative multitasking'. The owner
			// must reschedule the task if wants more IO.
			return aEvent.ReturnBytes(rc);
		}
		MG_BOX_ASSERT(rc < 0);
		if (errno != EWOULDBLOCK && errno != EAGAIN)
			return aEvent.ReturnError(mg::box::ErrorCodeErrno());
		// Clear the event. When it will be unlocked, it should not contain any old data
		// until a next IO attempt happens.
		aEvent.ReturnEmpty();
		aEvent.Lock();
		MG_BOX_ASSERT(myOutEvent == nullptr);
		myOutEvent = &aEvent;
		return true;
	}

	bool
	IOTask::Recv(
		mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		IOEvent& aEvent)
	{
		MG_BOX_ASSERT(!aEvent.IsLocked());
		// recvmsg() is preferred over readv() because read*() calls are accounted like
		// disk operations in IO monitoring in the kernel.
		msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = mg::box::IOVecToNative(aBuffers);
		msg.msg_iovlen = aBufferCount;
		ssize_t rc = recvmsg(GetSocket(), &msg, 0);
		if (rc >= 0)
		{
			// Read is done. But in edge-triggered (EPOLLET) epoll must read until would
			// block. Or save the event to finish its consumption later.
			SaveEventReadable();
			// Reads are not retried in a loop, because
			//
			// 1) usually it is not need (all is read first time), but complicates the
			//    code notably;
			//
			// 2) it may lead to unfair CPU usage if some sockets would retry io
			//    operations. If they had more data to read/write, they could occupy the
			//    worker threads for too long.
			//
			// Instead, let the other tasks work. 'Cooperative multitasking'. The owner
			// must reschedule the task if wants more IO.
			return aEvent.ReturnBytes(rc);
		}
		if (errno != EWOULDBLOCK && errno != EAGAIN)
			return aEvent.ReturnError(mg::box::ErrorCodeErrno());
		// Clear the event. When it will be unlocked, it should not contain any old data
		// until a next IO attempt happens.
		aEvent.ReturnEmpty();
		aEvent.Lock();
		MG_BOX_ASSERT(myInEvent == nullptr);
		myInEvent = &aEvent;
		return true;
	}

	bool
	IOTask::ConnectStart(
		mg::net::Socket aSocket,
		const mg::net::Host& aHost,
		IOEvent& aEvent)
	{
		MG_BOX_ASSERT(IsInWorkerNow());
		MG_BOX_ASSERT(!aEvent.IsLocked());
		MG_BOX_ASSERT(mySocket == mg::net::theInvalidSocket);

		bool ok = connect(aSocket, &aHost.myAddr, aHost.GetSockaddrSize()) == 0;
		if (ok)
		{
			// Can legally finish immediately, even for a non-blocking socket.
			AttachSocket(aSocket);
			return aEvent.ReturnBytes(0);
		}
		if (errno != EINPROGRESS)
			return aEvent.ReturnError(mg::box::ErrorCodeErrno());
		AttachSocket(aSocket);
		aEvent.Lock();
		// Connect finish is signaled by the socket becoming writable. Hence wait for
		// EPOLLOUT.
		MG_BOX_ASSERT(myOutEvent == nullptr);
		myOutEvent = &aEvent;
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
		MG_BOX_ASSERT(aEvent.IsEmpty());
		return aEvent.ReturnBytes(0);
	}

	mg::net::Socket
	IOTask::Accept(
		IOEvent& aEvent,
		mg::net::Host& aOutPeer)
	{
		MG_BOX_ASSERT(IsInWorkerNow());
		// Check if still waiting for an epoll event.
		if (aEvent.IsLocked())
			return mg::net::theInvalidSocket;
		// Accept never returns 'bytes', and errors are supposed to be consumed by the
		// user before trying accept() again.
		MG_BOX_ASSERT(aEvent.IsEmpty());
		MG_BOX_ASSERT(myInEvent == nullptr);
		sockaddr_storage remoteAddr;
		socklen_t remoteAddrLen = sizeof(remoteAddr);
		mg::net::Socket sock = accept4(mySocket, (sockaddr*)&remoteAddr, &remoteAddrLen,
			SOCK_NONBLOCK);
		if (sock >= 0)
		{
			aOutPeer.Set((mg::net::Sockaddr*)&remoteAddr);
			return sock;
		}
		int err = errno;
		if (err == EWOULDBLOCK || err == EAGAIN)
		{
			aEvent.Lock();
			// EPOLLIN is sent when a client is incoming. Hence use input-event.
			myInEvent = &aEvent;
			return mg::net::theInvalidSocket;
		}
		// Non-critical means that the accept() should be retried. Might not return from
		// the edge-triggered epoll() otherwise. Don't loop right here - it would be
		// unfair to other tasks.
		mg::box::ErrorCode errCode = mg::box::ErrorCodeFromErrno(err);
		if (mg::net::SocketIsAcceptErrorCritical(errCode))
			aEvent.ReturnError(errCode);
		else
			aEvent.ReturnEmpty();
		return mg::net::theInvalidSocket;
	}

	bool
	IOTask::ProcessArgs(
		const IOArgs& aArgs,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(!IsClosed());
		MG_DEV_ASSERT(IsInWorkerNow());

		if ((aArgs.myEvents & (EPOLLERR | EPOLLHUP)) != 0)
		{
			aOutErr.Clear();
			if (mySocket != mg::net::theInvalidSocket)
				mg::net::SocketCheckState(mySocket, aOutErr);
			if (!aOutErr.IsSet())
				aOutErr = mg::box::ErrorRaise(mg::box::ERR_NET_ABORTED, "EPOLLERR/HUP");
			return false;
		}
		if ((aArgs.myEvents & EPOLLOUT) != 0 && myOutEvent != nullptr)
		{
			myOutEvent->UnlockForced();
			myOutEvent = nullptr;
		}
		if ((aArgs.myEvents & EPOLLIN) != 0 && myInEvent != nullptr)
		{
			myInEvent->UnlockForced();
			myInEvent = nullptr;
		}
		return true;
	}

	void
	IOTask::PrivDumpReadyEvents(
		IOArgs& aOutArgs)
	{
		PrivTouch();
		// Ready events can be accessed by a worker thread and by the scheduler. But only
		// by one thread at a time. Since the task is here (in the worker thread), it
		// can't be in the scheduler. Safe to read and update non-atomically.
		aOutArgs.myEvents = myReadyEvents;
		myReadyEvents = 0;
	}

	void
	IOTask::PrivCloseEnd()
	{
		MG_BOX_ASSERT(myStatus.LoadRelaxed() == IOTASK_STATUS_CLOSED);
		// Task couldn't return to the scheduler and get new pending events.
		MG_BOX_ASSERT(myPendingEvents == 0);
		// Delete whatever the owner tried to 'save'.
		myReadyEvents = 0;
		if (myInEvent != nullptr)
		{
			myInEvent->ReturnError(mg::box::ERR_NET_ABORTED);
			myInEvent->Unlock();
			myInEvent = nullptr;
		}
		if (myOutEvent != nullptr)
		{
			myOutEvent->ReturnError(mg::box::ERR_NET_ABORTED);
			myOutEvent->Unlock();
			myOutEvent = nullptr;
		}
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
