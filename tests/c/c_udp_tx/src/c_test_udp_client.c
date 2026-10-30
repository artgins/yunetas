/***********************************************************************
 *          C_TEST_UDP_CLIENT.C
 *
 *          Driver of the C_UDP transmit test.
 *
 *          A C_UDP child sends to a plain UDP socket of the test, bound to
 *          a port of the kernel's choice (the url of the C_UDP is built
 *          with it):
 *
 *              1. an EMPTY gbuffer, with nothing in flight: nothing to
 *                 send, ignored with a warning, and its EV_TX_READY comes a
 *                 turn later, never inside the send. In the fixes after
 *                 7.29.3 none came, and a host that advances only on
 *                 EV_TX_READY waited for ever.
 *              2. "one", "two" and "three" in one turn: the socket must get
 *                 the three, in order, EV_TX_READY must come once, when the
 *                 queue is done, and the stats must say 3 datagrams and 11
 *                 bytes (up to 7.29.3 txBytes read 0 for ever). Up to 7.29.3 the completion of a send
 *                 asked for ST_CONNECTED before sending the next, a state
 *                 C_UDP does not have: "one" went, the others waited in the
 *                 queue for ever, and no EV_TX_READY came.
 *              3. a stop: C_UDP must end in ST_STOPPED and publish
 *                 EV_STOPPED, and the yuno dies: the memory check at the end
 *                 catches a leaked event or gbuffer.
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
#include "c_test_udp_client.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SETTLE_MS   300

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
    int phase;
    int tx_ready;
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
     *  The peer: a plain UDP socket of the test, on a port of the kernel's
     *  choice
     */
    char url[64] = "";
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t addrlen = sizeof(addr);
    priv->rx_fd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
    if(priv->rx_fd < 0 ||
            bind(priv->rx_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
            getsockname(priv->rx_fd, (struct sockaddr *)&addr, &addrlen) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot bind the receiving socket",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    } else {
        snprintf(url, sizeof(url), "udp://127.0.0.1:%d", (int)ntohs(addr.sin_port));
    }

    priv->gobj_udp = gobj_create(
        "udp",
        C_UDP,
        json_pack("{s:s}",
            "url", url
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

    gobj_start(priv->gobj_udp);
    set_timeout(priv->timer, SETTLE_MS);

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
 *
 ***************************************************************************/
PRIVATE void test_fail(hgobj gobj, const char *what, int count, int expected)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "count",        "%d", count,
        "expected",     "%d", expected,
        NULL
    );
}

/***************************************************************************
 *  Send a datagram to the C_UDP: the kw owns the gbuffer
 ***************************************************************************/
PRIVATE void send_datagram(hgobj gobj, const char *data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(64, 64);
    if(!empty_string(data)) {
        gbuffer_append_string(gbuf, data);
    }
    gobj_send_event(
        priv->gobj_udp,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
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

    switch(priv->phase) {
        case 0:
            /*
             *  An empty datagram, with nothing in flight
             */
            send_datagram(gobj, "");
            if(priv->tx_ready != 0) {
                test_fail(gobj, "TEST: the EV_TX_READY of an empty send came inside the send",
                    priv->tx_ready, 0);
            }
            priv->phase = 1;
            set_timeout(priv->timer, SETTLE_MS);
            break;

        case 1:
            if(priv->tx_ready != 1) {
                test_fail(gobj, "TEST: an empty send with nothing in flight did not get one EV_TX_READY",
                    priv->tx_ready, 1);
            }
            priv->tx_ready = 0;
            send_datagram(gobj, "one");
            send_datagram(gobj, "two");
            send_datagram(gobj, "three");
            priv->phase = 2;
            set_timeout(priv->timer, SETTLE_MS);
            break;

        case 2:
            {
                char received[256] = "";
                char bf[128];
                ssize_t n;
                while((n = recv(priv->rx_fd, bf, sizeof(bf) - 1, 0)) > 0) {
                    bf[n] = 0;
                    size_t ln = strlen(received);
                    snprintf(received + ln, sizeof(received) - ln, "%s%s", ln? " ": "", bf);
                }
                if(strcmp(received, "one two three") != 0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "TEST: the peer did not get the datagrams",
                        "received",     "%s", received,
                        "expected",     "%s", "one two three",
                        NULL
                    );
                }
                if(priv->tx_ready != 1) {
                    test_fail(gobj, "TEST: three sends did not get one EV_TX_READY",
                        priv->tx_ready, 1);
                }
                json_int_t tx_msgs = gobj_read_integer_attr(priv->gobj_udp, "txMsgs");
                json_int_t tx_bytes = gobj_read_integer_attr(priv->gobj_udp, "txBytes");
                if(tx_msgs != 3 || tx_bytes != 11) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "TEST: the counters of C_UDP do not say what it sent",
                        "txMsgs",       "%d", (int)tx_msgs,
                        "txBytes",      "%d", (int)tx_bytes,
                        NULL
                    );
                }
                if(!gobj_in_this_state(priv->gobj_udp, ST_IDLE)) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "TEST: C_UDP is not idle",
                        "state",        "%s", gobj_current_state(priv->gobj_udp),
                        NULL
                    );
                }
                gobj_stop(priv->gobj_udp);
                priv->phase = 3;
                set_timeout(priv->timer, SETTLE_MS);
            }
            break;

        default:
            if(!gobj_in_this_state(priv->gobj_udp, ST_STOPPED)) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST: the stop of C_UDP did not end",
                    "state",        "%s", gobj_current_state(priv->gobj_udp),
                    NULL
                );
            }
            if(priv->stopped != 1) {
                test_fail(gobj, "TEST: the stop of C_UDP did not publish one EV_STOPPED",
                    priv->stopped, 1);
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: done",
                NULL
            );
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Nothing is sent to the C_UDP from its peer here
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST: C_UDP received a datagram, nobody sent one",
        NULL
    );
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tx_ready(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->tx_ready++;

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
GOBJ_DEFINE_GCLASS(C_TEST_UDP_CLIENT);

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
        {EV_TX_READY,       ac_tx_ready,    0},
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
PUBLIC int register_c_test_udp_client(void)
{
    return create_gclass(C_TEST_UDP_CLIENT);
}
