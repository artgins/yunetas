##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(yunos C)

#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#-----------------------------------------------------#
if(DEFINED ENV{YUNETAS_BASE})
    set(YUNETAS_BASE $ENV{YUNETAS_BASE})
else()
    set(YUNETAS_BASE "/yuneta/development/yunetas")
endif()
include("${YUNETAS_BASE}/tools/cmake/project.cmake")

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
