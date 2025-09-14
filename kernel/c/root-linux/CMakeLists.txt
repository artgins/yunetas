##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(yunetas-core-linux C)

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
set (SRCS
    src/sha1.c
    src/c_yuno.c
    src/c_timer0.c
    src/c_timer.c
    src/c_tcp.c
    src/c_tcp_s.c
    src/c_uart.c
    src/c_channel.c
    src/c_iogate.c
    src/c_qiogate.c
    src/c_mqiogate.c
    src/c_ievent_srv.c
    src/c_ievent_cli.c
    src/c_prot_http_cl.c
    src/c_prot_http_sr.c
    src/c_prot_mqtt.c
    src/c_prot_tcp4h.c
    src/c_websocket.c
    src/c_authz.c
    src/c_task.c
    src/c_task_authenticate.c
    src/c_tranger.c
    src/c_node.c
    src/c_treedb.c
    src/c_resource2.c
    src/c_fs.c
    src/c_prot_raw.c
    src/c_udp.c
    src/c_udp_s.c
    src/c_gss_udp_s.c
    src/c_counter.c
    src/c_pty.c
    src/c_serial.c
    src/ghttp_parser.c
    src/istream.c
    src/yunetas_environment.c
    src/dbsimple.c
    src/entry_point.c
    src/msg_ievent.c
    src/yunetas_register.c
    src/ydaemon.c
    src/run_command.c
)

set (HDRS
    src/c_yuno.h
    src/c_timer0.h
    src/c_timer.h
    src/c_tcp.h
    src/c_tcp_s.h
    src/c_uart.h
    src/c_channel.h
    src/c_iogate.h
    src/c_qiogate.h
    src/c_mqiogate.h
    src/c_ievent_srv.h
    src/c_ievent_cli.h
    src/c_prot_http_cl.h
    src/c_prot_http_sr.h
    src/c_prot_mqtt.h
    src/c_prot_tcp4h.h
    src/c_websocket.h
    src/c_authz.h
    src/c_task.h
    src/c_task_authenticate.h
    src/c_tranger.h
    src/c_node.h
    src/c_treedb.h
    src/c_resource2.h
    src/c_fs.h
    src/c_prot_raw.h
    src/c_udp.h
    src/c_udp_s.h
    src/c_gss_udp_s.h
    src/c_counter.h
    src/c_pty.h
    src/c_serial.h
    src/ghttp_parser.h
    src/istream.h
    src/yunetas_environment.h
    src/dbsimple.h
    src/entry_point.h
    src/msg_ievent.h
    src/yunetas_register.h
    src/ydaemon.h
    src/run_command.h

    src/yunetas.h  # TODO tiene que ser generado por menuconfig
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
