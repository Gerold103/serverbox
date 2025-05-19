#include "mg/box/Assert.h"
#include "mg/box/StringFunctions.h"
#include "mg/net/SSLOpenSSL.h"
#include "mg/test/CommandLine.h"

#include "UnitTest.h"

#include <csignal>
#include <cstdio>
#if IS_PLATFORM_WIN
#include <winsock.h>
#endif

namespace mg {
namespace unittests {

	struct TestSettings
	{
		std::string myPattern;
		bool myIsStrictStart;
		bool myIsStrictEnd;
	};

	static TestSettings ParseSettings(
		const mg::tst::CommandLine& aCmd);

	static void RunTest(
		const TestSettings& aSettings,
		void (*aFunc)(),
		const char* aNamespace,
		const char* aName);

namespace aio {
	void UnitTestTCPServer();
	void UnitTestTCPSocketIFace();
}
namespace box {
	void UnitTestAlgorithm();
	void UnitTestAtomic();
	void UnitTestBinaryHeap();
	void UnitTestConditionVariable();
	void UnitTestDoublyList();
	void UnitTestError();
	void UnitTestForwardList();
	void UnitTestInterruptibleMutex();
	void UnitTestIOVec();
	void UnitTestLog();
	void UnitTestMultiConsumerQueue();
	void UnitTestMultiProducerQueue();
	void UnitTestMutex();
	void UnitTestRefCount();
	void UnitTestSharedPtr();
	void UnitTestSignal();
	void UnitTestString();
	void UnitTestSysinfo();
	void UnitTestThreadLocalPool();
	void UnitTestTime();
}
namespace net {
	void UnitTestBuffer();
	void UnitTestDomainToIP();
	void UnitTestHost();
	void UnitTestSSL();
	void UnitTestURL();
}
namespace sch {
	void UnitTestTaskScheduler();
}
namespace sio {
	void UnitTestTCPServer();
	void UnitTestTCPSocket();
}

}
}

int
main(
	int aArgc,
	char** aArgv)
{
	using namespace mg::unittests;

	Report("======== Unit tests ========");

#if IS_PLATFORM_WIN
	WSADATA data;
	MG_BOX_ASSERT(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#else
	// In LLDB: process handle SIGPIPE -n false -p true -s false
	signal(SIGPIPE, SIG_IGN);
#endif

	mg::tst::CommandLine cmd(aArgc, aArgv);
	TestSettings settings = ParseSettings(cmd);

	mg::net::OpenSSLInit();

#define MG_RUN_TEST(nm, func) RunTest(settings, nm::func, #nm, #func)

	MG_RUN_TEST(aio, UnitTestTCPServer);
	MG_RUN_TEST(aio, UnitTestTCPSocketIFace);
	MG_RUN_TEST(box, UnitTestAlgorithm);
	MG_RUN_TEST(box, UnitTestAtomic);
	MG_RUN_TEST(box, UnitTestBinaryHeap);
	MG_RUN_TEST(box, UnitTestConditionVariable);
	MG_RUN_TEST(box, UnitTestDoublyList);
	MG_RUN_TEST(box, UnitTestError);
	MG_RUN_TEST(box, UnitTestForwardList);
	MG_RUN_TEST(box, UnitTestInterruptibleMutex);
	MG_RUN_TEST(box, UnitTestIOVec);
	MG_RUN_TEST(box, UnitTestLog);
	MG_RUN_TEST(box, UnitTestMultiConsumerQueue);
	MG_RUN_TEST(box, UnitTestMultiProducerQueue);
	MG_RUN_TEST(box, UnitTestMutex);
	MG_RUN_TEST(box, UnitTestRefCount);
	MG_RUN_TEST(box, UnitTestSharedPtr);
	MG_RUN_TEST(box, UnitTestSignal);
	MG_RUN_TEST(box, UnitTestString);
	MG_RUN_TEST(box, UnitTestSysinfo);
	MG_RUN_TEST(box, UnitTestThreadLocalPool);
	MG_RUN_TEST(box, UnitTestTime);
	MG_RUN_TEST(net, UnitTestBuffer);
	MG_RUN_TEST(net, UnitTestDomainToIP);
	MG_RUN_TEST(net, UnitTestHost);
	MG_RUN_TEST(net, UnitTestSSL);
	MG_RUN_TEST(net, UnitTestURL);
	MG_RUN_TEST(sch, UnitTestTaskScheduler);
	MG_RUN_TEST(sio, UnitTestTCPServer);
	MG_RUN_TEST(sio, UnitTestTCPSocket);

	mg::net::OpenSSLDestroy();

#if IS_PLATFORM_WIN
	MG_BOX_ASSERT(WSACleanup() == 0);
#endif
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

namespace mg {
namespace unittests {

	static TestSettings
	ParseSettings(
		const mg::tst::CommandLine& aCmd)
	{
		TestSettings res;
		std::string& pattern = res.myPattern;
		res.myIsStrictStart = false;
		res.myIsStrictEnd = false;

		if (aCmd.IsPresent("test"))
			pattern = aCmd.GetStr("test");
		if (pattern.length() > 0)
		{
			if (pattern.front() == '[')
			{
				res.myIsStrictStart = true;
				pattern = pattern.substr(1);
			}
			if (pattern.back() == ']')
			{
				res.myIsStrictEnd = true;
				pattern.pop_back();
			}
		}
		return res;
	}

	static void
	RunTest(
		const TestSettings& aSettings,
		void (*aFunc)(),
		const char* aNamespace,
		const char* aName)
	{
		const char* prefix = "UnitTest";
		uint32_t prefixLen = mg::box::Strlen(prefix);
		MG_BOX_ASSERT(prefixLen < mg::box::Strlen(aName));
		MG_BOX_ASSERT(memcmp(aName, prefix, prefixLen) == 0);
		std::string name = mg::box::StringFormat("%s::%s", aNamespace, aName + prefixLen);

		const char* pattern = aSettings.myPattern.c_str();
		size_t patternLen = aSettings.myPattern.length();
		size_t nameLen = name.length();
		aName = name.c_str();
		if (patternLen > 0)
		{
			const char* pos = mg::box::Strcasestr(aName, pattern);
			if (pos == nullptr)
				return;
			if (aSettings.myIsStrictStart && pos != aName)
				return;
			if (aSettings.myIsStrictEnd && pos + patternLen != aName + nameLen)
				return;
		}
		else if (aSettings.myIsStrictStart && aSettings.myIsStrictEnd)
		{
			return;
		}
		aFunc();
	}

}
}
