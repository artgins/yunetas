##################################################################
#       CMake
##################################################################
cmake_minimum_required(VERSION 3.5)
include(/yuneta/development/yunetas/tools/cmake/project.cmake)
project(yunetas C)

# Add tools/cmake to the module path
list(APPEND CMAKE_MODULE_PATH "/yuneta/development/yunetas/tools/cmake")
list(APPEND CMAKE_SYSTEM_PREFIX_PATH "/yuneta/development/outputs")

#-------------------------------#
#       Options
#-------------------------------#
option (ENABLE_TESTS "Build tests" ON)
option (ENABLE_PERFORMANCE "Build performance" ON)


#-----------------------------------------------------#
#   Generate yuneta_config.h from .config (Kconfig)
#-----------------------------------------------------#
set(CONFIG_H_CONTENT
[=[
/*
 *  Yuneta Configuration
 *  Automatically generated file. DO NOT EDIT.
 *  Use 'kconfig-mconf Kconfig' or 'kconfig-nconf Kconfig' in Yuneta source code directory.
 *
 *  Copyright (c) 2013, Niyamaka <niyamaka@yuneta.io>
 */
#pragma once

]=]
)

# Read file and append definitions to CONFIG_H_CONTENT
file(STRINGS ${CMAKE_SOURCE_DIR}/.config CONFIG_LINES)
kconfig2include("${CONFIG_H_CONTENT}" "${CONFIG_LINES}")
# Write the content to .h
file(WRITE ${INC_DEST_DIR}/yuneta_config.h ${CONFIG_H_CONTENT})

#-----------------------------------------------------#
#   Generate yuneta_version.h from YUNETA_VERSION
#-----------------------------------------------------#
set(CONFIG_H_CONTENT
    [=[
/*
 *  Yuneta Version
 *  Automatically generated file. DO NOT EDIT.
 *  Set version in YUNETA_VERSION file
 *
 *  Copyright (c) 2013, Niyamaka <niyamaka@yuneta.io>
 */
#pragma once

]=]
)

# Read file and append definitions to CONFIG_H_CONTENT
file(STRINGS ${CMAKE_SOURCE_DIR}/YUNETA_VERSION CONFIG_LINES)
kconfig2include("${CONFIG_H_CONTENT}" "${CONFIG_LINES}")
# Write the content to .h
file(WRITE ${INC_DEST_DIR}/yuneta_version.h ${CONFIG_H_CONTENT})

##############################################
#   Source
##############################################
add_subdirectory(kernel/c/gobj-c)
add_subdirectory(kernel/c/root-linux)
add_subdirectory(kernel/c/root-esp32)
add_subdirectory(modules/c/c_prot)
#add_subdirectory(yunos/c)

if (ENABLE_TESTS)
    # Check for criterion
    find_package (Criterion REQUIRED)

    # Add tests
    enable_testing ()
    add_subdirectory(tests/c)
endif ()

if (ENABLE_PERFORMANCE)
    add_subdirectory(performance/c)
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
