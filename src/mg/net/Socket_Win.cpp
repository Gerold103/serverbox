#include "Socket.h"

#include <mstcpip.h>

namespace mg {
namespace net {

	uint32_t
	SocketMaxBacklog()
	{
		return SOMAXCONN;
	}

	bool
	SocketBind(
		Socket aSock,
		const Host& aHost,
		mg::box::Error::Ptr& aOutErr)
	{
		if (bind(aSock, &aHost.myAddr, aHost.GetSockaddrSize()) != 0)
		{
			aOutErr = mg::box::ErrorRaiseWSA("bind()");
			return false;
		}
		return true;
	}

	Host
	SocketGetBoundHost(
		Socket aSock)
	{
		sockaddr_storage addr;
		int len = sizeof(addr);
		int rc = getsockname(aSock, (sockaddr*)&addr, &len);
		MG_BOX_ASSERT_F(rc == 0, "getsockname() fail: %s",
			mg::box::ErrorRaiseWSA("getsockname()")->myMessage.c_str());
		return Host((sockaddr*)&addr);
	}

	bool
	SocketSetKeepAlive(
		Socket aSock,
		bool aValue,
		uint32_t aTimeout,
		mg::box::Error::Ptr& aOutErr)
	{
		tcp_keepalive settings;
		settings.onoff = aValue ? 1 : 0;
		settings.keepalivetime = aTimeout;
		settings.keepaliveinterval = 3000;

		DWORD unused = 0;
		if (WSAIoctl(aSock, SIO_KEEPALIVE_VALS, &settings, sizeof(settings), nullptr, 0,
			&unused, nullptr, nullptr) == SOCKET_ERROR)
		{
			aOutErr = mg::box::ErrorRaiseWSA("WSAIoctl(SIO_KEEPALIVE_VALS)");
			return false;
		}
		return true;
	}

	bool
	SocketFixReuseAddr(
		Socket aSock,
		mg::box::Error::Ptr& aOutErr)
	{
		int value = 1;
		bool ok = setsockopt(aSock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
			(const char*)&value, sizeof(value)) == 0;
		if (!ok)
			aOutErr = mg::box::ErrorRaiseWSA("setsockopt(SO_EXCLUSIVEADDRUSE)");
		return ok;
	}

	bool
	SocketSetDualStack(
		Socket aSock,
		bool aValue,
		mg::box::Error::Ptr& aOutErr)
	{
		int value = aValue ? 0 : 1;
		bool ok = setsockopt(aSock, IPPROTO_IPV6, IPV6_V6ONLY,
			(const char*)&value, sizeof(value)) == 0;
		if (!ok)
			aOutErr = mg::box::ErrorRaiseWSA("setsockopt(IPV6_V6ONLY)");
		return ok;
	}

	bool
	SocketSetNoDelay(
		Socket aSock,
		bool aValue,
		mg::box::Error::Ptr& aOutErr)
	{
		int value = aValue ? 0 : 1;
		bool ok = setsockopt(aSock, IPPROTO_TCP, TCP_NODELAY,
			(const char*)&value, sizeof(value)) == 0;
		if (!ok)
			aOutErr = mg::box::ErrorRaiseWSA("setsockopt(TCP_NODELAY)");
		return ok;
	}

	bool
	SocketShutdown(
		Socket aSock,
		mg::box::Error::Ptr& aOutErr)
	{
		if (shutdown(aSock, SD_BOTH) == SOCKET_ERROR)
		{
			aOutErr = mg::box::ErrorRaiseWSA("shutdown()");
			return false;
		}
		return true;
	}

	void
	SocketClose(
		Socket aSock)
	{
		bool ok = aSock == theInvalidSocket || closesocket(aSock) == 0;
		if (ok)
			return;
		// If code tries to close a seemingly valid socket, but it fails as 'not a
		// socket', it can mean only 2 conclusions (almost always):
		// - The socket wasn't properly initialized;
		// - The socket was already closed.
		// In both cases it is dangerous as the socket descriptor could already be reused,
		// and there is a risk of closing a foreign socket which makes the whole
		// application corrupted and behave unpredictably. Must not continue the
		// execution.
		MG_BOX_ASSERT_F(false,
			"Couldn't close a socket, err: %s",
			mg::box::ErrorRaiseWSA()->myMessage.c_str());
	}

	bool
	SocketCheckState(
		Socket aSock,
		mg::box::Error::Ptr& aOutErr)
	{
		int sockerr;
		int len = sizeof(sockerr);
		int rc = getsockopt(aSock, SOL_SOCKET, SO_ERROR, (char*)&sockerr, &len);
		MG_BOX_ASSERT_F(rc == 0, "getsockopt() fail: %s",
			mg::box::ErrorRaiseWSA("getsockopt()")->myMessage.c_str());
		if (sockerr == 0)
			return true;
		aOutErr = mg::box::ErrorRaiseWSA(sockerr, "socket state");
		return false;
	}

	bool
	SocketIsAcceptErrorCritical(
		mg::box::ErrorCode aError)
	{
		MG_DEV_ASSERT(aError != mg::box::ERR_BOX_NONE);
		return aError != mg::box::ERR_NET_CLOSE_BY_PEER;
	}

	void
	SocketMakeNonBlocking(
		mg::net::Socket aSock)
	{
		unsigned long value = 1;
		int rc = ioctlsocket(aSock, FIONBIO, &value);
		MG_BOX_ASSERT_F(rc == 0, "ioctlsocket() fail: %s",
			mg::box::ErrorRaiseWSA("ioctlsocket()")->myMessage.c_str());
	}

}
}
