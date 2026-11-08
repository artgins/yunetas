/***********************************************************************
 *          C_TEST11.C
 *
 *          A class to test C_TCP
 *          Test: the deadline of the TLS handshake of a client, against an
 *          inactivity timer and against many mute peers.
 *
 *          Two listeners of the test, raw TCP, on ports of the kernel's
 *          choice: one drips a TLS record header and its body, one byte
 *          each 200 ms (a handshake that never ends), the other accepts
 *          and says nothing.
 *
 *          - A TLS client with timeout_tls_handshake 1000 AND
 *            timeout_inactivity 2000 connects to the drip: it must be
 *            dropped at its handshake deadline, within 4 s of its accept.
 *            Up to 7.30.17 each byte read re-armed the timer with the
 *            inactivity time, which the drip never let pass: the handshake
 *            stayed open as long as the drip went on (red: "the drip kept
 *            the TLS handshake past its deadline").
 *          - Three TLS clients (timeout_tls_handshake 1000) connect to the
 *            mute listener at the same time: the four deadlines of the
 *            first window write ONE warning ("suppressed" 0).
 *          - 11 s later a fifth client connects to the mute listener: its
 *            deadline writes the next warning, "suppressed" 3. Up to
 *            7.30.17 each deadline wrote a warning (red: "Unexpected
 *            error").
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
#include "c_test11.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MUTE_CLIENTS        3
#define MAX_PEERS           8
#define DRIP_EVERY_MS       200
#define DRIP_BYTES          30          // 6 s of drip: past the deadline and the inactivity time
#define DROP_BEFORE_MS      4000        // the deadline (1 s) + the tick of C_TIMER, with a margin
#define LATE_CLIENT_AFTER_MS 11000      // past the window of the warning (10 s)
#define WATCHDOG_MS         20000

enum {
    PHASE_FIRST_WINDOW = 0, // the drip and the mute clients: their deadlines
    PHASE_WAIT_WINDOW,      // the window of the warning goes by
    PHASE_LATE_CLIENT,      // a mute client after the window: its deadline
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
    hgobj timer;            // the phases (seconds)
    hgobj drip_timer;       // the drip and the accepts (milliseconds)
    hgobj gobj_drip_client;
    hgobj gobj_mute_clients[MUTE_CLIENTS];
    hgobj gobj_late_client;
    int fd_listen_drip;
    int fd_listen_mute;
    int fd_drip_peer;
    int fd_mute_peers[MAX_PEERS];
    int mute_peers;
    int dripped;
    BOOL drip_peer_ended;
    uint64_t drop_before;   // msectimer: the drip's client must be dropped before it
    uint64_t watchdog;
    int phase;
} PRIVATE_DATA;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *  A listener of the test, raw TCP, on a port of the kernel's choice
 ***************************************************************************/
PRIVATE int open_listener(hgobj gobj, char *url, size_t url_size)
{
    int fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t addrlen = sizeof(addr);
    if(fd < 0 ||
            bind(fd, (struct sockaddr *)&addr, sizeof(addr))<0 ||
            listen(fd, 8)<0 ||
            getsockname(fd, (struct sockaddr *)&addr, &addrlen)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot create a listener",
            NULL
        );
        if(fd >= 0) {
            close(fd);
        }
        url[0] = 0;
        return -1;
    }
    snprintf(url, url_size, "tcps://127.0.0.1:%d", (int)ntohs(addr.sin_port));
    return fd;
}

/***************************************************************************
 *  A TLS client of the test, not started
 ***************************************************************************/
PRIVATE hgobj create_tls_client(hgobj gobj, const char *name, const char *url, int timeout_inactivity)
{
    return gobj_create(
        name,
        C_TCP,
        json_pack("{s:s, s:i, s:i, s:i, s:{s:s, s:s, s:s}}",
            "url", url,
            "timeout_tls_handshake", 1000,
            "timeout_inactivity", timeout_inactivity,
            "timeout_between_connections", 60000,
            "crypto",
                "library", TLS_LIBRARY_NAME,
                "ssl_trusted_certificate", "/yuneta/agent/certs/localhost.crt",
                "ssl_server_name", "localhost"
        ),
        gobj
    );
}

