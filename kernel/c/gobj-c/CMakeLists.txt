##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(yunetas-gobj C)

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
set (SRCS
    src/00_http_parser.c
    src/ghttp_parser.c
    src/gobj.c
    src/glog.c
    src/gobj_environment.c
    src/istream.c
    src/parse_url.c
    src/log_udp_handler.c
    src/helpers.c
    src/kwid.c
    src/gbuffer.c
    src/comm_prot.c
    src/command_parser.c
    src/stats_parser.c
    src/testing.c
    src/json_config.c
    src/replace_string.c
    src/yunetas_ev_loop.c
    src/launch_daemon.c

    src/stacktrace_with_bfd.c
)

set (HDRS
    src/ansi_escape_codes.h
    src/00_http_parser.h
    src/ghttp_parser.h
    src/gobj.h
    src/istream.h
    src/parse_url.h
    src/gobj_environment.h
    src/log_udp_handler.h
    src/helpers.h
    src/kwid.h
    src/comm_prot.h
    src/command_parser.h
    src/stats_parser.h
    src/testing.h
    src/json_config.h
    src/replace_string.h
    src/yunetas_ev_loop.h
    src/launch_daemon.h

    src/stacktrace_with_bfd.h
)

##############################################
#   Lib
##############################################
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

target_link_options(${PROJECT_NAME} PUBLIC LINKER:-Map=${PROJECT_NAME}.map)

##############################################
#   System install
##############################################
install(FILES ${HDRS} DESTINATION ${INC_DEST_DIR})

install(
    TARGETS ${PROJECT_NAME}
    PERMISSIONS
    OWNER_READ OWNER_WRITE
    GROUP_READ GROUP_WRITE
    WORLD_READ
    DESTINATION ${LIB_DEST_DIR}
)

# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# or compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
