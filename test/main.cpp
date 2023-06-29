#include "mg/box/Assert.h"
#include "mg/box/StringFunctions.h"
#include "mg/test/CommandLine.h"

#include "UnitTest.h"

#include <cstdio>

namespace mg {
namespace unittests {

	struct TestSettings
	{
		std::string myPattern;
		bool myIsStrictStart;
		bool myIsStrictEnd;
	};

	static TestSettings ParseSettings(
		const mg::test::CommandLine& aCmd);

	static void RunTest(
		const TestSettings& aSettings,
		void (*aFunc)(),
		const char* aName);

	void UnitTestAtomic();
	void UnitTestBinaryHeap();
	void UnitTestConditionVariable();
	void UnitTestForwardList();
	void UnitTestMultiConsumerQueue();
	void UnitTestMultiProducerQueue();
	void UnitTestMutex();
	void UnitTestSignal();
	void UnitTestString();
	void UnitTestTaskScheduler();
	void UnitTestUtil();

}
}

int
main(
	int aArgc,
	char** aArgv)
{
	using namespace mg::unittests;

	Report("======== Unit tests ========");

	mg::test::CommandLine cmd(aArgc, aArgv);
	TestSettings settings = ParseSettings(cmd);

#define MG_RUN_TEST(func) RunTest(settings, func, #func)

	MG_RUN_TEST(UnitTestAtomic);
	MG_RUN_TEST(UnitTestBinaryHeap);
	MG_RUN_TEST(UnitTestConditionVariable);
	MG_RUN_TEST(UnitTestForwardList);
	MG_RUN_TEST(UnitTestMultiConsumerQueue);
	MG_RUN_TEST(UnitTestMultiProducerQueue);
	MG_RUN_TEST(UnitTestMutex);
	MG_RUN_TEST(UnitTestSignal);
	MG_RUN_TEST(UnitTestString);
	MG_RUN_TEST(UnitTestTaskScheduler);
	MG_RUN_TEST(UnitTestUtil);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

namespace mg {
namespace unittests {

	static TestSettings
	ParseSettings(
		const mg::test::CommandLine& aCmd)
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
		const char* aName)
	{
		uint32_t nameLen = mg::box::Strlen(aName);
		const char* prefix = "UnitTest";
		uint32_t prefixLen = mg::box::Strlen(prefix);
		MG_BOX_ASSERT(prefixLen < nameLen);
		MG_BOX_ASSERT(memcmp(aName, prefix, prefixLen) == 0);
		aName += prefixLen;
		nameLen -= prefixLen;
		const std::string& pattern = aSettings.myPattern;
		if (pattern.length() > 0)
		{
			const char* pos = mg::box::Strcasestr(aName, pattern.c_str());
			if (pos == nullptr)
				return;
			if (aSettings.myIsStrictStart && pos != aName)
				return;
			if (aSettings.myIsStrictEnd && pos + pattern.length() != aName + nameLen)
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
