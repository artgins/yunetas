/***********************************************************************
 *          C_TEST6.C
 *
 *          A C_WEBSOCKET server closes a handshake left half done.
 *
 *          The server has 2 channels and a timeout_handshake of 1 s. Two
 *          raw clients (C_PROT_RAW) connect and send ONE byte each, "G",
 *          the start of a "GET": the server must close both at
 *          timeout_handshake (not before 0.9 s, and with a wide margin,
 *          before 5 s). Then a third raw client sends a whole upgrade
 *          request, and must get its 101: the channels the half handshakes
 *          held are free again. In that session it sends a text frame not
 *          final and another text frame not final: a data frame inside a
 *          fragmented message, which the server closes for with ONE
 *          warning (up to 7.30.15 with no log); the test ends when that
 *          connection is closed.
 *
 *          Up to 7.30.15 every read stopped the timer of the handshake and
 *          nothing armed it again: a peer that sent one byte held its
 *          connection, and a channel of the pool, for ever. The two clients
 *          stayed open and the third one was never served (red: "A half
 *          handshake held its connection").
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <limits.h>
#include <string.h>

#include "c_test6.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define HALF_CLIENTS    2       // case-0 and case-1: one byte each
#define FULL_CLIENT     2       // case-2: a whole upgrade, once the others are closed
#define CLOSE_NOT_BEFORE_MS 900     // timeout_handshake is 1000
#define CLOSE_BEFORE_MS     5000    // a wide margin: the test machine is busy
#define WATCHDOG_MS     10000
#define RFC_KEY         "dGhlIHNhbXBsZSBub25jZQ=="          // RFC 6455 1.3
#define RFC_ACCEPT      "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="      // its accept

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
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Timeout"),
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
    json_int_t timeout;
    hgobj timer;

    hgobj gobj_input_side;
    hgobj gobj_output_side;
    uint64_t not_before[HALF_CLIENTS];  // msectimers from the byte sent
    uint64_t before[HALF_CLIENTS];
    BOOL closed[HALF_CLIENTS];
    int half_closed;
    BOOL full_started;
    BOOL upgraded;          // the whole upgrade got its 101
    gbuffer_t *answer;      // the answer to the whole upgrade, until its head ends
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
    priv->answer = gbuffer_create(4096, 4096);

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
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

    set_timeout(priv->timer, 1000); // then the clients connect

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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->answer)
}



                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The case of a client is the number of its channel, "case-<n>"
 ***************************************************************************/
PRIVATE int case_of_channel(hgobj channel)
{
    return atoi(gobj_name(channel) + strlen("case-"));
}

/***************************************************************************
 *  A client connects: its C_TCP, which C_PROT_RAW leaves to its host
 ***************************************************************************/
PRIVATE void start_client(hgobj gobj, int n)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char name[NAME_MAX];
    snprintf(name, sizeof(name), "case-%d", n);
    hgobj channel = gobj_child_by_name(priv->gobj_output_side, name);
    hgobj prot = gobj_child_by_name(channel, name);
    gobj_start(gobj_child_by_name(prot, name));
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




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  First timeout: the server listens, the two clients of one byte connect.
 *  The second one: the watchdog.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_output_side) {
        priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
        gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
        gobj_start_tree(priv->gobj_output_side);
        for(int n = 0; n < HALF_CLIENTS; n++) {
            start_client(gobj, n);
        }
        set_timeout(priv->timer, WATCHDOG_MS);
    } else {
        test_fail(priv->half_closed < HALF_CLIENTS?
            "A half handshake held its connection" :
            "The session of the whole upgrade was not closed",
            priv->half_closed
        );
        set_yuno_must_die();
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A client connected: one byte, or the whole upgrade
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side) {
        hgobj channel = (hgobj)(uintptr_t)kw_get_int(
            gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED
        );
        int n = case_of_channel(channel);
        if(n < HALF_CLIENTS) {
            priv->not_before[n] = start_msectimer(CLOSE_NOT_BEFORE_MS);
            priv->before[n] = start_msectimer(CLOSE_BEFORE_MS);
            send_bytes(gobj, channel, "G", 1);
        } else {
            static const char upgrade[] =
                "GET / HTTP/1.1\r\n"
                "Host: 127.0.0.1:7884\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Key: " RFC_KEY "\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n";
            send_bytes(gobj, channel, upgrade, sizeof(upgrade) - 1);
        }
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The answer to the whole upgrade: a 101, and the two frames go.
 *  The other clients get the server's Close: nothing to check.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(src != priv->gobj_output_side) {
        test_fail("The server delivered a frame no client sent", -1);
        KW_DECREF(kw)
        return 0;
    }

    hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    int n = case_of_channel(channel);
    if(!gbuf || n != FULL_CLIENT) {
        KW_DECREF(kw)
        return 0;
    }

    gbuffer_append_gbuf(priv->answer, gbuf);
    char *p = gbuffer_cur_rd_pointer(priv->answer);
    size_t len = gbuffer_leftbytes(priv->answer);
    if(!priv->upgraded && memmem(p, len, "\r\n\r\n", 4)) {
        priv->upgraded = TRUE;
        if(!memmem(p, len, "HTTP/1.1 101 Switching Protocols\r\n", 34) ||
                !memmem(p, len, "Sec-WebSocket-Accept: " RFC_ACCEPT "\r\n", 22 + strlen(RFC_ACCEPT) + 2)) {
            test_fail("The whole upgrade was not answered a 101", n);
        }
        /*
         *  Two frames of a client (masked, with a key of zeros): a text
         *  frame not final, and another one not final
         */
        static const char frames[] = "\x01\x81" "\0\0\0\0" "a" "\x01\x81" "\0\0\0\0" "b";
        send_bytes(gobj, channel, frames, sizeof(frames) - 1);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The server closed a client of one byte: at timeout_handshake. When both
 *  are closed, the client of the whole upgrade connects. That one closed
 *  after its frames: the end.
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side) {
        hgobj channel = (hgobj)(uintptr_t)kw_get_int(
            gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED
        );
        int n = case_of_channel(channel);
        if(n < HALF_CLIENTS && !priv->closed[n]) {
            priv->closed[n] = TRUE;
            priv->half_closed++;
            if(!test_msectimer(priv->not_before[n])) {
                test_fail("A half handshake was closed before timeout_handshake", n);
            }
            if(test_msectimer(priv->before[n])) {
                test_fail("A half handshake was closed long after timeout_handshake", n);
            }
            if(priv->half_closed == HALF_CLIENTS && !priv->full_started) {
                priv->full_started = TRUE;
                start_client(gobj, FULL_CLIENT);
            }
        } else if(n == FULL_CLIENT && priv->upgraded) {
            /*
             *  Closed for the data frame inside a fragmented message: the end
             */
            clear_timeout(priv->timer);
            set_yuno_must_die();
        }
    }

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
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST6);

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
PUBLIC int register_c_test6(void)
{
    return create_gclass(C_TEST6);
}
