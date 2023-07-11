#include "mg/net/Buffer.h"

#include "UnitTest.h"

#include "mg/box/StringFunctions.h"

#define TEST_CHECK MG_BOX_ASSERT

namespace mg {
namespace unittests {

	static void
	UnitTestBufferBasic()
	{
		TestCaseGuard guard("Basic");

		mg::net::Buffer::Ptr b = mg::net::BufferRaw::NewShared("abc", 4, 0);
		TEST_CHECK(b->myPos == 4);
		TEST_CHECK(b->myCapacity == 0);
		mg::net::Buffer::Ptr head = b;

		mg::net::Buffer::Ptr next = mg::net::BufferCopy::NewShared();
		TEST_CHECK(&next->myWData == &next->myRData);
		TEST_CHECK(next->myPos == 0);
		TEST_CHECK(next->myCapacity == mg::net::theBufferCopySize);
		memcpy(next->myWData, "defg", 5);
		next->myPos = 5;
		b->myNext = std::move(next);
		b = b->myNext;

		b->myNext = mg::net::BufferRaw::NewShared();
		TEST_CHECK(b->myNext->myPos == 0);
		TEST_CHECK(b->myNext->myCapacity == 0);

		TEST_CHECK(head->myPos == 4);
		TEST_CHECK(mg::box::Strcmp((const char*)head->myRData, "abc") == 0);

		b = head->myNext;
		TEST_CHECK(b->myPos == 5);
		TEST_CHECK(mg::box::Strcmp((const char*)b->myRData, "defg") == 0);

		b = b->myNext;
		TEST_CHECK(b->myPos == 0);
		TEST_CHECK(!b->myNext.IsSet());
	}

	void
	UnitTestBuffer()
	{
		TestSuiteGuard suite("Buffer");

		UnitTestBufferBasic();
	}

}
}
