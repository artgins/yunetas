##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)

#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#-----------------------------------------------------#
if(NOT DEFINED YUNETAS_BASE)
    if(DEFINED ENV{YUNETAS_BASE})
        set(YUNETAS_BASE "$ENV{YUNETAS_BASE}")
    else()
        set(YUNETAS_BASE "/yuneta/development/yunetas")
    endif()
endif()

include("${YUNETAS_BASE}/tools/cmake/config_prelude.cmake")
project(yuneta_agent C)
include("${YUNETAS_BASE}/tools/cmake/project_common.cmake")

##############################################
#   Source
##############################################
SET (YUNO_SRCS
    src/main.c
    src/c_agent.c
)
SET (YUNO_HDRS
    src/c_agent.h
)

##############################################
#   yuno
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
#   Installation
##############################################

install(DIRECTORY service
    USE_SOURCE_PERMISSIONS
    DESTINATION ${AGENT_DEST_DIR}
)

install(DIRECTORY certs
    USE_SOURCE_PERMISSIONS
    DESTINATION ${AGENT_DEST_DIR}
)

install(FILES
    deploy_agent.sh
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${AGENT_DEST_DIR}
)

install(FILES
    deploy_remote_agent.sh
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${AGENT_DEST_DIR}
)

install(FILES
    yuneta_agent.json
    PERMISSIONS
    OWNER_READ OWNER_WRITE
    GROUP_READ GROUP_WRITE
    WORLD_READ
    DESTINATION ${AGENT_DEST_DIR}
)

install(
    TARGETS ${PROJECT_NAME}
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${AGENT_DEST_DIR}
)

# compile in Release mode :
#
#     cmake -DCMAKE_BUILD_TYPE=Release ..
#
# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
# compile to build static binary :
#
#     cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/yuneta/development/yunetas/tools/cmake/musl-toolchain.cmake ..
#
#
