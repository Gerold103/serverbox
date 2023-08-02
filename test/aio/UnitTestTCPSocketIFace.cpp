#include "mg/aio/TCPSocketIFace.h"

#include "mg/aio/IOCore.h"
#include "mg/aio/TCPServer.h"
#include "mg/aio/TCPSocket.h"
#include "mg/aio/TCPSocketSubscription.h"
#include "mg/test/Message.h"

#include "UnitTest.h"

#define TEST_RECV_SIZE 8092
#define TEST_YIELD_PERIOD 5

namespace mg {
namespace unittests {
namespace aio {
namespace tcpsocketiface {

	class TestContext
	{
	public:
		TestContext() { myCore.Start(); }

		void OpenSocket(
			mg::aio::TCPSocketIFace*& aSocket);

		mg::aio::IOCore myCore;
	};

	static TestContext* theContext = nullptr;

	//////////////////////////////////////////////////////////////////////////////////////

	struct TestMessage
	{
		TestMessage();

		uint64_t myId;
		uint64_t myKey;
		uint64_t myValue;
		uint32_t myPaddingSize;
		bool myDoClose;
		TestMessage* myNext;

		void ToStream(
			mg::tst::WriteMessage& aMessage);
		void FromStream(
			mg::tst::ReadMessage& aMessage);
	};

	using TestMessageList = mg::box::ForwardList<TestMessage>;

	//////////////////////////////////////////////////////////////////////////////////////
	//
	// Client handler doing echo of each request and executing
	// commands in the messages.
	//

	class TestServerSocket
		: public mg::aio::TCPSocketSubscription
	{
	public:
		TestServerSocket(
			mg::net::Socket aSocket);

	private:
		void OnRecv(
			mg::net::BufferReadStream& aStream) override;

		void OnClose() override;

		void PrivOnMessage(
			const TestMessage& aMsg);

		mg::aio::TCPSocketIFace* mySocket;
	};

	//////////////////////////////////////////////////////////////////////////////////////
	//
	// Client to connect to a server.
	//

	struct TestClientSocketParams
	{
		TestClientSocketParams()
			: myRecvSize(TEST_RECV_SIZE)
			, myReconnectDelay(0)
			, myDoReconnect(false)
		{
		}

		uint32_t myRecvSize;
		uint64_t myReconnectDelay;
		bool myDoReconnect;
	};

	class TestClientSocket
		: public mg::aio::TCPSocketSubscription
	{
	public:
		TestClientSocket(
			const TestClientSocketParams& aParams = TestClientSocketParams());
		~TestClientSocket();

		void PostConnect(
			uint16_t aPort);
		void PostConnect(
			const mg::aio::TCPSocketConnectParams& aParams);

		void ConnectBlocking(
			uint16_t aPort);
		void ConnectBlocking(
			const mg::aio::TCPSocketConnectParams& aParams);

		void PostTask();
		void ConnectTask(
			const mg::aio::TCPSocketConnectParams& aParams);

		void Send(
			TestMessage* aMessage);
		void PostSend(
			TestMessage* aMessage);
		void PostRecv(
			uint64_t aSize);

		void PostWakeup() { mySocket->PostWakeup(); }
		void PostShutdown() { mySocket->PostShutdown(); }
		void PostClose() { mySocket->PostClose(); }

		void CloseBlocking();

		void WaitClose();
		void WaitConnect();
		void WaitIdleFor(
			uint64_t aDuration);
		void WaitWakeupCountGrow(
			uint64_t aCount);
		void WaitCloseCount(
			uint64_t aCount);

		TestMessage* PopBlocking();
		TestMessage* Pop();

		void DisableReconnect();
		void DisableRecvOnNextMessage();
		void BlockInWorker(
			bool aValue);
		void SetInterval(
			uint64_t aInterval);
		void SetCloseOnError(
			bool aValue);
		void SetRecvSize(
			uint32_t aValue);

		bool IsOnConnectCalled();
		uint64_t GetWakeupCount();

		mg::box::ErrorCode GetError();
		mg::box::ErrorCode GetErrorConnect();
		mg::box::ErrorCode GetErrorSend();
		mg::box::ErrorCode GetErrorRecv();

	private:
		void OnEvent() override;
		void OnSend(
			uint32_t aByteCount) override;
		void OnRecv(
			mg::net::BufferReadStream& aStream) override;
		void OnClose() override;
		void OnError(
			mg::box::Error* aError) override;
		void OnConnect() override;
		void OnConnectError(
			mg::box::Error* aError) override;
		void OnRecvError(
			mg::box::Error* aError) override;
		void OnSendError(
			mg::box::Error* aError) override;

		mg::box::Mutex myLock;
		mg::box::ConditionVariable myCond;
		TestMessageList myOutMessages;
		TestMessageList myInMessages;
		mg::aio::TCPSocketIFace* mySocket;
		mg::aio::TCPSocketConnectParams myConnectParams;
		bool myDoConnectInTask;
		bool myIsBlocked;
		bool myIsBlockRequested;
		bool myIsOnConnectCalled;
		bool myIsClosed;
		bool myDoCloseOnError;
		bool myDoReconnect;
		bool myDoDisableRecvOnNextMessage;
		const uint64_t myReconnectDelay;
		uint64_t myByteCountInFly;
		uint32_t myRecvSize;
		uint64_t myWakeupCount;
		uint64_t myCloseCount;
		uint64_t myWakeupInterval;
		mg::box::ErrorCode myErrorConnect;
		mg::box::ErrorCode myError;
		mg::box::ErrorCode myErrorRecv;
		mg::box::ErrorCode myErrorSend;
	};

	//////////////////////////////////////////////////////////////////////////////////////
	//
	// Server which creates new raw sockets in IOCore.
	//

	class TestServerSubscription final
		: public mg::aio::TCPServerSubscription
	{
	public:
		TestServerSubscription();
		~TestServerSubscription();

	private:
		void OnAccept(
			mg::net::Socket aSock,
			const mg::net::Host& aHost) override;

		void OnClose() override;

		bool myIsClosed;
	};

