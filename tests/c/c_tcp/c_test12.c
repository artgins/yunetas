/***********************************************************************
 *          C_TEST12.C
 *
 *          A class to test C_TCP: a local stop of a TLS C_TCP says the TLS
 *          output it drops.
 *
 *          A TLS client (`tcps://`) connects to a listener of the test; the
 *          test accepts the connection and gives it to a TLS clisrv (legacy
 *          method: its fd_clisrv, its `use_ssl` and the `ytls` of the
 *          test), so both ends are C_TCPs of this yuno. With TLS 1.3 the
 *          client ends its handshake writing its Finished: that write is in
 *          flight when its EV_CONNECTED comes. There the test sends it a
 *          message: it is encrypted at once, and its record waits in the
 *          queue of the TLS output (dl_tx_encrypted) behind the Finished;
 *          the queue of the messages (dl_tx) stays empty. The stop comes in
 *          the next turn of the loop, before the completion of the Finished
 *          (a posted event is delivered before the next completion): it
 *          drops the record and says it in ONE INFO (*"tcp data queued at
 *          a local stop, dropped"*): `dropped_msgs` 0, `dropped_bytes` 0,
 *          and `dropped_tls_bytes`, the record, more than the 26 bytes of
 *          the text. The record is not a message of the host:
 *          txMsgsDroppedAtStop stays 0. (A message waiting in dl_tx is
 *          test9's.) Up to 7.30.26 the TLS output went in silence.
 *
 *              1. The client connects; the test accepts the connection
 *                 (polled every 10 ms) and starts the clisrv; the client's
 *                 EV_CONNECTED -> the message, and the stop posted;
 *                 not connected in 3 s: the test fails
 *              2. The stop: the INFO, the stat
 *              3. 500 ms later the yuno dies
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
#include "c_test12.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define ENCRYPTED_TEXT  "encrypted, waits its write"    // 26 bytes, its TLS record is queued

enum {
    PHASE_ACCEPT = 0,       // the accept of the client's connection, polled
    PHASE_CONNECT,          // waiting the client's EV_CONNECTED
    PHASE_STOPPED,          // the stop happened
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
    hgobj gobj_tcp;
    hgobj gobj_clisrv;
    hytls ytls_server;
    int fd_listen;
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
     *  A listener of the test, non-blocking, on a port of the kernel's choice
     */
    char url[64] = "";
    priv->fd_listen = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
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
        snprintf(url, sizeof(url), "tcps://127.0.0.1:%d", (int)ntohs(addr.sin_port));
    }

    priv->gobj_tcp = gobj_create(
        "tls_client",
        C_TCP,
        json_pack("{s:s, s:{s:s, s:s, s:s}}",
            "url", url,
            "crypto",
                "library", TLS_LIBRARY_NAME,
                "ssl_trusted_certificate", "/yuneta/agent/certs/localhost.crt",
                "ssl_server_name", "localhost"
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
    EXEC_AND_RESET(ytls_cleanup, priv->ytls_server)  // the clisrv, a child, is gone
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

    priv->watchdog = start_msectimer(3000);     // the connection must not take this long
    gobj_start(priv->gobj_tcp);
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
 *  The client's connection, accepted by the test, goes to a TLS clisrv.
 *  0: accepted, 1: not yet, -1: error (logged)
 ***************************************************************************/
PRIVATE int accept_the_client(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int fd = accept4(priv->fd_listen, NULL, NULL, SOCK_NONBLOCK);
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

    priv->gobj_clisrv = gobj_create(
        "tls_clisrv",
        C_TCP,
        json_pack("{s:b, s:I}",
            "__clisrv__", 1,
            "fd_clisrv", (json_int_t)fd
        ),
        gobj
    );
    gobj_write_bool_attr(priv->gobj_clisrv, "use_ssl", TRUE);
    gobj_write_pointer_attr(priv->gobj_clisrv, "ytls", priv->ytls_server);
    gobj_start(priv->gobj_clisrv);
    return 0;
}

/***************************************************************************
 *  The message, while the Finished of the handshake is in flight
 ***************************************************************************/
PRIVATE void send_the_message(hgobj gobj, hgobj gobj_tcp)
{
    gbuffer_t *gbuf = gbuffer_create(64, 64);
    gbuffer_append_string(gbuf, ENCRYPTED_TEXT);
    gobj_send_event(
        gobj_tcp,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The client connected: its Finished is in flight. The message now;
 *  the stop in the next turn, not inside the publication of EV_CONNECTED
 *  (a stop of the publisher there is an error of its own)
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_tcp && priv->phase == PHASE_CONNECT) {
        send_the_message(gobj, priv->gobj_tcp);
        priv->phase = PHASE_STOPPED;
        gobj_post_event(gobj, EV_STOP_THE_CLIENT, json_object(), gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The stop: the queues dropped, said and counted
 ***************************************************************************/
PRIVATE int ac_stop_the_client(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->gobj_tcp);
    json_int_t counted = gobj_read_integer_attr(priv->gobj_tcp, "txMsgsDroppedAtStop");
    if(counted != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: txMsgsDroppedAtStop counts TLS output as messages",
            "counted",      "%ld", (long)counted,
            NULL
        );
    }
    if(!gobj_in_this_state(priv->gobj_tcp, ST_WAIT_STOPPED)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: the Finished of the handshake was not in flight at the stop",
            "state",        "%s", gobj_current_state(priv->gobj_tcp),
            NULL
        );
    }
    priv->phase = PHASE_END;
    set_timeout(priv->timer, 500);

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
        case PHASE_ACCEPT:
            {
                int accepted = accept_the_client(gobj);
                if(accepted == 0) {
                    priv->phase = PHASE_CONNECT;
                }
                if(accepted < 0) {
                    set_yuno_must_die();    // Error already logged
                } else if(test_msectimer(priv->watchdog)) {
                    test_fail(gobj, "TEST: the TLS client did not connect");
                    set_yuno_must_die();
                } else {
                    set_timeout(priv->timer, 10);
                }
            }
            break;

        case PHASE_CONNECT:
            if(test_msectimer(priv->watchdog)) {
                test_fail(gobj, "TEST: the TLS client did not connect");
                set_yuno_must_die();
            } else {
                set_timeout(priv->timer, 10);
            }
            break;

        default:
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: the TLS C_TCP was stopped with TLS output queued",
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
GOBJ_DEFINE_GCLASS(C_TEST12);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_STOP_THE_CLIENT);

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
        {EV_STOP_THE_CLIENT,    ac_stop_the_client, 0},
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
        {EV_STOP_THE_CLIENT,    0},
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
PUBLIC int register_c_test12(void)
{
    return create_gclass(C_TEST12);
}
