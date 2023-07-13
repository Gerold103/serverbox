#include "IOTask.h"

#include "mg/asio/IOCore.h"

#include "mg/box/IOVec.h"

#include <sys/epoll.h>

namespace mg {
namespace asio {

	IOServerSocket::IOServerSocket()
		: mySock(mg::net::theInvalidSocket)
		, myPort(0)
	{
	}

	IOServerSocket::~IOServerSocket()
	{
		if (mySock != mg::net::theInvalidSocket)
			mg::net::SocketClose(mySock);
	}

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
		MG_DEV_ASSERT(myReadyEvents == 0);
		MG_DEV_ASSERT(myPendingEvents == 0);
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
		mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		IOEvent& aEvent)
	{
		MG_DEV_ASSERT(!aEvent.IsLocked());
		// sendmsg() is prefered over writev() because write*() calls are accounted like
		// disk operations in IO monitoring in the kernel.
		struct msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = mg::box::IOVecToNative(aBuffers);
		msg.msg_iovlen = aBufferCount;
		ssize_t rc = sendmsg(GetSocket(), &msg, 0);
		if (rc > 0)
		{
			// Write is done. But in edge-triggered epoll must write until would block. Or
			// save the event to finish its consumption later.
			SaveEventWritable();
			// Writes are not retried in a loop, because
			//
			// 1) usually it is not need (all is written first time), but complicates the
			//    code a lot;
			//
			// 2) it may lead to unfair cpu usage if some sockets would retry io
			//    operations. If they had more data to read/write, they could occupy the
			//    worker threads for too long.
			//
			// Instead, led the other tasks work. 'Cooperative multitasking'. The owner
			// must reschedule the task if wants more IO.
			return aEvent.ReturnBytes(rc);
		}
		MG_DEV_ASSERT(rc < 0);
		if (errno != EWOULDBLOCK && errno != EAGAIN)
			return aEvent.ReturnError(mg::box::ErrorCodeErrno());
		// Clear the event. When it will be unlocked, it should not contain any old data
		// until a next IO attempt happens.
		aEvent.ReturnEmpty();
		aEvent.Lock();
		MG_DEV_ASSERT(myOutEvent == nullptr);
		myOutEvent = &aEvent;
		return true;
	}

	bool
	IOTask::Recv(
		mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		IOEvent& aEvent)
	{
		MG_DEV_ASSERT(!aEvent.IsLocked());
		// recvmsg() is prefered over readv() because read*() calls are accounted like
		// disk operations in IO monitoring in the kernel.
		struct msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = mg::box::IOVecToNative(aBuffers);
		msg.msg_iovlen = aBufferCount;
		ssize_t rc = recvmsg(GetSocket(), &msg, 0);
		if (rc >= 0)
		{
			// Read is done. But in edge-triggered epoll must read until would block. Or
			// save the event to finish its consumption later.
			SaveEventReadable();
			// Reads are not retried in a loop, because
			//
			// 1) usually it is not need (all is read first time), but complicates the
			//    code a lot;
			//
			// 2) it may lead to unfair cpu usage if some sockets would retry io
			//    operations. If they had more data to read/write, they could occupy the
			//    worker threads for too long.
			//
			// Instead, led the other tasks work. 'Cooperative multitasking'. The owner
			// must reschedule the task if wants more IO.
			return aEvent.ReturnBytes(rc);
		}
		if (errno != EWOULDBLOCK && errno != EAGAIN)
			return aEvent.ReturnError(mg::box::ErrorCodeErrno());
		// Clear the event. When it will be unlocked, it should not contain any old data
		// until a next IO attempt happens.
		aEvent.ReturnEmpty();
		aEvent.Lock();
		MG_DEV_ASSERT(myInEvent == nullptr);
		myInEvent = &aEvent;
		return true;
	}

	bool
	IOTask::ConnectStart(
		mg::net::Socket aSocket,
		const mg::net::Host& aHost,
		IOEvent& aEvent)
	{
		MG_DEV_ASSERT(IsInWorkerNow());
		MG_DEV_ASSERT(!aEvent.IsLocked());
		MG_DEV_ASSERT(mySocket == mg::net::theInvalidSocket);

		bool ok = connect(aSocket, aHost.GetSockaddr(), aHost.GetSockaddrSize()) == 0;
		if (ok)
		{
			// Can legally finish immediately, even for a non-blocking socket.
			myCore->AttachSocket(aSocket, this);
			return aEvent.ReturnBytes(0);
		}
		if (errno != EINPROGRESS)
			return aEvent.ReturnError(mg::box::ErrorCodeErrno());
		myCore->AttachSocket(aSocket, this);
		aEvent.Lock();
		return true;
	}

