#include "UnitTest.h"

#include "mg/box/Time.h"

#include <cstdio>

namespace mg {
namespace unittests {

	MG_STRFORMAT_PRINTF(1, 2)
	void
	Report(
		const char *aFormat,
		...)
	{
		va_list va;
		va_start(va, aFormat);
		ReportV(aFormat, va);
		va_end(va);
	}

	MG_STRFORMAT_PRINTF(1, 0)
	void
	ReportV(
		const char *aFormat,
		va_list aArg)
	{
		vprintf(aFormat, aArg);
		printf("\n");
	}

	TestSuiteGuard::TestSuiteGuard(
		const char* aName)
		: myStartMs(mg::box::GetMillisecondsPrecise())
	{
		Report("======== [%s] start", aName);
	}

	TestSuiteGuard::~TestSuiteGuard()
	{
		Report("======== took %.6lf ms", mg::box::GetMillisecondsPrecise() - myStartMs);
	}

	MG_STRFORMAT_PRINTF(2, 3)
	TestCaseGuard::TestCaseGuard(
		const char* aFormat,
		...)
		: myStartMs(mg::box::GetMillisecondsPrecise())
	{
		va_list va;
		va_start(va, aFormat);
		ReportV(aFormat, va);
		va_end(va);
	}

	TestCaseGuard::~TestCaseGuard()
	{
		Report("took %.6lf ms", mg::box::GetMillisecondsPrecise() - myStartMs);
	}

}
}
