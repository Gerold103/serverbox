#include "mg/box/Definitions.h"

#include "UnitTest.h"

#include <cstdio>

namespace mg {
namespace unittests {

namespace box {
	void UnitTestAtomic();
	void UnitTestBinaryHeap();
	void UnitTestConditionVariable();
	void UnitTestDoublyList();
	void UnitTestError();
	void UnitTestForwardList();
	void UnitTestIOVec();
	void UnitTestLog();
	void UnitTestMultiConsumerQueue();
	void UnitTestMultiProducerQueue();
	void UnitTestMutex();
	void UnitTestRefCount();
	void UnitTestSharedPtr();
	void UnitTestSignal();
	void UnitTestString();
	void UnitTestSysinfo();
	void UnitTestThreadLocalPool();
	void UnitTestUtil();
}
namespace net {
	void UnitTestBuffer();
	void UnitTestHost();
}
namespace sched {
	void UnitTestTaskScheduler();
}
namespace sio {
	void UnitTestTCPServer();
	void UnitTestTCPSocket();
}

}
}

int
main()
{
	using namespace mg::unittests;

	Report("======== Unit tests ========");

	sio::UnitTestTCPSocket();
	sio::UnitTestTCPServer();
	if ("123" != nullptr)
		return 0;

	box::UnitTestAtomic();
	box::UnitTestBinaryHeap();
	box::UnitTestConditionVariable();
	box::UnitTestDoublyList();
	box::UnitTestError();
	box::UnitTestForwardList();
	box::UnitTestIOVec();
	box::UnitTestLog();
	box::UnitTestMultiConsumerQueue();
	box::UnitTestMultiProducerQueue();
	box::UnitTestMutex();
	box::UnitTestRefCount();
	box::UnitTestSharedPtr();
	box::UnitTestSignal();
	box::UnitTestString();
	box::UnitTestSysinfo();
	box::UnitTestThreadLocalPool();
	box::UnitTestUtil();

	net::UnitTestBuffer();
	net::UnitTestHost();

	sched::UnitTestTaskScheduler();

	sio::UnitTestTCPServer();
	sio::UnitTestTCPSocket();
	return 0;
}
