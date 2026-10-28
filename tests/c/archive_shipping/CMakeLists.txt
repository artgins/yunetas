##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(test_archive_shipping C)
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
SET (YUNO_SRCS
    main_test1.c
    c_test1.c
    ${YUNETAS_BASE}/yunos/c/archive_sender/src/c_archive_sender.c
    ${YUNETAS_BASE}/yunos/c/archive_keeper/src/c_archive_keeper.c
)
SET (YUNO_HDRS
    c_test1.h
    test_pki.h
)

##############################################
#   Binary
##############################################
set(binary "test_archive_shipping")
add_yuno_executable(${binary} ${YUNO_SRCS} ${YUNO_HDRS})

# the gclasses of the two yunos are their own sources
target_include_directories(${binary} PRIVATE
    ${YUNETAS_BASE}/yunos/c/archive_sender/src
    ${YUNETAS_BASE}/yunos/c/archive_keeper/src
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

##############################################
#   Test
##############################################
add_test("${current_directory_name}/test1" ${binary})
