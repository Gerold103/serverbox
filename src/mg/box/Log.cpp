#include "Log.h"

namespace mg {
namespace box {

	void
	Log(
		LogLevel aLevel,
		const char* aTag,
		int aLine,
		const char* aFile,
		const char* aFormat,
		...)
	{
		va_list va;
		va_start(va, aFormat);
		LogV(aLevel, aTag, aLine, aFile, aFormat, va);
		va_end(va);
	}

}
}
