/***********************************************************************
 *          C_TEST5.C
 *
 *          A class to test C_TCP: a write that does not start.
 *
 *          The C_TCP (a child of this gclass) connects to a listener of
 *          the test (a plain socket: the kernel completes the handshake,
 *          nobody accepts). Once connected it is given an EMPTY gbuffer to
 *          send: yev_start_event() refuses the write ("Cannot start event:
 *          gbuffer WITHOUT data to write"), the same -1 a write gets when
 *          there is no memory to keep its submission.
 *
 *          A write that does not start never completes. C_TCP must drop
 *          the connection (a stream cannot go on with a hole in it), free
 *          the event and its gbuffer, and a stop must end (ST_STOPPED).
 *          Up to 7.25.4 the -1 was ignored: tx_in_progress counted a write
 *          that never completes, the event and its gbuffer leaked, the
 *          connection stayed up, and the stop waited in ST_WAIT_STOPPED
 *          for ever.
 *
 *              1. EV_CONNECTED -> EV_TX_DATA with an empty gbuffer
 *              2. 1 s later: the connection must have been dropped
 *                 (EV_DISCONNECTED); the C_TCP is stopped
 *              3. 1 s later: the C_TCP must be in ST_STOPPED
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "c_test5.h"

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
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
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
    hgobj gobj_tcp;
    int fd_listen;
    int phase;
    int connected;
    int disconnected;
    int stopped;
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

    /*
     *  A listener of the test, on a port of the kernel's choice
     */
    char url[64] = "";
    priv->fd_listen = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t addrlen = sizeof(addr);
    if(priv->fd_listen < 0 ||
            bind(priv->fd_listen, (struct sockaddr *)&addr, sizeof(addr))<0 ||
            listen(priv->fd_listen, 4)<0 ||
            getsockname(priv->fd_listen, (struct sockaddr *)&addr, &addrlen)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot create the listener",
            NULL
        );
    } else {
        snprintf(url, sizeof(url), "tcp://127.0.0.1:%d", (int)ntohs(addr.sin_port));
    }

    priv->gobj_tcp = gobj_create(
        "tcp_client",
        C_TCP,
        json_pack("{s:s}", "url", url),
        gobj
    );
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd_listen >= 0) {
        close(priv->fd_listen);
        priv->fd_listen = -1;
    }
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

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    if(gobj_is_running(priv->gobj_tcp)) {
        gobj_stop(priv->gobj_tcp);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_tcp);
    set_timeout(priv->timer, 2000);     // the connect must not take this long

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
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_fail(hgobj gobj, const char *what)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        NULL
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Connected: send an EMPTY gbuffer, a write that does not start
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->connected++;
    if(priv->connected == 1) {
        gbuffer_t *gbuf = gbuffer_create(16, 16);
        gobj_send_event(
            priv->gobj_tcp,
            EV_TX_DATA,
            json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
            gobj
        );
        priv->phase = 1;
        set_timeout(priv->timer, 1000);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->disconnected++;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->stopped++;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The other events of the transport: nothing to do
 ***************************************************************************/
PRIVATE int ac_transport_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
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
            test_fail(gobj, "TEST: the C_TCP did not connect");
            set_yuno_must_die();
            break;

        case 1:
            if(priv->disconnected != 1) {
                test_fail(gobj, "TEST: a write that did not start did not drop the connection");
            }
            if(gobj_is_running(priv->gobj_tcp)) {
                gobj_stop(priv->gobj_tcp);
            }
            priv->phase = 2;
            set_timeout(priv->timer, 1000);
            break;

        default:
            if(!gobj_in_this_state(priv->gobj_tcp, ST_STOPPED)) {
                test_fail(gobj, "TEST: the stop of the C_TCP did not end (ST_WAIT_STOPPED for ever)");
            } else {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "TEST: the connection was dropped and the stop ended",
                    NULL
                );
            }
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}




                    /***************************
                     *      FSM
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST5);

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
        {EV_TIMEOUT,            ac_timeout,         0},
        {EV_CONNECTED,          ac_connected,       0},
        {EV_DISCONNECTED,       ac_disconnected,    0},
        {EV_STOPPED,            ac_stopped,         0},
        {EV_RX_DATA,            ac_transport_event, 0},
        {EV_TX_READY,           ac_transport_event, 0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_CONNECTED,          0},
        {EV_DISCONNECTED,       0},
        {EV_STOPPED,            0},
        {EV_RX_DATA,            0},
        {EV_TX_READY,           0},
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
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // authz_table
        0,              // command_table
        s_user_trace_level,
        0               // gcflag_t
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
PUBLIC int register_c_test5(void)
{
    return create_gclass(C_TEST5);
}
