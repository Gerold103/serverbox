#include "Socket.h"

#include "mg/box/Log.h"
#include "mg/box/Sysinfo.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <unistd.h>

#if IS_PLATFORM_APPLE
#include <sys/sysctl.h>
#endif

namespace mg {
namespace net {

	uint32_t
	SocketMaxBacklog()
	{
#if !IS_PLATFORM_APPLE
		return SOMAXCONN;
#else
		int somaxconn = 0;
		size_t size = sizeof(somaxconn);
		const char *name = "kern.ipc.somaxconn";
		int rc = sysctlbyname(name, &somaxconn, &size, NULL, 0);
		if (rc == 0) {
			// From tests it appears that values > INT16_MAX work strangely. For example,
			// 32768 behaves worse than 32767. Like if nothing was changed. The suspicion
			// is that listen() on Apple internally uses int16_t or 'short' for storing
			// the queue size and it simply gets reset to default on bigger values.
			if (somaxconn > INT16_MAX)
			{
				MG_LOG_WARN("socket_max_backlog.01",
					"%s is too high (%d), truncated to %d",
					name, somaxconn, (int)INT16_MAX);
				somaxconn = INT16_MAX;
			}
			return somaxconn;
		}
		MG_LOG_ERROR("socket_max_backlog.02", "couldn't get system's %s setting", name);
		return SOMAXCONN;
#endif
	}

	bool
	SocketBind(
		Socket aSock,
		const Host& aHost,
		mg::box::Error::Ptr& aOutErr)
	{
		if (bind(aSock, &aHost.myAddr, aHost.GetSockaddrSize()) != 0)
		{
			aOutErr = mg::box::ErrorRaiseErrno("bind()");
			return false;
		}
		return true;
	}

	Host
	SocketGetBoundHost(
		Socket aSock)
	{
		sockaddr_storage addr;
		socklen_t len = sizeof(addr);
		int rc = getsockname(aSock, (sockaddr*)&addr, &len);
		MG_BOX_ASSERT_F(rc == 0, "getsockname() fail: %d %s", errno, strerror(errno));
		return Host((sockaddr*)&addr);
	}

