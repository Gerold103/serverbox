#include "mg/box/Log.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace box {
namespace log {

	static void
	UnitTestLogBasic()
	{
		TestCaseGuard guard("Basic");

		MG_LOG_ERROR("test.01", "abc");
		MG_LOG_ERROR("test.02", "abc %d %s", 1, "2");

		MG_LOG_WARN("test.03", "abc");
		MG_LOG_WARN("test.04", "abc %d %s", 1, "2");

		MG_LOG_INFO("test.05", "abc");
		MG_LOG_INFO("test.06", "abc %d %s", 1, "2");

		MG_LOG_DEBUG("test.07", "abc");
		MG_LOG_DEBUG("test.08", "abc %d %s", 1, "2");

		mg::box::Log(mg::box::LOG_LEVEL_ERROR, "test.09", 123, "file",
			"abc %d %s", 1, "2");
		mg::box::Log(mg::box::LOG_LEVEL_WARN, "test.10", 123, "file",
			"abc %d %s", 1, "2");
		mg::box::Log(mg::box::LOG_LEVEL_INFO, "test.11", 123, "file",
			"abc %d %s", 1, "2");
		mg::box::Log(mg::box::LOG_LEVEL_DEBUG, "test.12", 123, "file",
			"abc %d %s", 1, "2");
	}
}

	void
	UnitTestLog()
	{
		using namespace log;
		TestSuiteGuard suite("Log");

		UnitTestLogBasic();
	}

}
}
}
