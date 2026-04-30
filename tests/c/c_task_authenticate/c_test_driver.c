/****************************************************************************
 *          C_TEST_DRIVER.C
 *
 *          Shared driver for the c_task_authenticate test suite.
 *          See the companion .h for the public surface and contract.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>

#include "c_test_driver.h"

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-------------flag----default-description*/
SDATA (DTP_INTEGER,   "expected_result", SDF_RD, "0",   "Expected EV_ON_TOKEN result field"),
SDATA (DTP_POINTER,   "user_data",     0,      0,      "user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",    "Trace test driver flow"},
    {0, 0}
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj   timer;
    hgobj   gobj_task;      /* C_TASK_AUTHENTICATE service we drive */
    BOOL    test_passed;
    BOOL    dying;          /* TRUE once a death-timer is armed */
} PRIVATE_DATA;




            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /* Give the __idp_side__ listen socket a moment to come up */
    set_timeout(priv->timer, 500);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Timer fired.  Two roles, distinguished by priv->dying:
 *
 *    Initial role (dying == FALSE)
 *        Locate the task-authenticate service, subscribe to EV_ON_TOKEN,
 *        and start it.  The task fires its OIDC chain immediately and
 *        will publish EV_ON_TOKEN exactly once.
 *
 *    Death role (dying == TRUE)
 *        100 ms grace period after stopping the task elapsed; safe to die.
 ***************************************************************************/
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->dying) {
        JSON_DECREF(kw)
        set_yuno_must_die();
        return 0;
    }

    priv->gobj_task = gobj_find_service("task-authenticate", FALSE);
    if(!priv->gobj_task) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test_driver: task-authenticate service not found",
            NULL
        );
        priv->dying = TRUE;
        set_timeout(priv->timer, 100);
        JSON_DECREF(kw)
        return 0;
    }

    gobj_subscribe_event(priv->gobj_task, EV_ON_TOKEN, 0, gobj);
    gobj_start(priv->gobj_task);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_ON_TOKEN: the only output event of c_task_authenticate.  Validate
 *  that its `result` field equals expected_result and die.
 ***************************************************************************/
PRIVATE int ac_on_token(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int got      = (int)kw_get_int(gobj, kw, "result", 99, 0);
    int expected = (int)gobj_read_integer_attr(gobj, "expected_result");

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "test_driver: EV_ON_TOKEN result=%d (expected=%d) comment=%s",
            got, expected,
            kw_get_str(gobj, kw, "comment", "", 0)
        );
    }

    if(got != expected) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test_driver: EV_ON_TOKEN result mismatch",
            "expected", "%d", expected,
            "got",      "%d", got,
            "comment",  "%s", kw_get_str(gobj, kw, "comment", "", 0),
            NULL
        );
    } else {
        priv->test_passed = TRUE;
    }

    KW_DECREF(kw)

    /*
     *  Stop the task service now (while io_uring is still spinning) and
     *  arm a 100 ms death-timer so close events propagate before the
     *  framework destroys the still-RUNNING server-side stacks.
     */
    if(priv->gobj_task && gobj_is_running(priv->gobj_task)) {
        gobj_stop_tree(priv->gobj_task);
    }
    hgobj idp_side = gobj_find_service("__idp_side__", FALSE);
    if(idp_side) {
        gobj_stop_tree(idp_side);
    }

    priv->dying = TRUE;
    set_timeout(priv->timer, 100);
    return 0;
}

/***************************************************************************
 *  Volatil child stopped: destroy it.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *      FSM
                     ***************************/




PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start  = mt_start,
    .mt_stop   = mt_stop,
    .mt_play   = mt_play,
    .mt_pause  = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_DRIVER);

/***************************************************************************
 *
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

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,        ac_timer,       0},
        {EV_ON_TOKEN,       ac_on_token,    0},
        {EV_STOPPED,        ac_stopped,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,        0},
        {EV_ON_TOKEN,       0},
        {EV_STOPPED,        0},
        {0, 0}
    };

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // authz_table
        0,              // command_table
        s_user_trace_level,
        0               // gcflag_t
    );
    if(!__gclass__) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test_driver(void)
{
    return create_gclass(C_TEST_DRIVER);
}
