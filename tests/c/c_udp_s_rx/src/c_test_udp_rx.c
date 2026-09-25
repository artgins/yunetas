/***********************************************************************
 *          C_TEST_UDP_RX.C
 *
 *          Driver of the C_UDP_S receive test.
 *
 *          1.  The peer of each datagram. A C_GSS_UDP_S child (a C_UDP_S
 *              and the frames it joins until a NUL, as logcenter receives
 *              the logs of every yuno) gets the pieces of two long messages
 *              of two peers, interleaved:
 *
 *                  A "a1",  B "b1",  A "a2\0",  B "b2\0"
 *
 *              It must publish "a1a2" and "b1b2", from two channels.
 *              C_GSS_UDP_S keys its channels by the label of the EV_RX_DATA
 *              gbuffer, the peer. Up to 7.25.4 C_UDP_S wrote that label
 *              only when tracing: every peer was the channel "", and the
 *              frames came out as "a1b1a2" and "b2".
 *
 *          2.  The yuno's ip lists, as C_TCP_S asks them at accept. A
 *              C_UDP_S child with `only_allowed_ips` gets a datagram from
 *              127.0.1.5 (not allowed: dropped, with a warning), from
 *              127.0.1.6 (in the yuno's `allowed_ips`), from 127.0.0.1 (the
 *              loopback, always heard) and from 127.0.1.8 (in `allowed_ips`
 *              AND in `denied_ips`: denied wins, dropped with a warning).
 *              Up to 7.25.4 the attribute was documented and never read,
 *              and the deny-list not asked: every peer was heard.
 *
 *          3.  A refused peer is said on the transition, not per datagram
 *              (the source of a datagram can be forged): the refused peers
 *              send three datagrams each, and there is ONE warning per
 *              cause (denied, not allowed); the six drops are counted in
 *              the stat `rxRefusedMsgs`. Before this fix each datagram was a
 *              warning, and nothing counted them.
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
#include "c_test_udp_rx.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define GSS_PORT        34285
#define GSS_URL         "udp://127.0.0.1:34285"
#define ALLOW_PORT      34286
#define ALLOW_URL       "udp://127.0.0.1:34286"

#define IP_DENIED       "127.0.1.5"
#define IP_ALLOWED      "127.0.1.6"
#define IP_BOTH         "127.0.1.8"     // allowed and denied: denied wins

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
#define MAX_PEERS 6

typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj gobj_gss;
    hgobj gobj_allow;
    int peer_fd[MAX_PEERS];
    int opened;
    char frames[256];
    char heard[256];
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

    for(int i = 0; i < MAX_PEERS; i++) {
        priv->peer_fd[i] = -1;
    }
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->gobj_gss = gobj_create(
        "gss",
        C_GSS_UDP_S,
        json_pack("{s:s}",
            "url", GSS_URL
        ),
        gobj
    );
    priv->gobj_allow = gobj_create(
        "allow",
        C_UDP_S,
        json_pack("{s:s, s:b, s:b}",
            "url", ALLOW_URL,
            "exitOnError", 0,
            "only_allowed_ips", 1
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

    for(int i = 0; i < MAX_PEERS; i++) {
        if(priv->peer_fd[i] >= 0) {
            close(priv->peer_fd[i]);
            priv->peer_fd[i] = -1;
        }
    }
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->gobj_gss);
    gobj_start(priv->gobj_allow);

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
    if(gobj_is_running(priv->gobj_gss)) {
        gobj_stop(priv->gobj_gss);
    }
    if(gobj_is_running(priv->gobj_allow)) {
        gobj_stop(priv->gobj_allow);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *peer_ips[MAX_PEERS] = {
        "127.0.0.1",    // A, to C_GSS_UDP_S
        "127.0.0.1",    // B, to C_GSS_UDP_S
        IP_DENIED,      // to the C_UDP_S with only_allowed_ips
        IP_ALLOWED,     // idem
        "127.0.0.1",    // idem, the loopback
        IP_BOTH         // idem
    };
    for(int i = 0; i < MAX_PEERS; i++) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        inet_pton(AF_INET, peer_ips[i], &addr.sin_addr);
        priv->peer_fd[i] = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
        if(priv->peer_fd[i] < 0 ||
                bind(priv->peer_fd[i], (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "TEST: cannot bind a peer socket",
                "ip",           "%s", peer_ips[i],
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            set_yuno_must_die();
            return -1;
        }
    }

    json_object_set_new(
        gobj_read_json_attr(gobj_yuno(), "allowed_ips"),
        IP_ALLOWED,
        json_true()
    );
    json_object_set_new(
        gobj_read_json_attr(gobj_yuno(), "allowed_ips"),
        IP_BOTH,
        json_true()
    );
    json_object_set_new(
        gobj_read_json_attr(gobj_yuno(), "denied_ips"),
        IP_BOTH,
        json_true()
    );

    struct sockaddr_in gss_addr;
    memset(&gss_addr, 0, sizeof(gss_addr));
    gss_addr.sin_family = AF_INET;
    gss_addr.sin_port = htons(GSS_PORT);
    gss_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    struct sockaddr_in allow_addr = gss_addr;
    allow_addr.sin_port = htons(ALLOW_PORT);

    /*
     *  1. The pieces of two messages, interleaved (with their NUL)
     */
    struct {
        int peer;
        const char *data;
        size_t len;
    } pieces[] = {
        {0, "a1",   2},
        {1, "b1",   2},
        {0, "a2",   3},
        {1, "b2",   3},
    };
    for(size_t i = 0; i < ARRAY_SIZE(pieces); i++) {
        sendto(priv->peer_fd[pieces[i].peer], pieces[i].data, pieces[i].len, 0,
            (struct sockaddr *)&gss_addr, sizeof(gss_addr));
    }

    /*
     *  2. The peers of a server with only_allowed_ips
     */
    sendto(priv->peer_fd[2], "denied", 6, 0, (struct sockaddr *)&allow_addr, sizeof(allow_addr));
    sendto(priv->peer_fd[3], "allowed", 7, 0, (struct sockaddr *)&allow_addr, sizeof(allow_addr));
    sendto(priv->peer_fd[4], "local", 5, 0, (struct sockaddr *)&allow_addr, sizeof(allow_addr));
    sendto(priv->peer_fd[5], "both", 4, 0, (struct sockaddr *)&allow_addr, sizeof(allow_addr));

    /*
     *  3. The refused peers insist: counted, not said again
     */
    for(int i=0; i<2; i++) {
        sendto(priv->peer_fd[2], "denied", 6, 0, (struct sockaddr *)&allow_addr, sizeof(allow_addr));
        sendto(priv->peer_fd[5], "both", 4, 0, (struct sockaddr *)&allow_addr, sizeof(allow_addr));
    }

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
 *  Append the text of a gbuffer to a list of words
 ***************************************************************************/
