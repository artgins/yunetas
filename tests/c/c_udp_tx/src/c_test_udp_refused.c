/***********************************************************************
 *          C_TEST_UDP_REFUSED.C
 *
 *          Driver of the C_UDP test of a peer that refuses.
 *
 *          A C_UDP child sends to a port of the loopback where nobody
 *          listens (a port the kernel gave to a socket of the test, closed
 *          before the start). The kernel answers the first datagram with
 *          "port unreachable", and the read in flight fails with
 *          ECONNREFUSED while the next datagram is being sent:
 *
 *              1. 50 datagrams in one turn: the read fails, and the C_UDP
 *                 stops by itself (an ERROR, "UDP: read FAILED, the client
 *                 stops"). 500 ms later it must be in ST_STOPPED, NOT
 *                 running, with ONE EV_STOPPED. In the fixes after 7.29.3
 *                 the stop waited for the send in flight in
 *                 ST_WAIT_STOPPED for ever (only a gobj not running ended
 *                 it at the completion of a send), or, with no send in
 *                 flight, it ended with the gobj still running.
 *              2. a start must be taken (ST_IDLE): in the fixes after
 *                 7.29.3 it said "GObj ALREADY RUNNING". Then a stop: a
 *                 second EV_STOPPED.
 *              3. started again, RESTARTS times: a datagram, a stop (it
 *                 must wait in ST_WAIT_STOPPED, for the send and the
 *                 cancel of the read) and a start in one turn. The start
 *                 is taken when the stop ends: RESTART_MS later the C_UDP
 *                 must run in ST_IDLE, with no new EV_STOPPED. In the
 *                 fixes after 7.29.3 the start armed the read event again
 *                 while the completion of its cancel was still to come;
 *                 that completion landed on the new read, which failed,
 *                 and the C_UDP stopped by itself ("UDP: read FAILED, the
 *                 client stops") at every restart.
 *              4. RESTARTS times: a stop (in ST_WAIT_STOPPED, for the
 *                 cancel of the read), and a start made at its EV_STOPPED,
 *                 inside the action: the stop ended at the completion of
 *                 the read canceled, and the completion of the cancel is
 *                 still to come. RESTART_MS later the C_UDP must run in
 *                 ST_IDLE, with one EV_STOPPED per stop. In the fixes
 *                 after 7.29.3 that completion failed the read armed
 *                 again, and the C_UDP stopped by itself at every restart.
 *              5. a stop: the last EV_STOPPED, and the yuno dies; the
 *                 memory check at the end catches a leaked event or
 *                 gbuffer.
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
#include "c_test_udp_refused.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SETTLE_MS       300
#define DATAGRAMS       50
#define RESTARTS        25
#define RESTART_MS      100

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
    int phase;
    int stopped;
    int restarts;
    int restarts_at_stopped;
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
     *  A port of the loopback where nobody listens: the kernel gives it
     *  to a socket of the test, which is closed at once
     */
    char url[64] = "";
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t addrlen = sizeof(addr);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0 ||
            bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
            getsockname(fd, (struct sockaddr *)&addr, &addrlen) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot get a free port",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    } else {
        snprintf(url, sizeof(url), "udp://127.0.0.1:%d", (int)ntohs(addr.sin_port));
    }
    if(fd >= 0) {
        close(fd);
    }

    priv->gobj_udp = gobj_create(
        "udp_refused",
        C_UDP,
        json_pack("{s:s}",
            "url", url
        ),
        gobj
    );
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
 *  A stop of a C_UDP with its read in flight must wait in ST_WAIT_STOPPED
 ***************************************************************************/
