#include "UnitTest.h"

#include "mg/box/Assert.h"
#include "mg/box/Util.h"

namespace mg {
namespace unittests {

	static void
	UnitTestUtilFormat()
	{
		MG_BOX_ASSERT(mg::box::StringFormat("") == "");
		MG_BOX_ASSERT(mg::box::StringFormat("abc") == "abc");
		MG_BOX_ASSERT(mg::box::StringFormat(
			"a %d %s b c %u", 1, "str", 2U) == "a 1 str b c 2");
	}

	void
	UnitTestUtil()
	{
		TestSuiteGuard suite("Util");

		UnitTestUtilFormat();
	}

}
}
