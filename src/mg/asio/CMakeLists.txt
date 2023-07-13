cmake_minimum_required (VERSION 3.8)

set(mgasio_src
	IOCore.cpp
	IOCore.h
	IOTask.cpp
	IOTask.h
	TCPServer.cpp
	TCPServer.h
)

if(WIN32)
	set(mgasio_src ${mgasio_src}
		IOCore_Win.cpp
		IOTask_Win.cpp
	)
elseif(APPLE)
	set(mgasio_src ${mgasio_src}
		IOCore_Apple.cpp
		IOTask_Apple.cpp
	)
else()
	set(mgasio_src ${mgasio_src}
		IOCore_Linux.cpp
		IOTask_Linux.cpp
	)
endif()

add_library(mgasio ${mgasio_src})

target_include_directories(mgasio PUBLIC
	${CMAKE_SOURCE_DIR}/src/
)

set(mgasio_libs mgnet)

target_link_libraries(mgasio
	${mgasio_libs}
)

set(install_headers
	IOCore.h
	IOTask.h
	TCPServer.h
)

install(TARGETS mgasio DESTINATION "${install_lib_root}")
install(FILES ${install_headers} DESTINATION "${install_include_root}/mg/asio/")
