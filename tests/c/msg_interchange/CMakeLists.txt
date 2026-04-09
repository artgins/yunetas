##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(test_msg_interchange C)
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
#   Build the generic test runner binary
##############################################
set(BINARY_NAME "test_msg_interchange")

add_yuno_executable(${BINARY_NAME}
    main.c
    c_test_runner.c
)

if(CONFIG_FULLY_STATIC)
    set_target_properties(${BINARY_NAME} PROPERTIES
        LINK_SEARCH_START_STATIC TRUE
        LINK_SEARCH_END_STATIC TRUE
    )
endif()

target_link_libraries(${BINARY_NAME}
    ${MODULE_MQTT}
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
#   Discover JSON test files and register
#   one ctest per file
##############################################
file(GLOB TEST_JSON_FILES "${CMAKE_CURRENT_SOURCE_DIR}/json_tests/*.json")
foreach(test_file ${TEST_JSON_FILES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_test(
        NAME "${current_directory_name}/${test_name}"
        COMMAND ${BINARY_NAME} ${test_file}
    )
endforeach()
