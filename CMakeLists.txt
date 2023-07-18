cmake_minimum_required (VERSION 3.8)

project("ServerBox")

set(CMAKE_CXX_STANDARD 11)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	add_compile_options(
		-Wall -Wextra -Werror -Wno-unknown-warning-option -Wunused-function
		-Wno-invalid-offsetof -Wno-unused-value -Wno-deprecated-copy
		-Wno-gnu-zero-variadic-macro-arguments
		-fno-exceptions
		-fno-rtti
	)
	if (NOT CMAKE_COMPILER_IS_GNUCXX OR
	CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.1)
		add_compile_options(-Wpedantic)
	endif()

	set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-pthread")
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-pthread")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(
		/Wall /WX /wd4324 /wd4355 /wd4365 /wd4458 /wd4514 /wd4625 /wd4626 /wd4668 /wd4710
		/wd4820 /wd5026 /wd5027 /wd5039 /wd5045 /wd5219
	)
endif()

add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(bench)