	bool
	IOTask::ConnectUpdate(
		const mg::net::Host& aHost,
		IOEvent& aEvent)
	{
		MG_DEV_ASSERT(mySocket != mg::net::theInvalidSocket);
		MG_DEV_ASSERT(IsInWorkerNow());
		MG_DEV_ASSERT(aEvent.IsLocked());

		bool ok = connect(mySocket, aHost.GetSockaddr(), aHost.GetSockaddrSize()) == 0;
		if (ok)
			goto success;
		if (errno == EISCONN)
			goto success;
		if (errno != EALREADY)
			goto fail;
		// Still not ready. Wait for more events.
		return true;

	success:
		aEvent.Unlock();
		return aEvent.ReturnBytes(0);

	fail:
		aEvent.Unlock();
		return aEvent.ReturnError(mg::box::ErrorCodeErrno());
	}

	mg::net::Socket
	IOTask::Accept(
		IOEvent& aEvent,
		mg::net::Host& aOutPeer)
	{
		MG_DEV_ASSERT(IsInWorkerNow());
		// Check if still waiting for an epoll event.
		if (aEvent.IsLocked())
			return mg::net::theInvalidSocket;
		if (!aEvent.IsEmpty())
		{
			// Check if epoll returned an error on the current wakeup or an error wasn't
			// consumed by the user since a previous wakeup.
			if (aEvent.IsError())
				return mg::net::theInvalidSocket;
			// Otherwise epoll finally unlocked this event. Meaning there is a pending
			// connection probably.
			uint32_t bytes = aEvent.PopBytes();
			MG_DEV_ASSERT(bytes == 0);
		}
		MG_DEV_ASSERT(myInEvent == nullptr);
		sockaddr_storage remoteAddr;
		socklen_t remoteAddrLen = sizeof(remoteAddr);
		mg::net::Socket sock = accept4(mySocket, (sockaddr*)&remoteAddr, &remoteAddrLen,
			SOCK_NONBLOCK);
		if (sock >= 0)
		{
			aOutPeer.Set((mg::net::Sockaddr*)&remoteAddr);
			// Retry accepts as long as they are succeeding.
			Reschedule();
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
		const int nonCritErrors[] = {
			// Errors which might be forwarded from a connection which was closed after
			// being accepted in the kernel. See 'man accept' for a list of non-critical
			// errors.
			ENETDOWN, EPROTO, ENOPROTOOPT, EHOSTDOWN, ENONET, EHOSTUNREACH,
			EOPNOTSUPP, ENETUNREACH, ECONNABORTED,
			// Interruption by a signal is retryable.
			EINTR,
			// EMFILE and ENFILE are critical intentionally. They could be retried
			// periodically like IOTask::SetDeadline(now + 100ms), but there are many
			// other places in Massgate which crash on not being able to get a file
			// descriptor. If want this to be tolerable, then need to fix all of them.
		};
		bool isCritical = true;
		for (int nonCritErr : nonCritErrors)
		{
			if (err != nonCritErr)
				continue;
			isCritical = false;
			break;
		}
		// Non-critical means that the accept() should be retried. Might not return from
		// the edge-triggered epoll() otherwise. Hence explicit reschedule is done. Don't
		// loop right here - it would be unfair to other tasks.
		if (isCritical)
			aEvent.ReturnError(mg::box::ErrorCodeFromErrno(err));
		else
			Reschedule();
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
	IOTask::PrivCloseEndPlatform()
	{
		MG_DEV_ASSERT(myStatus.LoadRelaxed() == IOTASK_STATUS_CLOSED);
		// Task couldn't return to the scheduler and get new pending events.
		MG_DEV_ASSERT(myPendingEvents == 0);
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

	bool
	IOListen(
		IOServerSocket* aSock,
		uint32_t aBacklog,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(aSock != nullptr && aSock->mySock != mg::net::theInvalidSocket &&
			"First bind, then listen");
		bool ok = listen(aSock->mySock, aBacklog) == 0;
		if (!ok)
			aOutErr = mg::box::ErrorRaiseErrno("listen()");
		return ok;
	}

}
}
