cmake_minimum_required (VERSION 3.8)

set(mgaio_src
	DomainToIP.cpp
	DomainToIP.h
	IOCore.cpp
	IOCore.h
	IOTask.cpp
	IOTask.h
	TCPServer.cpp
	TCPServer.h
)

if(WIN32)
	set(mgaio_src ${mgaio_src}
		DomainToIP_Win.cpp
		IOCore_Win.cpp
		IOTask_Win.cpp
	)
elseif(APPLE)
	set(mgaio_src ${mgaio_src}
		DomainToIP_Posix.cpp
		IOCore_Apple.cpp
		IOTask_Apple.cpp
	)
else()
	set(mgaio_src ${mgaio_src}
		DomainToIP_Posix.cpp
		IOCore_Linux.cpp
		IOTask_Linux.cpp
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
	DomainToIP.h
	IOCore.h
	IOTask.h
	TCPServer.h
)

install(TARGETS mgaio DESTINATION "${install_lib_root}")
install(FILES ${install_headers} DESTINATION "${install_include_root}/mg/aio/")
