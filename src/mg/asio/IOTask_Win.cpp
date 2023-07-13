#include "IOTask.h"

namespace mg {
namespace asio {

	IOServerSocket::IOServerSocket()
		: myPeerSock(mg::net::theInvalidSocket)
		, mySock(mg::net::theInvalidSocket)
		, myPort(0)
		, myAcceptExF(nullptr)
		, myGetAcceptExSockaddrsF(nullptr)
	{
	}

	IOServerSocket::~IOServerSocket()
	{
		if (myPeerSock != mg::net::theInvalidSocket)
			SocketClose(myPeerSock);
		if (mySock != mg::net::theInvalidSocket)
			SocketClose(mySock);
	}

	void
	IOTask::PrivConstructPlatform()
	{
		myReadyEventCount = 0;
		myPendingEventCount = 0;
		myOperationCount = 0;
		myServerSock = nullptr;
	}

	void
	IOTask::PrivDestructPlatform()
	{
		PrivTouch();
		MG_DEV_ASSERT(myReadyEvents.IsEmpty());
		MG_DEV_ASSERT(myReadyEventCount == 0);
		MG_DEV_ASSERT(myPendingEvents.IsEmpty());
		MG_DEV_ASSERT(myPendingEventCount == 0);
		MG_DEV_ASSERT(myOperationCount == 0);
		MG_DEV_ASSERT(myServerSock == nullptr);
	}

	void
	IOTask::OperationStart()
	{
		PrivTouch();
		++myOperationCount;
	}

	void
	IOTask::OperationAbort()
	{
		PrivTouch();
		int count = --myOperationCount;
		MG_DEV_ASSERT(count >= 0);
	}

	bool
	IOTask::Send(
		mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		IOEvent& aEvent)
	{
		MG_DEV_ASSERT(!aEvent.IsLocked());
		DWORD bytesSent;
		int rc = WSASend(GetSocket(), mg::box::IOVecToNative(aBuffers), aBufferCount,
			&bytesSent, 0, &aEvent.myOverlap, nullptr);
		if (rc != 0)
		{
			int err = WSAGetLastError();
			if (err != WSA_IO_PENDING)
			{
				aEvent.ReturnError(mg::box::ErrorCodeFromWSA(err));
				return false;
			}
		}
		// Even if the operation ended immediately, it still will be returned through IOCP
		// queues. It means it can't be considered completed.
		OperationStart();
		aEvent.Lock();
		return true;
	}

	bool
	IOTask::Recv(
		mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		IOEvent& aEvent)
	{
		MG_DEV_ASSERT(!aEvent.IsLocked());
		DWORD bytesRecvd;
		DWORD flags = 0;
		int rc = WSARecv(GetSocket(), mg::box::IOVecToNative(aBuffers), aBufferCount,
			&bytesRecvd, &flags, &aEvent.myOverlap, nullptr);
		if (rc != 0)
		{
			int err = WSAGetLastError();
			if (err != WSA_IO_PENDING)
			{
				aEvent.ReturnError(mg::box::ErrorCodeFromWSA(err));
				return false;
			}
		}
		// Even if the operation ended immediately, it still will be returned through IOCP
		// queues. It means it can't be considered completed.
		OperationStart();
		aEvent.Lock();
		return true;
	}

	bool
	IOTask::ConnectStart(
		Socket aSocket,
		const mg::net::Host& aHost,
		IOEvent& aEvent)
	{
		MG_DEV_ASSERT(IsInWorkerNow());
		MG_DEV_ASSERT(!aEvent.IsLocked());
		MG_DEV_ASSERT(mySocket == mg::net::theInvalidSocket);

		mg::box::Error::Ptr err;
		if (!mg::net::SocketBindAny(aSocket, aHost.GetAddressFamily(), err))
			return aEvent.ReturnError(err->myCode);

		myCore->AttachSocket(aSocket, this);

		LPFN_CONNECTEX ConnectEx;
		GUID GuiConnectEx = WSAID_CONNECTEX;
		DWORD unused;
		BOOL ok = WSAIoctl(aSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuiConnectEx,
			sizeof(GuiConnectEx), &ConnectEx, sizeof(ConnectEx), &unused, nullptr,
			nullptr) == 0;
		MG_DEV_ASSERT(ok);

		if (!ConnectEx(aSocket, aHost.GetSockaddr(),  aHost.GetSockaddrSize(), nullptr, 0,
			&unused, &aEvent.myOverlap))
		{
			uint32_t errCode = WSAGetLastError();
			if (errCode != WSA_IO_PENDING)
				return aEvent.ReturnError(mg::box::ErrorCodeFromWSA(errCode));
		}
		// Even if the operation ended immediately, it still will be returned through IOCP
		// queues. It means it can't be considered completed.
		aEvent.Lock();
		OperationStart();
		return true;
	}

	bool
	IOTask::ConnectUpdate(
		const mg::net::Host&,
		IOEvent& aEvent)
	{
		MG_DEV_ASSERT(IsInWorkerNow());
		MG_DEV_ASSERT(mySocket != mg::net::theInvalidSocket);
		if (aEvent.IsLocked())
			return true;
		if (aEvent.IsError())
			return false;

		bool ok = setsockopt(mySocket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT,
			nullptr, 0) == 0;
		if (!ok)
			return aEvent.ReturnError(mg::box::ErrorCodeWSA());
		return true;
	}

