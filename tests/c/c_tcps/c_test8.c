/***********************************************************************
 *          C_TEST8.C
 *
 *          C_TCP and what a subscriber does from inside its events.
 *
 *          The connections are accepted by the legacy method into channels
 *          with no C_TCP of their own: each gets a VOLATILE C_TCP, whose
 *          host (C_TEST8_HOST, here) destroys it when it publishes
 *          EV_STOPPED. Freed memory is poisoned (main_test8.c), so a use of
 *          the destroyed C_TCP fails every run.
 *
 *          1. TLS, the host drops the connection from its EV_CONNECTED.
 *             That event is published from the read that ended the
 *             handshake: nothing is in flight, the end is synchronous, and
 *             the host destroys the C_TCP inside the publish. Up to 7.25.21
 *             set_secure_connected() went on with the flush of the session
 *             (freed) and of the gobj (freed).
 *          2. Clear, the host answers -1 to each EV_RX_DATA (its own
 *             error). The client sends two messages: both must arrive. Up
 *             to 7.25.21 a non-zero answer stopped the reading, and the
 *             connection hung in silence, neither read nor stopped.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test8.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void test_fail(hgobj gobj, const char *what);
PRIVATE int count_clisrvs_left(const char *service);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE int host_rx_messages = 0;       // EV_RX_DATA the hosts of the clear server got

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

PRIVATE sdata_desc_t host_attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_STRING,      "mode",             SDF_RD,             "",         "drop_on_connect: EV_DROP from EV_CONNECTED; refuse_rx: -1 to each EV_RX_DATA"),
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
} PRIVATE_DATA;

typedef struct _HOST_PRIVATE_DATA {
    BOOL drop_on_connect;
} HOST_PRIVATE_DATA;





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

    gobj_start_tree(gobj_find_service("__input_side_tls__", TRUE));
    gobj_start_tree(gobj_find_service("__input_side_plain__", TRUE));
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
        "host_rx",      "%d", host_rx_messages,
        NULL
    );
}

/***************************************************************************
 *  The C_TCP left in the channels of a server
 ***************************************************************************/
PRIVATE int count_clisrvs_left(const char *service)
{
    hgobj input_side = gobj_find_service(service, TRUE);
    json_t *dl_tcp = gobj_match_children_tree(
        input_side,
        json_pack("{s:s}", "__gclass_name__", C_TCP)
    );
    int n = (int)json_array_size(dl_tcp);
    gobj_free_iter(dl_tcp);
    return n;
}

/***************************************************************************
 *  A message from the clear client
 ***************************************************************************/
PRIVATE void client_send(hgobj gobj, const char *text)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(strlen(text), strlen(text));
    gbuffer_append_string(gbuf, text);
    gobj_send_event(
        priv->client_plain,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A client connected: the TLS one is dropped by its host; the clear one
 *  sends its first message
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->client_plain) {
        client_send(gobj, "one");
        set_timeout(priv->timer, 300);  // then the second
    }

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
        if(priv->phase == 1) {
            gobj_stop(priv->client_tls);    // do not reconnect
            gobj_start(priv->client_plain);
            priv->phase = 2;
            set_timeout(priv->timer, 3000);
        }
    } else if(src == priv->client_plain && priv->phase < 4) {
        test_fail(gobj, "TEST: the clear connection was dropped");
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The servers answer nothing
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    test_fail(gobj, "TEST: a server answered");

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

        case 2:
            client_send(gobj, "two");
            priv->phase = 3;
            set_timeout(priv->timer, 500);
            break;

        case 3:
            priv->phase = 4;
            if(host_rx_messages != 2) {
                test_fail(gobj, "TEST: the host refused the first message and the second did not arrive");
            } else if(count_clisrvs_left("__input_side_tls__") != 0) {
                test_fail(gobj, "TEST: the volatile clisrv of the TLS server is left in the channels");
            } else {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "TEST: the drop from EV_CONNECTED destroyed the clisrv, and a refused EV_RX_DATA did not stop the reading",
                    NULL
                );
            }
            set_yuno_must_die();
            break;

        default:
            test_fail(gobj, "TEST: the TLS connection was not dropped");
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A client stopped: a pure child, nothing to do
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}




                    /***************************
                     *      Host of the channels
                     ***************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void host_mt_create(hgobj gobj)
{
    HOST_PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->drop_on_connect = strcmp(gobj_read_str_attr(gobj, "mode"), "drop_on_connect") == 0;
}

/***************************************************************************
 *  Drop the connection from its EV_CONNECTED (drop_on_connect)
 ***************************************************************************/
PRIVATE int host_ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    HOST_PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->drop_on_connect) {
        gobj_send_event(src, EV_DROP, 0, gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int host_ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Count, and answer an error of its own (refuse_rx)
 ***************************************************************************/
PRIVATE int host_ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    host_rx_messages++;

    KW_DECREF(kw)
    return -1;
}

/***************************************************************************
 *  The volatile C_TCP stopped: its host destroys it
 ***************************************************************************/
PRIVATE int host_ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)

    if(gobj_is_volatil(src)) {
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
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

PRIVATE const GMETHODS host_gmt = {
    .mt_create = host_mt_create,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST8);
GOBJ_DEFINE_GCLASS(C_TEST8_HOST);

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
 *  The host of the channels, where the volatile C_TCPs are put
 ***************************************************************************/
PRIVATE int create_host_gclass(gclass_name_t gclass_name)
{
    ev_action_t st_idle[] = {
        {EV_CONNECTED,              host_ac_connected,          0},
        {EV_DISCONNECTED,           host_ac_disconnected,       0},
        {EV_RX_DATA,                host_ac_rx_data,            0},
        {EV_TX_READY,               0,                          0},
        {EV_STOPPED,                host_ac_stopped,            0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_CONNECTED,              0},
        {EV_DISCONNECTED,           0},
        {EV_RX_DATA,                0},
        {EV_TX_READY,               0},
        {EV_STOPPED,                0},
        {0, 0}
    };

    hgclass gclass = gclass_create(
        gclass_name,
        event_types,
        states,
        &host_gmt,
        0,  // lmt,
        host_attrs_table,
        sizeof(HOST_PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level,
        0   // gcflag_t
    );
    if(!gclass) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test8(void)
{
    int ret = create_gclass(C_TEST8);
    ret += create_host_gclass(C_TEST8_HOST);
    return ret;
}
