cmake_minimum_required (VERSION 3.8)

option(MG_IS_CI "Whether is running in CI" 0)

if (MG_IS_CI)
	add_compile_definitions(MG_IS_CI=1)
else()
	add_compile_definitions(MG_IS_CI=0)
endif()

add_executable(test
	main.cpp
	UnitTest.cpp
	UnitTestSSLCerts.cpp
	aio/UnitTestTCPServer.cpp
	aio/UnitTestTCPSocketIFace.cpp
	box/UnitTestAlgorithm.cpp
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
	box/UnitTestTime.cpp
	net/UnitTestBuffer.cpp
	net/UnitTestDomainToIP.cpp
	net/UnitTestHost.cpp
	net/UnitTestSSL.cpp
	net/UnitTestURL.cpp
	sch/UnitTestTaskScheduler.cpp
	sio/UnitTestTCPServer.cpp
	sio/UnitTestTCPSocket.cpp
)

target_include_directories(test PUBLIC
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/test
)

target_link_libraries(test
	mgaio
	mgbox
	mgboxstub
	mgnet
	mgsch
	mgsio
	mgtest
)
