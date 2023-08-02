#include "mg/sio/TCPSocket.h"

#include "mg/sio/TCPServer.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace sio {
namespace tcpsocket {

	static mg::net::Socket TCPSocketConnect(
		mg::sio::TCPSocket& aClient,
		const mg::net::Host& aDstHost,
		mg::sio::TCPServer& aServer);

	static void TCPSocketWaitClosed(
		mg::sio::TCPSocket& aClient);

	static void TCPSocketRecv(
		mg::sio::TCPSocket& aSrc,
		mg::sio::TCPSocket& aDst,
		const void* aExpectedData,
		uint64_t aExpectedSize);

	static void TCPSocketSendRecv(
		mg::sio::TCPSocket& aSrc,
		mg::sio::TCPSocket& aDst);

	static void TCPSocketInteract(
		mg::sio::TCPSocket& aSock1,
		mg::sio::TCPSocket& aSock2);

	//////////////////////////////////////////////////////////////////////////////////////

	static void
	UnitTestTCPSocketDestructor()
	{
		TestCaseGuard guard("Destructor");

		mg::box::Error::Ptr err;
		mg::net::Host host;
		mg::sio::TCPServer server;
		host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(server.Bind(host, err));
		TEST_CHECK(server.Listen(mg::net::theMaxBacklog, err));
		host.SetPort(server.GetPort());

		mg::sio::TCPSocket client;
		mg::net::Socket peerSock = TCPSocketConnect(client, host, server);
		{
			mg::sio::TCPSocket peer;
			peer.Wrap(peerSock);
			TEST_CHECK(peer.IsConnected());
			TCPSocketInteract(client, peer);
		}
		TCPSocketWaitClosed(client);
	}

	static void
	UnitTestTCPSocketConnect()
	{
		TestCaseGuard guard("Connect");

		mg::box::Error::Ptr err;
		mg::net::Host host;
		mg::sio::TCPServer server;
		// To IPv4.
		host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(server.Bind(host, err));
		TEST_CHECK(server.Listen(mg::net::theMaxBacklog, err));
		host.SetPort(server.GetPort());

		mg::sio::TCPSocket client;
		mg::net::Socket peerSock = TCPSocketConnect(client, host, server);
		mg::net::SocketClose(peerSock);
		TCPSocketWaitClosed(client);
		server.Close();
	}

	static void
	UnitTestTCPSocketWrap()
	{
		TestCaseGuard guard("Wrap");

		mg::box::Error::Ptr err;
		mg::net::Host host;
		mg::sio::TCPServer server;
		host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(server.Bind(host, err));
		TEST_CHECK(server.Listen(mg::net::theMaxBacklog, err));
		host.SetPort(server.GetPort());

		mg::sio::TCPSocket client;
		mg::net::Socket peerSock = TCPSocketConnect(client, host, server);
		mg::sio::TCPSocket peer;
		peer.Wrap(peerSock);
		TEST_CHECK(peer.IsConnected());
		TCPSocketInteract(client, peer);
	}

	static void
	UnitTestTCPSocketSendRefBuffers()
	{
		TestCaseGuard guard("SendRef(Buffer)");

		mg::box::Error::Ptr err;
		mg::net::Host host;
		mg::sio::TCPServer server;
		host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(server.Bind(host, err));
		TEST_CHECK(server.Listen(mg::net::theMaxBacklog, err));
		host.SetPort(server.GetPort());

		mg::sio::TCPSocket client;
		mg::net::Socket peerSock = TCPSocketConnect(client, host, server);
		mg::sio::TCPSocket peer;
		peer.Wrap(peerSock);

		mg::net::Buffer::Ptr head = mg::net::BufferRaw::NewShared("123", 3);
		head->myNext = mg::net::BuffersCopy("45678", 5);
		client.SendRef(head.GetPointer());
		head.Clear();
		TCPSocketRecv(client, peer, "12345678", 8);
	}

