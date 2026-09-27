/***********************************************************************
 *          C_TEST1.C
 *
 *          A websocket client that is closing still receives.
 *
 *          When a websocket CLIENT closes (a timeout, a protocol error),
 *          ws_close() moves it to ST_DISCONNECTED and sends its Close frame,
 *          and it keeps the TCP connection until the server drops it (or
 *          timeout_close runs out): RFC 6455 lets an endpoint that sent a
 *          Close go on receiving. What arrives meanwhile -- the rest of the
 *          frame it gave up on, the server's own Close -- came to
 *          ST_DISCONNECTED as EV_RX_DATA, which the state did not declare:
 *          "Event NOT DEFINED in state", an ERROR in every such close. Seen
 *          on a sim of controllers too busy to read a frame in time.
 *
 *          The server here is raw (C_PROT_RAW) and plays websocket by hand:
 *          it answers the upgrade (the client checks only the 101 and drops
 *          whatever comes behind it in the same read, so the frame goes
 *          HEADER_DELAY_MS later), sends the header of a frame of FRAME_LEN
 *          bytes with the first FIRST_PART of them, and the rest
 *          REST_DELAY_MS later -- after the client's timeout_payload (1 s)
 *          closed it. The client must take
 *          the late bytes as nothing: no error, no message; it drops the
 *          connection at timeout_close, and the test ends.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test1.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define FRAME_LEN       1000    // payload the frame header announces
#define FIRST_PART      10      // of it, sent with the header
#define HEADER_DELAY_MS 300     // the frame, after the 101
#define REST_DELAY_MS   2500    // the rest, after the client's timeout_payload (1000)

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
    hgobj server_channel;       // the channel of the server that answers the client
    gbuffer_t *request;         // the client's upgrade request, until complete
    BOOL answered;
    BOOL header_sent;
    BOOL rest_sent;
    int client_messages;        // messages the client delivered: must be 0
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




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  First timeout: the server listens, the client connects.
 *  Second one: the header of the frame, with its first part.
 *  Third one: the rest of the frame, after the client gave up on it.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_output_side) {
        priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
        gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
        gobj_start_tree(priv->gobj_output_side);

    } else if(priv->answered && !priv->header_sent) {
        priv->header_sent = TRUE;
        gbuffer_t *gbuf = gbuffer_create(FIRST_PART + 4, FIRST_PART + 4);
        gbuffer_append_char(gbuf, (char)0x82);              // FIN, binary
        gbuffer_append_char(gbuf, 126);                     // 16-bit length follows
        gbuffer_append_char(gbuf, (char)(FRAME_LEN >> 8));
        gbuffer_append_char(gbuf, (char)(FRAME_LEN & 0xFF));
        for(int i = 0; i < FIRST_PART; i++) {
            gbuffer_append_char(gbuf, 'x');
        }
        gobj_send_event(
            priv->server_channel,
            EV_SEND_MESSAGE,
            json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
            gobj
        );
        set_timeout(priv->timer, REST_DELAY_MS);

    } else if(priv->header_sent && !priv->rest_sent) {
        priv->rest_sent = TRUE;
        gbuffer_t *gbuf = gbuffer_create(FRAME_LEN, FRAME_LEN);
        for(int i = FIRST_PART; i < FRAME_LEN; i++) {
            gbuffer_append_char(gbuf, 'x');
        }
        gobj_send_event(
            priv->server_channel,
            EV_SEND_MESSAGE,
            json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
            gobj
        );
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The server gets the client's bytes: its upgrade request, answered by
 *  hand once complete (the client checks only the 101)
 ***************************************************************************/
PRIVATE int ac_server_message(hgobj gobj, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(priv->answered || !gbuf) {
        return 0;   // the client's Close frame, after its timeout: not for us
    }
    if(!priv->request) {
        priv->request = gbuffer_create(4096, 4096);
    }
    gbuffer_append_gbuf(priv->request, gbuf);
    char *p = gbuffer_cur_rd_pointer(priv->request);
    size_t len = gbuffer_leftbytes(priv->request);
    if(!memmem(p, len, "\r\n\r\n", 4)) {
        return 0;   // the request is not complete yet
    }

    priv->answered = TRUE;
    priv->server_channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);

    gbuffer_t *answer = gbuffer_create(1024, 1024);
    gbuffer_printf(answer,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: whatever\r\n"
        "\r\n"
    );
    gobj_send_event(
        priv->server_channel,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)answer),
        gobj
    );

    set_timeout(priv->timer, HEADER_DELAY_MS);
    return 0;
}

/***************************************************************************
 *  A message: at the server, the client's bytes; at the client, a frame --
 *  which must never come, the only frame is given up on
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        ac_server_message(gobj, kw);
    } else {
        priv->client_messages++;
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The client delivered a frame it had given up on",
            NULL
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client closed (timeout_close ran out, it dropped the TCP): the end
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side) {
        if(!priv->rest_sent) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The client closed before the late bytes were sent: nothing tested",
                NULL
            );
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
GOBJ_DEFINE_GCLASS(C_TEST1);

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
PUBLIC int register_c_test1(void)
{
    return create_gclass(C_TEST1);
}
