#pragma once

#include "mg/box/Definitions.h"

namespace mg {
namespace box {

	// Returns the number of milliseconds since some point in the
	// past. Monotonic, won't go back on clock changes.
	uint64_t GetMilliseconds();

}
}
