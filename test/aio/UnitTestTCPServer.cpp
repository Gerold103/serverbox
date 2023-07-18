#include "mg/aio/TCPServer.h"

#include "UnitTest.h"

#include "mg/aio/IOCore.h"

#include "mg/sio/TCPSocket.h"

#include <deque>

#define TEST_CHECK MG_BOX_ASSERT

namespace mg {
namespace unittests {
namespace aio {

	class TestTCPServerSubscription
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
		mg::aio::TCPServer::Ptr server = mg::aio::TCPServer::NewShared();
		TEST_CHECK(!server->IsClosed());
		TEST_CHECK(server->Bind(host, err));
		TEST_CHECK(!server->IsClosed());
		server->Close();

		// Any IPv6.
		host = mg::net::HostMakeAllIPV6(0);
		server = mg::aio::TCPServer::NewShared();
		TEST_CHECK(server->Bind(host, err));
		uint16_t port = server->GetPort();
		TestTCPServerSubscription sub;
		TEST_CHECK(server->Listen(core, &sub, err));
		TEST_CHECK(!server->IsClosed());

		// Fail when busy.
		mg::aio::TCPServer::Ptr server2 = mg::aio::TCPServer::NewShared();
		host.SetPort(port);
		TEST_CHECK(!server2->Bind(host, err));
		TEST_CHECK(!server2->IsClosed());
		TEST_CHECK(err->myCode == mg::box::ERR_NET_ADDR_IN_USE);

		host.SetPort(0);
		TEST_CHECK(server2->Bind(host, err));
		TEST_CHECK(!server2->IsClosed());
		TestTCPServerSubscription sub2;
		TEST_CHECK(server2->Listen(core, &sub2, err));

		server->Close();
		server2->Close();
		while (!server->IsClosed() || !server2->IsClosed() ||
			!sub.IsClosed() || !sub2.IsClosed())
			mg::box::Sleep(1);
	}

	static void
	UnitTestTCPServerListen()
	{
		TestCaseGuard guard("Listen");
		mg::aio::IOCore core;
		core.Start(3);

		mg::box::Error::Ptr err;
		mg::net::Host host = mg::net::HostMakeLocalIPV4(0);
		mg::aio::TCPServer::Ptr server = mg::aio::TCPServer::NewShared();
		TEST_CHECK(server->Bind(host, err));
		TestTCPServerSubscription sub;
		TEST_CHECK(server->Listen(core, &sub, err));
		TEST_CHECK(!sub.IsClosed());
		server->Close();
		while (!server->IsClosed() || !sub.IsClosed())
			mg::box::Sleep(1);
	}

	static void
	UnitTestTCPServerOnAccept()
	{
		TestCaseGuard guard("Accept");
		mg::aio::IOCore core;
		core.Start(3);

		mg::box::Error::Ptr err;
		mg::net::Host host = mg::net::HostMakeLocalIPV4(0);
		mg::aio::TCPServer::Ptr server = mg::aio::TCPServer::NewShared();
		TEST_CHECK(server->Bind(host, err));
		host.SetPort(server->GetPort());
		TestTCPServerSubscription sub;
		TEST_CHECK(server->Listen(core, &sub, err));

		const uint32_t clientCount = 200;
		mg::sio::TCPSocket clients[clientCount];
		mg::sio::TCPSocket peers[clientCount];
		uint32_t peerCount = 0;
		for (uint32_t i = 0; i < clientCount; ++i)
			TEST_CHECK(clients[i].Connect(host, err));
		while (true)
		{
			bool areAllConnected = true;
			for (uint32_t i = 0; i < clientCount; ++i)
			{
				TEST_CHECK(!clients[i].IsClosed());
				TEST_CHECK(clients[i].Update(err));
				if (clients[i].IsConnected())
					continue;
				areAllConnected = false;
				break;
			}
			mg::net::Socket peerSock;
			while ((peerSock = sub.PopNext()) != mg::net::theInvalidSocket)
			{
				TEST_CHECK(peerCount < clientCount);
				peers[peerCount++].Wrap(peerSock);
			}
			if (areAllConnected && peerCount == clientCount)
				break;
		}

		for (uint32_t i = 0; i < clientCount; ++i)
			peers[i].Close();
		for (uint32_t i = 0; i < clientCount; ++i)
		{
			mg::sio::TCPSocket& cli = clients[i];
			while (!cli.IsClosed())
			{
				cli.Update(err);
				uint8_t buf;
				int64_t rc = cli.Recv(&buf, 1, err);
				TEST_CHECK(rc <= 0 && !err.IsSet());
			}
		}
		server->Close();
	}

	void
	UnitTestTCPServer()
	{
		TestSuiteGuard suite("TCPServer");

		UnitTestTCPServerBind();
		UnitTestTCPServerListen();
		UnitTestTCPServerOnAccept();
	}

	//////////////////////////////////////////////////////////////////////////////////////

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
