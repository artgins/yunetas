##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(c-modules C)

##############################################
#   Source
##############################################
add_subdirectory(c_console)
add_subdirectory(c_postgres)
add_subdirectory(c_prot)
add_subdirectory(c_test)

# compile in Release mode :
#
#     cmake -DCMAKE_BUILD_TYPE=Release ..
#
# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
# compile to build static binary :
#
#     cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/yuneta/development/yunetas/tools/cmake/musl-toolchain.cmake ..
#
#
