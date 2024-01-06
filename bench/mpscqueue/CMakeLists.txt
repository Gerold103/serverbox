cmake_minimum_required (VERSION 3.8)

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
