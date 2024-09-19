// ProjectFilter(Serverbox/TCPSocketBase)
#include "stdafx.h"
#include "UnitTestTCPSocketBase.h"

#include "mg/common/Error.h"
#include "mg/common/ForwardList.h"

#include "mg/network/ReadMessage.h"
#include "mg/network/WriteMessage.h"

#include "mg/clientbox/RawTCPConnection.h"
#include "mg/clientbox/Socket.h"

#include "mg/serverbox/MessageHandler.h"
#include "mg/serverbox/ScratchPad.h"
#include "mg/serverbox/SocketUtils.h"
#include "mg/serverbox/SSLSocket.h"
#include "mg/serverbox/TCPSocket.h"
#include "mg/serverbox/TCPSocketListener.h"
#include "mg/serverbox/TCPAcceptSocket.h"
#include "mg/serverbox/TCPAppSocket.h"

#include "mg/unittests/UnitTest.h"
#include "mg/unittests/UnitTestSSLCerts.h"

// UTTCPS - Unit Test TCP Socket. A unique prefix to avoid having
// an ugly private namespace with also would not be visible during
// debug.

#define TEST_RECV_SIZE 8092
#define TEST_COMPRESSION (mg::network::COMPRESSION_MODE_ZSTD)
#define TEST_YIELD_TIMEOUT 5

namespace mg {
namespace unittests {

	class TestMessageHandler;

	class TestRegistry
		: public mg::common::Singleton<TestRegistry, true>
	{
	public:
		mg::serverbox::ScratchBuffer ScratchConfig();

		void OpenClientSocket(
			mg::serverbox::TCPSocketBase*& aSocket,
			uint32 aRecvSize);

		void OpenServerSocket(
			mg::serverbox::TCPSocketBase*& aSocket,
			uint32 aRecvSize);

		TestMessageHandler* NewMessageHandler();

		mg::network::SSLContext::Ptr GetClientSSL();

		TestRegistry& Reset();

		TestRegistry& CfgSSL();

		TestRegistry& CfgBadSSL();

		TestRegistry& CfgIsSSLEnabled(
			bool aValue);

		TestRegistry& CfgApp();

		TestRegistry& CfgProofOfWork(
			uint32 aTimeout = TCP_APP_TIMEOUT_INFINITE);

		TestRegistry& CfgCompression();

	private:
		TestRegistry();

		~TestRegistry() = default;

		void PrivOpenSocket(
			mg::serverbox::TCPSocketBase*& aSocket,
			uint32 aRecvSize,
			bool aIsServer);

		mg::common::ReadWriteMutex myLock;
		mg::network::SSLContext::Ptr myCfgClientSSL;
		mg::network::SSLContext::Ptr myCfgServerSSL;
		bool myCfgIsSSLEnabled;
		bool myCfgIsApp;
		bool myCfgIsProofOfWorkEnabled;
		uint32 myCfgProofOfWorkTimeout;
		mg::network::CompressionMode myCfgCompression;

		friend Singleton;
	};

	static inline TestRegistry&
	GetRegistry()
	{
		return TestRegistry::GetInstance();
	}

	static inline mg::serverbox::ScratchBuffer
	ScratchConfig()
	{
		return GetRegistry().ScratchConfig();
	}

	TestRegistry::TestRegistry()
	{
		Reset();
	}

	mg::serverbox::ScratchBuffer
	TestRegistry::ScratchConfig()
	{
		mg::common::ReadWriteMutex::ReadLocker lock(myLock);
		mg::common::HybridString<512> str = "{";
		const char* comma = ",";
		const char* delimiter = "";
		if (myCfgServerSSL.IsSet())
		{
			MG_COMMON_ASSERT(myCfgClientSSL.IsSet());
			str.Append(delimiter);
			str.Append("ssl");
			delimiter = comma;
			if (!myCfgIsSSLEnabled)
			{
				str.Append(delimiter);
				str.Append("ssl_dis");
				delimiter = comma;
			}
		}
		else
		{
			MG_COMMON_ASSERT(!myCfgClientSSL.IsSet());
		}
		if (myCfgIsApp)
		{
			str.Append(delimiter);
			str.Append("app");
			delimiter = comma;
		}
		if (myCfgIsProofOfWorkEnabled)
		{
			str.Append(delimiter);
			str.Append("pow");
			delimiter = comma;
			if (myCfgProofOfWorkTimeout != TCP_APP_TIMEOUT_INFINITE)
			{
				str.Append(delimiter);
				str.AppendFormat("pow_%u_ms", myCfgProofOfWorkTimeout);
				delimiter = comma;
			}
		}
		if (myCfgCompression != mg::network::COMPRESSION_MODE_NONE)
		{
			str.Append(delimiter);
			str.Append("compression");
			delimiter = comma;
		}
		str.Append("}");
		return mg::serverbox::ScratchSprintf("%s", str.GetBuffer());
	}

	void
	TestRegistry::OpenClientSocket(
		mg::serverbox::TCPSocketBase*& aSocket,
		uint32 aRecvSize)
	{
		return PrivOpenSocket(aSocket, aRecvSize, false);
	}

	void
	TestRegistry::OpenServerSocket(
		mg::serverbox::TCPSocketBase*& aSocket,
		uint32 aRecvSize)
	{
		return PrivOpenSocket(aSocket, aRecvSize, true);
	}

	mg::network::SSLContext::Ptr
	TestRegistry::GetClientSSL()
	{
		mg::common::ReadWriteMutex::ReadLocker lock(myLock);
		return myCfgClientSSL;
	}

	TestRegistry&
	TestRegistry::Reset()
	{
		mg::common::ReadWriteMutex::WriteLocker lock(myLock);
		myCfgClientSSL.Clear();
		myCfgServerSSL.Clear();
		myCfgIsSSLEnabled = false;
		myCfgIsApp = false;
		myCfgIsProofOfWorkEnabled = false;
		myCfgProofOfWorkTimeout = TCP_APP_TIMEOUT_INFINITE;
		myCfgCompression = mg::network::COMPRESSION_MODE_NONE;
		return *this;
	}

	TestRegistry&
	TestRegistry::CfgSSL()
	{
		mg::common::ReadWriteMutex::WriteLocker lock(myLock);
		myCfgServerSSL = NewSSLContextServer();
		myCfgClientSSL = NewSSLContextClient();
		myCfgIsSSLEnabled = true;
		return *this;
	}

	TestRegistry&
	TestRegistry::CfgBadSSL()
	{
		mg::common::ReadWriteMutex::WriteLocker lock(myLock);

		myCfgServerSSL = mg::network::SSLContext::NewShared(true,
			mg::network::platform::SSL_TRUST_BYPASS_VERIFICATION);
		MG_COMMON_ASSERT(myCfgServerSSL->InitLocal(theUnitTestCert1, theUnitTestCert1Size,
			theUnitTestKey1, theUnitTestKey1Size));

		myCfgClientSSL = mg::network::SSLContext::NewShared(false,
			mg::network::platform::SSL_TRUST_STRICT);
		MG_COMMON_ASSERT(myCfgClientSSL->InitRemote(theUnitTestCert2,
			theUnitTestCert2Size));

		myCfgIsSSLEnabled = true;
		return *this;
	}

	TestRegistry&
	TestRegistry::CfgIsSSLEnabled(
		bool aValue)
	{
		mg::common::ReadWriteMutex::WriteLocker lock(myLock);
		myCfgIsSSLEnabled = aValue;
		return *this;
	}

	TestRegistry&
	TestRegistry::CfgApp()
	{
		mg::common::ReadWriteMutex::WriteLocker lock(myLock);
		myCfgIsApp = true;
		return *this;
	}

	TestRegistry&
	TestRegistry::CfgProofOfWork(
		uint32 aTimeout)
	{
		mg::common::ReadWriteMutex::WriteLocker lock(myLock);
		myCfgIsProofOfWorkEnabled = true;
		myCfgProofOfWorkTimeout = aTimeout;
		return *this;
	}

	TestRegistry&
	TestRegistry::CfgCompression()
	{
		mg::common::ReadWriteMutex::WriteLocker lock(myLock);
		myCfgCompression = TEST_COMPRESSION;
		return *this;
	}

	void
	TestRegistry::PrivOpenSocket(
		mg::serverbox::TCPSocketBase*& aSocket,
		uint32 aRecvSize,
		bool aIsServer)
	{
		mg::common::ReadWriteMutex::ReadLocker lock(myLock);
		mg::network::SSLContext* ssl = aIsServer ? myCfgServerSSL.GetPointer() :
			myCfgClientSSL.GetPointer();
		if (myCfgIsApp)
		{
			mg::serverbox::TCPAppSocketParams sockParams;
			sockParams.myRecvSize = aRecvSize;
			sockParams.mySSL = ssl;
			sockParams.myIsServer = aIsServer;
			sockParams.myProofOfWorkTimeout = myCfgProofOfWorkTimeout;
			sockParams.myIsProofOfWorkEnabled = myCfgIsProofOfWorkEnabled;
			sockParams.myCompressionMode = myCfgCompression;
			if (ssl != nullptr)
				MG_COMMON_ASSERT(myCfgIsSSLEnabled);
			if (aSocket == nullptr)
				aSocket = new mg::serverbox::TCPAppSocket();
			((mg::serverbox::TCPAppSocket*)aSocket)->Open(sockParams);
			return;
		}
		if (ssl != nullptr)
		{
			mg::serverbox::SSLSocketParams sockParams;
			sockParams.myRecvSize = aRecvSize;
			sockParams.mySSL = ssl;
			sockParams.myDoEnableEncryption = myCfgIsSSLEnabled;
			if (aSocket == nullptr)
				aSocket = new mg::serverbox::SSLSocket();
			((mg::serverbox::SSLSocket*)aSocket)->Open(sockParams);
			return;
		}
		mg::serverbox::TCPSocketParams sockParams;
		sockParams.myRecvSize = aRecvSize;
		if (aSocket == nullptr)
			aSocket = new mg::serverbox::TCPSocket();
		((mg::serverbox::TCPSocket*)aSocket)->Open(sockParams);
	}

