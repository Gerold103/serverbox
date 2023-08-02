#include "Socket.h"

#include "mg/box/IOVec.h"

namespace mg {
namespace sio {

	mg::net::Socket
	SocketCreate(
		mg::net::SockAddrFamily aAddrFamily,
		mg::net::TransportProtocol aProtocol,
		mg::box::Error::Ptr& aOutErr)
	{
		int type = -1;
		switch (aProtocol)
		{
		case mg::net::TRANSPORT_PROT_DEFAULT:
		case mg::net::TRANSPORT_PROT_TCP:
			type = SOCK_STREAM;
			break;
		default:
			MG_BOX_ASSERT(!"Unknown protocol");
			break;
		}
		mg::net::Socket res = WSASocketW(aAddrFamily, type, 0, nullptr, 0, 0);
		if (res == mg::net::theInvalidSocket)
			aOutErr = mg::box::ErrorRaiseWSA("WSASocketW()");
		else
			mg::net::SocketMakeNonBlocking(res);
		return res;
	}

	mg::net::Socket
	SocketAccept(
		mg::net::Socket aServer,
		mg::net::Host& aOutPeer,
		mg::box::Error::Ptr& aOutErr)
	{
		sockaddr_storage remoteAddr;
		socklen_t remoteAddrLen = sizeof(remoteAddr);
		mg::net::Socket sock = accept(aServer, (sockaddr*)&remoteAddr, &remoteAddrLen);
		if (sock != mg::net::theInvalidSocket)
		{
			mg::net::SocketMakeNonBlocking(sock);
			aOutPeer.Set((mg::net::Sockaddr*)&remoteAddr);
			return sock;
		}
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
		{
			aOutErr.Clear();
			return mg::net::theInvalidSocket;
		}
		aOutErr = mg::box::ErrorRaiseWSA(err, "accept()");
		return mg::net::theInvalidSocket;
	}

	bool
	SocketListen(
		mg::net::Socket aSock,
		uint32_t aBacklog,
		mg::box::Error::Ptr& aOutErr)
	{
		bool ok = listen(aSock, aBacklog) == 0;
		if (!ok)
			aOutErr = mg::box::ErrorRaiseWSA("listen()");
		return ok;
	}

	bool
	SocketConnectStart(
		mg::net::Socket aSock,
		const mg::net::Host& aHost,
		mg::box::Error::Ptr& aOutErr)
	{
		bool ok = connect(aSock, &aHost.myAddr, aHost.GetSockaddrSize()) == 0;
		if (ok)
			return true;
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			aOutErr.Clear();
		else
			aOutErr = mg::box::ErrorRaiseWSA(err, "connect()");
		return false;
	}

	bool
	SocketConnectUpdate(
		mg::net::Socket aSock,
		mg::box::Error::Ptr& aOutErr)
	{
		fd_set writeSet;
		FD_ZERO(&writeSet);
		FD_SET(aSock, &writeSet);

		fd_set errSet;
		FD_ZERO(&errSet);
		FD_SET(aSock, &errSet);

		timeval timeout;
		memset(&timeout, 0, sizeof(timeout));
		int rc = select(0, nullptr, &writeSet, &errSet, &timeout);
		if (rc == 0)
		{
			aOutErr.Clear();
			return false;
		}
		if (rc < 0)
		{
			aOutErr = mg::box::ErrorRaiseWSA("select()");
			return false;
		}
		MG_DEV_ASSERT(rc == 1 || rc == 2);
		if (FD_ISSET(aSock, &writeSet))
			return true;
		MG_BOX_ASSERT(FD_ISSET(aSock, &errSet));
		if (!mg::net::SocketCheckState(aSock, aOutErr))
			aOutErr = mg::box::ErrorRaise(mg::box::ERR_NET_ABORTED, "async connect");
		return false;
	}

	int64_t
	SocketSend(
		mg::net::Socket aSock,
		const mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		mg::box::Error::Ptr& aOutErr)
	{
		DWORD size = 0;
		int rc = WSASend(aSock, mg::box::IOVecToNative(aBuffers), aBufferCount, &size, 0, nullptr, nullptr);
		if (rc == 0)
			return size;
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			aOutErr.Clear();
		else
			aOutErr = mg::box::ErrorRaiseWSA(err, "WSASend()");
		return -1;
	}

	int64_t
	SocketRecv(
		mg::net::Socket aSock,
		mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		mg::box::Error::Ptr& aOutErr)
	{
		DWORD size = 0;
		DWORD flags = 0;
		int rc = WSARecv(aSock, mg::box::IOVecToNative(aBuffers), aBufferCount, &size, &flags, nullptr, nullptr);
		if (rc == 0)
			return size;
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			aOutErr.Clear();
		else
			aOutErr = mg::box::ErrorRaiseWSA(err, "WSASend()");
		return -1;
	}

}
}
