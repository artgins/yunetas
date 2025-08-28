################################################################################
#   project_common.cmake
################################################################################
cmake_minimum_required(VERSION 3.11)

include(CheckIncludeFiles)
include(CheckSymbolExists)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Require YUNETAS_BASE (set by prelude)
if("${YUNETAS_BASE}" STREQUAL "")
    message(FATAL_ERROR "YUNETAS_BASE is empty. Define it in environment.")
endif()

# --- Helper: add_yuno_executable
function(add_yuno_executable name)
    message(STATUS "Add executable: ${name}")
    add_executable(${name} ${ARGN})
    if(AS_STATIC)
        message(STATUS "is AS_STATIC: ${name}")
        set_target_properties(${name} PROPERTIES
            LINK_SEARCH_START_STATIC TRUE
            LINK_SEARCH_END_STATIC TRUE
        )
    endif()
endfunction()

# --- Paths derived from YUNETAS_BASE
get_filename_component(YUNETAS_PARENT_BASE_DIR "${YUNETAS_BASE}" DIRECTORY)
list(APPEND CMAKE_MODULE_PATH "${YUNETAS_BASE}/tools/cmake")

# --- Common compile flags
set(COMMON_C_FLAGS
    -Wall -Wextra
    -Wno-type-limits
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wmissing-prototypes
    -Wstrict-prototypes
    -funsigned-char
)
add_compile_options(${COMMON_C_FLAGS})

# --- Link flags (minimal)
set(COMPILER_LINK_FLAGS
    #   -no-pie
)
add_link_options(${COMPILER_LINK_FLAGS})

# --- Include/link dirs per static/dynamic layout
if(AS_STATIC)
    list(APPEND CMAKE_SYSTEM_PREFIX_PATH "${YUNETAS_PARENT_BASE_DIR}/outputs_static")
    set(CMAKE_INSTALL_PREFIX "${YUNETAS_PARENT_BASE_DIR}/outputs_static")
    include_directories("${YUNETAS_PARENT_BASE_DIR}/outputs_ext_static/include")
    link_directories("${YUNETAS_PARENT_BASE_DIR}/outputs_ext_static/lib")
    include_directories("${YUNETAS_PARENT_BASE_DIR}/outputs_static/include")
    link_directories("${YUNETAS_PARENT_BASE_DIR}/outputs_static/lib")
else()
    list(APPEND CMAKE_SYSTEM_PREFIX_PATH "${YUNETAS_PARENT_BASE_DIR}/outputs")
    set(CMAKE_INSTALL_PREFIX "${YUNETAS_PARENT_BASE_DIR}/outputs")
    include_directories("${YUNETAS_PARENT_BASE_DIR}/outputs_ext/include")
    link_directories("${YUNETAS_PARENT_BASE_DIR}/outputs_ext/lib")
    include_directories("${YUNETAS_PARENT_BASE_DIR}/outputs/include")
    link_directories("${YUNETAS_PARENT_BASE_DIR}/outputs/lib")
endif()

# --- Install destinations
set(INC_DEST_DIR     ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_DEST_DIR     ${CMAKE_INSTALL_PREFIX}/lib)
set(BIN_DEST_DIR     ${CMAKE_INSTALL_PREFIX}/bin)
set(YUNOS_DEST_DIR   ${CMAKE_INSTALL_PREFIX}/yunos)
set(AGENT_DEST_DIR   ${CMAKE_INSTALL_PREFIX}/agent)

# --- Default build type already set by prelude; keep GUI list
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

message(STATUS "Yunetas: Install prefix: ${CMAKE_INSTALL_PREFIX}")

# --- Libraries (unchanged)
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
