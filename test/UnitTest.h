#pragma once

#include "mg/box/Assert.h"
#include "mg/box/Thread.h"

#include <cstdarg>

#if IS_COMPILER_CLANG
// Clang does not put namespaces into __FUNCTION__.
// __PRETTY_FUNCTION__ contains the whole signature, including
// namespaces. Unfortunately, Clang does not feature a macros,
// which would print function name + namespaces without arguments.
#define FUNCTION_NAME_WITH_NAMESPACE __PRETTY_FUNCTION__
#else
#define FUNCTION_NAME_WITH_NAMESPACE __FUNCTION__
#endif

#define TEST_CHECK MG_BOX_ASSERT
#define TEST_YIELD_PERIOD 5
#define TEST_TIMEOUT 300000 // 5 mins

namespace mg {
namespace unittests {

	MG_STRFORMAT_PRINTF(1, 2)
	void Report(
		const char *aFormat,
		...);

	MG_STRFORMAT_PRINTF(1, 0)
	void ReportV(
		const char *aFormat,
		va_list aArg);

	struct TestSuiteGuard
	{
		TestSuiteGuard(
			const char* aName);

		~TestSuiteGuard();

	private:
		double myStartMs;
	};

	struct TestCaseGuard
	{
		MG_STRFORMAT_PRINTF(2, 3)
		TestCaseGuard(
			const char* aFormat,
			...);

		~TestCaseGuard();

	private:
		double myStartMs;
	};

	template <typename T>
	static inline void
	Wait(T&& aCondition, uint32_t aPeriodMs = TEST_YIELD_PERIOD, uint32_t aTimeout = TEST_TIMEOUT)
	{
		uint64_t deadline = mg::box::GetMilliseconds() + aTimeout;
		while (!aCondition())
		{
			TEST_CHECK(mg::box::GetMilliseconds() < deadline);
			if (aPeriodMs != 0)
				mg::box::Sleep(aPeriodMs);
		}
	}

}
}
