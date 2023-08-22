#include "BenchIOTools.h"

#if MG_BENCH_IO_HAS_BOOST

#include "mg/box/ThreadFunc.h"
#include "mg/net/DomainToIP.h"
#include "mg/test/CommandLine.h"
#include "mg/test/Message.h"
#include "mg/test/Random.h"

#include "Bench.h"
#include "BenchIOBoostCore.h"

#include <boost/asio.hpp>
#include <deque>

#else // MG_BENCH_IO_HAS_BOOST

#include "Bench.h"
#include "BenchIOBoostCore.h"

#endif // MG_BENCH_IO_HAS_BOOST

namespace mg {
namespace bench {

#if MG_BENCH_IO_HAS_BOOST
namespace io_tcp_client_boost {

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
	{
	public:
		BenchIOTCPClient(
			BoostIOCore& aCore,
			const BenchIOTCPClientSettings& aSettings,
			uint16_t aPort)
			: mySocket(aCore.GetCtx())
			, myStrand(aCore.GetCtx().get_executor())
			, myHost(aSettings.myHostNoPort)
			, mySentCount(0)
			, myIsConnected(false)
			, myIsSendInProgress(false)
			, myIsRecvInProgress(false)
			, mySettings(aSettings)
		{
			myHost.SetPort(aPort);
			PrivConnect();
		}

	private:
		void
		OnConnect(const boost::system::error_code& aError)
		{
			if (aError)
			{
				PrivReconnect();
				return;
			}
			myIsConnected = true;
			BenchIOMetricsAddConnection();
			mg::tst::WriteMessage wmsg;
			Message msg;
			msg.myIntCount = mySettings.myIntCount;
			msg.myPayloadSize = mySettings.myPayloadSize;
			uint32_t msgParralel = mySettings.myMsgParallel;
			for (uint32_t i = 0; i < msgParralel; ++i)
			{
				msg.myTimestamp = mg::box::GetMilliseconds();
				msg.myIntValue = mg::tst::RandomUInt32();
				BenchIOEncodeMessage(wmsg, msg);
			}
			mySendQueue.AppendRef(wmsg.TakeData());
			PrivSend();
			PrivRecv();
		}

		void
		OnSend(
			const boost::system::error_code& aError,
			std::size_t aSize)
		{
			MG_DEV_ASSERT(myIsSendInProgress);
			myIsSendInProgress = false;
			if (aError)
			{
				PrivReconnect();
				return;
			}
			uint32_t offset = 0;
			mySendQueue.SkipData(offset, aSize);
			MG_BOX_ASSERT(offset == 0);
			PrivSend();
		}

		void
		OnRecv(
			const boost::system::error_code& aError,
			std::size_t aSize)
		{
			MG_DEV_ASSERT(myIsRecvInProgress);
			myIsRecvInProgress = false;
			if (aError)
			{
				PrivReconnect();
				return;
			}
			myRecvQueue.PropagateWritePos(aSize);
			mg::net::BufferReadStream stream(myRecvQueue);
			mg::tst::ReadMessage rmsg(stream);
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
			mySendQueue.AppendRef(wmsg.TakeData());
			PrivSend();
			PrivRecv();
			if (doClose)
				PrivReconnect();
		}

		void
		PrivConnect()
		{
			boost::asio::ip::tcp::endpoint endpoint(
				boost::asio::ip::address::from_string(myHost.ToStringNoPort()),
				myHost.GetPort());
			mySocket.async_connect(endpoint, boost::asio::bind_executor(myStrand,
				std::bind(&BenchIOTCPClient::OnConnect, this, std::placeholders::_1)));
		}

		void
		PrivReconnect()
		{
			BenchIOMetricsDropConnection();
			mySocket.close();
			myIsConnected = false;
			mySendQueue.Clear();
			myRecvQueue.Clear();
			PrivConnect();
		}

		void
		PrivSend()
		{
			if (!myIsConnected)
				return;
			if (mySendQueue.IsEmpty())
				return;
			if (myIsSendInProgress)
				return;
			myIsSendInProgress = true;
			std::deque<boost::asio::const_buffer> bufs;
			const mg::net::BufferLink* link = mySendQueue.GetFirst();
			while (link != nullptr)
			{
				const mg::net::Buffer* b = link->myHead.GetPointer();
				while (b != nullptr)
				{
					bufs.push_back(boost::asio::const_buffer(b->myRData, b->myPos));
					b = b->myNext.GetPointer();
				}
				link = link->myNext;
			}
			boost::asio::async_write(mySocket, std::move(bufs),
				boost::asio::bind_executor(myStrand, std::bind(
					&BenchIOTCPClient::OnSend, this, std::placeholders::_1,
					std::placeholders::_2)));
		}

		void
		PrivRecv()
		{
			if (!myIsConnected)
				return;
			if (myIsRecvInProgress)
				return;
			myIsRecvInProgress = true;
			myRecvQueue.EnsureWriteSize(mySettings.myRecvSize);
			std::deque<boost::asio::mutable_buffer> bufs;
			mg::net::Buffer* b = myRecvQueue.GetWritePos();
			while (b != nullptr)
			{
				bufs.push_back(boost::asio::mutable_buffer(
					b->myWData + b->myPos, b->myCapacity - b->myPos));
				b = b->myNext.GetPointer();
			}
			mySocket.async_read_some(std::move(bufs),
				boost::asio::bind_executor(myStrand, std::bind(
					&BenchIOTCPClient::OnRecv, this, std::placeholders::_1,
					std::placeholders::_2)));
		}

		boost::asio::ip::tcp::socket mySocket;
		boost::asio::strand<boost::asio::io_context::executor_type> myStrand;
		mg::net::BufferLinkList mySendQueue;
		mg::net::BufferStream myRecvQueue;
		mg::net::Host myHost;
		uint64_t mySentCount;
		bool myIsConnected;
		bool myIsSendInProgress;
		bool myIsRecvInProgress;
		const BenchIOTCPClientSettings& mySettings;
	};

}
#endif // MG_BENCH_IO_HAS_BOOST

	bool
	BenchIOStartTCPClientBoost(
		mg::tst::CommandLine& aCmd,
		BoostIOCore& aCore)
	{
#if MG_BENCH_IO_HAS_BOOST
		using namespace mg::bench::io_tcp_client_boost;

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
#else
		MG_UNUSED(aCmd, aCore);
		Report("Boost is not supported in this build");
		return false;
#endif
	}

}
}