/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child("phases", C_TIMER, 0, gobj);
    priv->drip_timer = gobj_create_pure_child("drip", C_TIMER0, 0, gobj);
    priv->fd_drip_peer = -1;
    for(int i = 0; i < MAX_PEERS; i++) {
        priv->fd_mute_peers[i] = -1;
    }

    char url_drip[PATH_MAX];
    char url_mute[PATH_MAX];
    priv->fd_listen_drip = open_listener(gobj, url_drip, sizeof(url_drip));
    priv->fd_listen_mute = open_listener(gobj, url_mute, sizeof(url_mute));

    priv->gobj_drip_client = create_tls_client(gobj, "drip_client", url_drip, 2000);
    for(int i = 0; i < MUTE_CLIENTS; i++) {
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "mute_client%d", i + 1);
        priv->gobj_mute_clients[i] = create_tls_client(gobj, name, url_mute, -1);
    }
    priv->gobj_late_client = create_tls_client(gobj, "late_client", url_mute, -1);
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd_drip_peer >= 0) {
        close(priv->fd_drip_peer);
        priv->fd_drip_peer = -1;
    }
    for(int i = 0; i < MAX_PEERS; i++) {
        if(priv->fd_mute_peers[i] >= 0) {
            close(priv->fd_mute_peers[i]);
            priv->fd_mute_peers[i] = -1;
        }
    }
    if(priv->fd_listen_drip >= 0) {
        close(priv->fd_listen_drip);
        priv->fd_listen_drip = -1;
    }
    if(priv->fd_listen_mute >= 0) {
        close(priv->fd_listen_mute);
        priv->fd_listen_mute = -1;
    }
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->drip_timer);

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
    clear_timeout0(priv->drip_timer);
    gobj_stop(priv->drip_timer);
    if(gobj_is_running(priv->gobj_drip_client)) {
        gobj_stop(priv->gobj_drip_client);
    }
    for(int i = 0; i < MUTE_CLIENTS; i++) {
        if(gobj_is_running(priv->gobj_mute_clients[i])) {
            gobj_stop(priv->gobj_mute_clients[i]);
        }
    }
    if(gobj_is_running(priv->gobj_late_client)) {
        gobj_stop(priv->gobj_late_client);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_drip_client);
    for(int i = 0; i < MUTE_CLIENTS; i++) {
        gobj_start(priv->gobj_mute_clients[i]);
    }
    priv->watchdog = start_msectimer(WATCHDOG_MS);
    set_timeout(priv->timer, LATE_CLIENT_AFTER_MS);
    set_timeout0(priv->drip_timer, DRIP_EVERY_MS);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    clear_timeout0(priv->drip_timer);

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
 *  The connections waiting in the listeners
 ***************************************************************************/
PRIVATE void accept_the_peers(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd_drip_peer < 0) {
        priv->fd_drip_peer = accept4(priv->fd_listen_drip, NULL, NULL, SOCK_NONBLOCK);
        if(priv->fd_drip_peer >= 0) {
            priv->drop_before = start_msectimer(DROP_BEFORE_MS);
        }
    }
    while(priv->mute_peers < MAX_PEERS) {
        int fd = accept4(priv->fd_listen_mute, NULL, NULL, SOCK_NONBLOCK);
        if(fd < 0) {
            break;
        }
        priv->fd_mute_peers[priv->mute_peers++] = fd;   // kept open, says nothing
    }
}

/***************************************************************************
 *  One byte of a TLS record that never ends (a ServerHello of 16 KB).
 *  What the client sends is read and thrown away; its end is seen here.
 ***************************************************************************/
