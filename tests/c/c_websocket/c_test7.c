/***********************************************************************
 *          C_TEST7.C
 *
 *          What a websocket client closes for, and what it takes.
 *
 *          The server here is raw (C_PROT_RAW) and answers each upgrade by
 *          hand, one case per connection; the client (timeout_handshake
 *          1 s, timeout_close 0.5 s) reconnects after each close:
 *          0. half a 101, and then nothing: the client closes at
 *             timeout_handshake, ONE warning. Up to 7.30.15 every read
 *             stopped that timer and nothing armed it again: the client
 *             waited for ever (red: "Not every case was answered");
 *          1. a right 101 and a text frame with an RSV bit (no extension
 *             was agreed): closed, ONE warning;
 *          2. a right 101 and a final continuation frame, with no message
 *             begun: closed, ONE warning;
 *          3. the same, the continuation not final: closed, ONE warning;
 *          4. a 101 with OWS around its values ("Upgrade: websocket ", a
 *             tab and a space around "Upgrade", a space after the accept),
 *             and a text frame "hello" in the same write: the session opens
 *             and the client delivers the frame; the server then drops the
 *             connection. Up to 7.30.15 the parser kept the trailing OWS,
 *             and the client refused this legal 101 (red);
 *          5. a right 101, a text frame not final and a text frame final:
 *             data inside a fragmented message, closed, ONE warning; the
 *             test ends when that connection is closed.
 *          Up to 7.30.15 the closes of 1, 2, 3 and 5 left no log (red). A
 *          data frame inside a fragmented message that is not final is
 *          test6 (the server side): a fragment begun stays in the gobj
 *          after its connection, so this client cannot take a second one.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "ws_handshake.h"
#include "c_test7.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CASE_HALF_101       0
#define CASE_RSV            1
#define CASE_CONT_FINAL     2
#define CASE_CONT           3
#define CASE_OWS            4
#define CASE_DATA_IN_FRAG   5
#define WS_CASES            6
#define FIRST_FRAME         "hello"     // written with the 101 of CASE_OWS
#define WATCHDOG_MS         20000

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
    BOOL answered;              // the upgrade of this connection is answered
    hgobj server_channel;       // the channel of the server that answered it
    int cases_answered;         // the upgrades answered, one case each
    int opens;                  // sessions the client opened
    BOOL delivered;             // the frame of CASE_OWS
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
 *  A frame of a server: not masked, a payload of less than 126 bytes
 ***************************************************************************/
PRIVATE void append_frame(gbuffer_t *gbuf, uint8_t byte0, const char *payload)
{
    gbuffer_append_char(gbuf, (char)byte0);
    gbuffer_append_char(gbuf, (char)strlen(payload));
    gbuffer_append(gbuf, (void *)payload, strlen(payload));
}

/***************************************************************************
 *  The answer of a case: its 101, and the frames behind it
 ***************************************************************************/
PRIVATE gbuffer_t *build_answer(int ws_case, const char *accept)
{
    gbuffer_t *answer = gbuffer_create(1024, 1024);
    gbuffer_printf(answer, "HTTP/1.1 101 Switching Protocols\r\n");
    if(ws_case == CASE_HALF_101) {
        gbuffer_printf(answer, "Upgrade: websocket\r\n");   // and no more
        return answer;
    }
    if(ws_case == CASE_OWS) {
        gbuffer_printf(answer,
            "Upgrade: websocket \r\n"
            "Connection:\t Upgrade \t\r\n"
            "Sec-WebSocket-Accept: %s \r\n"
            "\r\n",
            accept
        );
    } else {
        gbuffer_printf(answer,
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n"
            "\r\n",
            accept
        );
    }

    switch(ws_case) {
        case CASE_RSV:
            append_frame(answer, 0x80|0x40|0x01, "hi");     // FIN, RSV1, text
            break;
        case CASE_CONT_FINAL:
            append_frame(answer, 0x80|0x00, "hi");          // FIN, continuation
            break;
        case CASE_CONT:
            append_frame(answer, 0x00, "hi");               // continuation, not final
            break;
        case CASE_OWS:
            append_frame(answer, 0x80|0x01, FIRST_FRAME);   // FIN, text
            break;
        case CASE_DATA_IN_FRAG:
            append_frame(answer, 0x01, "a");                // text, not final
            append_frame(answer, 0x80|0x01, "b");           // FIN, text: not a continuation
            break;
        default:
            break;
    }
    return answer;
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
        test_fail("Not every case was answered", priv->cases_answered);
        set_yuno_must_die();
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The server gets the client's bytes: its upgrade request, answered by
 *  hand once complete, with the case that comes. What the client sends
 *  after it (its Close) is not looked at.
 *  The client gets the frame of CASE_OWS: the server drops that connection.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(src != priv->gobj_input_side) {
        size_t ln = gbuf? gbuffer_leftbytes(gbuf) : 0;
        if(priv->cases_answered != CASE_OWS + 1 || ln != strlen(FIRST_FRAME) ||
                memcmp(gbuffer_cur_rd_pointer(gbuf), FIRST_FRAME, ln) != 0) {
            test_fail("The client delivered a frame it must not", priv->cases_answered - 1);
        }
        priv->delivered = TRUE;
        gobj_send_event(priv->server_channel, EV_DROP, 0, gobj);  // the next case
        KW_DECREF(kw)
        return 0;
    }
    if(!gbuf || priv->answered) {
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
    gbuffer_reset_wr(priv->request);
    gbuffer_reset_rd(priv->request);
    priv->answered = TRUE;

    if(priv->cases_answered >= WS_CASES) {
        test_fail("An upgrade after the last case", priv->cases_answered);
        KW_DECREF(kw)
        return 0;
    }

    hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    priv->server_channel = channel;
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
 *  A connection opens on the server: its upgrade is to be answered.
 *  The client's session opens: not with half a 101.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->answered = FALSE;
        gbuffer_reset_wr(priv->request);
        gbuffer_reset_rd(priv->request);
    } else if(src == priv->gobj_output_side) {
        priv->opens++;
        if(priv->cases_answered - 1 == CASE_HALF_101) {
            test_fail("The client opened a session with half a 101", CASE_HALF_101);
        }
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The frame of CASE_OWS delivered, the server drops its connection; the
 *  client closed after the last case: the end
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side && priv->cases_answered == WS_CASES) {
        if(!priv->delivered) {
            test_fail("The frame of the 101 with OWS was not delivered", CASE_OWS);
        }
        if(priv->opens != WS_CASES - 1) {
            test_fail("The client did not open a session with each right 101", priv->opens);
        }
        clear_timeout(priv->timer);
        set_yuno_must_die();
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
GOBJ_DEFINE_GCLASS(C_TEST7);

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
PUBLIC int register_c_test7(void)
{
    return create_gclass(C_TEST7);
}
