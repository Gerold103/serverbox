add_executable(iocore_04_tcp_periodic main.cpp)
add_dependencies(iocore_04_tcp_periodic install_serverbox)

target_include_directories(iocore_04_tcp_periodic PUBLIC
	${MG_SERVERBOX_DIR}/include
)
target_link_directories(iocore_04_tcp_periodic PUBLIC
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
target_link_libraries(iocore_04_tcp_periodic
	${libs}
)
