/***********************************************************************
 *          C_TEST7.C
 *
 *          A class to test C_TCP: data sent while the connection closes.
 *
 *          A drop (or a disconnection) takes C_TCP to ST_WAIT_STOPPED until
 *          its last io_uring operation completes, and only then publishes
 *          EV_DISCONNECTED. Until then the layers above do not know, and
 *          send: EV_TX_DATA in ST_WAIT_STOPPED, which the state did not
 *          declare -- "Event NOT DEFINED in state", an ERROR per message.
 *          Seen by the thousand on a sim of 300 controllers whose central
 *          went away. The data must go the way the pending queue of a dead
 *          connection goes (away), with no error, and leave ONE warning
 *          for the connection with what was dropped (up to 7.25.20 it went
 *          with no trace at the default levels).
 *
 *          The C_TCP connects to a listener of the test (a plain socket:
 *          the kernel completes the handshake, nobody accepts).
 *
 *              1. EV_CONNECTED -> EV_DROP, then two EV_TX_DATA ("late data",
 *                 "more": 13 bytes), in ST_WAIT_STOPPED (the read is being
 *                 cancelled)
 *              2. 1 s later: the connection must have been dropped
 *                 (EV_DISCONNECTED); the C_TCP is stopped
 *              3. 1 s later: the C_TCP must be in ST_STOPPED
 *              4. A second C_TCP connects, is dropped, and is sent "gone"
 *                 (4 bytes) in ST_WAIT_STOPPED; on the next cycle of the
 *                 loop, still in ST_WAIT_STOPPED, it is DESTROYED. Its
 *                 drop is said too, at the destroy: the close never ended
 *                 through the path that says it.
 *              5. A third C_TCP (timeout_inactivity 300 ms) connects and
 *                 stays idle: closed by its inactivity. The disconnect_cause
 *                 of the first ("Local dropping") and of the third
 *                 ("Inactivity timeout") are read at their EV_DISCONNECTED
 *                 and after the close: the cancel of their read
 *                 (-ECANCELED, "Operation canceled") must not hide them.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "c_test7.h"

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
    hgobj gobj_tcp2;
    hgobj gobj_tcp3;
    char cause1[256];   // disconnect_cause of tcp_client at its EV_DISCONNECTED
    char cause3[256];   // of tcp_client3
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
    priv->gobj_tcp3 = gobj_create(
        "tcp_client3",
        C_TCP,
        json_pack("{s:s, s:i}", "url", url, "timeout_inactivity", 300),
        gobj
    );
    priv->gobj_tcp2 = gobj_create(
        "tcp_client2",
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
    if(priv->gobj_tcp2 && gobj_is_running(priv->gobj_tcp2)) {
        gobj_stop(priv->gobj_tcp2);
    }
    if(gobj_is_running(priv->gobj_tcp3)) {
        gobj_stop(priv->gobj_tcp3);
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
 *  Connected: drop, and send while the drop is in progress
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_tcp3) {
        KW_DECREF(kw)
        return 0;   // it stays idle: closed by its timeout_inactivity
    }

    if(src == priv->gobj_tcp2) {
        gobj_send_event(priv->gobj_tcp2, EV_DROP, 0, gobj);
        gbuffer_t *gbuf = gbuffer_create(16, 16);
        gbuffer_append_string(gbuf, "gone");
        gobj_send_event(
            priv->gobj_tcp2,
            EV_TX_DATA,
            json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
            gobj
        );
        gobj_post_event(gobj, EV_TEST_DESTROY_TCP, json_object(), gobj);
        KW_DECREF(kw)
        return 0;
    }

    priv->connected++;
    if(priv->connected == 1) {
        gobj_send_event(priv->gobj_tcp, EV_DROP, 0, gobj);
        if(!gobj_in_this_state(priv->gobj_tcp, ST_WAIT_STOPPED)) {
            test_fail(gobj, "TEST: the drop did not wait (not in ST_WAIT_STOPPED): nothing tested");
        }
        const char *late[] = {"late data", "more", 0};
        for(int i=0; late[i]; i++) {
            gbuffer_t *gbuf = gbuffer_create(16, 16);
            gbuffer_append_string(gbuf, late[i]);
            gobj_send_event(
                priv->gobj_tcp,
                EV_TX_DATA,
                json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
                gobj
            );
        }
        priv->phase = 1;
        set_timeout(priv->timer, 1000);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The second C_TCP, still closing: destroyed
 ***************************************************************************/
