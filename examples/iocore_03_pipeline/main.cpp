#include "mg/aio/IOCore.h"
#include "mg/aio/TCPServer.h"
#include "mg/aio/TCPSocket.h"
#include "mg/aio/TCPSocketSubscription.h"
#include "mg/box/Log.h"
#include "mg/sch/TaskScheduler.h"

#include <csignal>

//
// It is assumed you have seen the previous examples in this section, and some previously
// explained things don't need another repetition.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// A very real example with a task as the main business logic request, and a networking
// client to do sub-requests of that task.
//
// In this example the business logic is math. Request comes to the app and wants some
// arith operations done. But the main request processor can't do that. Instead, it sends
// those math operations as sub-requests to a "remote backend" (our CalcServer in this
// case).
//
// A more realistic example: imagine a server processing save games of an online game. The
// server is stateless, doesn't store any games itself. Instead, for any request like
// "create a save game", "drop it", "update it" the app downloads the save game from a
// database, does the work, and uploads the result. It would involve 2 sub-requests to the
// database for one main request like "update a save game".
//

//////////////////////////////////////////////////////////////////////////////////////////
//
// CalcClient asynchronously talks to a "remote" CalcServer in request-response manner.
// Each request on completion fires its completion-callback, where we can wake a task up,
// send it a signal, or do anything else.
//
struct CalcRequest
{
	char myOp;
	int64_t myArg1;
	int64_t myArg2;
	std::function<void(int64_t)> myOnComplete;
	// Intrusive list link.
	CalcRequest* myNext;
};

class CalcClient final
	: private mg::aio::TCPSocketSubscription
{
public:
	CalcClient(
		mg::aio::IOCore& aCore,
		uint16_t aPort)
		: mySock(new mg::aio::TCPSocket(aCore))
	{
		mySock->Open({});
		mg::aio::TCPSocketConnectParams connParams;
		connParams.myEndpoint = mg::net::HostMakeLocalIPV4(aPort).ToString();
		MG_LOG_INFO("Client", "start connecting");
		mySock->PostConnect(connParams, this);
	}

	void
	Delete()
	{
		// Don't just 'delete this'. The socket might be being handled in a worker thread
		// right now. Better close the socket first. When this is done, we can safely
		// delete this client in OnClose().
		mySock->PostClose();
	}

	void
	Submit(
		char aOp,
		int64_t aArg1,
		int64_t aArg2,
		std::function<void(int64_t)>&& aOnComplete)
	{
		MG_LOG_INFO("Client.Submit", "new request");
		CalcRequest* req = new CalcRequest();
		req->myOp = aOp;
		req->myArg1 = aArg1;
		req->myArg2 = aArg2;
		req->myOnComplete = std::move(aOnComplete);
		// Wakeup the socket only if the request is first in the queue. If it is not, the
		// socket was already woken up anyway, and is going to be executed soon.
		// Could also wakeup always, but it is just not needed.
		if (myFrontQueue.Push(req))
			mySock->PostWakeup();
	}

private:
	~CalcClient() final
	{
		// Doesn't matter in which order to delete the requests. Hence can use the
		// non-ordered fast pop.
		CalcRequest* head = myFrontQueue.PopAllFastReversed();
		while (head != nullptr)
		{
			CalcRequest* next = head->myNext;
			delete head;
			head = next;
		}
		while (!myQueue.IsEmpty())
			delete myQueue.PopFirst();
	}

	// TCPSocketSubscription event.
	void
	OnConnect() final
	{
		MG_LOG_INFO("Client.OnConnect", "");
		mySock->Recv(128);
	}

	// TCPSocketSubscription event.
	void
	OnEvent() final
	{
		CalcRequest* tail;
		CalcRequest* head = myFrontQueue.PopAll(tail);
		if (head == nullptr)
			return;

		// Save the requests for later response handling.
		myQueue.Append(head, tail);
		// Send requests in bulk.
		mg::net::BufferStream data;
		while (head != nullptr)
		{
			MG_LOG_INFO("Client.OnEvent", "got request");
			// It is a bad idea to send integers like that, because the receiver might
			// have a different byte order. In general, apps must encode numbers in some
			// platform-agnostic way. Like always use Big Endian, or use a protocol like
			// MesssagePack. Here it is omitted for simplicity.
			data.WriteCopy(&head->myOp, sizeof(head->myOp));
			data.WriteCopy(&head->myArg1, sizeof(head->myArg1));
			data.WriteCopy(&head->myArg2, sizeof(head->myArg2));
			head = head->myNext;
		}
		mySock->SendRef(data.PopData());
	}

	// TCPSocketSubscription event.
	void
	OnRecv(
		mg::net::BufferReadStream& aStream) final
	{
		// Responses in this case come in the same order as requests.
		int64_t res = 0;
		while (aStream.GetReadSize() >= sizeof(res))
		{
			aStream.ReadData(&res, sizeof(res));
			CalcRequest* pos = myQueue.PopFirst();
			MG_BOX_ASSERT(pos != nullptr);
			pos->myOnComplete(res);
			delete pos;
		}
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
		MG_LOG_INFO("Client.OnClose", "");
		// Try to always have all deletions in a single place. And the only suitable place
		// is on-close. This is the only place where you can safely assume that the socket
		// won't run again if you delete it right now.
		mySock->Delete();
		delete this;
	}

	mg::aio::TCPSocket* mySock;
	// Queue of sent requests waiting for their responses.
	mg::box::ForwardList<CalcRequest> myQueue;
	// Queue if requests submitted by other threads. Not yet sent to the network.
	mg::box::MultiProducerQueueIntrusive<CalcRequest> myFrontQueue;
};