	//////////////////////////////////////////////////////////////

	static uint32
	UTTCPSGetWriteBufferListSize(
		const mg::network::WriteBuffer* aHead)
	{
		uint32 res = 0;
		while (aHead != nullptr)
		{
			res += aHead->myWriteOffset;
			aHead = aHead->myNextBuffer.GetPointer();
		}
		return res;
	}

	//////////////////////////////////////////////////////////////

	struct UTTCPSMessage
	{
		UTTCPSMessage();

		uint64 myId;
		uint64 myKey;
		uint64 myValue;
		uint32 myPaddingSize;
		bool myDoClose;
		bool myDoEnableEncryption;
		UTTCPSMessage* myNext;

		void ToStream(
			mg::network::WriteMessage& aMessage);

		void FromStream(
			mg::network::ReadMessage& aMessage);
	};

	UTTCPSMessage::UTTCPSMessage()
		: myId(1)
		, myKey(2)
		, myValue(3)
		, myPaddingSize(4)
		, myDoClose(false)
		, myDoEnableEncryption(false)
		, myNext((UTTCPSMessage*)0x1234)
	{
	}

	void
	UTTCPSMessage::ToStream(
		mg::network::WriteMessage& aMessage)
	{
		aMessage.WriteDelimiter(1);
		aMessage.WriteUInt64(myId);
		aMessage.WriteUInt64(myKey);
		aMessage.WriteUInt64(myValue);
		aMessage.WriteBool(myDoClose);
		aMessage.WriteBool(myDoEnableEncryption);
		aMessage.WriteUInt32(myPaddingSize);
		const uint32 padPartSize = 1024;
		char padPart[padPartSize];
		uint32 padSize = myPaddingSize;
		while (padSize >= padPartSize)
		{
			aMessage.WriteBytes(padPart, padPartSize);
			padSize -= padPartSize;
		}
		aMessage.WriteBytes(padPart, padSize);
		aMessage.Close();
	}

	void
	UTTCPSMessage::FromStream(
		mg::network::ReadMessage& aMessage)
	{
		bool ok = aMessage.GetDelimiter() == 1;
		ok = ok && aMessage.ReadUInt64(myId);
		ok = ok && aMessage.ReadUInt64(myKey);
		ok = ok && aMessage.ReadUInt64(myValue);
		ok = ok && aMessage.ReadBool(myDoClose);
		ok = ok && aMessage.ReadBool(myDoEnableEncryption);
		ok = ok && aMessage.ReadUInt32(myPaddingSize);
		ok = ok && aMessage.SkipBytes(myPaddingSize);
		MG_COMMON_ASSERT(ok);
	}

	using UTTCPSMessageList = mg::common::ForwardList<UTTCPSMessage>;

	//////////////////////////////////////////////////////////////
	//
	// Client handler doing echo of each request and executing
	// commands in the messages.
	//

	class UTTCPSClientHandler
		: public mg::serverbox::TCPSocketListener
	{
	public:
		UTTCPSClientHandler(
			mg::serverbox::Socket aSocket);

	private:
		void OnRecv(
			mg::network::WriteBuffer* aHead,
			mg::network::WriteBuffer* aTail,
			uint32 aByteCount) override;

		void OnClose() override;

		void PrivOnMessage(
			const UTTCPSMessage& aMsg);

		mg::serverbox::TCPSocketBase* mySocket;
		mg::network::ReadStream myStream;
	};

	UTTCPSClientHandler::UTTCPSClientHandler(
		mg::serverbox::Socket aSocket)
		: mySocket(nullptr)
	{
		GetRegistry().OpenServerSocket(mySocket, TEST_RECV_SIZE);
		mySocket->PostWrap(aSocket, this);
	}

	void
	UTTCPSClientHandler::OnRecv(
		mg::network::WriteBuffer* aHead,
		mg::network::WriteBuffer* aTail,
		uint32 aByteCount)
	{
		MG_COMMON_ASSERT(aHead != nullptr);
		MG_COMMON_ASSERT(!aTail->myNextBuffer.IsSet());

		uint32 byteCount = 0;
		mg::network::WriteBuffer* pos = aHead;
		while (pos != nullptr)
		{
			byteCount += pos->myWriteOffset;
			pos = pos->myNextBuffer.GetPointer();
		}
		MG_COMMON_ASSERT(byteCount == aByteCount);

		// Copy because the original buffers are going to be
		// echoed.
		mg::network::WriteBufferList list;
		list.AppendListCopy(aHead);
		myStream.AppendBufferList(list.PopAll());

		bool ok = mySocket->Send(aHead, aTail);
		MG_COMMON_ASSERT(ok);

		mg::network::ReadMessage rmsg(&myStream);
		if (!rmsg.IsMessageComplete())
			return;

		UTTCPSMessage msg;
		do
		{
			bool ok = rmsg.DecodeHeader();
			ok = ok && rmsg.DecodeDelimiter();
			MG_COMMON_ASSERT(ok);

			msg.FromStream(rmsg);
			PrivOnMessage(msg);
		} while (rmsg.IsMessageComplete());
	}

	void
	UTTCPSClientHandler::OnClose()
	{
		mySocket->Delete();
		delete this;
	}

	void
	UTTCPSClientHandler::PrivOnMessage(
		const UTTCPSMessage& aMsg)
	{
		if (aMsg.myDoEnableEncryption)
			((mg::serverbox::SSLSocket*)mySocket)->EnableEncryption();
		if (aMsg.myDoClose)
			mySocket->PostClose();
	}

	//////////////////////////////////////////////////////////////
	//
	// Same as UTTCPSClientHandler but MessageHandler.
	//

	class TestMessageHandler
		: public mg::serverbox::MessageHandler
	{
	public:
		TestMessageHandler(
			const mg::serverbox::MessageHandlerParams& aBaseParams);

	private:
		bool OnInit(
			mg::network::WriteMessage& aWriteMessage) override;

		bool OnMessage(
			mg::network::ReadMessage& aReadMessage,
			mg::network::WriteMessage& aWriteMessage) override;

		void OnClose() override;
	};

	TestMessageHandler::TestMessageHandler(
		const mg::serverbox::MessageHandlerParams& aBaseParams)
		: MessageHandler(aBaseParams)
	{
	}

	bool
	TestMessageHandler::OnInit(
		mg::network::WriteMessage& /*aWriteMessage*/)
	{
		return true;
	}

	bool
	TestMessageHandler::OnMessage(
		mg::network::ReadMessage& aReadMessage,
		mg::network::WriteMessage& aWriteMessage)
	{
		UTTCPSMessage msg;
		msg.FromStream(aReadMessage);
		if (msg.myDoClose)
			Close();
		// Echo.
		msg.ToStream(aWriteMessage);
		return true;
	}

	void
	TestMessageHandler::OnClose()
	{
	}

	TestMessageHandler*
	TestRegistry::NewMessageHandler()
	{
		mg::serverbox::MessageHandlerParams params;

		myLock.BeginRead();
		params.mySSL = myCfgServerSSL.GetPointer();
		params.myCompressionMode = myCfgCompression;
		params.myDoProofOfWork = myCfgIsProofOfWorkEnabled;
		myLock.EndRead();

		return new TestMessageHandler(params);
	}

	//////////////////////////////////////////////////////////////
	//
	// Client to connect to a server.
	//

	struct UTTCPSClientParams
	{
		UTTCPSClientParams();

		uint32 myRecvSize;
		uint32 myReconnectDelay;
		bool myDoReconnect;
	};

	UTTCPSClientParams::UTTCPSClientParams()
		: myRecvSize(TEST_RECV_SIZE)
		, myReconnectDelay(0)
		, myDoReconnect(false)
	{
	}

