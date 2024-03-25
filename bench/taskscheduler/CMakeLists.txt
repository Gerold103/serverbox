cmake_minimum_required (VERSION 3.8)

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
