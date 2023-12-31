#pragma once

#include "mg/box/Definitions.h"

namespace mg {
namespace box {

	// Returns the number of milliseconds since some point in the
	// past. Monotonic, won't go back on clock changes.
	uint64_t GetMilliseconds();

	// Same but with higher precision.
	double GetMillisecondsPrecise();

	// Same but with nanoseconds precision (might be less precise, depending on platform).
	uint64_t GetNanoseconds();

}
}