	//////////////////////////////////////////////////////////////////////////////////////
	//
	// The tests.
	//

	static void
	UnitTestTCPSocketIFaceBasic(
		uint16_t aPort)
	{
		TestCaseGuard guard("Basic");
		{
			// Basic test to see if works at all.
			TestClientSocket client;
			client.ConnectBlocking(aPort);
			client.Send(new TestMessage());
			delete client.PopBlocking();
			client.CloseBlocking();
		}
		{
			// One buffer for receipt.
			TestClientSocketParams params;
			params.myRecvSize = 100;
			TestClientSocket client(params);
			client.ConnectBlocking(aPort);
			client.Send(new TestMessage());
			delete client.PopBlocking();
			client.CloseBlocking();
		}
		{
			// Big message sent in multiple Buffers.
			TestClientSocketParams params;
			params.myRecvSize = 100;
			TestClientSocket client(params);
			client.ConnectBlocking(aPort);
			const uint32_t size = 1024 * 100;
			for (int i = 0; i < 100; ++i)
			{
				TestMessage* msg = new TestMessage();
				msg->myPaddingSize = size;
				client.Send(msg);
				msg = client.PopBlocking();
				TEST_CHECK(msg->myPaddingSize == size);
				delete msg;
			}
			client.CloseBlocking();
		}
		{
			// Can delete a socket if it didn't start connecting.
			mg::aio::TCPSocketIFace* sock = nullptr;
			theContext->OpenSocket(sock);
			sock->Delete();
		}
		{
			// Manual receive many.
			TestClientSocketParams params;
			params.myRecvSize = 0;
			TestClientSocket client(params);
			client.ConnectBlocking(aPort);
			const uint32_t size = 100;
			for (int i = 0; i < 100; ++i)
			{
				TestMessage* msg = new TestMessage();
				msg->myPaddingSize = size;

				mg::tst::WriteMessage wmsg;
				msg->ToStream(wmsg);
				client.Send(msg);
				mg::box::Sleep(10);
				TEST_CHECK(client.Pop() == nullptr);
				client.DisableRecvOnNextMessage();
				client.SetRecvSize(TEST_RECV_SIZE);
				client.PostRecv(1);

				msg = client.PopBlocking();
				client.SetRecvSize(0);
				TEST_CHECK(msg->myPaddingSize == size);
				delete msg;
			}
			client.CloseBlocking();
		}
	}

	static void
	UnitTestTCPSocketIFacePostConnect(
		uint16_t aPort)
	{
		TestCaseGuard guard("PostConnect()");

		auto testIsAlive = [=](TestClientSocket& aClient) {
			aClient.WaitConnect();
			aClient.Send(new TestMessage());
			delete aClient.PopBlocking();
			aClient.CloseBlocking();
		};
		{
			// Normal connect to a known local port.
			TestClientSocket client;
			client.PostConnect(aPort);
			testIsAlive(client);
		}
		mg::box::Error::Ptr err;
		mg::net::Socket sock;
		{
			// Connect with an own socket.
			TestClientSocket client;
			sock = mg::aio::SocketCreate(mg::net::ADDR_FAMILY_IPV4,
				mg::net::TRANSPORT_PROT_TCP, err);
			TEST_CHECK(sock != mg::net::theInvalidSocket);
			mg::net::Host host = mg::net::HostMakeLocalIPV4(aPort);
			mg::aio::TCPSocketConnectParams params;
			params.mySocket = sock;
			params.myEndpoint = host.ToString();
			client.PostConnect(params);
			testIsAlive(client);
		}
		{
			// Connect by a domain name.
			TestClientSocket client;
			mg::aio::TCPSocketConnectParams params;
			params.myEndpoint = mg::box::StringFormat("localhost:%u", aPort);
			params.myAddrFamily = mg::net::ADDR_FAMILY_IPV4;
			client.PostConnect(params);
			testIsAlive(client);
		}
		{
			// Connect by a domain name with an own socket.
			TestClientSocket client;
			sock = mg::aio::SocketCreate(mg::net::ADDR_FAMILY_IPV4,
				mg::net::TRANSPORT_PROT_TCP, err);
			TEST_CHECK(sock != mg::net::theInvalidSocket);
			mg::aio::TCPSocketConnectParams params;
			params.mySocket = sock;
			params.myEndpoint = mg::box::StringFormat("localhost:%u", aPort);
			params.myAddrFamily = mg::net::ADDR_FAMILY_IPV4;
			client.PostConnect(params);
			testIsAlive(client);
		}
	}

	static void
	UnitTestTCPSocketIFacePingPong(
		uint16_t aPort)
	{
		TestCaseGuard guard("Ping pong");

		constexpr uint32_t iterCount = 300;
		constexpr uint32_t parallelMsgCount = 10;

		// Many messages sent in parallel.
		for (uint32_t i = 0; i < iterCount; ++i)
		{
			TestClientSocket client;
			client.PostConnect(aPort);
			TestMessage* msg;
			for (uint32_t j = 0; j < parallelMsgCount; ++j)
			{
				msg = new TestMessage();
				msg->myId = j + i;
				msg->myKey = j + i + 1;
				msg->myValue = j + i + 2;
				msg->myPaddingSize = j + i * 100;
				client.Send(msg);
			}
			for (uint32_t j = 0; j < parallelMsgCount; ++j)
			{
				msg = client.PopBlocking();
				TEST_CHECK(msg->myId == j + i);
				TEST_CHECK(msg->myKey == j + i + 1);
				TEST_CHECK(msg->myValue == j + i + 2);
				TEST_CHECK(msg->myPaddingSize == j + i * 100);
				delete msg;
			}
			client.CloseBlocking();
			TEST_CHECK(client.IsOnConnectCalled());
			TEST_CHECK(client.GetError() == 0);
			TEST_CHECK(client.GetErrorConnect() == 0);
			TEST_CHECK(client.GetErrorSend() == 0);
			TEST_CHECK(client.GetErrorRecv() == 0);
		}
	}

