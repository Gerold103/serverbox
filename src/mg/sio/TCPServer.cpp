#include "TCPServer.h"

#include "mg/sio/Socket.h"

namespace mg {
namespace sio {

	TCPServer::TCPServer()
		: myState(TCP_SERVER_STATE_CLOSED)
		, mySocket(mg::net::theInvalidSocket)
	{
	}

	TCPServer::~TCPServer()
	{
		Close();
	}

	bool
	TCPServer::Bind(
		const mg::net::Host& aHost,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myState == TCP_SERVER_STATE_CLOSED);
		mg::net::Socket sock = SocketCreate(aHost.myAddr.sa_family,
			mg::net::TRANSPORT_PROT_TCP, aOutErr);
		if (sock == mg::net::theInvalidSocket)
			return false;
		if (!mg::net::SocketFixReuseAddr(sock, aOutErr) ||
			!mg::net::SocketBind(sock, aHost, aOutErr))
		{
			mg::net::SocketClose(sock);
			return false;
		}
		myState = TCP_SERVER_STATE_BOUND;
		mySocket = sock;
		return true;
	}

	uint16_t
	TCPServer::GetPort() const
	{
		MG_DEV_ASSERT(myState == TCP_SERVER_STATE_BOUND ||
			myState == TCP_SERVER_STATE_LISTENING);
		mg::net::Host host = mg::net::SocketGetBoundHost(mySocket);
		return host.GetPort();
	}

	bool
	TCPServer::Listen(
		uint32_t aBacklog,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myState == TCP_SERVER_STATE_BOUND);
		if (!SocketListen(mySocket, aBacklog, aOutErr))
		{
			Close();
			return false;
		}
		myState = TCP_SERVER_STATE_LISTENING;
		return true;
	}

	mg::net::Socket
	TCPServer::Accept(
		mg::net::Host& aOutPeer,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myState == TCP_SERVER_STATE_LISTENING);
		mg::net::Socket res = SocketAccept(mySocket, aOutPeer, aOutErr);
		if (res == mg::net::theInvalidSocket && aOutErr.IsSet())
			Close();
		return res;
	}

	void
	TCPServer::Close()
	{
		if (myState == TCP_SERVER_STATE_CLOSED)
			return;
		myState = TCP_SERVER_STATE_CLOSED;
		mg::net::SocketClose(mySocket);
		mySocket = mg::net::theInvalidSocket;
	}

}
}
