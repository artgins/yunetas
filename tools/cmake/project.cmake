################################################################################
#   project.cmake
################################################################################
cmake_minimum_required(VERSION 3.11)

include(CheckIncludeFiles)
include(CheckSymbolExists)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

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
    message(DEBUG "${PROJECT_NAME}: Loading Kconfig-style variables from ${CONFIG_FILE}")

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
            message(DEBUG "Loaded: ${KEY} = ${VALUE}")
        endif()
    endforeach()
else()
    message(FATAL_ERROR ".config file not found at ${CONFIG_FILE}")
endif()


#--------------------------------------------------#
#   glibc coherence guard
#
#   The core library records the glibc it was built with; every other build
#   refuses to link against that record if it does not match this machine.
#   See tools/cmake/libc_guard.cmake for why a mismatch corrupts the heap.
#--------------------------------------------------#
include(${YUNETAS_BASE}/tools/cmake/libc_guard.cmake)

if(NOT ESP_PLATFORM)
    if(PROJECT_NAME STREQUAL "yunetas-gobj")
        yuneta_write_libc_stamp()
    else()
        yuneta_check_libc_stamp()
    endif()
endif()

#--------------------------------------------------#
#   Wrapp add_yuno_executable
#--------------------------------------------------#
function(add_yuno_executable name)
    message(STATUS "Add executable: ${name}")
    add_executable(${name} ${ARGN})
endfunction()

#----------------------------------------#
#   Add tools/cmake to the module path
#----------------------------------------#
list(APPEND CMAKE_MODULE_PATH "${YUNETAS_BASE}/tools/cmake")

#----------------------------------------#
#   Global definitions and include paths
#----------------------------------------#
list(APPEND CMAKE_SYSTEM_PREFIX_PATH "${YUNETAS_BASE}/outputs")
set(CMAKE_INSTALL_PREFIX "${YUNETAS_BASE}/outputs")

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

    # The build datetime of every yuno is __DATE__ " " __TIME__, which is the
    # LOCAL time of the compiling process and says no zone. Compile under
    # TZ=UTC so it is UTC, always; entry_point.c publishes it as ISO 8601
    # with a Z (print-role, the agent's `binaries` topic).
    if(NOT "${CMAKE_C_COMPILER_LAUNCHER}" MATCHES "TZ=UTC")
        set(CMAKE_C_COMPILER_LAUNCHER env TZ=UTC ${CMAKE_C_COMPILER_LAUNCHER})
    endif()

    include_directories("${YUNETAS_BASE}/outputs_ext/include")
    link_directories("${YUNETAS_BASE}/outputs_ext/lib")

    include_directories("${INC_DEST_DIR}")
    link_directories("${LIB_DEST_DIR}")

endif()

