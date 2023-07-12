cmake_minimum_required (VERSION 3.8)

add_library(mgboxstub
	BoxStub.cpp
)

target_include_directories(mgboxstub PUBLIC
	${CMAKE_SOURCE_DIR}/src/
)

target_link_libraries(mgboxstub
	mgbox
)

install(TARGETS mgboxstub DESTINATION "${install_lib_root}")
