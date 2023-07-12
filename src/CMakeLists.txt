cmake_minimum_required (VERSION 3.8)

set(install_include_root "include/")
set(install_lib_root "lib/")

add_subdirectory(mg/box)
add_subdirectory(mg/net)
add_subdirectory(mg/sch)
add_subdirectory(mg/stub)
add_subdirectory(mg/test)
