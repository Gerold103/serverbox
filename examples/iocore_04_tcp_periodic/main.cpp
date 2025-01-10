#include "mg/aio/IOCore.h"
#include "mg/aio/TCPServer.h"
#include "mg/aio/TCPSocket.h"
#include "mg/aio/TCPSocketSubscription.h"
#include "mg/box/Log.h"

#include <csignal>

//
// It is assumed you have seen the previous examples in this section, and some previously
// explained things don't need another repetition.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// An untrivial example how to use deadlines with IOCore. I.e. make a socket wakeup on a
// deadline or a delay if nothing is happening with it for a while.
//
// With this it becomes very simple and cheap to implement request timeouts. Normally it
// would look like a client has a bunch of requests in fly, waiting for their responses.
// Each request could have a different deadline (time_now + timeout). Then your code could
// store those requests in a binary heap (mg::box::BinaryHeap) sorted by the deadline.
//
// OnEvent() of your socket you would do socket->SetDeadline(heap.Top()->myDeadline) or
// something like that. Also in OnEvent() you check socket->IsExpired(). If yes, then you
// get the current timestamp, and pop from the heap all the requests whose deadline is
// less than the current time. They are expired, do anything with them.
//

static const int theClientCount = 5;
static const int theMessageCount = 6;

static bool
ReadData(
	mg::net::BufferReadStream& aStream,
	std::vector<char>& aDst)
{
	uint64_t newSize = aStream.GetReadSize();
	MG_BOX_ASSERT(newSize > 0);
	uint64_t oldSize = aDst.size();
	aDst.resize(oldSize + newSize);
	aStream.ReadData(aDst.data() + oldSize, newSize);
	// Terminating zero means that there is a complete message.
	return aDst.back() == 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// Client receives a fixed number of messages and deletes itself. The interesting part is
// that those requests are sent by the server once per second, not all at once.
//

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
		, myRecvCount(0)
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
		mySock->Recv(128);
	}

	// TCPSocketSubscription event.
	void
	OnRecv(
		mg::net::BufferReadStream& aStream) final
	{
		if (!ReadData(aStream, myInput))
			return;
		MG_LOG_INFO("Client.OnRecv", "%d: got '%s'", myID, myInput.data());
		if (++myRecvCount == theMessageCount)
		{
			mySock->PostClose();
			return;
		}
		myInput.clear();
		mySock->Recv(128);
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
		mySock->Delete();
		delete this;
	}

	const int myID;
	int myRecvCount;
	mg::aio::TCPSocket* mySock;
	std::vector<char> myInput;
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
		, mySendDeadline(0)
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
		// Receive isn't planned, but lets schedule it to catch a potential socket
		// closure. Socket close = read of zero bytes, so we need a pending read to notice
		// it. Can be of any > 0 size.
		mySock->Recv(1);
	}

	// TCPSocketSubscription event.
	void
	OnEvent() final
	{
		if (mySentCount >= theMessageCount)
		{
			// Everything is sent. Time to shutdown.
			mySock->PostClose();
			return;
		}
		if (mySendDeadline > mg::box::GetMilliseconds())
		{
			// Not expired yet. A spurious wakeup, or the deadline is set first time.
			// Anyway, lets wait until it expires.
			mySock->SetDeadline(mySendDeadline);
			return;
		}
		MG_LOG_INFO("Peer.OnEvent", "%d: sending next msg", myID);
		MG_BOX_ASSERT(mySendSize == 0);
		// Deadline is infinite so a next message is only sent after this one is fully
		// flushed. Although in such a simple example it is probably sent completely in
		// one go.
		mySendDeadline = MG_TIME_INFINITE;
		std::string msg = mg::box::StringFormat("message %d", ++mySentCount);
		mySendSize = (uint32_t)msg.size() + 1;
		// Copy the string, because it is on stack right now.
		mySock->SendCopy(msg.c_str(), mySendSize);
	}

	// TCPSocketSubscription event.
	void
	OnRecv(
		mg::net::BufferReadStream&) final
	{
		// The client isn't supposed to send anything.
		MG_BOX_ASSERT(!"Unreachable");
	}

	// TCPSocketSubscription event.
	void
	OnSend(
		uint32_t aByteCount) final
	{
		MG_BOX_ASSERT(aByteCount <= mySendSize);
		mySendSize -= aByteCount;
		if (mySendSize != 0)
			return;

		// Finally all is sent. Send the next one in 1 second.
		mySendDeadline = mg::box::GetMilliseconds() + 1000;
		// OnEvent() is going to be executed again ASAP, but this is ok. It is simpler
		// for us to implement all the logic in one function. It will handle this
		// reschedule a spurious wakeup.
		mySock->Reschedule();
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
	int mySentCount;
	uint32_t mySendSize;
	uint64_t mySendDeadline;
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
