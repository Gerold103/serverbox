#include "Socket.h"

#include "mg/box/IOVec.h"

#include <poll.h>

#if IS_PLATFORM_APPLE
#include <fcntl.h>
#endif

namespace mg {
namespace sio {

#if IS_PLATFORM_APPLE
	static void SocketMakeNonBlocking(
		mg::net::Socket aSock);
#endif

	mg::net::Socket
	SocketCreate(
		mg::net::SockAddrFamily aAddrFamily,
		mg::net::TransportProtocol aProtocol,
		mg::box::Error::Ptr& aOutErr)
	{
#if IS_PLATFORM_APPLE
		int flags = 0;
#else
		int flags = SOCK_NONBLOCK;
#endif
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
#if IS_PLATFORM_APPLE
		SocketMakeNonBlocking(res);
#endif
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
		mg::net::Socket sock;
#if IS_PLATFORM_APPLE
		sock = accept(aServer, (sockaddr*)&remoteAddr, &remoteAddrLen);
		if (sock >= 0)
			SocketMakeNonBlocking(sock);
#else
		sock = accept4(aServer, (sockaddr*)&remoteAddr, &remoteAddrLen,
			SOCK_NONBLOCK);
#endif
		if (sock >= 0)
		{
			aOutPeer.Set((mg::net::Sockaddr*)&remoteAddr);
			return sock;
		}
		int err = errno;
		if (err == EWOULDBLOCK || err == EAGAIN)
		{
			aOutErr.Clear();
			return mg::net::theInvalidSocket;
		}
		// Non-critical means that the accept() should be retried.
		mg::box::ErrorCode errCode = mg::box::ErrorCodeFromErrno(err);
		if (mg::net::SocketIsAcceptErrorCritical(errCode))
			aOutErr = mg::box::ErrorRaise(errCode, "accept4()");
		else
			aOutErr.Clear();
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
			aOutErr = mg::box::ErrorRaiseErrno("listen()");
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
		if (errno == EINPROGRESS)
			aOutErr.Clear();
		else
			aOutErr = mg::box::ErrorRaiseErrno("connect()");
		return false;
	}

	bool
	SocketConnectUpdate(
		mg::net::Socket aSock,
		mg::box::Error::Ptr& aOutErr)
	{
		pollfd fd;
		memset(&fd, 0, sizeof(fd));
		fd.fd = aSock;
		fd.events = POLLOUT;
		int rc = poll(&fd, 1, 0);
		if (rc == 0)
		{
			aOutErr.Clear();
			return false;
		}
		if (rc < 0)
		{
			aOutErr = mg::box::ErrorRaiseErrno("poll()");
			return false;
		}
		MG_DEV_ASSERT(rc == 1);
		if (fd.revents == POLLOUT)
			return true;
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
		// sendmsg() is prefered over writev() because write*() calls are accounted like
		// disk operations in IO monitoring in the kernel.
		msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = mg::box::IOVecToNative(aBuffers);
		msg.msg_iovlen = aBufferCount;
		ssize_t rc = sendmsg(aSock, &msg, 0);
		if (rc > 0)
			return rc;
		MG_DEV_ASSERT(rc < 0);
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			aOutErr.Clear();
		else
			aOutErr = mg::box::ErrorRaiseErrno("sendmsg()");
		return -1;
	}

	int64_t
	SocketRecv(
		mg::net::Socket aSock,
		mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		mg::box::Error::Ptr& aOutErr)
	{
		// recvmsg() is prefered over readv() because read*() calls are accounted like
		// disk operations in IO monitoring in the kernel.
		msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = mg::box::IOVecToNative(aBuffers);
		msg.msg_iovlen = aBufferCount;
		ssize_t rc = recvmsg(aSock, &msg, 0);
		if (rc >= 0)
			return rc;
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			aOutErr.Clear();
		else
			aOutErr = mg::box::ErrorRaiseErrno("recvmsg()");
		return -1;
	}

	//////////////////////////////////////////////////////////////////////////////////////

#if IS_PLATFORM_APPLE
	static void
	SocketMakeNonBlocking(
		mg::net::Socket aSock)
	{
		int flags = fcntl(aSock, F_GETFL);
		MG_BOX_ASSERT(flags >= 0);
		flags = fcntl(aSock, F_SETFL, flags | O_NONBLOCK);
		MG_BOX_ASSERT(flags >= 0);
	}
#endif

}
}
