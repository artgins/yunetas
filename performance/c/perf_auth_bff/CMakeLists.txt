##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(perf_auth_bff C)
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

set(_yunetas_project_cmake "${YUNETAS_BASE}/tools/cmake/project.cmake")
if(NOT EXISTS "${_yunetas_project_cmake}")
    message(FATAL_ERROR "Missing: ${_yunetas_project_cmake}")
endif()

include("${_yunetas_project_cmake}")

if(CONFIG_FULLY_STATIC)
    set(CMAKE_EXE_LINKER_FLAGS "-static -Wl,-Bstatic")
    set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-static")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBS OFF)
endif()

##############################################
#   Shared sources from tests/c/c_auth_bff
##############################################
set(AUTH_BFF_TEST_DIR "${YUNETAS_BASE}/tests/c/c_auth_bff")

SET(SHARED_SRCS
    "${AUTH_BFF_TEST_DIR}/c_mock_keycloak.c"
    "${AUTH_BFF_TEST_DIR}/test_helpers.c"
)

include_directories("${AUTH_BFF_TEST_DIR}")

##############################################
#   Binary
##############################################
SET(BIN "perf_auth_bff")

add_yuno_executable(${BIN}
    "main_perf_auth_bff.c"
    "c_perf_auth_bff.c"
    ${SHARED_SRCS}
)

if(CONFIG_FULLY_STATIC)
    set_target_properties(${BIN} PROPERTIES
        LINK_SEARCH_START_STATIC TRUE
        LINK_SEARCH_END_STATIC TRUE
    )
endif()

target_link_libraries(${BIN}
    ${YUNETAS_KERNEL_LIBS}
    ${YUNETAS_EXTERNAL_LIBS}
    ${YUNETAS_PCRE_LIBS}
    ${JWT_LIBS}
    ${OPENSSL_LIBS}
    ${MBEDTLS_LIBS}
    ${DEBUG_LIBS}
)

##############################################
#   System install
##############################################
install(
    TARGETS ${BIN}
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
)
