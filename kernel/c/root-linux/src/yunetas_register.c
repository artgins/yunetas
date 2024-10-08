/****************************************************************************
 *              YUNETA_REGISTER.C
 *              Yuneta
 *
 *              Copyright (c) 2014-2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include "c_linux_yuno.h"         // the grandmother
#include "c_linux_transport.h"
#include "c_linux_uart.h"
#include "c_timer.h"
#include "c_authz.h"
#include "c_ievent_cli.h"
#include "c_node.h"
#include "c_prot_http_cl.h"
#include "c_prot_http_sr.h"
#include "c_task.h"
#include "c_task_authz.h"
#include "c_tranger.h"
#include "c_treedb.h"

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
    register_c_linux_transport();
    register_c_linux_uart();
    register_c_linux_yuno();
    register_c_node();
    register_c_prot_http_cl();
    //register_c_prot_http_sr();
    register_c_task();
    register_c_task_authenticate();
    register_c_timer();
    register_c_tranger();
    register_c_treedb();

    initialized = TRUE;

    return 0;
}
