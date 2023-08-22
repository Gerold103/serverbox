#include "BenchIOTools.h"

#include "mg/aio/TCPSocket.h"
#include "mg/aio/TCPSocketSubscription.h"
#include "mg/net/DomainToIP.h"
#include "mg/test/CommandLine.h"
#include "mg/test/Message.h"
#include "mg/test/Random.h"

#include "Bench.h"

#include <iostream>

namespace mg {
namespace bench {
namespace io_tcp_client_aio {

	struct BenchIOTCPClientSettings
	{
		uint32_t myRecvSize;
		uint32_t myMsgParallel;
		uint32_t myIntCount;
		uint32_t myPayloadSize;
		uint32_t myDisconnectPeriod;
		mg::net::Host myHostNoPort;
	};

	class BenchIOTCPClient
		: public mg::aio::TCPSocketSubscription
	{
	public:
		BenchIOTCPClient(
			mg::aio::IOCore& aCore,
			BenchIOTCPClientSettings& aSettings,
			uint16_t aPort)
			: mySocket(new mg::aio::TCPSocket(aCore))
			, myHost(aSettings.myHostNoPort)
			, mySentCount(0)
			, myIsConnected(false)
			, mySettings(aSettings)
		{
			myHost.SetPort(aPort);
			PrivConnect();
		}

		~BenchIOTCPClient()
		{
			mySocket->Delete();
		}

	private:
		void
		OnConnect() override
		{
			myIsConnected = true;
			BenchIOMetricsAddConnection();
			mg::tst::WriteMessage wmsg;
			Message msg;
			msg.myIntCount = mySettings.myIntCount;
			msg.myPayloadSize = mySettings.myPayloadSize;
			msg.myTimestamp = mg::box::GetMilliseconds();
			uint32_t msgParralel = mySettings.myMsgParallel;
			for (uint32_t i = 0; i < msgParralel; ++i)
			{
				msg.myIntValue = mg::tst::RandomUInt32();
				BenchIOEncodeMessage(wmsg, msg);
			}
			mySocket->SendRef(wmsg.TakeData());
			mySocket->Recv(mySettings.myRecvSize);
		}

		void
		OnRecv(
			mg::net::BufferReadStream& aStream) override
		{
			mg::tst::ReadMessage rmsg(aStream);
			mg::tst::WriteMessage wmsg;
			Message msg;

			uint64_t curMsec = mg::box::GetMilliseconds();
			bool doClose = false;
			while (rmsg.IsComplete())
			{
				BenchIODecodeMessage(rmsg, msg);
				BenchIOMetricsAddMessage();
				MG_BOX_ASSERT(msg.myTimestamp <= curMsec);
				BenchIOMetricsAddLatency(curMsec - msg.myTimestamp);

				msg.myTimestamp = curMsec;
				msg.myIntValue = mg::tst::RandomUInt32();
				BenchIOEncodeMessage(wmsg, msg);

				if (mySettings.myDisconnectPeriod != 0 &&
					mySentCount++ % mySettings.myDisconnectPeriod == 0)
				{
					doClose = true;
					break;
				}
			}
			mySocket->SendRef(wmsg.TakeData());
			if (doClose)
				mySocket->PostClose();
			else
				mySocket->Recv(mySettings.myRecvSize);
		}

		void
		OnClose() override
		{
			if (myIsConnected)
				BenchIOMetricsDropConnection();
			myIsConnected = false;
			PrivConnect();
		}

		void
		PrivConnect()
		{
			mg::aio::TCPSocketParams sockParams;
			((mg::aio::TCPSocket*)mySocket)->Open(sockParams);

			mg::aio::TCPSocketConnectParams connParams;
			connParams.myEndpoint = myHost.ToString();
			mySocket->PostConnect(connParams, this);
		}

		mg::aio::TCPSocketIFace* mySocket;
		mg::net::Host myHost;
		uint64_t mySentCount;
		bool myIsConnected;
		BenchIOTCPClientSettings& mySettings;
	};

}

	bool
	BenchIOStartTCPClient(
		mg::tst::CommandLine& aCmd,
		mg::aio::IOCore& aCore)
	{
		using namespace mg::bench::io_tcp_client_aio;

		uint32_t connectCountPerPort = 1;
		uint32_t disconnectPeriod = 0;
		const char* domain = "127.0.0.1";
		uint32_t messagePayloadSize = 1024;
		uint32_t messageIntCount = 0;
		uint32_t messageParallelCount = 1;
		uint32_t portCount = 1;
		uint32_t startPort = 12345;
		uint32_t recvSize = 8192;

		aCmd.GetU32("connect_count_per_port", connectCountPerPort);
		aCmd.GetU32("disconnect_period", disconnectPeriod);
		aCmd.GetStr("host", domain);
		aCmd.GetU32("message_payload_size", messagePayloadSize);
		aCmd.GetU32("message_int_count", messageIntCount);
		aCmd.GetU32("message_parallel_count", messageParallelCount);
		aCmd.GetU32("port_count", portCount);
		aCmd.GetU32("start_port", startPort);
		aCmd.GetU32("recv_size", recvSize);

		mg::box::Error::Ptr err;
		std::vector<mg::net::DomainEndpoint> endpoints;
		if (!mg::net::DomainToIPBlocking(domain, mg::box::TimeDuration(5000),
			endpoints, err))
		{
			Report("Couldn't resolve the host name '%s': %s", domain,
				err->myMessage.c_str());
			return false;
		}

		BenchIOTCPClientSettings* settings = new BenchIOTCPClientSettings();
		settings->myDisconnectPeriod = disconnectPeriod;
		settings->myHostNoPort = endpoints.front().myHost;
		settings->myIntCount = messageIntCount;
		settings->myMsgParallel = messageParallelCount;
		settings->myPayloadSize = messagePayloadSize;
		settings->myRecvSize = recvSize;

		for (uint32_t port = startPort, endPort = port + portCount;
			port < endPort; ++port)
		{
			for (uint32_t i = 0; i < connectCountPerPort; ++i)
				new BenchIOTCPClient(aCore, *settings, port);
		}
		return true;
	}

}
}
