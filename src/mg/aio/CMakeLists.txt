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

set(mgaio_libs mgnet)
set(mgaio_macros)

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
elseif(MG_AIO_USE_IOURING)
	if (LINUX_KERNEL_VERSION VERSION_LESS 5.19)
	    message(FATAL_ERROR "io_uring is only supported for Linux >= 5.19")
	endif()
	set(mgaio_src ${mgaio_src}
		IOCore_iouring.cpp
		IOTask_iouring.cpp
	)
	set(mgaio_libs ${mgaio_libs} -luring)
	set(mgaio_macros MG_IOCORE_USE_IOURING=1)
else()
	set(mgaio_src ${mgaio_src}
		IOCore_epoll.cpp
		IOTask_epoll.cpp
	)
endif()

add_library(mgaio ${mgaio_src})
target_compile_definitions(mgaio PUBLIC ${mgaio_macros})

target_include_directories(mgaio PUBLIC
	${CMAKE_SOURCE_DIR}/src/
)

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
