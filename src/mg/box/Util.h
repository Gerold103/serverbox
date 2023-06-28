#pragma once

#include "mg/box/Definitions.h"

#include <cstdarg>
#include <string>

namespace mg {
namespace box {

	MG_STRFORMAT_PRINTF(1, 2)
	std::string StringFormat(
		const char *aFormat,
		...);

	MG_STRFORMAT_PRINTF(1, 0)
	std::string StringVFormat(
		const char *aFormat,
		va_list aParams);

}
}
