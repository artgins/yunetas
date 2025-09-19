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

#------------------------------------------------------#
#           Source
#------------------------------------------------------#
set(srcs
    src/00_http_parser.c
    src/command_parser.c
    src/dl_list.c
    src/gbmem.c
    src/gbuffer.c
    src/g_ev_kernel.c
    src/glogger.c
    src/gobj.c
    src/g_st_kernel.c
    src/helpers.c
    src/json_config.c
    src/json_replace_vars.c
    src/kwid.c
    src/log_udp_handler.c
    src/replace_string.c
    src/rotatory.c
    src/stats_parser.c
    src/testing.c
)

set(include
    src
)

set(requires
    esp_event
    esp_jansson
)

#------------------------------------------------------#
#           Register
#------------------------------------------------------#
idf_component_register(
    SRCS ${srcs}
    REQUIRES ${requires}
    INCLUDE_DIRS ${include} ${INC_DEST_DIR}
)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_CONFIG_H=1")
#target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format-truncation")
