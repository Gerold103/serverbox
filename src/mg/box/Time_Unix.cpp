#include "Time.h"

#include "mg/box/Assert.h"

#include <ctime>

namespace mg {
namespace box {

	static inline timespec
	TimeGetTimespec()
	{
		timespec ts;
#if IS_PLATFORM_APPLE
		const clockid_t clockid = CLOCK_MONOTONIC;
#else
		// Boottime is preferable - it takes system suspension time into account.
		const clockid_t clockid = CLOCK_BOOTTIME;
#endif
		MG_BOX_ASSERT(clock_gettime(clockid, &ts) == 0);
		return ts;
	}

	uint64_t
	GetMilliseconds()
	{
		timespec ts = TimeGetTimespec();
		return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	}

	double
	GetMillisecondsPrecise()
	{
		timespec ts = TimeGetTimespec();
		return ts.tv_sec * 1000 + ts.tv_nsec / 1000000.0;
	}

	uint64_t
	GetNanoseconds()
	{
		timespec ts = TimeGetTimespec();
		return ts.tv_sec * 1000000000 + ts.tv_nsec;
	}

}
}
