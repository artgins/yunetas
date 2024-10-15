/****************************************************************************
 *              YUNETA_REGISTER.C
 *              Yuneta
 *
 *              Copyright (c) 2014-2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include "c_yuno.h"         // the grandmother
#include "c_tcp.h"
#include "c_uart.h"
#include "c_timer.h"
#include "c_authz.h"
#include "c_ievent_cli.h"
#include "c_node.h"
#include "c_prot_http_cl.h"
#include "c_prot_http_sr.h"
#include "c_prot_tcp4h.h"
#include "c_task.h"
#include "c_task_authz.h"
#include "c_tranger.h"
#include "c_treedb.h"

#include "c_iogate.h"
#include "c_channel.h"
#include "c_websocket.h"
#include "c_qiogate.h"
#include "c_mqiogate.h"

#include "yunetas_register.h"

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

    // TODO include with menuconfig
    register_c_authz();
    register_c_ievent_cli();
    register_c_tcp();
    register_c_uart();
    register_c_yuno();
    register_c_node();
    register_c_prot_http_cl();
    //register_c_prot_http_sr();
    register_c_prot_tcp4h();
    register_c_task();
    register_c_task_authenticate();
    register_c_timer();
    register_c_tranger();
    register_c_treedb();

    register_c_iogate();
    register_c_channel();
    register_c_websocket();

    initialized = TRUE;

    return 0;
}
