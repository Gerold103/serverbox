cmake_minimum_required (VERSION 3.8)

find_package(Boost REQUIRED)

add_executable(bench_taskscheduler
	BenchTaskScheduler.cpp
)
target_link_libraries(bench_taskscheduler
	mgsch
	bench
)

add_executable(bench_taskscheduler_trivial
	BenchTaskSchedulerTrivial.cpp
)
target_link_libraries(bench_taskscheduler_trivial
	mgsch
	bench
)

add_executable(bench_taskscheduler_boost
	BenchTaskSchedulerBoost.cpp
)
target_link_libraries(bench_taskscheduler_boost
	mgsch
	bench
)
target_include_directories(bench_taskscheduler_boost SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
