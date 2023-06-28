#pragma once

#include "mg/box/Definitions.h"

#define MG_BOX_ASSERT_F(X, ...) do {														\
	if (!(X))																				\
		mg::box::AssertF(#X, __FILE__, __LINE__, __VA_ARGS__);								\
} while(false)

#define MG_BOX_ASSERT(X) do {																\
	if (!(X))																				\
		mg::box::AssertS(#X, __FILE__, __LINE__);											\
} while(false)

#if IS_BUILD_DEBUG
#define MG_DEV_ASSERT MG_BOX_ASSERT
#define MG_DEV_ASSERT_F MG_BOX_ASSERT_F
#else
#define MG_DEV_ASSERT MG_UNUSED
#define MG_DEV_ASSERT_F MG_UNUSED
#endif

namespace mg {
namespace box {

	void AssertS(
		const char* aExpression,
		const char* aFile,
		int aLine);

	MG_STRFORMAT_PRINTF(4, 5)
	void AssertF(
		const char* aExpression,
		const char* aFile,
		int aLine,
		const char* aFormat,
		...);

}
}
