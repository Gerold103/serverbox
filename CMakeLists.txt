cmake_minimum_required (VERSION 3.8)

project("ServerBox")

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
		-fno-rtti
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

add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(bench)