	class UTTCPSClient
		: public mg::serverbox::TCPSocketListener
	{
	public:
		UTTCPSClient(
			const UTTCPSClientParams& aParams = UTTCPSClientParams());

		~UTTCPSClient();

		void Connect(
			uint16 aPort);

		void Connect(
			mg::serverbox::Socket aSocket,
			uint16 aPort);

		void Connect(
			const char* aHost,
			uint16 aPort);

		void Connect(
			mg::serverbox::Socket aSocket,
			const char* aHost,
			uint16 aPort);
		
		void Connect(
			const mg::serverbox::TCPSocketConnectParams& aParams);

		void ConnectBlocking(
			uint16 aPort);

		void ConnectBlocking(
			const char* aHost,
			uint16 aPort);

		void PostTask();

		void ConnectTask(
			const mg::serverbox::TCPSocketConnectParams& aParams);

		void Send(
			UTTCPSMessage* aMessage);

		void SendEnableEncryption();

		bool PostSend(
			UTTCPSMessage* aMessage);

		void Wakeup();

		void DisableReconnect();

		void Shutdown();

		void CloseBlocking();

		void Close();

		void WaitClose();

		void WaitConnect();

		void WaitIdleFor(
			uint32 aDuration);

		void WaitWakeupCountInc(
			uint64 aCount);

		void WaitCloseCount(
			uint64 aCount);

		UTTCPSMessage* RecvBlocking();

		UTTCPSMessage* Recv();

		// Make the client hang in a worker thread and not process
		// any new commands. Helps to test stacking of things.
		void BlockInWorker();

		void Unblock();

		void SetInterval(
			int32 aInterval);

		void ClearInterval();

		void SetCloseOnError(
			bool aValue);

		bool IsOnConnectCalled();

		uint64 GetWakeupCount();

		mg::common::ErrorCode GetError();

		mg::common::ErrorCode GetErrorConnect();

		mg::common::ErrorCode GetErrorSend();

		mg::common::ErrorCode GetErrorRecv();

	private:
		void OnWakeup() override;

		void OnSend(
			uint32 aByteCount) override;

		void OnRecv(
			mg::network::WriteBuffer* aHead,
			mg::network::WriteBuffer* aTail,
			uint32 aByteCount) override;

		void OnClose() override;

		void OnError(
			mg::common::Error* aError) override;

		void OnConnect() override;

		void OnConnectError(
			mg::common::Error* aError) override;

		void OnRecvError(
			mg::common::Error* aError) override;

		void OnSendError(
			mg::common::Error* aError) override;

		void PrivOnMessage(
			UTTCPSMessage* aMsg);

		mg::common::Mutex myLock;
		mg::common::ConditionVariable myCond;
		UTTCPSMessageList myOutMessages;
		UTTCPSMessageList myInMessages;
		mg::serverbox::TCPSocketBase* mySocket;
		mg::network::ReadStream myStream;
		mg::serverbox::TCPSocketConnectParams myConnectParams;
		bool myDoConnectInTask;
		bool myIsBlocked;
		bool myIsBlockRequested;
		bool myIsOnConnectCalled;
		bool myIsClosed;
		bool myDoCloseOnError;
		bool myDoReconnect;
		const uint32 myReconnectDelay;
		uint32 myByteCountInFly;
		const uint32 myRecvSize;
		uint64 myWakeupCount;
		uint64 myCloseCount;
		int32 myWakeupInterval;
		mg::common::ErrorCode myErrorConnect;
		mg::common::ErrorCode myError;
		mg::common::ErrorCode myErrorRecv;
		mg::common::ErrorCode myErrorSend;
		mg::common::HybridString<64> myHostStorage;
		mg::network::Host myAddrStorage;
	};

	UTTCPSClient::UTTCPSClient(
		const UTTCPSClientParams& aParams)
		: mySocket(nullptr)
		, myDoConnectInTask(false)
		, myIsBlocked(false)
		, myIsBlockRequested(false)
		, myIsOnConnectCalled(false)
		, myIsClosed(false)
		, myDoCloseOnError(false)
		, myDoReconnect(aParams.myDoReconnect)
		, myReconnectDelay(aParams.myReconnectDelay)
		, myByteCountInFly(0)
		, myRecvSize(aParams.myRecvSize)
		, myWakeupCount(0)
		, myCloseCount(0)
		, myWakeupInterval(INT32_MAX)
		, myErrorConnect(mg::common::ERR_NONE)
		, myError(mg::common::ERR_NONE)
		, myErrorRecv(mg::common::ERR_NONE)
		, myErrorSend(mg::common::ERR_NONE)
	{
	}

	UTTCPSClient::~UTTCPSClient()
	{
		if (mySocket != nullptr)
		{
			WaitClose();
			mySocket->Delete();
		}
		if (myErrorConnect != 0 || myErrorSend != 0 || myErrorRecv != 0)
			MG_COMMON_ASSERT(myError != 0);
		for (UTTCPSMessage* msg : myInMessages)
			delete msg;
		for (UTTCPSMessage* msg : myOutMessages)
			delete msg;
	}

	void
	UTTCPSClient::Connect(
		uint16 aPort)
	{
		MG_COMMON_ASSERT(myOutMessages.IsEmpty());
		MG_COMMON_ASSERT(myInMessages.IsEmpty());
		MG_COMMON_ASSERT(mySocket == nullptr);
		GetRegistry().OpenClientSocket(mySocket, myRecvSize);
		// Save params for reconnect.
		myAddrStorage = mg::network::HostMakeLocalIPV6(aPort);
		myConnectParams.myAddr = &myAddrStorage;

		mySocket->PostConnect(myAddrStorage, this);
	}

	void
	UTTCPSClient::Connect(
		mg::serverbox::Socket aSocket,
		uint16 aPort)
	{
		MG_COMMON_ASSERT(myOutMessages.IsEmpty());
		MG_COMMON_ASSERT(myInMessages.IsEmpty());
		MG_COMMON_ASSERT(mySocket == nullptr);
		GetRegistry().OpenClientSocket(mySocket, myRecvSize);
		// Save params for reconnect.
		myAddrStorage = mg::network::HostMakeLocalIPV6(aPort);
		myConnectParams.myAddr = &myAddrStorage;

		mySocket->PostConnect(aSocket, myAddrStorage, this);
	}

	void
	UTTCPSClient::Connect(
		const char* aHost,
		uint16 aPort)
	{
		MG_COMMON_ASSERT(myOutMessages.IsEmpty());
		MG_COMMON_ASSERT(myInMessages.IsEmpty());
		MG_COMMON_ASSERT(mySocket == nullptr);
		GetRegistry().OpenClientSocket(mySocket, myRecvSize);
		// Save params for reconnect.
		myHostStorage = aHost;
		myConnectParams.myHost = myHostStorage;
		myConnectParams.myPort = aPort;

		mySocket->PostConnect(aHost, aPort, this);
	}

	void
	UTTCPSClient::Connect(
		mg::serverbox::Socket aSocket,
		const char* aHost,
		uint16 aPort)
	{
		MG_COMMON_ASSERT(myOutMessages.IsEmpty());
		MG_COMMON_ASSERT(myInMessages.IsEmpty());
		MG_COMMON_ASSERT(mySocket == nullptr);
		GetRegistry().OpenClientSocket(mySocket, myRecvSize);
		// Save params for reconnect.
		myHostStorage = aHost;
		myConnectParams.myHost = myHostStorage;
		myConnectParams.myPort = aPort;

		mySocket->PostConnect(aSocket, aHost, aPort, this);
	}

	void
	UTTCPSClient::Connect(
		const mg::serverbox::TCPSocketConnectParams& aParams)
	{
		MG_COMMON_ASSERT(myOutMessages.IsEmpty());
		MG_COMMON_ASSERT(myInMessages.IsEmpty());
		MG_COMMON_ASSERT(mySocket == nullptr);
		GetRegistry().OpenClientSocket(mySocket, myRecvSize);
		// Save params for reconnect.
		myConnectParams = aParams;
		myHostStorage = aParams.myHost;
		myConnectParams.myHost = myHostStorage;
		if (aParams.myAddr != nullptr)
		{
			myAddrStorage = *aParams.myAddr;
			myConnectParams.myAddr = &myAddrStorage;
		}
		// Socket won't be usable for reconnect after closure.
		myConnectParams.mySocket = INVALID_SOCKET_VALUE;

		mySocket->PostConnect(aParams, this);
	}

	void
	UTTCPSClient::ConnectBlocking(
		uint16 aPort)
	{
		Connect(aPort);
		WaitConnect();
	}

	void
	UTTCPSClient::ConnectBlocking(
		const char* aHost,
		uint16 aPort)
	{
		Connect(aHost, aPort);
		WaitConnect();
	}

	void
	UTTCPSClient::PostTask()
	{
		MG_COMMON_ASSERT(mySocket == nullptr);
		GetRegistry().OpenClientSocket(mySocket, myRecvSize);
		mySocket->PostTask(this);
	}

	void
	UTTCPSClient::ConnectTask(
		const mg::serverbox::TCPSocketConnectParams& aParams)
	{
		myLock.Lock();
		MG_COMMON_ASSERT(!myDoConnectInTask);
		myConnectParams = aParams;
		myDoConnectInTask = true;
		myLock.Unlock();

		mySocket->PostWakeup();
	}

	void
	UTTCPSClient::Send(
		UTTCPSMessage* aMessage)
	{
		myLock.Lock();
		bool isFirst = myOutMessages.IsEmpty();
		myOutMessages.Append(aMessage);
		myLock.Unlock();

		if (isFirst)
			mySocket->PostWakeup();
	}

	void
	UTTCPSClient::SendEnableEncryption()
	{
		UTTCPSMessage* msg = new UTTCPSMessage();
		msg->myDoEnableEncryption = true;
		Send(msg);
	}

	bool
	UTTCPSClient::PostSend(
		UTTCPSMessage* aMessage)
	{
		mg::network::WriteMessage wmsg;
		aMessage->ToStream(wmsg);
		delete aMessage;
		mg::network::WriteBuffer::Ptr head = wmsg.GetStream().DetachBuffers();
		myByteCountInFly += UTTCPSGetWriteBufferListSize(head.GetPointer());
		return mySocket->PostSend(head.GetPointer());
	}

	void
	UTTCPSClient::Wakeup()
	{
		mySocket->PostWakeup();
	}

	void
	UTTCPSClient::DisableReconnect()
	{
		myLock.Lock();
		myDoReconnect = false;
		myLock.Unlock();
	}

	void
	UTTCPSClient::Shutdown()
	{
		mySocket->PostShutdown();
	}

	void
	UTTCPSClient::CloseBlocking()
	{
		Close();
		WaitClose();
	}

	void
	UTTCPSClient::Close()
	{
		mySocket->PostClose();
	}