	static void
	UnitTestTCPSocketIFaceCloseDuringSend(
		uint16_t aPort)
	{
		TestCaseGuard guard("Close during send");

		TestClientSocket client;
		client.ConnectBlocking(aPort);
		TEST_CHECK(client.IsOnConnectCalled());

		client.Send(new TestMessage());
		client.CloseBlocking();
		TEST_CHECK(client.GetErrorConnect() == 0);
		TEST_CHECK(client.GetErrorSend() == 0);
		TEST_CHECK(client.GetErrorRecv() == 0);
	}

	static void
	UnitTestTCPSocketIFaceCloseFromServer(
		uint16_t aPort)
	{
		TestCaseGuard guard("Close from server");

		TestClientSocket client;
		client.PostConnect(aPort);
		TestMessage* msg = new TestMessage();
		client.Send(msg);

		msg = new TestMessage();
		msg->myDoClose = true;
		client.Send(msg);

		client.WaitClose();
		TEST_CHECK(client.IsOnConnectCalled());
#if IS_PLATFORM_WIN
		// XXX: graceful remote close on Windows is not considered a success.
		TEST_CHECK(client.GetError() != 0);
		TEST_CHECK(client.GetErrorRecv() != 0);
#else
		TEST_CHECK(client.GetError() == 0);
		TEST_CHECK(client.GetErrorRecv() == 0);
#endif
		TEST_CHECK(client.GetErrorConnect() == 0);
		TEST_CHECK(client.GetErrorSend() == 0);
	}

	static void
	UnitTestTCPSocketIFaceCloseOnSendErrorAtShutdown(
		uint16_t aPort)
	{
		TestCaseGuard guard("Close on error");

		TestClientSocket client;
		client.PostConnect(aPort);
		client.Send(new TestMessage());
		delete client.PopBlocking();

		// The test is against Linux specifically. A bunch of sends + shutdown often
		// generate EPOLLHUP on the socket. There was an issue with how auto-close on
		// EPOLLHUP coexists with manual post-close.
		client.SetCloseOnError(true);
		for (int i = 0; i < 10; ++i)
			client.Send(new TestMessage());
		client.PostShutdown();
		for (int i = 0; i < 10; ++i)
			client.Send(new TestMessage());

		client.WaitClose();
	}

	static void
	UnitTestTCPSocketIFacePostSend(
		uint16_t aPort)
	{
		TestCaseGuard guard("PostSend");

		TestClientSocket client;
		client.PostConnect(aPort);
		// Post to connected or connecting client works.
		client.PostSend(new TestMessage());
		delete client.PopBlocking();

		// Multiple sends work fine even if the client is blocked somewhere in an IO
		// worker.
		client.BlockInWorker(true);
		for (int i = 0; i < 3; ++i)
			client.PostSend(new TestMessage());
		client.BlockInWorker(false);
		for (int i = 0; i < 3; ++i)
			delete client.PopBlocking();

		client.CloseBlocking();
	}

	static void
	UnitTestTCPSocketIFaceConnectError()
	{
		TestCaseGuard guard("Connect error");

		TestClientSocket client;
		client.PostConnect(0);
		client.WaitClose();
		TEST_CHECK(!client.IsOnConnectCalled());
		TEST_CHECK(client.GetError() != 0);
	}

	static void
	UnitTestTCPSocketIFaceShutdown(
		uint16_t aPort)
	{
		TestCaseGuard guard("Shutdown");
		{
			// Try many connect + shutdown to catch the moment when shutdown is handled
			// together with connect.
			for (int i = 0; i < 10; ++i)
			{
				TestClientSocket client;
				client.PostConnect(aPort);
				client.PostShutdown();
				client.Send(new TestMessage());
				client.WaitClose();
			}
		}
		{
			// Multiple shutdowns should merge into each other.
			TestClientSocket client;
			client.PostConnect(aPort);
			for (int i = 0; i < 10; ++i)
				client.PostShutdown();
			client.Send(new TestMessage());
			client.WaitClose();
		}
		{
			// Shutdown right after send.
			TestClientSocket client;
			client.PostConnect(aPort);
			client.WaitConnect();
			// Receive something to ensure the handshake is done.
			client.Send(new TestMessage());
			delete client.PopBlocking();

			client.BlockInWorker(true);
			client.Send(new TestMessage());
			client.PostShutdown();
			client.BlockInWorker(false);
			client.WaitClose();
			// It is not checked if there was an error. Because on Linux sometimes the
			// peer notices the shutdown too fast and closes the socket gracefully, so it
			// is not counted as an error.
		}
	}

	static void
	UnitTestTCPSocketIFaceDeadline(
		uint16_t aPort)
	{
		TestCaseGuard guard("Deadline");
		{
			// Wakeup every given interval even though no data to send or receive.
			TestClientSocket client;
			client.ConnectBlocking(aPort);
			client.SetInterval(1);
			client.WaitWakeupCountGrow(50);

			// Change to huge interval. No more wakeups.
			client.SetInterval(1000000);
			client.WaitIdleFor(5);

			// Infinity, same as when not specified, means no wakeups.
			client.SetInterval(MG_TIME_INFINITE);
			client.WaitIdleFor(5);

			// Back to huge interval - no wakeups.
			client.SetInterval(1000000);
			client.WaitIdleFor(5);

			// Reschedule infinitely without a deadline.
			client.SetInterval(0);
			client.WaitWakeupCountGrow(0);

			client.CloseBlocking();
		}
	}