	static void
	UnitTestTCPSocketSendRefVoidPtr()
	{
		TestCaseGuard guard("SendRef(void*)");

		mg::box::Error::Ptr err;
		mg::net::Host host;
		mg::sio::TCPServer server;
		host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(server.Bind(host, err));
		TEST_CHECK(server.Listen(mg::net::theMaxBacklog, err));
		host.SetPort(server.GetPort());

		mg::sio::TCPSocket client;
		mg::net::Socket peerSock = TCPSocketConnect(client, host, server);
		mg::sio::TCPSocket peer;
		peer.Wrap(peerSock);

		client.SendRef("12345", 5);
		TCPSocketRecv(client, peer, "12345", 5);
	}

	static void
	UnitTestTCPSocketSendCopyVoidPtr()
	{
		TestCaseGuard guard("SendRef(void*)");

		mg::box::Error::Ptr err;
		mg::net::Host host;
		mg::sio::TCPServer server;
		host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(server.Bind(host, err));
		TEST_CHECK(server.Listen(mg::net::theMaxBacklog, err));
		host.SetPort(server.GetPort());

		mg::sio::TCPSocket client;
		mg::net::Socket peerSock = TCPSocketConnect(client, host, server);
		mg::sio::TCPSocket peer;
		peer.Wrap(peerSock);

		char* str = mg::box::Strdup("1234567891011");
		uint32_t len = mg::box::Strlen(str);
		client.SendCopy(str, len);
		memset(str, '#', len);
		free(str);
		TCPSocketRecv(client, peer, "1234567891011", 13);
	}

	static void
	UnitTestTCPSocketRecvBuffer()
	{
		TestCaseGuard guard("Recv(Buffer)");

		mg::box::Error::Ptr err;
		mg::net::Host host;
		mg::sio::TCPServer server;
		host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(server.Bind(host, err));
		TEST_CHECK(server.Listen(mg::net::theMaxBacklog, err));
		host.SetPort(server.GetPort());

		mg::sio::TCPSocket client;
		mg::net::Socket peerSock = TCPSocketConnect(client, host, server);
		mg::sio::TCPSocket peer;
		peer.Wrap(peerSock);

		const char* expectedData = "123" "456" "7890";
		uint64_t expectedSize = mg::box::Strlen(expectedData);
		client.SendRef(expectedData, expectedSize);
		mg::net::Buffer::Ptr head = mg::net::BufferCopy::NewShared();
		mg::net::Buffer* pos = head.GetPointer();
		pos->myCapacity = 3;
		pos = (pos->myNext = mg::net::BufferCopy::NewShared()).GetPointer();
		pos->myCapacity = 3;
		pos = (pos->myNext = mg::net::BufferCopy::NewShared()).GetPointer();

		while (expectedSize > 0)
		{
			TEST_CHECK(client.Update(err));
			TEST_CHECK(client.IsConnected());
			TEST_CHECK(peer.Update(err));
			TEST_CHECK(peer.IsConnected());
			int64_t rc = peer.Recv(head.GetPointer(), err);
			TEST_CHECK(peer.IsConnected() && !err.IsSet());
			while (rc > 0)
			{
				TEST_CHECK((uint64_t)rc <= expectedSize);
				uint64_t cap = head->myCapacity - head->myPos;
				uint64_t toCmp;
				if ((uint64_t)rc <= cap)
					toCmp = rc;
				else
					toCmp = cap;
				TEST_CHECK(memcmp(expectedData,
					head->myRData + head->myPos, toCmp) == 0);
				head->myPos += (uint32_t)toCmp;
				if (head->myPos == head->myCapacity)
					head = std::move(head->myNext);
				expectedData += toCmp;
				expectedSize -= toCmp;
				rc -= toCmp;
			}
		}
	}

