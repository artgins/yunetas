/***********************************************************************
 *          C_TEST11.C
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: a TLS handshake that does not end is dropped at
 *          timeout_tls_handshake, by a server and by a client.
 *
 *          The server is wss, with 2 channels (C_CHANNEL -> C_WEBSOCKET ->
 *          C_TCP) and a timeout_tls_handshake of 1 s. Two raw TCP clients
 *          (C_PROT_RAW, no TLS) connect: one sends nothing, the other one
 *          byte, 0x16 (the first byte of a TLS record). The server must
 *          drop both at timeout_tls_handshake (not before 0.9 s, and with a
 *          wide margin, before 5 s). Then a raw
 *          client sends a line that is not TLS to a TLS server of the new
 *          method (no child_tree_filter: its clisrv stays running between
 *          connections): the server drops it at once, for the TLS error,
 *          and its deadline must not fire after that (a quiet wait of
 *          3 s). Then a wss client must open its session on the first
 *          server (the channels are free again) and keep it past the
 *          deadline: the deadline ends with the handshake.
 *
 *          The client: a wss client (timeout_tls_handshake 1 s) connects
 *          to a plain TCP server that accepts and says nothing. The client
 *          must drop the connection at timeout_tls_handshake and connect
 *          again (its reconnect): the second connection seen by the mute
 *          server ends the test.
 *
 *          The three deadlines come within 10 s: they write ONE warning,
 *          the first ("suppressed" 0), as every deadline of a yuno does in
 *          a window of 10 s (c_tcp's test11 counts the next window).
 *
 *          Up to 7.30.16 C_TCP armed no timer while its TLS handshake ran,
 *          and C_WEBSOCKET's timeout_handshake starts only at EV_CONNECTED,
 *          which over TLS comes when the handshake ends: the two raw
 *          clients held both channels for ever, and the wss client waited
 *          for ever, never connecting again (red: "A TLS handshake that
 *          never ends held its connection").
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <limits.h>
#include <string.h>

#include "c_test11.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MUTE_CLIENTS        2       // case-0 sends nothing, case-1 one byte, 0x16
#define CLOSE_NOT_BEFORE_MS 900     // timeout_tls_handshake is 1000
#define CLOSE_BEFORE_MS     5000    // a wide margin: the test machine is busy
#define QUIET_MS            3000    // past the deadline the bad client's handshake had
#define WATCHDOG_MS         15000

enum {
    PHASE_START = 0,        // the servers listen
    PHASE_MUTE,             // the two raw clients of nothing and of 0x16
    PHASE_BAD,              // the raw client of a line that is not TLS
    PHASE_QUIET,            // its connection ended: nothing may fire after it
    PHASE_WSS,              // the wss client of the wss server
    PHASE_CLIENT,           // the wss client of the silent server
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
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
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

    hgobj gobj_input_side;      // the wss server
    hgobj gobj_output_side;     // the raw clients, no TLS
    hgobj gobj_output_side2;    // the wss client of the wss server
    hgobj gobj_mute_side;       // the plain server that says nothing
    hgobj gobj_output_side3;    // the wss client of the mute server
    hgobj gobj_new_side;        // a tls server of the new method (no child_tree_filter)
    hgobj gobj_bad_side;        // its raw client of a line that is not TLS

    uint64_t not_before[MUTE_CLIENTS];  // msectimers from the connection
    uint64_t before[MUTE_CLIENTS];
    BOOL closed[MUTE_CLIENTS];
    int mute_closed;
    int phase;
    uint64_t bad_before;

    int mute_server_opens;
    BOOL mute_server_closed;
    uint64_t mute_not_before;
    uint64_t mute_before;
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

    gobj_stop(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    priv->gobj_mute_side = gobj_find_service("__mute_side__", TRUE);
    gobj_subscribe_event(priv->gobj_mute_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_mute_side);

    priv->gobj_new_side = gobj_find_service("__new_side__", TRUE);
    gobj_subscribe_event(priv->gobj_new_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_new_side);

    priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
    priv->gobj_output_side2 = gobj_find_service("__output_side2__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side2, NULL, 0, gobj);
    priv->gobj_output_side3 = gobj_find_service("__output_side3__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side3, NULL, 0, gobj);
    priv->gobj_bad_side = gobj_find_service("__bad_side__", TRUE);
    gobj_subscribe_event(priv->gobj_bad_side, NULL, 0, gobj);

    set_timeout(priv->timer, 1000); // then the raw clients connect

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

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The case of a raw client is the number of its channel, "case-<n>"
 ***************************************************************************/
