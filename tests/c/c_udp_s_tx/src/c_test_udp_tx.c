/***********************************************************************
 *          C_TEST_UDP_TX.C
 *
 *          Driver of the C_UDP_S transmit test.
 *
 *          A C_UDP_S child is asked to send three datagrams (EV_TX_DATA):
 *
 *              1. a gbuffer with NO peer address: yev_start_event()
 *                 refuses the sendmsg, and C_UDP_S drops the datagram with
 *                 an error. Up to 7.25.4 the send event and its gbuffer
 *                 leaked, tx_in_progress never went back to 0, and
 *                 gbuf_txing held every later datagram in the queue.
 *              2. "one", to a plain UDP socket of the test.
 *              3. "two", to the same socket. Up to 7.25.4 the send
 *                 completion asked for ST_CONNECTED before sending the
 *                 next datagram, a state C_UDP_S does not have: the first
 *                 datagram went, and every later one waited for ever.
 *
 *          After 500 ms the socket must have "one" and "two", in order.
 *          Then C_UDP_S is stopped and the yuno dies: the memory check at
 *          the end catches a leaked event or gbuffer.
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
#include "c_test_udp_tx.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define UDP_S_URL   "udp://127.0.0.1:34281"
#define RX_PORT     34282

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
    int rx_fd;
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
    struct sockaddr_in rx_addr;
    memset(&rx_addr, 0, sizeof(rx_addr));
    rx_addr.sin_family = AF_INET;
    rx_addr.sin_port = htons(RX_PORT);
    rx_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    priv->rx_fd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
    if(priv->rx_fd < 0 || bind(priv->rx_fd, (struct sockaddr *)&rx_addr, sizeof(rx_addr)) < 0) {
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

    const char *datagrams[] = {"no address", "one", "two"};
    for(int i = 0; i < 3; i++) {
        gbuffer_t *gbuf = gbuffer_create(64, 64);
        gbuffer_append_string(gbuf, datagrams[i]);
        if(i > 0) {
            gbuffer_setaddr(gbuf, (struct sockaddr *)&rx_addr, sizeof(rx_addr));
        }
        gobj_send_event(
            priv->gobj_udp,
            EV_TX_DATA,
            json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),  // the kw owns the gbuffer
            gobj
        );
    }

    set_timeout(priv->timer, 500);
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
 *  What the peer received
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char received[256] = "";
    char bf[128];
    ssize_t n;
    while((n = recv(priv->rx_fd, bf, sizeof(bf) - 1, 0)) > 0) {
        bf[n] = 0;
        size_t ln = strlen(received);
        snprintf(received + ln, sizeof(received) - ln, "%s%s", ln? " ": "", bf);
    }

    if(strcmp(received, "one two") != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: the peer did not get the datagrams",
            "received",     "%s", received,
            "expected",     "%s", "one two",
            NULL
        );
    } else {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "TEST: the peer got the datagrams",
            "received",     "%s", received,
            NULL
        );
    }

    gobj_stop(priv->gobj_udp);
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  C_UDP_S publishes what it receives: nothing is sent to it here
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST: C_UDP_S received a datagram, nobody sent one",
        NULL
    );
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
GOBJ_DEFINE_GCLASS(C_TEST_UDP_TX);

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
PUBLIC int register_c_test_udp_tx(void)
{
    return create_gclass(C_TEST_UDP_TX);
}