	static void
	UnitTestTCPSocketRecvVoidPtr()
	{
		TestCaseGuard guard("Recv(void*)");

		mg::box::Error::Ptr err;
		mg::net::Host host;
		mg::sio::TCPServer server;
		host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(server.Bind(host, err));
		TEST_CHECK(server.Listen(mg::net::theMaxBacklog, err));
		host.SetPort(server.GetPort());

		mg::sio::TCPSocket client;
		mg::net::Socket peerSock = TCPSocketConnect(client, host, server);
		mg::sio::TCPSocket peer;
		peer.Wrap(peerSock);

		const char* expectedData = "123" "456" "7890";
		uint64_t expectedSize = mg::box::Strlen(expectedData);
		client.SendRef(expectedData, expectedSize);
		constexpr uint64_t recvBufSize = 1024;
		uint8_t recvBuf[recvBufSize];

		while (expectedSize > 0)
		{
			TEST_CHECK(client.Update(err));
			TEST_CHECK(client.IsConnected());
			TEST_CHECK(peer.Update(err));
			TEST_CHECK(peer.IsConnected());
			int64_t rc = peer.Recv(recvBuf, recvBufSize, err);
			TEST_CHECK(peer.IsConnected() && !err.IsSet());
			if (rc > 0)
			{
				TEST_CHECK((uint64_t)rc <= expectedSize);
				TEST_CHECK(memcmp(expectedData, recvBuf, rc) == 0);
				expectedData += rc;
				expectedSize -= rc;
			}
		}
	}
}

	void
	UnitTestTCPSocket()
	{
		using namespace tcpsocket;
		TestSuiteGuard suite("TCPSocket");

		UnitTestTCPSocketDestructor();
		UnitTestTCPSocketConnect();
		UnitTestTCPSocketWrap();
		UnitTestTCPSocketSendRefBuffers();
		UnitTestTCPSocketSendRefVoidPtr();
		UnitTestTCPSocketSendCopyVoidPtr();
		UnitTestTCPSocketRecvBuffer();
		UnitTestTCPSocketRecvVoidPtr();
	}

namespace tcpsocket {
	//////////////////////////////////////////////////////////////////////////////////////

	static mg::net::Socket
	TCPSocketConnect(
		mg::sio::TCPSocket& aClient,
		const mg::net::Host& aDstHost,
		mg::sio::TCPServer& aServer)
	{
		mg::box::Error::Ptr err;
		TEST_CHECK(aClient.Connect(aDstHost, err));
		mg::net::Host peerHost;
		while (true)
		{
			TEST_CHECK(aClient.Update(err));
			TEST_CHECK(!aClient.IsClosed());
			mg::net::Socket sock = aServer.Accept(peerHost, err);
			if (sock != mg::net::theInvalidSocket)
			{
				while (!aClient.IsConnected())
				{
					TEST_CHECK(aClient.IsConnecting());
					TEST_CHECK(aClient.Update(err));
				}
				return sock;
			}
			TEST_CHECK(!err.IsSet());
		}
	}

	static void
	TCPSocketWaitClosed(
		mg::sio::TCPSocket& aClient)
	{
		mg::box::Error::Ptr err;
		while (!aClient.IsClosed())
		{
			uint8_t buf;
			int64_t rc = aClient.Recv(&buf, 1, err);
			TEST_CHECK(rc <= 0 && !err.IsSet());
		}
	}

	static void
	TCPSocketRecv(
		mg::sio::TCPSocket& aSrc,
		mg::sio::TCPSocket& aDst,
		const void* aExpectedData,
		uint64_t aExpectedSize)
	{
		constexpr uint64_t bufSize = 1024;
		uint8_t buf[bufSize];
		mg::box::Error::Ptr err;
		while (aExpectedSize > 0)
		{
			TEST_CHECK(aSrc.Update(err));
			TEST_CHECK(aSrc.IsConnected());
			TEST_CHECK(aDst.Update(err));
			TEST_CHECK(aDst.IsConnected());
			int64_t rc = aDst.Recv(buf, bufSize, err);
			TEST_CHECK(aDst.IsConnected() && !err.IsSet());
			if (rc > 0)
			{
				TEST_CHECK((uint64_t)rc <= aExpectedSize);
				TEST_CHECK(memcmp(aExpectedData, buf, rc) == 0);
				aExpectedData = (uint8_t*)aExpectedData + rc;
				aExpectedSize -= rc;
			}
		}
	}

	static void
	TCPSocketSendRecv(
		mg::sio::TCPSocket& aSrc,
		mg::sio::TCPSocket& aDst)
	{
		const char* data = "test data test data";
		uint64_t size = mg::box::Strlen(data) + 1;
		aSrc.SendRef(data, size);
		TCPSocketRecv(aSrc, aDst, data, size);
	}

	static void
	TCPSocketInteract(
		mg::sio::TCPSocket& aSock1,
		mg::sio::TCPSocket& aSock2)
	{
		TCPSocketSendRecv(aSock1, aSock2);
		TCPSocketSendRecv(aSock2, aSock1);
	}

}
}
}
}
