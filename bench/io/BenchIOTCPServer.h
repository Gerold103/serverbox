#pragma once

#include "BenchIO.h"

#include "mg/aio/IOCore.h"
#include "mg/aio/TCPServer.h"

namespace mg {
namespace bench {
namespace io {

	class Reporter;

namespace aiotcpsrv {

	struct Settings
	{
		Settings();

		uint32_t myRecvSize;
		std::vector<uint16_t> myPorts;
		uint32_t myThreadCount;
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
		mg::aio::IOCore myCore;
		const Settings mySettings;
	};

}
}
}
}
