cmake_minimum_required (VERSION 3.8)

project("Examples")

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "Build type ${CMAKE_BUILD_TYPE}")

if (NOT DEFINED CMAKE_CXX_STANDARD)
	message(STATUS "Using C++20 standard as default")
	set(CMAKE_CXX_STANDARD 20)
else()
	message(STATUS "Using C++${CMAKE_CXX_STANDARD} standard as explicitly requested")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	add_compile_options(
		-Wall -Wextra -Wpedantic -Werror -Wno-unknown-warning-option -Wunused-function
		-Wno-invalid-offsetof -Wno-unused-value -Wno-deprecated-copy
		-Wno-gnu-zero-variadic-macro-arguments
	)

	set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-pthread")
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-pthread")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(
		# This is needed at least for correct macro handling. Default behaviour won't
		# expand __VA_ARGS__ correctly.
		/Zc:preprocessor
		/WX /wd4266 /wd4324 /wd4355 /wd4365 /wd4458 /wd4514 /wd4548 /wd4625 /wd4626
		/wd4668 /wd4710 /wd4820 /wd5026 /wd5027 /wd5039 /wd5045 /wd5105 /wd5219 /wd26439
		/wd26800
		# It ignores 'break' and 'fallthrough' done via a macro which makes it annoying
		# and pointless.
		/wd5262
		# Info message about a function being inlined.
		/wd4711
	)
endif()

set(MG_SERVERBOX_BUILD_DIR ${CMAKE_BINARY_DIR}/build_serverbox)
set(MG_SERVERBOX_DIR ${MG_SERVERBOX_BUILD_DIR}/installed)

add_custom_target(install_serverbox
	COMMAND ${CMAKE_COMMAND}
		-S ${CMAKE_SOURCE_DIR}/..
		-B ${MG_SERVERBOX_BUILD_DIR}
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DMG_ENABLE_TEST=0
		-DMG_ENABLE_BENCH=0
		-DCMAKE_INSTALL_PREFIX=${MG_SERVERBOX_DIR}

	COMMAND ${CMAKE_COMMAND}
		--build ${MG_SERVERBOX_BUILD_DIR} --config ${CMAKE_BUILD_TYPE} -j

	COMMAND ${CMAKE_COMMAND}
		--install ${MG_SERVERBOX_BUILD_DIR} --config ${CMAKE_BUILD_TYPE}

	COMMENT "Installing serverbox"
)

add_subdirectory(iocore_01_tcp_hello)
add_subdirectory(iocore_02_ssl_hello)
add_subdirectory(iocore_03_pipeline)
add_subdirectory(iocore_04_tcp_periodic)
add_subdirectory(scheduler_01_simple_task)
add_subdirectory(scheduler_02_coroutine_task)
add_subdirectory(scheduler_03_multistep_task)
add_subdirectory(scheduler_04_interacting_tasks)
