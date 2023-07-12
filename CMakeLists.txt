cmake_minimum_required (VERSION 3.8)

project("ServerBox")

set(CMAKE_CXX_STANDARD 11)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	add_compile_options(
		-Wall -Wextra -Wpedantic -Werror -Wno-unknown-warning-option -Wunused-function
		-Wno-invalid-offsetof -Wno-unused-value -Wno-deprecated-copy
		-Wno-gnu-zero-variadic-macro-arguments
		-fno-exceptions
		-fno-rtti
	)

	set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-pthread")
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-pthread")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(
		# This is needed at least for correct macro handling. Default behaviour won't
		# expand __VA_ARGS__ correctly.
		/Zc:preprocessor
		/Wall /WX /wd4324 /wd4355 /wd4365 /wd4458 /wd4514 /wd4625 /wd4626 /wd4668 /wd4710
		/wd4820 /wd5026 /wd5027 /wd5039 /wd5045 /wd5219 /wd26439 /wd26800
	)
endif()

add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(bench)
