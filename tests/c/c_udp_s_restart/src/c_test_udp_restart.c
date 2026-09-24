/***********************************************************************
 *          C_TEST_UDP_RESTART.C
 *
 *          Driver of the C_UDP_S restart test: a stop and a start again,
 *          as logcenter does at pause-yuno and play-yuno.
 *
 *              1. "one" is sent, and C_UDP_S is stopped at once: the
 *                 send is in flight, the read is canceling. The stop
 *                 ends (EV_STOPPED) once both are done.
 *              2. A file is opened: it takes the number of the socket the
 *                 stop closed (logcenter opens its log file just before it
 *                 starts its listener). C_UDP_S is started again, "two" is
 *                 sent, and the peer sends "hello" to the server.
 *                 Up to 7.25.4 the read was started again on the number of
 *                 the OLD socket -- the file by then -- and the server
 *                 stopped reading with nothing logged; and the datagram of
 *                 the stop was kept as the one being sent, so "two" waited
 *                 behind it for ever.
 *              3. A stop and a start in the SAME turn (the read of the
 *                 stop is still canceling), and the peer sends "again".
 *                 Up to 7.25.4 the start re-armed the canceling read
 *                 ("is CANCELING") and the server had no read at all.
 *
 *          At the end the peer must have "one two", C_UDP_S must have
 *          received "hello" and "again", and be listening (ST_IDLE).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "c_test_udp_restart.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define UDP_S_PORT  34283
#define UDP_S_URL   "udp://127.0.0.1:34283"
#define RX_PORT     34284

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_datagram(hgobj gobj, const char *text);

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
    int rx_fd;
    int file_fd;
    int phase;
    int stopped;
    char server_received[256];
    struct sockaddr_in rx_addr;
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

    priv->rx_fd = -1;
    priv->file_fd = -1;
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
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->rx_fd >= 0) {
        close(priv->rx_fd);
        priv->rx_fd = -1;
    }
    if(priv->file_fd >= 0) {
        close(priv->file_fd);
        priv->file_fd = -1;
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
    memset(&priv->rx_addr, 0, sizeof(priv->rx_addr));
    priv->rx_addr.sin_family = AF_INET;
    priv->rx_addr.sin_port = htons(RX_PORT);
    priv->rx_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    priv->server_addr = priv->rx_addr;
    priv->server_addr.sin_port = htons(UDP_S_PORT);

    priv->rx_fd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
    if(priv->rx_fd < 0 ||
            bind(priv->rx_fd, (struct sockaddr *)&priv->rx_addr, sizeof(priv->rx_addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot bind the receiving socket",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        set_yuno_must_die();
        return -1;
    }

    /*
     *  1. A send in flight and a stop
     */
    send_datagram(gobj, "one");
    gobj_stop(priv->gobj_udp);

    set_timeout(priv->timer, 100);
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
 *  Send a datagram to the peer through C_UDP_S
 ***************************************************************************/
PRIVATE int send_datagram(hgobj gobj, const char *text)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(64, 64);
    gbuffer_append_string(gbuf, text);
    gbuffer_setaddr(gbuf, (struct sockaddr *)&priv->rx_addr, sizeof(priv->rx_addr));
    return gobj_send_event(
        priv->gobj_udp,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),  // the kw owns the gbuffer
        gobj
    );
}

/***************************************************************************
 *  The peer sends a datagram to the server
 ***************************************************************************/
PRIVATE void peer_sends(hgobj gobj, const char *text)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(sendto(priv->rx_fd, text, strlen(text), 0,
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
}




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

    if(priv->phase == 1) {
        if(priv->stopped != 1 || !gobj_in_this_state(priv->gobj_udp, ST_STOPPED)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST: the stop did not end",
                "state",        "%s", gobj_current_state(priv->gobj_udp),
                "stopped",      "%d", priv->stopped,
                NULL
            );
        }

        /*
         *  2. A file takes the number of the closed socket, then a start
         */
        priv->file_fd = open("/dev/null", O_RDONLY|O_CLOEXEC);
        gobj_start(priv->gobj_udp);
        send_datagram(gobj, "two");
        peer_sends(gobj, "hello");
        set_timeout(priv->timer, 200);
        KW_DECREF(kw)
        return 0;
    }

    if(priv->phase == 2) {
        /*
         *  3. A stop and a start in the same turn
         */
        gobj_stop(priv->gobj_udp);
        gobj_start(priv->gobj_udp);
        peer_sends(gobj, "again");
        set_timeout(priv->timer, 200);
        KW_DECREF(kw)
        return 0;
    }

    char received[256] = "";
    char bf[128];
    ssize_t n;
    while((n = recv(priv->rx_fd, bf, sizeof(bf) - 1, 0)) > 0) {
        bf[n] = 0;
        size_t ln = strlen(received);
        snprintf(received + ln, sizeof(received) - ln, "%s%s", ln? " ": "", bf);
    }

    if(strcmp(received, "one two") != 0 ||
            strcmp(priv->server_received, "hello again") != 0 ||
            !gobj_in_this_state(priv->gobj_udp, ST_IDLE)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: C_UDP_S did not work again after a stop",
            "peer_received",    "%s", received,
            "peer_expected",    "%s", "one two",
            "server_received",  "%s", priv->server_received,
            "server_expected",  "%s", "hello again",
            "state",        "%s", gobj_current_state(priv->gobj_udp),
            NULL
        );
    } else {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "TEST: C_UDP_S works again after a stop",
            "peer_received",    "%s", received,
            "server_received",  "%s", priv->server_received,
            NULL
        );
    }

    gobj_stop(priv->gobj_udp);
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the server received
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gbuf) {
        size_t ln = strlen(priv->server_received);
        snprintf(priv->server_received + ln, sizeof(priv->server_received) - ln, "%s%.*s",
            ln? " ": "",
            (int)gbuffer_leftbytes(gbuf),
            (const char *)gbuffer_cur_rd_pointer(gbuf)
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

    priv->stopped++;

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
GOBJ_DEFINE_GCLASS(C_TEST_UDP_RESTART);

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
PUBLIC int register_c_test_udp_restart(void)
{
    return create_gclass(C_TEST_UDP_RESTART);
}
