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
    set(YUNETAS_BASE_DIR $ENV{YUNETAS_BASE})
else()
    set(YUNETAS_BASE_DIR "/yuneta/development/yunetas")
endif()
include("${YUNETAS_BASE_DIR}/tools/cmake/project.cmake")

##############################################
#   Source
##############################################
SET(SRCS
    hello.c
)

##############################################
#   Tests
##############################################
add_executable(${PROJECT_NAME} ${SRCS})

if(AS_STATIC)
    # Enforce full static link flags globally
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_SEARCH_START_STATIC TRUE
        LINK_SEARCH_END_STATIC TRUE
    )
    # These linker flags ensure no shared libs are pulled in
    target_link_options(${PROJECT_NAME} PRIVATE -static -no-pie -Wl,-Bstatic)
endif()

target_link_libraries(${PROJECT_NAME}
    ${YUNETAS_KERNEL_LIBS}
    ${YUNETAS_EXTERNAL_LIBS}
    ${YUNETAS_PCRE_LIBS}
)

if(CONFIG_YTLS_USE_MBEDTLS)
    target_link_libraries(${PROJECT_NAME}
        ${MBEDTLS_LIBS}
    )

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
