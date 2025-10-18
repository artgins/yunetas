##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(yunetas-gobj C)

#-----------------------------------------------------#
#   Resolve YUNETAS_BASE
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#   - else default "/yuneta/development"
#-----------------------------------------------------#
if(DEFINED ENV{YUNETAS_BASE} AND IS_DIRECTORY "$ENV{YUNETAS_BASE}")
    set(YUNETAS_BASE "$ENV{YUNETAS_BASE}")
elseif(IS_DIRECTORY "/yuneta/development/yunetas")
    set(YUNETAS_BASE "/yuneta/development/yunetas")
elseif(IS_DIRECTORY "/yuneta/development")
    set(YUNETAS_BASE "/yuneta/development")
else()
    message(FATAL_ERROR
        "YUNETAS_BASE not found.\n"
        "Set the environment variable YUNETAS_BASE to a valid directory, "
        "or ensure /yuneta/development[/yunetas] exists.")
endif()

message(WARNING "Using YUNETAS_BASE: ${YUNETAS_BASE}")

# Ensure the expected cmake file exists
set(_yunetas_project_cmake "${YUNETAS_BASE}/tools/cmake/project.cmake")
if(NOT EXISTS "${_yunetas_project_cmake}")
    message(FATAL_ERROR "Missing: ${_yunetas_project_cmake}")
endif()

include("${_yunetas_project_cmake}")

##############################################
#   Source
##############################################
set (SRCS
    src/00_http_parser.c
    src/dl_list.c
    src/gobj.c
    src/gbmem.c
    src/gbuffer.c
    src/glogger.c
    src/log_udp_handler.c
    src/helpers.c
    src/kwid.c
    src/gbuffer.c
    src/command_parser.c
    src/stats_parser.c
    src/testing.c
    src/json_config.c
    src/json_replace_vars.c
    src/rotatory.c
    src/replace_string.c
    src/g_ev_kernel.c
    src/g_st_kernel.c
)

set (HDRS
    src/ansi_escape_codes.h
    src/00_http_parser.h
    src/00_security.h
    src/gtypes.h
    src/dl_list.h
    src/gobj.h
    src/gbmem.h
    src/gbuffer.h
    src/glogger.h
    src/msgsets.h
    src/log_udp_handler.h
    src/helpers.h
    src/kwid.h
    src/command_parser.h
    src/stats_parser.h
    src/testing.h
    src/rotatory.h
    src/replace_string.h
    src/g_ev_kernel.h
    src/g_st_kernel.h
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
