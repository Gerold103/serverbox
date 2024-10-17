#include "mg/aio/TCPSocketIFace.h"

#include "mg/aio/IOCore.h"
#include "mg/aio/TCPServer.h"
#include "mg/aio/TCPSocket.h"
#include "mg/aio/TCPSocketSubscription.h"
#include "mg/box/DoublyList.h"
#include "mg/box/IOVec.h"
#include "mg/test/Message.h"

#include "UnitTest.h"

#include <functional>

#define TEST_RECV_SIZE 8092
#define TEST_YIELD_PERIOD 5

namespace mg {
namespace unittests {
namespace aio {
namespace tcpsocketiface {

	static uint64_t BuffersGetSize(
		const mg::net::BufferLink* aHead);

	static uint64_t BuffersGetSize(
		const mg::net::Buffer* aHead);

	//////////////////////////////////////////////////////////////////////////////////////

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

	enum TestMessageType
	{
		TEST_MESSAGE_ECHO,
		TEST_MESSAGE_CLOSE_AND_ECHO,
	};

	struct TestMessage
	{
		TestMessage(
			TestMessageType aType = TEST_MESSAGE_ECHO);

		TestMessageType myType;
		uint64_t myId;
		uint64_t myKey;
		uint64_t myValue;
		uint32_t myPaddingSize;
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
			TestMessage& aMsg,
			mg::tst::WriteMessage& aOut);

		mg::aio::TCPSocketIFace* mySocket;
	};

	//////////////////////////////////////////////////////////////////////////////////////
	//
	// Client to connect to a server.
	//

	using TestClientSocketOnConnectOkF = std::function<void(void)>;
	using TestClientSocketOnRecvOkF = std::function<void(void)>;
	using TestClientSocketOnCloseF = std::function<void(void)>;
	using TestClientSocketOnErrorF = std::function<void(mg::box::Error* err)>;
	using TestClientSocketOnEventF = std::function<void(void)>;

	enum TestClientEventType
	{
		TEST_CLIENT_EVENT_ON_CONNECT_OK,
		TEST_CLIENT_EVENT_ON_RECV_OK,
		TEST_CLIENT_EVENT_ON_CLOSE,
		TEST_CLIENT_EVENT_ON_ERROR,
		TEST_CLIENT_EVENT_ON_EVENT,
	};

	struct TestClientSub
	{
		TestClientSub(
			TestClientEventType aType)
			: myType(aType), myNext(nullptr), myPrev(nullptr) {}
		virtual ~TestClientSub() = default;

		virtual void Invoke() { TEST_CHECK(false); }
		virtual void Invoke(mg::box::Error *) { TEST_CHECK(false); }

		uint64_t myID;
		const TestClientEventType myType;
		TestClientSub* myNext;
		TestClientSub* myPrev;
	};

	using TestClientSubList = mg::box::DoublyList<TestClientSub>;

	struct TestClientSubOnConnectOk final
		: public TestClientSub
	{
		TestClientSubOnConnectOk() : TestClientSub(TEST_CLIENT_EVENT_ON_CONNECT_OK) {}
		void Invoke() final { myCallback(); }

		TestClientSocketOnConnectOkF myCallback;
	};

	struct TestClientSubOnRecvOk final
		: public TestClientSub
	{
		TestClientSubOnRecvOk() : TestClientSub(TEST_CLIENT_EVENT_ON_RECV_OK) {}
		void Invoke() final { myCallback(); }

		TestClientSocketOnRecvOkF myCallback;
	};

	struct TestClientSubOnClose final
		: public TestClientSub
	{
		TestClientSubOnClose() : TestClientSub(TEST_CLIENT_EVENT_ON_CLOSE) {}
		void Invoke() final { myCallback(); }

		TestClientSocketOnCloseF myCallback;
	};

	struct TestClientSubOnError final
		: public TestClientSub
	{
		TestClientSubOnError() : TestClientSub(TEST_CLIENT_EVENT_ON_ERROR) {}
		void Invoke(mg::box::Error *err) final { myCallback(err); }

		TestClientSocketOnErrorF myCallback;
	};

