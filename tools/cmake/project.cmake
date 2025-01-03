cmake_minimum_required(VERSION 3.5)

include(CheckIncludeFiles)
include(CheckSymbolExists)

#--------------------------------------------------#
#   Check YUNETAS_BASE_DIR
#--------------------------------------------------#
if("${YUNETAS_BASE_DIR}" STREQUAL "")
    message(FATAL_ERROR "YUNETAS_BASE_DIR is empty. Define it in environment.")
endif()

#--------------------------------------------------#
#   Get the parent of YUNETAS_BASE
#   where the project code can be
#   and where the output of yunetas is installed
#--------------------------------------------------#
get_filename_component(YUNETAS_PARENT_BASE_DIR "${YUNETAS_BASE_DIR}" DIRECTORY)

#----------------------------------------#
#   Add tools/cmake to the module path
#----------------------------------------#
list(APPEND CMAKE_MODULE_PATH "${YUNETAS_BASE_DIR}/tools/cmake")

#----------------------------------------#
#   Add system prefix and install prefix
#   In `outputs` of parent
#----------------------------------------#
list(APPEND CMAKE_SYSTEM_PREFIX_PATH "${YUNETAS_PARENT_BASE_DIR}/outputs")
set(CMAKE_INSTALL_PREFIX "${YUNETAS_PARENT_BASE_DIR}/outputs")

set(INC_DEST_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_DEST_DIR ${CMAKE_INSTALL_PREFIX}/lib)
set(BIN_DEST_DIR ${CMAKE_INSTALL_PREFIX}/bin)
set(YUNOS_DEST_DIR ${CMAKE_INSTALL_PREFIX}/yunos)

add_definitions(-D_GNU_SOURCE)
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)

include_directories("${YUNETAS_PARENT_BASE_DIR}/outputs_ext/include")
link_directories("${YUNETAS_PARENT_BASE_DIR}/outputs_ext/lib")

include_directories("${YUNETAS_PARENT_BASE_DIR}/outputs/include")
link_directories("${YUNETAS_PARENT_BASE_DIR}/outputs/lib")

include_directories("/usr/include")
link_directories("/usr/lib")

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
# TODO check if -fno-stack-protector is only for esp32
    add_compile_options(-std=c99 -Wall -Wextra -Wno-type-limits -Wno-sign-compare -g3 -fno-pie -fno-stack-protector -Wno-unused-parameter -fPIC)
    add_link_options(-no-pie)
else()
    add_compile_options(-std=c99 -Wall -Wextra -Wno-type-limits -Wno-sign-compare -g3 -O -fno-pie -fno-stack-protector -Wno-unused-parameter -fPIC)
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

function(kconfig2include FILE_H_CONTENT CONFIG_LINES)
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
                set(FILE_H_CONTENT "${FILE_H_CONTENT}#define ${CONFIG_KEY} 1\n" PARENT_SCOPE)
            elseif("${CONFIG_VALUE}" MATCHES "^[0-9]+$")
                set(FILE_H_CONTENT "${FILE_H_CONTENT}#define ${CONFIG_KEY} ${CONFIG_VALUE}\n" PARENT_SCOPE)
            else()
                set(FILE_H_CONTENT "${FILE_H_CONTENT}#define ${CONFIG_KEY} \"${CONFIG_VALUE}\"\n" PARENT_SCOPE)
            endif()
        endif()
    endforeach()
endfunction()
