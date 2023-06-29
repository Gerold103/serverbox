cmake_minimum_required (VERSION 3.8)

add_library(mgnet
	Host.cpp
	Host.h
)

target_include_directories(mgnet PUBLIC
	${CMAKE_SOURCE_DIR}/src/
)

set(mgnet_libs mgbox)
if(WIN32)
	set(mgnet_libs ${mgnet_libs} ws2_32.lib)
endif()

target_link_libraries(mgnet
	${mgnet_libs}
)

set(install_headers
	Host.h
)

install(TARGETS mgnet DESTINATION "${install_lib_root}")
install(FILES ${install_headers} DESTINATION "${install_include_root}/mg/net/")
