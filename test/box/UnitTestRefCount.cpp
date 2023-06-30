#include "mg/box/RefCount.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace box {

	static void
	UnitTestRefCountBasic()
	{
		TestCaseGuard guard("RefCount basic");
		{
			// Default constructor.
			mg::box::RefCount ref;
			TEST_CHECK(ref.Get() == 1);

			// Last dec returns true.
			TEST_CHECK(ref.Dec());
			TEST_CHECK(ref.Get() == 0);
			ref.Inc();
			ref.Inc();
			TEST_CHECK(!ref.Dec());
			TEST_CHECK(ref.Dec());
		}
		{
			// Non-default constructor.
			mg::box::RefCount ref(3);
			TEST_CHECK(ref.Get() == 3);
			TEST_CHECK(!ref.Dec());
			TEST_CHECK(!ref.Dec());
			TEST_CHECK(ref.Dec());

			ref.Inc();
			TEST_CHECK(ref.Dec());
		}
	}

	void
	UnitTestRefCount()
	{
		TestSuiteGuard suite("RefCount");

		UnitTestRefCountBasic();
	}

}
}
}
