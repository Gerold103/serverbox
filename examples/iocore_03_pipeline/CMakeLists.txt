add_executable(iocore_03_pipeline main.cpp)
add_dependencies(iocore_03_pipeline install_serverbox)

target_include_directories(iocore_03_pipeline PUBLIC
	${MG_SERVERBOX_DIR}/include
)
target_link_directories(iocore_03_pipeline PUBLIC
	${MG_SERVERBOX_DIR}/lib
)
set(libs
	mgaio
	mgnet
	mgsch
	mgbox
	mgboxstub
)
if(WIN32)
	set(libs ${libs} ws2_32.lib crypt32.lib)
endif()
target_link_libraries(iocore_03_pipeline
	${libs}
)
