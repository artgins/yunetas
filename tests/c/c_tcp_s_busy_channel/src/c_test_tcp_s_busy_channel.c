/***********************************************************************
 *          C_TEST_TCP_S_BUSY_CHANNEL.C
 *
 *          Driver of test_c_tcp_s_busy_channel.
 *
 *          child_tree_filter asks an attribute that can say nothing of the
 *          transport. {"connected": false} reads the first `connected` of
 *          the chain, and C_PROT_MQTT has its own, true only at its session;
 *          here {"opened": false} of C_CHANNEL does the same with no MQTT:
 *          true only when the TLS handshake ended.
 *
 *          Two C_TCP_S share ONE channel, tls_port and plain_port. A raw
 *          socket connects to tls_port and never starts the handshake; a
 *          second one connects to plain_port. Up to 7.30.1 the second accept
 *          took the channel of the first: "GObj ALREADY RUNNING", the socket
 *          of the first leaked, its ytls overwritten with the NULL of
 *          plain_port, and the end of the connection a SIGSEGV in
 *          ytls_free_secure_filter() (a real core: a scanner of the internet
 *          on a gate with C_PROT_MQTT).
 *
 *          The channel is taken: plain_port refuses the second peer and
 *          counts it. When the first peer leaves, the channel is free and
 *          a third peer is accepted.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "c_test_tcp_s_busy_channel.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TLS_PORT    7878
#define PLAIN_PORT  7879

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int connect_to(int port);
PRIVATE hgobj find_server(const char *name);
PRIVATE void test_error(hgobj gobj, const char *msg, int got, int expected);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE const sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag------------default---------description---------- */
SDATA (DTP_POINTER,     "user_data",        0,              0,              "user data"),
SDATA (DTP_POINTER,     "subscriber",       0,              0,              "subscriber of output-events"),
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
    int phase;
    int fd_tls;
    int fd_plain;
    int fd_third;
} PRIVATE_DATA;

PRIVATE hgclass __gclass__ = 0;




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
    priv->fd_tls = -1;
    priv->fd_plain = -1;
    priv->fd_third = -1;
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
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj gate = gobj_find_service("__side__", TRUE);
    gobj_subscribe_event(gate, NULL, 0, gobj);
    gobj_start_tree(gate);
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
 *  A raw socket to 127.0.0.1:<port>; it sends nothing
 ***************************************************************************/
PRIVATE int connect_to(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC, 0);
    if(fd < 0) {
        return -1;  // the test says it
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;  // the test says it
    }
    return fd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE hgobj find_server(const char *name)
{
    hgobj gate = gobj_find_service("__side__", TRUE);
    return gobj_find_child(gate, json_pack("{s:s, s:s}",
        "__gclass_name__", C_TCP_S,
        "__gobj_name__", name
    ));
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_error(hgobj gobj, const char *msg, int got, int expected)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", msg,
        "got",          "%d", got,
        "expected",     "%d", expected,
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
    hgobj plain_port = find_server("plain_port");

    priv->phase++;

    switch(priv->phase) {
        case 1:
            priv->fd_tls = connect_to(TLS_PORT);
            if(priv->fd_tls < 0) {
                test_error(gobj, "TEST: cannot connect to tls_port", -1, 0);
            }
            set_timeout(priv->timer, 300);
            break;

        case 2:
            priv->fd_plain = connect_to(PLAIN_PORT);
            if(priv->fd_plain < 0) {
                test_error(gobj, "TEST: cannot connect to plain_port", -1, 0);
            }
            set_timeout(priv->timer, 300);
            break;

        case 3:
            {
                int refused = (int)gobj_read_integer_attr(plain_port, "noChannelConnxs");
                if(refused != 1) {
                    test_error(gobj, "TEST: the channel in its handshake was taken", refused, 1);
                }
                close(priv->fd_tls);
                priv->fd_tls = -1;
                set_timeout(priv->timer, 500);
            }
            break;

        case 4:
            priv->fd_third = connect_to(PLAIN_PORT);
            if(priv->fd_third < 0) {
                test_error(gobj, "TEST: cannot connect to plain_port again", -1, 0);
            }
            set_timeout(priv->timer, 300);
            break;

        default:
            {
                int accepted = (int)gobj_read_integer_attr(plain_port, "tconnxs");
                if(accepted != 1) {
                    test_error(gobj, "TEST: the channel freed was not given to the next peer",
                        accepted, 1
                    );
                }
                if(priv->fd_plain >= 0) {
                    close(priv->fd_plain);
                    priv->fd_plain = -1;
                }
                if(priv->fd_third >= 0) {
                    close(priv->fd_third);
                    priv->fd_third = -1;
                }
                set_yuno_must_die();
            }
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the gate publishes
 ***************************************************************************/
PRIVATE int ac_channel(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_TCP_S_BUSY_CHANNEL);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
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

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_ON_OPEN,                ac_channel,                 0},
        {EV_ON_CLOSE,               ac_channel,                 0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
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
PUBLIC int register_c_test_tcp_s_busy_channel(void)
{
    return create_gclass(C_TEST_TCP_S_BUSY_CHANNEL);
}