//////////////////////////////////////////////////////////////////////////////////////////
//
// CalcServer and its peers. Can be running remotely. Serves the math requests.
//
class CalcPeer final
	: private mg::aio::TCPSocketSubscription
{
public:
	CalcPeer(
		mg::aio::IOCore& aCore,
		mg::net::Socket aSock)
		: mySock(new mg::aio::TCPSocket(aCore))
	{
		mySock->Open({});
		mySock->PostWrap(aSock, this);
	}

	~CalcPeer() final = default;

private:
	// TCPSocketSubscription event.
	void
	OnConnect() final
	{
		MG_LOG_INFO("Peer.OnConnect", "");
		mySock->Recv(128);
	}

	// TCPSocketSubscription event.
	void
	OnRecv(
		mg::net::BufferReadStream& aStream) final
	{
		char op;
		int64_t arg1, arg2;
		constexpr uint32_t msgSize = sizeof(op) + sizeof(arg1) + sizeof(arg2);
		mg::net::BufferStream data;
		while (aStream.GetReadSize() >= msgSize)
		{
			aStream.ReadData(&op, sizeof(op));
			aStream.ReadData(&arg1, sizeof(arg1));
			aStream.ReadData(&arg2, sizeof(arg2));
			int64_t res;
			switch(op)
			{
			case '+': res = arg1 + arg2; break;
			case '-': res = arg1 - arg2; break;
			default:
				MG_BOX_ASSERT(!"Unreachable");
				res = 0;
				break;
			}
			data.WriteCopy(&res, sizeof(res));
		}
		mySock->SendRef(data.PopData());
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
		MG_LOG_INFO("Peer.OnClose", "");
		mySock->Delete();
		delete this;
	}

	mg::aio::TCPSocket* mySock;
};

//////////////////////////////////////////////////////////////////////////////////////////

class CalcServer final
	: private mg::aio::TCPServerSubscription
{
public:
	CalcServer(
		mg::aio::IOCore& aCore)
		: myServer(mg::aio::TCPServer::NewShared(aCore))
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

	void
	Delete()
	{
		myServer->PostClose();
	}

private:
	~CalcServer() = default;

	// TCPServerSubscription event.
	void
	OnAccept(
		mg::net::Socket aSock,
		const mg::net::Host&) final
	{
		MG_LOG_INFO("Server.OnAccept", "new client");
		new CalcPeer(myServer->GetCore(), aSock);
	}

	// TCPServerSubscription event.
	void
	OnClose() final
	{
		MG_LOG_INFO("Server.OnClose", "");
		delete this;
	}

	mg::aio::TCPServer::Ptr myServer;
};

