/***********************************************************************
 *          C_TEST_ADDR_IN_USE.C
 *
 *          GClass to test a C_TCP_S whose address is in use at its start
 *
 *          A yuno killed and launched at once (the agent's restart of a
 *          node) finds its port held by the dead one for a while: the
 *          io_uring ring of a process is torn down after the process is
 *          gone, with the accept that holds the listening socket. Up to
 *          7.27.3 the start of the C_TCP_S failed at once, the yuno exited
 *          (exitOnError) and its watcher launched it again 2 s later, with a
 *          "Daemon relaunched" ERROR. Now it listens again, at each tick of
 *          the yuno's periodic timeout, for `listen_retry_msec`.
 *
 *          The port is held here by a raw listening socket. The yuno's tick
 *          is 50 ms (main.c), and the C_TIMER of this gobj subscribes to it
 *          before the one of the C_TCP_S: on a tick where both expire, this
 *          one fires first. That is what makes the count of failed binds
 *          exact.
 *
 *          Phase 1, the port held at the start, released before the retry:
 *              one bind() FAILED (a WARNING), "Address in use: listening
 *              again", and at the retry "Listening: its address in use was
 *              released". The server is in ST_IDLE and a client connects
 *              (a channel opens).
 *
 *          Phase 2, held for longer than `listen_retry_msec` (1 ms): the
 *              retry fails too, and the start fails as any other listen
 *              ("yev_create_accept_event() FAILED", with its errno).
 *
 *          Phase 3, `listen_retry_msec` 0: no retry, the start fails at
 *              once, as up to 7.27.3.
 *
 *          A wrong verdict is logged as an error, which the expected-logs
 *          check of main.c does not expect.
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
#include "c_test_addr_in_use.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_PORT   7796

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int hold_port(hgobj gobj);
PRIVATE void release_port(hgobj gobj);
PRIVATE int connect_client(hgobj gobj);
PRIVATE void check_state(hgobj gobj, gobj_state_t expected, const char *phase);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj gobj_input_side;
    hgobj server_port;
    int fd_hold;        // the raw socket holding the port
    int fd_client;
    int step;
    int opens;
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

    priv->fd_hold = -1;
    priv->fd_client = -1;
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Started now, it subscribes to the yuno's tick before the C_TIMER of
     *  the C_TCP_S, which starts at its first retry
     */
    gobj_start(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
    release_port(gobj);
    if(priv->fd_client >= 0) {
        close(priv->fd_client);
        priv->fd_client = -1;
    }
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    priv->server_port = gobj_find_child(
        priv->gobj_input_side,
        json_pack("{s:s}", "__gclass_name__", C_TCP_S)
    );
    gobj_subscribe_event(priv->gobj_input_side, NULL, 0, gobj);

    /*
     *  Phase 1: the port held at the start. This timer is armed BEFORE the
     *  one of the retry, with the same time: it fires first, and releases
     *  the port before the retry.
     */
    hold_port(gobj);
    set_timeout(priv->timer, 100);
    gobj_start_tree(priv->gobj_input_side);
    check_state(gobj, ST_STOPPED, "the port held at the start");
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
 *  A raw socket listening on the test port: what the dead yuno's ring
 *  holds in the real case
 ***************************************************************************/
PRIVATE int hold_port(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if(fd < 0 ||
        bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
        listen(fd, 8) < 0
    ) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "cannot hold the test port",
            "errno",        "%s", strerror(errno),
            NULL
        );
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    priv->fd_hold = fd;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void release_port(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fd_hold >= 0) {
        close(priv->fd_hold);
        priv->fd_hold = -1;
    }
}

/***************************************************************************
 *  A client of the server: it listens for real when a channel opens
 ***************************************************************************/
PRIVATE int connect_client(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dst = {0};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(TEST_PORT);
    inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr);
    if(fd < 0 || connect(fd, (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "connect() FAILED",
            "errno",        "%s", strerror(errno),
            NULL
        );
        if(fd >= 0) {
            close(fd);
        }
        return -1;
    }
    priv->fd_client = fd;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void check_state(hgobj gobj, gobj_state_t expected, const char *phase)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_state_t state = gobj_current_state(priv->server_port);
    if(state != expected) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "C_TCP_S in a wrong state",
            "phase",        "%s", phase,
            "expected",     "%s", expected,
            "got",          "%s", state,
            NULL
        );
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Each timeout is one step of the test
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->step++) {
        case 0:
            /*
             *  Phase 1: released before the retry, which comes in this
             *  tick or a later one
             */
            release_port(gobj);
            set_timeout(priv->timer, 1000);     // ample for the retry on a busy box
            break;

        case 1:
            check_state(gobj, ST_IDLE, "the port released before the retry");
            if(empty_string(gobj_read_str_attr(priv->server_port, "lPort"))) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "C_TCP_S listening with no lPort",
                    NULL
                );
            }
            connect_client(gobj);
            set_timeout(priv->timer, 300);
            break;

        case 2:
            if(priv->opens != 1) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "the server did not accept after its retry",
                    "opens",        "%d", priv->opens,
                    NULL
                );
            }
            close(priv->fd_client);
            priv->fd_client = -1;
            gobj_stop(priv->server_port);
            set_timeout(priv->timer, 300);
            break;

        case 3:
            /*
             *  Phase 2: held for longer than listen_retry_msec. This timer
             *  is longer than the retry's, which fails before it
             */
            check_state(gobj, ST_STOPPED, "stopped after phase 1");
            hold_port(gobj);
            gobj_write_integer_attr(priv->server_port, "listen_retry_msec", 1);
            gobj_start(priv->server_port);
            set_timeout(priv->timer, 1000);     // ample for the retry on a busy box
            break;

        case 4:
            check_state(gobj, ST_STOPPED, "the retries over");

            /*
             *  Phase 3: no retry, the start fails at once
             */
            gobj_stop(priv->server_port);
            gobj_write_integer_attr(priv->server_port, "listen_retry_msec", 0);
            gobj_start(priv->server_port);
            check_state(gobj, ST_STOPPED, "no retry");
            set_timeout(priv->timer, 300);
            break;

        default:
            release_port(gobj);
            set_yuno_must_die();
            break;
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A channel opened: the client of phase 1
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->opens++;

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A channel closed: the client of phase 1, when the test closes it
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
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
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_ADDR_IN_USE);

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
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_ON_OPEN,                ac_on_open,                 0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_STOPPED,                0},
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
        s_user_trace_level,  // s_user_trace_level,
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
PUBLIC int register_c_test_addr_in_use(void)
{
    return create_gclass(C_TEST_ADDR_IN_USE);
}
