/****************************************************************************
 *              YUNETA_REGISTER.C
 *              Yuneta
 *
 *              Copyright (c) 2014-2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include "c_yuno.h"         // the grandmother
#include "c_tcp.h"
#include "c_tcp_s.h"
#include "c_udp.h"
#include "c_udp_s.h"
#include "c_gss_udp_s.h"
#include "c_uart.h"
#include "c_timer0.h"
#include "c_timer.h"
#include "c_authz.h"
#include "c_ievent_cli.h"
#include "c_ievent_srv.h"
#include "c_node.h"
#include "c_prot_http_cl.h"
#include "c_prot_http_sr.h"
#include "c_prot_raw.h"
#include "c_prot_tcp4h.h"
#include "c_prot_mqtt.h"
#include "c_task.h"
#include "c_task_authenticate.h"
#include "c_tranger.h"
#include "c_treedb.h"
#include "c_fs.h"
#include "c_counter.h"
#include "c_pty.h"

#include "c_iogate.h"
#include "c_channel.h"
#include "c_websocket.h"
#include "c_qiogate.h"
#include "c_mqiogate.h"

#include "yunetas_register.h"

#include "c_resource2.h"

/***************************************************************************
 *  Data
 ***************************************************************************/

/***************************************************************************
 *  Register internal yuno gclasses and services
 ***************************************************************************/
PUBLIC int yunetas_register_c_core(void)
{
    static BOOL initialized = FALSE;
    if(initialized) {
        return -1;
    }
    int result = 0;

    // TODO include with menuconfig
    result += register_c_authz();
    result += register_c_ievent_cli();
    result += register_c_ievent_srv();
    result += register_c_tcp();
    result += register_c_tcp_s();
    // result += register_c_udp();
    result += register_c_udp_s();
    result += register_c_gss_udp_s();
    result += register_c_uart();
    result += register_c_yuno();
    result += register_c_node();
    result += register_c_prot_http_cl();
    result += register_c_prot_http_sr();
    result += register_c_prot_mqtt();
    result += register_c_prot_raw();
    result += register_c_prot_tcp4h();
    result += register_c_task();
    result += register_c_task_authenticate();
    result += register_c_timer0();
    result += register_c_timer();
    result += register_c_tranger();
    result += register_c_treedb();
    result += register_c_fs();
    result += register_c_counter();
    result += register_c_pty();

    result += register_c_iogate();
    result += register_c_qiogate();
    result += register_c_mqiogate();
    result += register_c_channel();
    result += register_c_resource2();
    result += register_c_websocket();

    initialized = TRUE;

    return result;
}
