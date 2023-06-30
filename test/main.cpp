#include "mg/box/Definitions.h"

#include "UnitTest.h"

#include <cstdio>

namespace mg {
namespace unittests {

	void UnitTestAtomic();
	void UnitTestBinaryHeap();
	void UnitTestConditionVariable();
	void UnitTestHost();
	void UnitTestForwardList();
	void UnitTestMultiConsumerQueue();
	void UnitTestMultiProducerQueue();
	void UnitTestMutex();
	void UnitTestRefCount();
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

	UnitTestAtomic();
	UnitTestBinaryHeap();
	UnitTestConditionVariable();
	UnitTestHost();
	UnitTestForwardList();
	UnitTestMultiConsumerQueue();
	UnitTestMultiProducerQueue();
	UnitTestMutex();
	UnitTestRefCount();
	UnitTestSignal();
	UnitTestString();
	UnitTestTaskScheduler();
	UnitTestUtil();
	return 0;
}
