##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(emailsender C)

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
    src/c_emailsender.c
    src/c_curl.c
)
SET (YUNO_HDRS
    src/c_emailsender.h
    src/c_curl.h
)

##############################################
#   yuno
##############################################
ADD_EXECUTABLE(emailsender ${YUNO_SRCS} ${YUNO_HDRS})

TARGET_LINK_LIBRARIES(emailsender
    /yuneta/development/output/lib/libyuneta-core.a
    /yuneta/development/output/lib/libginsfsm.a
    /yuneta/development/output/lib/libghelpers.a
    /yuneta/development/output/lib/libuv.a
    /yuneta/development/output/lib/libjansson.a
    /yuneta/development/output/lib/libunwind.a
    /yuneta/development/output/lib/libpcre2-8.a
    /yuneta/development/output/lib/libcurl.a
    /yuneta/development/output/lib/libssl.a
    /yuneta/development/output/lib/libcrypto.a

    pthread dl  # used by libuv
    lzma        # used by libunwind

    dl z
    uuid
    m
    util
)

# if(LIBIDN_FOUND)
#     target_link_libraries(${PROJECT_NAME} ${LIBIDN_LIBRARIES})
# endif()

##############################################
#   Installation
##############################################
install(
    TARGETS emailsender
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
)
