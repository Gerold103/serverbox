#include "IOTask.h"

#include "mg/box/IOVec.h"

namespace mg {
namespace aio {

	IOServerSocket::IOServerSocket()
		: myPeerSock(mg::net::theInvalidSocket)
		, mySock(mg::net::theInvalidSocket)
		, myAddFamily(mg::net::ADDR_FAMILY_NONE)
		, myAcceptExF(nullptr)
		, myGetAcceptExSockaddrsF(nullptr)
	{
	}

	IOServerSocket::~IOServerSocket()
	{
		if (myPeerSock != mg::net::theInvalidSocket)
			mg::net::SocketClose(myPeerSock);
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
		const mg::box::IOVec* aBuffers,
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
		mg::net::Socket aSocket,
		const mg::net::Host& aHost,
		IOEvent& aEvent)
	{
		MG_DEV_ASSERT(IsInWorkerNow());
		MG_DEV_ASSERT(!aEvent.IsLocked());
		MG_DEV_ASSERT(mySocket == mg::net::theInvalidSocket);

		mg::box::Error::Ptr err;
		if (!mg::net::SocketBindAny(aSocket, aHost.myAddr.sa_family, err))
			return aEvent.ReturnError(err->myCode);

		// Without attachment the overlapped functions won't work. Have to attach now even
		// if errors will happen later. Then they would have to be handled asynchronously.
		AttachSocket(aSocket);

		LPFN_CONNECTEX ConnectEx;
		GUID GuiConnectEx = WSAID_CONNECTEX;
		DWORD unused;
		BOOL ok = WSAIoctl(aSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuiConnectEx,
			sizeof(GuiConnectEx), &ConnectEx, sizeof(ConnectEx), &unused, nullptr,
			nullptr) == 0;
		MG_BOX_ASSERT(ok);

		if (!ConnectEx(aSocket, &aHost.myAddr,  aHost.GetSockaddrSize(), nullptr, 0,
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

	mg::net::Socket
	IOTask::Accept(
		IOEvent& aEvent,
		mg::net::Host& aOutPeer)
	{
		MG_DEV_ASSERT(IsInWorkerNow());
		MG_DEV_ASSERT(myServerSock != nullptr);
		// The socket is moved to task->mySocket when posted into IOCore.
		MG_DEV_ASSERT(myServerSock->mySock == mg::net::theInvalidSocket);
		// Lock = spurious wakeup. Keep waiting then.
		if (aEvent.IsLocked())
			return mg::net::theInvalidSocket;
		if (aEvent.IsEmpty())
		{
			// No accept in progress right now. Start one.
			MG_DEV_ASSERT(myServerSock->myPeerSock == mg::net::theInvalidSocket);
			mg::box::Error::Ptr err;
			myServerSock->myPeerSock = SocketCreate(myServerSock->myAddFamily,
				mg::net::TRANSPORT_PROT_TCP, err);

			DWORD unused = 0;
			bool isStarted = myServerSock->myAcceptExF(mySocket, myServerSock->myPeerSock,
				myServerSock->myBuffer, 0, theAcceptAddrMinSize, theAcceptAddrMinSize,
				&unused, &aEvent.myOverlap);
			if (!isStarted)
			{
				uint32_t errCode = WSAGetLastError();
				if (errCode == ERROR_IO_PENDING)
					isStarted = true;
				else
					aEvent.ReturnError(mg::box::ErrorCodeFromWSA(errCode));
			}
			if (isStarted)
			{
				// Even if the operation ended immediately, it still will be returned
				// through iocp queues. It means it can't be considered completed.
				OperationStart();
				aEvent.Lock();
				return mg::net::theInvalidSocket;
			}
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
			if (myServerSock->myPeerSock != mg::net::theInvalidSocket)
				mg::net::SocketClose(myServerSock->myPeerSock);
			myServerSock->myPeerSock = mg::net::theInvalidSocket;
			return mg::net::theInvalidSocket;
		}
		// A previously started accept has finally ended successfully.
		MG_BOX_ASSERT(aEvent.PopBytes() == 0);
		MG_DEV_ASSERT(myServerSock->myPeerSock != mg::net::theInvalidSocket);
		sockaddr* localAddr = nullptr;
		sockaddr* remoteAddr = nullptr;
		int localLen;
		int remoteLen;
		myServerSock->myGetAcceptExSockaddrsF(myServerSock->myBuffer, 0,
			theAcceptAddrMinSize, theAcceptAddrMinSize, &localAddr, &localLen,
			&remoteAddr, &remoteLen);
		mg::net::Socket sock = myServerSock->myPeerSock;
		myServerSock->myPeerSock = mg::net::theInvalidSocket;
		// Windows requires to make this "update context" call or the socket won't be
		// functional.
		bool ok = setsockopt(sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
			(char*)&mySocket, sizeof(mySocket)) == 0;
		if (!ok)
		{
			aEvent.ReturnError(mg::box::ErrorCodeWSA());
			mg::net::SocketClose(sock);
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
	IOTask::PrivCloseEnd()
	{
		MG_DEV_ASSERT(myStatus.LoadRelaxed() == IOTASK_STATUS_CLOSED);
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

	mg::net::Socket
	SocketCreate(
		mg::net::SockAddrFamily aAddrFamily,
		mg::net::TransportProtocol aProtocol,
		mg::box::Error::Ptr& aOutErr)
	{
		int flags = 0;
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
		mg::net::Socket res = WSASocketW(aAddrFamily, flags, 0, nullptr, 0,
			WSA_FLAG_OVERLAPPED);
		if (res == mg::net::theInvalidSocket)
		{
			aOutErr = mg::box::ErrorRaiseWSA("WSASocketW()");
			return res;
		}
		return res;
	}

	bool
	SocketListen(
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
