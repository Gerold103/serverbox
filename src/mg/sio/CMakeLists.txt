cmake_minimum_required (VERSION 3.8)

set(mgsio_src
	Socket.cpp
	Socket.h
	TCPServer.cpp
	TCPServer.h
	TCPSocket.cpp
	TCPSocket.h
)

if(WIN32)
	set(mgsio_src ${mgsio_src}
		Socket_Win.cpp
	)
else()
	set(mgsio_src ${mgsio_src}
		Socket_Unix.cpp
	)
endif()

add_library(mgsio ${mgsio_src})

target_include_directories(mgsio PUBLIC
	${CMAKE_SOURCE_DIR}/src/
)

set(mgsio_libs mgnet)

target_link_libraries(mgsio
	${mgsio_libs}
)

set(install_headers
	Socket.h
	TCPServer.h
	TCPSocket.h
)

install(TARGETS mgsio DESTINATION "${install_lib_root}")
install(FILES ${install_headers} DESTINATION "${install_include_root}/mg/sio/")
