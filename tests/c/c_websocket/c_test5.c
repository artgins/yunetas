/***********************************************************************
 *          C_TEST5.C
 *
 *          A websocket client refuses a handshake that does not prove the
 *          server read its key.
 *
 *          RFC 6455 4.1: the client fails the connection when the 101 has
 *          no Sec-WebSocket-Accept, or one that is not the SHA-1 of the key
 *          it sent and the GUID, in base64; when it has no "Upgrade:
 *          websocket" or no "Connection: upgrade" (tokens, any case); and
 *          when it names a subprotocol or an extension the client did not
 *          offer (a C_WEBSOCKET client offers none). Up to 7.30.14 the
 *          client looked only at the 101, and opened the session with all
 *          of them.
 *
 *          The server here is raw (C_PROT_RAW) and answers each upgrade by
 *          hand, one case per connection; the client reconnects after each
 *          refusal. Every key it gets must be 16 random bytes in base64, and
 *          not the key of the connection before (up to 7.30.14 it was the
 *          clock, in hex):
 *          0. a 101 with no Sec-WebSocket-Accept: refused, ONE warning;
 *          1. a 101 with a wrong one: refused, ONE warning;
 *          2. the right accept and "Sec-WebSocket-Protocol: chat": refused,
 *             ONE warning;
 *          3. no Upgrade header: refused, ONE warning;
 *          4. "Upgrade: h2c": refused, ONE warning;
 *          5. no Connection header: refused, ONE warning;
 *          6. "Sec-WebSocket-Extensions: permessage-deflate": refused, ONE
 *             warning;
 *          7. "Upgrade: WebSocket", "Connection: keep-alive, Upgrade", the
 *             right accept, and a text frame "hello" in the same write: the
 *             session opens and the client delivers the frame (up to 7.30.14
 *             the bytes behind the 101 in the same read were dropped), and
 *             the test ends.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "ws_handshake.h"
#include "c_test5.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CASE_NO_ACCEPT      0
#define CASE_WRONG_ACCEPT   1
#define CASE_SUBPROTOCOL    2
#define CASE_NO_UPGRADE     3
#define CASE_BAD_UPGRADE    4
#define CASE_NO_CONNECTION  5
#define CASE_EXTENSION      6
#define CASE_RIGHT          7
#define WS_CASES            8
#define FIRST_FRAME         "hello"     // written with the 101 of the last case
#define WATCHDOG_MS         20000   // seven reconnections and the session

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
    gbuffer_t *request;         // the client's upgrade request, until complete
    int cases_answered;         // the upgrades answered, one case each
    BOOL opened;
    char last_key[64];          // the Sec-WebSocket-Key of the connection before
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
    priv->request = gbuffer_create(4096, 4096);

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

    set_timeout(priv->timer, 1000); // then the client connects

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

    GBUFFER_DECREF(priv->request)
}



                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The 101 of a case
 ***************************************************************************/
PRIVATE gbuffer_t *build_answer(int ws_case, const char *accept)
{
    gbuffer_t *answer = gbuffer_create(1024, 1024);
    gbuffer_printf(answer, "HTTP/1.1 101 Switching Protocols\r\n");
    if(ws_case == CASE_RIGHT) {
        gbuffer_printf(answer, "Upgrade: WebSocket\r\nConnection: keep-alive, Upgrade\r\n");
    } else {
        if(ws_case == CASE_BAD_UPGRADE) {
            gbuffer_printf(answer, "Upgrade: h2c\r\n");
        } else if(ws_case != CASE_NO_UPGRADE) {
            gbuffer_printf(answer, "Upgrade: websocket\r\n");
        }
        if(ws_case != CASE_NO_CONNECTION) {
            gbuffer_printf(answer, "Connection: Upgrade\r\n");
        }
    }
    if(ws_case == CASE_WRONG_ACCEPT) {
        gbuffer_printf(answer, "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n");
    } else if(ws_case != CASE_NO_ACCEPT) {
        gbuffer_printf(answer, "Sec-WebSocket-Accept: %s\r\n", accept);
    }
    if(ws_case == CASE_SUBPROTOCOL) {
        gbuffer_printf(answer, "Sec-WebSocket-Protocol: chat\r\n");
    }
    if(ws_case == CASE_EXTENSION) {
        gbuffer_printf(answer, "Sec-WebSocket-Extensions: permessage-deflate\r\n");
    }
    gbuffer_printf(answer, "\r\n");
    if(ws_case == CASE_RIGHT) {
        gbuffer_append_char(answer, (char)0x81);                // FIN, text
        gbuffer_append_char(answer, (char)strlen(FIRST_FRAME)); // unmasked: a server's frame
        gbuffer_append(answer, FIRST_FRAME, strlen(FIRST_FRAME));
    }
    return answer;
}