PRIVATE void append_text(char *bf, size_t bfsize, gbuffer_t *gbuf)
{
    if(!gbuf) {
        return;
    }
    size_t ln = strlen(bf);
    snprintf(bf + ln, bfsize - ln, "%s%.*s",
        ln? " ": "",
        (int)gbuffer_leftbytes(gbuf),
        (const char *)gbuffer_cur_rd_pointer(gbuf)
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  What was received
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t refused = gobj_read_integer_attr(priv->gobj_allow, "rxRefusedMsgs");
    if(strcmp(priv->frames, "a1a2 b1b2") != 0 ||
            priv->opened != 2 ||
            strcmp(priv->heard, "allowed local") != 0 ||
            refused != 6) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: the peers of the datagrams are not kept apart",
            "frames",       "%s", priv->frames,
            "frames_expected", "%s", "a1a2 b1b2",
            "channels",     "%d", priv->opened,
            "heard",        "%s", priv->heard,
            "heard_expected", "%s", "allowed local",
            "rxRefusedMsgs", "%ld", (long)refused,
            "rxRefusedMsgs_expected", "%d", 6,
            NULL
        );
    } else {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "TEST: every peer has its channel, and a peer not allowed is not heard",
            "frames",       "%s", priv->frames,
            "heard",        "%s", priv->heard,
            NULL
        );
    }

    gobj_stop(priv->gobj_gss);
    gobj_stop(priv->gobj_allow);
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A channel of C_GSS_UDP_S
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->opened++;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A frame of C_GSS_UDP_S
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    append_text(priv->frames, sizeof(priv->frames), gbuf);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A datagram of the C_UDP_S with only_allowed_ips
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    append_text(priv->heard, sizeof(priv->heard), gbuf);

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
GOBJ_DEFINE_GCLASS(C_TEST_UDP_RX);

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
        {EV_ON_OPEN,        ac_on_open,     0},
        {EV_ON_MESSAGE,     ac_on_message,  0},
        {EV_ON_CLOSE,       0,              0},
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
        {EV_ON_OPEN,        0},
        {EV_ON_MESSAGE,     0},
        {EV_ON_CLOSE,       0},
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
PUBLIC int register_c_test_udp_rx(void)
{
    return create_gclass(C_TEST_UDP_RX);
}
