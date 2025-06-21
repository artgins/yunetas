##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(test_tr_treedb)

#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#-----------------------------------------------------#
if(DEFINED ENV{YUNETAS_BASE})
  set(YUNETAS_BASE_DIR $ENV{YUNETAS_BASE})
else()
  set(YUNETAS_BASE_DIR "/yuneta/development/yunetas")
endif()
include("${YUNETAS_BASE_DIR}/tools/cmake/project.cmake")

##############################################
#   Source
##############################################
set(SRCS
    test_tr_treedb.c
    test_departments.c
    test_users.c
    test_compound.c
    foto_final_departments.c
    foto_final_users.c
    users_file.c
    foto_final1.c
    foto_final2.c
)

##############################################
#   Binary
##############################################
add_yuno_executable(${PROJECT_NAME} ${SRCS})
target_link_libraries(${PROJECT_NAME}
    ${YUNETAS_KERNEL_LIBS}
    ${YUNETAS_EXTERNAL_LIBS}
    ${YUNETAS_PCRE_LIBS}
    ${OPENSSL_LIBS}
    ${MBEDTLS_LIBS}
    ${DEBUG_LIBS}
)

##############################################
#   Test
##############################################
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
