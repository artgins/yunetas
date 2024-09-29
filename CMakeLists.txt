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

#-----------------------------------------------------#
#   Check if not exist .config
#-----------------------------------------------------#
# Specify the path to the .config file
set(CONFIG_FILE "${CMAKE_SOURCE_DIR}/.config")
set(KCONFIG_COMMAND "menuconfig")

if(NOT EXISTS ${CONFIG_FILE})
    # The .config file does not exist
    # Run a Kconfig command to create or configure the .config file.
    # For example, using `make menuconfig` or a similar command to generate the .config
    message(STATUS "No .config file found, generating default configuration...")

    # Execute a command in the source directory
    execute_process(
        COMMAND "${KCONFIG_COMMAND}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    # Check if the .config file was successfully created
    if(EXISTS ${CONFIG_FILE})
        message(STATUS "default .config file successfully created.")
    else()
        message(FATAL_ERROR "Failed to create ${CONFIG_FILE} file.\n In upper directory execute manually:\n   ${KCONFIG_COMMAND}\n or other kconfig-? command (See README.rst)")
        exit()
    endif()
endif()

#-----------------------------------------------------#
#   Generate yuneta_config.h from .config (Kconfig)
#-----------------------------------------------------#
set(FILE_H_CONTENT
[=[
/*
 *  Yuneta Configuration
 *  Automatically generated file. DO NOT EDIT.
 *  Use 'kconfig-mconf Kconfig' or 'kconfig-nconf Kconfig' in Yuneta source code directory.
 *
 *  Copyright (c) 2024, ArtGins
 */
#pragma once

]=]
)

# Read file and append definitions to FILE_H_CONTENT
file(STRINGS ${CONFIG_FILE} CONFIG_LINES)
kconfig2include("${FILE_H_CONTENT}" "${CONFIG_LINES}")
# Write the content to .h
file(WRITE ${INC_DEST_DIR}/yuneta_config.h ${FILE_H_CONTENT})

#-----------------------------------------------------#
#   Generate yuneta_version.h from YUNETA_VERSION
#-----------------------------------------------------#
set(FILE_H_CONTENT
[=[
/*
 *  Yuneta Version
 *  Automatically generated file. DO NOT EDIT.
 *  Set version in YUNETA_VERSION file
 *
 *  Copyright (c) 2024, ArtGins
 */
#pragma once

]=]
)

# Read file and append definitions to FILE_H_CONTENT
file(STRINGS ${CMAKE_SOURCE_DIR}/YUNETA_VERSION CONFIG_LINES)
kconfig2include("${FILE_H_CONTENT}" "${CONFIG_LINES}")
# Write the content to .h
file(WRITE ${INC_DEST_DIR}/yuneta_version.h ${FILE_H_CONTENT})

##############################################
#   Source
##############################################
add_subdirectory(kernel/c/gobj-c)
add_subdirectory(kernel/c/root-linux)
add_subdirectory(kernel/c/root-esp32)
add_subdirectory(kernel/c/timeranger2)
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
