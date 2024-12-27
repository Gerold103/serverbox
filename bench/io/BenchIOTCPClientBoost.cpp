#include "BenchIOTCPClientBoost.h"

#include "Bench.h"
#include "BenchIOTools.h"
#include "mg/aio/IOCore.h"
#include "mg/box/ThreadFunc.h"
#include "mg/test/CommandLine.h"
#include "mg/test/Message.h"
#include "mg/test/Random.h"

#include <boost/asio.hpp>
#include <deque>
#include <memory>

namespace mg {
namespace bench {
namespace io {
namespace bsttcpcli {

	class Client final : public std::enable_shared_from_this<Client>
	{
	public:
		Client(
			BoostIOCore& aCore,
			Stat& aStat,
			Reporter& aReporter,
			const Settings& aSettings,
			uint16_t aPort)
			: mySocket(aCore.GetCtx())
			, myStrand(aCore.GetCtx().get_executor())
			, myHost(aSettings.myHostNoPort)
			, mySentCount(0)
			, myIsConnected(false)
			, myIsSendInProgress(false)
			, myIsRecvInProgress(false)
			, myStat(aStat)
			, myReporter(aReporter)
			, mySettings(aSettings)
		{
			myHost.SetPort(aPort);
		}

		void
		Start()
		{
			PrivConnect();
		}

	private:
		void
		OnConnect(
			const boost::system::error_code& aError)
		{
			if (aError)
			{
				PrivReconnect();
				return;
			}
			myIsConnected = true;
			myReporter.StatAddConnection();
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
				MG_BOX_ASSERT(msg.myTimestamp <= curMsec);
				myStat.myMessageCount.IncrementRelaxed();
				myReporter.StatAddMessage();
				myReporter.StatAddLatency(curMsec - msg.myTimestamp);

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
				boost::asio::ip::make_address(myHost.ToStringNoPort()),
				myHost.GetPort());
			mySocket.async_connect(endpoint,
				boost::asio::bind_executor(myStrand, std::bind(&Client::OnConnect,
					shared_from_this(), std::placeholders::_1)));
		}

		void
		PrivReconnect()
		{
			myReporter.StatDelConnection();
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
				boost::asio::bind_executor(myStrand, std::bind(&Client::OnSend,
					shared_from_this(), std::placeholders::_1, std::placeholders::_2)));
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
				boost::asio::bind_executor(myStrand, std::bind(&Client::OnRecv,
					shared_from_this(), std::placeholders::_1, std::placeholders::_2)));
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
		Stat& myStat;
		Reporter& myReporter;
		const Settings& mySettings;
	};

	Settings::Settings()
		: myRecvSize(8192)
		, myMsgParallel(1)
		, myIntCount(0)
		, myPayloadSize(1024)
		, myDisconnectPeriod(0)
		, myClientsPerPort(1)
		, myThreadCount(MG_IOCORE_DEFAULT_THREAD_COUNT)
		, myTargetMessageCount(UINT64_MAX)
		, myHostNoPort(mg::net::HostMakeLocalIPV4(0))
	{
	}

	Stat::Stat()
		: myMessageCount(0)
	{
	}

	Instance::Instance(
		const Settings& aSettings,
		Reporter& aReporter)
		: mySettings(aSettings)
	{
		myCore.Start(mySettings.myThreadCount);
		for (uint16_t port : mySettings.myPorts)
		{
			for (uint32_t i = 0; i < mySettings.myClientsPerPort; ++i)
			{
				std::make_shared<Client>(myCore, myStat, aReporter, mySettings,
					port)->Start();
			}
		}
	}

	Instance::~Instance()
	{
		myCore.Stop();
	}

	bool
	Instance::IsFinished() const
	{
		return myStat.myMessageCount.LoadRelaxed() >= mySettings.myTargetMessageCount;
	}

}
}
}
}
