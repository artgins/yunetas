/***********************************************************************
 *          C_TEST_UDP_SELF_STOP.C
 *
 *          Driver of the C_UDP_S self-stop test: the stops that C_UDP_S
 *          makes by itself must be said, and must END (ST_STOPPED and
 *          EV_STOPPED), even with a send in flight; and a secure url is
 *          refused at the start.
 *
 *              1. The server's rx_buffer_size goes down to 0, and the peer
 *                 sends "deaf". The host answers IN the gbuffer it got
 *                 (EV_TX_DATA with it), so the next read needs a new
 *                 gbuffer: one of 0 bytes, that cannot be read into. The
 *                 re-arm of the read fails with the answer in flight.
 *                 C_UDP_S must say it (an ERROR), stop, and when the answer
 *                 completes reach ST_STOPPED and publish EV_STOPPED.
 *                 Up to 7.25.4 the failure of the re-arm was ignored: the
 *                 server stayed in ST_IDLE, "running", and deaf, and
 *                 nothing was said. And a self-stop with a send in flight
 *                 stayed in ST_WAIT_STOPPED for ever: the completion of
 *                 the send took the path of a running server and never
 *                 ended the stop.
 *              2. A C_UDP_S with a udps:// url is started: it must refuse
 *                 (an ERROR, the start fails). Up to 7.25.4 it listened,
 *                 with no TLS session, and the first datagram crashed the
 *                 yuno (ytls_decrypt_data() with a NULL session).
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
#include "c_test_udp_self_stop.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define UDP_S_PORT  34293
#define UDP_S_URL   "udp://127.0.0.1:34293"
#define UDPS_PORT   34294
#define UDPS_URL    "udps://127.0.0.1:34294"
#define PEER_PORT   34295

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
    hgobj gobj_udp;
    hgobj gobj_udps;
    int peer_fd;
    int phase;
    int stopped;
    struct sockaddr_in peer_addr;
    struct sockaddr_in server_addr;
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

    priv->peer_fd = -1;
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->gobj_udp = gobj_create(
        "udp",
        C_UDP_S,
        json_pack("{s:s, s:b}",
            "url", UDP_S_URL,
            "exitOnError", 0
        ),
        gobj
    );
    priv->gobj_udps = gobj_create(
        "udps",
        C_UDP_S,
        json_pack("{s:s, s:b, s:{s:s, s:s}}",
            "url", UDPS_URL,
            "exitOnError", 0,
            "crypto",
                "ssl_certificate", "/yuneta/agent/certs/localhost.crt",
                "ssl_certificate_key", "/yuneta/agent/certs/localhost.key"
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

    if(priv->peer_fd >= 0) {
        close(priv->peer_fd);
        priv->peer_fd = -1;
    }
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->gobj_udp);

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
    if(gobj_is_running(priv->gobj_udp)) {
        gobj_stop(priv->gobj_udp);
    }
    if(gobj_is_running(priv->gobj_udps)) {
        gobj_stop(priv->gobj_udps);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The peer: a plain UDP socket of the test
     */
    memset(&priv->peer_addr, 0, sizeof(priv->peer_addr));
    priv->peer_addr.sin_family = AF_INET;
    priv->peer_addr.sin_port = htons(PEER_PORT);
    priv->peer_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    priv->server_addr = priv->peer_addr;
    priv->server_addr.sin_port = htons(UDP_S_PORT);

    priv->peer_fd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
    if(priv->peer_fd < 0 ||
            bind(priv->peer_fd, (struct sockaddr *)&priv->peer_addr, sizeof(priv->peer_addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot bind the peer socket",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        set_yuno_must_die();
        return -1;
    }

    /*
     *  1. The next read will need a gbuffer of 0 bytes; the peer sends
     */
    gobj_write_integer_attr(priv->gobj_udp, "rx_buffer_size", 0);
    if(sendto(priv->peer_fd, "deaf", 4, 0,
            (struct sockaddr *)&priv->server_addr, sizeof(priv->server_addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: sendto() FAILED",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }

    set_timeout(priv->timer, 200);
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




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase++;

    /*
     *  1. The self-stop ended, and the answer reached the peer
     */
    char received[64] = "";
    ssize_t n = recv(priv->peer_fd, received, sizeof(received) - 1, 0);
    if(n > 0) {
        received[n] = 0;
    }
    BOOL stop_ok = strcmp(received, "deaf") == 0 &&
        gobj_in_this_state(priv->gobj_udp, ST_STOPPED) &&
        priv->stopped == 1;

    /*
     *  2. A secure url is refused
     */
    int ret = gobj_start(priv->gobj_udps);
    BOOL listening = TRUE;  // its port taken: something listens there
    struct sockaddr_in udps_probe = priv->server_addr;
    udps_probe.sin_port = htons(UDPS_PORT);
    int probe_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(probe_fd >= 0) {
        if(bind(probe_fd, (struct sockaddr *)&udps_probe, sizeof(udps_probe)) == 0) {
            listening = FALSE;
        }
        close(probe_fd);
    }
    BOOL udps_ok = ret < 0 && !listening;
    if(!udps_ok) {
        /*
         *  Listening (up to 7.25.4): one datagram shows what it does with it
         */
        struct sockaddr_in udps_addr = priv->server_addr;
        udps_addr.sin_port = htons(UDPS_PORT);
        sendto(priv->peer_fd, "boom", 4, 0, (struct sockaddr *)&udps_addr, sizeof(udps_addr));
    }

    if(!stop_ok || !udps_ok) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: the self-stop of C_UDP_S did not end, or udps:// was not refused",
            "peer_received",    "%s", received,
            "peer_expected",    "%s", "deaf",
            "state",        "%s", gobj_current_state(priv->gobj_udp),
            "state_expected", "%s", ST_STOPPED,
            "stopped",      "%d", priv->stopped,
            "udps_start",   "%d", ret,
            "udps_listening", "%d", listening,
            NULL
        );
    } else {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "TEST: the self-stop of C_UDP_S ended, and udps:// was refused",
            NULL
        );
    }

    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the server received: answered IN its gbuffer
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gbuf && src == priv->gobj_udp) {
        gobj_send_event(
            priv->gobj_udp,
            EV_TX_DATA,
            json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuffer_incref(gbuf)),
            gobj
        );
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

    if(src == priv->gobj_udp) {
        priv->stopped++;
    }

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
GOBJ_DEFINE_GCLASS(C_TEST_UDP_SELF_STOP);

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
        {EV_TIMEOUT,        ac_timeout,     0},
        {EV_RX_DATA,        ac_rx_data,     0},
        {EV_TX_READY,       0,              0},
        {EV_STOPPED,        ac_stopped,     0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,           st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,        0},
        {EV_RX_DATA,        0},
        {EV_TX_READY,       0},
        {EV_STOPPED,        0},
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
PUBLIC int register_c_test_udp_self_stop(void)
{
    return create_gclass(C_TEST_UDP_SELF_STOP);
}
