##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(keycloak_pkey_to_jwks C)

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
SET (YUNO_SRCS
    keycloak_pkey_to_jwks.c
)

##############################################
#   yuno
##############################################
add_yuno_executable(${PROJECT_NAME} ${YUNO_SRCS})

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
    ${OPENSSL_LIBS}
    ${MBEDTLS_LIBS}
    ${DEBUG_LIBS}
)


#if(ESP32_MODE)
#    target_link_libraries(${PROJECT_NAME}
#        /yuneta/development/outputs/lib/libyunetas-core-linux.a
## NO resuelto       /yuneta/development/outputs/lib/libyunetas-esp32.a   # To test partially esp32 you can include this
#    )
#else()
#    target_link_libraries(${PROJECT_NAME}
#        /yuneta/development/outputs/lib/libyunetas-core-linux.a
#    )
#endif()

target_link_options(${PROJECT_NAME} PUBLIC LINKER:-Map=${PROJECT_NAME}.map)

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


# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# or compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