	void
	UTTCPSClient::WaitClose()
	{
		bool didTimeOut = false;
		mg::common::MutexLock lock(myLock);
		while (!mySocket->IsClosed() || !myIsClosed)
			myCond.TimedWait(myLock, TEST_YIELD_TIMEOUT, &didTimeOut);
	}

	void
	UTTCPSClient::WaitConnect()
	{
		bool didTimeOut = false;
		mg::common::MutexLock lock(myLock);
		while (!myIsOnConnectCalled)
		{
			MG_COMMON_ASSERT(!myIsClosed);
			MG_COMMON_ASSERT(!mySocket->IsClosed() || myDoReconnect);
			myCond.TimedWait(myLock, TEST_YIELD_TIMEOUT, &didTimeOut);
		}
	}

	void
	UTTCPSClient::WaitIdleFor(
		uint32 aDuration)
	{
		BlockInWorker();
		Unblock();
		uint64 wCount = GetWakeupCount();
		mg::common::Sleep(aDuration);
		// Worker block works via signaling. So it could wakeup
		// one last time after wcount was remembered.
		MG_COMMON_ASSERT(wCount <= GetWakeupCount() + 1);
	}

	void
	UTTCPSClient::WaitWakeupCountInc(
		uint64 aCount)
	{
		bool didTimeOut = false;
		myLock.Lock();
		uint64 target = myWakeupCount + aCount;
		while (target > myWakeupCount)
			myCond.TimedWait(myLock, TEST_YIELD_TIMEOUT, &didTimeOut);
		myLock.Unlock();
	}

	void
	UTTCPSClient::WaitCloseCount(
		uint64 aCount)
	{
		bool didTimeOut = false;
		myLock.Lock();
		while (myCloseCount < aCount)
			myCond.TimedWait(myLock, TEST_YIELD_TIMEOUT, &didTimeOut);
		myLock.Unlock();
	}

	UTTCPSMessage*
	UTTCPSClient::RecvBlocking()
	{
		bool didTimeOut = false;
		mg::common::MutexLock lock(myLock);
		while (myInMessages.IsEmpty())
			myCond.TimedWait(myLock, TEST_YIELD_TIMEOUT, &didTimeOut);
		return myInMessages.PopFirst();
	}

	UTTCPSMessage*
	UTTCPSClient::Recv()
	{
		mg::common::MutexLock lock(myLock);
		if (!myInMessages.IsEmpty())
			return myInMessages.PopFirst();
		return nullptr;
	}

	void
	UTTCPSClient::BlockInWorker()
	{
		bool didTimeOut = false;
		mg::common::MutexLock lock(myLock);
		MG_COMMON_ASSERT(!myIsBlockRequested);
		myIsBlockRequested = true;
		while (!myIsBlocked)
		{
			mySocket->PostWakeup();
			myCond.TimedWait(myLock, TEST_YIELD_TIMEOUT, &didTimeOut);
		}
	}

	void
	UTTCPSClient::Unblock()
	{
		mg::common::MutexLock lock(myLock);
		myIsBlockRequested = false;
		myCond.Broadcast();
	}

	void
	UTTCPSClient::SetInterval(
		int32 aInterval)
	{
		mg::common::MutexLock lock(myLock);
		// Avoid zero interval in the lab as it can't handle busy loops.
		if (aInterval <= 0 && IsLabSlow())
			aInterval = 1;
		if (myWakeupInterval > aInterval)
			mySocket->PostWakeup();
		myWakeupInterval = aInterval;
	}

	void
	UTTCPSClient::ClearInterval()
	{
		mg::common::MutexLock lock(myLock);
		myWakeupInterval = INT32_MAX;
	}

	void
	UTTCPSClient::SetCloseOnError(
		bool aValue)
	{
		mg::common::MutexLock lock(myLock);
		myDoCloseOnError = aValue;
	}

	bool
	UTTCPSClient::IsOnConnectCalled()
	{
		mg::common::MutexLock lock(myLock);
		return myIsOnConnectCalled;
	}

	uint64
	UTTCPSClient::GetWakeupCount()
	{
		mg::common::MutexLock lock(myLock);
		return myWakeupCount;
	}

	mg::common::ErrorCode
	UTTCPSClient::GetError()
	{
		mg::common::MutexLock lock(myLock);
		return myError;
	}

	mg::common::ErrorCode
	UTTCPSClient::GetErrorConnect()
	{
		mg::common::MutexLock lock(myLock);
		return myErrorConnect;
	}

	mg::common::ErrorCode
	UTTCPSClient::GetErrorSend()
	{
		mg::common::MutexLock lock(myLock);
		return myErrorSend;
	}

	mg::common::ErrorCode
	UTTCPSClient::GetErrorRecv()
	{
		mg::common::MutexLock lock(myLock);
		return myErrorRecv;
	}

	void
	UTTCPSClient::OnWakeup()
	{
		myLock.Lock();
		if (myWakeupInterval == 0)
			mySocket->Reschedule();
		else if (myWakeupInterval < INT32_MAX)
			mySocket->SetDeadline(mg::common::GetMilliseconds() + myWakeupInterval);
		++myWakeupCount;
		myCond.Broadcast();
		if (myIsBlockRequested)
		{
			bool didTimeOut = false;
			myIsBlocked = true;
			do
			{
				myCond.TimedWait(myLock, TEST_YIELD_TIMEOUT, &didTimeOut);
			} while (myIsBlockRequested);
			myIsBlocked = false;
		}
		if (myDoConnectInTask)
		{
			myDoConnectInTask = false;
			mySocket->Connect(myConnectParams);
		}

		UTTCPSMessage* head = myOutMessages.PopAll();
		myLock.Unlock();
		UTTCPSMessage* next;

		mg::network::WriteMessage wmsg;
		while (head != nullptr)
		{
			next = head->myNext;
			head->ToStream(wmsg);
			mg::network::WriteBuffer::Ptr wbuf = wmsg.GetStream().DetachBuffers();
			myByteCountInFly += UTTCPSGetWriteBufferListSize(wbuf.GetPointer());
			mySocket->Send(wbuf.GetPointer());
			wmsg.Clear();
			delete head;
			head = next;
		}
	}

	void
	UTTCPSClient::OnSend(
		uint32 aByteCount)
	{
		MG_COMMON_ASSERT(myIsOnConnectCalled);
		MG_COMMON_ASSERT(aByteCount <= myByteCountInFly);
		// Ensure it never reports more sent bytes than it was
		// actually provided. The main intension here is to test
		// that SSL reports number of raw bytes, not encoded
		// bytes.
		myByteCountInFly -= aByteCount;
	}

	void
	UTTCPSClient::OnRecv(
		mg::network::WriteBuffer* aHead,
		mg::network::WriteBuffer* aTail,
		uint32 aByteCount)
	{
		MG_COMMON_ASSERT(myIsOnConnectCalled);
		MG_COMMON_ASSERT(UTTCPSGetWriteBufferListSize(aHead) == aByteCount);
		mg::network::WriteBuffer::Ptr head;
		head.Set(aHead);
		MG_COMMON_ASSERT(!aTail->myNextBuffer.IsSet());
		myStream.AppendBufferList(head);
		mg::network::ReadMessage rmsg(&myStream);
		if (!rmsg.IsMessageComplete())
			return;

		UTTCPSMessageList msgs;
		do
		{
			UTTCPSMessage* msg = new UTTCPSMessage();
			bool ok = rmsg.DecodeHeader();
			ok = ok && rmsg.DecodeDelimiter();
			MG_COMMON_ASSERT(ok);

			msg->FromStream(rmsg);
			PrivOnMessage(msg);
			msgs.Append(msg);
		} while (rmsg.IsMessageComplete());

		mg::common::MutexLock lock(myLock);
		myInMessages.Append(mg::common::Move(msgs));
		myCond.Broadcast();
	}

