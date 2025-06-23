##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
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
    test_tr_msg1
    test_tr_msg2
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
    target_link_options(${binary} PUBLIC LINKER:-Map=${PROJECT_NAME}.map)

    add_test("${current_directory_name}/${test}" ${binary})

endforeach()
