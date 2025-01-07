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
#   Include .config file
#--------------------------------------------------#
set(CONFIG_FILE ${YUNETAS_BASE_DIR}/.config)

if(EXISTS ${CONFIG_FILE})
    message(STATUS "Loading Kconfig-style variables from ${CONFIG_FILE}")

    # Read the file line by line
    file(READ ${CONFIG_FILE} CONFIG_CONTENTS)
    string(REPLACE "\n" ";" CONFIG_LINES ${CONFIG_CONTENTS})

    foreach(LINE ${CONFIG_LINES})
        string(STRIP "${LINE}" LINE) # Remove leading/trailing whitespace

        # Skip comments and empty lines
        if(LINE MATCHES "^#.*" OR LINE STREQUAL "")
            continue()
        endif()

        # Parse KEY=VALUE pairs
        if(LINE MATCHES "([^=]+)=(.*)")
            set(KEY "${CMAKE_MATCH_1}")
            set(VALUE "${CMAKE_MATCH_2}")

            # Handle special cases for values
            if(VALUE STREQUAL "y") # Convert 'y' to 1
                set(VALUE 1)
            elseif(VALUE STREQUAL "n") # Convert 'n' to 0
                set(VALUE 0)
            elseif(VALUE MATCHES "^\".*\"$") # Remove quotes from strings
                string(SUBSTRING ${VALUE} 1 -1 VALUE)
            endif()

            # Set the variable in CMake
            set(${KEY} ${VALUE})
            message(STATUS "Loaded: ${KEY} = ${VALUE}")
        endif()
    endforeach()
else()
    message(FATAL_ERROR ".config file not found at ${CONFIG_FILE}")
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

# Specify C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

#----------------------------------------#
#   Libraries
#----------------------------------------#
set(YUNETAS_KERNEL_LIBS
    libyunetas-core-linux.a
    libargp-standalone.a
    libtimeranger2.a
    libyev_loop.a
    libytls.a
    libyunetas-gobj.a
)

set(YUNETAS_EXTERNAL_LIBS
    libjansson.a
    liburing.a
    libpcre2-8.a
)

set(YUNETAS_PCRE_LIBS
    libpcre2-8.a
)

if (CONFIG_YTLS_USE_OPENSSL)
    set(OPENSSL_LIBS
        libssl.a
        libcrypto.a
        pthread dl
    )
else()
    set(OPENSSL_LIBS "")
endif()

if (CONFIG_YTLS_USE_MBEDTLS)
    set(MBEDTLS_LIBS
        libmbedtls.a
        libmbedx509.a
        libmbedcrypto.a
    )
else()
    set(MBEDTLS_LIBS "")
endif()

if (CONFIG_DEBUG_WITH_BACKTRACE)
    set(DEBUG_LIBS
        libbacktrace.a
    )
else()
    set(DEBUG_LIBS "")
endif()
