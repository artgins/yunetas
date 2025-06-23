##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(test_timeranger2 C)
get_filename_component(current_directory_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)

#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#-----------------------------------------------------#
if(DEFINED ENV{YUNETAS_BASE})
    set(YUNETAS_BASE $ENV{YUNETAS_BASE})
else()
    set(YUNETAS_BASE "/yuneta/development/yunetas")
endif()
include("${YUNETAS_BASE}/tools/cmake/project.cmake")

##############################################
#   Source
##############################################
set(SRCS
    test_tranger_startup
    test_topic_pkey_integer
    test_topic_pkey_integer_iterator
    test_topic_pkey_integer_iterator2
    test_topic_pkey_integer_iterator3
    test_topic_pkey_integer_iterator4
    test_topic_pkey_integer_iterator5
    test_topic_pkey_integer_iterator6
    test_testing
)

##############################################
#   Tests
##############################################
foreach(test ${SRCS})
    set(binary "${test}")
    add_yuno_executable(${binary} "${test}.c")

    target_link_libraries(${binary}
        ${YUNETAS_KERNEL_LIBS}
        ${YUNETAS_EXTERNAL_LIBS}
        ${YUNETAS_PCRE_LIBS}
        ${OPENSSL_LIBS}
        ${MBEDTLS_LIBS}
        ${DEBUG_LIBS}
    )
    add_test("${current_directory_name}/${test}" ${binary})

endforeach()
