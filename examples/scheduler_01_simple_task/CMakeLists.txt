add_executable(scheduler_01_simple_task main.cpp)
add_dependencies(scheduler_01_simple_task install_serverbox)

target_include_directories(scheduler_01_simple_task PUBLIC
	${MG_SERVERBOX_DIR}/include
)
target_link_directories(scheduler_01_simple_task PUBLIC
	${MG_SERVERBOX_DIR}/lib
)
target_link_libraries(scheduler_01_simple_task
	mgsch
	mgboxstub
	mgbox
)
