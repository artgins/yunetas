/***********************************************************************
 *          C_TEST4.C
 *
 *          A C_WEBSOCKET server answers one subprotocol, or none.
 *
 *          RFC 6455 4.2.2: the server selects one of the values the client
 *          offered, or none. Up to 7.29.3 it echoed the whole header, a list
 *          included, and cut it at 512 bytes with no word; a browser fails
 *          the connection when the answer is no value it offered.
 *
 *          Each raw client (C_PROT_RAW) sends one upgrade request, by hand,
 *          with the case of its channel, and reads the 101. All the answers
 *          read, the yuno ends.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <limits.h>
#include <string.h>

#include "c_test4.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define LONG_TOKEN_SIZE 600

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char *offered;    // NULL: no header; "": a token too long
    const char *answered;   // NULL: no header in the 101
} ws_case_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE const ws_case_t ws_cases[] = {
    {NULL,              NULL},
    {"chat, superchat", "chat"},
    {" , mqtt ,x",      "mqtt"},
    {"bad value, chat", NULL},
    {"",                NULL},
};
#define WS_CASES (sizeof(ws_cases)/sizeof(ws_cases[0]))

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
    gbuffer_t *answer[WS_CASES];
    BOOL case_done[WS_CASES];
    size_t cases_done;
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

    for(size_t i = 0; i < WS_CASES; i++) {
        GBUFFER_DECREF(priv->answer[i])
    }
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
PRIVATE size_t case_of_channel(hgobj channel)
{
    return (size_t)atoi(gobj_name(channel) + strlen("case-"));
}

/***************************************************************************
 *  The upgrade request of a case, with the subprotocol it offers
 ***************************************************************************/
PRIVATE gbuffer_t *build_upgrade_request(const ws_case_t *ws_case)
{
    gbuffer_t *gbuf = gbuffer_create(2048, 2048);
    gbuffer_printf(gbuf,
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:7750\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
    );
    if(ws_case->offered) {
        gbuffer_printf(gbuf, "Sec-WebSocket-Protocol: ");
        if(*ws_case->offered) {
            gbuffer_printf(gbuf, "%s", ws_case->offered);
        } else {
            for(int i = 0; i < LONG_TOKEN_SIZE; i++) {
                gbuffer_append_char(gbuf, 'a');
            }
        }
        gbuffer_printf(gbuf, "\r\n");
    }
    gbuffer_printf(gbuf, "\r\n");
    return gbuf;
}

/***************************************************************************
 *  The 101 of a case must say the expected subprotocol, and only it
 ***************************************************************************/
PRIVATE void check_answer(hgobj gobj, size_t n, const char *answer, size_t len)
{
    static const char header[] = "Sec-Websocket-Protocol: ";
    char expected[256] = {0};
    if(ws_cases[n].answered) {
        snprintf(expected, sizeof(expected), "%s%s\r\n", header, ws_cases[n].answered);
    }

    BOOL right;
    if(!memmem(answer, len, "101 Switching Protocols", 23)) {
        right = FALSE;
    } else if(ws_cases[n].answered) {
        right = memmem(answer, len, expected, strlen(expected)) != NULL;
    } else {
        right = memmem(answer, len, header, strlen(header)) == NULL;
    }

    if(!right) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The 101 does not say the subprotocol due",
            "case",         "%d", (int)n,
            "expected",     "%s", expected,
            "answer",       "%.*s", (int)(len > 256 ? 256 : len), answer,
            NULL
        );
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The server listens: the clients connect.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_output_side) {
        priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
        gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
        gobj_start_tree(priv->gobj_output_side);
        for(size_t i = 0; i < WS_CASES; i++) {
            char name[NAME_MAX];
            snprintf(name, sizeof(name), "case-%d", (int)i);
            hgobj channel = gobj_child_by_name(priv->gobj_output_side, name);
            hgobj prot = gobj_child_by_name(channel, name);
            gobj_start(gobj_child_by_name(prot, name)); // C_PROT_RAW leaves a client's C_TCP to its host
        }
        set_timeout(priv->timer, 5000);
    } else {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Not every upgrade was answered",
            "answered",     "%d", (int)priv->cases_done,
            NULL
        );
        set_yuno_must_die();
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A client connected: it sends the upgrade request of its case
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side) {
        hgobj channel = (hgobj)(uintptr_t)kw_get_int(
            gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED
        );
        gobj_send_event(
            channel,
            EV_SEND_MESSAGE,
            json_pack("{s:I}", "gbuffer",
                (json_int_t)(uintptr_t)build_upgrade_request(&ws_cases[case_of_channel(channel)])
            ),
            gobj
        );
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A client gets its answer, whole at the end of the head.
 *  The server side has nothing to say: no client sends a frame.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(src != priv->gobj_output_side) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The server delivered a frame no client sent",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    size_t n = case_of_channel(channel);
    if(!gbuf || priv->case_done[n]) {
        KW_DECREF(kw)
        return 0;
    }

    if(!priv->answer[n]) {
        priv->answer[n] = gbuffer_create(4096, 4096);
    }
    gbuffer_append_gbuf(priv->answer[n], gbuf);
    char *p = gbuffer_cur_rd_pointer(priv->answer[n]);
    size_t len = gbuffer_leftbytes(priv->answer[n]);
    if(memmem(p, len, "\r\n\r\n", 4)) {
        check_answer(gobj, n, p, len);
        priv->case_done[n] = TRUE;
        priv->cases_done++;
        if(priv->cases_done == WS_CASES) {
            clear_timeout(priv->timer);
            set_yuno_must_die();
        }
    }

    KW_DECREF(kw)
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
GOBJ_DEFINE_GCLASS(C_TEST4);

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
PUBLIC int register_c_test4(void)
{
    return create_gclass(C_TEST4);
}
