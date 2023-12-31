#include "Time.h"

static_assert(sizeof(LARGE_INTEGER) == sizeof(uint64_t), "LARGE_INTEGER is 64 bit");

namespace mg {
namespace box {

	static LONGLONG
	TimeCalcFrequency()
	{
		LARGE_INTEGER freq;
		::QueryPerformanceFrequency(&freq);
		return freq.QuadPart;
	}

	static double
	TimeGetUnitsPerNs()
	{
		// Calculate it just once when request it first time.
		static double res = 1000000000.0 / TimeCalcFrequency();
		return res;
	}

	static double
	TimeGetUnitsPerMs()
	{
		// Calculate it just once when request it first time.
		static double res = 1000.0 / TimeCalcFrequency();
		return res;
	}

	uint64_t
	GetMilliseconds()
	{
		return ::GetTickCount64();
	}

	double
	GetMillisecondsPrecise()
	{
		LARGE_INTEGER ts;
		::QueryPerformanceCounter(&ts);
		return ts.QuadPart * TimeGetUnitsPerMs();
	}

	uint64_t
	GetNanoseconds()
	{
		LARGE_INTEGER ts;
		::QueryPerformanceCounter(&ts);
		return (uint64_t)(ts.QuadPart * TimeGetUnitsPerNs());
	}

}
}
