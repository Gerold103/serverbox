#include "mg/box/Definitions.h"

#include "UnitTest.h"

#include <cstdio>

namespace mg {
namespace unittests {

	void UnitTestAtomic();
	void UnitTestBinaryHeap();
	void UnitTestBuffer();
	void UnitTestConditionVariable();
	void UnitTestDoublyList();
	void UnitTestError();
	void UnitTestForwardList();
	void UnitTestHost();
	void UnitTestIOVec();
	void UnitTestLog();
	void UnitTestMultiConsumerQueue();
	void UnitTestMultiProducerQueue();
	void UnitTestMutex();
	void UnitTestRefCount();
	void UnitTestSharedPtr();
	void UnitTestSignal();
	void UnitTestString();
	void UnitTestTaskScheduler();
	void UnitTestThreadLocalPool();
	void UnitTestUtil();

}
}

int
main()
{
	using namespace mg::unittests;

	Report("======== Unit tests ========");

	UnitTestLog();
	if ("123" != nullptr)
		return 0;

	UnitTestAtomic();
	UnitTestBinaryHeap();
	UnitTestBuffer();
	UnitTestConditionVariable();
	UnitTestDoublyList();
	UnitTestError();
	UnitTestForwardList();
	UnitTestHost();
	UnitTestIOVec();
	UnitTestLog();
	UnitTestMultiConsumerQueue();
	UnitTestMultiProducerQueue();
	UnitTestMutex();
	UnitTestRefCount();
	UnitTestSharedPtr();
	UnitTestSignal();
	UnitTestString();
	UnitTestTaskScheduler();
	UnitTestThreadLocalPool();
	UnitTestUtil();
	return 0;
}
