/***********************************************************************
 *          C_TEST1.C
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: Use pepon as server and test the tcp client in c_test1
 *          No interchange of messages, only connections
 *
 *          Tasks
 *          - In 1 second, connecting to pepon
 *          - In 1 seconds after connected ,dropping the connection.
 *          - After 3 disconnections, shutdown
 *
 *          Copyright (c) 2024 by ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include "common/c_pepon.h"
#include "c_test1.h"

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
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Timeout"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    json_int_t timeout;
    hgobj timer;

    hgobj pepon;

    hgobj gobj_output_side;
    json_int_t txMsgs;
    json_int_t rxMsgs;
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
    priv->pepon = gobj_create_pure_child("server", C_PEPON, 0, gobj);

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    if(!gobj_is_running(priv->pepon)) {
        gobj_start(priv->pepon);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
    gobj_stop(priv->pepon);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 *  Yuneta rule:
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_play(priv->pepon);

    priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);

    set_timeout(priv->timer, 1000);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_pause(priv->pepon);

    return 0;
}



                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Gps connected
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 1000); // timeout to drop

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Gps disconnected
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    static int i=0;
    i++;

    if(i>2) {
        set_yuno_must_die();
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_connect(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start_tree(priv->gobj_output_side);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_send_event(priv->gobj_output_side, EV_DROP, json_object(), gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_log_info(0, 0,
        "msgset",           "%s", MSGSET_INFO,
        "msg",              "%s", "child stopped",
        "src",              "%s", gobj_full_name(src),
        NULL
    );

    JSON_DECREF(kw)

    if(gobj_is_volatil(src)) {
        gobj_log_info(0, 0,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "child destroyed",
            "src",              "%s", gobj_full_name(src),
            NULL
        );
        gobj_destroy(src);
    }

    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST1);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_closed[] = {
        {EV_TIMEOUT,                ac_timeout_connect,         0},
        {EV_STOPPED,                ac_stopped,                 0},
        {EV_ON_OPEN,                ac_on_open,                 ST_OPENED},
        {0,0,0}
    };
    ev_action_t st_opened[] = {
        {EV_ON_MESSAGE,             0,                          0},
        {EV_ON_CLOSE,               ac_on_close,                ST_CLOSED},
        {EV_TIMEOUT,                ac_timeout_close,           0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_CLOSED,                 st_closed},
        {ST_OPENED,                 st_opened},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_MESSAGE,             0},
        {EV_ON_CLOSE,               0},
        {EV_TIMEOUT,                0},
        {EV_STOPPED,                0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,  // s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test1(void)
{
    return create_gclass(C_TEST1);
}
