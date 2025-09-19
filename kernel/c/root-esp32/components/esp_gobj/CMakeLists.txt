#------------------------------------------------------#
#           ESP32 Source
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
    INCLUDE_DIRS
        src/
)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_CONFIG_H=1")
#target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format-truncation")
