##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(test_c_tcps C)
get_filename_component(current_directory_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)

#-----------------------------------------------------#
#   Resolve YUNETAS_BASE
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#   - else default "/yuneta/development"
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

# Ensure the expected cmake file exists
set(_yunetas_project_cmake "${YUNETAS_BASE}/tools/cmake/project.cmake")
if(NOT EXISTS "${_yunetas_project_cmake}")
  message(FATAL_ERROR "Missing: ${_yunetas_project_cmake}")
endif()

include("${_yunetas_project_cmake}")

#----------------------------------------#
#   Static binaries
#   To compile as static,
#   also using gcc, set next:
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
    test1
    test2
    test3
    test4
    test5
    test6
    test7
    test8
)

##############################################
#   Tests
##############################################
foreach(test ${SRCS})
    set(binary "test_tcps_${test}")
    add_yuno_executable(${binary} "main_${test}.c" "c_${test}.c")

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

# test6 poisons freed memory (a use after free reads poison, every run)
target_sources(test_tcps_test6 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../emailsender/poison_alloc.c)

# test7 poisons freed memory too (a use of the destroyed clisrv reads poison)
target_sources(test_tcps_test7 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../emailsender/poison_alloc.c)

# test8 too (a use of a clisrv destroyed inside its EV_CONNECTED reads poison)
target_sources(test_tcps_test8 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../emailsender/poison_alloc.c)

# test6 again on mbedTLS when both backends are compiled in (TLS_LIBRARY_NAME
# prefers OpenSSL), on a port of its own
if(CONFIG_HAVE_OPENSSL AND CONFIG_HAVE_MBEDTLS)
    add_yuno_executable(test_tcps_test6_mbedtls
        "main_test6.c"
        "c_test6.c"
        ${CMAKE_CURRENT_SOURCE_DIR}/../emailsender/poison_alloc.c
    )
    target_compile_definitions(test_tcps_test6_mbedtls PRIVATE
        TEST6_TLS_LIBRARY="mbedtls"
        TEST6_PORT="7789"
        TEST6_APP_NAME="test_tcps_test6_mbedtls"
    )
    if(CONFIG_FULLY_STATIC)
        set_target_properties(test_tcps_test6_mbedtls PROPERTIES
            LINK_SEARCH_START_STATIC TRUE
            LINK_SEARCH_END_STATIC TRUE
        )
    endif()
    target_link_libraries(test_tcps_test6_mbedtls
        ${MODULE_TEST}
        ${YUNETAS_KERNEL_LIBS}
        ${YUNETAS_EXTERNAL_LIBS}
        ${YUNETAS_PCRE_LIBS}
        ${JWT_LIBS}
        ${OPENSSL_LIBS}
        ${MBEDTLS_LIBS}
        ${DEBUG_LIBS}
    )
    add_test("${current_directory_name}/test6_mbedtls" test_tcps_test6_mbedtls)

    add_yuno_executable(test_tcps_test7_mbedtls
        "main_test7.c"
        "c_test7.c"
        ${CMAKE_CURRENT_SOURCE_DIR}/../emailsender/poison_alloc.c
    )
    target_compile_definitions(test_tcps_test7_mbedtls PRIVATE
        TEST7_TLS_LIBRARY="mbedtls"
        TEST7_PORT_TLS="7798"
        TEST7_PORT_PLAIN="7799"
        TEST7_APP_NAME="test_tcps_test7_mbedtls"
    )
    if(CONFIG_FULLY_STATIC)
        set_target_properties(test_tcps_test7_mbedtls PROPERTIES
            LINK_SEARCH_START_STATIC TRUE
            LINK_SEARCH_END_STATIC TRUE
        )
    endif()
    target_link_libraries(test_tcps_test7_mbedtls
        ${MODULE_TEST}
        ${YUNETAS_KERNEL_LIBS}
        ${YUNETAS_EXTERNAL_LIBS}
        ${YUNETAS_PCRE_LIBS}
        ${JWT_LIBS}
        ${OPENSSL_LIBS}
        ${MBEDTLS_LIBS}
        ${DEBUG_LIBS}
    )
    add_test("${current_directory_name}/test7_mbedtls" test_tcps_test7_mbedtls)
endif()
