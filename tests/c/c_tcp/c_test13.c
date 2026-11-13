/***********************************************************************
 *          C_TEST13.C
 *
 *          A class to test C_TCP: `txMsgs` counts the messages of the host
 *          in clear and over TLS alike, and `cur_tx_bytes` says the bytes
 *          of the messages it holds to send.
 *
 *          Two clients of the test, one in clear (`tcp://`) and one over
 *          TLS (`tcps://`), each to a listener of its own; the test accepts
 *          each connection and gives it to a clisrv (legacy method: its
 *          fd_clisrv; the TLS one with `use_ssl` and the `ytls` of the
 *          test), so both ends are C_TCPs of this yuno.
 *
 *              1. Both clients connect (not in 3 s: the test fails); 200 ms
 *                 later (the TLS client's Finished is written by then)
 *              2. Each client is sent 5 messages of 1000 bytes in one turn:
 *                 at once `txMsgs` 1 (the first is taken to be written),
 *                 `cur_tx_queue` 4 and `cur_tx_bytes` 5000 (the one being
 *                 written and the four waiting)
 *              3. At its EV_TX_READY (all written): `txMsgs` 5,
 *                 `cur_tx_queue` 0, `cur_tx_bytes` 0. Up to 7.30.26 the TLS
 *                 client's `txMsgs` stayed 0: it was counted only on the
 *                 clear path
 *              4. Each client is sent 3 more messages: `cur_tx_bytes` 3000.
 *                 The clear client is dropped: at its EV_DISCONNECTED
 *                 `cur_tx_bytes` is 0 (the queue went with the
 *                 connection). The TLS client is stopped: right after the
 *                 stop `cur_tx_bytes` is 0
 *              5. 500 ms later the yuno dies
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
#include "c_test13.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MSG_SIZE        1000
#define FIRST_MSGS      5
#define LAST_MSGS       3

enum {
    CLEAR = 0,
    TLS,
    N_PAIRS
};

enum {
    PHASE_ACCEPT = 0,       // the accepts of the clients' connections, polled
    PHASE_CONNECT,          // waiting the clients' EV_CONNECTED
    PHASE_SETTLE,           // both connected, the handshake's last write goes
    PHASE_SENT,             // the first messages sent, waiting their EV_TX_READY
    PHASE_DROPPED,          // the last messages sent, the clear client dropped
    PHASE_END
};

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
    hgobj gobj_tcp[N_PAIRS];
    hgobj gobj_clisrv[N_PAIRS];
    int fd_listen[N_PAIRS];
    BOOL connected[N_PAIRS];
    BOOL tx_ready[N_PAIRS];
    BOOL dropped_disconnected;
    hytls ytls_server;
    int phase;
    uint64_t watchdog;
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
     *  The TLS of the server end
     */
    json_t *jn_crypto = json_pack("{s:s, s:s, s:s}",
        "library", TLS_LIBRARY_NAME,
        "ssl_certificate", "/yuneta/agent/certs/localhost.crt",
        "ssl_certificate_key", "/yuneta/agent/certs/localhost.key"
    );
    priv->ytls_server = ytls_init(gobj, jn_crypto, TRUE);
    JSON_DECREF(jn_crypto)
    if(!priv->ytls_server) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot create the TLS of the server end",
            NULL
        );
    }

    /*
     *  A listener of the test per client, non-blocking, on a port of the
     *  kernel's choice
     */
    for(int i=0; i<N_PAIRS; i++) {
        char url[64] = "";
        priv->fd_listen[i] = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t addrlen = sizeof(addr);
        if(priv->fd_listen[i] < 0 ||
                bind(priv->fd_listen[i], (struct sockaddr *)&addr, sizeof(addr))<0 ||
                listen(priv->fd_listen[i], 4)<0 ||
                getsockname(priv->fd_listen[i], (struct sockaddr *)&addr, &addrlen)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "TEST: cannot create the listener",
                NULL
            );
        } else {
            snprintf(url, sizeof(url), "%s://127.0.0.1:%d",
                i==TLS? "tcps" : "tcp",
                (int)ntohs(addr.sin_port)
            );
        }

        if(i == TLS) {
            priv->gobj_tcp[i] = gobj_create(
                "tls_client",
                C_TCP,
                json_pack("{s:s, s:i, s:{s:s, s:s, s:s}}",
                    "url", url,
                    "timeout_between_connections", -1,
                    "crypto",
                        "library", TLS_LIBRARY_NAME,
                        "ssl_trusted_certificate", "/yuneta/agent/certs/localhost.crt",
                        "ssl_server_name", "localhost"
                ),
                gobj
            );
        } else {
            priv->gobj_tcp[i] = gobj_create(
                "clear_client",
                C_TCP,
                json_pack("{s:s, s:i}",
                    "url", url,
                    "timeout_between_connections", -1
                ),
                gobj
            );
        }
    }
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<N_PAIRS; i++) {
        if(priv->fd_listen[i] >= 0) {
            close(priv->fd_listen[i]);
            priv->fd_listen[i] = -1;
        }
    }
    EXEC_AND_RESET(ytls_cleanup, priv->ytls_server)  // the clisrvs, children, are gone
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
    for(int i=0; i<N_PAIRS; i++) {
        if(priv->gobj_tcp[i] && gobj_is_running(priv->gobj_tcp[i])) {
            gobj_stop(priv->gobj_tcp[i]);
        }
        if(priv->gobj_clisrv[i] && gobj_is_running(priv->gobj_clisrv[i])) {
            gobj_stop(priv->gobj_clisrv[i]);
        }
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->watchdog = start_msectimer(3000);     // the connections must not take this long
    for(int i=0; i<N_PAIRS; i++) {
        gobj_start(priv->gobj_tcp[i]);
    }
    set_timeout(priv->timer, 10);

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
 *  An attribute of a client must have a value, or the test fails
 ***************************************************************************/
PRIVATE void check_attr(hgobj gobj, hgobj gobj_tcp, const char *attr, json_int_t expected, const char *when)
{
    json_int_t value = gobj_read_integer_attr(gobj_tcp, attr);
    if(value != expected) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: a counter of the transport is wrong",
            "client",       "%s", gobj_name(gobj_tcp),
            "attr",         "%s", attr,
            "when",         "%s", when,
            "value",        "%ld", (long)value,
            "expected",     "%ld", (long)expected,
            NULL
        );
    }
}

