#include "mg/aio/IOCore.h"
#include "mg/aio/TCPServer.h"
#include "mg/aio/TCPSocket.h"
#include "mg/aio/TCPSocketSubscription.h"
#include "mg/box/Log.h"

#include <csignal>

//
// Example of how to use TCPSocket and TCPServer. The clients connect to the server, send
// a message, wait for any response as a receipt confirmation, and delete themselves.
//

static const int theClientCount = 5;

//////////////////////////////////////////////////////////////////////////////////////////

class MyClient final
	// The socket subscription has a number of callbacks notifying us about various TCP
	// events like on-connect, on-recv, on-send, etc. All of them are optional, and are
	// always executed in a single thread, never in parallel. This allows us not to use
	// any mutexes for data used exclusively by those callbacks.
	: private mg::aio::TCPSocketSubscription
{
public:
	MyClient(
		int aID,
		mg::aio::IOCore& aCore,
		uint16_t aPort)
		: myID(aID)
		, mySock(new mg::aio::TCPSocket(aCore))
	{
		mySock->Open({});
		mg::aio::TCPSocketConnectParams connParams;
		connParams.myEndpoint = mg::net::HostMakeLocalIPV4(aPort).ToString();
		mySock->PostConnect(connParams, this);
	}

	~MyClient() final = default;

private:
	// TCPSocketSubscription event.
	void
	OnConnect() final
	{
		MG_LOG_INFO("Client.OnConnect", "%d", myID);
		// Using SendRef(), because the string is in the constant data section. It is
		// never deleted. Hence no need to copy it.
		const char* msg = "hello handshake";
		mySock->SendRef(msg, mg::box::Strlen(msg) + 1);
		mySock->Recv(1);
	}

	// TCPSocketSubscription event.
	void
	OnRecv(
		mg::net::BufferReadStream& aStream) final
	{
		MG_LOG_INFO("Client.OnRecv", "%d: got response", myID);
		MG_BOX_ASSERT(aStream.GetReadSize() > 0);
		mySock->PostClose();
	}

	// TCPSocketSubscription event.
	void
	OnError(
		mg::box::Error*) final
	{
		// Can happen on Windows even on graceful close.
		mySock->PostClose();
	}

	// TCPSocketSubscription event.
	void
	OnClose() final
	{
		MG_LOG_INFO("Client.OnClose", "%d", myID);
		// Try to always have all deletions in a single place. And the only suitable place
		// is on-close. This is the only place where you can safely assume that the socket
		// won't run again if you delete it right now.
		mySock->Delete();
		delete this;
	}

	const int myID;
	mg::aio::TCPSocket* mySock;
};

//////////////////////////////////////////////////////////////////////////////////////////

class MyPeer final
	: private mg::aio::TCPSocketSubscription
{
public:
	MyPeer(
		int aID,
		mg::aio::IOCore& aCore,
		mg::net::Socket aSock)
		: myID(aID)
		, mySock(new mg::aio::TCPSocket(aCore))
	{
		mySock->Open({});
		mySock->PostWrap(aSock, this);
	}

	~MyPeer() final = default;

private:
	// TCPSocketSubscription event.
	void
	OnConnect() final
	{
		MG_LOG_INFO("Peer.OnConnect", "%d", myID);
		mySock->Recv(128);
	}

	// TCPSocketSubscription event.
	void
	OnRecv(
		mg::net::BufferReadStream& aStream) final
	{
		uint64_t newSize = aStream.GetReadSize();
		MG_BOX_ASSERT(newSize > 0);
		uint64_t oldSize = myInput.size();
		myInput.resize(oldSize + newSize);
		aStream.ReadData(myInput.data() + oldSize, newSize);
		// Zero character (string terminator) is the end of the message. Until it is
		// received, the message is incomplete.
		if (myInput.back() != 0)
		{
			mySock->Recv(128);
			return;
		}
		MG_LOG_INFO("Peer.OnRecv", "%d: got '%s'", myID, myInput.data());
		mySock->PostClose();
	}

	// TCPSocketSubscription event.
	void
	OnError(
		mg::box::Error*) final
	{
		// Can happen on Windows even on graceful close.
		mySock->PostClose();
	}

	// TCPSocketSubscription event.
	void
	OnClose() final
	{
		MG_LOG_INFO("Peer.OnClose", "%d", myID);
		mySock->Delete();
		delete this;
	}

	const int myID;
	mg::aio::TCPSocket* mySock;
	std::vector<char> myInput;
};

//////////////////////////////////////////////////////////////////////////////////////////

class MyServer final
	// Identical to TCPSocketSubscription, but provides TCP-server events, like on-accept.
	: private mg::aio::TCPServerSubscription
{
public:
	MyServer(
		mg::aio::IOCore& aCore)
		: myAcceptCount(0)
		, myServer(mg::aio::TCPServer::NewShared(aCore))
	{
	}

	uint16_t
	Bind()
	{
		mg::box::Error::Ptr err;
		bool ok = myServer->Bind(mg::net::HostMakeAllIPV4(0), err);
		MG_BOX_ASSERT(ok);
		return myServer->GetPort();
	}

	void
	Start()
	{
		mg::box::Error::Ptr err;
		bool ok = myServer->Listen(mg::net::SocketMaxBacklog(), this, err);
		MG_BOX_ASSERT(ok);
	}

private:
	// TCPServerSubscription event.
	void
	OnAccept(
		mg::net::Socket aSock,
		const mg::net::Host&) final
	{
		MG_LOG_INFO("Server.OnAccept", "new client");
		new MyPeer(++myAcceptCount, myServer->GetCore(), aSock);
		if (myAcceptCount == theClientCount)
		{
			MG_LOG_INFO("Server.OnAccept", "start closing");
			myServer->PostClose();
		}
	}

	// TCPServerSubscription event.
	void
	OnClose() final
	{
		MG_LOG_INFO("Server.OnClose", "");
	}

	int myAcceptCount;
	mg::aio::TCPServer::Ptr myServer;
};

//////////////////////////////////////////////////////////////////////////////////////////

static void
RunExample()
{
	mg::aio::IOCore core;
	core.Start(3 /* threads */);

	MG_LOG_INFO("Main", "start server");
	MyServer server(core);
	uint16_t port = server.Bind();
	server.Start();

	MG_LOG_INFO("Main", "start clients");
	for (int i = 0; i < theClientCount; ++i)
		new MyClient(i + 1, core, port);

	MG_LOG_INFO("Main", "wait for end");
	uint32_t remaining = core.WaitEmpty();
	MG_BOX_ASSERT(remaining == 0);

	MG_LOG_INFO("Main", "terminating");
}

//////////////////////////////////////////////////////////////////////////////////////////

int
main()
{
#if IS_PLATFORM_WIN
	// WSAStartup() on Windows has to be done by the user. To be able to customize it as
	// they want. Besides, it is a too low-level thing to be able to wrap it into anything
	// platform-agnostic.
	WSADATA data;
	MG_BOX_ASSERT(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#else
	// SIGPIPE is pointless. This signal kills the process when trying to write to a
	// socket whose connection is dead. The problem is that there is no way to know that
	// at the moment of write.
	signal(SIGPIPE, SIG_IGN);
#endif

	RunExample();

#if IS_PLATFORM_WIN
	MG_BOX_ASSERT(WSACleanup() == 0);
#endif
	return 0;
}
