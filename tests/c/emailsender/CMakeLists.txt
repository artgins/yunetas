##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(test_emailsender C)
get_filename_component(current_directory_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)

#-----------------------------------------------------#
#   Resolve YUNETAS_BASE
#-----------------------------------------------------#
if(DEFINED ENV{YUNETAS_BASE} AND IS_DIRECTORY "$ENV{YUNETAS_BASE}")
    set(YUNETAS_BASE "$ENV{YUNETAS_BASE}")
elseif(IS_DIRECTORY "/yuneta/development/yunetas")
    set(YUNETAS_BASE "/yuneta/development/yunetas")
elseif(IS_DIRECTORY "/yuneta/development")
    set(YUNETAS_BASE "/yuneta/development")
else()
    message(FATAL_ERROR
        "YUNETAS_BASE not found.\n"
        "Set the environment variable YUNETAS_BASE to a valid directory, "
        "or ensure /yuneta/development[/yunetas] exists.")
endif()

message(DEBUG "Using YUNETAS_BASE: ${YUNETAS_BASE}")

set(_yunetas_project_cmake "${YUNETAS_BASE}/tools/cmake/project.cmake")
if(NOT EXISTS "${_yunetas_project_cmake}")
    message(FATAL_ERROR "Missing: ${_yunetas_project_cmake}")
endif()

include("${_yunetas_project_cmake}")

#----------------------------------------#
#   Static binaries
#----------------------------------------#
if(CONFIG_FULLY_STATIC)
    set(CMAKE_EXE_LINKER_FLAGS "-static -Wl,-Bstatic")
    set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-static")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBS OFF)
endif()

##############################################
#   Source
##############################################
SET(SRCS
    auth_transient
    auth_rejected
    set_email_user
    set_url_running
    send_in_handshake
    auth_backoff
    auth_334
    pause_in_flight
    shutdown_in_flight
    set_url_stash
    no_recipients
    data_4xx_paced
    server_close_paced
    late_server
    refill_paced
    long_line
    url_log
)

##############################################
#   Tests
##############################################
foreach(test ${SRCS})
    set(binary "test_emailsender_${test}")
    add_yuno_executable(${binary}
        "main_${test}.c"
        c_fake_smtp.c
        c_test_emailsender.c
        ${YUNETAS_BASE}/yunos/c/emailsender/src/c_emailsender.c
        ${YUNETAS_BASE}/yunos/c/emailsender/src/c_smtp_session.c
        ${YUNETAS_BASE}/yunos/c/emailsender/src/mime_encoder.c
    )

    # the gclasses of emailsender are its own sources
    target_include_directories(${binary} PRIVATE
        ${YUNETAS_BASE}/yunos/c/emailsender/src
    )

    if(CONFIG_FULLY_STATIC)
        set_target_properties(${binary} PROPERTIES
            LINK_SEARCH_START_STATIC TRUE
            LINK_SEARCH_END_STATIC TRUE
        )
    endif()

    target_link_libraries(${binary}
        ${MODULE_TEST}
        ${YUNETAS_KERNEL_LIBS}
        ${YUNETAS_EXTERNAL_LIBS}
        ${YUNETAS_PCRE_LIBS}
        ${JWT_LIBS}
        ${OPENSSL_LIBS}
        ${MBEDTLS_LIBS}
        ${DEBUG_LIBS}
    )
    add_test("${current_directory_name}/${test}" ${binary})
endforeach()
