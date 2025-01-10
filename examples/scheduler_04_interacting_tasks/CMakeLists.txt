add_executable(scheduler_04_interacting_tasks main.cpp)
add_dependencies(scheduler_04_interacting_tasks install_serverbox)

target_include_directories(scheduler_04_interacting_tasks PUBLIC
	${MG_SERVERBOX_DIR}/include
)
target_link_directories(scheduler_04_interacting_tasks PUBLIC
	${MG_SERVERBOX_DIR}/lib
)
target_link_libraries(scheduler_04_interacting_tasks
	mgsch
	mgboxstub
	mgbox
)