	bool
	SocketSetKeepAlive(
		Socket aSock,
		bool aValue,
		uint32_t aTimeout,
		mg::box::Error::Ptr& aOutErr)
	{
		int optValue = aValue ? 1 : 0;
		if (setsockopt(aSock, SOL_SOCKET, SO_KEEPALIVE, &optValue, sizeof(optValue)) != 0)
		{
			aOutErr = mg::box::ErrorRaiseErrno("setsockopt(SO_KEEPALIVE)");
			return false;
		}
		if (!aValue)
			return true;

		int keepAliveIdle = aTimeout / 1000;
		if (keepAliveIdle == 0 && aTimeout != 0)
			keepAliveIdle = 1;
#if IS_PLATFORM_APPLE
		if (setsockopt(aSock, IPPROTO_TCP, TCP_KEEPALIVE, &keepAliveIdle,
			sizeof(keepAliveIdle)) != 0)
		{
			aOutErr = mg::box::ErrorRaiseErrno("setsockopt(TCP_KEEPALIVE)");
			return false;
		}
#else
		// TCP_KEEPIDLE - time the connection should be idle to start sending keepalive
		//   packets.
		//
		// TCP_KEEPINTVL - time between keepalive packets.
		//
		// TCP_KEEPCNT - number of keepalive probes to fail to declare the connection
		//   dead.
		//
		// On Linux the keepalive timeouts are in seconds and not ms as in Windows.
		int keepAliveInterval = 3;
		// This keep-alive-count is a default from the kernel. Nonetheless it is
		// configured explicitly so as to have a predictable user-timeout below.
		int keepAliveCount = 9;
		// The problem with keepalive is that it starts working only on an idle but
		// seemingly healthy connection. Idle means absolutely no data. Empty send
		// buffers. If there is non-confirmed data to send/retry, it will be retried for
		// long time regardless of keepalive values. Moreover, buffer means in-kernel
		// buffer. Not something controllable by userspace. From userspace point of view
		// all send/sendto/write/... could be finished successfully.
		//
		// There is a separate timeout to decide when pending not ACKed data should break
		// the connection. When it is not enabled, a connection may live up to 30 minutes
		// with default TCP retry settings. See value of 'net.ipv4.tcp_retries2' for
		// details. The current value can be checked by the command in a terminal:
		//
		//     $> sudo sysctl net.ipv4 | grep retries2
		//
		// The option is Linux specific, has nothing to do with TCP protocol itself.
		//
		// The timeout here is configured to be maximum of how long keepalive can hold the
		// connection. Therefore 'fixing' keepalive for non-empty sockets.
		unsigned userTimeout = keepAliveIdle + keepAliveInterval * keepAliveCount;
		// User timeout is measured in milliseconds.
		userTimeout *= 1000;

		if (setsockopt(aSock, IPPROTO_TCP, TCP_KEEPINTVL, &keepAliveInterval,
			sizeof(keepAliveInterval)) != 0)
		{
			aOutErr = mg::box::ErrorRaiseErrno("setsockopt(TCP_KEEPINTVL)");
			return false;
		}
		if (setsockopt(aSock, IPPROTO_TCP, TCP_KEEPIDLE, &keepAliveIdle,
			sizeof(keepAliveIdle)) != 0)
		{
			aOutErr = mg::box::ErrorRaiseErrno("setsockopt(TCP_KEEPIDLE)");
			return false;
		}
		if (setsockopt(aSock, IPPROTO_TCP, TCP_KEEPCNT, &keepAliveCount,
			sizeof(keepAliveCount)) != 0)
		{
			aOutErr = mg::box::ErrorRaiseErrno("setsockopt(TCP_KEEPCNT)");
			return false;
		}
		if (setsockopt(aSock, IPPROTO_TCP, TCP_USER_TIMEOUT, &userTimeout,
			sizeof(userTimeout)) != 0)
		{
			aOutErr = mg::box::ErrorRaiseErrno("setsockopt(TCP_USER_TIMEOUT)");
			return false;
		}
#endif
		return true;
	}

	bool
	SocketFixReuseAddr(
		Socket aSock,
		mg::box::Error::Ptr& aOutErr)
	{
		// On WSL1 the address reuse option behaves like on Windows. Meaning that it
		// allows to have multiple alive servers on the same port. Can't be used then.
		if (mg::box::SysIsWSL())
			return true;
		int optValue = 1;
		bool ok = setsockopt(aSock, SOL_SOCKET, SO_REUSEADDR,
			&optValue, sizeof(optValue)) == 0;
		if (!ok)
			aOutErr = mg::box::ErrorRaiseErrno("setsockopt(SO_REUSEADDR)");
		return ok;
	}

	bool
	SocketSetDualStack(
		Socket aSock,
		bool aValue,
		mg::box::Error::Ptr& aOutErr)
	{
		int optValue = aValue ? 0 : 1;
		bool ok = setsockopt(aSock, IPPROTO_IPV6, IPV6_V6ONLY,
			&optValue, sizeof(optValue)) == 0;
		if (!ok)
			aOutErr = mg::box::ErrorRaiseErrno("setsockopt(IPV6_V6ONLY)");
		return ok;
	}

	bool
	SocketSetNoDelay(
		Socket aSock,
		bool aValue,
		mg::box::Error::Ptr& aOutErr)
	{
		int optValue = aValue ? 0 : 1;
		bool ok = setsockopt(aSock, IPPROTO_TCP, TCP_NODELAY,
			&optValue, sizeof(optValue)) == 0;
		if (!ok)
			aOutErr = mg::box::ErrorRaiseErrno("setsockopt(TCP_NODELAY)");
		return ok;
	}

