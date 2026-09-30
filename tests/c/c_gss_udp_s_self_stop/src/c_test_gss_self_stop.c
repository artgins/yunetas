/***********************************************************************
 *          C_TEST_GSS_SELF_STOP.C
 *
 *          Driver of the C_GSS_UDP_S test: its C_UDP_S stops by itself.
 *
 *          A C_GSS_UDP_S child (timeout_base 1 s, disable_end_of_frame, so
 *          each datagram comes as it is, in the gbuffer C_UDP_S read it
 *          into) and a plain UDP socket, the peer.
 *
 *              -. (create) A C_GSS_UDP_S with timeout_base 0: refused with a
 *                 warning, the default 5000 used.
 *              0. The C_UDP_S's rx_buffer_size goes down to 0 and the peer
 *                 sends "one". The host keeps that gbuffer and, in the
 *                 same stack, sends "early" (a send in flight) and posts
 *                 itself an event. The next read needs a new gbuffer, of 0
 *                 bytes: it cannot be started again, and C_UDP_S stops by
 *                 itself, in ST_WAIT_STOPPED until "early" completes. The
 *                 posted event comes first: the host sends "wait" to a
 *                 C_UDP_S still stopping: refused, ONE warning. Then
 *                 EV_STOPPED.
 *              1. (50 ms) rx_buffer_size back to 4096, the gbuffer
 *                 released, and the host sends "lost": refused, no second
 *                 warning.
 *              2. (after timeout_base) the peer sends "two".
 *              3. The host must have got "two", and sends "back".
 *              4. The peer must have got "early" and "back", nothing else.
 *                 Then the C_UDP_S is made to stop by itself again ("three"
 *                 kept, rx_buffer_size 0), within RESTART_HOLD_MS of its
 *                 restart: the backoff doubles, 2 s.
 *              5. (1.3 s after that stop) it must still be down.
 *              6. (2.3 s) it must be up again. Then it is stopped from
 *                 outside (gobj_stop() of the C_UDP_S): said, not started
 *                 again, and a send meanwhile refused.
 *              7. (1.2 s) it must still be down. The C_GSS_UDP_S is then
 *                 stopped and started again (a pause and a play of its host):
 *                 at its stop it says the sends refused since, and its start
 *                 finds its timers stopped (no "GObj ALREADY RUNNING").
 *              8. The peer sends "four": the host must get it.
 *
 *          Up to 7.25.20 C_GSS_UDP_S took the EV_STOPPED of its C_UDP_S
 *          with no action: it went on sending to it ("Event NOT DEFINED in
 *          state" per datagram, from C_UDP_S in ST_STOPPED), and the
 *          service could not receive again.
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
#include "c_test_gss_self_stop.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define GSS_PORT    34296
#define GSS_URL     "udp://127.0.0.1:34296"
#define PEER_PORT   34297
#define GSS0_URL    "udp://127.0.0.1:34298"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void peer_send(hgobj gobj, const char *data);
PRIVATE void host_send(hgobj gobj, const char *data);
PRIVATE void peer_recv_all(hgobj gobj, char *bf, size_t size);
PRIVATE void check(hgobj gobj, BOOL ok, const char *what);

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
    hgobj gobj_gss;
    hgobj gobj_udp_s;
    int peer_fd;
    int phase;
    BOOL keep_next;
    gbuffer_t *kept;
    int failures;
    char received[128];     // what the host got, in order
    struct sockaddr_in peer_addr;
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

    priv->peer_fd = -1;
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->gobj_gss = gobj_create(
        "gss",
        C_GSS_UDP_S,
        json_pack("{s:s, s:i, s:b}",
            "url", GSS_URL,
            "timeout_base", 1000,
            "disable_end_of_frame", 1
        ),
        gobj
    );
    priv->gobj_udp_s = gobj_find_child(
        priv->gobj_gss,
        json_pack("{s:s}", "__gclass_name__", C_UDP_S)
    );

    /*
     *  timeout_base 0: refused, the default used
     */
    hgobj gss0 = gobj_create(
        "gss0",
        C_GSS_UDP_S,
        json_pack("{s:s, s:i}",
            "url", GSS0_URL,
            "timeout_base", 0
        ),
        gobj
    );
    check(gobj,
        gobj_read_integer_attr(gss0, "timeout_base") == 5000,
        "timeout_base 0 not replaced by the default 5000"
    );
    gobj_destroy(gss0);
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->kept)
    if(priv->peer_fd >= 0) {
        close(priv->peer_fd);
        priv->peer_fd = -1;
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
    gobj_stop(priv->gobj_gss);

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
    memset(&priv->peer_addr, 0, sizeof(priv->peer_addr));
    priv->peer_addr.sin_family = AF_INET;
    priv->peer_addr.sin_port = htons(PEER_PORT);
    priv->peer_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    priv->server_addr = priv->peer_addr;
    priv->server_addr.sin_port = htons(GSS_PORT);

    priv->peer_fd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
    if(priv->peer_fd < 0 ||
            bind(priv->peer_fd, (struct sockaddr *)&priv->peer_addr, sizeof(priv->peer_addr)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot bind the peer socket",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        set_yuno_must_die();
        return -1;
    }

    /*
     *  0. The next read will need a gbuffer of 0 bytes; the peer sends
     */
    gobj_write_integer_attr(priv->gobj_udp_s, "rx_buffer_size", 0);
    priv->keep_next = TRUE;
    peer_send(gobj, "one");

    set_timeout(priv->timer, 50);
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
PRIVATE void peer_send(hgobj gobj, const char *data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(sendto(priv->peer_fd, data, strlen(data), 0,
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
 *  What the peer got, the datagrams separated by a space
 ***************************************************************************/
PRIVATE void peer_recv_all(hgobj gobj, char *bf, size_t size)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    bf[0] = 0;
    char got[64];
    ssize_t n;
    while((n = recv(priv->peer_fd, got, sizeof(got) - 1, 0)) > 0) {
        got[n] = 0;
        size_t len = strlen(bf);
        snprintf(bf + len, size - len, "%s ", got);
    }
}

/***************************************************************************
 *  A failed check is an ERROR, and the test fails
 ***************************************************************************/
PRIVATE void check(hgobj gobj, BOOL ok, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!ok) {
        priv->failures++;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: check FAILED",
            "check",        "%s", what,
            "phase",        "%d", priv->phase,
            "udp_state",    "%s", priv->gobj_udp_s?gobj_current_state(priv->gobj_udp_s):"",
            NULL
        );
    }
}

/***************************************************************************
 *  The host sends to the peer, by its address
 ***************************************************************************/
PRIVATE void host_send(hgobj gobj, const char *data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(strlen(data), strlen(data));
    gbuffer_append_string(gbuf, data);
    gbuffer_setaddr(gbuf, (struct sockaddr *)&priv->peer_addr, sizeof(priv->peer_addr));
    gobj_send_event(
        priv->gobj_gss,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
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

    switch(priv->phase++) {
        case 0:
            gobj_write_integer_attr(priv->gobj_udp_s, "rx_buffer_size", 4096);
            GBUFFER_DECREF(priv->kept)
            host_send(gobj, "lost");
            set_timeout(priv->timer, 1200);
            break;

        case 1:
            peer_send(gobj, "two");
            set_timeout(priv->timer, 200);
            break;

        case 2:
            host_send(gobj, "back");
            set_timeout(priv->timer, 200);
            break;

        case 3:
            {
                char got[128];
                peer_recv_all(gobj, got, sizeof(got));
                check(gobj, strcmp(priv->received, "one two ")==0,
                    "the host did not get 'one two' (deaf after its C_UDP_S stopped by itself)"
                );
                check(gobj, strcmp(got, "early back ")==0,
                    "the peer did not get 'early back' (mute, or a refused send went out)"
                );

                /*
                 *  Stop by itself again, within RESTART_HOLD_MS of the restart
                 */
                gobj_write_integer_attr(priv->gobj_udp_s, "rx_buffer_size", 0);
                priv->keep_next = TRUE;
                peer_send(gobj, "three");
                set_timeout(priv->timer, 100);
            }
            break;

        case 4:
            gobj_write_integer_attr(priv->gobj_udp_s, "rx_buffer_size", 4096);
            GBUFFER_DECREF(priv->kept)
            check(gobj, !gobj_in_this_state(priv->gobj_udp_s, ST_IDLE),
                "the C_UDP_S did not stop by itself the second time"
            );
            set_timeout(priv->timer, 1200);
            break;

        case 5:
            check(gobj, !gobj_in_this_state(priv->gobj_udp_s, ST_IDLE),
                "the C_UDP_S started again before its doubled backoff (2 s)"
            );
            set_timeout(priv->timer, 1000);
            break;

        case 6:
            check(gobj, gobj_in_this_state(priv->gobj_udp_s, ST_IDLE),
                "the C_UDP_S did not start again after its doubled backoff"
            );

            /*
             *  Stopped from outside: not started again
             */
            gobj_stop(priv->gobj_udp_s);
            host_send(gobj, "after");
            set_timeout(priv->timer, 1200);
            break;

        case 7:
            {
                check(gobj, !gobj_is_running(priv->gobj_udp_s),
                    "the C_UDP_S stopped from outside was started again"
                );
                char got[128];
                peer_recv_all(gobj, got, sizeof(got));
                check(gobj, strcmp(priv->received, "one two three ")==0,
                    "the host got more or less than 'one two three'"
                );
                check(gobj, got[0] == 0, "the peer got a refused send");

                /*
                 *  The C_GSS_UDP_S stopped and started again, as a pause and
                 *  a play of its host do (logcenter)
                 */
                gobj_stop(priv->gobj_gss);
                gobj_start(priv->gobj_gss);
                set_timeout(priv->timer, 100);
            }
            break;

        case 8:
            peer_send(gobj, "four");
            set_timeout(priv->timer, 200);
            break;

        case 9:
            check(gobj, strcmp(priv->received, "one two three four ")==0,
                "the C_GSS_UDP_S started again does not hear"
            );
            if(!priv->failures) {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "TEST: C_GSS_UDP_S refused the sends while stopped or stopping, started it again with a backoff, and not after a stop from outside",
                    NULL
                );
            }
            set_yuno_must_die();
            break;

        default:
            check(gobj, FALSE, "unexpected phase");
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A datagram, as it came. The first one is kept: the next read of
 *  C_UDP_S then needs a new gbuffer.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gbuf) {
        size_t len = strlen(priv->received);
        snprintf(priv->received + len, sizeof(priv->received) - len, "%.*s ",
            (int)gbuffer_leftbytes(gbuf), (char *)gbuffer_cur_rd_pointer(gbuf)
        );
        if(priv->keep_next) {
            priv->keep_next = FALSE;
            priv->kept = gbuffer_incref(gbuf);
            if(priv->phase == 0) {
                /*
                 *  A send in flight when the C_UDP_S stops by itself, and a
                 *  send before its completion, while it is still stopping
                 */
                host_send(gobj, "early");
                gobj_post_event(gobj, EV_TEST_SEND_IN_WAIT, json_object(), gobj);
            }
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The C_UDP_S is in ST_WAIT_STOPPED ("early" in flight): send to it
 ***************************************************************************/
PRIVATE int ac_send_in_wait(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    check(gobj, gobj_in_this_state(priv->gobj_udp_s, ST_WAIT_STOPPED),
        "the C_UDP_S is not in ST_WAIT_STOPPED with a send in flight"
    );
    host_send(gobj, "wait");

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A peer opened or closed: nothing to do
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
GOBJ_DEFINE_GCLASS(C_TEST_GSS_SELF_STOP);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TEST_SEND_IN_WAIT);

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
        {EV_ON_MESSAGE,         ac_on_message,      0},
        {EV_ON_OPEN,            ac_channel,         0},
        {EV_ON_CLOSE,           ac_channel,         0},
        {EV_TEST_SEND_IN_WAIT,  ac_send_in_wait,    0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_ON_MESSAGE,         0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
        {EV_TEST_SEND_IN_WAIT,  0},
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
PUBLIC int register_c_test_gss_self_stop(void)
{
    return create_gclass(C_TEST_GSS_SELF_STOP);
}
