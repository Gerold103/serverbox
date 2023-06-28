cmake_minimum_required (VERSION 3.8)

add_library(mgsch
	Task.cpp
	Task.h
	TaskScheduler.cpp
	TaskScheduler.h
)

target_include_directories(mgsch PUBLIC
	${CMAKE_SOURCE_DIR}/src/
)

target_link_libraries(mgsch
	mgbox
)

set(install_headers
	Task.h
	TaskScheduler.h
)

install(TARGETS mgsch DESTINATION "${install_lib_root}")
install(FILES ${install_headers} DESTINATION "${install_include_root}/mg/sched/")
