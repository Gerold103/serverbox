#include "BenchIOTCPServer.h"
#include "BenchIOTools.h"

#include "mg/aio/TCPServer.h"
#include "mg/aio/TCPSocket.h"
#include "mg/aio/TCPSocketSubscription.h"
#include "mg/test/CommandLine.h"
#include "mg/test/Message.h"

namespace mg {
namespace bench {
namespace io {
namespace aiotcpsrv {

	class Peer
		: public mg::aio::TCPSocketSubscription
	{
	public:
		Peer(
			mg::aio::IOCore& aCore,
			Reporter& aReporter,
			const Settings& aSettings,
			mg::net::Socket aSock)
			: mySocket(nullptr)
			, myRecvSize(aSettings.myRecvSize)
			, myReporter(aReporter)
		{
			myReporter.StatAddConnection();
			mg::aio::TCPSocketParams sockParams;
			mySocket = new mg::aio::TCPSocket(aCore);
			((mg::aio::TCPSocket*)mySocket)->Open(sockParams);
			mySocket->PostRecv(myRecvSize);
			mySocket->PostWrap(aSock, this);
		}

		void
		Delete();

	private:
		~Peer() = default;

		void
		OnRecv(
			mg::net::BufferReadStream& aStream) override
		{
			mg::tst::ReadMessage rmsg(aStream);
			mg::tst::WriteMessage wmsg;
			Message msg;
			while (rmsg.IsComplete())
			{
				BenchIODecodeMessage(rmsg, msg);
				myReporter.StatAddMessage();
				BenchIOEncodeMessage(wmsg, msg);
			}
			mySocket->SendRef(wmsg.TakeData());
			mySocket->Recv(myRecvSize);
		}

		void
		OnClose() override
		{
			myReporter.StatDelConnection();
			mySocket->Delete();
			delete this;
		}

		mg::aio::TCPSocketIFace* mySocket;
		const uint64_t myRecvSize;
		Reporter& myReporter;
	};

	class ServerSub final
		: public mg::aio::TCPServerSubscription
	{
	public:
		ServerSub(
			const Settings& aSettings,
			mg::aio::IOCore& aCore,
			Reporter& aReporter)
			: mySettings(aSettings)
			, myCore(aCore)
			, myReporter(aReporter)
		{
		}

	private:
		~ServerSub() = default;

		void
		OnAccept(
			mg::net::Socket aSock,
			const mg::net::Host&) final
		{
			new Peer(myCore, myReporter, mySettings, aSock);
		}

		void
		OnClose() final
		{
			MG_BOX_ASSERT(!"Unreachable");
		}

		const Settings& mySettings;
		mg::aio::IOCore& myCore;
		Reporter& myReporter;
	};

	Settings::Settings()
		: myRecvSize(8192)
		, myThreadCount(MG_IOCORE_DEFAULT_THREAD_COUNT)
	{
	}

	Instance::Instance(
		const Settings& aSettings,
		Reporter& aReporter)
		: mySettings(aSettings)
	{
		myCore.Start(aSettings.myThreadCount);
		ServerSub* sub = new ServerSub(mySettings, myCore, aReporter);
		for (uint16_t port : mySettings.myPorts)
		{
			mg::box::Error::Ptr err;
			mg::aio::TCPServer::Ptr server = mg::aio::TCPServer::NewShared(myCore);
			MG_BOX_ASSERT(server->Bind(mg::net::HostMakeAllIPV4(port), err));
			MG_BOX_ASSERT(server->Listen(mg::net::SocketMaxBacklog(), sub, err));
			server.Unwrap();
		}
	}

	Instance::~Instance()
	{
		MG_BOX_ASSERT(!"Unreachable");
	}

	bool
	Instance::IsFinished() const
	{
		return false;
	}

}
}
}
}
