/****************************************************************************
 *              yunetas.h
 *              Includes
 *              Copyright (c) 2024- Niyamaka.
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
#include <gobj.h>

#include <ansi_escape_codes.h>
#include <command_parser.h>
#include <ghttp_parser.h>
#include <helpers.h>
#include <istream.h>
#include <json_config.h>
#include <msg_ievent.h>
#include <kwid.h>
#include <log_udp_handler.h>
#include <stacktrace_with_backtrace.h>
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
#include <c_task.h>
#include <c_task_authz.h>
#include <c_tranger.h>
#include <c_treedb.h>
#include <dbsimple.h>
#include <yev_loop.h>
#include <entry_point.h>
#include <rotatory.h>
#include <ydaemon.h>
#include <yunetas_environment.h>
#include <yev_loop.h>
#include <yunetas_register.h>

#include <fs_watcher.h>
#include <timeranger2.h>
#include <tr_msg.h>
#include <tr_msg2db.h>
#include <tr_queue.h>
#include <tr_treedb.h>

#include <c_ota.h>
#include <c_prot_modbus_m.h>
#include <c_prot_tcp4h.h>

/*
 *  Services
 */
//#include "c_treedb.h"
//#include "c_tranger.h"
//#include "c_node.h"
//#include "c_resource.h"
//#include "c_resource2.h"
//#include "c_ievent_srv.h"
//#include "c_ievent_cli.h"

/*
 *  Gadgets
 */
//#include "c_mqiogate.h"
//#include "c_qiogate.h"
//#include "c_iogate.h"
//#include "c_channel.h"
//#include "c_counter.h"
//#include "c_task.h"
//#include "c_dynrule.h"
//#include "c_timetransition.h"
//#include "c_rstats.h"

/*
 *  Protocols
 */
//#include "c_connex.h"
//#include "c_websocket.h"
//#include "c_prot_header4.h"
//#include "c_prot_raw.h"
//#include "c_prot_http.h"
//#include "c_prot_http_srv.h"
//#include "c_prot_http_cli.h"
//#include "c_serial.h"

/*
 *  Mixin io_uring-gobj
 */

//#include "c_tcp0.h"
//#include "c_tcp_s0.h"
//#include "c_udp_s0.h"
//#include "c_udp0.h"
//#include "c_timer.h"
//#include "c_fs.h"

/*
 *  Decoders
 */
//#include "ghttp_parser.h"


#ifdef __cplusplus
}
#endif
