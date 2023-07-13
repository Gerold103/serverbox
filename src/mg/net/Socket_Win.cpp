#include "Socket.h"

namespace mg {
namespace net {

	bool
	SocketBind(
		Socket aSock,
		const Host& aHost,
		mg::box::Error::Ptr& aOutErr)
	{
		if (bind(aSock, aHost.GetSockaddr(), aHost.GetSockaddrSize()) != 0)
		{
			aOutErr = mg::box::ErrorRaiseWSA("bind()");
			return false;
		}
		return true;
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
	SocketSetFixReuseAddr(
		Socket aSock,
		bool aValue,
		mg::box::Error::Ptr& aOutErr)
	{
		int bvalue = 1;
		bool ok = setsockopt(aSock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
			&bvalue, sizeof(bvalue)) == 0;
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
		int optValue = aValue ? 0 : 1;
		bool ok = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY,
			&optValue, sizeof(optValue)) == 0;
		if (!ok)
			aOutErr = mg::box::ErrorRaiseWSA("setsockopt(IPV6_V6ONLY)");
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
		bool ok = aSocket == INVALID_SOCKET_VALUE || closesocket(aSocket) == 0;
		if (ok)
			return;
		mg::box::Error::Ptr err = mg::box::ErrorRaiseWSA();
		// If code tries to close a seemingly valid socket, but it fails as 'not a
		// socket', it can mean only 2 conclusions (almost always):
		// - The socket wasn't properly initialized;
		// - The socket was already closed.
		// In both cases it is dangerous as the socket descriptor could already be reused,
		// and there is a risk of closing a foreign socket which makes the whole
		// application corrupted and behave unpredictably. Must not continue the
		// execution.
		MG_BOX_ASSERT_F(false,
			"Couldn't close a socket, err: %s", err->myMessage.c_str());
	}

}
}