	Socket
	IOTask::Accept(
		IOEvent& aEvent,
		mg::net::Host& aOutPeer)
	{
		MG_DEV_ASSERT(IsInWorkerNow());
		MG_DEV_ASSERT(myServerSock != nullptr);
		// The socket is moved to task->mySocket when posted into IoCore.
		MG_DEV_ASSERT(myServerSock->mySock == mg::net::theInvalidSocket);
		// Lock = spurious wakeup.Keep waiting then.
		if (aEvent.IsLocked())
			return mg::net::theInvalidSocket;
		if (aEvent.IsEmpty())
		{
			// No accept in progress right now. Start one.
			MG_DEV_ASSERT(myServerSock->myPeerSock == mg::net::theInvalidSocket);
			mg::box::Error::Ptr err;
			myServerSock->myPeerSock = mg::net::SocketCreate(mg::net::ADDR_FAMILY_IPV6,
				mg::net::TRANSPORT_PROT_TCP, err);

			DWORD unused = 0;
			if (!myServerSock->myAcceptExF(mySocket, myServerSock->myPeerSock,
				myServerSock->myBuffer, 0, theAcceptAddrMinSize, theAcceptAddrMinSize,
				&unused, &aEvent.myOverlap))
			{
				uint32_t err = WSAGetLastError();
				if (err != ERROR_IO_PENDING)
				{
					// The to-accept connection could be closed before being accepted.
					// Retry then (reschedule, don't loop - it would be unfair).
					if (err == WSAECONNRESET)
						Reschedule();
					else
						aEvent.ReturnError(mg::box::ErrorCodeFromWSA(err));
					// The socket has to be deleted. Apparently it is spoiled if
					// WSAECONNRESET happens.
					SocketClose(myServerSock->myPeerSock);
					myServerSock->myPeerSock = mg::net::theInvalidSocket;
					return mg::net::theInvalidSocket;
				}
			}
			// Even if the operation ended immediately, it still will be returned through
			// iocp queues. It means it can't be considered completed.
			OperationStart();
			aEvent.Lock();
			return mg::net::theInvalidSocket;
		}
		if (aEvent.IsError())
		{
			if (myServerSock->myPeerSock != mg::net::theInvalidSocket)
				SocketClose(myServerSock->myPeerSock);
			myServerSock->myPeerSock = mg::net::theInvalidSocket;
			return mg::net::theInvalidSocket;
		}
		// A previously started accept has finally ended successfully.
		MG_DEV_ASSERT(aEvent.PopBytes() == 0);
		MG_DEV_ASSERT(myServerSock->myPeerSock != mg::net::theInvalidSocket);
		sockaddr* localAddr = nullptr;
		sockaddr* remoteAddr = nullptr;
		int localLen;
		int remoteLen;
		myServerSock->myGetAcceptExSockaddrsF(myServerSock->myBuffer, 0,
			theAcceptAddrMinSize, theAcceptAddrMinSize, &localAddr, &localLen,
			&remoteAddr, &remoteLen);
		Socket sock = myServerSock->myPeerSock;
		myServerSock->myPeerSock = mg::net::theInvalidSocket;
		// Windows requires to make this "update context" call or the socket won't be
		// functional.
		bool ok = setsockopt(sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
			(char*)&mySocket, sizeof(mySocket)) == 0;
		if (!ok)
		{
			aEvent.ReturnError(mg::box::ErrorCodeWSA());
			SocketClose(sock);
			return mg::net::theInvalidSocket;
		}
		aOutPeer.Set(remoteAddr);
		// Retry accepts as long as they are succeeding.
		Reschedule();
		return sock;
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
	IOTask::PrivCloseEndPlatform()
	{
		MG_DEV_ASSERT(myStatus == IO_CORE_TASK_STATUS_CLOSED);
		// Task couldn't return to the scheduler and get new pending events.
		MG_DEV_ASSERT(myPendingEventCount == 0);
		MG_DEV_ASSERT(myPendingEvents.IsEmpty());
		// Ready events are already dumped into IOArgs.
		MG_DEV_ASSERT(myReadyEventCount == 0);
		MG_DEV_ASSERT(myReadyEvents.IsEmpty());
		// IOCP can report task as closed only after all its operations are complete.
		MG_DEV_ASSERT(myOperationCount == 0);
		delete myServerSock;
		myServerSock = nullptr;
	}

	bool
	IOListen(
		IOServerSocket* aSock,
		uint32_t aBacklog,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(aSock != nullptr && aSock->mySock != mg::net::theInvalidSocket &&
			"First bind, then listen");
		if (listen(aSock->mySock, aBacklog) != 0)
		{
			aOutErr = mg::box::ErrorRaiseWSA("listen()");
			return false;
		}
		GUID guid = WSAID_ACCEPTEX;
		DWORD dvalue = 0;
		if (WSAIoctl(aSock->mySock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid,
			sizeof(guid), &aSock->myAcceptExF, sizeof(aSock->myAcceptExF), &dvalue,
			nullptr, nullptr) != 0)
		{
			aOutErr = mg::box::ErrorRaiseWSA("WSAIoctl(get WSAID_ACCEPTEX)");
			return false;
		}
		guid = WSAID_GETACCEPTEXSOCKADDRS;
		if (WSAIoctl(aSock->mySock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid,
			sizeof(guid), &aSock->myGetAcceptExSockaddrsF,
			sizeof(aSock->myGetAcceptExSockaddrsF), &dvalue, nullptr, nullptr) != 0)
		{
			aOutErr = mg::box::ErrorRaiseWSA("WSAIoctl(get WSAID_GETACCEPTEXSOCKADDRS)");
			return false;
		}
		return true;
	}

}
}