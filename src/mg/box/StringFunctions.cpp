#include "StringFunctions.h"

#include "mg/box/Assert.h"

#include <cctype>
#include <cstdio>

namespace mg {
namespace box {

	const char*
	Strcasestr(
		const char* aString,
		const char* aToFind)
	{
#if IS_PLATFORM_WIN
		char c, sc;
		if ((c = *aToFind++) != 0) {
			c = (char)tolower((unsigned char)c);
			size_t len = strlen(aToFind);
			do {
				do {
					if ((sc = *aString++) == 0)
						return (NULL);
				} while ((char)tolower((unsigned char)sc) != c);
			} while (_strnicmp(aString, aToFind, len) != 0);
			aString--;
		}
		return ((char*)aString);
#else
		return strcasestr(aString, aToFind);
#endif
	}

	void
	StringTrim(
		std::string& aStr)
	{
		size_t len = aStr.length();
		while (true)
		{
			if (len == 0)
			{
				aStr.clear();
				return;
			}
			if (!isspace(aStr[len - 1]))
			{
				aStr.resize(len);
				break;
			}
			--len;
		}
		size_t toCut = 0;
		while (toCut <= len && isspace(aStr[toCut]))
			++toCut;
		aStr.erase(0, toCut);
	}

	uint32_t
	Vsnprintf(
		char* aBuffer,
		uint32_t aBufferSize,
		const char* aFmtString,
		va_list aArgList)
	{
		int rc = vsnprintf(aBuffer, aBufferSize, aFmtString, aArgList);
		MG_DEV_ASSERT(rc >= 0);
		return (uint32_t)rc;
	}

	std::string
	StringFormat(
		const char *aFormat,
		...)
	{
		va_list va;
		va_start(va, aFormat);
		std::string res = StringVFormat(aFormat, va);
		va_end(va);
		return res;
	}

	std::string
	StringVFormat(
		const char *aFormat,
		va_list aParams)
	{
		va_list va;
		va_copy(va, aParams);
		int size = vsnprintf(nullptr, 0, aFormat, va);
		MG_DEV_ASSERT(size >= 0);
		va_end(va);
		++size;
		char* data = new char[size];
		va_copy(va, aParams);
		int size2 = vsnprintf(data, size, aFormat, va);
		MG_DEV_ASSERT(size2 + 1 == size);
		va_end(va);
		std::string res(data);
		delete[] data;
		return res;
	}

	bool
	StringToNumber(
		const char* aString,
		uint64_t& aOutNumber)
	{
		// Need to check for minus manually, because strtoull()
		// applies negation to the result. Despite the fact it
		// returns an unsigned type. Result is garbage.
		while (isspace(*aString))
			++aString;
		if (*aString == '-')
			return false;
		char* end;
		errno = 0;
		aOutNumber = strtoull(aString, &end, 10);
		return errno == 0 && *aString != 0 && *end == 0;
	}

	template<typename ResT, ResT aMax>
	static bool
	StringToUnsignedNumberTemplate(
		const char* aString,
		ResT& aOutNumber)
	{
		uint64_t res;
		if (StringToNumber(aString, res) && res <= aMax)
		{
			aOutNumber = (ResT)res;
			return true;
		}
		return false;
	}

	bool
	StringToNumber(
		const char* aString,
		uint16_t& aOutNumber)
	{
		return StringToUnsignedNumberTemplate<uint16_t, UINT16_MAX>(aString, aOutNumber);
	}

	bool
	StringToNumber(
		const char* aString,
		uint32_t& aOutNumber)
	{
		return StringToUnsignedNumberTemplate<uint32_t, UINT32_MAX>(aString, aOutNumber);
	}

}
}
