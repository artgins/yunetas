##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(dba_postgres C)

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
    src/c_dba_postgres.c
)
SET (YUNO_HDRS
    src/c_dba_postgres.h
)

##############################################
#   yuno
##############################################
ADD_EXECUTABLE(dba_postgres ${YUNO_SRCS} ${YUNO_HDRS})

TARGET_LINK_LIBRARIES(dba_postgres
    /yuneta/development/output/lib/libyuneta-postgres.a
    /yuneta/development/output/lib/libyuneta-tls.a
    /yuneta/development/output/lib/libyuneta-core.a
    /yuneta/development/output/lib/libginsfsm.a
    /yuneta/development/output/lib/libytls.a
    /yuneta/development/output/lib/libghelpers.a
    /yuneta/development/output/lib/libuv.a
    /yuneta/development/output/lib/libjwt.a
    /yuneta/development/output/lib/libjansson.a
    /yuneta/development/output/lib/libunwind.a
    /yuneta/development/output/lib/libpcre2-8.a
    /yuneta/development/output/lib/libssl.a
    /yuneta/development/output/lib/libcrypto.a

    pq          # postgres, WARNING this library use openssl of distribution

    pthread dl  # used by libuv
    lzma        # used by libunwind
    uuid
    z rt m
    util
)

##############################################
#   Installation
##############################################
install(
    TARGETS dba_postgres
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
