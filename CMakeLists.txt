##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
include(/yuneta/development/yunetas/tools/cmake/project.cmake)
project(yunetas C)

# Add tools/cmake to the module path
list(APPEND CMAKE_MODULE_PATH "/yuneta/development/yunetas/tools/cmake")
list(APPEND CMAKE_SYSTEM_PREFIX_PATH "/yuneta/development/outputs")





# Initialize the header content
set(CONFIG_H_CONTENT "/*\n *  Automatically generated file. DO NOT EDIT.\n *  Use 'kconfig-mconf Kconfig' or 'kconfig-nconf Kconfig' in Yuneta directory.\n */\n\n")


# Read .config file and append definitions to CONFIG_H_CONTENT
file(STRINGS ${CMAKE_SOURCE_DIR}/.config CONFIG_LINES)
foreach(line ${CONFIG_LINES})
    if(line MATCHES "^#.*" OR line MATCHES "^$")
        # Skip comments and empty lines
    else()
        # Extract configuration key and value
        string(REPLACE "=" ";" CONFIG_PAIR ${line})
        list(GET CONFIG_PAIR 0 CONFIG_KEY)
        list(GET CONFIG_PAIR 1 CONFIG_VALUE)
        # Remove possible quotes from string values
        string(REPLACE "\"" "" CONFIG_VALUE ${CONFIG_VALUE})

        # Append the configuration to the header content
        if("${CONFIG_VALUE}" STREQUAL "y")
            set(CONFIG_H_CONTENT "${CONFIG_H_CONTENT}#define ${CONFIG_KEY} 1\n")
        elseif("${CONFIG_VALUE}" MATCHES "^[0-9]+$")
            set(CONFIG_H_CONTENT "${CONFIG_H_CONTENT}#define ${CONFIG_KEY} ${CONFIG_VALUE}\n")
        else()
            set(CONFIG_H_CONTENT "${CONFIG_H_CONTENT}#define ${CONFIG_KEY} \"${CONFIG_VALUE}\"\n")
        endif()
    endif()
endforeach()

# Write the content to config.h
file(WRITE ${INC_DEST_DIR}/yuneta_config.h ${CONFIG_H_CONTENT})






option (ENABLE_TESTS "Build tests" ON)
option (ENABLE_PERFORMANCE "Build performance" ON)

##############################################
#   Source
##############################################
add_subdirectory(kernel/c/gobj-c)
add_subdirectory(kernel/c/root-linux)
add_subdirectory(kernel/c/root-esp32)
add_subdirectory(libs/c/c_prot)
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
