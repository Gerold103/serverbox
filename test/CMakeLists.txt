cmake_minimum_required (VERSION 3.8)

add_executable(test
	main.cpp
	UnitTest.cpp
	UnitTest.h
	box/UnitTestAtomic.cpp
	box/UnitTestBinaryHeap.cpp
	box/UnitTestConditionVariable.cpp
	box/UnitTestForwardList.cpp
	box/UnitTestMultiConsumerQueue.cpp
	box/UnitTestMultiProducerQueue.cpp
	box/UnitTestMutex.cpp
	box/UnitTestSignal.cpp
	box/UnitTestString.cpp
	sch/UnitTestTaskScheduler.cpp
)

target_include_directories(test PUBLIC
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(test
	mgsch
	mgtest
)
