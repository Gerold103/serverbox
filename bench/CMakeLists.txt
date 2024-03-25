cmake_minimum_required (VERSION 3.8)

add_library(bench
	Bench.cpp
)
target_link_libraries(bench
	mgbox
	mgboxstub
	mgtest
)
target_include_directories(bench PUBLIC
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/bench
)

add_subdirectory(io)
add_subdirectory(mcspqueue)
add_subdirectory(mpscqueue)
add_subdirectory(taskscheduler)
