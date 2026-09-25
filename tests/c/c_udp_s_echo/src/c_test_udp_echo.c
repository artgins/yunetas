/***********************************************************************
 *          C_TEST_UDP_ECHO.C
 *
 *          Driver of the C_UDP_S echo test: a host that answers IN the
 *          gbuffer of the EV_RX_DATA, as transport.md documents (the peer
 *          address is there already): it forwards the kw of every
 *          EV_RX_DATA to C_UDP_S as EV_TX_DATA.
 *
 *              1. Five datagrams (s1..s5) are sent to peer A, and in the
 *                 same turn peer A sends "hello" and peer B sends
 *                 "world" to the server: the answers wait in the queue
 *                 behind s1..s5 while the next datagram is read.
 *              2. Peer B sends b1, b2, b3 in one turn: each answer is in
 *                 flight while the next datagram is read.
 *
 *          Peer A must get "s1 s2 s3 s4 s5 hello" and peer B "world" then
 *          "b1 b2 b3", with no error logged. Up to 7.25.4 C_UDP_S cleared
 *          the gbuffer after the publish and read the next datagram into
 *          it: an answer was sent empty (dropped: "Cannot start event:
 *          gbuffer WITHOUT data to write", "Cannot send datagram:
 *          dropped"), or with the bytes and the peer of another datagram.
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
#include "c_test_udp_echo.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define UDP_S_PORT  34287
#define UDP_S_URL   "udp://127.0.0.1:34287"
#define PEER_A_PORT 34288
#define PEER_B_PORT 34289

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_datagram(hgobj gobj, const char *text);
PRIVATE void peer_sends(hgobj gobj, int peer_fd, const char *text);
PRIVATE void peer_receives(int peer_fd, char *bf, size_t bfsize);

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
    int peer_a_fd;
    int peer_b_fd;
    int phase;
    char peer_a_received[256];
    char peer_b_received[256];
    struct sockaddr_in peer_a_addr;
    struct sockaddr_in peer_b_addr;
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

    priv->peer_a_fd = -1;
    priv->peer_b_fd = -1;
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

    if(priv->peer_a_fd >= 0) {
        close(priv->peer_a_fd);
        priv->peer_a_fd = -1;
    }
    if(priv->peer_b_fd >= 0) {
        close(priv->peer_b_fd);
        priv->peer_b_fd = -1;
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
     *  The peers: plain UDP sockets of the test
     */
    memset(&priv->server_addr, 0, sizeof(priv->server_addr));
    priv->server_addr.sin_family = AF_INET;
    priv->server_addr.sin_port = htons(UDP_S_PORT);
    priv->server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    priv->peer_a_addr = priv->server_addr;
    priv->peer_a_addr.sin_port = htons(PEER_A_PORT);
    priv->peer_b_addr = priv->server_addr;
    priv->peer_b_addr.sin_port = htons(PEER_B_PORT);

    priv->peer_a_fd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
    priv->peer_b_fd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
    if(priv->peer_a_fd < 0 || priv->peer_b_fd < 0 ||
            bind(priv->peer_a_fd, (struct sockaddr *)&priv->peer_a_addr, sizeof(priv->peer_a_addr)) < 0 ||
            bind(priv->peer_b_fd, (struct sockaddr *)&priv->peer_b_addr, sizeof(priv->peer_b_addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot bind the peer sockets",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        set_yuno_must_die();
        return -1;
    }

    /*
     *  1. Sends in the queue, and two peers talk in the same turn
     */
    send_datagram(gobj, "s1");
    send_datagram(gobj, "s2");
    send_datagram(gobj, "s3");
    send_datagram(gobj, "s4");
    send_datagram(gobj, "s5");
    peer_sends(gobj, priv->peer_a_fd, "hello");
    peer_sends(gobj, priv->peer_b_fd, "world");

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
 *  Send a datagram to peer A through C_UDP_S
 ***************************************************************************/
PRIVATE int send_datagram(hgobj gobj, const char *text)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(64, 64);
    gbuffer_append_string(gbuf, text);
    gbuffer_setaddr(gbuf, (struct sockaddr *)&priv->peer_a_addr, sizeof(priv->peer_a_addr));
    return gobj_send_event(
        priv->gobj_udp,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),  // the kw owns the gbuffer
        gobj
    );
}

/***************************************************************************
 *  A peer sends a datagram to the server
 ***************************************************************************/
PRIVATE void peer_sends(hgobj gobj, int peer_fd, const char *text)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(sendto(peer_fd, text, strlen(text), 0,
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

/***************************************************************************
 *  What a peer received, appended to `bf`
 ***************************************************************************/
PRIVATE void peer_receives(int peer_fd, char *bf, size_t bfsize)
{
    char datagram[128];
    ssize_t n;
    while((n = recv(peer_fd, datagram, sizeof(datagram) - 1, 0)) >= 0) {
        datagram[n] = 0;
        size_t ln = strlen(bf);
        snprintf(bf + ln, bfsize - ln, "%s%s", ln? " ": "", n? datagram: "<empty>");
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

    peer_receives(priv->peer_a_fd, priv->peer_a_received, sizeof(priv->peer_a_received));
    peer_receives(priv->peer_b_fd, priv->peer_b_received, sizeof(priv->peer_b_received));

    if(priv->phase == 1) {
        /*
         *  2. Three datagrams in one turn: each answer in flight while
         *  the next one is read
         */
        peer_sends(gobj, priv->peer_b_fd, "b1");
        peer_sends(gobj, priv->peer_b_fd, "b2");
        peer_sends(gobj, priv->peer_b_fd, "b3");
        set_timeout(priv->timer, 300);
        KW_DECREF(kw)
        return 0;
    }

    const char *a_expected = "s1 s2 s3 s4 s5 hello";
    const char *b_expected = "world b1 b2 b3";
    if(strcmp(priv->peer_a_received, a_expected) != 0 ||
            strcmp(priv->peer_b_received, b_expected) != 0 ||
            !gobj_in_this_state(priv->gobj_udp, ST_IDLE)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: an answer in the rx gbuffer was lost or went to another peer",
            "peer_a_received",  "%s", priv->peer_a_received,
            "peer_a_expected",  "%s", a_expected,
            "peer_b_received",  "%s", priv->peer_b_received,
            "peer_b_expected",  "%s", b_expected,
            "state",        "%s", gobj_current_state(priv->gobj_udp),
            NULL
        );
    } else {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "TEST: every answer in the rx gbuffer went back to its sender, whole",
            "peer_a_received",  "%s", priv->peer_a_received,
            "peer_b_received",  "%s", priv->peer_b_received,
            NULL
        );
    }

    gobj_stop(priv->gobj_udp);
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The answer, in the rx gbuffer: the kw goes back as it came
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return gobj_send_event(priv->gobj_udp, EV_TX_DATA, kw, gobj);
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
GOBJ_DEFINE_GCLASS(C_TEST_UDP_ECHO);

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
        {EV_STOPPED,        0,              0},
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
PUBLIC int register_c_test_udp_echo(void)
{
    return create_gclass(C_TEST_UDP_ECHO);
}
