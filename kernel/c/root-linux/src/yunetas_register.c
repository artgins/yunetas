/****************************************************************************
 *              YUNETA_REGISTER.C
 *              Yuneta
 *
 *              Copyright (c) 2014-2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
//#include "yuneta.h"
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

//    /*
//     *  Services
//     */
//    gobj_register_gclass(GCLASS_TREEDB);
//    gobj_register_gclass(GCLASS_NODE);
//    gobj_register_gclass(GCLASS_TRANGER);
//    gobj_register_gclass(GCLASS_RESOURCE);
//    gobj_register_gclass(GCLASS_RESOURCE2);
//    gobj_register_gclass(GCLASS_IEVENT_SRV);
//    gobj_register_gclass(GCLASS_IEVENT_CLI);
//
//    /*
//     *  Gadgets
//     */
//    gobj_register_gclass(GCLASS_MQIOGATE);
//    gobj_register_gclass(GCLASS_QIOGATE);
//    gobj_register_gclass(GCLASS_IOGATE);
//    gobj_register_gclass(GCLASS_CHANNEL);
//    gobj_register_gclass(GCLASS_COUNTER);
//    gobj_register_gclass(GCLASS_TASK);
//    gobj_register_gclass(GCLASS_DYNRULE);
//    gobj_register_gclass(GCLASS_TIMETRANSITION);
//    gobj_register_gclass(GCLASS_RSTATS);
//
//    /*
//     *  Protocols
//     */
//    gobj_register_gclass(GCLASS_CONNEX);
//    gobj_register_gclass(GCLASS_GWEBSOCKET);
//    gobj_register_gclass(GCLASS_PROT_HEADER4);
//    gobj_register_gclass(GCLASS_PROT_RAW);
//    gobj_register_gclass(GCLASS_PROT_HTTP);
//    gobj_register_gclass(GCLASS_PROT_HTTP_SRV);
//    gobj_register_gclass(GCLASS_PROT_HTTP_CLI);
//    gobj_register_gclass(GCLASS_SERIAL);
//
//    /*
//     *  Mixin uv-gobj
//     */
//    gobj_register_gclass(GCLASS_GSS_UDP_S0);
//    gobj_register_gclass(GCLASS_TCP0);
//    gobj_register_gclass(GCLASS_TCP_S0);
//    gobj_register_gclass(GCLASS_UDP_S0);
//    gobj_register_gclass(GCLASS_UDP0);
//    gobj_register_gclass(GCLASS_TIMETRANSITION);
//    gobj_register_gclass(GCLASS_TIMER);
//    gobj_register_gclass(GCLASS_FS);
//
//    register_ievent_answer_filter();

    initialized = TRUE;

    return 0;
}