//////////////////////////////////////////////////////////////////////////////////////////
//
// The business logic request, which is doing sub-requests to the remote CalcServer.
//
class MyRequest
{
public:
	MyRequest(
		int aID,
		CalcClient& aCalcClient,
		mg::sch::TaskScheduler& aSched)
		: myID(aID)
		, myCalcClient(aCalcClient)
		, myTask(Execute(this))
	{
		aSched.Post(&myTask);
	}

private:
	mg::box::Coro
	Execute(
		MyRequest* aSelf)
	{
		MG_LOG_INFO("MyRequest.Execute", "%d: start", myID);

		// Send a first math request.
		MG_LOG_INFO("MyRequest.Execute", "%d: send sub-request 1", myID);
		int64_t res = 0;
		myCalcClient.Submit('+', 10, myID, [aSelf, &res](int64_t aRes) {
			res = aRes;
			// The completion callback sends a signal to the MyRequest's task which also
			// wakes it up.
			aSelf->myTask.PostSignal();
		});
		// Wait for the signal in a yielding loop, to handle potential spurious wakeups.
		while (!co_await aSelf->myTask.AsyncReceiveSignal());
		// 
		MG_LOG_INFO("MyRequest.Execute", "%d: got response %lld", myID, (long long)res);

		// Do it all again with other data.
		MG_LOG_INFO("MyRequest.Execute", "%d: send sub-request 2", myID);
		res = 0;
		myCalcClient.Submit('-', 100, myID, [aSelf, &res](int64_t aRes) {
			res = aRes;
			aSelf->myTask.PostSignal();
		});
		while (!co_await aSelf->myTask.AsyncReceiveSignal());
		MG_LOG_INFO("MyRequest.Execute", "%d: got response %lld", myID, (long long)res);

		// 'delete this' + co_return wouldn't work here. Because deletion of the self
		// would destroy the C++ coroutine object. co_return would then fail with
		// use-after-free. Also can't use myTask.AsyncExitDelete(), because it would
		// delete the myTask member, not the whole MyRequest, and would crash. For this
		// case there is a special handler AsyncExitExec() - can put here any termination
		// logic.
		co_await aSelf->myTask.AsyncExitExec([this](mg::sch::Task*) {
			delete this;
		});
		MG_BOX_ASSERT(!"Unreachable");
	}

	// Can add here any other data needed by the request. Moreover, things like clients to
	// other remote systems (myCalcClient) typically would be either global singletons, or
	// would be wrapped into a bigger struct having all such clients.
	const int myID;
	CalcClient& myCalcClient;
	mg::sch::Task myTask;
};

static void
ServeRequests(
	CalcClient& aClient)
{
	// Typically here we would start a server, listen for incoming connections, receive
	// the requests, create MyRequest for each, and submit them to the scheduler. But in
	// this case it is simplified to just a couple of hardcoded MyRequests.
	mg::sch::TaskScheduler scheduler("tst",
		1, // Thread count.
		5  // Subqueue size.
	);
	MG_LOG_INFO("ServeRequests", "got a couple of complex requests");
	new MyRequest(1, aClient, scheduler);
	new MyRequest(2, aClient, scheduler);
}

//////////////////////////////////////////////////////////////////////////////////////////

static void
RunExample()
{
	mg::aio::IOCore core;
	core.Start(1 /* threads */);

	MG_LOG_INFO("Main", "start calc server");
	CalcServer* calcServer = new CalcServer(core);
	uint16_t port = calcServer->Bind();
	calcServer->Start();

	MG_LOG_INFO("Main", "start calc client");
	CalcClient *calcClient = new CalcClient(core, port);

	ServeRequests(*calcClient);

	MG_LOG_INFO("Main", "terminating");
	calcClient->Delete();
	calcServer->Delete();
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
