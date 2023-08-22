#pragma once

#include "BenchIO.h"

#include "mg/aio/IOCore.h"

namespace mg {
namespace bench {
namespace io {

	class Reporter;

namespace aiotcpcli {

	class Client;

	struct Settings
	{
		Settings();

		uint32_t myRecvSize;
		uint32_t myMsgParallel;
		uint32_t myIntCount;
		uint32_t myPayloadSize;
		uint32_t myDisconnectPeriod;
		std::vector<uint16_t> myPorts;
		uint32_t myClientsPerPort;
		uint32_t myThreadCount;
		uint64_t myTargetMessageCount;
		mg::net::Host myHostNoPort;
	};

	struct Stat
	{
		Stat();

		mg::box::AtomicU64 myMessageCount;
	};

	class Instance final : public mg::bench::io::Instance
	{
	public:
		Instance(
			const Settings& aSettings,
			Reporter& aReporter);

		~Instance() final;

		bool IsFinished() const final;

	private:
		std::vector<Client*> myClients;
		mg::aio::IOCore myCore;
		Stat myStat;
		const Settings mySettings;
	};

}
}
}
}