	bool
	SocketShutdown(
		Socket aSock,
		mg::box::Error::Ptr& aOutErr)
	{
		bool ok = shutdown(aSock, SHUT_RDWR) == 0;
		if (!ok)
			aOutErr = mg::box::ErrorRaiseErrno("shutdown()");
		return ok;
	}

	void
	SocketClose(
		Socket aSock)
	{
		bool ok = aSock == theInvalidSocket || close(aSock) == 0;
		if (ok)
			return;
		int err = errno;
		// If code tries to close a seemingly valid socket, but it fails as 'not a
		// socket', it can mean only 2 conclusions (almost always):
		// - The socket wasn't properly initialized;
		// - The socket was already closed.
		// In both cases it is dangerous as the socket descriptor could already be reused,
		// and there is a risk of closing a foreign socket which makes the whole
		// application corrupted and behave unpredictably. Must not continue the
		// execution.
		MG_BOX_ASSERT_F(false,
			"Couldn't close a socket, err: %d, %s", err, strerror(err));
	}

	bool
	SocketCheckState(
		Socket aSock,
		mg::box::Error::Ptr& aOutErr)
	{
		int sockerr;
		socklen_t len = sizeof(sockerr);
		int rc = getsockopt(aSock, SOL_SOCKET, SO_ERROR, (void*)&sockerr, &len);
		int err = errno;
		MG_BOX_ASSERT_F(rc == 0, "getsockopt() failed: %d %s", err, strerror(err));
		if (sockerr == 0)
			return true;
		aOutErr = mg::box::ErrorRaiseErrno(sockerr, "socket state");
		return false;
	}

	bool
	SocketIsAcceptErrorCritical(
		mg::box::ErrorCode aError)
	{
		MG_DEV_ASSERT(aError != mg::box::ERR_BOX_NONE);
		// These should have been filtered out before.
		MG_DEV_ASSERT(aError != mg::box::ERR_SYS_WOULDBLOCK);
		// Apparently, Apple's accept() doesn't forward errors from the backlog. If any
		// occurs, it is related to the server itself.
		const mg::box::ErrorCode nonCritErrors[] = {
			// Errors which might be forwarded from a connection which was closed after
			// being accepted in the kernel. See 'man accept' for a list of non-critical
			// errors.
#if IS_PLATFORM_LINUX
			// = ENETDOWN, EPROTO, ENOPROTOOPT, EHOSTDOWN, ENONET, EHOSTUNREACH,
			// = EOPNOTSUPP, ENETUNREACH, ECONNABORTED,
			mg::box::ERR_NET_DOWN, mg::box::ERR_NET_PROTOCOL, mg::box::ERR_NET_ABORTED,
			mg::box::ERR_NET_HOST_UNREACHABLE, mg::box::ERR_SYS_NOT_SUPPORTED,
#elif IS_PLATFORM_APPLE
			// = ECONNABORTED,
			mg::box::ERR_NET_ABORTED,
#endif
			// Interruption by a signal is retryable.
			// = EINTR,
			mg::box::ERR_SYS_INTERRUPTED,
			// EMFILE and ENFILE are critical intentionally. They could be retried
			// periodically like IOTask::SetDeadline(now + 100ms), but there are many
			// other places in Massgate which crash on not being able to get a file
			// descriptor. If want this to be tolerable, then need to fix all of them.
		};
		for (mg::box::ErrorCode nonCritErr : nonCritErrors)
		{
			if (aError != nonCritErr)
				continue;
			return false;
		}
		return true;
	}

	void
	SocketMakeNonBlocking(
		mg::net::Socket aSock)
	{
		int flags = fcntl(aSock, F_GETFL);
		MG_BOX_ASSERT(flags >= 0);
		flags = fcntl(aSock, F_SETFL, flags | O_NONBLOCK);
		MG_BOX_ASSERT(flags >= 0);
	}

}
}
