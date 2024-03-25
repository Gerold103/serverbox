cmake_minimum_required (VERSION 3.8)

add_executable(bench_mcspqueue
	BenchMultiConsumerQueue.cpp
)
target_link_libraries(bench_mcspqueue
	mgbox
	bench
)

add_executable(bench_mcspqueue_trivial
	BenchMultiConsumerQueueTrivial.cpp
)
target_link_libraries(bench_mcspqueue_trivial
	mgbox
	bench
)
