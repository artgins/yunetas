##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.10)
project(static_musl_test C)

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
    hello.c
)

##############################################
#   Tests
##############################################
add_yuno_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(${PROJECT_NAME}
#    ${YUNETAS_KERNEL_LIBS}
#    ${YUNETAS_EXTERNAL_LIBS}
#    ${YUNETAS_PCRE_LIBS}
#    ${OPENSSL_LIBS}
#    ${MBEDTLS_LIBS}
#    ${DEBUG_LIBS}
)

##############################################
#   Installation
##############################################
install(
    TARGETS ${PROJECT_NAME}
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
)
