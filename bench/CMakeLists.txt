cmake_minimum_required (VERSION 3.8)

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
	${CMAKE_SOURCE_DIR}/bench
)

add_subdirectory(mcspqueue)
add_subdirectory(mpscqueue)
add_subdirectory(taskscheduler)
