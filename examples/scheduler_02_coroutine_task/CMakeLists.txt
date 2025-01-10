add_executable(scheduler_02_coroutine_task main.cpp)
add_dependencies(scheduler_02_coroutine_task install_serverbox)

target_include_directories(scheduler_02_coroutine_task PUBLIC
	${MG_SERVERBOX_DIR}/include
)
target_link_directories(scheduler_02_coroutine_task PUBLIC
	${MG_SERVERBOX_DIR}/lib
)
target_link_libraries(scheduler_02_coroutine_task
	mgsch
	mgboxstub
	mgbox
)
