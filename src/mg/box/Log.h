#pragma once

#include "mg/box/Definitions.h"

#include <cstdarg>

#define MG_LOG(level, tag, format, ...) do {												\
	mg::box::Log((level), (tag), __LINE__, __FILE__, (format), ##__VA_ARGS__);				\
} while (false)

#define MG_LOG_ERROR(...)	MG_LOG(mg::box::LOG_LEVEL_ERROR, __VA_ARGS__)
#define MG_LOG_WARN(...)	MG_LOG(mg::box::LOG_LEVEL_WARN, __VA_ARGS__)
#define MG_LOG_INFO(...)	MG_LOG(mg::box::LOG_LEVEL_INFO, __VA_ARGS__)
#define MG_LOG_DEBUG(...)	MG_LOG(mg::box::LOG_LEVEL_DEBUG, __VA_ARGS__)

namespace mg {
namespace box {

	enum LogLevel
	{
		LOG_LEVEL_ERROR = 1,
		LOG_LEVEL_WARN = 2,
		LOG_LEVEL_INFO = 3,
		LOG_LEVEL_DEBUG = 4,
	};

	MG_STRFORMAT_PRINTF(5, 6)
	void Log(
		LogLevel aLevel,
		const char* aTag,
		int aLine,
		const char* aFile,
		const char* aFormat,
		...);

	MG_STRFORMAT_PRINTF(5, 0)
	void LogV(
		LogLevel aLevel,
		const char* aTag,
		int aLine,
		const char* aFile,
		const char* aFormat,
		va_list aParams);

}
}
