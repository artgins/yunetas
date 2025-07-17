##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(yuneta_agent C)

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
    src/main.c
    src/c_agent.c
    src/c_pty.c
)
SET (YUNO_HDRS
    src/c_agent.h
    src/c_pty.h
)

##############################################
#   yuno
##############################################
ADD_EXECUTABLE(yuneta_agent ${YUNO_SRCS} ${YUNO_HDRS})

TARGET_LINK_LIBRARIES(yuneta_agent
    /yuneta/development/output/lib/libyuneta-tls.a
    /yuneta/development/output/lib/libyuneta-core.a
    /yuneta/development/output/lib/libginsfsm.a
    /yuneta/development/output/lib/libytls.a
    /yuneta/development/output/lib/libghelpers.a
    /yuneta/development/output/lib/libuv.a
    /yuneta/development/output/lib/libjwt.a
    /yuneta/development/output/lib/libjansson.a
    /yuneta/development/output/lib/libunwind.a
    /yuneta/development/output/lib/libpcre2-posix.a
    /yuneta/development/output/lib/libpcre2-8.a
    /yuneta/development/output/lib/libssl.a
    /yuneta/development/output/lib/libcrypto.a

    pthread dl  # used by libuv
    lzma        # used by libunwind
    z rt m
    uuid
    util        # used by c_pty
)

##############################################
#   Installation
##############################################

install(DIRECTORY service
    USE_SOURCE_PERMISSIONS
    DESTINATION ${BIN_DEST_DIR}
)

install(DIRECTORY certs
    USE_SOURCE_PERMISSIONS
    DESTINATION ${BIN_DEST_DIR}
)

install(FILES
    deploy_agent.sh
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
)
install(FILES
    yuneta_agent.json
    PERMISSIONS
    OWNER_READ OWNER_WRITE
    GROUP_READ GROUP_WRITE
    WORLD_READ
    DESTINATION ${BIN_DEST_DIR}
)

install(FILES
    deploy_remote_agent.sh
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
)

install(
    TARGETS yuneta_agent
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
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