/***************************************************************************
 *  The Sec-WebSocket-Key of a request must be 16 random bytes in base64,
 *  and not the one of the connection before
 ***************************************************************************/
PRIVATE void check_key(hgobj gobj, const char *request, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    static const char field[] = "Sec-WebSocket-Key: ";
    const char *key = memmem(request, len, field, strlen(field));
    const char *end = key? memmem(key, len - (size_t)(key - request), "\r\n", 2) : NULL;
    BOOL right = FALSE;
    char text[64] = {0};
    if(end) {
        key += strlen(field);
        snprintf(text, sizeof(text), "%.*s", (int)(end - key), key);
        gbuffer_t *gbuf = gbuffer_base64_to_binary(text, strlen(text));
        if(gbuf && gbuffer_leftbytes(gbuf) == 16) {
            /*
             *  16 random bytes are all hex digits once in 2^64 times: a key
             *  that is only hex digits is the clock of 7.30.14
             */
            const char *bytes = gbuffer_cur_rd_pointer(gbuf);
            right = strspn(bytes, "0123456789ABCDEFabcdef") < 16 && strcmp(text, priv->last_key) != 0;
        }
        GBUFFER_DECREF(gbuf)
    }
    if(!right) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The Sec-WebSocket-Key is not 16 random bytes",
            "key",          "%s", text,
            NULL
        );
    }
    snprintf(priv->last_key, sizeof(priv->last_key), "%s", text);
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  First timeout: the server listens, the client connects.
 *  The second one: the watchdog, a case never ended.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_output_side) {
        priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
        gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
        gobj_start_tree(priv->gobj_output_side);
        set_timeout(priv->timer, WATCHDOG_MS);
    } else {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Not every case of the handshake was answered",
            "answered",     "%d", priv->cases_answered,
            NULL
        );
        set_yuno_must_die();
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The server gets the client's bytes: its upgrade request, answered by
 *  hand once complete, with the case that comes
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(src != priv->gobj_input_side) {
        /*
         *  The frame written with the last 101: the end
         */
        size_t ln = gbuf? gbuffer_leftbytes(gbuf) : 0;
        if(!priv->opened || ln != strlen(FIRST_FRAME) ||
                memcmp(gbuffer_cur_rd_pointer(gbuf), FIRST_FRAME, ln) != 0) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The client delivered a frame no server sent",
                NULL
            );
        }
        clear_timeout(priv->timer);
        set_yuno_must_die();
        KW_DECREF(kw)
        return 0;
    }
    if(!gbuf) {
        KW_DECREF(kw)
        return 0;
    }

    gbuffer_append_gbuf(priv->request, gbuf);
    char *p = gbuffer_cur_rd_pointer(priv->request);
    size_t len = gbuffer_leftbytes(priv->request);
    if(!memmem(p, len, "\r\n\r\n", 4)) {
        KW_DECREF(kw)
        return 0;   // the request is not complete yet
    }

    char accept[64];
    ws_accept_of_request(p, len, accept, sizeof(accept));
    check_key(gobj, p, len);
    gbuffer_reset_wr(priv->request);
    gbuffer_reset_rd(priv->request);

    if(priv->cases_answered >= WS_CASES) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "An upgrade after the last case",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    gobj_send_event(
        channel,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer",
            (json_int_t)(uintptr_t)build_answer(priv->cases_answered, accept)
        ),
        gobj
    );
    priv->cases_answered++;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client's session opens: only with the last case
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side) {
        if(priv->cases_answered != CASE_RIGHT + 1 || priv->opened) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The client opened a session with a handshake it must refuse",
                "case",         "%d", priv->cases_answered - 1,
                NULL
            );
        }
        priv->opened = TRUE;    // the test ends with the frame written with the 101
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
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
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST5);

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
PUBLIC int register_c_test5(void)
{
    return create_gclass(C_TEST5);
}