	static void
	UnitTestTCPSocketIFaceResolve(
		uint16_t aPort)
	{
		TestCaseGuard guard("Resolve");
		{
			TestClientSocket client;
			mg::aio::TCPSocketConnectParams params;
			params.myEndpoint = mg::box::StringFormat("127.0.0.1:%u", aPort);
			client.ConnectBlocking(params);
			TEST_CHECK(client.IsOnConnectCalled());
			TEST_CHECK(client.GetError() == 0);
			client.Send(new TestMessage());
			delete client.PopBlocking();
			client.CloseBlocking();
		}
		{
			TestClientSocket client;
			mg::aio::TCPSocketConnectParams params;
			params.myEndpoint = mg::box::StringFormat("localhost:%u", aPort);
			params.myAddrFamily = mg::net::ADDR_FAMILY_IPV4;
			client.ConnectBlocking(params);
			TEST_CHECK(client.IsOnConnectCalled());
			TEST_CHECK(client.GetError() == 0);
			client.Send(new TestMessage());
			delete client.PopBlocking();
			client.CloseBlocking();
		}
		{
			TestClientSocket client;
			mg::aio::TCPSocketConnectParams params;
			params.myEndpoint = mg::box::StringFormat(
				"####### invalid host #######:%u", aPort);
			client.PostConnect(params);
			client.WaitClose();
			TEST_CHECK(!client.IsOnConnectCalled());
			TEST_CHECK(client.GetError() != 0);
		}
		mg::aio::TCPSocketConnectParams connectLocalhostParams;
		connectLocalhostParams.myEndpoint = mg::box::StringFormat("localhost:%u", aPort);
		connectLocalhostParams.myAddrFamily = mg::net::ADDR_FAMILY_IPV4;
		for (int i = 0; i < 5; ++i)
		{
			TestClientSocket client;
			client.PostConnect(connectLocalhostParams);
			client.CloseBlocking();
		}
		for (int i = 0; i < 50; ++i)
		{
			TestClientSocket client;
			uint64_t wakeupCount = client.GetWakeupCount();
			client.PostConnect(connectLocalhostParams);
			// Try to catch the moment when resolve is started but not finished. Close
			// must cancel it.
			uint64_t yield = 0;
			while (client.GetWakeupCount() == wakeupCount)
			{
				if (++yield % 10000 == 0)
					mg::box::Sleep(1);
			}
			client.CloseBlocking();
		}
		for (int i = 0; i < 5; ++i)
		{
			TestClientSocket client;
			client.PostConnect(connectLocalhostParams);
			client.PostShutdown();
			client.CloseBlocking();
		}
		for (int i = 0; i < 5; ++i)
		{
			TestClientSocket client;
			client.PostConnect(connectLocalhostParams);
			client.Send(new TestMessage());
			delete client.PopBlocking();
			client.CloseBlocking();
		}
	}

	static void
	UnitTestTCPSocketIFaceConnectDelay(
		uint16_t aPort)
	{
		TestCaseGuard guard("Connect delay");
		{
			// Connect delay with host resolution.
			TestClientSocket client;
			mg::aio::TCPSocketConnectParams params;
			params.myEndpoint = mg::box::StringFormat("localhost:%u", aPort);
			params.myAddrFamily = mg::net::ADDR_FAMILY_IPV4;
			params.myDelay = mg::box::TimeDuration(50);
			uint64_t start = mg::box::GetMilliseconds();
			client.PostConnect(params);
			client.WaitConnect();
			uint64_t end = mg::box::GetMilliseconds();
			TEST_CHECK(end - start >= params.myDelay.myDuration.myValue);

			client.Send(new TestMessage());
			delete client.PopBlocking();
			client.CloseBlocking();
		}
		{
			// Connect delay when the host is already known.
			TestClientSocket client;
			mg::aio::TCPSocketConnectParams params;
			params.myEndpoint = mg::net::HostMakeLocalIPV4(aPort).ToString();
			params.myDelay = mg::box::TimeDuration(50);
			uint64_t start = mg::box::GetMilliseconds();
			client.PostConnect(params);
			client.WaitConnect();
			uint64_t end = mg::box::GetMilliseconds();
			TEST_CHECK(end - start >= params.myDelay.myDuration.myValue);

			client.Send(new TestMessage());
			delete client.PopBlocking();
			client.CloseBlocking();
		}
	}

	static void
	UnitTestTCPSocketIFacePostTask(
		uint16_t aPort)
	{
		TestCaseGuard guard("PostTask");

		mg::aio::TCPSocketConnectParams connParams;
		connParams.myEndpoint = mg::box::StringFormat("localhost:%u", aPort);
		connParams.myAddrFamily = mg::net::ADDR_FAMILY_IPV4;
		// Ensure the socket can be used as a task and then connected later.
		{
			TestClientSocket client;
			client.PostTask();
			mg::box::Sleep(10);
			TEST_CHECK(client.GetWakeupCount() == 0);
			client.PostWakeup();
			while (client.GetWakeupCount() != 1)
				mg::box::Sleep(1);

			client.PostSend(new TestMessage());
			TEST_CHECK(!client.IsOnConnectCalled());
			mg::box::Sleep(10);
			TEST_CHECK(client.Pop() == nullptr);
			
			client.SetInterval(1);
			client.WaitWakeupCountGrow(10);

			client.ConnectTask(connParams);
			client.WaitConnect();
			delete client.PopBlocking();
			client.CloseBlocking();
		}
		// Stress test for empty tasks closure.
		{
			for (int i = 0; i < 10; ++i)
			{
				TestClientSocket client;
				client.PostTask();
				client.PostWakeup();
				if (i % 2 == 0)
					client.PostShutdown();
				client.CloseBlocking();
			}
		}
		// Stress test for in-task connect vs external close.
		{
			for (int i = 0; i < 10; ++i)
			{
				TestClientSocket client;
				client.PostTask();
				client.ConnectTask(connParams);
				client.PostWakeup();
				client.CloseBlocking();
			}
		}
	}

