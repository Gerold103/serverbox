cmake_minimum_required (VERSION 3.8)

set(mgbox_src
	Assert.cpp
	ConditionVariable.cpp
	Coro.cpp
	Error.cpp
	IOVec.cpp
	Log.cpp
	MultiConsumerQueueBase.cpp
	Mutex.cpp
	Signal.cpp
	StringFunctions.cpp
	Thread.cpp
)

if(WIN32)
	set(mgbox_src ${mgbox_src}
		Sysinfo_Win.cpp
		Thread_Win.cpp
		Time_Win.cpp
	)
elseif(APPLE)
	set(mgbox_src ${mgbox_src}
		Sysinfo_Apple.cpp
		Thread_Unix.cpp
		Time_Unix.cpp
	)
else()
	set(mgbox_src ${mgbox_src}
		Sysinfo_Linux.cpp
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
	Coro.h
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
	StringFunctions.h
	Thread.h
	ThreadLocalPool.h
	Time.h
	TypeTraits.h
)

install(TARGETS mgbox DESTINATION "${install_lib_root}")
install(FILES ${install_headers} DESTINATION "${install_include_root}/mg/box/")
