##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(logcenter C)

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
    src/c_logcenter.c
)
SET (YUNO_HDRS
    src/c_logcenter.h
)

##############################################
#   yuno
##############################################
ADD_EXECUTABLE(logcenter ${YUNO_SRCS} ${YUNO_HDRS})

TARGET_LINK_LIBRARIES(logcenter
    /yuneta/development/output/lib/libyuneta-core.a
    /yuneta/development/output/lib/libginsfsm.a
    /yuneta/development/output/lib/libghelpers.a
    /yuneta/development/output/lib/libuv.a
    /yuneta/development/output/lib/libjansson.a
    /yuneta/development/output/lib/libunwind.a
    /yuneta/development/output/lib/libpcre2-8.a

    pthread dl  # used by libuv
    lzma        # used by libunwind
    uuid
    m
    util
)

##############################################
#   Installation
##############################################
install(
    TARGETS logcenter
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
)
