#include "Bench.h"

#include "BenchIOBoostCore.h"
#include "BenchIOTools.h"
#include "mg/aio/IOCore.h"
#include "mg/test/CommandLine.h"

#include <csignal>

namespace mg {
namespace bench {

	bool BenchIOStartTCPClient(
		mg::tst::CommandLine& aCmd,
		mg::aio::IOCore& aCore);
	bool BenchIOStartTCPClientBoost(
		mg::tst::CommandLine& aCmd,
		BoostIOCore& aCore);
	bool BenchIOStartTCPServer(
		mg::tst::CommandLine& aCmd,
		mg::aio::IOCore& aCore);

	void BenchIOShowTutorial();
	void BenchIOShowHelp();

}
}

int
main(
	int aArgc,
	char** aArgv)
{
	mg::tst::CommandLine cmdLine(aArgc, aArgv);

#if !IS_PLATFORM_WIN
	// In LLDB: process handle SIGPIPE -n false -p true -s false
	signal(SIGPIPE, SIG_IGN);
#endif
	if (cmdLine.IsPresent("help"))
	{
		mg::bench::BenchIOShowHelp();
		return 0;
	}
	if (cmdLine.IsPresent("tutorial"))
	{
		mg::bench::BenchIOShowTutorial();
		return 0;
	}
	const char* mode = cmdLine.GetStr("mode");

	uint32_t ioThreadCount = MG_IOCORE_DEFAULT_THREAD_COUNT;
	cmdLine.GetU32("io_threads", ioThreadCount);

	uint32_t startPort = 12345;
	cmdLine.GetU32("start_port", startPort);

	const char* backend = "mg_aio";
	cmdLine.GetStr("backend", backend);

	bool isServer = false;
	if (mg::box::Strcmp(mode, "client") == 0)
	{
		isServer = false;
	}
	else if (mg::box::Strcmp(mode, "server") == 0)
	{
		isServer = true;
	}
	else
	{
		mg::bench::Report("Unknown operation mode '%s'", mode);
		return 1;
	}

	bool ok = false;
	if (mg::box::Strcmp(backend, "mg_aio") == 0)
	{
		mg::aio::IOCore& core = *(new mg::aio::IOCore());
		core.Start(ioThreadCount);
		if (isServer)
			ok = mg::bench::BenchIOStartTCPServer(cmdLine, core);
		else
			ok = mg::bench::BenchIOStartTCPClient(cmdLine, core);
	}
	else if (mg::box::Strcmp(backend, "boost_asio") == 0)
	{
		mg::bench::BoostIOCore& core = *(new mg::bench::BoostIOCore());
		core.Start(ioThreadCount);
		if (isServer)
		{
			mg::bench::Report("Unsupported operation mode '%s' for this backend '%s'",
				mode, backend);
			return 1;
		}
		ok = mg::bench::BenchIOStartTCPClientBoost(cmdLine, core);
	}
	else
	{
		mg::bench::Report("Unknown backend '%s'", backend);
		return 1;
	}
	if (!ok)
		return 1;
	mg::bench::BenchIORunMetrics();
	MG_BOX_ASSERT(!"Unreachable");
	return 0;
}