	struct TestClientSubOnEvent final
		: public TestClientSub
	{
		TestClientSubOnEvent() : TestClientSub(TEST_CLIENT_EVENT_ON_EVENT) {}
		void Invoke() final { myCallback(); }

		TestClientSocketOnEventF myCallback;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	struct TestClientSubInterval
	{
		SHARED_PTR_API(TestClientSubInterval, myRef);
	public:
		uint64_t myDeadline;
		uint64_t myPeriod;
		uint64_t myID;

	private:
		mg::box::RefCount myRef;
	};

	struct TestClientSubWorkerBlock
	{
		SHARED_PTR_API(TestClientSubWorkerBlock, myRef);
	public:
		mg::box::AtomicBool myDoBlock;
		mg::box::AtomicBool myIsBlocked;
		uint64_t myID;

	private:
		mg::box::RefCount myRef;
	};

	struct TestClientSubWasConnected
	{
		SHARED_PTR_API(TestClientSubWasConnected, myRef);
	public:
		mg::box::AtomicBool myWasConnected;
		uint64_t myID;

	private:
		mg::box::RefCount myRef;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	class TestClientSocket
		: public mg::aio::TCPSocketSubscription
	{
	public:
		TestClientSocket();
		~TestClientSocket();

		void PostConnect(
			uint16_t aPort);
		void PostConnect(
			const mg::aio::TCPSocketConnectParams& aParams);

		void PostConnectBlocking(
			uint16_t aPort);
		void PostConnectBlocking(
			const mg::aio::TCPSocketConnectParams& aParams);

		void PostTask();

		void Send(
			TestMessage* aMessage);
		void SendMove(
			mg::net::BufferLink* aHead);
		void SendRef(
			mg::net::Buffer* aHead);
		void SendRef(
			mg::net::Buffer::Ptr&& aHead);
		void SendRef(
			const void* aData,
			uint64_t aSize);
		void SendCopy(
			const mg::net::BufferLink* aHead);
		void SendCopy(
			const mg::net::Buffer* aHead);
		void SendCopy(
			const void* aData,
			uint64_t aSize);
		void Recv(
			uint64_t aSize);
		void PostSend(
			TestMessage* aMessage);
		void PostSendMove(
			mg::net::BufferLink* aHead);
		void PostSendRef(
			mg::net::Buffer* aHead);
		void PostSendRef(
			mg::net::Buffer::Ptr&& aHead);
		void PostSendRef(
			const void* aData,
			uint64_t aSize);
		void PostSendCopy(
			const mg::net::BufferLink* aHead);
		void PostSendCopy(
			const mg::net::Buffer* aHead);
		void PostSendCopy(
			const void* aData,
			uint64_t aSize);
		void PostRecv(
			uint64_t aSize);
		void SetDeadline(
			uint64_t aDeadline);
		void Connect(
			const mg::aio::TCPSocketConnectParams& aParams);

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

		uint64_t GetWakeupCount();

		mg::box::ErrorCode GetError();
		mg::box::ErrorCode GetErrorConnect();
		mg::box::ErrorCode GetErrorSend();
		mg::box::ErrorCode GetErrorRecv();

		uint64_t SubscribeOnConnectOk(
			TestClientSocketOnConnectOkF&& aCallback);
		uint64_t SubscribeOnRecvOk(
			TestClientSocketOnRecvOkF&& aCallback);
		uint64_t SubscribeOnClose(
			TestClientSocketOnCloseF&& aCallback);
		uint64_t SubscribeOnError(
			TestClientSocketOnErrorF&& aCallback);
		uint64_t SubscribeOnEvent(
			TestClientSocketOnEventF&& aCallback);
		void Unsubscribe(
			uint64_t aID);

		void SetAutoRecv(
			uint64_t aRecvSize = TEST_RECV_SIZE);
		TestClientSubInterval::Ptr SetInterval(
			uint64_t aPeriod);
		TestClientSubWorkerBlock::Ptr SetWorkerBlock();
		TestClientSubWasConnected::Ptr SetWatchOnConnected();

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

		template<typename SubType, typename Callback>
		uint64_t PrivSubscribe(
			Callback&& aCallback);

		mg::aio::TCPSocketIFace* mySocket;

		mg::box::Mutex myStateMutex;
		mg::box::ConditionVariable myCond;
		TestMessageList myOutMessages;
		TestMessageList myInMessages;
		mg::box::AtomicI64 myByteCountInFly;
		uint64_t myWakeupCount;
		uint64_t myCloseCount;
		mg::aio::TCPSocketState myState;
		mg::box::ErrorCode myErrorConnect;
		mg::box::ErrorCode myError;
		mg::box::ErrorCode myErrorRecv;
		mg::box::ErrorCode myErrorSend;

		mg::box::Mutex mySubsMutex;
		TestClientSubList mySubs;
		uint64_t mySubID;

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
			client.PostConnectBlocking(aPort);
			client.SetAutoRecv();
			client.Send(new TestMessage());
			delete client.PopBlocking();
			client.CloseBlocking();
		}
		{
			// One buffer for receipt.
			TestClientSocket client;
			client.PostConnectBlocking(aPort);
			client.SetAutoRecv(100);
			client.Send(new TestMessage());
			delete client.PopBlocking();
			client.CloseBlocking();
		}
		{
			// Big message sent in multiple Buffers.
			TestClientSocket client;
			client.PostConnectBlocking(aPort);
			client.SetAutoRecv(100);
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
			TestClientSocket client;
			client.PostConnectBlocking(aPort);
			const uint32_t size = 100;
			for (int i = 0; i < 100; ++i)
			{
				TestMessage* msg = new TestMessage();
				msg->myPaddingSize = size;
				client.Send(msg);
				mg::box::Sleep(10);
				TEST_CHECK(client.Pop() == nullptr);
				mg::box::Signal onRecv;
				uint64_t sid = client.SubscribeOnRecvOk([&onRecv]() {
					onRecv.Send();
				});
				while ((msg = client.Pop()) == nullptr)
				{
					client.PostRecv(1);
					onRecv.ReceiveBlocking();
				}
				client.Unsubscribe(sid);
				TEST_CHECK(msg->myPaddingSize == size);
				delete msg;
			}
			client.CloseBlocking();
		}
		{
			// Manual receive of large messages. To ensure the receive-queue
			// stays valid between on-recv calls.
			TestClientSocket client;
			client.PostConnectBlocking(aPort);
			const uint32_t size = 100000;
			for (int i = 0; i < 10; ++i)
			{
				TestMessage* msg = new TestMessage();
				msg->myPaddingSize = size;
				client.Send(msg);
				mg::box::Sleep(10);
				TEST_CHECK(client.Pop() == nullptr);
				mg::box::Signal onRecv;
				uint64_t sid = client.SubscribeOnRecvOk([&onRecv]() {
					onRecv.Send();
				});
				while ((msg = client.Pop()) == nullptr)
				{
					client.PostRecv(1);
					onRecv.ReceiveBlocking();
				}
				client.Unsubscribe(sid);
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
			client.SetAutoRecv();
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
			client.SetAutoRecv();
			testIsAlive(client);
		}
		{
			// Connect by a domain name.
			TestClientSocket client;
			mg::aio::TCPSocketConnectParams params;
			params.myEndpoint = mg::box::StringFormat("localhost:%u", aPort);
			params.myAddrFamily = mg::net::ADDR_FAMILY_IPV4;
			client.PostConnect(params);
			client.SetAutoRecv();
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
			client.SetAutoRecv();
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
			client.SetAutoRecv();
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
		client.PostConnectBlocking(aPort);
		client.SetAutoRecv();
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
		TestClientSubWasConnected::Ptr watch = client.SetWatchOnConnected();
		client.PostConnect(aPort);
		client.SetAutoRecv();
		client.Send(new TestMessage());
		client.Send(new TestMessage(TEST_MESSAGE_CLOSE_AND_ECHO));

		client.WaitClose();
		TEST_CHECK(watch->myWasConnected.LoadRelaxed());
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
		client.SetAutoRecv();
		client.Send(new TestMessage());
		delete client.PopBlocking();

		// The test is against Linux specifically. A bunch of sends + shutdown often
		// generate EPOLLHUP on the socket. There was an issue with how auto-close on
		// EPOLLHUP coexists with manual post-close.
		uint64_t id = client.SubscribeOnError([&](mg::box::Error*) {
			client.PostClose();
		});
		for (int i = 0; i < 10; ++i)
			client.Send(new TestMessage());
		client.PostShutdown();
		for (int i = 0; i < 10; ++i)
			client.Send(new TestMessage());

		client.WaitClose();
		client.Unsubscribe(id);
	}

	static void
	UnitTestTCPSocketIFacePostSend(
		uint16_t aPort)
	{
		TestCaseGuard guard("PostSend");

		TestClientSocket client;
		client.PostConnect(aPort);
		client.SetAutoRecv();
		auto interact = [&]() {
			client.PostSend(new TestMessage());
			delete client.PopBlocking();
		};
		// Post to connected or connecting client works.
		{
			interact();
		}
		// Multiple sends work fine even if the client is blocked somewhere in an IO
		// worker.
		{
			TestClientSubWorkerBlock::Ptr sub = client.SetWorkerBlock();
			while (!sub->myIsBlocked.LoadRelaxed())
				mg::box::Sleep(1);

			for (int i = 0; i < 3; ++i)
				client.PostSend(new TestMessage());

			sub->myDoBlock.StoreRelaxed(false);
			for (int i = 0; i < 3; ++i)
				delete client.PopBlocking();
			client.Unsubscribe(sub->myID);
		}
		// Empty send used to cause a crash.
		{
			client.PostSendMove((mg::net::BufferLink*)nullptr);
			interact();

			client.PostSendMove(new mg::net::BufferLink());
			interact();

			client.PostSendRef((mg::net::Buffer*)nullptr);
			interact();

			client.PostSendRef(mg::net::BufferRaw::NewShared(nullptr, 0).GetPointer());
			interact();

			client.PostSendRef(mg::net::BufferRaw::NewShared(nullptr, 0));
			interact();

			client.PostSendRef(nullptr, 0);
			interact();

			client.PostSendCopy((mg::net::BufferLink*)nullptr);
			interact();

			mg::net::BufferLink* link = new mg::net::BufferLink();
			client.PostSendCopy(link);
			delete link;
			interact();

			client.PostSendCopy((mg::net::Buffer*)nullptr);
			interact();

			mg::net::Buffer::Ptr buf = mg::net::BufferRaw::NewShared(nullptr, 0);
			client.PostSendCopy(buf.GetPointer());
			interact();

			client.PostSendCopy(nullptr, 0);
			interact();
		}
		// Empty buffer in the middle of a buffer list.
		{
			TestMessage msg;
			mg::tst::WriteMessage wm;
			msg.ToStream(wm);
			mg::net::Buffer::Ptr data = wm.TakeData();
			TEST_CHECK(data->myPos > 3);

			mg::net::Buffer::Ptr head = mg::net::BufferRaw::NewShared(data->myRData, 1);
			mg::net::Buffer* pos = head.GetPointer();
			pos->myNext = mg::net::BufferRaw::NewShared();
			pos = pos->myNext.GetPointer();

			pos->myNext = mg::net::BufferRaw::NewShared(data->myRData + 1, 1);
			pos = pos->myNext.GetPointer();

			data->myRData += 2;
			data->myPos -= 2;
			pos->myNext = data;

			client.PostSendRef(std::move(head));
			delete client.PopBlocking();
		}
		// Empty buffer links in the middle of a link chain.
		{
			TestMessage msg;
			mg::tst::WriteMessage wm;
			msg.ToStream(wm);
			mg::net::Buffer::Ptr data = wm.TakeData();
			TEST_CHECK(data->myPos > 3);

			mg::net::BufferLink* head = new mg::net::BufferLink();
			mg::net::BufferLink* pos = head;

			pos->myHead = mg::net::BufferRaw::NewShared(data->myRData, 1);
			pos = (pos->myNext = new mg::net::BufferLink());
			pos = (pos->myNext = new mg::net::BufferLink());
			pos = (pos->myNext = new mg::net::BufferLink());
			pos->myHead = mg::net::BufferRaw::NewShared();
			pos = (pos->myNext = new mg::net::BufferLink());
			pos->myHead = mg::net::BufferRaw::NewShared(data->myRData + 1, 1);
			pos = (pos->myNext = new mg::net::BufferLink());
			data->myRData += 2;
			data->myPos -= 2;
			pos->myHead = data;

			client.PostSendMove(head);
			delete client.PopBlocking();
		}
		// More empty buffers than would fit into a single send call.
		{
			TestMessage msg;
			mg::tst::WriteMessage wm;
			msg.ToStream(wm);
			mg::net::Buffer::Ptr data = wm.TakeData();
			TEST_CHECK(data->myPos > 3);

			mg::net::BufferLink* head = new mg::net::BufferLink();
			head->myHead = mg::net::BufferRaw::NewShared();
			mg::net::Buffer* pos = head->myHead.GetPointer();
			for (uint32_t i = 1; i < mg::box::theIOVecMaxCount * 2; ++i)
				pos = (pos->myNext = mg::net::BufferRaw::NewShared()).GetPointer();

			head->myNext = new mg::net::BufferLink();
			head->myNext->myHead = std::move(data);

			client.PostSendMove(head);
			delete client.PopBlocking();
		}
		client.CloseBlocking();
	}

	static void
	UnitTestTCPSocketIFaceSend(
		uint16_t aPort)
	{
		TestCaseGuard guard("Send");

		TestClientSocket client;
		client.PostConnect(aPort);
		client.SetAutoRecv();
		auto interact = [&](std::function<void(TestClientSocket&)>&& sendEmpty) {
			uint64_t id = client.SubscribeOnEvent([&]() {
				sendEmpty(client);
				client.PostSend(new TestMessage());
			});
			client.PostWakeup();
			delete client.PopBlocking();
			client.Unsubscribe(id);
		};
		// Empty send used to cause a crash.
		{
			interact([](TestClientSocket& s) {
				s.SendMove((mg::net::BufferLink*)nullptr);
			});
			interact([](TestClientSocket& s) {
				s.SendMove(new mg::net::BufferLink());
			});
			interact([](TestClientSocket& s) {
				s.SendRef((mg::net::Buffer*)nullptr);
			});
			interact([](TestClientSocket& s) {
				s.SendRef(mg::net::BufferRaw::NewShared(nullptr, 0).GetPointer());
			});
			interact([](TestClientSocket& s) {
				s.SendRef(mg::net::BufferRaw::NewShared(nullptr, 0));
			});
			interact([](TestClientSocket& s) {
				s.SendRef(nullptr, 0);
			});
			interact([](TestClientSocket& s) {
				s.SendCopy((mg::net::BufferLink*)nullptr);
			});
			interact([](TestClientSocket& s) {
				mg::net::BufferLink* link = new mg::net::BufferLink();
				s.SendCopy(link);
				delete link;
			});
			interact([](TestClientSocket& s) {
				s.SendCopy((mg::net::Buffer*)nullptr);
			});
			interact([](TestClientSocket& s) {
				mg::net::Buffer::Ptr buf = mg::net::BufferRaw::NewShared(nullptr, 0);
				s.SendCopy(buf.GetPointer());
			});
			interact([](TestClientSocket& s) {
				s.SendCopy(nullptr, 0);
			});
		}
		client.CloseBlocking();
	}

	static void
	UnitTestTCPSocketIFaceConnectError()
	{
		TestCaseGuard guard("Connect error");

		TestClientSocket client;
		TestClientSubWasConnected::Ptr watch = client.SetWatchOnConnected();
		client.PostConnect(0);
		client.WaitClose();
		TEST_CHECK(!watch->myWasConnected.LoadRelaxed());
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
				client.SetAutoRecv();
				client.PostShutdown();
				client.Send(new TestMessage());
				client.WaitClose();
			}
		}
		{
			// Multiple shutdowns should merge into each other.
			TestClientSocket client;
			client.PostConnect(aPort);
			client.SetAutoRecv();
			for (int i = 0; i < 10; ++i)
				client.PostShutdown();
			client.Send(new TestMessage());
			client.WaitClose();
		}
		{
			// Shutdown right after send.
			TestClientSocket client;
			client.PostConnect(aPort);
			client.SetAutoRecv();
			client.WaitConnect();
			// Receive something to ensure the handshake is done.
			client.Send(new TestMessage());
			delete client.PopBlocking();

			TestClientSubWorkerBlock::Ptr sub = client.SetWorkerBlock();
			while (!sub->myIsBlocked.LoadRelaxed())
				mg::box::Sleep(1);

			client.PostSend(new TestMessage());
			client.PostShutdown();

			sub->myDoBlock.StoreRelaxed(false);
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

			client.PostConnectBlocking(aPort);
			TestClientSubInterval::Ptr sub = client.SetInterval(1);
			client.WaitWakeupCountGrow(50);
			client.Unsubscribe(sub->myID);

			// Change to huge interval. No more wakeups.
			sub = client.SetInterval(1000000);
			client.WaitIdleFor(5);
			client.Unsubscribe(sub->myID);

			// Infinity, same as when not specified, means no wakeups.
			sub = client.SetInterval(MG_TIME_INFINITE);
			client.WaitIdleFor(5);
			client.Unsubscribe(sub->myID);

			// Back to huge interval - no wakeups.
			sub = client.SetInterval(1000000);
			client.WaitIdleFor(5);
			client.Unsubscribe(sub->myID);

			// Reschedule infinitely without a deadline.
			sub = client.SetInterval(0);
			client.WaitWakeupCountGrow(50);
			client.Unsubscribe(sub->myID);

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
			TestClientSubWasConnected::Ptr watch = client.SetWatchOnConnected();

			mg::aio::TCPSocketConnectParams params;
			params.myEndpoint = mg::box::StringFormat("127.0.0.1:%u", aPort);
			client.PostConnectBlocking(params);
			client.SetAutoRecv();
			TEST_CHECK(watch->myWasConnected.LoadRelaxed());
			TEST_CHECK(client.GetError() == 0);
			client.Send(new TestMessage());
			delete client.PopBlocking();
			client.CloseBlocking();
		}
		{
			TestClientSocket client;
			TestClientSubWasConnected::Ptr watch = client.SetWatchOnConnected();

			mg::aio::TCPSocketConnectParams params;
			params.myEndpoint = mg::box::StringFormat("localhost:%u", aPort);
			params.myAddrFamily = mg::net::ADDR_FAMILY_IPV4;
			client.PostConnectBlocking(params);
			client.SetAutoRecv();
			TEST_CHECK(watch->myWasConnected.LoadRelaxed());
			TEST_CHECK(client.GetError() == 0);
			client.Send(new TestMessage());
			delete client.PopBlocking();
			client.CloseBlocking();
		}
		{
			TestClientSocket client;
			TestClientSubWasConnected::Ptr watch = client.SetWatchOnConnected();

			mg::aio::TCPSocketConnectParams params;
			params.myEndpoint = mg::box::StringFormat(
				"####### invalid host #######:%u", aPort);
			client.PostConnect(params);
			client.WaitClose();
			TEST_CHECK(!watch->myWasConnected.LoadRelaxed());
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
			client.SetAutoRecv();
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

			client.SetAutoRecv();
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

			client.SetAutoRecv();
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
			TestClientSubWasConnected::Ptr watch = client.SetWatchOnConnected();

			client.PostTask();
			mg::box::Sleep(10);
			TEST_CHECK(client.GetWakeupCount() == 0);
			client.PostWakeup();
			while (client.GetWakeupCount() != 1)
				mg::box::Sleep(1);

			client.SetAutoRecv();
			client.PostSend(new TestMessage());
			TEST_CHECK(!watch->myWasConnected.LoadRelaxed());
			mg::box::Sleep(10);
			TEST_CHECK(client.Pop() == nullptr);

			client.SetInterval(1);
			client.WaitWakeupCountGrow(10);

			mg::box::AtomicBool isConnectStarted(false);
			uint64_t id = client.SubscribeOnEvent([&]() {
				if (isConnectStarted.ExchangeRelaxed(true))
					return;
				client.Connect(connParams);
			});
			client.PostWakeup();
			client.WaitConnect();
			delete client.PopBlocking();
			client.CloseBlocking();
			client.Unsubscribe(id);
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
				uint64_t id = client.SubscribeOnEvent([&]() {
					client.Connect(connParams);
				});
				client.PostWakeup();
				client.CloseBlocking();
				client.Unsubscribe(id);
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
			TestClientSocket client;
			uint64_t id = client.SubscribeOnClose([&]() {
				client.PostConnect(aPort);
			});
			client.PostConnect(aPort);
			client.SetAutoRecv();
			TestMessage msgClose(TEST_MESSAGE_CLOSE_AND_ECHO);
			msgClose.myId = 1;

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
			client.Unsubscribe(id);
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
#if IS_PLATFORM_APPLE
		// It works exceptionally slow on Mac. Might even think it hangs, until it
		// suddenly continues working after a minute. MacOS in general seems to be
		// having problems with too frequent socket accept + close, not just in this
		// code.
		constexpr int iterCount = 100;
#else
		constexpr int iterCount = 1000;
#endif
		constexpr int count = 5;
		constexpr int retryCount = 5;

		for (int i = 0; i < iterCount; ++i)
		{
			TestClientSocket clients[count];
			for (int j = 0; j < count; ++j)
			{
				TestClientSocket* cl = &clients[j];
				cl->PostConnect(aPort);
				cl->SetAutoRecv();
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
#if IS_PLATFORM_APPLE
		// It works exceptionally slow on Mac. Might even think it hangs, until it
		// suddenly continues working after a minute. MacOS in general seems to be
		// having problems with too frequent socket accept + close, not just in this
		// code.
		constexpr int iterCount = 100;
#else
		constexpr int iterCount = 1000;
#endif
		constexpr int count = 10;
		constexpr int retryCount = 100;

		TestMessage msgClose(TEST_MESSAGE_CLOSE_AND_ECHO);
		mg::box::AtomicBool doReconnect(true);

		std::vector<TestClientSocket*> clients;
		clients.resize(count);
		for (int i = 0; i < count; ++i)
		{
			TestClientSocket*& cl = clients[i];
			cl = new TestClientSocket();
			uint64_t reconnDelay = i % 3;
			cl->SubscribeOnClose([cl, reconnDelay, aPort, &doReconnect]() {
				if (!doReconnect.LoadRelaxed())
					return;
				mg::aio::TCPSocketConnectParams params;
				params.myDelay = mg::box::TimeDuration(reconnDelay);
				params.myEndpoint = mg::net::HostMakeLocalIPV4(aPort).ToString();
				cl->PostConnect(params);
			});
			cl->PostConnect(aPort);
			cl->SetAutoRecv();
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
		doReconnect.StoreRelaxed(false);
		for (int i = 0; i < count; ++i)
		{
			TestClientSocket*& cl = clients[i];
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
		UnitTestTCPSocketIFaceSend(aPort);
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

	static uint64_t
	BuffersGetSize(
		const mg::net::BufferLink* aHead)
	{
		uint64_t size = 0;
		while (aHead != nullptr)
		{
			size += BuffersGetSize(aHead->myHead.GetPointer());
			aHead = aHead->myNext;
		}
		return size;
	}

	static uint64_t
	BuffersGetSize(
		const mg::net::Buffer* aHead)
	{
		uint64_t size = 0;
		while (aHead != nullptr)
		{
			size += aHead->myPos;
			aHead = aHead->myNext.GetPointer();
		}
		return size;
	}

	//////////////////////////////////////////////////////////////////////////////////////

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

	TestMessage::TestMessage(
		TestMessageType aType)
		: myType(aType)
		, myId(1)
		, myKey(2)
		, myValue(3)
		, myPaddingSize(4)
		, myNext((TestMessage*)0x1234)
	{
	}

	void
	TestMessage::ToStream(
		mg::tst::WriteMessage& aMessage)
	{
		aMessage.Open();
		aMessage.WriteUInt8((uint8_t)myType);
		aMessage.WriteUInt64(myId);
		aMessage.WriteUInt64(myKey);
		aMessage.WriteUInt64(myValue);
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
		uint8_t valU8;
		aMessage.ReadUInt8(valU8);
		myType = (TestMessageType)valU8;
		aMessage.ReadUInt64(myId);
		aMessage.ReadUInt64(myKey);
		aMessage.ReadUInt64(myValue);
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
			PrivOnMessage(msg, wmsg);
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
		TestMessage& aMsg,
		mg::tst::WriteMessage& aOut)
	{
		switch(aMsg.myType)
		{
		case TEST_MESSAGE_ECHO:
			aMsg.ToStream(aOut);
			return;
		case TEST_MESSAGE_CLOSE_AND_ECHO:
			aMsg.ToStream(aOut);
			mySocket->PostClose();
			return;
		}
		TEST_CHECK(!"Uknown message type");
	}

	//////////////////////////////////////////////////////////////////////////////////////

	TestClientSocket::TestClientSocket()
		: mySocket(nullptr)
		, myByteCountInFly(0)
		, myWakeupCount(0)
		, myCloseCount(0)
		, myState(mg::aio::TCP_SOCKET_STATE_NEW)
		, myErrorConnect(mg::box::ERR_BOX_NONE)
		, myError(mg::box::ERR_BOX_NONE)
		, myErrorRecv(mg::box::ERR_BOX_NONE)
		, myErrorSend(mg::box::ERR_BOX_NONE)
		, mySubID(1)
	{
	}

	TestClientSocket::~TestClientSocket()
	{
		if (mySocket != nullptr)
		{
			WaitClose();
			mySocket->Delete();
			myStateMutex.Lock();
			myStateMutex.Unlock();

			mySubsMutex.Lock();
			while (!mySubs.IsEmpty())
				delete mySubs.PopFirst();
			mySubsMutex.Unlock();
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
		myStateMutex.Lock();
		TEST_CHECK(myState == mg::aio::TCP_SOCKET_STATE_NEW ||
			myState == mg::aio::TCP_SOCKET_STATE_CLOSED ||
			myState == mg::aio::TCP_SOCKET_STATE_EMPTY);
		myState = mg::aio::TCP_SOCKET_STATE_CONNECTING;
		myErrorConnect = mg::box::ERR_BOX_NONE;
		myError = mg::box::ERR_BOX_NONE;
		myErrorRecv = mg::box::ERR_BOX_NONE;
		myErrorSend = mg::box::ERR_BOX_NONE;
		myStateMutex.Unlock();

		theContext->OpenSocket(mySocket);
		mySocket->PostConnect(aParams, this);
	}

	void
	TestClientSocket::PostConnectBlocking(
		uint16_t aPort)
	{
		PostConnect(aPort);
		WaitConnect();
	}

	void
	TestClientSocket::PostConnectBlocking(
		const mg::aio::TCPSocketConnectParams& aParams)
	{
		PostConnect(aParams);
		WaitConnect();
	}

	void
	TestClientSocket::PostTask()
	{
		myStateMutex.Lock();
		TEST_CHECK(myState == mg::aio::TCP_SOCKET_STATE_NEW ||
			myState == mg::aio::TCP_SOCKET_STATE_CLOSED);
		myState = mg::aio::TCP_SOCKET_STATE_EMPTY;
		myErrorConnect = mg::box::ERR_BOX_NONE;
		myError = mg::box::ERR_BOX_NONE;
		myErrorRecv = mg::box::ERR_BOX_NONE;
		myErrorSend = mg::box::ERR_BOX_NONE;
		myStateMutex.Unlock();

		theContext->OpenSocket(mySocket);
		mySocket->PostTask(this);
	}

	void
	TestClientSocket::Send(
		TestMessage* aMessage)
	{
		myStateMutex.Lock();
		bool isFirst = myOutMessages.IsEmpty();
		myOutMessages.Append(aMessage);
		myStateMutex.Unlock();

		if (isFirst)
			mySocket->PostWakeup();
	}

	void
	TestClientSocket::SendMove(
		mg::net::BufferLink* aHead)
	{
		myByteCountInFly.AddRelaxed(BuffersGetSize(aHead));
		mySocket->SendMove(aHead);
	}

	void
	TestClientSocket::SendRef(
		mg::net::Buffer* aHead)
	{
		myByteCountInFly.AddRelaxed(BuffersGetSize(aHead));
		mySocket->SendRef(aHead);
	}

	void
	TestClientSocket::SendRef(
		mg::net::Buffer::Ptr&& aHead)
	{
		myByteCountInFly.AddRelaxed(BuffersGetSize(aHead.GetPointer()));
		mySocket->SendRef(std::move(aHead));
	}

	void
	TestClientSocket::SendRef(
		const void* aData,
		uint64_t aSize)
	{
		myByteCountInFly.AddRelaxed(aSize);
		mySocket->SendRef(aData, aSize);
	}

	void
	TestClientSocket::SendCopy(
		const mg::net::BufferLink* aHead)
	{
		myByteCountInFly.AddRelaxed(BuffersGetSize(aHead));
		mySocket->SendCopy(aHead);
	}

	void
	TestClientSocket::SendCopy(
		const mg::net::Buffer* aHead)
	{
		myByteCountInFly.AddRelaxed(BuffersGetSize(aHead));
		mySocket->SendCopy(aHead);
	}

	void
	TestClientSocket::SendCopy(
		const void* aData,
		uint64_t aSize)
	{
		myByteCountInFly.AddRelaxed(aSize);
		mySocket->SendCopy(aData, aSize);
	}

	void
	TestClientSocket::Recv(
		uint64_t aSize)
	{
		mySocket->Recv(aSize);
	}

	void
	TestClientSocket::PostSend(
		TestMessage* aMessage)
	{
		mg::tst::WriteMessage wmsg;
		aMessage->ToStream(wmsg);
		delete aMessage;
		myByteCountInFly.AddRelaxed(wmsg.GetTotalSize());
		return mySocket->PostSendRef(wmsg.TakeData());
	}

	void
	TestClientSocket::PostSendMove(
		mg::net::BufferLink* aHead)
	{
		myByteCountInFly.AddRelaxed(BuffersGetSize(aHead));
		mySocket->PostSendMove(aHead);
	}

	void
	TestClientSocket::PostSendRef(
		mg::net::Buffer* aHead)
	{
		myByteCountInFly.AddRelaxed(BuffersGetSize(aHead));
		mySocket->PostSendRef(aHead);
	}

	void
	TestClientSocket::PostSendRef(
		mg::net::Buffer::Ptr&& aHead)
	{
		myByteCountInFly.AddRelaxed(BuffersGetSize(aHead.GetPointer()));
		mySocket->PostSendRef(std::move(aHead));
	}

	void
	TestClientSocket::PostSendRef(
		const void* aData,
		uint64_t aSize)
	{
		myByteCountInFly.AddRelaxed(aSize);
		mySocket->PostSendRef(aData, aSize);
	}

	void
	TestClientSocket::PostSendCopy(
		const mg::net::BufferLink* aHead)
	{
		myByteCountInFly.AddRelaxed(BuffersGetSize(aHead));
		mySocket->PostSendCopy(aHead);
	}

	void
	TestClientSocket::PostSendCopy(
		const mg::net::Buffer* aHead)
	{
		myByteCountInFly.AddRelaxed(BuffersGetSize(aHead));
		mySocket->PostSendCopy(aHead);
	}

	void
	TestClientSocket::PostSendCopy(
		const void* aData,
		uint64_t aSize)
	{
		myByteCountInFly.AddRelaxed(aSize);
		mySocket->PostSendCopy(aData, aSize);
	}

	void
	TestClientSocket::PostRecv(
		uint64_t aSize)
	{
		mySocket->PostRecv(aSize);
	}

	void
	TestClientSocket::SetDeadline(
		uint64_t aDeadline)
	{
		mySocket->SetDeadline(aDeadline);
	}

	void
	TestClientSocket::Connect(
		const mg::aio::TCPSocketConnectParams& aParams)
	{
		myStateMutex.Lock();
		TEST_CHECK(myState == mg::aio::TCP_SOCKET_STATE_EMPTY ||
			myState == mg::aio::TCP_SOCKET_STATE_CLOSED);
		myState = mg::aio::TCP_SOCKET_STATE_CONNECTING;
		myErrorConnect = mg::box::ERR_BOX_NONE;
		myError = mg::box::ERR_BOX_NONE;
		myErrorRecv = mg::box::ERR_BOX_NONE;
		myErrorSend = mg::box::ERR_BOX_NONE;
		myStateMutex.Unlock();

		mySocket->Connect(aParams);
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
		mg::box::MutexLock lock(myStateMutex);
		while (myState != mg::aio::TCP_SOCKET_STATE_CLOSED)
			myCond.TimedWait(myStateMutex, mg::box::TimeDuration(TEST_YIELD_PERIOD));
	}

	void
	TestClientSocket::WaitConnect()
	{
		mg::box::MutexLock lock(myStateMutex);
		while (myState != mg::aio::TCP_SOCKET_STATE_CONNECTED)
			myCond.TimedWait(myStateMutex, mg::box::TimeDuration(TEST_YIELD_PERIOD));
	}

	void
	TestClientSocket::WaitIdleFor(
		uint64_t aDuration)
	{
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
		mg::box::MutexLock lock(myStateMutex);
		uint64_t target = myWakeupCount + aCount;
		while (target > myWakeupCount)
			myCond.TimedWait(myStateMutex, mg::box::TimeDuration(TEST_YIELD_PERIOD));
	}

	void
	TestClientSocket::WaitCloseCount(
		uint64_t aCount)
	{
		mg::box::MutexLock lock(myStateMutex);
		while (myCloseCount < aCount)
			myCond.TimedWait(myStateMutex, mg::box::TimeDuration(TEST_YIELD_PERIOD));
	}

	TestMessage*
	TestClientSocket::PopBlocking()
	{
		mg::box::MutexLock lock(myStateMutex);
		while (myInMessages.IsEmpty())
			myCond.TimedWait(myStateMutex, mg::box::TimeDuration(TEST_YIELD_PERIOD));
		return myInMessages.PopFirst();
	}

	TestMessage*
	TestClientSocket::Pop()
	{
		mg::box::MutexLock lock(myStateMutex);
		if (!myInMessages.IsEmpty())
			return myInMessages.PopFirst();
		return nullptr;
	}

	uint64_t
	TestClientSocket::GetWakeupCount()
	{
		mg::box::MutexLock lock(myStateMutex);
		return myWakeupCount;
	}

	mg::box::ErrorCode
	TestClientSocket::GetError()
	{
		mg::box::MutexLock lock(myStateMutex);
		return myError;
	}

	mg::box::ErrorCode
	TestClientSocket::GetErrorConnect()
	{
		mg::box::MutexLock lock(myStateMutex);
		return myErrorConnect;
	}

	mg::box::ErrorCode
	TestClientSocket::GetErrorSend()
	{
		mg::box::MutexLock lock(myStateMutex);
		return myErrorSend;
	}

	mg::box::ErrorCode
	TestClientSocket::GetErrorRecv()
	{
		mg::box::MutexLock lock(myStateMutex);
		return myErrorRecv;
	}

	uint64_t
	TestClientSocket::SubscribeOnConnectOk(
		TestClientSocketOnConnectOkF&& aCallback)
	{
		return PrivSubscribe<TestClientSubOnConnectOk>(std::move(aCallback));
	}

	uint64_t
	TestClientSocket::SubscribeOnRecvOk(
		TestClientSocketOnRecvOkF&& aCallback)
	{
		return PrivSubscribe<TestClientSubOnRecvOk>(std::move(aCallback));
	}

	uint64_t
	TestClientSocket::SubscribeOnClose(
		TestClientSocketOnCloseF&& aCallback)
	{
		return PrivSubscribe<TestClientSubOnClose>(std::move(aCallback));
	}

	uint64_t
	TestClientSocket::SubscribeOnError(
		TestClientSocketOnErrorF&& aCallback)
	{
		return PrivSubscribe<TestClientSubOnError>(std::move(aCallback));
	}

	uint64_t
	TestClientSocket::SubscribeOnEvent(
		TestClientSocketOnEventF&& aCallback)
	{
		return PrivSubscribe<TestClientSubOnEvent>(std::move(aCallback));
	}

	void
	TestClientSocket::Unsubscribe(
		uint64_t aID)
	{
		mg::box::MutexLock lock(mySubsMutex);
		for (TestClientSub* sub : mySubs)
		{
			if (sub->myID != aID)
				continue;
			mySubs.Remove(sub);
			delete sub;
			return;
		}
		TEST_CHECK(false);
	}

	void
	TestClientSocket::SetAutoRecv(
		uint64_t aRecvSize)
	{
		SubscribeOnConnectOk([this, aRecvSize]() {
			PostRecv(aRecvSize);
		});
		SubscribeOnRecvOk([this, aRecvSize]() {
			PostRecv(aRecvSize);
		});
		PostRecv(aRecvSize);
	}

	TestClientSubInterval::Ptr
	TestClientSocket::SetInterval(
		uint64_t aPeriod)
	{
		TestClientSubInterval::Ptr sub = TestClientSubInterval::NewShared();
		sub->myDeadline = 0;
		sub->myPeriod = aPeriod;
		sub->myID = SubscribeOnEvent([this, sub]() mutable {
			uint64_t now = mg::box::GetMilliseconds();
			if (now >= sub->myDeadline)
				sub->myDeadline = now + sub->myPeriod;
			SetDeadline(sub->myDeadline);
		});
		PostWakeup();
		return sub;
	}

	TestClientSubWorkerBlock::Ptr
	TestClientSocket::SetWorkerBlock()
	{
		TestClientSubWorkerBlock::Ptr sub = TestClientSubWorkerBlock::NewShared();
		sub->myDoBlock.StoreRelaxed(true);
		sub->myIsBlocked.StoreRelaxed(false);
		sub->myID = SubscribeOnEvent([sub]() mutable {
			while (sub->myDoBlock.LoadRelaxed()) {
				sub->myIsBlocked.StoreRelaxed(true);
				mg::box::Sleep(1);
			}
			sub->myIsBlocked.StoreRelaxed(false);
		});
		PostWakeup();
		return sub;
	}

	TestClientSubWasConnected::Ptr
	TestClientSocket::SetWatchOnConnected()
	{
		TestClientSubWasConnected::Ptr sub = TestClientSubWasConnected::NewShared();
		sub->myWasConnected.StoreRelaxed(false);
		sub->myID = SubscribeOnConnectOk([sub]() mutable {
			sub->myWasConnected.StoreRelaxed(true);
		});
		return sub;
	}

	void
	TestClientSocket::OnEvent()
	{
		myStateMutex.Lock();
		++myWakeupCount;
		myCond.Broadcast();
		TestMessage* head = myOutMessages.PopAll();
		myStateMutex.Unlock();

		mySubsMutex.Lock();
		for (TestClientSub* sub : mySubs)
		{
			if (sub->myType == TEST_CLIENT_EVENT_ON_EVENT)
				sub->Invoke();
		}
		mySubsMutex.Unlock();

		while (head != nullptr)
		{
			mg::tst::WriteMessage wmsg;
			TestMessage* next = head->myNext;
			head->ToStream(wmsg);
			myByteCountInFly.AddRelaxed(wmsg.GetTotalSize());
			mySocket->SendRef(wmsg.TakeData());
			delete head;
			head = next;
		}
	}

	void
	TestClientSocket::OnSend(
		uint32_t aByteCount)
	{
		mg::box::MutexLock lock(myStateMutex);
		TEST_CHECK(myState == mg::aio::TCP_SOCKET_STATE_CONNECTED);
		// Ensure it never reports more sent bytes than it was actually provided. The main
		// intension here is to test that SSL reports number of raw bytes, not encoded
		// bytes.
		TEST_CHECK(myByteCountInFly.SubFetchRelaxed(aByteCount) >= 0);
	}

	void
	TestClientSocket::OnRecv(
		mg::net::BufferReadStream& aStream)
	{
		mg::tst::ReadMessage rmsg(aStream);
		TestMessageList msgs;
		while (rmsg.IsComplete())
		{
			TestMessage* msg = new TestMessage();
			msg->FromStream(rmsg);
			msgs.Append(msg);
		}

		myStateMutex.Lock();
		TEST_CHECK(myState == mg::aio::TCP_SOCKET_STATE_CONNECTED);
		bool hasMessage = !msgs.IsEmpty();
		if (hasMessage)
		{
			myInMessages.Append(std::move(msgs));
			myCond.Broadcast();
		}
		myStateMutex.Unlock();

		mySubsMutex.Lock();
		for (TestClientSub* sub : mySubs)
		{
			if (sub->myType == TEST_CLIENT_EVENT_ON_RECV_OK)
				sub->Invoke();
		}
		mySubsMutex.Unlock();
	}

	void
	TestClientSocket::OnClose()
	{
		mg::box::MutexLock lock(myStateMutex);
		TEST_CHECK(myState != mg::aio::TCP_SOCKET_STATE_CLOSED);
		myState = mg::aio::TCP_SOCKET_STATE_CLOSED;
		++myCloseCount;
		while (!myOutMessages.IsEmpty())
			delete myOutMessages.PopFirst();
		while (!myInMessages.IsEmpty())
			delete myInMessages.PopFirst();
		myCond.Broadcast();

		mySubsMutex.Lock();
		for (TestClientSub* sub : mySubs)
		{
			if (sub->myType == TEST_CLIENT_EVENT_ON_CLOSE)
				sub->Invoke();
		}
		mySubsMutex.Unlock();
	}

	void
	TestClientSocket::OnError(
		mg::box::Error* aError)
	{
		myStateMutex.Lock();
		TEST_CHECK(myError == 0);
		myError = aError->myCode;
		myStateMutex.Unlock();

		mySubsMutex.Lock();
		for (TestClientSub* sub : mySubs)
		{
			if (sub->myType == TEST_CLIENT_EVENT_ON_ERROR)
				sub->Invoke(aError);
		}
		mySubsMutex.Unlock();
	}

	void
	TestClientSocket::OnConnect()
	{
		mg::box::MutexLock lock(myStateMutex);
		TEST_CHECK(myState == mg::aio::TCP_SOCKET_STATE_CONNECTING);
		myState = mg::aio::TCP_SOCKET_STATE_CONNECTED;
		myCond.Broadcast();

		mySubsMutex.Lock();
		for (TestClientSub* sub : mySubs)
		{
			if (sub->myType == TEST_CLIENT_EVENT_ON_CONNECT_OK)
				sub->Invoke();
		}
		mySubsMutex.Unlock();
	}

	void
	TestClientSocket::OnConnectError(
		mg::box::Error* aError)
	{
		mg::box::MutexLock lock(myStateMutex);
		TEST_CHECK(myState == mg::aio::TCP_SOCKET_STATE_CONNECTING);
		TEST_CHECK(myErrorConnect == 0);
		myErrorConnect = aError->myCode;
	}

	void
	TestClientSocket::OnRecvError(
		mg::box::Error* aError)
	{
		mg::box::MutexLock lock(myStateMutex);
		TEST_CHECK(myErrorRecv == 0);
		myErrorRecv = aError->myCode;
	}

	void
	TestClientSocket::OnSendError(
		mg::box::Error* aError)
	{
		mg::box::MutexLock lock(myStateMutex);
		TEST_CHECK(myErrorSend == 0);
		myErrorSend = aError->myCode;
	}

	template<typename SubType, typename Callback>
	uint64_t
	TestClientSocket::PrivSubscribe(
		Callback&& aCallback)
	{
		SubType* sub = new SubType();

		mg::box::MutexLock lock(mySubsMutex);
		uint64_t id = mySubID++;
		sub->myID = id;
		sub->myCallback = std::forward<Callback>(aCallback);
		mySubs.Append(sub);
		return id;
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