#--------------------------------------------------#
#   An installed archive that CHANGED is newer than its consumers
#
#   Everything links the INSTALLED archives (outputs/lib), so `make` relinks
#   a consumer only when the installed file is newer than the consumer. But
#   `install()` keeps the mtime of the file it copies, which is the time the
#   archive was BUILT, not installed. A consumer linked between the two
#   moments -- the unified build/ tree builds the libraries, then links the
#   tests against the old installed copies, then installs -- looks newer than
#   the library it was not linked with, and is never relinked: a later
#   `cmake --build build --target <test>` runs the previous library.
#
#   So, before the library is installed: when the built archive differs from
#   the installed one, stamp the built one with the current time, and the
#   copy carries it. An archive that did not change is not stamped, is not
#   copied again ("Up-to-date"), and relinks nobody.
#
#   The stamp waits for the next whole second first: the copy keeps the
#   mtime in whole seconds (x.000000000), so an archive stamped at 12:00:07.3
#   is installed as 12:00:07.0 -- OLDER than a test linked at 12:00:07.1,
#   which then is not relinked. Up to one second per CHANGED library.
#
#   It runs in every directory that includes this file, and does nothing
#   where no lib${PROJECT_NAME}.a is built (yunos, tests, utils).
#--------------------------------------------------#
if(NOT ESP_PLATFORM)
    set(_yuneta_built_archive "${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.a")
    install(CODE "
        set(_built \"${_yuneta_built_archive}\")
        set(_installed \"\$ENV{DESTDIR}${LIB_DEST_DIR}/lib${PROJECT_NAME}.a\")
        if(EXISTS \"\${_built}\")
            execute_process(
                COMMAND \"\${CMAKE_COMMAND}\" -E compare_files \"\${_built}\" \"\${_installed}\"
                RESULT_VARIABLE _same
                OUTPUT_QUIET
                ERROR_QUIET
            )
            if(NOT _same EQUAL 0)
                string(TIMESTAMP _second \"%s\")
                string(TIMESTAMP _now \"%s\")
                while(_now STREQUAL _second)
                    execute_process(COMMAND \"\${CMAKE_COMMAND}\" -E sleep 0.05)
                    string(TIMESTAMP _now \"%s\")
                endwhile()
                file(TOUCH_NOCREATE \"\${_built}\")
            endif()
        endif()
    ")
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

# Apply the compile flags globally
add_compile_options(${COMMON_C_FLAGS})

#----------------------------------------#
#   Compiler link flags
#----------------------------------------#
set(COMPILER_LINK_FLAGS
    #    -no-pie # it seems that with this is getting slower
)

if(NOT ESP_PLATFORM)
    # desktop / linux / glibc
    add_link_options(${COMPILER_LINK_FLAGS})
endif()

#----------------------------------------#
#   Libraries
#
#   Each .a is given by FULL PATH (not a bare -l name resolved via
#   link_directories) so CMake tracks it as a link dependency of every
#   consuming target: editing a kernel/module source rebuilds its .a and the
#   yuno relinks automatically on the next build (no `clean` needed). Yuneta's
#   own archives live in LIB_DEST_DIR; third-party archives in EXT_LIB_DIR.
#   System libs (pthread, dl) stay bare names -- they are -l flags, not files.
#----------------------------------------#
set(EXT_LIB_DIR ${YUNETAS_BASE}/outputs_ext/lib)

set(YUNETAS_KERNEL_LIBS
    ${LIB_DEST_DIR}/libyunetas-core-linux.a
    ${EXT_LIB_DIR}/libargp-standalone.a
    ${LIB_DEST_DIR}/libtimeranger2.a
    ${LIB_DEST_DIR}/libyev_loop.a
    ${LIB_DEST_DIR}/libytls.a
    ${LIB_DEST_DIR}/libyunetas-gobj.a
)

set(YUNETAS_EXTERNAL_LIBS
    ${EXT_LIB_DIR}/libjansson.a
    ${EXT_LIB_DIR}/liburing.a
)

set(YUNETAS_PCRE_LIBS
    ${EXT_LIB_DIR}/libpcre2-8.a
)

# libjwt-y.a is listed separately to avoid duplication when both TLS backends are enabled
if (CONFIG_HAVE_OPENSSL OR CONFIG_HAVE_MBEDTLS)
    set(JWT_LIBS ${LIB_DEST_DIR}/libjwt-y.a)
else()
    set(JWT_LIBS "")
endif()

if (CONFIG_HAVE_OPENSSL)
    set(OPENSSL_LIBS
        ${EXT_LIB_DIR}/libssl.a
        ${EXT_LIB_DIR}/libcrypto.a
        pthread
        dl
    )
else()
    set(OPENSSL_LIBS "")
endif()

if (CONFIG_HAVE_MBEDTLS)
    set(MBEDTLS_LIBS
        ${EXT_LIB_DIR}/libmbedtls.a
        ${EXT_LIB_DIR}/libmbedx509.a
        ${EXT_LIB_DIR}/libmbedcrypto.a
    )
else()
    set(MBEDTLS_LIBS "")
endif()

if (CONFIG_DEBUG_WITH_BACKTRACE)
    set(DEBUG_LIBS
        ${EXT_LIB_DIR}/libbacktrace.a
    )
else()
    set(DEBUG_LIBS "")
endif()

if (CONFIG_MODULE_CONSOLE)
    set(MODULE_CONSOLE
        ${LIB_DEST_DIR}/libyunetas-module-console.a
    )
else()
    set(MODULE_CONSOLE "")
endif()

if (CONFIG_MODULE_MQTT)
    set(MODULE_MQTT
        ${LIB_DEST_DIR}/libyunetas-module-mqtt.a
    )
else()
    set(MODULE_MQTT "")
endif()

if (CONFIG_MODULE_MODBUS)
    set(MODULE_MODBUS
        ${LIB_DEST_DIR}/libyunetas-module-modbus.a
    )
else()
    set(MODULE_MODBUS "")
endif()

if (CONFIG_MODULE_POSTGRES)
    set(MODULE_POSTGRES
        ${LIB_DEST_DIR}/libyunetas-module-postgres.a
    )
else()
    set(MODULE_POSTGRES "")
endif()

if (CONFIG_MODULE_TEST)
    set(MODULE_TEST
        ${LIB_DEST_DIR}/libyunetas-module-test.a
    )
else()
    set(MODULE_TEST "")
endif()
