#pragma once

#include "mg/box/Error.h"
#include "mg/net/Host.h"

#if IS_PLATFORM_WIN
#include <winsock.h>
#endif

namespace mg {
namespace net {

#if IS_PLATFORM_WIN
	using Socket = SOCKET;
	static constexpr Socket theInvalidSocket = INVALID_SOCKET;
#else
	using Socket = int;
	static constexpr Socket theInvalidSocket = -1;
#endif

	static constexpr uint32_t theMaxBacklog = SOMAXCONN;

	enum TransportProtocol
	{
		TRANSPORT_PROT_DEFAULT,
		TRANSPORT_PROT_TCP,
	};

	bool SocketBind(
		Socket aSock,
		const Host& aHost,
		mg::box::Error::Ptr& aOutErr);

	bool SocketBindAny(
		Socket aSock,
		SockAddrFamily aAddrFamily,
		mg::box::Error::Ptr& aOutErr);

	Host SocketGetBoundHost(
		Socket aSock);

	bool SocketSetKeepAlive(
		Socket aSock,
		bool aValue,
		uint32_t aTimeout,
		mg::box::Error::Ptr& aOutErr);

	bool SocketFixReuseAddr(
		Socket aSock,
		mg::box::Error::Ptr& aOutErr);

	bool SocketSetDualStack(
		Socket aSock,
		bool aValue,
		mg::box::Error::Ptr& aOutErr);

	bool SocketSetNoDelay(
		Socket aSock,
		bool aValue,
		mg::box::Error::Ptr& aOutErr);

	bool SocketShutdown(
		Socket aSock,
		mg::box::Error::Ptr& aOutErr);

	void SocketClose(
		Socket aSock);

	bool SocketCheckState(
		Socket aSock,
		mg::box::Error::Ptr& aOutErr);

	bool SocketIsAcceptErrorCritical(
		mg::box::ErrorCode aError);

	void SocketMakeNonBlocking(
		mg::net::Socket aSock);

}
}