	void
	UTTCPSClient::OnClose()
	{
		mg::common::MutexLock lock(myLock);
		MG_COMMON_ASSERT(!myIsClosed);
		++myCloseCount;
		if (myDoReconnect)
		{
			myIsOnConnectCalled = false;
			myErrorRecv = mg::common::ERR_NONE;
			myErrorSend = mg::common::ERR_NONE;
			myErrorConnect = mg::common::ERR_NONE;
			myError = mg::common::ERR_NONE;
			GetRegistry().OpenClientSocket(mySocket, myRecvSize);
			myCond.Broadcast();
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
	UTTCPSClient::OnError(
		mg::common::Error* aError)
	{
		mg::common::MutexLock lock(myLock);
		MG_COMMON_ASSERT(myError == 0);
		myError = aError->myCode;
		if (myDoCloseOnError)
			mySocket->PostClose();
	}

	void
	UTTCPSClient::OnConnect()
	{
		mg::common::MutexLock lock(myLock);
		MG_COMMON_ASSERT(!myIsOnConnectCalled);
		myIsOnConnectCalled = true;
		myCond.Broadcast();
	}

	void
	UTTCPSClient::OnConnectError(
		mg::common::Error* aError)
	{
		mg::common::MutexLock lock(myLock);
		MG_COMMON_ASSERT(myErrorConnect == 0);
		myErrorConnect = aError->myCode;
	}

	void
	UTTCPSClient::OnRecvError(
		mg::common::Error* aError)
	{
		mg::common::MutexLock lock(myLock);
		MG_COMMON_ASSERT(myErrorRecv == 0);
		myErrorRecv = aError->myCode;
	}

	void
	UTTCPSClient::OnSendError(
		mg::common::Error* aError)
	{
		mg::common::MutexLock lock(myLock);
		MG_COMMON_ASSERT(myErrorSend == 0);
		myErrorSend = aError->myCode;
	}

	void
	UTTCPSClient::PrivOnMessage(
		UTTCPSMessage* aMsg)
	{
		// The encryption enabling protocol is supposed to happen
		// in a blocking manner. Client requests it, servers
		// responds with a non-encrypted message, and then enables
		// the encryption for the next messages.
		//
		// Otherwise encrypted data could be read by the client or
		// server in the same chunk as previous non-encrypted data
		// and they don't have a way to separate them.
		//
		// This is how HTTP request 'CONNECT' works via a proxy.
		// Client sends raw CONNECT, server responds, then the
		// client enables encryption. And only then starts sending
		// encrypted requests.
		if (aMsg->myDoEnableEncryption)
			((mg::serverbox::SSLSocket*)mySocket)->EnableEncryption();
	}

	//
	// Server which creates new raw sockets in IoCore.
	//

	class UTTCPSAcceptor
		: public mg::serverbox::ITCPAcceptSocketListener
	{
		void OnNewTCPSocket(
			mg::serverbox::Socket aSocket,
			mg::network::Sockaddr* aRemoteAddr,
			mg::serverbox::AcceptHandler* aAcceptHandler) override;

		void OnClosed() override;
	};

	void
	UTTCPSAcceptor::OnNewTCPSocket(
		mg::serverbox::Socket aSocket,
		mg::network::Sockaddr*,
		mg::serverbox::AcceptHandler*)
	{
		new UTTCPSClientHandler(aSocket);
	}

	void
	UTTCPSAcceptor::OnClosed()
	{
	}

	//
	// The tests.
	//

	static void
	UnitTestTCPSocketBaseBasic(
		uint16 aPort)
	{
		TestCaseGuard guard("Basic, %s", (const char*)ScratchConfig().GetBuffer());
		{
			// Basic test to see if works at all.
			UTTCPSClient client;
			client.ConnectBlocking(aPort);
			client.Send(new UTTCPSMessage());
			delete client.RecvBlocking();
			client.CloseBlocking();
		}
		{
			// One buffer for receipt.
			UTTCPSClientParams params;
			params.myRecvSize = 100;
			UTTCPSClient client(params);
			client.ConnectBlocking(aPort);
			client.Send(new UTTCPSMessage());
			delete client.RecvBlocking();
			client.CloseBlocking();
		}
		{
			// Big message sent in multiple WriteBuffers.
			UTTCPSClientParams params;
			params.myRecvSize = 100;
			UTTCPSClient client(params);
			client.ConnectBlocking(aPort);
			const uint32 size = 1024 * 100;
			for (int i = 0; i < 100; ++i)
			{
				UTTCPSMessage* msg = new UTTCPSMessage();
				msg->myPaddingSize = size;
				client.Send(msg);
				msg = client.RecvBlocking();
				MG_COMMON_ASSERT(msg->myPaddingSize == size);
				delete msg;
			}
			client.CloseBlocking();
		}
		{
			// Can delete a socket if it didn't start connecting.
			mg::serverbox::TCPSocketBase* sock = nullptr;
			GetRegistry().OpenClientSocket(sock, TEST_RECV_SIZE);
			sock->Delete();
		}
	}

	static void
	UnitTestTCPSocketBasePostConnect(
		uint16 aPort)
	{
		TestCaseGuard guard("PostConnect, %s", (const char*)ScratchConfig().GetBuffer());

		auto testIsAlive = [=](UTTCPSClient& aClient) {
			aClient.WaitConnect();
			aClient.Send(new UTTCPSMessage());
			delete aClient.RecvBlocking();
			aClient.CloseBlocking();
		};
		{
			// Normal connect to a known local port.
			UTTCPSClient client;
			client.Connect(aPort);
			testIsAlive(client);
		}
		uint32 err;
		mg::serverbox::Socket sock;
		{
			// Connect with an own socket.
			UTTCPSClient client;
			sock = INVALID_SOCKET_VALUE;
			MG_COMMON_ASSERT(mg::serverbox::SocketUtils::CreateSocket(
				AF_INET6, sock, err
			));
			client.Connect(sock, aPort);
			testIsAlive(client);
		}
		{
			// Connect by a domain name.
			UTTCPSClient client;
			client.Connect("localhost", aPort);
			testIsAlive(client);
		}
		{
			// Connect by a domain name with an own socket.
			UTTCPSClient client;
			sock = INVALID_SOCKET_VALUE;
			MG_COMMON_ASSERT(mg::serverbox::SocketUtils::CreateSocket(
				AF_INET, sock, err
			));
			client.Connect(sock, "localhost", aPort);
			testIsAlive(client);
		}
	}

	static void
	UnitTestTCPSocketBasePingPong(
		uint16 aPort,
		uint32 aIterCount,
		uint32 aParallelMsgCount)
	{
		TestCaseGuard guard("Ping pong, %s", (const char*)ScratchConfig().GetBuffer());

		// Many messages sent in parallel.
		for (uint32 i = 0; i < aIterCount; ++i)
		{
			UTTCPSClient client;
			client.Connect(aPort);
			UTTCPSMessage* msg;
			for (uint32 j = 0; j < aParallelMsgCount; ++j)
			{
				msg = new UTTCPSMessage();
				msg->myId = j + i;
				msg->myKey = j + i + 1;
				msg->myValue = j + i + 2;
				msg->myPaddingSize = j + i * 100;
				client.Send(msg);
			}
			for (uint32 j = 0; j < aParallelMsgCount; ++j)
			{
				msg = client.RecvBlocking();
				MG_COMMON_ASSERT(msg->myId == j + i);
				MG_COMMON_ASSERT(msg->myKey == j + i + 1);
				MG_COMMON_ASSERT(msg->myValue == j + i + 2);
				MG_COMMON_ASSERT(msg->myPaddingSize == j + i * 100);
				delete msg;
			}
			client.CloseBlocking();
			MG_COMMON_ASSERT(client.IsOnConnectCalled());
			MG_COMMON_ASSERT(client.GetError() == 0);
			MG_COMMON_ASSERT(client.GetErrorConnect() == 0);
			MG_COMMON_ASSERT(client.GetErrorSend() == 0);
			MG_COMMON_ASSERT(client.GetErrorRecv() == 0);
		}
	}

	static void
	UnitTestTCPSocketBaseCloseDuringSend(
		uint16 aPort)
	{
		TestCaseGuard guard("Close during send, %s", (const char*)ScratchConfig().GetBuffer());

		UTTCPSClient client;
		client.ConnectBlocking(aPort);
		MG_COMMON_ASSERT(client.IsOnConnectCalled());

		client.Send(new UTTCPSMessage());
		client.CloseBlocking();
		MG_COMMON_ASSERT(client.GetErrorConnect() == 0);
		MG_COMMON_ASSERT(client.GetErrorSend() == 0);
		MG_COMMON_ASSERT(client.GetErrorRecv() == 0);
	}

	static void
	UnitTestTCPSocketBaseCloseFromServer(
		uint16 aPort)
	{
		TestCaseGuard guard("Close from server, %s", (const char*)ScratchConfig().GetBuffer());

		UTTCPSClient client;
		client.Connect(aPort);
		UTTCPSMessage* msg = new UTTCPSMessage();
		client.Send(msg);

		msg = new UTTCPSMessage();
		msg->myDoClose = true;
		client.Send(msg);

		client.WaitClose();
		MG_COMMON_ASSERT(client.IsOnConnectCalled());
#if IS_WINDOWS_PLATFORM
		// Graceful remote close on Windows it not considered a
		// success.
		MG_COMMON_ASSERT(client.GetError() != 0);
		MG_COMMON_ASSERT(client.GetErrorRecv() != 0);
#else
		MG_COMMON_ASSERT(client.GetError() == 0);
		MG_COMMON_ASSERT(client.GetErrorRecv() == 0);
#endif
		MG_COMMON_ASSERT(client.GetErrorConnect() == 0);
		MG_COMMON_ASSERT(client.GetErrorSend() == 0);
	}

	static void
	UnitTestTCPSocketBaseCloseOnError(
		uint16 aPort)
	{
		TestCaseGuard guard("Close on error, %s", (const char*)ScratchConfig().GetBuffer());

		UTTCPSClient client;
		client.Connect(aPort);
		client.Send(new UTTCPSMessage());
		delete client.RecvBlocking();

		// The test is against Linux specifically. A bunch of
		// sends + shutdown often generate EPOLLHUP on the socket.
		// There was an issue with how auto-close on EPOLLHUP
		// coexists with manual post-close.
		client.SetCloseOnError(true);
		for (int i = 0; i < 10; ++i)
			client.Send(new UTTCPSMessage());
		client.Shutdown();
		for (int i = 0; i < 10; ++i)
			client.Send(new UTTCPSMessage());

		client.WaitClose();
	}

	static void
	UnitTestTCPSocketBasePostSend(
		uint16 aPort)
	{
		TestCaseGuard guard("PostSend, %s", (const char*)ScratchConfig().GetBuffer());

		UTTCPSClient client;

		client.Connect(aPort);
		// Post to connected or connecting client works.
		MG_COMMON_ASSERT(client.PostSend(new UTTCPSMessage()));
		delete client.RecvBlocking();

		// Multiple sends work fine even if the client is blocked
		// somewhere in an IO worker.
		client.BlockInWorker();
		for (int i = 0; i < 3; ++i)
			MG_COMMON_ASSERT(client.PostSend(new UTTCPSMessage()));
		client.Unblock();
		for (int i = 0; i < 3; ++i)
			delete client.RecvBlocking();

		client.CloseBlocking();
	}

	static void
	UnitTestTCPSocketBaseConnectError()
	{
		TestCaseGuard guard("Connect error, %s", (const char*)ScratchConfig().GetBuffer());

		UTTCPSClient client;
		client.Connect(0);
		client.WaitClose();
		MG_COMMON_ASSERT(!client.IsOnConnectCalled());
		MG_COMMON_ASSERT(client.GetError() != 0);
	}

	static void
	UnitTestTCPSocketBaseShutdown(
		uint16 aPort)
	{
		TestCaseGuard guard("Shutdown, %s", (const char*)ScratchConfig().GetBuffer());
		{
			// Try many connect + shutdown to catch the moment
			// when shutdown is handled together with connect.
			for (int i = 0; i < 10; ++i)
			{
				UTTCPSClient client;
				client.Connect(aPort);
				client.Shutdown();
				client.Send(new UTTCPSMessage());
				client.WaitClose();
				MG_COMMON_ASSERT(client.GetError() != 0);
			}
		}
		{
			// Multiple shutdowns should merge into each other.
			UTTCPSClient client;
			client.Connect(aPort);
			for (int i = 0; i < 10; ++i)
				client.Shutdown();
			client.Send(new UTTCPSMessage());
			client.WaitClose();
			MG_COMMON_ASSERT(client.GetError() != 0);
		}
		{
			// Shutdown right after send.
			UTTCPSClient client;
			client.Connect(aPort);
			client.WaitConnect();
			// Send something to ensure the handshake is done.
			client.Send(new UTTCPSMessage());
			delete client.RecvBlocking();

			client.BlockInWorker();
			client.Send(new UTTCPSMessage());
			client.Shutdown();
			client.Unblock();
			client.WaitClose();
			// It is not checked if there was an error. Because on
			// Linux sometimes the peer notices the shutdown too
			// fast and closes the socket gracefully, so it is not
			// counted as an error.
		}
	}

	static void
	UnitTestTCPSocketBaseDeadline(
		uint16 aPort)
	{
		TestCaseGuard guard("Deadline, %s", (const char*)ScratchConfig().GetBuffer());
		{
			// Wakeup every given interval even though no data
			// to send or receive.
			UTTCPSClient client;
			client.ConnectBlocking(aPort);
			client.SetInterval(1);
			client.WaitWakeupCountInc(50);

			// Change to huge interval. No more wakeups.
			client.SetInterval(1000000);
			client.WaitIdleFor(5);

			// Deadline in the past also works. Like an immediate
			// reschedule.
			client.SetInterval(-1);
			client.WaitWakeupCountInc(50);

			// When not specified, becomes infinity.
			client.ClearInterval();
			client.WaitIdleFor(5);

			// Back to huge interval - no wakeups.
			client.SetInterval(1000000);
			client.WaitIdleFor(5);

			// Reschedule infinitely without a deadline.
			client.SetInterval(0);
			client.WaitWakeupCountInc(5);

			client.CloseBlocking();
		}
	}

	static void
	UnitTestTCPSocketBaseResolve(
		uint16 aPort)
	{
		TestCaseGuard guard("Resolve, %s", (const char*)ScratchConfig().GetBuffer());
		{
			UTTCPSClient client;
			client.ConnectBlocking("127.0.0.1", aPort);
			MG_COMMON_ASSERT(client.IsOnConnectCalled());
			MG_COMMON_ASSERT(client.GetError() == 0);
			client.Send(new UTTCPSMessage());
			delete client.RecvBlocking();
			client.CloseBlocking();
		}
		{
			UTTCPSClient client;
			client.ConnectBlocking("localhost", aPort);
			MG_COMMON_ASSERT(client.IsOnConnectCalled());
			MG_COMMON_ASSERT(client.GetError() == 0);
			client.Send(new UTTCPSMessage());
			delete client.RecvBlocking();
			client.CloseBlocking();
		}
		{
			UTTCPSClient client;
			client.Connect("####### invalid host #######", aPort);
			client.WaitClose();
			MG_COMMON_ASSERT(!client.IsOnConnectCalled());
			MG_COMMON_ASSERT(client.GetError() != 0);
		}
		for (int i = 0; i < 5; ++i)
		{
			UTTCPSClient client;
			client.Connect("localhost", aPort);
			client.CloseBlocking();
		}
		for (int i = 0; i < 50; ++i)
		{
			UTTCPSClient client;
			uint64 wakeupCount = client.GetWakeupCount();
			client.Connect("localhost", aPort);
			// Try to catch the moment when resolve is started but
			// not finished. Close must cancel it.
			uint64 yield = 0;
			while (client.GetWakeupCount() == wakeupCount)
			{
				if (++yield % 10000 == 0)
					mg::common::Sleep(1);
				mg::common::ThreadYield();
			}
			client.CloseBlocking();
		}
		for (int i = 0; i < 5; ++i)
		{
			UTTCPSClient client;
			client.Connect("localhost", aPort);
			client.Shutdown();
			client.CloseBlocking();
		}
		for (int i = 0; i < 5; ++i)
		{
			UTTCPSClient client;
			client.Connect("localhost", aPort);
			client.Send(new UTTCPSMessage());
			delete client.RecvBlocking();
			client.CloseBlocking();
		}
	}

	static void
	UnitTestTCPSocketBaseConnectDelay(
		uint16 aPort)
	{
		TestCaseGuard guard("Connect delay, %s", (const char*)ScratchConfig().GetBuffer());
		{
			// Connect delay with host resolution.
			UTTCPSClient client;
			mg::serverbox::TCPSocketConnectParams params;
			params.myHost = "localhost";
			params.myPort = aPort;
			params.myDelay = 50;
			uint64 start = mg::common::GetMilliseconds();
			client.Connect(params);
			client.WaitConnect();
			uint64 end = mg::common::GetMilliseconds();
			MG_COMMON_ASSERT(end - start >= params.myDelay);

			client.Send(new UTTCPSMessage());
			delete client.RecvBlocking();
			client.CloseBlocking();
		}
		{
			// Connect delay when the host is already known.
			UTTCPSClient client;
			mg::network::Host addr = mg::network::HostMakeLocalIPV6(aPort);

			mg::serverbox::TCPSocketConnectParams params;
			params.myAddr = &addr;
			params.myDelay = 50;
			uint64 start = mg::common::GetMilliseconds();
			client.Connect(params);
			client.WaitConnect();
			uint64 end = mg::common::GetMilliseconds();
			MG_COMMON_ASSERT(end - start >= params.myDelay);

			client.Send(new UTTCPSMessage());
			delete client.RecvBlocking();
			client.CloseBlocking();
		}
	}

	static void
	UnitTestTCPSocketBasePostTask(
		uint16 aPort)
	{
		TestCaseGuard guard("PostTask, %s", (const char*)ScratchConfig().GetBuffer());
		// Ensure the socket can be used as a task and then
		// connected later.
		{
			UTTCPSClient client;
			client.PostTask();
			mg::common::Sleep(10);
			MG_COMMON_ASSERT(client.GetWakeupCount() == 0);
			client.Wakeup();
			while (client.GetWakeupCount() != 1)
				mg::common::Sleep(1);

			client.PostSend(new UTTCPSMessage());
			MG_COMMON_ASSERT(!client.IsOnConnectCalled());
			mg::common::Sleep(10);
			MG_COMMON_ASSERT(client.Recv() == nullptr);
			
			client.SetInterval(1);
			uint64 cnt = client.GetWakeupCount();
			while (client.GetWakeupCount() - cnt < 10)
				mg::common::Sleep(1);

			mg::serverbox::TCPSocketConnectParams params;
			params.myHost = "localhost";
			params.myPort = aPort;
			client.ConnectTask(params);
			client.WaitConnect();
			delete client.RecvBlocking();
			client.CloseBlocking();
		}
		// Stress test for empty tasks closure.
		{
			for (int i = 0; i < 10; ++i)
			{
				UTTCPSClient client;
				client.PostTask();
				client.Wakeup();
				if (i % 2 == 0)
					client.Shutdown();
				client.CloseBlocking();
			}
		}
		// Stress test for in-task connect vs external close.
		{
			mg::serverbox::TCPSocketConnectParams params;
			params.myHost = "localhost";
			params.myPort = aPort;
			for (int i = 0; i < 10; ++i)
			{
				UTTCPSClient client;
				client.PostTask();
				client.ConnectTask(params);
				client.Wakeup();
				client.CloseBlocking();
			}
		}
	}

	static void
	UnitTestTCPSocketBaseReconnect(
		uint16 aPort)
	{
		TestCaseGuard guard("Reconnect, %s", (const char*)ScratchConfig().GetBuffer());
		// Basic reconnect test.
		{
			UTTCPSClientParams params;
			params.myDoReconnect = true;
			UTTCPSClient client(params);
			client.Connect(aPort);
			UTTCPSMessage msgClose;
			msgClose.myId = 1;
			msgClose.myDoClose = true;

			UTTCPSMessage msgCheck;
			msgCheck.myId = 2;
		
			for (int i = 0; i < 10; ++i)
			{
				client.WaitConnect();
				client.Send(new UTTCPSMessage(msgClose));
				client.WaitCloseCount(i + 1);
				client.Send(new UTTCPSMessage(msgCheck));
				UTTCPSMessage* msg = client.RecvBlocking();
				// The server could manage to send the response
				// to close-message into the network. Consume it
				// then too.
				if (msg->myId == msgClose.myId)
				{
					delete msg;
					msg = client.RecvBlocking();
				}
				// The normal message was posted for sending after
				// the closure. Which means it was actually sent
				// only after reconnect happened and should get a
				// guaranteed response.
				MG_COMMON_ASSERT(msg->myId == msgCheck.myId);
				delete msg;
			}
			client.DisableReconnect();
			client.CloseBlocking();
		}
	}

	static void
	UnitTestTCPSocketBaseStressWakeupAndClose(
		uint16 aPort,
		int aIterCount)
	{
		TestCaseGuard guard("Stress wakeup and close, %s", (const char*)ScratchConfig().GetBuffer());

		// Stress test to check how wakeups and closures live
		// together when done multiple times and at the same time.
		const int count = 5;
		const int retryCount = 5;

		for (int i = 0; i < aIterCount; ++i)
		{
			UTTCPSClient clients[count];
			for (int j = 0; j < count; ++j)
			{
				UTTCPSClient* cl = &clients[j];
				cl->Connect(aPort);
				cl->SetInterval(j % 5);
			}
			for (int j = 0; j < count; ++j)
			{
				if (j % 2 == 0)
					clients[j].WaitConnect();
			}
			for (int j = 0; j < count; ++j)
			{
				UTTCPSClient& cli = clients[j];
				if (i % 2 == 0)
					cli.Send(new UTTCPSMessage());
				if (i % 3 == 0)
				{
					cli.Send(new UTTCPSMessage());
					cli.Send(new UTTCPSMessage());
				}
				for (int k = 0; k < retryCount; ++k)
				{
					if (i % 3 == 0)
					{
						cli.Close();
						cli.Wakeup();
						cli.Close();
					}
					else
					{
						cli.Wakeup();
						cli.Close();
						cli.Wakeup();
					}
				}
			}
		}
	}

	static void
	UnitTestTCPSocketBaseStressReconnect(
		uint16 aPort,
		int aIterCount)
	{
		TestCaseGuard guard("Stress reconnect, %s", (const char*)ScratchConfig());

		// Stress test to check any kinds of close + reconnect +
		// wakeup + normal messages together.
		const int count = 10;
		const int retryCount = 5;

		UTTCPSMessage msgClose;
		msgClose.myDoClose = true;

		UTTCPSClientParams params;
		params.myDoReconnect = true;
		mg::common::Array<UTTCPSClient*> clients;
		clients.SetCount(count);
		for (int i = 0; i < count; ++i)
		{
			params.myReconnectDelay = i % 3;
			UTTCPSClient*& cl = clients[i];
			cl = new UTTCPSClient(params);
			cl->Connect(aPort);
			cl->SetInterval(i % 5);
		}
		for (int i = 0; i < aIterCount; ++i)
		{
			for (int j = 0; j < count; ++j)
			{
				if (j % 2 == 0)
					clients[j]->WaitConnect();
			}
			for (int j = 0; j < count; ++j)
			{
				UTTCPSClient& cli = *clients[j];
				if (i % 2 == 0)
					cli.Send(new UTTCPSMessage());
				if (i % 3 == 0)
				{
					cli.Send(new UTTCPSMessage());
					cli.Send(new UTTCPSMessage());
				}
				if (i % 4 == 0)
					cli.Send(new UTTCPSMessage(msgClose));
				for (int k = 0; k < retryCount; ++k)
				{
					if (i % 3 == 0)
					{
						cli.Close();
						cli.Wakeup();
						cli.Close();
					}
					else
					{
						cli.Wakeup();
						cli.Close();
						cli.Wakeup();
					}
				}
				// Don't let the messages stack without limits.
				// Consume whatever has arrived but don't block on
				// it: 1) no guarantees on which responses managed
				// to arrive, 2) otherwise the test wouldn't be
				// stress.
				UTTCPSMessage* msg;
				while ((msg = cli.Recv()) != nullptr)
					delete msg;
			}
		}
		for (int i = 0; i < count; ++i)
		{
			UTTCPSClient*& cl = clients[i];
			cl->DisableReconnect();
			cl->CloseBlocking();
			delete cl;
		}
	}

	static void
	UnitTestSSLSocketEnableEncryption(
		uint16 aPort)
	{
		TestRegistry& reg = GetRegistry();
		TestCaseGuard guard("SSL enable encryption, %s", (const char*)ScratchConfig().GetBuffer());
		{
			reg.CfgIsSSLEnabled(false);
			UTTCPSClient client;
			client.ConnectBlocking(aPort);
			client.SendEnableEncryption();
			delete client.RecvBlocking();
			client.CloseBlocking();
			reg.CfgIsSSLEnabled(true);
		}
		{
			// Ensure the data sent before encryption was enabled
			// stays not encrypted.
			reg.CfgIsSSLEnabled(false);
			UTTCPSClient client;
			client.ConnectBlocking(aPort);
			UTTCPSMessage* msg1 = new UTTCPSMessage();
			msg1->myId = 1;
			UTTCPSMessage* msg2 = new UTTCPSMessage();
			msg2->myId = 2;
			client.Send(msg1);
			client.Send(msg2);

			msg1 = client.RecvBlocking();
			MG_COMMON_ASSERT(msg1->myId == 1);
			delete msg1;

			msg2 = client.RecvBlocking();
			MG_COMMON_ASSERT(msg2->myId == 2);
			delete msg2;

			// Encryption handshake.
			client.SendEnableEncryption();
			delete client.RecvBlocking();

			UTTCPSMessage* msg3 = new UTTCPSMessage();
			msg3->myId = 3;
			client.Send(msg3);
			msg3 = client.RecvBlocking();
			MG_COMMON_ASSERT(msg3->myId == 3);
			delete msg3;

			client.CloseBlocking();
			reg.CfgIsSSLEnabled(true);
		}
	}

	static void
	UnitTestSSLSocketWrongCertificate(
		uint16 aPort)
	{
		TestCaseGuard guard("SSL wrong certificate, %s", (const char*)ScratchConfig().GetBuffer());
		UTTCPSClient client;
		client.Connect(aPort);
		client.WaitClose();
		MG_COMMON_ASSERT(client.GetError() == mg::common::ERR_SSL);
	}

	static void
	UnitTestTCPAppSocketProofOfWorkTimeout()
	{
		TestRegistry& reg = GetRegistry();
		const uint32 powTimeout = 1;
		reg.Reset().CfgApp().CfgProofOfWork(powTimeout);

		TestCaseGuard guard("App proof of work timeout, %s",
			(const char*)reg.ScratchConfig().GetBuffer());

		mg::network::PortRange ports(5000, 10000);
		mg::clientbox::Socket listenSock = INVALID_SOCKET_VALUE;
		uint16 port = 0;
		uint32 err = 0;
		MG_COMMON_ASSERT(mg::clientbox::CreateListenSocket(ports, listenSock, &port,
			err, 1));
		UTTCPSClient client;
		client.Connect(port);
		client.WaitClose();
		MG_COMMON_ASSERT(client.GetError() == mg::common::ERR_POW_TIMEOUT);
		mg::clientbox::CloseSocket(listenSock);
	}

	static void
	UnitTestTCPAppSocketProofOfWorkReceiveInParts(
		uint16 aPort)
	{
		TestRegistry& reg = GetRegistry();
		reg.Reset().CfgApp().CfgProofOfWork();

		TestCaseGuard guard("App proof of work receipt in parts, %s",
			(const char*)reg.ScratchConfig().GetBuffer());

		const uint32 timeout = 1000000;
		mg::clientbox::RawTCPConnection* conn = new mg::clientbox::RawTCPConnection(
			"localhost", aPort, timeout);
		while (!conn->IsConnected())
		{
			conn->Update();
			MG_COMMON_ASSERT(!conn->IsClosed());
			mg::common::Sleep(1);
		}
		//
		// Receive PoW request.
		//
		mg::network::ReadStream stream;
		mg::network::ReadMessage rm(&stream);
		const uint32 bsize = mg::network::WriteBuffer::GetBufferSize();
		mg::network::WriteBuffer::Ptr wb = mg::network::WriteBuffer::NewShared();
		while (!rm.IsMessageComplete())
		{
			wb->myWriteOffset = conn->Recv(wb->myBuffer, bsize);
			if (wb->myWriteOffset > 0)
			{
				stream.AppendBufferList(wb);
				wb = mg::network::WriteBuffer::NewShared();
				continue;
			}
			MG_COMMON_ASSERT(!conn->IsClosed());
			mg::common::Sleep(1);
		}
		MG_COMMON_ASSERT(rm.DecodeHeader());
		MG_COMMON_ASSERT(rm.DecodeDelimiter());
		MG_COMMON_ASSERT(rm.GetDelimiter() == mg::network::PROOF_OF_WORK_DELIM_REQUEST);
		//
		// Make PoW solution.
		//
		mg::network::ProofOfWorkRequest req;
		MG_COMMON_ASSERT(req.FromStream(rm));
		mg::network::ProofOfWorkSolution sol;
		MG_COMMON_ASSERT(mg::network::ProofOfWorkSolve(req, sol));
		//
		// Send PoW solution in 2 parts with a small delay in
		// between. That should test how TCPAppSocket deals with a
		// partially received PoW packet.
		//
		mg::network::WriteMessage wm;
		sol.ToStream(wm);
		wb = wm.GetHeadBuffer();
		MG_COMMON_ASSERT(!wb->myNextBuffer.IsSet() && wb->myWriteOffset > 1);
		conn->Send(wb->myBuffer, 1);
		mg::common::Sleep(10);
		conn->Send(wb->myBuffer + 1, wb->myWriteOffset - 1);
		//
		// If all went fine, the server should respond with a new
		// message.
		//
		wb = mg::network::WriteBuffer::NewShared();
		while (!rm.IsMessageComplete())
		{
			wb->myWriteOffset = conn->Recv(wb->myBuffer, bsize);
			if (wb->myWriteOffset > 0)
			{
				stream.AppendBufferList(wb);
				wb = mg::network::WriteBuffer::NewShared();
				continue;
			}
			MG_COMMON_ASSERT(!conn->IsClosed());
			mg::common::Sleep(1);
		}
		MG_COMMON_ASSERT(rm.DecodeHeader());
		MG_COMMON_ASSERT(rm.DecodeDelimiter());
		MG_COMMON_ASSERT(rm.GetDelimiter() == mg::network::PROOF_OF_WORK_DELIM_RESPONSE);
		conn->Delete();
	}

	static void
	UnitTestTCPAppSocketAndTCPConnection(
		uint16 aPort)
	{
		// Check compatibility of TCPConnection and TCPAppSocket.
		// They need to do same handshake, same order of data pack
		// layers.
		TestRegistry& reg = GetRegistry();
		reg.Reset().CfgApp().CfgProofOfWork().CfgCompression().CfgSSL();
		TestCaseGuard guard("App socket talking with TCPConnection, %s",
			(const char*)reg.ScratchConfig().GetBuffer());

		mg::network::SSLContext::Ptr clientSSL = reg.GetClientSSL();
		mg::clientbox::TCPConnection::Params params;
		params.myCompressionMode = TEST_COMPRESSION;
		params.myConnectTimeout = UINT32_MAX;
		params.myProofOfWorkTimeout = UINT32_MAX;
		params.myProofOfWork = true;
		params.mySSLContext = clientSSL.GetPointer();
		mg::clientbox::TCPConnection* conn = new mg::clientbox::TCPConnection("localhost",
			aPort, 0, params);
		while (!conn->IsConnected())
		{
			conn->Update();
			MG_COMMON_ASSERT(!conn->IsClosed());
			mg::common::Sleep(1);
		}
		MG_COMMON_ASSERT(!conn->IsPoWInProgress());

		mg::network::WriteMessage wmsg;
		UTTCPSMessage outMsg;
		outMsg.ToStream(wmsg);
		conn->Send(wmsg);

		mg::network::ReadMessage rmsg;
		while (!conn->GetNext(rmsg))
		{
			conn->Update();
			MG_COMMON_ASSERT(!conn->IsClosed());
			mg::common::Sleep(1);
		}

		UTTCPSMessage inMsg;
		inMsg.FromStream(rmsg);
		MG_COMMON_ASSERT(inMsg.myId == outMsg.myId);
		MG_COMMON_ASSERT(inMsg.myKey == outMsg.myKey);
		MG_COMMON_ASSERT(inMsg.myValue == outMsg.myValue);
		MG_COMMON_ASSERT(inMsg.myPaddingSize == outMsg.myPaddingSize);

		conn->Delete();
	}

	static void
	UnitTestTCPAppSocketAndMessageHandler()
	{
		// Check compatibility of MessageHandler and TCPAppSocket.
		// They need to do same handshake, same order of data pack
		// layers.
		TestRegistry& reg = GetRegistry();
		reg.Reset().CfgApp().CfgProofOfWork().CfgCompression().CfgSSL();
		TestCaseGuard guard("App socket talking with MessageHandler, %s",
			(const char*)reg.ScratchConfig().GetBuffer());

		mg::network::PortRange ports(5000, 10000);
		mg::network::Host acceptHost;
		mg::clientbox::Socket listenSock = INVALID_SOCKET_VALUE;
		uint16 port = 0;
		uint32 err = 0;
		MG_COMMON_ASSERT(mg::clientbox::CreateListenSocket(ports, listenSock, &port,
			err, 1));

		UTTCPSClient client;
		client.Connect(port);

		mg::clientbox::Socket serverSock = INVALID_SOCKET_VALUE;
		while (!mg::clientbox::AcceptConnectionNonBlocking(listenSock, serverSock,
			acceptHost, err))
		{
			MG_COMMON_ASSERT(err == 0);
			mg::common::Sleep(1);
		}

		TestMessageHandler* handler = reg.NewMessageHandler();
		handler->SetSocket(serverSock, acceptHost);
		serverSock = INVALID_SOCKET_VALUE;

		UTTCPSMessage outMsg;
		client.Send(new UTTCPSMessage(outMsg));
		UTTCPSMessage* inMsg = client.RecvBlocking();
		MG_COMMON_ASSERT(inMsg->myId == outMsg.myId);
		MG_COMMON_ASSERT(inMsg->myKey == outMsg.myKey);
		MG_COMMON_ASSERT(inMsg->myValue == outMsg.myValue);
		MG_COMMON_ASSERT(inMsg->myPaddingSize == outMsg.myPaddingSize);
		delete inMsg;

		handler->Delete();
		mg::clientbox::CloseSocket(listenSock);
	}

	// Common tests which should work regardless of socket type.
	static void
	UnitTestTCPSocketBaseSuite(
		uint16 aPort)
	{
		const bool isSlow = IsLabSlow();
		UnitTestTCPSocketBaseBasic(aPort);
		UnitTestTCPSocketBasePostConnect(aPort);
		if (isSlow)
			UnitTestTCPSocketBasePingPong(aPort, 30, 5);
		else
			UnitTestTCPSocketBasePingPong(aPort, 300, 10);
		UnitTestTCPSocketBaseCloseDuringSend(aPort);
		UnitTestTCPSocketBaseCloseFromServer(aPort);
		UnitTestTCPSocketBaseCloseOnError(aPort);
		UnitTestTCPSocketBasePostSend(aPort);
		UnitTestTCPSocketBaseConnectError();
		UnitTestTCPSocketBaseShutdown(aPort);
		UnitTestTCPSocketBaseDeadline(aPort);
		UnitTestTCPSocketBaseResolve(aPort);
		UnitTestTCPSocketBaseConnectDelay(aPort);
		UnitTestTCPSocketBasePostTask(aPort);
		UnitTestTCPSocketBaseReconnect(aPort);
		if (isSlow)
			UnitTestTCPSocketBaseStressWakeupAndClose(aPort, 1);
		else
			UnitTestTCPSocketBaseStressWakeupAndClose(aPort, 1000);
		if (isSlow)
			UnitTestTCPSocketBaseStressReconnect(aPort, 1);
		else
			UnitTestTCPSocketBaseStressReconnect(aPort, 1000);
	}

	// TCP-specific tests.
	static void
	UnitTestTCPSocketSuite(
		uint16 aPort)
	{
		GetRegistry().Reset();
		UnitTestTCPSocketBaseSuite(aPort);
	}

	// SSL-specific tests.
	static void
	UnitTestSSLSocketSuite(
		uint16 aPort)
	{
		TestRegistry& reg = GetRegistry().Reset().CfgSSL();
		UnitTestTCPSocketBaseSuite(aPort);
		UnitTestSSLSocketEnableEncryption(aPort);

		reg.CfgBadSSL();
		UnitTestSSLSocketWrongCertificate(aPort);
	}

	// App-specific tests.
	static void
	UnitTestAppSocketSuite(
		uint16 aPort)
	{
		TestRegistry& reg = GetRegistry();

		// Plain.
		reg.Reset().CfgApp();
		UnitTestTCPSocketBaseSuite(aPort);

		// SSL.
		reg.Reset().CfgApp().CfgSSL();
		UnitTestTCPSocketBaseSuite(aPort);

		// PoW.
		reg.Reset().CfgApp().CfgProofOfWork();
		UnitTestTCPSocketBaseSuite(aPort);

		// PoW + SSL.
		reg.Reset().CfgApp().CfgProofOfWork().CfgSSL();
		UnitTestTCPSocketBaseSuite(aPort);

		// Compression.
		reg.Reset().CfgApp().CfgCompression();
		UnitTestTCPSocketBaseSuite(aPort);

		// Compression + PoW.
		reg.Reset().CfgApp().CfgProofOfWork().CfgCompression();
		UnitTestTCPSocketBaseSuite(aPort);

		// Compression + SSL.
		reg.Reset().CfgApp().CfgCompression().CfgSSL();
		UnitTestTCPSocketBaseSuite(aPort);

		// Compression + PoW + SSL.
		reg.Reset().CfgApp().CfgProofOfWork().CfgCompression().CfgSSL();
		UnitTestTCPSocketBaseSuite(aPort);

		reg.Reset().CfgApp().CfgProofOfWork().CfgCompression().CfgBadSSL();
		UnitTestSSLSocketWrongCertificate(aPort);

		UnitTestTCPAppSocketProofOfWorkTimeout();
		UnitTestTCPAppSocketProofOfWorkReceiveInParts(aPort);
		UnitTestTCPAppSocketAndTCPConnection(aPort);
		UnitTestTCPAppSocketAndMessageHandler();
	}

	void
	UnitTestTCPSocketBase::Run()
	{
		MG_UNITTESTS_UNITTEST_START();
		printf("\n");

		mg::network::PortRange ports(10000, 50000);
		UTTCPSAcceptor::Ptr acceptor;
		acceptor.Set(new UTTCPSAcceptor());
		mg::serverbox::TCPAcceptSocket* server = new mg::serverbox::TCPAcceptSocket(
			acceptor, nullptr);
		server->Bind(ports);
		server->Listen();
		uint16 port = server->GetListenPort();

		UnitTestTCPSocketSuite(port);
		UnitTestSSLSocketSuite(port);
		UnitTestAppSocketSuite(port);

		server->Close();

		MG_UNITTESTS_UNITTEST_DONE();
	}

} // unittests
} // mg

#undef TEST_COMPRESSION
#undef TEST_RECV_SIZE
