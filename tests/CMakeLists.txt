##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.0)
include(/yuneta/development/yuneta/yunetas/tools/cmake/project.cmake)
project(tests C)

##############################################
#   Source
##############################################
add_subdirectory(test_yev_loop)

# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# or compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
