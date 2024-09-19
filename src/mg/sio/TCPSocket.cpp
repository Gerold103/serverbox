#include "TCPSocket.h"

#include "mg/sio/Socket.h"

namespace mg {
namespace sio {

	TCPSocket::TCPSocket()
		: myState(TCP_SOCKET_STATE_CLOSED)
		, mySocket(mg::net::theInvalidSocket)
		, mySendOffset(0)
	{
	}

	TCPSocket::~TCPSocket()
	{
		Close();
	}

	bool
	TCPSocket::Connect(
		const mg::net::Host& aHost,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CLOSED);
		MG_DEV_ASSERT(mySocket == mg::net::theInvalidSocket);
		mg::net::Socket sock = SocketCreate(aHost.myAddr.sa_family,
			mg::net::TRANSPORT_PROT_TCP, aOutErr);
		if (sock == mg::net::theInvalidSocket)
			return false;

		if (!SocketConnectStart(sock, aHost, aOutErr))
		{
			if (aOutErr.IsSet())
			{
				mg::net::SocketClose(sock);
				return false;
			}
			// Async connect is started.
			mySocket = sock;
			myState = TCP_SOCKET_STATE_CONNECTING;
			return true;
		}
		mySocket = sock;
		myState = TCP_SOCKET_STATE_CONNECTED;
		return true;
	}

	void
	TCPSocket::Wrap(
		mg::net::Socket aSock)
	{
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CLOSED);
		MG_DEV_ASSERT(mySocket == mg::net::theInvalidSocket);
		mySocket = aSock;
		myState = TCP_SOCKET_STATE_CONNECTED;
	}

	bool
	TCPSocket::Update(
		mg::box::Error::Ptr& aOutErr)
	{
		// Fast-path.
		if (myState == TCP_SOCKET_STATE_CONNECTED)
			return PrivUpdateSend(aOutErr);

		// Slow-path.
		if (myState == TCP_SOCKET_STATE_CLOSED)
			return true;
		MG_BOX_ASSERT(myState == TCP_SOCKET_STATE_CONNECTING);
		return PrivUpdateConnect(aOutErr);
	}

	void
	TCPSocket::Close()
	{
		if (myState == TCP_SOCKET_STATE_CLOSED)
			return;
		myState = TCP_SOCKET_STATE_CLOSED;
		mg::net::SocketClose(mySocket);
		mySocket = mg::net::theInvalidSocket;
		myOutput.Clear();
		mySendOffset = 0;
	}

	bool
	TCPSocket::IsConnecting() const
	{
		return myState == TCP_SOCKET_STATE_CONNECTING;
	}

	bool
	TCPSocket::IsConnected() const
	{
		return myState == TCP_SOCKET_STATE_CONNECTED;
	}

	bool
	TCPSocket::IsClosed() const
	{
		return myState == TCP_SOCKET_STATE_CLOSED;
	}

	void
	TCPSocket::SendRef(
		const mg::net::Buffer* aHead)
	{
		if (myState != TCP_SOCKET_STATE_CLOSED)
			myOutput.AppendRef(aHead);
	}

	void
	TCPSocket::SendRef(
		mg::net::Buffer::Ptr&& aHead)
	{
		if (myState != TCP_SOCKET_STATE_CLOSED)
			myOutput.AppendRef(std::move(aHead));
	}

	void
	TCPSocket::SendRef(
		const void* aData,
		uint64_t aSize)
	{
		if (myState != TCP_SOCKET_STATE_CLOSED)
			myOutput.AppendRef(aData, aSize);
	}

	void
	TCPSocket::SendCopy(
		const void* aData,
		uint64_t aSize)
	{
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CONNECTED);
		if (myState != TCP_SOCKET_STATE_CLOSED)
			myOutput.AppendCopy(aData, aSize);
	}

	int64_t
	TCPSocket::Recv(
		mg::net::Buffer* aHead,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CONNECTED);
		int64_t rc = SocketRecv(mySocket, aHead, aOutErr);
		if ((rc < 0 && aOutErr.IsSet()) || rc == 0)
			Close();
		return rc;
	}

	int64_t
	TCPSocket::Recv(
		void* aData,
		uint64_t aSize,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CONNECTED);
		int64_t rc = SocketRecv(mySocket, aData, aSize, aOutErr);
		if ((rc < 0 && aOutErr.IsSet()) || rc == 0)
			Close();
		return rc;
	}

	bool
	TCPSocket::PrivUpdateSend(
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CONNECTED);
		if (myOutput.IsEmpty())
			return true;
		int64_t rc = SocketSend(mySocket, myOutput, mySendOffset, aOutErr);
		if (rc < 0)
		{
			if (!aOutErr.IsSet())
				return true;
			Close();
			return false;
		}
		myOutput.SkipData(mySendOffset, rc);
		return true;
	}

	bool
	TCPSocket::PrivUpdateConnect(
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CONNECTING);
		if (!SocketConnectUpdate(mySocket, aOutErr))
		{
			if (!aOutErr.IsSet())
				return true;
			Close();
			return false;
		}
		myState = TCP_SOCKET_STATE_CONNECTED;
		return true;
	}

}
}
