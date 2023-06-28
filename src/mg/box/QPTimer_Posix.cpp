#include "QPTimer.h"

#include "mg/box/Assert.h"

#include <ctime>

namespace mg {
namespace box {

	static uint64_t
	QPTimerGetNs()
	{
		struct timespec ts;
		MG_BOX_ASSERT(clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0);
		return ts.tv_sec * 1000000000 + ts.tv_nsec;
	}

	QPTimer::QPTimer()
		: myStartNs(0)
	{
	}

	void
	QPTimer::Start()
	{
		myStartNs = QPTimerGetNs();
	}

	double
	QPTimer::GetMilliSeconds()
	{
		return (QPTimerGetNs() - myStartNs) / 1000000.0;
	}

}
}
