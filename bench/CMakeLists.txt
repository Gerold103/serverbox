cmake_minimum_required (VERSION 3.8)

find_package(Boost)

add_library(bench
	Bench.cpp
	Bench.h
)
target_link_libraries(bench
	mgbox
	mgtest
)
target_include_directories(bench PUBLIC
	${CMAKE_SOURCE_DIR}/src
)

add_executable(bench_taskscheduler
	BenchTaskScheduler.cpp
	BenchTaskSchedulerTemplate.hpp
)
target_link_libraries(bench_taskscheduler
	mgsch
	bench
)

add_executable(bench_taskscheduler_trivial
	BenchTaskSchedulerTrivial.cpp
	BenchTaskSchedulerTemplate.hpp
)
target_link_libraries(bench_taskscheduler_trivial
	mgsch
	bench
)

add_executable(bench_mcspqueue
	BenchMultiConsumerQueue.cpp
	BenchMultiConsumerQueueTemplate.hpp
)
target_link_libraries(bench_mcspqueue
	mgbox
	bench
)

add_executable(bench_mcspqueue_trivial
	BenchMultiConsumerQueueTrivial.cpp
	BenchMultiConsumerQueueTemplate.hpp
)
target_link_libraries(bench_mcspqueue_trivial
	mgbox
	bench
)

add_executable(bench_mpscqueue
	BenchMultiProducerQueue.cpp
	BenchMultiProducerQueueTemplate.hpp
)
target_link_libraries(bench_mpscqueue
	mgbox
	bench
)

add_executable(bench_mpscqueue_trivial
	BenchMultiProducerQueueTrivial.cpp
	BenchMultiProducerQueueTemplate.hpp
)
target_link_libraries(bench_mpscqueue_trivial
	mgbox
	bench
)

set(bench_io_src
	BenchIO.cpp
	BenchIOBoostCore.cpp
	BenchIOHelp.cpp
	BenchIOTCPClient.cpp
	BenchIOTCPClientBoost.cpp
	BenchIOTCPServer.cpp
	BenchIOTools.cpp
)
if(${Boost_FOUND})
	set(BOOST_MACRO MG_BENCH_IO_HAS_BOOST=1)
else()
	set(BOOST_MACRO MG_BENCH_IO_HAS_BOOST=0)
endif()
add_executable(bench_io ${bench_io_src})
target_compile_definitions(bench_io PRIVATE ${BOOST_MACRO})

target_link_libraries(bench_io
	mgaio
	mgbox
	mgboxstub
	mgnet
	bench
)