/***************************************************************************
 *  The connection of a client, accepted by the test, goes to a clisrv.
 *  0: accepted, 1: not yet, -1: error (logged)
 ***************************************************************************/
PRIVATE int accept_the_client(hgobj gobj, int i)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_clisrv[i]) {
        return 0;
    }
    int fd = accept4(priv->fd_listen[i], NULL, NULL, SOCK_NONBLOCK);
    if(fd < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1;
        }
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot accept the client",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    priv->gobj_clisrv[i] = gobj_create(
        i==TLS? "tls_clisrv" : "clear_clisrv",
        C_TCP,
        json_pack("{s:b, s:I}",
            "__clisrv__", 1,
            "fd_clisrv", (json_int_t)fd
        ),
        gobj
    );
    if(i == TLS) {
        gobj_write_bool_attr(priv->gobj_clisrv[i], "use_ssl", TRUE);
        gobj_write_pointer_attr(priv->gobj_clisrv[i], "ytls", priv->ytls_server);
    }
    gobj_start(priv->gobj_clisrv[i]);
    return 0;
}

/***************************************************************************
 *  `n` messages of MSG_SIZE bytes to a client, in one turn
 ***************************************************************************/
PRIVATE void send_messages(hgobj gobj, hgobj gobj_tcp, int n)
{
    char data[MSG_SIZE];
    memset(data, 'x', sizeof(data));

    for(int i=0; i<n; i++) {
        gbuffer_t *gbuf = gbuffer_create(MSG_SIZE, MSG_SIZE);
        gbuffer_append(gbuf, data, sizeof(data));
        gobj_send_event(
            gobj_tcp,
            EV_TX_DATA,
            json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
            gobj
        );
    }
}

/***************************************************************************
 *  Which client is src, or -1
 ***************************************************************************/
