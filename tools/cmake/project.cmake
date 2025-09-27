################################################################################
#   project.cmake
################################################################################
cmake_minimum_required(VERSION 3.11)

include(CheckIncludeFiles)
include(CheckSymbolExists)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

message(STATUS "Using $ENV{CC} compiler")

#--------------------------------------------------#
#   Check YUNETAS_BASE
#--------------------------------------------------#
if("${YUNETAS_BASE}" STREQUAL "")
    message(FATAL_ERROR "YUNETAS_BASE is empty. Define it in environment.")
endif()

#--------------------------------------------------#
#   Include .config file
#--------------------------------------------------#
set(CONFIG_FILE ${YUNETAS_BASE}/.config)

if(EXISTS ${CONFIG_FILE})
    message(STATUS "${PROJECT_NAME}: Loading Kconfig-style variables from ${CONFIG_FILE}")

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
#   Wrapp add_yuno_executable
#--------------------------------------------------#
function(add_yuno_executable name)
    message(STATUS "Add executable: ${name}")
    add_executable(${name} ${ARGN})
    if(AS_STATIC)
        # To use with musl compiler
        message(STATUS "is AS_STATIC: ${name}")
        set_target_properties(${name} PROPERTIES
            LINK_SEARCH_START_STATIC TRUE
            LINK_SEARCH_END_STATIC TRUE
        )
    endif()
endfunction()

#----------------------------------------#
#   Add tools/cmake to the module path
#----------------------------------------#
list(APPEND CMAKE_MODULE_PATH "${YUNETAS_BASE}/tools/cmake")

#----------------------------------------#
#   Global definitions and include paths
#----------------------------------------#
if(AS_STATIC)
    # To use with musl compiler
    list(APPEND CMAKE_SYSTEM_PREFIX_PATH "${YUNETAS_BASE}/outputs_static")
    set(CMAKE_INSTALL_PREFIX "${YUNETAS_BASE}/outputs_static")
else()
    list(APPEND CMAKE_SYSTEM_PREFIX_PATH "${YUNETAS_BASE}/outputs")
    set(CMAKE_INSTALL_PREFIX "${YUNETAS_BASE}/outputs")
endif()

#----------------------------------------#
#   Add system prefix and install prefix
#   In `outputs` of parent
#----------------------------------------#
set(INC_DEST_DIR     ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_DEST_DIR     ${CMAKE_INSTALL_PREFIX}/lib)
set(BIN_DEST_DIR     ${CMAKE_INSTALL_PREFIX}/bin)
set(YUNOS_DEST_DIR   ${CMAKE_INSTALL_PREFIX}/yunos)

if(ESP_PLATFORM)
    message(STATUS "ESP-IDF platform build")
else()
    # desktop / linux / glibc
    add_definitions(-D_GNU_SOURCE)

    if(AS_STATIC)
        # To use with musl compiler
        include_directories("${YUNETAS_BASE}/outputs_ext_static/include")
        link_directories("${YUNETAS_BASE}/outputs_ext_static/lib")
    else()
        include_directories("${YUNETAS_BASE}/outputs_ext/include")
        link_directories("${YUNETAS_BASE}/outputs_ext/lib")
    endif()

    include_directories("${INC_DEST_DIR}")
    link_directories("${LIB_DEST_DIR}")

endif()

#----------------------------------------#
#   Default if not specified
#----------------------------------------#
if(NOT CMAKE_BUILD_TYPE)
#    message(FATAL_ERROR "No build type defined")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

#----------------------------------------#
#   Common compile flags
#----------------------------------------#
set(COMMON_C_FLAGS
    -Wall
    -Wextra
    -Wno-type-limits
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wmissing-prototypes
    -Wstrict-prototypes
    -funsigned-char
)


if(NOT ESP_PLATFORM)
    # desktop / linux / glibc
    add_link_options(${COMPILER_LINK_FLAGS})
endif()

#----------------------------------------#
#   Compiler link flags
#----------------------------------------#
set(COMPILER_LINK_FLAGS
#    -no-pie # it seems that with this is getting slower
)

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
)

set(YUNETAS_PCRE_LIBS
    libpcre2-8.a
)

if (CONFIG_HAVE_OPENSSL)
    set(OPENSSL_LIBS
        libjwt-y.a
        libssl.a
        libcrypto.a
        pthread
        dl
    )
else()
    set(OPENSSL_LIBS "")
endif()

if (CONFIG_HAVE_MBEDTLS)
    set(MBEDTLS_LIBS
        libjwt-y.a
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

if (CONFIG_C_PROT)
    set(PROT_LIBS
        libyunetas-c_prot.a
    )
else()
    set(PROT_LIBS "")
endif()

if (CONFIG_C_CONSOLE)
    set(CONSOLE_LIBS
        libyunetas-c_console.a
    )
else()
    set(CONSOLE_LIBS "")
endif()

if (CONFIG_C_POSTGRES)
    set(POSTGRES_LIBS
        libyunetas-c_postgres.a
    )
else()
    set(POSTGRES_LIBS "")
endif()
