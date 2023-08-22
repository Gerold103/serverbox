#include "BenchIOTools.h"

#include "mg/aio/TCPServer.h"
#include "mg/aio/TCPSocket.h"
#include "mg/aio/TCPSocketSubscription.h"
#include "mg/test/CommandLine.h"
#include "mg/test/Message.h"

namespace mg {
namespace bench {

	struct BenchIOTCPServerSettings
	{
		uint32_t myRecvSize;
	};

	class BenchIOTCPPeer
		: public mg::aio::TCPSocketSubscription
	{
	public:
		BenchIOTCPPeer(
			mg::aio::IOCore& aCore,
			const BenchIOTCPServerSettings& aSettings,
			mg::net::Socket aSock)
			: mySocket(nullptr)
			, myRecvSize(aSettings.myRecvSize)
		{
			BenchIOMetricsAddConnection();
			mg::aio::TCPSocketParams sockParams;
			mySocket = new mg::aio::TCPSocket(aCore);
			((mg::aio::TCPSocket*)mySocket)->Open(sockParams);
			mySocket->PostRecv(myRecvSize);
			mySocket->PostWrap(aSock, this);
		}

	private:
		~BenchIOTCPPeer() = default;

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
				BenchIOMetricsAddMessage();
				BenchIOEncodeMessage(wmsg, msg);
			}
			mySocket->SendRef(wmsg.TakeData());
			mySocket->Recv(myRecvSize);
		}

		void
		OnClose() override
		{
			BenchIOMetricsDropConnection();
			mySocket->Delete();
			delete this;
		}

		mg::aio::TCPSocketIFace* mySocket;
		const uint64_t myRecvSize;
	};

	class BenchIOTCPServerSub
		: public mg::aio::TCPServerSubscription
	{
	public:
		BenchIOTCPServerSub(
			const BenchIOTCPServerSettings& aSettings,
			mg::aio::IOCore& aCore)
			: mySettings(aSettings)
			, myCore(aCore)
		{
		}

	private:
		~BenchIOTCPServerSub() = default;

		void
		OnAccept(
			mg::net::Socket aSock,
			const mg::net::Host&) override
		{
			new BenchIOTCPPeer(myCore, mySettings, aSock);
		}

		void
		OnClose() override
		{
			MG_BOX_ASSERT(!"Server closed unexpectedly");
		}

		const BenchIOTCPServerSettings& mySettings;
		mg::aio::IOCore& myCore;
	};

	int
	BenchIOStartTCPServer(
		mg::tst::CommandLine& aCmd,
		mg::aio::IOCore& aCore)
	{
		uint32_t portCount = 1;
		uint32_t startPort = 12345;
		uint32_t recvSize = 8192;

		aCmd.GetU32("port_count", portCount);
		aCmd.GetU32("start_port", startPort);
		aCmd.GetU32("recv_size", recvSize);

		BenchIOTCPServerSettings* settings = new BenchIOTCPServerSettings();
		settings->myRecvSize = recvSize;

		BenchIOTCPServerSub* sub = new BenchIOTCPServerSub(*settings, aCore);
		for (uint32_t i = 0; i < portCount; ++i)
		{
			mg::box::Error::Ptr err;
			mg::aio::TCPServer::Ptr server = mg::aio::TCPServer::NewShared(aCore);
			MG_BOX_ASSERT(server->Bind(mg::net::HostMakeAllIPV4(startPort + i), err));
			MG_BOX_ASSERT(server->Listen(mg::net::theMaxBacklog, sub, err));
			server.Unwrap();
		}
		return true;
	}

}
}
