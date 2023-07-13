cmake_minimum_required (VERSION 3.8)

set(mgbox_src
	Assert.cpp
	Assert.h
	Atomic.h
	BinaryHeap.h
	ConditionVariable.cpp
	ConditionVariable.h
	Definitions.h
	DoublyList.h
	Error.cpp
	Error.h
	ForwardList.h
	IOVec.cpp
	IOVec.h
	Log.cpp
	Log.h
	MultiConsumerQueue.h
	MultiConsumerQueueBase.cpp
	MultiConsumerQueueBase.h
	MultiProducerQueueIntrusive.h
	Mutex.cpp
	Mutex.h
	QPTimer.h
	RefCount.h
	SharedPtr.h
	Signal.cpp
	Signal.h
	StringFunctions.cpp
	StringFunctions.h
	Sysinfo.h
	Thread.cpp
	Thread.h
	ThreadFunc.h
	ThreadLocalPool.h
	Time.h
	TypeTraits.h
	Util.cpp
	Util.h
)

if(WIN32)
	set(mgbox_src ${mgbox_src}
		QPTimer_Win.cpp
		Sysinfo_Win.cpp
		Thread_Win.cpp
		Time_Win.cpp
	)
else()
	set(mgbox_src ${mgbox_src}
		QPTimer_Posix.cpp
		Sysinfo_Posix.cpp
		Thread_Posix.cpp
		Time_Posix.cpp
	)
endif()

add_library(mgbox ${mgbox_src} "ThreadFunc.h")

target_include_directories(mgbox PUBLIC
	${CMAKE_SOURCE_DIR}/src/
)

set(install_headers
	Assert.h
	Atomic.h
	BinaryHeap.h
	ConditionVariable.h
	Definitions.h
	DoublyList.h
	Error.h
	ForwardList.h
	IOVec.h
	Log.h
	MultiConsumerQueue.h
	MultiConsumerQueueBase.h
	MultiProducerQueueIntrusive.h
	Mutex.h
	RefCount.h
	SharedPtr.h
	Signal.h
	Thread.h
	ThreadLocalPool.h
	TypeTraits.h
)

install(TARGETS mgbox DESTINATION "${install_lib_root}")
install(FILES ${install_headers} DESTINATION "${install_include_root}/mg/box/")
