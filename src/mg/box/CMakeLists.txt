cmake_minimum_required (VERSION 3.8)

set(mgbox_src
	Assert.cpp
	Assert.h
	Atomic.h
	BinaryHeap.h
	ConditionVariable.cpp
	ConditionVariable.h
	Definitions.h
	ForwardList.h
	MultiConsumerQueue.h
	MultiConsumerQueueBase.cpp
	MultiConsumerQueueBase.h
	MultiProducerQueueIntrusive.h
	Mutex.cpp
	Mutex.h
	RefCount.h
	SharedPtr.h
	Signal.cpp
	Signal.h
	StringFunctions.cpp
	StringFunctions.h
	Thread.cpp
	Thread.h
	ThreadFunc.h
	Time.h
	TypeTraits.h
)

if(WIN32)
	set(mgbox_src ${mgbox_src}
		Thread_Win.cpp
		Time_Win.cpp
	)
else()
	set(mgbox_src ${mgbox_src}
		Thread_Unix.cpp
		Time_Unix.cpp
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
	ForwardList.h
	MultiConsumerQueue.h
	MultiConsumerQueueBase.h
	MultiProducerQueueIntrusive.h
	Mutex.h
	RefCount.h
	SharedPtr.h
	Signal.h
	Thread.h
	TypeTraits.h
)

install(TARGETS mgbox DESTINATION "${install_lib_root}")
install(FILES ${install_headers} DESTINATION "${install_include_root}/mg/box/")