	static void
	UnitTestTCPSocketIFaceReconnect(
		uint16_t aPort)
	{
		TestCaseGuard guard("Reconnect");
		// Basic reconnect test.
		{
			TestClientSocketParams params;
			params.myDoReconnect = true;
			TestClientSocket client(params);
			client.PostConnect(aPort);
			TestMessage msgClose;
			msgClose.myId = 1;
			msgClose.myDoClose = true;

			TestMessage msgCheck;
			msgCheck.myId = 2;
		
			for (int i = 0; i < 10; ++i)
			{
				client.WaitConnect();
				client.Send(new TestMessage(msgClose));
				client.WaitCloseCount(i + 1);
				client.Send(new TestMessage(msgCheck));
				TestMessage* msg = client.PopBlocking();
				// The server could manage to send the response to close-message into the
				// network. Consume it then too.
				if (msg->myId == msgClose.myId)
				{
					delete msg;
					msg = client.PopBlocking();
				}
				// The normal message was posted for sending after the closure. Which
				// means it was actually sent only after reconnect happened and should get
				// a guaranteed response.
				TEST_CHECK(msg->myId == msgCheck.myId);
				delete msg;
			}
			client.DisableReconnect();
			client.CloseBlocking();
		}
	}

	static void
	UnitTestTCPSocketIFaceStressWakeupAndClose(
		uint16_t aPort)
	{
		TestCaseGuard guard("Stress wakeup and close");

		// Stress test to check how wakeups and closures live together when done multiple
		// times and at the same time.
		constexpr int iterCount = 1000;
		constexpr int count = 5;
		constexpr int retryCount = 5;

		for (int i = 0; i < iterCount; ++i)
		{
			TestClientSocket clients[count];
			for (int j = 0; j < count; ++j)
			{
				TestClientSocket* cl = &clients[j];
				cl->PostConnect(aPort);
				cl->SetInterval(j % 5);
			}
			for (int j = 0; j < count; ++j)
			{
				if (j % 2 == 0)
					clients[j].WaitConnect();
			}
			for (int j = 0; j < count; ++j)
			{
				TestClientSocket& cli = clients[j];
				if (i % 2 == 0)
					cli.Send(new TestMessage());
				if (i % 3 == 0)
				{
					cli.Send(new TestMessage());
					cli.Send(new TestMessage());
				}
				for (int k = 0; k < retryCount; ++k)
				{
					if (i % 3 == 0)
					{
						cli.PostClose();
						cli.PostWakeup();
						cli.PostClose();
					}
					else
					{
						cli.PostWakeup();
						cli.PostClose();
						cli.PostWakeup();
					}
				}
			}
		}
	}

	static void
	UnitTestTCPSocketIFaceStressReconnect(
		uint16_t aPort)
	{
		TestCaseGuard guard("Stress reconnect");

		// Stress test to check any kinds of close + reconnect + wakeup + normal messages
		// together.
		constexpr int iterCount = 1000;
		constexpr int count = 10;
		constexpr int retryCount = 100;

		TestMessage msgClose;
		msgClose.myDoClose = true;

		TestClientSocketParams params;
		params.myDoReconnect = true;
		std::vector<TestClientSocket*> clients;
		clients.resize(count);
		for (int i = 0; i < count; ++i)
		{
			params.myReconnectDelay = i % 3;
			TestClientSocket*& cl = clients[i];
			cl = new TestClientSocket(params);
			cl->PostConnect(aPort);
			cl->SetInterval(i % 5);
		}
		for (int i = 0; i < iterCount; ++i)
		{
			for (int j = 0; j < count; ++j)
			{
				if (j % 2 == 0)
					clients[j]->WaitConnect();
			}
			for (int j = 0; j < count; ++j)
			{
				TestClientSocket& cli = *clients[j];
				if (i % 2 == 0)
					cli.Send(new TestMessage());
				if (i % 3 == 0)
				{
					cli.Send(new TestMessage());
					cli.Send(new TestMessage());
				}
				if (i % 4 == 0)
					cli.Send(new TestMessage(msgClose));
				for (int k = 0; k < retryCount; ++k)
				{
					if (i % 3 == 0)
					{
						cli.PostClose();
						cli.PostWakeup();
						cli.PostClose();
					}
					else
					{
						cli.PostWakeup();
						cli.PostClose();
						cli.PostWakeup();
					}
				}
				// Don't let the messages stack without limits. Consume whatever has
				// arrived but don't block on it: 1) no guarantees for which responses
				// managed to arrive, 2) otherwise the test wouldn't be stress.
				TestMessage* msg;
				while ((msg = cli.Pop()) != nullptr)
					delete msg;
			}
		}
		for (int i = 0; i < count; ++i)
		{
			TestClientSocket*& cl = clients[i];
			cl->DisableReconnect();
			cl->CloseBlocking();
			delete cl;
		}
	}

	// Common tests which should work regardless of socket type.
	static void
	UnitTestTCPSocketIFaceSuite(
		uint16_t aPort)
	{
		UnitTestTCPSocketIFaceBasic(aPort);
		UnitTestTCPSocketIFacePostConnect(aPort);
		UnitTestTCPSocketIFacePingPong(aPort);
		UnitTestTCPSocketIFaceCloseDuringSend(aPort);
		UnitTestTCPSocketIFaceCloseFromServer(aPort);
		UnitTestTCPSocketIFaceCloseOnSendErrorAtShutdown(aPort);
		UnitTestTCPSocketIFacePostSend(aPort);
		UnitTestTCPSocketIFaceConnectError();
		UnitTestTCPSocketIFaceShutdown(aPort);
		UnitTestTCPSocketIFaceDeadline(aPort);
		UnitTestTCPSocketIFaceResolve(aPort);
		UnitTestTCPSocketIFaceConnectDelay(aPort);
		UnitTestTCPSocketIFacePostTask(aPort);
		UnitTestTCPSocketIFaceReconnect(aPort);
		UnitTestTCPSocketIFaceStressWakeupAndClose(aPort);
		UnitTestTCPSocketIFaceStressReconnect(aPort);
	}

	// TCP-specific tests.
	static void
	UnitTestTCPSocketSuite(
		uint16_t aPort)
	{
		UnitTestTCPSocketIFaceSuite(aPort);
	}
}

