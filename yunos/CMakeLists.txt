##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
include(/yuneta/development/yunetas/tools/cmake/project.cmake)
project(yunos C)

##############################################
#   Source
##############################################
#add_subdirectory(test_frigo)
#add_subdirectory(test_esteriliz)
#add_subdirectory(test_sonda)


# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#   mkdir build && cd build
#   cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# or compile with NO optimization and adding debug symbols :
#
#   mkdir build && cd build
#   cmake -DCMAKE_BUILD_TYPE=Debug ..
#
