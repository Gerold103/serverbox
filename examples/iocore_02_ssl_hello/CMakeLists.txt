add_executable(iocore_02_ssl_hello main.cpp Certs.cpp)
add_dependencies(iocore_02_ssl_hello install_serverbox)

find_package(OpenSSL REQUIRED)

target_include_directories(iocore_02_ssl_hello PUBLIC
	${MG_SERVERBOX_DIR}/include
	${OPENSSL_INCLUDE_DIR}
)
target_link_directories(iocore_02_ssl_hello PUBLIC
	${MG_SERVERBOX_DIR}/lib
)
set(libs
	mgaio
	mgnet
	mgbox
	mgboxstub
	${OPENSSL_SSL_LIBRARY}
	${OPENSSL_CRYPTO_LIBRARY}
)
if(WIN32)
	set(libs ${libs} ws2_32.lib crypt32.lib)
endif()
target_link_libraries(iocore_02_ssl_hello
	${libs}
)