PRIVATE void stop_and_check_it_waits(hgobj gobj, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->gobj_udp);
    if(!gobj_in_this_state(priv->gobj_udp, ST_WAIT_STOPPED)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", what,
            "state",        "%s", gobj_current_state(priv->gobj_udp),
            "restart",      "%d", priv->restarts,
            "restart_at_stopped", "%d", priv->restarts_at_stopped,
            NULL
        );
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void check_udp(hgobj gobj, const char *what, gobj_state_t state, BOOL running, int stopped)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_in_this_state(priv->gobj_udp, state) ||
            (gobj_is_running(priv->gobj_udp)?TRUE:FALSE) != running ||
            priv->stopped != stopped) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", what,
            "state",        "%s", gobj_current_state(priv->gobj_udp),
            "running",      "%d", gobj_is_running(priv->gobj_udp),
            "stopped",      "%d", priv->stopped,
            "expected_stopped", "%d", stopped,
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

    switch(priv->phase) {
        case 0:
            for(int i = 0; i < DATAGRAMS; i++) {
                gbuffer_t *gbuf = gbuffer_create(64, 64);
                gbuffer_append_string(gbuf, "nobody listens");
                gobj_send_event(
                    priv->gobj_udp,
                    EV_TX_DATA,
                    json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
                    gobj
                );
            }
            priv->phase = 1;
            set_timeout(priv->timer, 500);
            break;

        case 1:
            check_udp(gobj, "TEST: the C_UDP whose read failed did not end its stop",
                ST_STOPPED, FALSE, 1);
            if(gobj_start(priv->gobj_udp) < 0 || !gobj_in_this_state(priv->gobj_udp, ST_IDLE)) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST: the C_UDP stopped by its read did not start again",
                    "state",        "%s", gobj_current_state(priv->gobj_udp),
                    NULL
                );
            }
            gobj_stop(priv->gobj_udp);
            priv->phase = 2;
            set_timeout(priv->timer, SETTLE_MS);
            break;

        case 2:
            check_udp(gobj, "TEST: the stop of the C_UDP started again did not end",
                ST_STOPPED, FALSE, 2);
            if(gobj_start(priv->gobj_udp) < 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST: the C_UDP did not start for the restarts",
                    "state",        "%s", gobj_current_state(priv->gobj_udp),
                    NULL
                );
            }
            priv->phase = 3;
            set_timeout(priv->timer, RESTART_MS);
            break;

        case 3:
            check_udp(gobj, "TEST: a C_UDP started while its stop waited did not stay running",
                ST_IDLE, TRUE, 2);
            if(priv->restarts < RESTARTS) {
                priv->restarts++;
                gbuffer_t *gbuf = gbuffer_create(64, 64);
                gbuffer_append_string(gbuf, "nobody listens");
                gobj_send_event(
                    priv->gobj_udp,
                    EV_TX_DATA,
                    json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
                    gobj
                );
                stop_and_check_it_waits(gobj,
                    "TEST: the stop with a send and the read in flight did not wait"
                );
                if(gobj_start(priv->gobj_udp) < 0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "TEST: a start while the stop waits was refused",
                        "state",        "%s", gobj_current_state(priv->gobj_udp),
                        "restart",      "%d", priv->restarts,
                        NULL
                    );
                }
                set_timeout(priv->timer, RESTART_MS);
                break;
            }
            priv->phase = 4;
            set_timeout(priv->timer, RESTART_MS);
            break;

        case 4:
            check_udp(gobj, "TEST: a C_UDP started at its EV_STOPPED did not stay running",
                ST_IDLE, TRUE, 2 + priv->restarts_at_stopped);
            if(priv->restarts_at_stopped < RESTARTS) {
                stop_and_check_it_waits(gobj,
                    "TEST: the stop with the read in flight did not wait"
                );
                set_timeout(priv->timer, RESTART_MS);  // ac_stopped() starts it
                break;
            }
            gobj_stop(priv->gobj_udp);
            priv->phase = 5;
            set_timeout(priv->timer, SETTLE_MS);
            break;

        default:
            check_udp(gobj, "TEST: the last stop of the C_UDP did not end",
                ST_STOPPED, FALSE, 3 + RESTARTS);
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
 *  The other events of the C_UDP: nothing to do
 ***************************************************************************/
PRIVATE int ac_transport_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
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

    if(priv->phase == 4) {
        priv->restarts_at_stopped++;
        if(gobj_start(src) < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST: a start at EV_STOPPED was refused",
                "state",        "%s", gobj_current_state(src),
                "restart_at_stopped", "%d", priv->restarts_at_stopped,
                NULL
            );
        }
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
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_UDP_REFUSED);

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
        {EV_TIMEOUT,        ac_timeout,         0},
        {EV_RX_DATA,        ac_transport_event, 0},
        {EV_TX_READY,       ac_transport_event, 0},
        {EV_STOPPED,        ac_stopped,         0},
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
PUBLIC int register_c_test_udp_refused(void)
{
    return create_gclass(C_TEST_UDP_REFUSED);
}
