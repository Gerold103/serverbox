#pragma once

#include "mg/box/Definitions.h"

namespace mg {
namespace box {

	class QPTimer
	{
	public:
		QPTimer();

		void Start();

		double GetMilliSeconds();

	private:
#if IS_PLATFORM_POSIX
		uint64_t myStartNs;
#else
		uint64_t myStartQuant;
#endif
	};

}
}
