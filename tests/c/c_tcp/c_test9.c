/***********************************************************************
 *          C_TEST9.C
 *
 *          A class to test C_TCP: destroyed with a write in flight.
 *
 *          The C_TCP client (a child of this gclass) connects to a listener
 *          of the test (a plain socket: the kernel completes the handshake,
 *          nobody accepts). A C_TCP clisrv (legacy method: its fd_clisrv
 *          given) holds a connection the test accepted from a socket of its
 *          own, the peer. Once BOTH are connected, in ONE turn, to each:
 *          a write, the stop (it waits for the write and for the cancel of
 *          the read, in ST_WAIT_STOPPED) and the destroy, before
 *          EV_STOPPED -- the way every yuno ends. The destroy cancels the
 *          write, and the loop frees it with no callback. Up to 7.29.3 the
 *          write events were not kept, and the completion of the write ran
 *          on the freed gobj (a use after free: AddressSanitizer names it,
 *          a plain build may go on reading the freed memory, or crash).
 *          And the destroy closes the socket of the clisrv, which the stop
 *          closes when it ends, as the stop does: between the stop and the
 *          destroy the peer sends data nobody reads, and still it must read
 *          the end of the connection (a FIN), not a reset. Up to 7.29.3
 *          the socket was left open, and its peer never saw the end of the
 *          connection (a socket leaked per clisrv destroyed so); in the
 *          fixes after 7.29.3 it was closed with no shutdown(SHUT_WR), and
 *          the unread data made it a RST.
 *
 *              1. EV_CONNECTED of both (the clisrv's comes inside its
 *                 start, the client's when its connect completes) -> 100
 *                 ms later the write, the stop and the destroy, of both;
 *                 not connected in 2 s: the test fails
 *              2. 500 ms later: the peer of the clisrv must read the end of
 *                 the connection, a FIN; the yuno dies; the memory check at
 *                 the end catches a leaked event or gbuffer
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "c_test9.h"

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
    hgobj gobj_clisrv;
    int fd_listen;
    int fd_peer;        // the other end of the clisrv's connection
    int phase;
    BOOL tcp_connected;
    BOOL clisrv_connected;
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
    priv->fd_peer = -1;
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

    /*
     *  The connection of the clisrv: the peer connects to the listener
     *  (the kernel completes the handshake), and the test accepts it
     */
    int fd_clisrv = -1;
    priv->fd_peer = socket(AF_INET, SOCK_STREAM, 0);
    if(priv->fd_peer < 0 ||
            connect(priv->fd_peer, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
            (fd_clisrv = accept(priv->fd_listen, NULL, NULL)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot make the connection of the clisrv",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }

    priv->gobj_clisrv = gobj_create(
        "tcp_clisrv",
        C_TCP,
        json_pack("{s:b, s:I}",
            "__clisrv__", 1,
            "fd_clisrv", (json_int_t)fd_clisrv
        ),
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
    if(priv->fd_peer >= 0) {
        close(priv->fd_peer);
        priv->fd_peer = -1;
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
    if(priv->gobj_tcp && gobj_is_running(priv->gobj_tcp)) {
        gobj_stop(priv->gobj_tcp);
    }
    if(priv->gobj_clisrv && gobj_is_running(priv->gobj_clisrv)) {
        gobj_stop(priv->gobj_clisrv);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 2000);     // the connects must not take this long
    gobj_start(priv->gobj_tcp);
    gobj_start(priv->gobj_clisrv);      // connected at its start: its EV_CONNECTED comes inside

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

/***************************************************************************
 *  A write, the stop and the destroy, in one turn. fd_peer >= 0: between
 *  the stop and the destroy, the peer sends data that nobody reads (the
 *  stop canceled the read)
 ***************************************************************************/
PRIVATE void write_stop_and_destroy(hgobj gobj, hgobj gobj_tcp, int fd_peer)
{
    gbuffer_t *gbuf = gbuffer_create(64, 64);
    gbuffer_append_string(gbuf, "written while it is destroyed");
    gobj_send_event(
        gobj_tcp,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
    gobj_stop(gobj_tcp);
    if(!gobj_in_this_state(gobj_tcp, ST_WAIT_STOPPED)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: the stop with a write in flight did not wait",
            "tcp",          "%s", gobj_short_name(gobj_tcp),
            "state",        "%s", gobj_current_state(gobj_tcp),
            NULL
        );
    }
    if(fd_peer >= 0) {
        const char unread[] = "sent by the peer, never read";
        if(send(fd_peer, unread, sizeof(unread), 0) != (ssize_t)sizeof(unread)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "TEST: the peer cannot send",
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
    }
    gobj_destroy(gobj_tcp);
}

/***************************************************************************
 *  The peer of the clisrv reads what came and then the end of the
 *  connection: "fin" (an orderly end), "reset", or "open"
 ***************************************************************************/
PRIVATE const char *how_the_peer_saw_the_end(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char bf[256];
    while(1) {
        ssize_t n = recv(priv->fd_peer, bf, sizeof(bf), MSG_DONTWAIT);
        if(n > 0) {
            continue;   // the write, if the kernel took it before the destroy
        }
        if(n == 0) {
            return "fin";
        }
        if(errno == ECONNRESET) {
            return "reset";
        }
        return "open";  // EAGAIN: the connection is still open
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Connected: the write, the stop and the destroy come a turn later, not
 *  inside the publication of EV_CONNECTED (a destroy of the publisher there
 *  is an error of its own)
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_tcp) {
        priv->tcp_connected = TRUE;
    } else if(src == priv->gobj_clisrv) {
        priv->clisrv_connected = TRUE;
    }
    if(priv->phase == 0 && priv->tcp_connected && priv->clisrv_connected) {
        priv->phase = 1;
        set_timeout(priv->timer, 100);
    }

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
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST: the C_TCPs did not connect",
                "tcp_connected", "%d", (int)priv->tcp_connected,
                "clisrv_connected", "%d", (int)priv->clisrv_connected,
                NULL
            );
            set_yuno_must_die();
            break;

        case 1:
            write_stop_and_destroy(gobj, priv->gobj_tcp, -1);
            priv->gobj_tcp = 0;
            write_stop_and_destroy(gobj, priv->gobj_clisrv, priv->fd_peer);
            priv->gobj_clisrv = 0;
            priv->phase = 2;
            set_timeout(priv->timer, 500);
            break;

        default:
            {
                const char *end = how_the_peer_saw_the_end(gobj);
                if(strcmp(end, "open")==0) {
                    test_fail(gobj, "TEST: the socket of a clisrv destroyed before its stop ended was not closed");
                } else if(strcmp(end, "reset")==0) {
                    test_fail(gobj, "TEST: the peer of a clisrv destroyed before its stop ended got a reset, not the end");
                }
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: the C_TCP was destroyed with a write in flight",
                NULL
            );
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
GOBJ_DEFINE_GCLASS(C_TEST9);

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
        {EV_DISCONNECTED,       ac_transport_event, 0},
        {EV_STOPPED,            ac_transport_event, 0},
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
PUBLIC int register_c_test9(void)
{
    return create_gclass(C_TEST9);
}
