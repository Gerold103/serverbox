#include "BenchIOTCPClient.h"
#include "BenchIOTools.h"

#include "mg/aio/TCPSocket.h"
#include "mg/aio/TCPSocketSubscription.h"
#include "mg/test/CommandLine.h"
#include "mg/test/Message.h"
#include "mg/test/Random.h"

#include "Bench.h"

#include <iostream>

namespace mg {
namespace bench {
namespace io {
namespace aiotcpcli {

	class Client
		: public mg::aio::TCPSocketSubscription
	{
	public:
		Client(
			mg::aio::IOCore& aCore,
			Stat& aStat,
			Reporter& aReporter,
			const Settings& aSettings,
			uint16_t aPort)
			: mySocket(new mg::aio::TCPSocket(aCore))
			, myHost(aSettings.myHostNoPort)
			, mySentCount(0)
			, myIsConnected(false)
			, myIsDeleted(false)
			, myStat(aStat)
			, myReporter(aReporter)
			, mySettings(aSettings)
		{
			myHost.SetPort(aPort);
			PrivConnect();
		}

		void
		Delete()
		{
			myMutex.Lock();
			myIsDeleted = true;
			mySocket->PostClose();
			myMutex.Unlock();
		}

	private:
		~Client()
		{
			MG_BOX_ASSERT(myIsDeleted);
			mySocket->Delete();
		}

		void
		OnConnect() override
		{
			myIsConnected = true;
			myReporter.StatAddConnection();
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
				myReporter.StatDelConnection();
			myIsConnected = false;

			myMutex.Lock();
			if (myIsDeleted)
			{
				myMutex.Unlock();
				delete this;
				return;
			}
			PrivConnect();
			myMutex.Unlock();
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

		mg::box::Mutex myMutex;
		mg::aio::TCPSocketIFace* mySocket;
		mg::net::Host myHost;
		uint64_t mySentCount;
		bool myIsConnected;
		bool myIsDeleted;
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
				myClients.push_back(new Client(myCore, myStat, aReporter, mySettings,
					port));
			}
		}
	}

	Instance::~Instance()
	{
		for (Client* cli : myClients)
			cli->Delete();
		myClients.clear();
		myCore.WaitEmpty();
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
