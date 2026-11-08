/***********************************************************************
 *          C_TEST12.C
 *
 *          A class to test C_TCP
 *          Test: the inactivity timer of a client does not fire into
 *          ST_WAIT_STOPPED after a local drop.
 *
 *          The C_TCP (timeout_inactivity 1000, timeout_stop_tx 0: no bound)
 *          connects to a listener of the test with a small receive buffer
 *          that nobody accepts: the kernel completes the handshake and
 *          takes a few KB, and then the window is zero. The C_TCP is sent
 *          8 MB at once, so a write stays in flight. 300 ms after the
 *          connection it is dropped: the drop waits in ST_WAIT_STOPPED for
 *          that write. The inactivity timer, armed while connected, must
 *          not fire in that wait: 3 s later (three times
 *          timeout_inactivity) the C_TCP must still be in ST_WAIT_STOPPED,
 *          with nothing logged, and the test ends (the yuno's end destroys
 *          the C_TCP still closing, which is legal).
 *
 *          Up to 7.30.16 the drop left the inactivity timer armed, and its
 *          EV_TIMEOUT landed in ST_WAIT_STOPPED (red: "Event NOT DEFINED
 *          in state").
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "c_test12.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CHUNKS          8
#define CHUNK_SIZE      (1024*1024)
#define DROP_AFTER_MS   300
#define WAIT_MS         3000    // three times timeout_inactivity
#define WATCHDOG_MS     15000

enum {
    PHASE_CONNECT = 0,      // waiting the connection
    PHASE_FILL,             // the writes go, until the window closes
    PHASE_CLOSING,          // dropped, waiting in ST_WAIT_STOPPED for the write in flight
    PHASE_END
};

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
    hgobj gobj_tcp;
    int fd_listen;
    int phase;
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
     *  A listener of the test, on a port of the kernel's choice, with a
     *  small receive buffer (its connections inherit it), never accepted
     */
    char url[PATH_MAX] = "";
    priv->fd_listen = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t addrlen = sizeof(addr);
    int rcvbuf = 4096;
    if(priv->fd_listen < 0 ||
            setsockopt(priv->fd_listen, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf))<0 ||
            bind(priv->fd_listen, (struct sockaddr *)&addr, sizeof(addr))<0 ||
            listen(priv->fd_listen, 4)<0 ||
            getsockname(priv->fd_listen, (struct sockaddr *)&addr, &addrlen)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot create the listener",
            NULL
        );
    } else {
        snprintf(url, sizeof(url), "tcp://127.0.0.1:%d", (int)ntohs(addr.sin_port));
    }

    priv->gobj_tcp = gobj_create(
        "tcp_client",
        C_TCP,
        json_pack("{s:s, s:i, s:i, s:i}",
            "url", url,
            "timeout_inactivity", 1000,
            "timeout_stop_tx", 0,
            "timeout_between_connections", 60000
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

    if(priv->fd_listen >= 0) {
        close(priv->fd_listen);
        priv->fd_listen = -1;
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
    if(gobj_is_running(priv->gobj_tcp)) {
        gobj_stop(priv->gobj_tcp);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_tcp);
    set_timeout(priv->timer, WATCHDOG_MS);

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
 *
 ***************************************************************************/
PRIVATE void test_fail(hgobj gobj, const char *what)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        NULL
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Connected: 8 MB at once, more than the window and the buffers take
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase == PHASE_CONNECT) {
        priv->phase = PHASE_FILL;
        for(int i = 0; i < CHUNKS; i++) {
            gbuffer_t *gbuf = gbuffer_create(CHUNK_SIZE, CHUNK_SIZE);
            memset(gbuffer_cur_wr_pointer(gbuf), 'x', CHUNK_SIZE);
            gbuffer_set_wr(gbuf, CHUNK_SIZE);
            gobj_send_event(
                priv->gobj_tcp,
                EV_TX_DATA,
                json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
                gobj
            );
        }
        set_timeout(priv->timer, DROP_AFTER_MS);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The write in flight ended before the wait did: nothing tested
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase != PHASE_END) {
        test_fail(gobj, "TEST: disconnected before the end of the wait: nothing tested");
        priv->phase = PHASE_END;
        clear_timeout(priv->timer);
        set_yuno_must_die();
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The other events of the transport: nothing to do
 ***************************************************************************/
PRIVATE int ac_transport_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The phases
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase) {
        case PHASE_FILL:
            /*
             *  The drop: it must wait for the write in flight
             */
            priv->phase = PHASE_CLOSING;
            gobj_send_event(priv->gobj_tcp, EV_DROP, 0, gobj);
            if(!gobj_in_this_state(priv->gobj_tcp, ST_WAIT_STOPPED)) {
                test_fail(gobj, "TEST: the drop did not wait (not in ST_WAIT_STOPPED): nothing tested");
                set_yuno_must_die();
                break;
            }
            set_timeout(priv->timer, WAIT_MS);
            break;

        case PHASE_CLOSING:
            /*
             *  Past the inactivity timeout, still closing, nothing fired
             */
            priv->phase = PHASE_END;
            if(!gobj_in_this_state(priv->gobj_tcp, ST_WAIT_STOPPED)) {
                test_fail(gobj, "TEST: the drop did not wait long enough: nothing tested");
            }
            set_yuno_must_die();
            break;

        case PHASE_CONNECT:
            test_fail(gobj, "TEST: the C_TCP did not connect");
            set_yuno_must_die();
            break;

        default:
            test_fail(gobj, "TEST: unexpected phase");
            set_yuno_must_die();
            break;
    }

    JSON_DECREF(kw)
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
GOBJ_DEFINE_GCLASS(C_TEST12);

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
        {EV_CONNECTED,          ac_connected,       0},
        {EV_DISCONNECTED,       ac_disconnected,    0},
        {EV_STOPPED,            ac_transport_event, 0},
        {EV_RX_DATA,            ac_transport_event, 0},
        {EV_TX_READY,           ac_transport_event, 0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_CONNECTED,          0},
        {EV_DISCONNECTED,       0},
        {EV_STOPPED,            0},
        {EV_RX_DATA,            0},
        {EV_TX_READY,           0},
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
PUBLIC int register_c_test12(void)
{
    return create_gclass(C_TEST12);
}
