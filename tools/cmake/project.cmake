cmake_minimum_required(VERSION 3.5)

include(CheckIncludeFiles)
include(CheckSymbolExists)

set (CMAKE_ENABLE_EXPORTS TRUE)

set(CMAKE_INSTALL_PREFIX /yuneta/development/outputs)

set(INC_DEST_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_DEST_DIR ${CMAKE_INSTALL_PREFIX}/lib)
set(BIN_DEST_DIR ${CMAKE_INSTALL_PREFIX}/bin)

add_definitions(-D_GNU_SOURCE)
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)

include_directories(/yuneta/development/outputs/include)
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /yuneta/development/outputs/lib)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
# TODO check if -fno-stack-protector is only for esp32
    add_compile_options(-std=c99 -Wall -Wextra -g3 -fno-pie -fno-stack-protector -Wno-unused-parameter -fPIC)
    add_link_options(-no-pie)
else()
    add_compile_options(-std=c99 -Wall -Wextra -g3 -O -fno-pie -fno-stack-protector -Wno-unused-parameter -fPIC)
    add_link_options(-no-pie)
endif()

#if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
#    MESSAGE("=================> Clang")
#else()
#    MESSAGE("=================> NOT CLang")
#endif()
#
#MESSAGE(STATUS "DIR ${CMAKE_CURRENT_SOURCE_DIR}")
#MESSAGE(STATUS "COMPILER_ID ${CMAKE_C_COMPILER_ID}")
#MESSAGE(STATUS "COMPILER ${CMAKE_C_COMPILER}")

function(kconfig2include CONFIG_H_CONTENT CONFIG_LINES)
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
                set(CONFIG_H_CONTENT "${CONFIG_H_CONTENT}#define ${CONFIG_KEY} 1\n" PARENT_SCOPE)
            elseif("${CONFIG_VALUE}" MATCHES "^[0-9]+$")
                set(CONFIG_H_CONTENT "${CONFIG_H_CONTENT}#define ${CONFIG_KEY} ${CONFIG_VALUE}\n" PARENT_SCOPE)
            else()
                set(CONFIG_H_CONTENT "${CONFIG_H_CONTENT}#define ${CONFIG_KEY} \"${CONFIG_VALUE}\"\n" PARENT_SCOPE)
            endif()
        endif()
    endforeach()
endfunction()