	void
	UnitTestTCPSocketIFace()
	{
		using namespace tcpsocketiface;
		TestSuiteGuard suite("TCPSocketIFace");

		TestContext testCtx;
		theContext = &testCtx;

		TestServerSubscription* sub = new TestServerSubscription();
		mg::aio::TCPServer::Ptr server = mg::aio::TCPServer::NewShared(testCtx.myCore);
		mg::net::Host host = mg::net::HostMakeLocalIPV4(0);
		mg::box::Error::Ptr err;
		TEST_CHECK(server->Bind(host, err));
		uint16_t port = server->GetPort();
		TEST_CHECK(server->Listen(mg::net::theMaxBacklog, sub, err));

		UnitTestTCPSocketSuite(port);

		server->PostClose();
		testCtx.myCore.WaitEmpty();
		testCtx.myCore.Stop();
		server.Clear();
		delete sub;
		theContext = nullptr;
	}

	//////////////////////////////////////////////////////////////////////////////////////
namespace tcpsocketiface {

	void
	TestContext::OpenSocket(
		mg::aio::TCPSocketIFace*& aSocket)
	{
		mg::aio::TCPSocketParams sockParams;
		if (aSocket == nullptr)
			aSocket = new mg::aio::TCPSocket(myCore);
		((mg::aio::TCPSocket*)aSocket)->Open(sockParams);
	}

	//////////////////////////////////////////////////////////////////////////////////////

	TestMessage::TestMessage()
		: myId(1)
		, myKey(2)
		, myValue(3)
		, myPaddingSize(4)
		, myDoClose(false)
		, myNext((TestMessage*)0x1234)
	{
	}

	void
	TestMessage::ToStream(
		mg::tst::WriteMessage& aMessage)
	{
		aMessage.Open();
		aMessage.WriteUInt64(myId);
		aMessage.WriteUInt64(myKey);
		aMessage.WriteUInt64(myValue);
		aMessage.WriteBool(myDoClose);
		aMessage.WriteUInt32(myPaddingSize);
		const uint32_t padPartSize = 1024;
		char padPart[padPartSize];
		uint32_t padSize = myPaddingSize;
		while (padSize >= padPartSize)
		{
			aMessage.WriteData(padPart, padPartSize);
			padSize -= padPartSize;
		}
		aMessage.WriteData(padPart, padSize);
		aMessage.Close();
	}

	void
	TestMessage::FromStream(
		mg::tst::ReadMessage& aMessage)
	{
		aMessage.Open();
		aMessage.ReadUInt64(myId);
		aMessage.ReadUInt64(myKey);
		aMessage.ReadUInt64(myValue);
		aMessage.ReadBool(myDoClose);
		aMessage.ReadUInt32(myPaddingSize);
		aMessage.SkipBytes(myPaddingSize);
		aMessage.Close();
	}

	//////////////////////////////////////////////////////////////////////////////////////

	TestServerSocket::TestServerSocket(
		mg::net::Socket aSocket)
		: mySocket(nullptr)
	{
		theContext->OpenSocket(mySocket);
		mySocket->PostRecv(TEST_RECV_SIZE);
		mySocket->PostWrap(aSocket, this);
	}

	void
	TestServerSocket::OnRecv(
		mg::net::BufferReadStream& aStream)
	{
		mg::tst::ReadMessage rmsg(aStream);
		TestMessage msg;
		mg::tst::WriteMessage wmsg;
		while (rmsg.IsComplete())
		{
			msg.FromStream(rmsg);
			msg.ToStream(wmsg);
			PrivOnMessage(msg);
		}
		if (wmsg.GetTotalSize() > 0)
			mySocket->SendRef(wmsg.TakeData());
		mySocket->Recv(TEST_RECV_SIZE);
	}

	void
	TestServerSocket::OnClose()
	{
		mySocket->Delete();
		delete this;
	}

	void
	TestServerSocket::PrivOnMessage(
		const TestMessage& aMsg)
	{
		if (aMsg.myDoClose)
			mySocket->PostClose();
	}

	//////////////////////////////////////////////////////////////////////////////////////

	TestClientSocket::TestClientSocket(
		const TestClientSocketParams& aParams)
		: mySocket(nullptr)
		, myDoConnectInTask(false)
		, myIsBlocked(false)
		, myIsBlockRequested(false)
		, myIsOnConnectCalled(false)
		, myIsClosed(false)
		, myDoCloseOnError(false)
		, myDoReconnect(aParams.myDoReconnect)
		, myDoDisableRecvOnNextMessage(false)
		, myReconnectDelay(aParams.myReconnectDelay)
		, myByteCountInFly(0)
		, myRecvSize(aParams.myRecvSize)
		, myWakeupCount(0)
		, myCloseCount(0)
		, myWakeupInterval(MG_TIME_INFINITE)
		, myErrorConnect(mg::box::ERR_BOX_NONE)
		, myError(mg::box::ERR_BOX_NONE)
		, myErrorRecv(mg::box::ERR_BOX_NONE)
		, myErrorSend(mg::box::ERR_BOX_NONE)
	{
	}

	TestClientSocket::~TestClientSocket()
	{
		if (mySocket != nullptr)
		{
			WaitClose();
			mySocket->Delete();
		}
		if (myErrorConnect != 0 || myErrorSend != 0 || myErrorRecv != 0)
			TEST_CHECK(myError != 0);
		for (TestMessage* msg : myInMessages)
			delete msg;
		for (TestMessage* msg : myOutMessages)
			delete msg;
	}

	void
	TestClientSocket::PostConnect(
		uint16_t aPort)
	{
		mg::aio::TCPSocketConnectParams params;
		params.myEndpoint = mg::net::HostMakeLocalIPV4(aPort).ToString();
		PostConnect(params);
	}

	void
	TestClientSocket::PostConnect(
		const mg::aio::TCPSocketConnectParams& aParams)
	{
		TEST_CHECK(myOutMessages.IsEmpty());
		TEST_CHECK(myInMessages.IsEmpty());
		TEST_CHECK(mySocket == nullptr);
		theContext->OpenSocket(mySocket);
		// Save params for reconnect.
		myConnectParams = aParams;
		myConnectParams.myEndpoint = aParams.myEndpoint;
		// Socket won't be usable for reconnect after closure.
		myConnectParams.mySocket = mg::net::theInvalidSocket;
		mySocket->PostConnect(aParams, this);
	}

