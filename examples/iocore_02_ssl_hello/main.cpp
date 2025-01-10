#include "mg/aio/IOCore.h"
#include "mg/aio/SSLSocket.h"
#include "mg/aio/TCPServer.h"
#include "mg/aio/TCPSocketSubscription.h"
#include "mg/box/Log.h"
#include "mg/net/SSLContext.h"

#include "Certs.h"

#include <csignal>

//
// It is assumed you have seen the previous examples in this section, and some previously
// explained things don't need another repetition.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// A simple example how to use SSL in Serverbox. Here a few SSL clients connect to an SSL
// server, send a simple message, gets its receipt confirmation, and close themselves.
//
// The server here has a properly signed certificate, and the clients validate it.
//

static const int theClientCount = 5;

//////////////////////////////////////////////////////////////////////////////////////////
//
// The client sends a handshake message, and closes itself when any response is received.
//
class MyClient final
	: private mg::aio::TCPSocketSubscription
{
public:
	MyClient(
		int aID,
		mg::aio::IOCore& aCore,
		uint16_t aPort,
		mg::net::SSLContext* aSSL)
		: myID(aID)
		, mySock(new mg::aio::SSLSocket(aCore))
	{
		mg::aio::SSLSocketParams sslParams;
		sslParams.mySSL = aSSL;
		mySock->Open(sslParams);

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
		const char* msg = "hello handshake";
		// Using SendRef(), because the string is in the constant data section. It is
		// never deleted. Hence no need to copy it.
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
	mg::aio::SSLSocket* mySock;
};

//////////////////////////////////////////////////////////////////////////////////////////

class MyPeer final
	: private mg::aio::TCPSocketSubscription
{
public:
	MyPeer(
		int aID,
		mg::aio::IOCore& aCore,
		mg::net::Socket aSock,
		mg::net::SSLContext* aSSL)
		: myID(aID)
		, mySock(new mg::aio::SSLSocket(aCore))
	{
		mg::aio::SSLSocketParams sslParams;
		sslParams.mySSL = aSSL;
		mySock->Open(sslParams);
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
	mg::aio::SSLSocket* mySock;
	std::vector<char> myInput;
};

//////////////////////////////////////////////////////////////////////////////////////////

class MyServer final
	: private mg::aio::TCPServerSubscription
{
public:
	MyServer(
		mg::aio::IOCore& aCore)
		: myAcceptCount(0)
		, myServer(mg::aio::TCPServer::NewShared(aCore))
		, mySSL(mg::net::SSLContext::NewShared(true /* is server */))
	{
		// This would normally be AddLocalCertFile(), because in most cases the SSL data
		// is stored on disk.
		bool ok = mySSL->AddLocalCert(
			theTestCert, theTestCertSize,
			theTestKey, theTestKeySize);
		MG_BOX_ASSERT(ok);
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
		new MyPeer(++myAcceptCount, myServer->GetCore(), aSock, mySSL.GetPointer());
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
	mg::net::SSLContext::Ptr mySSL;
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

	mg::net::SSLContext::Ptr clientSSL =
		mg::net::SSLContext::NewShared(false /* is server */);
	// Usually SSL cert is a file. Then you can use AddRemoteCertFile() if it is something
	// custom, or AddRemoteCertFromSystem() when need to talk to the open Internet and the
	// system's certificates are enough.
	clientSSL->AddRemoteCert(theTestCACert, theTestCACertSize);

	MG_LOG_INFO("Main", "start clients");
	for (int i = 0; i < theClientCount; ++i)
		new MyClient(i + 1, core, port, clientSSL.GetPointer());

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