PRIVATE int ac_destroy_tcp(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_in_this_state(priv->gobj_tcp2, ST_WAIT_STOPPED)) {
        test_fail(gobj, "TEST: the second C_TCP is not closing (not in ST_WAIT_STOPPED): nothing tested");
    }
    gobj_stop(priv->gobj_tcp2);     // its stop waits too, in ST_WAIT_STOPPED
    if(!gobj_in_this_state(priv->gobj_tcp2, ST_WAIT_STOPPED)) {
        test_fail(gobj, "TEST: the stop of the second C_TCP did not wait: nothing tested");
    }
    gobj_destroy(priv->gobj_tcp2);
    priv->gobj_tcp2 = 0;

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "TEST: a C_TCP destroyed while closing",
        NULL
    );

    /*
     *  A third C_TCP, closed by its timeout_inactivity
     */
    gobj_start(priv->gobj_tcp3);
    priv->phase = 4;
    set_timeout(priv->timer, 1500);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Why it ended, as the owner reads it at EV_DISCONNECTED
     */
    const char *cause = gobj_read_str_attr(src, "disconnect_cause");
    if(src == priv->gobj_tcp) {
        snprintf(priv->cause1, sizeof(priv->cause1), "%s", cause);
        priv->disconnected++;
    } else if(src == priv->gobj_tcp3) {
        snprintf(priv->cause3, sizeof(priv->cause3), "%s", cause);
    }

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
                test_fail(gobj, "TEST: the drop did not disconnect");
            }
            if(gobj_is_running(priv->gobj_tcp)) {
                gobj_stop(priv->gobj_tcp);
            }
            priv->phase = 2;
            set_timeout(priv->timer, 1000);
            break;

        case 2:
            if(!gobj_in_this_state(priv->gobj_tcp, ST_STOPPED)) {
                test_fail(gobj, "TEST: the stop of the C_TCP did not end (ST_WAIT_STOPPED for ever)");
            } else {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "TEST: the data sent while closing was dropped, and the stop ended",
                    NULL
                );
            }
            gobj_start(priv->gobj_tcp2);
            priv->phase = 3;
            set_timeout(priv->timer, 2000);     // the connect must not take this long
            break;

        case 3:
            test_fail(gobj, "TEST: the second C_TCP did not connect");
            set_yuno_must_die();
            break;

        case 4:
            /*
             *  The cause of each end, at EV_DISCONNECTED and after the close:
             *  the drop and the inactivity, not the cancel of their read
             */
            if(strcmp(priv->cause1, "Local dropping")!=0 ||
                    strcmp(gobj_read_str_attr(priv->gobj_tcp, "disconnect_cause"), "Local dropping")!=0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST: wrong disconnect_cause of a drop",
                    "at_disconnected", "%s", priv->cause1,
                    "after_close",  "%s", gobj_read_str_attr(priv->gobj_tcp, "disconnect_cause"),
                    NULL
                );
            }
            if(strcmp(priv->cause3, "Inactivity timeout")!=0 ||
                    strcmp(gobj_read_str_attr(priv->gobj_tcp3, "disconnect_cause"), "Inactivity timeout")!=0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST: wrong disconnect_cause of an inactivity close",
                    "at_disconnected", "%s", priv->cause3,
                    "after_close",  "%s", gobj_read_str_attr(priv->gobj_tcp3, "disconnect_cause"),
                    NULL
                );
            }
            set_yuno_must_die();
            break;

        default:
            test_fail(gobj, "TEST: unexpected phase");
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
GOBJ_DEFINE_GCLASS(C_TEST7);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TEST_DESTROY_TCP);

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
        {EV_TEST_DESTROY_TCP,   ac_destroy_tcp,     0},
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
        {EV_TEST_DESTROY_TCP,   0},
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
PUBLIC int register_c_test7(void)
{
    return create_gclass(C_TEST7);
}
