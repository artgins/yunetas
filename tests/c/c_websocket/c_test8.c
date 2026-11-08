/***********************************************************************
 *          C_TEST8.C
 *
 *          A C_WEBSOCKET server does not carry a message from one
 *          connection to the next one of the same channel.
 *
 *          The server has ONE channel. A raw client (C_PROT_RAW) upgrades,
 *          sends a text frame that is not final, "A-secret", and a ping;
 *          when the pong comes (the server read the frame) it leaves. Then
 *          another raw client upgrades, on the same channel, and sends a
 *          final continuation, "b": a continuation with no message begun,
 *          closed with ONE warning. The test ends when it is closed.
 *
 *          Up to 7.30.15 the fragment begun stayed in the gobj after its
 *          connection: the second client's continuation completed the
 *          first one's message, and the server delivered "A-secretb" to
 *          the session of the second peer (red: "The second connection got
 *          a message with the bytes of the first one").
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <limits.h>
#include <string.h>

#include "c_test8.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CLIENT_A        0
#define CLIENT_B        1
#define WATCHDOG_MS     10000
#define RFC_KEY         "dGhlIHNhbXBsZSBub25jZQ=="          // RFC 6455 1.3

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
    BOOL upgraded[2];       // the client got its 101
    BOOL left;              // client A dropped its connection
    BOOL a_closed;          // the close of A, seen by the client
    BOOL a_server_closed;   // and by the server
    BOOL b_started;
    gbuffer_t *answer[2];   // what each client got, until the 101 ends
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
    priv->answer[CLIENT_A] = gbuffer_create(4096, 4096);
    priv->answer[CLIENT_B] = gbuffer_create(4096, 4096);

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

    set_timeout(priv->timer, 1000); // then the first client connects

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

    GBUFFER_DECREF(priv->answer[CLIENT_A])
    GBUFFER_DECREF(priv->answer[CLIENT_B])
}



                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The client of a channel is the number of its channel, "case-<n>"
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
 *  First timeout: the server listens, client A connects.
 *  The second one: the watchdog.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_output_side) {
        priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
        gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
        gobj_start_tree(priv->gobj_output_side);
        start_client(gobj, CLIENT_A);
        set_timeout(priv->timer, WATCHDOG_MS);
    } else {
        test_fail("The second connection was not closed", priv->b_started? CLIENT_B : CLIENT_A);
        set_yuno_must_die();
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A client connected: its upgrade
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side) {
        hgobj channel = (hgobj)(uintptr_t)kw_get_int(
            gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED
        );
        static const char upgrade[] =
            "GET / HTTP/1.1\r\n"
            "Host: 127.0.0.1:7886\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: " RFC_KEY "\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n";
        send_bytes(gobj, channel, upgrade, sizeof(upgrade) - 1);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A client gets its 101, then sends its frames (masked, a key of zeros):
 *  A a text frame not final and a ping, B a final continuation. A leaves
 *  at the pong: the server read its fragment.
 *  The server delivers no message: if it does, it is B's with A's bytes.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(src != priv->gobj_output_side) {
        test_fail("The second connection got a message with the bytes of the first one", CLIENT_B);
        KW_DECREF(kw)
        return 0;
    }

    hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    int n = case_of_channel(channel);
    if(!gbuf || n > CLIENT_B) {
        KW_DECREF(kw)
        return 0;
    }

    if(priv->upgraded[n]) {
        if(n == CLIENT_A && !priv->left &&
                memchr(gbuffer_cur_rd_pointer(gbuf), 0x8A, gbuffer_leftbytes(gbuf))) {
            priv->left = TRUE;
            gobj_send_event(channel, EV_DROP, 0, gobj);
        }
        KW_DECREF(kw)
        return 0;
    }

    gbuffer_append_gbuf(priv->answer[n], gbuf);
    char *p = gbuffer_cur_rd_pointer(priv->answer[n]);
    size_t len = gbuffer_leftbytes(priv->answer[n]);
    if(memmem(p, len, "\r\n\r\n", 4)) {
        priv->upgraded[n] = TRUE;
        if(!memmem(p, len, "HTTP/1.1 101 Switching Protocols\r\n", 34)) {
            test_fail("The upgrade was not answered a 101", n);
        }
        if(n == CLIENT_A) {
            static const char frames[] =
                "\x01\x88" "\0\0\0\0" "A-secret"     // text, not final
                "\x89\x80" "\0\0\0\0";               // ping
            send_bytes(gobj, channel, frames, sizeof(frames) - 1);
        } else {
            static const char frames[] =
                "\x80\x81" "\0\0\0\0" "b";           // continuation, final
            send_bytes(gobj, channel, frames, sizeof(frames) - 1);
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A left, seen by the client and by the server: B connects, on the same
 *  channel of the server. B closed: the end.
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side) {
        hgobj channel = (hgobj)(uintptr_t)kw_get_int(
            gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED
        );
        int n = case_of_channel(channel);
        if(n == CLIENT_A) {
            priv->a_closed = TRUE;
        } else if(n == CLIENT_B && priv->upgraded[CLIENT_B]) {
            clear_timeout(priv->timer);
            set_yuno_must_die();
        }
    } else if(src == priv->gobj_input_side && priv->left) {
        priv->a_server_closed = TRUE;
    }

    if(priv->a_closed && priv->a_server_closed && !priv->b_started) {
        priv->b_started = TRUE;
        start_client(gobj, CLIENT_B);
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
PUBLIC int register_c_test8(void)
{
    return create_gclass(C_TEST8);
}
