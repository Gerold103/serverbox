#pragma once

#include "mg/box/Definitions.h"

namespace mg {
namespace test {

	uint32_t RandomUInt32();

	uint32_t RandomUniformUInt32(
		uint32_t aMin,
		uint32_t aMax);

	bool RandomBool();
}
}