	void
	TestClientSocket::ConnectBlocking(
		uint16_t aPort)
	{
		PostConnect(aPort);
		WaitConnect();
	}

	void
	TestClientSocket::ConnectBlocking(
		const mg::aio::TCPSocketConnectParams& aParams)
	{
		PostConnect(aParams);
		WaitConnect();
	}

	void
	TestClientSocket::PostTask()
	{
		TEST_CHECK(mySocket == nullptr);
		theContext->OpenSocket(mySocket);
		mySocket->PostTask(this);
	}

	void
	TestClientSocket::ConnectTask(
		const mg::aio::TCPSocketConnectParams& aParams)
	{
		myLock.Lock();
		TEST_CHECK(!myDoConnectInTask);
		myConnectParams = aParams;
		myDoConnectInTask = true;
		myLock.Unlock();

		mySocket->PostWakeup();
	}

	void
	TestClientSocket::Send(
		TestMessage* aMessage)
	{
		myLock.Lock();
		bool isFirst = myOutMessages.IsEmpty();
		myOutMessages.Append(aMessage);
		myLock.Unlock();

		if (isFirst)
			mySocket->PostWakeup();
	}

	void
	TestClientSocket::PostSend(
		TestMessage* aMessage)
	{
		mg::tst::WriteMessage wmsg;
		aMessage->ToStream(wmsg);
		delete aMessage;
		myByteCountInFly += wmsg.GetTotalSize();
		return mySocket->PostSendRef(wmsg.TakeData());
	}

	void
	TestClientSocket::PostRecv(
		uint64_t aSize)
	{
		mySocket->PostRecv(aSize);
	}

	void
	TestClientSocket::CloseBlocking()
	{
		PostClose();
		WaitClose();
	}

	void
	TestClientSocket::WaitClose()
	{
		mg::box::MutexLock lock(myLock);
		while (!mySocket->IsClosed() || !myIsClosed)
			myCond.TimedWait(myLock, mg::box::TimeDuration(TEST_YIELD_PERIOD));
	}

	void
	TestClientSocket::WaitConnect()
	{
		mg::box::MutexLock lock(myLock);
		while (!myIsOnConnectCalled)
		{
			TEST_CHECK(!myIsClosed);
			TEST_CHECK(!mySocket->IsClosed() || myDoReconnect);
			myCond.TimedWait(myLock, mg::box::TimeDuration(TEST_YIELD_PERIOD));
		}
	}

	void
	TestClientSocket::WaitIdleFor(
		uint64_t aDuration)
	{
		BlockInWorker(true);
		BlockInWorker(false);
		uint64_t wCount = GetWakeupCount();
		mg::box::Sleep(aDuration);
		// Worker block works via signaling. So it could wakeup one last time
		// after wcount was remembered.
		TEST_CHECK(wCount <= GetWakeupCount() + 1);
	}

	void
	TestClientSocket::WaitWakeupCountGrow(
		uint64_t aCount)
	{
		myLock.Lock();
		uint64_t target = myWakeupCount + aCount;
		while (target > myWakeupCount)
			myCond.TimedWait(myLock, mg::box::TimeDuration(TEST_YIELD_PERIOD));
		myLock.Unlock();
	}

	void
	TestClientSocket::WaitCloseCount(
		uint64_t aCount)
	{
		myLock.Lock();
		while (myCloseCount < aCount)
			myCond.TimedWait(myLock, mg::box::TimeDuration(TEST_YIELD_PERIOD));
		myLock.Unlock();
	}

	TestMessage*
	TestClientSocket::PopBlocking()
	{
		mg::box::MutexLock lock(myLock);
		while (myInMessages.IsEmpty())
			myCond.TimedWait(myLock, mg::box::TimeDuration(TEST_YIELD_PERIOD));
		return myInMessages.PopFirst();
	}

	TestMessage*
	TestClientSocket::Pop()
	{
		mg::box::MutexLock lock(myLock);
		if (!myInMessages.IsEmpty())
			return myInMessages.PopFirst();
		return nullptr;
	}

	void
	TestClientSocket::DisableReconnect()
	{
		myLock.Lock();
		myDoReconnect = false;
		myLock.Unlock();
	}

	void
	TestClientSocket::DisableRecvOnNextMessage()
	{
		myLock.Lock();
		myDoDisableRecvOnNextMessage = true;
		myLock.Unlock();
	}

	void
	TestClientSocket::BlockInWorker(
		bool aValue)
	{
		mg::box::MutexLock lock(myLock);
		TEST_CHECK(myIsBlockRequested != aValue);
		myIsBlockRequested = aValue;
		if (!aValue)
		{
			myCond.Broadcast();
			return;
		}
		while (!myIsBlocked)
		{
			mySocket->PostWakeup();
			myCond.TimedWait(myLock, mg::box::TimeDuration(TEST_YIELD_PERIOD));
		}
	}

	void
	TestClientSocket::SetInterval(
		uint64_t aInterval)
	{
		mg::box::MutexLock lock(myLock);
		if (myWakeupInterval > aInterval)
			mySocket->PostWakeup();
		myWakeupInterval = aInterval;
	}

	void
	TestClientSocket::SetCloseOnError(
		bool aValue)
	{
		mg::box::MutexLock lock(myLock);
		myDoCloseOnError = aValue;
	}

	void
	TestClientSocket::SetRecvSize(
		uint32_t aValue)
	{
		mg::box::MutexLock lock(myLock);
		myRecvSize = aValue;
	}

	bool
	TestClientSocket::IsOnConnectCalled()
	{
		mg::box::MutexLock lock(myLock);
		return myIsOnConnectCalled;
	}

	uint64_t
	TestClientSocket::GetWakeupCount()
	{
		mg::box::MutexLock lock(myLock);
		return myWakeupCount;
	}

