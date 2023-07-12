#include "mg/box/Assert.h"
#include "mg/box/Log.h"

#include <cstdio>

//
// The file implements missing functions from mg::box in some trivial way to get the tests
// compile.
//
// A real project should not link with the stub-lib and should define its own functions.
//

namespace mg {
namespace box {

	static const char*
	LogLevelToStringWithPadding(
		LogLevel aLevel)
	{
		switch(aLevel)
		{
		case LOG_LEVEL_ERROR:
			return "ERROR";
		case LOG_LEVEL_WARN:
			return " WARN";
		case LOG_LEVEL_INFO:
			return " INFO";
		case LOG_LEVEL_DEBUG:
			return "DEBUG";
		default:
			MG_BOX_ASSERT(!"Unknown log level");
			return nullptr;
		}
	}

	void
	LogV(
		LogLevel aLevel,
		const char* aTag,
		int /* aLine */,
		const char* /* aFile */,
		const char* aFormat,
		va_list aParams)
	{
		printf("%s (%s): ", LogLevelToStringWithPadding(aLevel), aTag);
		vprintf(aFormat, aParams);
		printf("\n");
	}

}
}
