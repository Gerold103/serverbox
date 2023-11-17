#include "Assert.h"

#include "mg/box/StringFunctions.h"

#include <cstdio>

namespace mg {
namespace box {

	void
	AssertS(
		const char* aExpression,
		const char* aFile,
		int aLine)
	{
		fprintf(stderr, "assertion failed: (%s) %s %u\n", aExpression, aFile, aLine);
#if IS_PLATFORM_UNIX
		abort();
#else
#if IS_COMPILER_MSVC
#pragma warning(push)
		// "Dereferencing NULL pointer".
#pragma warning(disable: 6011)
#endif
		int* t = NULL;
		*t = 0;
#if IS_COMPILER_MSVC
#pragma warning(pop)
#endif
#endif
	}

	void
	AssertF(
		const char* aExpression,
		const char* aFile,
		int aLine,
		const char* aFormat,
		...)
	{
		std::string expression = mg::box::StringFormat(
			"(%s): ", aExpression);

		va_list	ap;
		va_start(ap, aFormat);
		expression += mg::box::StringVFormat(aFormat, ap);
		va_end(ap);
		AssertS(expression.c_str(), aFile, aLine);
	}

}
}