PRIVATE void drip_a_byte(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    static const unsigned char record_header[] = {0x16, 0x03, 0x03, 0x40, 0x00, 0x02};

    if(priv->fd_drip_peer < 0 || priv->drip_peer_ended) {
        return;
    }

    char bf[4096];
    ssize_t n = recv(priv->fd_drip_peer, bf, sizeof(bf), MSG_DONTWAIT);
    if(n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        priv->drip_peer_ended = TRUE;
        if(test_msectimer(priv->drop_before)) {
            test_fail(gobj, "TEST: the drip kept the TLS handshake past its deadline");
        }
        return;
    }

    if(priv->dripped >= DRIP_BYTES) {
        return;
    }
    unsigned char c = priv->dripped < (int)sizeof(record_header)?
        record_header[priv->dripped] : 0x00;
    if(send(priv->fd_drip_peer, &c, 1, MSG_DONTWAIT|MSG_NOSIGNAL) == 1) {
        priv->dripped++;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The timers: the drip (C_TIMER0) and the phases (C_TIMER)
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(test_msectimer(priv->watchdog) && priv->phase != PHASE_END) {
        priv->phase = PHASE_END;
        test_fail(gobj, "TEST: the test did not end");
        set_yuno_must_die();
        JSON_DECREF(kw)
        return 0;
    }

    if(src == priv->drip_timer) {
        accept_the_peers(gobj);
        drip_a_byte(gobj);
        if(priv->drop_before && !priv->drip_peer_ended && test_msectimer(priv->drop_before)) {
            priv->drip_peer_ended = TRUE;   // said once
            test_fail(gobj, "TEST: the drip kept the TLS handshake past its deadline");
        }
        if(priv->phase != PHASE_END) {
            set_timeout0(priv->drip_timer, DRIP_EVERY_MS);
        }
        JSON_DECREF(kw)
        return 0;
    }

    switch(priv->phase) {
        case PHASE_FIRST_WINDOW:
            if(!priv->drip_peer_ended) {
                test_fail(gobj, "TEST: the client of the drip was not dropped");
            }
            if(priv->mute_peers != MUTE_CLIENTS) {
                test_fail(gobj, "TEST: the mute clients did not all connect");
            }
            for(int i = 0; i < MUTE_CLIENTS; i++) {
                const char *cause = gobj_read_str_attr(priv->gobj_mute_clients[i], "disconnect_cause");
                if(strcmp(cause, "TLS: the handshake did not end in time")!=0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "TEST: a mute client was not dropped at its deadline",
                        "client",       "%s", gobj_name(priv->gobj_mute_clients[i]),
                        "cause",        "%s", cause,
                        "state",        "%s", gobj_current_state(priv->gobj_mute_clients[i]),
                        NULL
                    );
                }
            }
            /*
             *  The window of the warning is over: one more deadline
             */
            priv->phase = PHASE_LATE_CLIENT;
            gobj_start(priv->gobj_late_client);
            set_timeout(priv->timer, DROP_BEFORE_MS);
            break;

        case PHASE_LATE_CLIENT:
            if(strcmp(gobj_read_str_attr(priv->gobj_late_client, "disconnect_cause"),
                    "TLS: the handshake did not end in time")!=0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST: the late client was not dropped at its deadline",
                    "cause",        "%s", gobj_read_str_attr(priv->gobj_late_client, "disconnect_cause"),
                    NULL
                );
            }
            priv->phase = PHASE_END;
            set_yuno_must_die();
            break;

        default:
            break;
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The events of the transports: no connection opens
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    test_fail(gobj, "TEST: a TLS handshake ended, with a peer that never answers");
    KW_DECREF(kw)
    return 0;
}

PRIVATE int ac_transport_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
GOBJ_DEFINE_GCLASS(C_TEST11);

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
PUBLIC int register_c_test11(void)
{
    return create_gclass(C_TEST11);
}
