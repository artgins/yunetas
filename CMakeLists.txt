##################################################################
#       CMake
##################################################################
cmake_minimum_required(VERSION 3.11)
project(yunetas C)

#-------------------------------#
#       Options
#-------------------------------#
option (ENABLE_SDK "Build the SDK (kernel, modules, utils, yunos) in this tree too" OFF)
option (ENABLE_TESTS "Build tests" ON)
option (ENABLE_PERFORMANCE "Build performance" ON)
option (ENABLE_STRESS "Build stress" ON)

##############################################
#   Source
##############################################
#   The SDK is built and installed by the build dir of each module
#   (`yunetas build`, and the first step of `yunetas test`); the tests here
#   link what that installs in outputs/lib, by full path, so a changed
#   library relinks them. Building the SDK here as well installed a SECOND
#   copy of every library into outputs/lib over the first one: each tree
#   then found the other's copy, installed its own, and the new mtime
#   relinked every executable of the other tree, on every run.
#   ENABLE_SDK=ON makes this tree self-contained (an ASan tree, a tree with
#   no module build dirs).
if (ENABLE_SDK)
    add_subdirectory(kernel/c/gobj-c)
    add_subdirectory(kernel/c/ytls)
    add_subdirectory(kernel/c/yev_loop)
    add_subdirectory(kernel/c/libjwt)
    add_subdirectory(kernel/c/timeranger2)
    add_subdirectory(kernel/c/root-linux)
    add_subdirectory(kernel/c/root-esp32)
    add_subdirectory(modules/c)
    add_subdirectory(utils/c)
    add_subdirectory(yunos/c)
endif ()

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
# To run tests (the SDK built and installed first by `yunetas build`, or
# configure with -DENABLE_SDK=ON):
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