PRIVATE int which_client(hgobj gobj, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<N_PAIRS; i++) {
        if(src == priv->gobj_tcp[i]) {
            return i;
        }
    }
    return -1;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A client connected. With both (seen at the next tick), a moment for
 *  the last write of the TLS handshake: its EV_TX_READY is not the one of
 *  the messages
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int i = which_client(gobj, src);
    if(i >= 0) {
        priv->connected[i] = TRUE;  // the clear one, before its accept (the backlog takes it)
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The first messages written whole: counted, nothing held
 ***************************************************************************/
PRIVATE int ac_tx_ready(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int i = which_client(gobj, src);
    if(i >= 0 && priv->phase == PHASE_SENT && !priv->tx_ready[i]) {
        priv->tx_ready[i] = TRUE;
        check_attr(gobj, src, "txMsgs", FIRST_MSGS, "all written");
        check_attr(gobj, src, "cur_tx_queue", 0, "all written");
        check_attr(gobj, src, "cur_tx_bytes", 0, "all written");
        if(priv->tx_ready[CLEAR] && priv->tx_ready[TLS]) {
            // not inside the publication of a client: the drop and the stop come next
            gobj_post_event(gobj, EV_SEND_THE_MESSAGES, json_object(), gobj);
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The last messages: the clear client dropped and the TLS one stopped
 *  with them held
 ***************************************************************************/
PRIVATE int ac_send_the_messages(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<N_PAIRS; i++) {
        send_messages(gobj, priv->gobj_tcp[i], LAST_MSGS);
        check_attr(gobj, priv->gobj_tcp[i], "cur_tx_bytes", LAST_MSGS*MSG_SIZE, "last messages sent");
    }

    priv->phase = PHASE_DROPPED;
    gobj_send_event(priv->gobj_tcp[CLEAR], EV_DROP, 0, gobj);

    gobj_stop(priv->gobj_tcp[TLS]);
    check_attr(gobj, priv->gobj_tcp[TLS], "cur_tx_bytes", 0, "stopped");

    set_timeout(priv->timer, 500);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The dropped client: its queue went with the connection
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_tcp[CLEAR] && priv->phase == PHASE_DROPPED) {
        priv->dropped_disconnected = TRUE;
        check_attr(gobj, src, "cur_tx_bytes", 0, "dropped");
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The other events of the transports: nothing to do
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
        case PHASE_ACCEPT:
            {
                int clear = accept_the_client(gobj, CLEAR);
                int tls = accept_the_client(gobj, TLS);
                if(clear == 0 && tls == 0) {
                    priv->phase = PHASE_CONNECT;
                }
                if(clear < 0 || tls < 0) {
                    set_yuno_must_die();    // Error already logged
                } else if(test_msectimer(priv->watchdog)) {
                    test_fail(gobj, "TEST: the clients did not connect");
                    set_yuno_must_die();
                } else {
                    set_timeout(priv->timer, 10);
                }
            }
            break;

        case PHASE_CONNECT:
            if(priv->connected[CLEAR] && priv->connected[TLS]) {
                priv->phase = PHASE_SETTLE;
                set_timeout(priv->timer, 200);
            } else if(test_msectimer(priv->watchdog)) {
                test_fail(gobj, "TEST: the clients did not connect");
                set_yuno_must_die();
            } else {
                set_timeout(priv->timer, 10);
            }
            break;

        case PHASE_SETTLE:
            priv->phase = PHASE_SENT;
            for(int i=0; i<N_PAIRS; i++) {
                send_messages(gobj, priv->gobj_tcp[i], FIRST_MSGS);
                check_attr(gobj, priv->gobj_tcp[i], "txMsgs", 1, "sent");
                check_attr(gobj, priv->gobj_tcp[i], "cur_tx_queue", FIRST_MSGS-1, "sent");
                check_attr(gobj, priv->gobj_tcp[i], "cur_tx_bytes", FIRST_MSGS*MSG_SIZE, "sent");
            }
            set_timeout(priv->timer, 3000);
            break;

        case PHASE_SENT:
            test_fail(gobj, "TEST: the messages were not written in 3 s");
            set_yuno_must_die();
            break;

        default:
            if(!priv->dropped_disconnected) {
                test_fail(gobj, "TEST: the dropped client did not disconnect");
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: the counters of the clients were checked",
                NULL
            );
            priv->phase = PHASE_END;
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
GOBJ_DEFINE_GCLASS(C_TEST13);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_SEND_THE_MESSAGES);

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
        {EV_TIMEOUT,            ac_timeout,             0},
        {EV_CONNECTED,          ac_connected,           0},
        {EV_TX_READY,           ac_tx_ready,            0},
        {EV_SEND_THE_MESSAGES,  ac_send_the_messages,   0},
        {EV_DISCONNECTED,       ac_disconnected,        0},
        {EV_STOPPED,            ac_transport_event,     0},
        {EV_RX_DATA,            ac_transport_event,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_CONNECTED,          0},
        {EV_TX_READY,           0},
        {EV_SEND_THE_MESSAGES,  0},
        {EV_DISCONNECTED,       0},
        {EV_STOPPED,            0},
        {EV_RX_DATA,            0},
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
PUBLIC int register_c_test13(void)
{
    return create_gclass(C_TEST13);
}
