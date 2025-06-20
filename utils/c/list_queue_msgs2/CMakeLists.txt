##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(list_queue_msgs2 C)

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
    list_queue_msgs2.c
)

##############################################
#   yuno
##############################################
add_executable(${PROJECT_NAME} ${YUNO_SRCS} ${YUNO_HDRS})

target_link_libraries(${PROJECT_NAME}
    ${YUNETAS_KERNEL_LIBS}
    ${YUNETAS_EXTERNAL_LIBS}
    ${YUNETAS_PCRE_LIBS}
)

if(CONFIG_YTLS_USE_MBEDTLS)
    target_link_libraries(${PROJECT_NAME}
        ${MBEDTLS_LIBS}
    )
endif()

if(CONFIG_YTLS_USE_OPENSSL)
    target_link_libraries(${PROJECT_NAME}
        ${OPENSSL_LIBS}
    )
endif()

if(CONFIG_DEBUG_WITH_BACKTRACE)
    target_link_libraries(${PROJECT_NAME}
        ${DEBUG_LIBS}
    )
endif()

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
install(
    TARGETS ${PROJECT_NAME}
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION "/yuneta/bin"
)
