#include "mg/sio/TCPServer.h"

#include "UnitTest.h"

#include "mg/sio/TCPSocket.h"

#define TEST_CHECK MG_BOX_ASSERT

namespace mg {
namespace unittests {
namespace sio {

	static void
	UnitTestTCPServerBind()
	{
		TestCaseGuard guard("Bind");

		// Local IPv4.
		mg::box::Error::Ptr err;
		mg::net::Host host = mg::net::HostMakeLocalIPV4(0);
		mg::sio::TCPServer server;
		TEST_CHECK(server.Bind(host, err));
		server.Close();

		// Any IPv6.
		host = mg::net::HostMakeAllIPV6(0);
		TEST_CHECK(server.Bind(host, err));
		TEST_CHECK(server.Listen(err));

		// Fail when busy.
		mg::sio::TCPServer server2;
		host.SetPort(server.GetPort());
		TEST_CHECK(!server2.Bind(host, err));
		TEST_CHECK(err->myCode == mg::box::ERR_NET_ADDR_IN_USE);

		host.SetPort(0);
		TEST_CHECK(server2.Bind(host, err));
		TEST_CHECK(server2.Listen(err));
	}

	static void
	UnitTestTCPServerListen()
	{
		TestCaseGuard guard("Listen");

		mg::box::Error::Ptr err;
		mg::net::Host host = mg::net::HostMakeLocalIPV4(0);
		mg::sio::TCPServer server;
		TEST_CHECK(server.Bind(host, err));
		TEST_CHECK(server.Listen(err));
		server.Close();
	}

	static void
	UnitTestTCPServerAccept()
	{
		TestCaseGuard guard("Accept");

		mg::box::Error::Ptr err;
		mg::net::Host host = mg::net::HostMakeLocalIPV4(0);
		mg::sio::TCPServer server;
		TEST_CHECK(server.Bind(host, err));
		TEST_CHECK(server.Listen(err));
		// No clients yet.
		mg::net::Socket sock = mg::net::theInvalidSocket;
		for (int i = 0; i < 3; ++i)
		{
			sock = server.Accept(host, err);
			TEST_CHECK(sock == mg::net::theInvalidSocket);
			TEST_CHECK(!err.IsSet());
		}

		// Make a client.
		host.SetPort(server.GetPort());
		mg::sio::TCPSocket peer;
		TEST_CHECK(peer.Connect(host, err));
		while (true)
		{
			bool isConnected = peer.IsConnected();
			TEST_CHECK(isConnected || !peer.IsClosed());
			TEST_CHECK(peer.Update(err));
			if (sock == mg::net::theInvalidSocket)
				sock = server.Accept(host, err);
			TEST_CHECK(!err.IsSet());
			if (isConnected && sock != mg::net::theInvalidSocket)
				break;
		}
		mg::net::SocketClose(sock);
		while (!peer.IsClosed())
		{
			peer.Update(err);
			uint8_t buf;
			int64_t rc = peer.Recv(&buf, 1, err);
			TEST_CHECK(rc <= 0 && !err.IsSet());
		}
	}

	static void
	UnitTestTCPServerClose()
	{
		TestCaseGuard guard("Close");

		mg::sio::TCPServer server;
		// Multiple close of a not started server.
		server.Close();
		server.Close();

		// Close a bound server.
		mg::box::Error::Ptr err;
		mg::net::Host host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(server.Bind(host, err));
		server.Close();

		// Close a listening server.
		TEST_CHECK(server.Bind(host, err));
		TEST_CHECK(server.Listen(err));
		server.Close();
	}

	void
	UnitTestTCPServer()
	{
		TestSuiteGuard suite("TCPServer");

		UnitTestTCPServerBind();
		UnitTestTCPServerListen();
		UnitTestTCPServerAccept();
		UnitTestTCPServerClose();
	}

}
}
}
