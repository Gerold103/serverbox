cmake_minimum_required (VERSION 3.8)

find_package(Boost)

set(bench_io_src
	BenchIO.cpp
	BenchIOHelp.cpp
	BenchIOTCPClient.cpp
	BenchIOTCPServer.cpp
	BenchIOTools.cpp
)
if(${Boost_FOUND})
	set(BOOST_MACRO MG_BENCH_IO_HAS_BOOST=1)
	set(bench_io_src ${bench_io_src}
		BenchIOBoostCore.cpp
		BenchIOTCPClientBoost.cpp
	)
	set(bench_io_include_dirs ${Boost_INCLUDE_DIRS})
	if (MG_BOOST_USE_IOURING)
		set(BOOST_MACRO ${BOOST_MACRO}
			BOOST_ASIO_HAS_IO_URING=1
			BOOST_ASIO_DISABLE_EPOLL=1
		)
	endif()
else()
	set(BOOST_MACRO MG_BENCH_IO_HAS_BOOST=0)
	set(bench_io_include_dirs)
endif()
add_executable(bench_io ${bench_io_src})
target_compile_definitions(bench_io PRIVATE ${BOOST_MACRO})
target_include_directories(bench_io SYSTEM PUBLIC ${bench_io_include_dirs})

target_link_libraries(bench_io
	mgaio
	mgbox
	mgboxstub
	mgnet
	bench
)
