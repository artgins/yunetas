/***********************************************************************
 *          C_TEST8.C
 *
 *          A class to test C_IOGATE: one message sent to ALL its channels.
 *
 *          A C_IOGATE with send_type 1 sends an EV_SEND_MESSAGE to every
 *          open channel. Its gbuffer was handed to all of them, and the
 *          first C_PROT_TCP4H read it out (gbuffer_append_gbuf() consumes
 *          the source): the others framed an EMPTY message, and a TCP4H
 *          peer drops the connection on it ("frame_length cannot be 0").
 *          Each channel must send the whole message.
 *
 *          The gate (__all_side__) listens with a C_TCP_S and 3 channels
 *          (C_PROT_TCP4H over C_TCP); the peers are raw sockets, which read
 *          the TCP4H frame by hand: a 4-byte length (header included),
 *          then the payload.
 *
 *              1. 3 peers connect
 *              2. (300 ms) one EV_SEND_MESSAGE with a gbuffer to the gate
 *              3. (300 ms) each peer must have got the whole frame
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "c_test8.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define ALL_PORT    7705
#define N_PEERS     3
#define MESSAGE     "the same message for all"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int connect_peer(hgobj gobj);
PRIVATE void test_fail(hgobj gobj, const char *what, int peer);

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
    hgobj gate;
    int phase;
    int fds[N_PEERS];
    int failures;
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
    for(int i=0; i<N_PEERS; i++) {
        priv->fds[i] = -1;
    }
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<N_PEERS; i++) {
        if(priv->fds[i] >= 0) {
            close(priv->fds[i]);
            priv->fds[i] = -1;
        }
    }
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gate = gobj_find_service("__all_side__", TRUE);
    gobj_subscribe_event(priv->gate, NULL, 0, gobj);
    gobj_start_tree(priv->gate);

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
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  A raw client socket connected to the gate
 ***************************************************************************/
PRIVATE int connect_peer(hgobj gobj)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        test_fail(gobj, "TEST: socket() FAILED", -1);
        return -1;
    }
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(ALL_PORT);
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    if(connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        test_fail(gobj, "TEST: connect() FAILED", -1);
        close(fd);
        return -1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    return fd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_fail(hgobj gobj, const char *what, int peer)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->failures++;
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "peer",         "%d", peer,
        NULL
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

    switch(priv->phase++) {
        case 0:
            for(int i=0; i<N_PEERS; i++) {
                priv->fds[i] = connect_peer(gobj);
            }
            set_timeout(priv->timer, 300);
            break;

        case 1:
            {
                gbuffer_t *gbuf = gbuffer_create(strlen(MESSAGE), strlen(MESSAGE));
                gbuffer_append_string(gbuf, MESSAGE);
                gobj_send_event(
                    priv->gate,
                    EV_SEND_MESSAGE,
                    json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),  // the kw owns it
                    gobj
                );
            }
            set_timeout(priv->timer, 300);
            break;

        case 2:
            for(int i=0; i<N_PEERS; i++) {
                char bf[256];
                ssize_t n = priv->fds[i] >= 0? recv(priv->fds[i], bf, sizeof(bf), 0) : -1;
                size_t expected = 4 + strlen(MESSAGE);
                uint32_t len = 0;
                if(n >= 4) {
                    memcpy(&len, bf, 4);
                    len = ntohl(len);
                }
                if(n != (ssize_t)expected || len != expected ||
                        memcmp(bf + 4, MESSAGE, strlen(MESSAGE)) != 0) {
                    test_fail(gobj, "TEST: a peer did not get the whole message", i);
                }
            }
            if(!priv->failures) {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "TEST: every channel sent the whole message",
                    NULL
                );
            }
            set_yuno_must_die();
            break;

        default:
            test_fail(gobj, "TEST: unexpected phase", -1);
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The channels of the gate open and close: nothing to do
 ***************************************************************************/
PRIVATE int ac_channel(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
GOBJ_DEFINE_GCLASS(C_TEST8);

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
        {EV_ON_OPEN,            ac_channel,         0},
        {EV_ON_CLOSE,           ac_channel,         0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
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
PUBLIC int register_c_test8(void)
{
    return create_gclass(C_TEST8);
}
