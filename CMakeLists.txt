##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.0)
include(/yuneta/development/yuneta/yunetas/tools/cmake/project.cmake)
project(yunetas C)

##############################################
#   Source
##############################################
add_subdirectory(gobj)
add_subdirectory(core-linux)
add_subdirectory(core-esp32)
add_subdirectory(c_prot)
add_subdirectory(yunos)
add_subdirectory(tests)


# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# or compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
