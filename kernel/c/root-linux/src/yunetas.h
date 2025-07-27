/****************************************************************************
 *              yunetas.h
 *              Includes
 *              Copyright (c) 2024- Niyamaka.
 *              Copyright (c) 2025, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#ifdef __cplusplus
extern "C"{
#endif

// TODO this file must be created by menuconfig ???

/*
 *  core
 */
#include <yuneta_version.h>
#include <yuneta_config.h>
#include <gtypes.h>
#include <dl_list.h>
#include <gobj.h>
#include <gbmem.h>
#include <gbuffer.h>
#include <glogger.h>
#include <msgsets.h>

#include <ansi_escape_codes.h>
#include <command_parser.h>
#include <helpers.h>
#include <msg_ievent.h>
#include <kwid.h>
#include <log_udp_handler.h>
#include <stats_parser.h>
#include <testing.h>
#include <ytls.h>

#include <c_yuno.h>         // the grandmother
#include <c_tcp.h>
#include <c_uart.h>
#include <c_timer0.h>
#include <c_timer.h>
#include <c_authz.h>
#include <c_ievent_cli.h>
#include <c_ievent_srv.h>
#include <c_node.h>
#include <c_prot_http_cl.h>
#include <c_prot_http_sr.h>
#include <c_prot_mqtt.h>
#include <c_prot_tcp4h.h>
#include <c_task.h>
#include <c_task_authenticate.h>
#include <c_tranger.h>
#include <c_treedb.h>
#include <c_resource2.h>
#include <c_fs.h>
#include <c_iogate.h>
#include <c_channel.h>
#include <c_websocket.h>
#include <c_qiogate.h>
#include <c_mqiogate.h>
#include <c_udp.h>
#include <c_udp_s.h>
#include <c_gss_udp_s.h>
#include <istream.h>
#include <ghttp_parser.h>
#include <dbsimple.h>
#include <yev_loop.h>
#include <entry_point.h>
#include <rotatory.h>
#include <ydaemon.h>
#include <yunetas_environment.h>
#include <yev_loop.h>
#include <run_command.h>
#include <yunetas_register.h>

#include <fs_watcher.h>
#include <timeranger2.h>
#include <tr_msg.h>
#include <tr_msg2db.h>
#include <tr_queue.h>
#include <tr_treedb.h>

#ifdef __cplusplus
}
#endif
