#include "mg/box/Definitions.h"

#include "UnitTest.h"

#include <cstdio>

namespace mg {
namespace unittests {

	void UnitTestAtomic();
	void UnitTestBinaryHeap();
	void UnitTestConditionVariable();
	void UnitTestDoublyList();
	void UnitTestHost();
	void UnitTestForwardList();
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

	UnitTestDoublyList();

	UnitTestAtomic();
	UnitTestBinaryHeap();
	UnitTestConditionVariable();
	UnitTestDoublyList();
	UnitTestHost();
	UnitTestForwardList();
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
