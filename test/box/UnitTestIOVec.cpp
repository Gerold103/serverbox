#include "mg/box/IOVec.h"

#include "mg/box/Assert.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace box {
namespace iovec {

	static void
	UnitTestIOVecPropagate()
	{
		TestCaseGuard guard("Propagate");
		mg::box::IOVec vecs[4];
		mg::box::IOVec* pos = nullptr;
		uint32_t count = 0;

		auto fillVecs = [&]() {
			vecs[0].myData = (void*)"123";
			vecs[0].mySize = 3;
			vecs[1].myData = (void*)"4567";
			vecs[1].mySize = 4;
			vecs[2].myData = nullptr;
			vecs[2].mySize = 0;
			vecs[3].myData = (void*)"89";
			vecs[3].mySize = 2;
			pos = vecs;
			count = 4;
		};

		fillVecs();
		mg::box::IOVecPropagate(pos, count, 0);
		TEST_CHECK(count == 4);
		TEST_CHECK(pos == vecs);
		TEST_CHECK(vecs[0].mySize == 3 && memcmp(vecs[0].myData, "123", 3) == 0);
		TEST_CHECK(vecs[1].mySize == 4 && memcmp(vecs[1].myData, "4567", 4) == 0);
		TEST_CHECK(vecs[2].mySize == 0 && vecs[2].myData == nullptr);
		TEST_CHECK(vecs[3].mySize == 2 && memcmp(vecs[3].myData, "89", 2) == 0);

		mg::box::IOVecPropagate(pos, count, 2);
		TEST_CHECK(count == 4);
		TEST_CHECK(pos == vecs);
		TEST_CHECK(vecs[0].mySize == 1 && memcmp(vecs[0].myData, "3", 1) == 0);
		TEST_CHECK(vecs[1].mySize == 4 && memcmp(vecs[1].myData, "4567", 4) == 0);

		fillVecs();
		mg::box::IOVecPropagate(pos, count, 3);
		TEST_CHECK(count == 3);
		TEST_CHECK(pos == vecs + 1);
		TEST_CHECK(vecs[1].mySize == 4 && memcmp(vecs[1].myData, "4567", 4) == 0);

		fillVecs();
		mg::box::IOVecPropagate(pos, count, 5);
		TEST_CHECK(count == 3);
		TEST_CHECK(pos == vecs + 1);
		TEST_CHECK(vecs[1].mySize == 2 && memcmp(vecs[1].myData, "67", 2) == 0);
		TEST_CHECK(vecs[2].mySize == 0 && vecs[2].myData == nullptr);
		TEST_CHECK(vecs[3].mySize == 2 && memcmp(vecs[3].myData, "89", 2) == 0);

		fillVecs();
		mg::box::IOVecPropagate(pos, count, 7);
		TEST_CHECK(count == 2);
		TEST_CHECK(pos == vecs + 2);
		TEST_CHECK(vecs[2].mySize == 0 && vecs[2].myData == nullptr);
		TEST_CHECK(vecs[3].mySize == 2 && memcmp(vecs[3].myData, "89", 2) == 0);

		fillVecs();
		mg::box::IOVecPropagate(pos, count, 8);
		TEST_CHECK(count == 1);
		TEST_CHECK(pos == vecs + 3);
		TEST_CHECK(vecs[3].mySize == 1 && memcmp(vecs[3].myData, "9", 1) == 0);

		fillVecs();
		mg::box::IOVecPropagate(pos, count, 9);
		TEST_CHECK(count == 0);
		TEST_CHECK(pos == vecs + 4);
	}
}

	void
	UnitTestIOVec()
	{
		using namespace iovec;
		TestSuiteGuard suite("IOVec");

		UnitTestIOVecPropagate();
	}

}
}
}
