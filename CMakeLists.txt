##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.0)
include(/yuneta/development/yuneta/yunetas/tools/cmake/project.cmake)
project(yunetas C)

# Add tools/cmake to the module path
list(APPEND CMAKE_MODULE_PATH "/yuneta/development/yuneta/yunetas/tools/cmake")
list(APPEND CMAKE_SYSTEM_PREFIX_PATH "/yuneta/development/outputs")


option (ENABLE_TESTS "Build tests" ON)
option (ENABLE_PERFORMANCE "Build performance" ON)

##############################################
#   Source
##############################################
add_subdirectory(gobj)
add_subdirectory(core-linux)
add_subdirectory(core-esp32)
add_subdirectory(c_prot)
add_subdirectory(yunos)

if (ENABLE_TESTS)
    # Check for criterion
    find_package (Criterion REQUIRED)

    # Add tests
    enable_testing ()
    add_subdirectory(tests)
endif ()

if (ENABLE_PERFORMANCE)
    add_subdirectory(performance)
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
#    message("  ${path}")
#endforeach()
