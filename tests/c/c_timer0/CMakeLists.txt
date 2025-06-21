##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(test_c_timer0 C)

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
SET (YUNO_SRCS
    src/main.c
    src/c_test_timer0.c
)
SET (YUNO_HDRS
    src/c_test_timer0.h
)

##############################################
#   Binary
##############################################
add_yuno_executable(${PROJECT_NAME} ${YUNO_SRCS} ${YUNO_HDRS})
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

##############################################
#   Installation
##############################################
#install(
#    TARGETS ${PROJECT_NAME}
#    PERMISSIONS
#    OWNER_READ OWNER_WRITE OWNER_EXECUTE
#    GROUP_READ GROUP_WRITE GROUP_EXECUTE
#    WORLD_READ WORLD_EXECUTE
#    DESTINATION ${YUNOS_DEST_DIR}
#)

# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# or compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
# Compilando para ARM poner cmake -DNOT_INCLUDE_LIBUNWIND
#
