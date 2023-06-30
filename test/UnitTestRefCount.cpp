#include "mg/box/RefCount.h"

#include "UnitTest.h"

#define TEST_CHECK MG_BOX_ASSERT

namespace mg {
namespace unittests {

	static void
	UnitTestRefCountBasic()
	{
		{
			// Default constructor.
			mg::box::RefCount ref;
			TEST_CHECK(ref.GetValue() == 1);

			// Last dec returns true.
			TEST_CHECK(ref.Dec());
			TEST_CHECK(ref.GetValue() == 0);
			ref.Inc();
			ref.Inc();
			TEST_CHECK(!ref.Dec());
			TEST_CHECK(ref.Dec());
		}
		{
			// Non-default constructor.
			mg::box::RefCount ref(3);
			TEST_CHECK(ref.GetValue() == 3);
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