	mg::box::ErrorCode
	TestClientSocket::GetError()
	{
		mg::box::MutexLock lock(myLock);
		return myError;
	}

	mg::box::ErrorCode
	TestClientSocket::GetErrorConnect()
	{
		mg::box::MutexLock lock(myLock);
		return myErrorConnect;
	}

	mg::box::ErrorCode
	TestClientSocket::GetErrorSend()
	{
		mg::box::MutexLock lock(myLock);
		return myErrorSend;
	}

	mg::box::ErrorCode
	TestClientSocket::GetErrorRecv()
	{
		mg::box::MutexLock lock(myLock);
		return myErrorRecv;
	}

	void
	TestClientSocket::OnEvent()
	{
		myLock.Lock();
		if (myWakeupInterval == 0)
			mySocket->Reschedule();
		else if (myWakeupInterval < MG_TIME_INFINITE)
			mySocket->SetDeadline(mg::box::GetMilliseconds() + myWakeupInterval);
		++myWakeupCount;
		myCond.Broadcast();
		if (myIsBlockRequested)
		{
			myIsBlocked = true;
			do
			{
				myCond.TimedWait(myLock, mg::box::TimeDuration(TEST_YIELD_PERIOD));
			} while (myIsBlockRequested);
			myIsBlocked = false;
		}
		if (myDoConnectInTask)
		{
			myDoConnectInTask = false;
			mySocket->Connect(myConnectParams);
		}

		TestMessage* head = myOutMessages.PopAll();
		myLock.Unlock();
		TestMessage* next;

		while (head != nullptr)
		{
			mg::tst::WriteMessage wmsg;
			next = head->myNext;
			head->ToStream(wmsg);
			myByteCountInFly += wmsg.GetTotalSize();
			mySocket->SendRef(wmsg.TakeData());
			delete head;
			head = next;
		}
	}

	void
	TestClientSocket::OnSend(
		uint32_t aByteCount)
	{
		TEST_CHECK(myIsOnConnectCalled);
		TEST_CHECK(aByteCount <= myByteCountInFly);
		// Ensure it never reports more sent bytes than it was actually provided. The main
		// intension here is to test that SSL reports number of raw bytes, not encoded
		// bytes.
		myByteCountInFly -= aByteCount;
	}

	void
	TestClientSocket::OnRecv(
		mg::net::BufferReadStream& aStream)
	{
		TEST_CHECK(myIsOnConnectCalled);
		mg::tst::ReadMessage rmsg(aStream);
		TestMessageList msgs;
		while (rmsg.IsComplete())
		{
			TestMessage* msg = new TestMessage();
			msg->FromStream(rmsg);
			msgs.Append(msg);
		}
		mg::box::MutexLock lock(myLock);
		bool hasMessage = !msgs.IsEmpty();
		if (hasMessage)
		{
			myInMessages.Append(std::move(msgs));
			myCond.Broadcast();
		}
		if (myRecvSize > 0 && !myDoDisableRecvOnNextMessage)
			mySocket->Recv(myRecvSize);
		if (hasMessage)
			myDoDisableRecvOnNextMessage = false;
	}

	void
	TestClientSocket::OnClose()
	{
		mg::box::MutexLock lock(myLock);
		TEST_CHECK(!myIsClosed);
		++myCloseCount;
		if (myDoReconnect)
		{
			myIsOnConnectCalled = false;
			myErrorRecv = mg::box::ERR_BOX_NONE;
			myErrorSend = mg::box::ERR_BOX_NONE;
			myErrorConnect = mg::box::ERR_BOX_NONE;
			myError = mg::box::ERR_BOX_NONE;
			theContext->OpenSocket(mySocket);
			myCond.Broadcast();
			myConnectParams.myDelay = mg::box::TimeDuration(myReconnectDelay);
			mySocket->PostConnect(myConnectParams, this);
			return;
		}
		while (!myOutMessages.IsEmpty())
			delete myOutMessages.PopFirst();
		while (!myInMessages.IsEmpty())
			delete myInMessages.PopFirst();
		myIsClosed = true;
		myCond.Broadcast();
	}

	void
	TestClientSocket::OnError(
		mg::box::Error* aError)
	{
		mg::box::MutexLock lock(myLock);
		TEST_CHECK(myError == 0);
		myError = aError->myCode;
		if (myDoCloseOnError)
			mySocket->PostClose();
	}

	void
	TestClientSocket::OnConnect()
	{
		mg::box::MutexLock lock(myLock);
		TEST_CHECK(!myIsOnConnectCalled);
		myIsOnConnectCalled = true;
		myCond.Broadcast();
		if (myRecvSize > 0)
			mySocket->Recv(myRecvSize);
	}

	void
	TestClientSocket::OnConnectError(
		mg::box::Error* aError)
	{
		mg::box::MutexLock lock(myLock);
		TEST_CHECK(myErrorConnect == 0);
		myErrorConnect = aError->myCode;
	}

	void
	TestClientSocket::OnRecvError(
		mg::box::Error* aError)
	{
		mg::box::MutexLock lock(myLock);
		TEST_CHECK(myErrorRecv == 0);
		myErrorRecv = aError->myCode;
	}

	void
	TestClientSocket::OnSendError(
		mg::box::Error* aError)
	{
		mg::box::MutexLock lock(myLock);
		TEST_CHECK(myErrorSend == 0);
		myErrorSend = aError->myCode;
	}

	//////////////////////////////////////////////////////////////////////////////////////

	TestServerSubscription::TestServerSubscription()
		: myIsClosed(false)
	{
	}

	TestServerSubscription::~TestServerSubscription()
	{
		TEST_CHECK(myIsClosed);
	}

	void
	TestServerSubscription::OnAccept(
		mg::net::Socket aSock,
		const mg::net::Host& /* aHost */)
	{
		new TestServerSocket(aSock);
	}

	void
	TestServerSubscription::OnClose()
	{
		myIsClosed = true;
	}

}
}
}
}
