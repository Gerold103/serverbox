cmake_minimum_required (VERSION 3.8)

set(mgnet_src
	Buffer.cpp
	DomainToIP.cpp
	Host.cpp
	Socket.cpp
	URL.cpp
)

if(WIN32)
	set(mgnet_src ${mgnet_src}
		Socket_Win.cpp
	)
else()
	set(mgnet_src ${mgnet_src}
		Socket_Unix.cpp
	)
endif()

add_library(mgnet ${mgnet_src})

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
	Buffer.h
	DomainToIP.h
	Host.h
	URL.h
)

install(TARGETS mgnet DESTINATION "${install_lib_root}")
install(FILES ${install_headers} DESTINATION "${install_include_root}/mg/net/")
