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

##############################################
#   Source
##############################################
add_subdirectory(gobj)
add_subdirectory(core-linux)
add_subdirectory(core-esp32)
add_subdirectory(c_prot)
add_subdirectory(yunos)

#message("Default Search Paths:")
#foreach(path ${CMAKE_SYSTEM_PREFIX_PATH})
#    message("  ${path}")
#endforeach()

if (ENABLE_TESTS)
    # Check for criterion
    find_package (Criterion REQUIRED)

    # Add tests
    enable_testing ()
    add_subdirectory(tests)
endif ()

# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# or compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
