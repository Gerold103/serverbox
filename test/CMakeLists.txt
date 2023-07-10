cmake_minimum_required (VERSION 3.8)

add_executable(test
	main.cpp
	UnitTest.cpp
	UnitTest.h
	UnitTestAtomic.cpp
	UnitTestBinaryHeap.cpp
	UnitTestConditionVariable.cpp
	UnitTestDoublyList.cpp
	UnitTestError.cpp
	UnitTestForwardList.cpp
	UnitTestHost.cpp
	UnitTestMultiConsumerQueue.cpp
	UnitTestMultiProducerQueue.cpp
	UnitTestMutex.cpp
	UnitTestRefCount.cpp
	UnitTestSharedPtr.cpp
	UnitTestSignal.cpp
	UnitTestString.cpp
	UnitTestTaskScheduler.cpp
	UnitTestThreadLocalPool.cpp
	UnitTestUtil.cpp
)

target_include_directories(test PUBLIC
	${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test
	mgnet
	mgsched
	mgtest
)
