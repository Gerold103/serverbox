cmake_minimum_required (VERSION 3.8)

set(mgtest_src
	CommandLine.cpp
	Message.cpp
	MetricMovingAverage.cpp
	MetricSpeed.cpp
	Random.cpp
)

add_library(mgtest ${mgtest_src})

target_include_directories(mgtest PUBLIC
	${CMAKE_SOURCE_DIR}/src/
)

target_link_libraries(mgtest
	mgbox
)
