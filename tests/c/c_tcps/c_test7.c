/***********************************************************************
 *          C_TEST7.C
 *
 *          A class to test C_TCP: a connection accepted by the legacy method
 *          (child_tree_filter) into a channel with no C_TCP of its own is
 *          given a VOLATILE C_TCP, which its host destroys when it publishes
 *          EV_STOPPED (c_websocket's ac_stopped). Here the host drops the
 *          connection from inside the EV_RX_DATA of that C_TCP (a request
 *          that is not HTTP), so the C_TCP is destroyed in its own stack.
 *
 *          Up to 7.25.21 the C_TCP went on using itself after that
 *          EV_STOPPED: set_disconnected() read its url, and over TLS the
 *          read path took ytls's -2222 ("the session was freed inside the
 *          callback") for a TLS error and wrote the cause and stopped the
 *          gobj again. main_test7.c poisons freed memory, so those reads
 *          fail every run.
 *
 *          Tasks
 *          - Start the two servers (TLS and clear, the same pool of
 *            channels: C_CHANNEL -> C_WEBSOCKET, no C_TCP)
 *          - Connect a TLS client; on connect, it sends a request that is
 *            not HTTP, and the server drops it
 *          - Then the same with a clear client
 *          - Both clients must be disconnected, and no clisrv must be left
 *            in the channels
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test7.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NOT_HTTP    "THIS IS NOT HTTP\r\n\r\n"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void test_fail(hgobj gobj, const char *what);
PRIVATE int count_clisrvs_left(void);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_STRING,      "url_tls",          SDF_RD,             "",         "url of the TLS server"),
SDATA (DTP_STRING,      "url_plain",        SDF_RD,             "",         "url of the clear server"),
SDATA (DTP_STRING,      "tls_library",      SDF_RD,             "",         "TLS backend of the TLS client"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj client_tls;
    hgobj client_plain;
    int phase;
    int dropped_tls;
    int dropped_plain;
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

    priv->client_tls = gobj_create_pure_child(
        "client_tls",
        C_TCP,
        json_pack("{s:s, s:b, s:{s:s, s:b, s:b}}",
            "url", gobj_read_str_attr(gobj, "url_tls"),
            "no_tx_ready_event", 1,
            "crypto",
                "library", gobj_read_str_attr(gobj, "tls_library"),
                "ssl_allow_insecure_client", 1,
                "trace", 0
        ),
        gobj
    );
    priv->client_plain = gobj_create_pure_child(
        "client_plain",
        C_TCP,
        json_pack("{s:s, s:b}",
            "url", gobj_read_str_attr(gobj, "url_plain"),
            "no_tx_ready_event", 1
        ),
        gobj
    );
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    if(gobj_is_running(priv->client_tls)) {
        gobj_stop(priv->client_tls);
    }
    if(gobj_is_running(priv->client_plain)) {
        gobj_stop(priv->client_plain);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start_tree(gobj_find_service("__input_side__", TRUE));
    set_timeout(priv->timer, 300);

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
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_fail(hgobj gobj, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "dropped_tls",  "%d", priv->dropped_tls,
        "dropped_plain","%d", priv->dropped_plain,
        NULL
    );
}

/***************************************************************************
 *  The C_TCP left in the channels of the servers: the volatile clisrvs are
 *  destroyed when they stop, so none must be left
 ***************************************************************************/
PRIVATE int count_clisrvs_left(void)
{
    hgobj input_side = gobj_find_service("__input_side__", TRUE);
    json_t *dl_tcp = gobj_match_children_tree(
        input_side,
        json_pack("{s:s}", "__gclass_name__", C_TCP)
    );
    int n = (int)json_array_size(dl_tcp);
    gobj_free_iter(dl_tcp);
    return n;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A client connected: it asks for something that is not HTTP
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = gbuffer_create(strlen(NOT_HTTP), strlen(NOT_HTTP));
    gbuffer_append_string(gbuf, NOT_HTTP);
    gobj_send_event(
        src,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A client dropped by its server
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->client_tls) {
        priv->dropped_tls++;
    } else if(src == priv->client_plain) {
        priv->dropped_plain++;
    }

    if(priv->phase == 1 && src == priv->client_tls) {
        /*
         *  One after the other: the logs of the test come in one order
         */
        gobj_start(priv->client_plain);
        priv->phase = 2;
        set_timeout(priv->timer, 3000);

    } else if(priv->phase == 2 && src == priv->client_plain) {
        priv->phase = 3;
        set_timeout(priv->timer, 300);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The server answered something: not expected, it drops
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    test_fail(gobj, "TEST: a server answered a request that is not HTTP");

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The phases
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase) {
        case 0:
            gobj_start(priv->client_tls);
            priv->phase = 1;
            set_timeout(priv->timer, 3000);     // the connect and the drop must not take this long
            break;

        case 3:
            if(count_clisrvs_left() != 0) {
                test_fail(gobj, "TEST: a volatile clisrv is left in the channels");
            } else {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "TEST: both connections dropped from inside their EV_RX_DATA, their clisrvs destroyed",
                    NULL
                );
            }
            set_yuno_must_die();
            break;

        default:
            test_fail(gobj, "TEST: a connection was not dropped");
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A client stopped (the end of the test): a pure child, nothing to do
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
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
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST7);

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
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_CONNECTED,              ac_connected,               0},
        {EV_DISCONNECTED,           ac_disconnected,            0},
        {EV_RX_DATA,                ac_rx_data,                 0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_CONNECTED,              0},
        {EV_DISCONNECTED,           0},
        {EV_RX_DATA,                0},
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
        s_user_trace_level,
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
PUBLIC int register_c_test7(void)
{
    return create_gclass(C_TEST7);
}
