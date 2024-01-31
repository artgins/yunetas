#------------------------------------------------------#
#           ESP32 Source
#------------------------------------------------------#
set(srcs
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
    src/timeranger.c
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
