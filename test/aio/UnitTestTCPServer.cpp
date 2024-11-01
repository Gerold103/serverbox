#include "mg/aio/TCPServer.h"

#include "UnitTest.h"

#include "mg/aio/IOCore.h"
#include "mg/box/Algorithm.h"
#include "mg/sio/TCPSocket.h"

#include <deque>

namespace mg {
namespace unittests {
namespace aio {
namespace tcpserver {

	class TestTCPServerSubscription final
		: public mg::aio::TCPServerSubscription
	{
	public:
		TestTCPServerSubscription();
		~TestTCPServerSubscription();

		bool IsClosed() const;

		mg::net::Socket PopNext();

	private:
		void OnAccept(
			mg::net::Socket aSock,
			const mg::net::Host& aHost) override;
		void OnClose() override;

		mutable mg::box::Mutex myMutex;
		bool myIsClosed;
		std::deque<mg::net::Socket> myQueue;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	static void
	UnitTestTCPServerBind()
	{
		TestCaseGuard guard("Bind");
		mg::aio::IOCore core;
		core.Start(3);

		// Local IPv4.
		mg::box::Error::Ptr err;
		mg::net::Host host = mg::net::HostMakeLocalIPV4(0);
		mg::aio::TCPServer::Ptr server = mg::aio::TCPServer::NewShared(core);
		TEST_CHECK(!server->IsClosed());
		TEST_CHECK(server->Bind(host, err));
		TEST_CHECK(!server->IsClosed());
		server->PostClose();

		// Any IPv4.
		host = mg::net::HostMakeAllIPV4(0);
		server = mg::aio::TCPServer::NewShared(core);
		TEST_CHECK(server->Bind(host, err));
		uint16_t port = server->GetPort();
		TestTCPServerSubscription sub;
		TEST_CHECK(server->Listen(mg::net::SocketMaxBacklog(), &sub, err));
		TEST_CHECK(!server->IsClosed());

		// Fail when busy.
		mg::aio::TCPServer::Ptr server2 = mg::aio::TCPServer::NewShared(core);
		host.SetPort(port);
		TEST_CHECK(!server2->Bind(host, err));
		TEST_CHECK(!server2->IsClosed());
		TEST_CHECK(err->myCode == mg::box::ERR_NET_ADDR_IN_USE);

		host.SetPort(0);
		TEST_CHECK(server2->Bind(host, err));
		TEST_CHECK(!server2->IsClosed());
		TestTCPServerSubscription sub2;
		TEST_CHECK(server2->Listen(mg::net::SocketMaxBacklog(), &sub2, err));

		server->PostClose();
		server2->PostClose();
		Wait([&]() {
			return server->IsClosed() && server2->IsClosed() &&
				sub.IsClosed() && sub2.IsClosed();
		});
	}

	static void
	UnitTestTCPServerListen()
	{
		TestCaseGuard guard("Listen");
		mg::aio::IOCore core;
		core.Start(3);

		mg::box::Error::Ptr err;
		mg::net::Host host = mg::net::HostMakeLocalIPV4(0);
		mg::aio::TCPServer::Ptr server = mg::aio::TCPServer::NewShared(core);
		TEST_CHECK(server->Bind(host, err));
		TestTCPServerSubscription sub;
		TEST_CHECK(server->Listen(mg::net::SocketMaxBacklog(), &sub, err));
		TEST_CHECK(!sub.IsClosed());
		server->PostClose();
		Wait([&]() { return server->IsClosed() && sub.IsClosed(); });
	}

	static void
	UnitTestTCPServerOnAccept()
	{
		TestCaseGuard guard("Accept");
		TestTCPServerSubscription sub;
		mg::aio::IOCore core;
		core.Start(3);

		mg::box::Error::Ptr err;
		mg::net::Host host = mg::net::HostMakeLocalIPV4(0);
		mg::aio::TCPServer::Ptr server = mg::aio::TCPServer::NewShared(core);
		TEST_CHECK(server->Bind(host, err));
		host.SetPort(server->GetPort());
		uint32_t backlog = mg::net::SocketMaxBacklog();
		uint32_t clientCount = 200;
#if IS_PLATFORM_APPLE
		clientCount = mg::box::Min(clientCount, backlog);
#endif
		TEST_CHECK(server->Listen(backlog, &sub, err));

		std::vector<std::unique_ptr<mg::sio::TCPSocket>> clients;
		clients.resize(clientCount);
		std::vector<std::unique_ptr<mg::sio::TCPSocket>> peers;
		peers.resize(clientCount);
		uint32_t peerCount = 0;
		for (uint32_t i = 0; i < clientCount; ++i)
		{
			clients[i].reset(new mg::sio::TCPSocket());
			peers[i].reset(new mg::sio::TCPSocket());
			TEST_CHECK(clients[i]->Connect(host, err));
		}
		Wait([&]() {
			bool areAllConnected = true;
			for (uint32_t i = 0; i < clientCount; ++i)
			{
				TEST_CHECK(!clients[i]->IsClosed());
				TEST_CHECK(clients[i]->Update(err));
				if (clients[i]->IsConnected())
					continue;
				areAllConnected = false;
				break;
			}
			mg::net::Socket peerSock;
			while ((peerSock = sub.PopNext()) != mg::net::theInvalidSocket)
			{
				TEST_CHECK(peerCount < clientCount);
				peers[peerCount++]->Wrap(peerSock);
			}
			return areAllConnected && peerCount == clientCount;
		});

		for (uint32_t i = 0; i < clientCount; ++i)
			peers[i]->Close();
		for (uint32_t i = 0; i < clientCount; ++i)
		{
			mg::sio::TCPSocket& cli = *clients[i];
			Wait([&]() {
				if (cli.IsClosed())
					return true;
				cli.Update(err);
				uint8_t buf;
				int64_t rc = cli.Recv(&buf, 1, err);
				TEST_CHECK(rc <= 0 && !err.IsSet());
				return false;
			});
		}
		server->PostClose();
	}
}

	void
	UnitTestTCPServer()
	{
		using namespace tcpserver;
		TestSuiteGuard suite("TCPServer");

		UnitTestTCPServerBind();
		UnitTestTCPServerListen();
		UnitTestTCPServerOnAccept();
	}

	//////////////////////////////////////////////////////////////////////////////////////
namespace tcpserver {

	TestTCPServerSubscription::TestTCPServerSubscription()
		: myIsClosed(false)
	{
	}

	TestTCPServerSubscription::~TestTCPServerSubscription()
	{
		for (mg::net::Socket s : myQueue)
			mg::net::SocketClose(s);
	}

	bool
	TestTCPServerSubscription::IsClosed() const
	{
		mg::box::MutexLock lock(myMutex);
		return myIsClosed;
	}

	mg::net::Socket
	TestTCPServerSubscription::PopNext()
	{
		mg::box::MutexLock lock(myMutex);
		if (myQueue.empty())
			return mg::net::theInvalidSocket;
		mg::net::Socket res = myQueue.front();
		myQueue.pop_front();
		return res;
	}

	void
	TestTCPServerSubscription::OnAccept(
		mg::net::Socket aSock,
		const mg::net::Host& aHost)
	{
		TEST_CHECK(aHost.IsSet());
		mg::box::MutexLock lock(myMutex);
		myQueue.push_back(aSock);
	}

	void
	TestTCPServerSubscription::OnClose()
	{
		mg::box::MutexLock lock(myMutex);
		TEST_CHECK(!myIsClosed);
		myIsClosed = true;
	}
	
}
}
}
}
