/***********************************************************************
 *          C_TEST_RESULTS.C
 *
 *          The gobj_results of the tasks of test_c_task, in the place a
 *          C_PROT_HTTP_CL or a postgres client takes in a real task: the
 *          task subscribes to everything it publishes.
 *
 *          - EV_TEST_OPEN      publishes EV_ON_OPEN: the task runs its job
 *          - EV_TEST_ANSWER    publishes EV_ON_MESSAGE: the job's result
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_results.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "Subscriber of output-events"),
SDATA (DTP_BOOLEAN,     "connected",        SDF_RD,             "0",        "Read by C_TASK at its start: not connected, it waits for EV_ON_OPEN"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int nothing;
} PRIVATE_DATA;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




                    /***************************
                     *      Local Methods
                     ***************************/




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_test_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return gobj_publish_event(gobj, EV_ON_OPEN, 0);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_test_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return gobj_publish_event(gobj, EV_ON_MESSAGE, json_pack("{s:i}", "result", 0));
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_RESULTS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TEST_OPEN);
GOBJ_DEFINE_EVENT(EV_TEST_ANSWER);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_TEST_OPEN,              ac_test_open,           0},
        {EV_TEST_ANSWER,            ac_test_answer,         0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_ON_OPEN,                EVF_OUTPUT_EVENT},
        /*
         *  The answer that comes after a task ended has nobody to hear it:
         *  that is what case B of the test sends.
         */
        {EV_ON_MESSAGE,             EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},

        {EV_TEST_OPEN,              0},
        {EV_TEST_ANSWER,            0},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // local methods
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        0, // command_table
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_test_results(void)
{
    return create_gclass(C_TEST_RESULTS);
}
