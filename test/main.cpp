#include "mg/box/Definitions.h"

#include "UnitTest.h"

#include <cstdio>

namespace mg {
namespace unittests {

	void UnitTestAtomic();
	void UnitTestBinaryHeap();
	void UnitTestConditionVariable();
	void UnitTestDoublyList();
	void UnitTestError();
	void UnitTestForwardList();
	void UnitTestHost();
	void UnitTestMultiConsumerQueue();
	void UnitTestMultiProducerQueue();
	void UnitTestMutex();
	void UnitTestRefCount();
	void UnitTestSharedPtr();
	void UnitTestSignal();
	void UnitTestString();
	void UnitTestTaskScheduler();
	void UnitTestUtil();

}
}

int
main()
{
	using namespace mg::unittests;

	Report("======== Unit tests ========");

	UnitTestError();

	UnitTestAtomic();
	UnitTestBinaryHeap();
	UnitTestConditionVariable();
	UnitTestDoublyList();
	UnitTestError();
	UnitTestForwardList();
	UnitTestHost();
	UnitTestMultiConsumerQueue();
	UnitTestMultiProducerQueue();
	UnitTestMutex();
	UnitTestRefCount();
	UnitTestSharedPtr();
	UnitTestSignal();
	UnitTestString();
	UnitTestTaskScheduler();
	UnitTestUtil();
	return 0;
}