PRIVATE int case_of_channel(hgobj channel)
{
    return atoi(gobj_name(channel) + strlen("case-"));
}

/***************************************************************************
 *  A client connects: the C_TCP of its channel, started by its host
 ***************************************************************************/
PRIVATE void start_client(hgobj iogate, const char *name)
{
    hgobj channel = gobj_child_by_name(iogate, name);
    hgobj top = gobj_child_by_name(channel, name);
    gobj_start(gobj_child_by_name(top, name));
}

/***************************************************************************
 *  A client sends these bytes
 ***************************************************************************/
PRIVATE void send_bytes(hgobj gobj, hgobj channel, const char *bytes, size_t len)
{
    gbuffer_t *gbuf = gbuffer_create(len, len);
    gbuffer_append(gbuf, (void *)bytes, len);
    gobj_send_event(
        channel,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_fail(const char *what, int n)
{
    gobj_log_error(0, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "case",         "%d", n,
        NULL
    );
}

/***************************************************************************
 *  A drop must come at the deadline: not before, not long after
 ***************************************************************************/
PRIVATE void check_deadline(uint64_t not_before, uint64_t before, int n)
{
    if(!test_msectimer(not_before)) {
        test_fail("A TLS handshake was dropped before timeout_tls_handshake", n);
    }
    if(test_msectimer(before)) {
        test_fail("A TLS handshake was dropped long after timeout_tls_handshake", n);
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  First timeout: the servers listen, the raw clients connect.
 *  The second one: the watchdog.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase) {
        case PHASE_START:
            priv->phase = PHASE_MUTE;
            gobj_start_tree(priv->gobj_output_side);
            for(int n = 0; n < MUTE_CLIENTS; n++) {
                char name[NAME_MAX];
                snprintf(name, sizeof(name), "case-%d", n);
                start_client(priv->gobj_output_side, name);
            }
            set_timeout(priv->timer, WATCHDOG_MS);
            break;

        case PHASE_QUIET:
            /*
             *  Nothing fired on the server while its deadlines would have:
             *  the wss client connects
             */
            priv->phase = PHASE_WSS;
            gobj_start_tree(priv->gobj_output_side2);
            start_client(priv->gobj_output_side2, "wss_client");
            set_timeout(priv->timer, WATCHDOG_MS);
            break;

        case PHASE_MUTE:
            test_fail("A TLS handshake that never ends held its connection", priv->mute_closed);
            set_yuno_must_die();
            break;

        case PHASE_BAD:
            test_fail("The server kept a connection that is not TLS", -1);
            set_yuno_must_die();
            break;

        case PHASE_WSS:
            test_fail("The wss client found no free channel", -1);
            set_yuno_must_die();
            break;

        default:
            test_fail("The wss client did not drop the silent server and connect again",
                priv->mute_server_opens
            );
            set_yuno_must_die();
            break;
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  An opening
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side) {
        /*
         *  A raw client connected: nothing, or one byte
         */
        hgobj channel = (hgobj)(uintptr_t)kw_get_int(
            gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED
        );
        int n = case_of_channel(channel);
        if(n >= 0 && n < MUTE_CLIENTS) {
            priv->not_before[n] = start_msectimer(CLOSE_NOT_BEFORE_MS);
            priv->before[n] = start_msectimer(CLOSE_BEFORE_MS);
            if(n == 1) {
                send_bytes(gobj, channel, "\x16", 1);
            }
        }

    } else if(src == priv->gobj_bad_side) {
        /*
         *  A line that is not TLS
         */
        hgobj channel = (hgobj)(uintptr_t)kw_get_int(
            gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED
        );
        priv->bad_before = start_msectimer(CLOSE_NOT_BEFORE_MS);
        send_bytes(gobj, channel, "hello\r\n\r\n", 9);

    } else if(src == priv->gobj_new_side) {
        test_fail("A connection with no TLS opened a session", -1);

    } else if(src == priv->gobj_input_side) {
        if(priv->phase < PHASE_WSS) {
            test_fail("A connection with no TLS opened a session", priv->phase);
        }

    } else if(src == priv->gobj_output_side2) {
        /*
         *  The wss client opened its session: the server's part is done,
         *  the client's begins
         */
        if(priv->phase == PHASE_WSS) {
            priv->phase = PHASE_CLIENT;
            gobj_start_tree(priv->gobj_output_side3);
            start_client(priv->gobj_output_side3, "tls_client");
        }

    } else if(src == priv->gobj_mute_side) {
        priv->mute_server_opens++;
        if(priv->mute_server_opens == 1) {
            priv->mute_not_before = start_msectimer(CLOSE_NOT_BEFORE_MS);
            priv->mute_before = start_msectimer(CLOSE_BEFORE_MS);
        } else {
            /*
             *  The client connected again: the end
             */
            if(!priv->mute_server_closed) {
                test_fail("The wss client connected again with its first connection open", -1);
            }
            priv->phase = PHASE_END;
            clear_timeout(priv->timer);
            set_yuno_must_die();
        }
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A closing
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side) {
        /*
         *  The server dropped a raw client: at timeout_tls_handshake. When
         *  both are dropped, the wss client connects.
         */
        hgobj channel = (hgobj)(uintptr_t)kw_get_int(
            gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED
        );
        int n = case_of_channel(channel);
        if(n >= 0 && n < MUTE_CLIENTS && !priv->closed[n]) {
            priv->closed[n] = TRUE;
            priv->mute_closed++;
            check_deadline(priv->not_before[n], priv->before[n], n);
            if(priv->mute_closed == MUTE_CLIENTS && priv->phase == PHASE_MUTE) {
                priv->phase = PHASE_BAD;
                gobj_start_tree(priv->gobj_bad_side);
                start_client(priv->gobj_bad_side, "bad_client");
            }
        }

    } else if(src == priv->gobj_bad_side) {
        /*
         *  Dropped for its bytes, at once: then a while with nothing,
         *  longer than the deadline its handshake had. The clisrv of the
         *  new method stays running, waiting for its next accept: a
         *  deadline left armed would fire there.
         */
        if(priv->phase == PHASE_BAD) {
            if(test_msectimer(priv->bad_before)) {
                test_fail("A connection that is not TLS was dropped late", -1);
            }
            priv->phase = PHASE_QUIET;
            set_timeout(priv->timer, QUIET_MS);
        }

    } else if(src == priv->gobj_output_side2) {
        if(priv->phase != PHASE_END) {
            test_fail("The wss session was dropped after its handshake", priv->phase);
        }

    } else if(src == priv->gobj_mute_side) {
        /*
         *  The wss client dropped the silent server: at timeout_tls_handshake
         */
        if(!priv->mute_server_closed) {
            priv->mute_server_closed = TRUE;
            check_deadline(priv->mute_not_before, priv->mute_before, -1);
        }
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the raw sides receive: nothing to check
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
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
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST11);

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
        {EV_ON_OPEN,                ac_on_open,                 0},
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_MESSAGE,             0},
        {EV_ON_CLOSE,               0},
        {EV_TIMEOUT,                0},
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
PUBLIC int register_c_test11(void)
{
    return create_gclass(C_TEST11);
}
