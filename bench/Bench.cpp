#include "Bench.h"

#include "mg/box/Assert.h"
#include "mg/box/Atomic.h"
#include "mg/box/StringFunctions.h"

#include "mg/test/Random.h"

#include <ctype.h>
#include <stdio.h>

namespace mg {
namespace bench {

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
		std::string buf = mg::box::StringVFormat(aFormat, aArg);
		printf("%s\n", buf.c_str());
	}

	BenchSuiteGuard::BenchSuiteGuard(
		const char* aName)
	{
		Report("======== [%s] suite", aName);
	}

	MG_STRFORMAT_PRINTF(2, 3)
	BenchCaseGuard::BenchCaseGuard(
		const char* aFormat,
		...)
	{
		va_list va;
		va_start(va, aFormat);
		std::string buf = mg::box::StringVFormat(aFormat, va);
		va_end(va);
		Report("==== [%s] case", buf.c_str());
	}

	MG_STRFORMAT_PRINTF(2, 3)
	TimedGuard::TimedGuard(
		const char* aFormat,
		...)
		: myIsStopped(false)
		, myDuration(0)
	{
		va_list va;
		va_start(va, aFormat);
		myName = mg::box::StringVFormat(aFormat, va);
		va_end(va);
		myTimer.Start();
	}

	void
	TimedGuard::Stop()
	{
		MG_BOX_ASSERT(!myIsStopped);
		myDuration = myTimer.GetMilliSeconds();
		myIsStopped = true;
	}

	void
	TimedGuard::Report()
	{
		MG_BOX_ASSERT(myIsStopped);
		mg::bench::Report("== [%s] took %.6lf ms",
			myName.c_str(), myDuration);
	}

	double
	TimedGuard::GetMilliseconds()
	{
		MG_BOX_ASSERT(myIsStopped);
		return myTimer.GetMilliSeconds();
	}

	const char*
	BenchLoadTypeToString(
		BenchLoadType aVal)
	{
		switch (aVal)
		{
		case BENCH_LOAD_EMPTY: return "empty";
		case BENCH_LOAD_NANO: return "nano";
		case BENCH_LOAD_MICRO: return "micro";
		case BENCH_LOAD_HEAVY: return "heavy";
		default: MG_BOX_ASSERT(false); return nullptr;
		}
	}

	BenchLoadType
	BenchLoadTypeFromString(
		const char* aVal)
	{
		if (mg::box::Strcmp(aVal, "empty") == 0)
			return BENCH_LOAD_EMPTY;
		if (mg::box::Strcmp(aVal, "nano") == 0)
			return BENCH_LOAD_NANO;
		if (mg::box::Strcmp(aVal, "micro") == 0)
			return BENCH_LOAD_MICRO;
		if (mg::box::Strcmp(aVal, "heavy") == 0)
			return BENCH_LOAD_HEAVY;
		MG_BOX_ASSERT_F(false, "Uknown load type '%s'", aVal);
		return BENCH_LOAD_EMPTY;
	}

	static inline void
	BenchMakeWork(
		int aCount)
	{
		for (int i = 0; i < aCount; ++i)
		{
			mg::box::AtomicBool flag(mg::test::RandomBool());
			flag.Store(true);
		}
	}

	void
	BenchMakeNanoWork()
	{
		BenchMakeWork(20);
	}

	void
	BenchMakeMicroWork()
	{
		BenchMakeWork(100);
	}

	void
	BenchMakeHeavyWork()
	{
		BenchMakeWork(500);
	}

}
}
