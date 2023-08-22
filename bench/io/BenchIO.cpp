#include "Bench.h"

#include "BenchIOTCPClient.h"
#include "BenchIOTCPServer.h"
#include "BenchIOTools.h"
#include "mg/net/DomainToIP.h"
#include "mg/test/CommandLine.h"

#if MG_BENCH_IO_HAS_BOOST
#include "BenchIOTCPClientBoost.h"
#endif

#include <csignal>

namespace mg {
namespace bench {
namespace io {

	void BenchIOShowTutorial();
	void BenchIOShowHelp();

}
}
}

int
main(
	int aArgc,
	char** aArgv)
{
	using namespace mg::bench::io;
	mg::tst::CommandLine cmdLine(aArgc, aArgv);

#if !IS_PLATFORM_WIN
	// In LLDB: process handle SIGPIPE -n false -p true -s false
	signal(SIGPIPE, SIG_IGN);
#endif
	if (cmdLine.IsPresent("help"))
	{
		mg::bench::io::BenchIOShowHelp();
		return 0;
	}
	if (cmdLine.IsPresent("tutorial"))
	{
		mg::bench::io::BenchIOShowTutorial();
		return 0;
	}
	if (cmdLine.IsPresent("test"))
	{
#if !MG_BENCH_IO_HAS_BOOST
		std::string backend = "mg_aio";
		cmdLine.GetStr("backend", backend);
		if (backend == "boost_asio")
		{
			mg::bench::Report("'boost_asio' backend is not supported");
			return 1;
		}
#endif
		mg::bench::Report("All requested features are supported");
		return 0;
	}
	//////////////////////////////////////////////////////////////////////////////////////
	std::string portsStr = "12345";
	cmdLine.GetStr("ports", portsStr);
	portsStr += ',';
	std::vector<uint16_t> ports;
	size_t pos;
	while ((pos = portsStr.find(',')) != std::string::npos)
	{
		std::string portStr = portsStr.substr(0, pos);
		if (portStr.empty())
			continue;
		portsStr = portsStr.substr(pos + 1);
		uint16_t port;
		MG_BOX_ASSERT(mg::box::StringToNumber(portStr.c_str(), port) &&
			"Couldn't parse a port");
		ports.push_back(port);
	}
	MG_BOX_ASSERT(!ports.empty() && "Couldn't parse the ports");
	//////////////////////////////////////////////////////////////////////////////////////
	std::string modeStr = "online";
	cmdLine.GetStr("report", modeStr);
	ReportMode reportMode = REPORT_MODE_ONLINE;
	if (modeStr == "online")
	{
		reportMode = REPORT_MODE_ONLINE;
	}
	else if (modeStr == "summary")
	{
		reportMode = REPORT_MODE_SUMMARY;
	}
	else
	{
		MG_BOX_ASSERT_F(false, "Unknown report mode '%s'", modeStr.c_str());
		return 1;
	}
	Reporter reporter;
	reporter.Start(reportMode);
	//////////////////////////////////////////////////////////////////////////////////////
	Instance* instance = nullptr;
	const std::string& benchMode = cmdLine.GetStr("mode");
	if (benchMode == "server")
	{
		aiotcpsrv::Settings settings;
		cmdLine.GetU32("recv_size", settings.myRecvSize);
		settings.myPorts = ports;
		cmdLine.GetU32("thread_count", settings.myThreadCount);
		instance = new aiotcpsrv::Instance(settings, reporter);
	}
	else if (benchMode == "client")
	{
		std::string hostStr = "127.0.0.1";
		cmdLine.GetStr("target_host", hostStr);
		mg::box::Error::Ptr err;
		std::vector<mg::net::DomainEndpoint> endpoints;
		bool ok = mg::net::DomainToIPBlocking(hostStr, mg::box::TimeDuration(5000),
			endpoints, err);
		MG_BOX_ASSERT_F(ok, "Couldn't resolve the host name '%s': %s", hostStr.c_str(),
			err->myMessage.c_str());

		std::string backend = "mg_aio";
		cmdLine.GetStr("backend", backend);
		if (backend == "mg_aio")
		{
			aiotcpcli::Settings settings;
			cmdLine.GetU32("recv_size", settings.myRecvSize);
			cmdLine.GetU32("message_parallel_count", settings.myMsgParallel);
			cmdLine.GetU32("message_int_count", settings.myIntCount);
			cmdLine.GetU32("message_payload_size", settings.myPayloadSize);
			cmdLine.GetU32("disconnect_period", settings.myDisconnectPeriod);
			settings.myPorts = ports;
			cmdLine.GetU32("connect_count_per_port", settings.myClientsPerPort);
			cmdLine.GetU32("thread_count", settings.myThreadCount);
			cmdLine.GetU64("message_target_count", settings.myTargetMessageCount);
			settings.myHostNoPort = endpoints[0].myHost;

			instance = new aiotcpcli::Instance(settings, reporter);
		}
		else if (backend == "boost_asio")
		{
#if MG_BENCH_IO_HAS_BOOST
			bsttcpcli::Settings settings;
			cmdLine.GetU32("recv_size", settings.myRecvSize);
			cmdLine.GetU32("message_parallel_count", settings.myMsgParallel);
			cmdLine.GetU32("message_int_count", settings.myIntCount);
			cmdLine.GetU32("message_payload_size", settings.myPayloadSize);
			cmdLine.GetU32("disconnect_period", settings.myDisconnectPeriod);
			settings.myPorts = ports;
			cmdLine.GetU32("connect_count_per_port", settings.myClientsPerPort);
			cmdLine.GetU32("thread_count", settings.myThreadCount);
			cmdLine.GetU64("message_target_count", settings.myTargetMessageCount);
			settings.myHostNoPort = endpoints[0].myHost;

			instance = new bsttcpcli::Instance(settings, reporter);
#else
			MG_BOX_ASSERT(!"'boost' is not supported");
			return 1;
#endif
		}
		else
		{
			MG_BOX_ASSERT_F(false, "Unknown client backend '%s'", backend.c_str());
			return 1;
		}
	}
	else
	{
		MG_BOX_ASSERT_F(false, "Unknown bench mode '%s'", benchMode.c_str());
		return 1;
	}
	//////////////////////////////////////////////////////////////////////////////////////
	while (!instance->IsFinished())
		mg::box::Sleep(10);
	reporter.Stop();
	delete instance;
	reporter.Print();
	return 0;
}
