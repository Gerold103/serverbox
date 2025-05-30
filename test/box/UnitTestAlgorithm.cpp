#include "mg/box/Algorithm.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace box {

	static void
	UnitTestMinMax()
	{
		TEST_CHECK(mg::box::Min(0, 0) == 0);
		TEST_CHECK(mg::box::Min(1, 0) == 0);
		TEST_CHECK(mg::box::Min(0, 1) == 0);

		TEST_CHECK(mg::box::Max(0, 0) == 0);
		TEST_CHECK(mg::box::Max(1, 0) == 1);
		TEST_CHECK(mg::box::Max(0, 1) == 1);

		int a = 0;
		constexpr int b = 0;
		TEST_CHECK(mg::box::Min(a, b) == 0);
		TEST_CHECK(mg::box::Max(a, b) == 0);
	}

	void
	UnitTestAlgorithm()
	{
		TestSuiteGuard suite("Algorithm");

		UnitTestMinMax();
	}

}
}
}
