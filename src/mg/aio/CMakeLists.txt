cmake_minimum_required (VERSION 3.8)

set(mgaio_src
	IOCore.cpp
	IOTask.cpp
	TCPServer.cpp
	TCPSocket.cpp
	TCPSocketCtl.cpp
	TCPSocketIFace.cpp
	TCPSocketSubscription.cpp
	SSLSocket.cpp
)

if(WIN32)
	set(mgaio_src ${mgaio_src}
		IOCore_IOCP.cpp
		IOTask_IOCP.cpp
	)
elseif(APPLE)
	set(mgaio_src ${mgaio_src}
		IOCore_kqueue.cpp
		IOTask_kqueue.cpp
	)
else()
	set(mgaio_src ${mgaio_src}
		IOCore_epoll.cpp
		IOTask_epoll.cpp
	)
endif()

add_library(mgaio ${mgaio_src})

target_include_directories(mgaio PUBLIC
	${CMAKE_SOURCE_DIR}/src/
)

set(mgaio_libs mgnet)

target_link_libraries(mgaio
	${mgaio_libs}
)

set(install_headers
	IOCore.h
	IOTask.h
	SSLSocket.h
	TCPServer.h
	TCPSocket.h
	TCPSocketIFace.h
	TCPSocketSubscription.h
)

install(TARGETS mgaio DESTINATION "${install_lib_root}")
install(FILES ${install_headers} DESTINATION "${install_include_root}/mg/aio/")
