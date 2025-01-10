add_executable(iocore_01_tcp_hello main.cpp)
add_dependencies(iocore_01_tcp_hello install_serverbox)

target_include_directories(iocore_01_tcp_hello PUBLIC
	${MG_SERVERBOX_DIR}/include
)
target_link_directories(iocore_01_tcp_hello PUBLIC
	${MG_SERVERBOX_DIR}/lib
)
set(libs
	mgaio
	mgnet
	mgbox
	mgboxstub
)
if(WIN32)
	set(libs ${libs} ws2_32.lib crypt32.lib)
endif()
target_link_libraries(iocore_01_tcp_hello
	${libs}
)
