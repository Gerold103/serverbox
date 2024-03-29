#pragma once

#include "mg/box/StringFunctions.h"
#include "mg/test/CommandLine.h"

#define MG_BENCH_FALSE_SHARING_BARRIER(name) \
	MG_UNUSED_MEMBER char name[128]

namespace mg {
namespace bench {

	MG_STRFORMAT_PRINTF(1, 2)
	void Report(
		const char *aFormat,
		...);

	MG_STRFORMAT_PRINTF(1, 0)
	void ReportV(
		const char *aFormat,
		va_list aArg);

	struct BenchSuiteGuard
	{
		BenchSuiteGuard(
			const char* aName);
	};

	struct BenchCaseGuard
	{
		MG_STRFORMAT_PRINTF(2, 3)
		BenchCaseGuard(
			const char* aFormat,
			...);
	};

	struct TimedGuard
	{
		MG_STRFORMAT_PRINTF(2, 3)
		TimedGuard(
			const char* aFormat,
			...);

		void Stop();

		void Report();

		double GetMilliseconds();

	private:
		double myStartMs;
		std::string myName;
		bool myIsStopped;
		double myDuration;
	};

	enum BenchLoadType
	{
		BENCH_LOAD_EMPTY,
		BENCH_LOAD_NANO,
		BENCH_LOAD_MICRO,
		BENCH_LOAD_HEAVY,
	};

	const char* BenchLoadTypeToString(
		BenchLoadType aVal);

	BenchLoadType BenchLoadTypeFromString(
		const char* aVal);

	static inline BenchLoadType
	BenchLoadTypeFromString(
		const std::string& aVal)
	{
		return BenchLoadTypeFromString(aVal.c_str());
	}

	void BenchMakeNanoWork();

	void BenchMakeMicroWork();

	void BenchMakeHeavyWork();

}
}
