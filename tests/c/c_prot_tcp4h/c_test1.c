/****************************************************************************
 *          C_TEST1.C
 *
 *          A C_PROT_TCP4H client and the size of a frame.
 *
 *          The length of a frame is the PEER's word, in a 4-byte header
 *          before any payload. Up to 7.25.21 C_PROT_TCP4H reserved that
 *          length at once (C_WEBSOCKET did too, up to the same release):
 *          a few headers claiming the max block reserved it each, with no
 *          payload sent. Now the buffer starts small and grows with what
 *          arrives.
 *
 *          The server is raw (C_PROT_RAW) and writes tcp4h by hand:
 *          - a frame of BIG_LEN bytes (past the first 4 KB of the buffer),
 *            sent in three parts: it must arrive whole;
 *          - then the header of a frame of HUGE_LEN bytes, with no
 *            payload: the memory of the yuno must not grow by its length
 *            (MEM_CHECK_DELAY_MS later), and the client drops at its
 *            timeout_payload.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <arpa/inet.h>
#include "c_test1.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BIG_LEN         60000   // a frame past the first 4 KB of the buffer
#define PART_LEN        20000   // sent in three parts
#define HUGE_LEN        (10*1024*1024)  // only its header is sent
#define HEADER_DELAY_MS 300     // the frame, after the client connects
#define PART_DELAY_MS   100     // between the parts
#define MEM_CHECK_DELAY_MS 300  // the memory, after the huge header
#define MEM_GROWTH_MAX  (1024*1024)     // what the huge header may take, at most

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
    hgobj server_channel;       // the channel of the server, where the client connected
    BOOL connected;
    int parts_sent;             // of the big frame
    BOOL huge_sent;
    size_t mem_before_huge;     // the yuno's memory before the huge header
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
 *  Then: the big frame, in three parts; and the check of the memory after
 *  the huge header.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_output_side) {
        priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
        gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
        gobj_start_tree(priv->gobj_output_side);

    } else if(priv->huge_sent) {
        size_t now = get_cur_system_memory();
        size_t growth = now > priv->mem_before_huge? now - priv->mem_before_huge : 0;
        if(growth > MEM_GROWTH_MAX) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The header of a huge frame reserved its length",
                "growth",       "%lu", (unsigned long)growth,
                "frame_length", "%lu", (unsigned long)HUGE_LEN,
                NULL
            );
        }

    } else if(priv->connected && priv->parts_sent < BIG_LEN/PART_LEN) {
        gbuffer_t *gbuf = gbuffer_create(PART_LEN + 4, PART_LEN + 4);
        if(priv->parts_sent == 0) {
            uint32_t len = htonl(BIG_LEN + 4);                 // the header counts itself
            gbuffer_append(gbuf, &len, sizeof(len));
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
 *  The client got the big frame: it must be whole. Then the header of a
 *  huge frame, with no payload.
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
    priv->mem_before_huge = get_cur_system_memory();
    gbuffer_t *huge = gbuffer_create(4, 4);
    uint32_t huge_len = htonl(HUGE_LEN + 4);
    gbuffer_append(huge, &huge_len, sizeof(huge_len));
    server_send(gobj, huge);
    set_timeout(priv->timer, MEM_CHECK_DELAY_MS);
}

/***************************************************************************
 *  A message: at the server, the client's bytes; at the client, a frame
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src != priv->gobj_input_side) {
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side && !priv->connected) {
        priv->connected = TRUE;
        priv->server_channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
        set_timeout(priv->timer, HEADER_DELAY_MS);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client closed (the huge header, then timeout_payload): the end
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_side) {
        if(!priv->huge_sent) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The client closed before the huge header was sent: nothing tested",
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
