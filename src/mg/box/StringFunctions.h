#pragma once

#include "mg/box/Definitions.h"

#include <cstdarg>
#include <string>

namespace mg {
namespace box {

	static inline uint32_t
	Strlen(
		const char* aString)
	{
		return (uint32_t)strlen(aString);
	}

	static inline int
	Strcmp(
		const char* aA,
		const char* aB)
	{
		return strcmp(aA, aB);
	}

	static inline int
	Strncmp(
		const char* aA,
		const char* aB,
		uint32_t aCount)
	{
		return strncmp(aA, aB, aCount);
	}

	static inline char*
	Strdup(
		const char* aStr)
	{
#if IS_PLATFORM_WIN
		return _strdup(aStr);
#else
		return strdup(aStr);
#endif
	}

	static inline int
	Strcasecmp(
		const char* aA,
		const char* aB)
	{
#if IS_PLATFORM_WIN
		return _stricmp(aA, aB);
#else
		return strcasecmp(aA, aB);
#endif
	}

	const char*
	Strcasestr(
		const char* aString,
		const char* aToFind);

	void StringTrim(
		std::string& aStr);

	MG_STRFORMAT_PRINTF(2, 0)
	uint32_t Vsprintf(
		char* aBuffer,
		const char* aFmtString,
		va_list aArgList);

	MG_STRFORMAT_PRINTF(3, 0)
	uint32_t Vsnprintf(
		char* aBuffer,
		uint32_t aBufferSize,
		const char* aFmtString,
		va_list aArgList);

	MG_STRFORMAT_PRINTF(1, 2)
	std::string StringFormat(
		const char *aFormat,
		...);

	MG_STRFORMAT_PRINTF(1, 0)
	std::string StringVFormat(
		const char *aFormat,
		va_list aParams);

	bool StringToNumber(
		const char* aString,
		uint16_t& aOutNumber);

	bool StringToNumber(
		const char* aString,
		uint32_t& aOutNumber);

	bool StringToNumber(
		const char* aString,
		uint64_t& aOutNumber);

}
}
