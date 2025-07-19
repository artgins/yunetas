##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(test_subscriptions C)
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
SET(SRCS
    test1
    test2
)

SET(COMMON
)

##############################################
#   Tests
##############################################
foreach(test ${SRCS})
    set(binary "test_subs_${test}")
    add_yuno_executable(${binary} "main_${test}.c" "c_${test}.c" ${COMMON})

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
