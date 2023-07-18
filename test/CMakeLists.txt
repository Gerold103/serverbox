cmake_minimum_required (VERSION 3.8)

add_executable(test_exe
	main.cpp
	UnitTest.cpp
	UnitTest.h
	aio/UnitTestTCPServer.cpp
	box/UnitTestAtomic.cpp
	box/UnitTestBinaryHeap.cpp
	box/UnitTestConditionVariable.cpp
	box/UnitTestDoublyList.cpp
	box/UnitTestError.cpp
	box/UnitTestForwardList.cpp
	box/UnitTestIOVec.cpp
	box/UnitTestLog.cpp
	box/UnitTestMultiConsumerQueue.cpp
	box/UnitTestMultiProducerQueue.cpp
	box/UnitTestMutex.cpp
	box/UnitTestRefCount.cpp
	box/UnitTestSharedPtr.cpp
	box/UnitTestSignal.cpp
	box/UnitTestString.cpp
	box/UnitTestSysinfo.cpp
	box/UnitTestThreadLocalPool.cpp
	box/UnitTestUtil.cpp
	net/UnitTestBuffer.cpp
	net/UnitTestHost.cpp
	sched/UnitTestTaskScheduler.cpp
	sio/UnitTestTCPServer.cpp
	sio/UnitTestTCPSocket.cpp
)
set_target_properties(test_exe PROPERTIES OUTPUT_NAME "test")

target_include_directories(test_exe PUBLIC
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(test_exe
	mgaio
	mgbox
	mgnet
	mgsched
	mgsio
	mgtest
	mgboxstub
)
