################################################################################
#   config_prelude.cmake
#   Reads .config,
#   applies Kconfig choices before project() (compiler, toolchain, build type),
#   and exports YUNETAS_BASE.
################################################################################
cmake_minimum_required(VERSION 3.11)

# Requires YUNETAS_BASE to be set by the including CMakeLists.txt
if(NOT DEFINED YUNETAS_BASE)
    message(FATAL_ERROR "YUNETAS_BASE must be set before including config_prelude.cmake")
endif()
set(YUNETAS_BASE "${YUNETAS_BASE}" CACHE PATH "Yunetas base dir" FORCE)

# --- Load .config (Kconfig output with CONFIG_* keys)
set(CONFIG_FILE "${YUNETAS_BASE}/.config")
if(NOT EXISTS "${CONFIG_FILE}")
    message(FATAL_ERROR ".config file not found at ${CONFIG_FILE}")
endif()

file(READ "${CONFIG_FILE}" _CFG)
string(REPLACE "\n" ";" _LINES "${_CFG}")
foreach(_L ${_LINES})
    string(STRIP "${_L}" _L)
    if(_L STREQUAL "" OR _L MATCHES "^#")  # skip comments/empty
        continue()
    endif()
    if(_L MATCHES "^([^=]+)=(.*)$")
        set(_K "${CMAKE_MATCH_1}")         # expects CONFIG_*
        set(_V "${CMAKE_MATCH_2}")
        # strip trailing inline comment: VALUE # comment
        if(_V MATCHES "^(.*)[ \t]+#")
            set(_V "${CMAKE_MATCH_1}")
        endif()
        string(STRIP "${_V}" _V)
        if(_V STREQUAL "y")
            set(_V 1)
        elseif(_V STREQUAL "n")
            set(_V 0)
        elseif(_V MATCHES "^\".*\"$")
            string(SUBSTRING "${_V}" 1 -1 _V)
        endif()
        set(${_K} "${_V}")
    endif()
endforeach()

# --- Select compiler/toolchain BEFORE project()
# Priority: MUSL toolchain (sets its own compiler), else CLANG, else GCC.
if(DEFINED CONFIG_USE_COMPILER_MUSL AND CONFIG_USE_COMPILER_MUSL)
    set(CMAKE_TOOLCHAIN_FILE
        "${YUNETAS_BASE}/tools/cmake/musl-toolchain.cmake"
        CACHE FILEPATH "" FORCE)
elseif(DEFINED CONFIG_USE_COMPILER_CLANG AND CONFIG_USE_COMPILER_CLANG)
    set(CMAKE_C_COMPILER clang CACHE STRING "" FORCE)
elseif(DEFINED CONFIG_USE_COMPILER_GCC AND CONFIG_USE_COMPILER_GCC)
    set(CMAKE_C_COMPILER gcc CACHE STRING "" FORCE)
endif()

# --- Build type (unless user already set -DCMAKE_BUILD_TYPE)
if(NOT CMAKE_BUILD_TYPE)
    if(DEFINED CONFIG_BUILD_TYPE_RELEASE AND CONFIG_BUILD_TYPE_RELEASE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    elseif(DEFINED CONFIG_BUILD_TYPE_DEBUG AND CONFIG_BUILD_TYPE_DEBUG)
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
    elseif(DEFINED CONFIG_BUILD_TYPE_MINSIZEREL AND CONFIG_BUILD_TYPE_MINSIZEREL)
        set(CMAKE_BUILD_TYPE "MinSizeRel" CACHE STRING "" FORCE)
    else()
        # Default/explicit RelWithDebInfo if selected or by fallback
        set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
    endif()
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

# --- Status
message(STATUS "Yunetas: YUNETAS_BASE=${YUNETAS_BASE}")
if(DEFINED CONFIG_USE_COMPILER_MUSL AND CONFIG_USE_COMPILER_MUSL)
    message(STATUS "Yunetas: Compiler=MUSL toolchain (${CMAKE_TOOLCHAIN_FILE})")
elseif(DEFINED CONFIG_USE_COMPILER_CLANG AND CONFIG_USE_COMPILER_CLANG)
    message(STATUS "Yunetas: Compiler=Clang")
elseif(DEFINED CONFIG_USE_COMPILER_GCC AND CONFIG_USE_COMPILER_GCC)
    message(STATUS "Yunetas: Compiler=GCC")
else()
    message(STATUS "Yunetas: Compiler=Auto (CMake default)")
endif()
message(STATUS "Yunetas: CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
