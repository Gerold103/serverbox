add_executable(scheduler_03_multistep_task main.cpp)
add_dependencies(scheduler_03_multistep_task install_serverbox)

target_include_directories(scheduler_03_multistep_task PUBLIC
	${MG_SERVERBOX_DIR}/include
)
target_link_directories(scheduler_03_multistep_task PUBLIC
	${MG_SERVERBOX_DIR}/lib
)
target_link_libraries(scheduler_03_multistep_task
	mgsch
	mgboxstub
	mgbox
)
