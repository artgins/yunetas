##################################################################
#       CMake
##################################################################
cmake_minimum_required(VERSION 3.5)
project(yunetas C)

#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#-----------------------------------------------------#
if(DEFINED ENV{YUNETAS_BASE})
    set(YUNETAS_BASE_DIR $ENV{YUNETAS_BASE})
else()
    set(YUNETAS_BASE_DIR "/yuneta/development/yunetas")
endif()
include("${YUNETAS_BASE_DIR}/tools/cmake/project.cmake")

#-------------------------------#
#       Options
#-------------------------------#
option (ENABLE_TESTS "Build tests" ON)
option (ENABLE_PERFORMANCE "Build performance" ON)
option (ENABLE_STRESS "Build stress" ON)

##############################################
#   Source
##############################################
add_subdirectory(kernel/c/gobj-c)
add_subdirectory(kernel/c/ytls)
add_subdirectory(kernel/c/yev_loop)
add_subdirectory(kernel/c/timeranger2)
add_subdirectory(kernel/c/root-linux)
add_subdirectory(kernel/c/root-esp32)
add_subdirectory(modules/c/c_prot)
add_subdirectory(utils/c)
#add_subdirectory(yunos/c)

if (ENABLE_TESTS)
    # Add tests
    enable_testing ()
    add_subdirectory(tests/c)
endif ()

if (ENABLE_PERFORMANCE)
    add_subdirectory(performance/c)
endif ()

if (ENABLE_STRESS)
    add_subdirectory(stress/c)
endif ()

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


#
# To exclude the tests add:
#
#   mkdir build && cd build
#   cmake -DCMAKE_BUILD_TYPE=Debug -D ENABLE_TESTS=OFF ..
#
# or exclude performance tests
#   mkdir build && cd build
#   cmake -DCMAKE_BUILD_TYPE=Debug -D ENABLE_PERFORMANCE=OFF ..
#

# With NINJA
# To build and install with debug and tests::
#
#   mkdir build && cd build
#   cmake -GNinja -DCMAKE_BUILD_TYPE=Debug ..
#   ninja
#   ninja install
#   ctest    # to run tests
#
#
# With NINJA
# To build without debug::
#
#   mkdir build && cd build
#   cmake -GNinja ..
#   ninja
#   ninja install
#   ctest    # to run tests

#
# To get options of cmake:
#
#   cd build
#   cmake -LH .
#


#
# To run tests:
#
#   cd build
#       make && make install
#   or
#       cmake --build . --target install
#   ctest
#


# Sample of loop
#message("Default Search Paths:")
#foreach(path ${CMAKE_SYSTEM_PREFIX_PATH})
#    MESSAGE(STATUS "${path}")
#endforeach()
