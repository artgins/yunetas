##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.0)
include(/yuneta/development/yuneta/yunetas/tools/cmake/project.cmake)
project(yunetas-gobj C)

##############################################
#   Source
##############################################
set (SRCS
    src/00_http_parser.c
    src/ghttp_parser.c
    src/gobj.c
    src/istream.c
    src/parse_url.c
    src/gobj_environment.c
    src/log_udp_handler.c
    src/helpers.c
    src/kwid.c
    src/stacktrace_with_bfd.c
    src/gbuffer.c
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
    src/stacktrace_with_bfd.h
    src/gbuffer.h
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
