/***********************************************************************
 *          C_TEST2.C
 *
 *          A websocket client and the size of a frame.
 *
 *          The length of a frame is the PEER's word, written in its header
 *          before any of the payload comes. Up to 7.25.21 C_WEBSOCKET
 *          reserved that length at once: a few connections that each
 *          claimed the max block of the yuno reserved it each, with a few
 *          bytes sent. Now the buffer starts small and grows with what
 *          arrives, up to `max_payload_size` (0: the max block), and a
 *          frame announced bigger than that closes the connection (1009)
 *          with a warning.
 *
 *          The server is raw (C_PROT_RAW) and plays websocket by hand, as
 *          test1. The client has a max_payload_size of 64 KB:
 *          - a frame of BIG_LEN bytes (past the first 4 KB of the buffer),
 *            sent in three parts: it must arrive whole;
 *          - then the header of a frame of HUGE_LEN bytes, past the max,
 *            with no payload: a warning, and the connection closed.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test2.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BIG_LEN         60000   // a frame within the client's max_payload_size (65536)
#define PART_LEN        20000   // sent in three parts
#define HUGE_LEN        1000000 // a frame past it: only its header is sent
#define HEADER_DELAY_MS 300     // the frame, after the 101
#define PART_DELAY_MS   100     // between the parts

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
    int parts_sent;             // of the big frame
    BOOL huge_sent;
    int client_messages;        // messages the client delivered: must be 1, the big frame
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
 *  A part of a frame from the server
 ***************************************************************************/
PRIVATE void server_send(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_send_event(
        priv->server_channel,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *  First timeout: the server listens, the client connects.
 *  Then: the big frame, in three parts.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_output_side) {
        priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
        gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
        gobj_start_tree(priv->gobj_output_side);

    } else if(priv->answered && priv->parts_sent < BIG_LEN/PART_LEN) {
        gbuffer_t *gbuf = gbuffer_create(PART_LEN + 4, PART_LEN + 4);
        if(priv->parts_sent == 0) {
            gbuffer_append_char(gbuf, (char)0x82);              // FIN, binary
            gbuffer_append_char(gbuf, 126);                     // 16-bit length follows
            gbuffer_append_char(gbuf, (char)(BIG_LEN >> 8));
            gbuffer_append_char(gbuf, (char)(BIG_LEN & 0xFF));
        }
        for(int i = 0; i < PART_LEN; i++) {
            gbuffer_append_char(gbuf, 'y');
        }
        server_send(gobj, gbuf);
        priv->parts_sent++;
        if(priv->parts_sent < BIG_LEN/PART_LEN) {
            set_timeout(priv->timer, PART_DELAY_MS);
        }
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
 *  The client got the big frame: it must be whole. Then the header of a
 *  frame past its max_payload_size.
 ***************************************************************************/
PRIVATE void client_message(hgobj gobj, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->client_messages++;
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    size_t len = gbuf? gbuffer_leftbytes(gbuf) : 0;
    char *p = gbuf? gbuffer_cur_rd_pointer(gbuf) : NULL;
    BOOL whole = (len == BIG_LEN && p && p[0] == 'y' && p[len-1] == 'y')? TRUE : FALSE;
    if(!whole || priv->client_messages != 1) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The client did not deliver the big frame whole, once",
            "len",          "%lu", (unsigned long)len,
            "messages",     "%d", priv->client_messages,
            NULL
        );
        return;
    }

    priv->huge_sent = TRUE;
    gbuffer_t *huge = gbuffer_create(10, 10);
    gbuffer_append_char(huge, (char)0x82);                  // FIN, binary
    gbuffer_append_char(huge, 127);                         // 64-bit length follows
    for(int i = 7; i >= 0; i--) {
        gbuffer_append_char(huge, (char)(((uint64_t)HUGE_LEN >> (i*8)) & 0xFF));
    }
    server_send(gobj, huge);
}

/***************************************************************************
 *  A message: at the server, the client's bytes; at the client, a frame
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        ac_server_message(gobj, kw);
    } else {
        client_message(gobj, kw);
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
 *  The client closed (the huge frame, then timeout_close): the end
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side) {
        if(!priv->huge_sent) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The client closed before the huge frame was sent: nothing tested",
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
GOBJ_DEFINE_GCLASS(C_TEST2);

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
PUBLIC int register_c_test2(void)
{
    return create_gclass(C_TEST2);
}
