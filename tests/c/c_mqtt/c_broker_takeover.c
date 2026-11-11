/****************************************************************************
 *          C_BROKER_TAKEOVER.C
 *
 *          Driver of the test of a take-over of the broker whose old
 *          connection closes AFTER the new one came and went. Two RAW MQTT 5
 *          clients (C_TCPs of this driver that write the packets by hand),
 *          one client id, both with clean start and no session expiry:
 *
 *              1. A connects.
 *              2. B connects with the same client id: the broker takes the
 *                 session over and closes A, which gets a DISCONNECT 0x8E
 *                 (Session taken over), though the session is not kept; A
 *                 does not close its side, so the broker waits timeout_close
 *                 (1 s here) for it.
 *              3. B disconnects at once (DISCONNECT, then it closes): its
 *                 session is not kept, the broker deletes it.
 *              4. A's wait ends: the broker drops it, with the WARNING
 *                 "Timeout waiting mqtt disconnected", and gets its
 *                 EV_ON_CLOSE with no session left for that client id.
 *
 *          The same happens in MQTT 3.1.1 while the old connection drains
 *          the answers it was sent. It is a normal sequence: up to 7.30.26
 *          the late EV_ON_CLOSE logged the ERROR "session NULL". And up to
 *          7.30.26 A got DISCONNECT 0x00 (Normal disconnection) whenever
 *          the session was not kept: MQTT 5 [MQTT-3.1.4-3] says 0x8E.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include "c_broker_takeover.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BROKER_URL          "tcp://127.0.0.1:18120"
#define CLIENT_ID           "tko_cl"

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
GOBJ_DECLARE_EVENT(EV_STOP_CLIENT);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-----------flag----default-----description---------*/
SDATA (DTP_POINTER,   "user_data",   0,      0,          "user data"),
SDATA (DTP_POINTER,   "subscriber",  0,      0,          "subscriber of output-events"),
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
    hgobj tcp_a;            // the old connection
    hgobj tcp_b;            // the new one, with the same client id
    gbuffer_t *rx_a;
    gbuffer_t *rx_b;
    json_t *got;            // what happened, in order
    BOOL started;
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
    priv->tcp_a = gobj_create(
        "client_a",
        C_TCP,
        json_pack("{s:s}", "url", BROKER_URL),
        gobj
    );
    priv->tcp_b = gobj_create(
        "client_b",
        C_TCP,
        json_pack("{s:s}", "url", BROKER_URL),
        gobj
    );
    priv->rx_a = gbuffer_create(1024, 1024);
    priv->rx_b = gbuffer_create(1024, 1024);
    priv->got = json_array();
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->rx_a)
    GBUFFER_DECREF(priv->rx_b)
    JSON_DECREF(priv->got)
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

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    if(gobj_is_running(priv->tcp_a)) {
        gobj_stop(priv->tcp_a);
    }
    if(gobj_is_running(priv->tcp_b)) {
        gobj_stop(priv->tcp_b);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 300);  // once the broker listens

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
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Write raw bytes to the broker
 ***************************************************************************/
PRIVATE void send_bytes(hgobj gobj, hgobj gobj_tcp, const uint8_t *bf, size_t len)
{
    gbuffer_t *gbuf = gbuffer_create(len, len);
    gbuffer_append(gbuf, (void *)bf, len);
    gobj_send_event(
        gobj_tcp,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
}

/***************************************************************************
 *  CONNECT, MQTT 5, clean start, no session expiry, no user
 ***************************************************************************/
PRIVATE void send_connect(hgobj gobj, hgobj gobj_tcp)
{
    static const uint8_t head5[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x05,   // protocol name, level 5
        0x02,                                   // flags: clean start
        0x00, 0x3C,                             // keep alive 60 s
        0x00                                    // no properties
    };
    uint8_t bf[64];
    size_t n = 0;

    bf[n++] = 0x10;
    bf[n++] = (uint8_t)(sizeof(head5) + 2 + strlen(CLIENT_ID));
    memcpy(bf + n, head5, sizeof(head5));
    n += sizeof(head5);
    bf[n++] = 0x00;
    bf[n++] = (uint8_t)strlen(CLIENT_ID);
    memcpy(bf + n, CLIENT_ID, strlen(CLIENT_ID));
    n += strlen(CLIENT_ID);
    send_bytes(gobj, gobj_tcp, bf, n);
}

/***************************************************************************
 *  What the broker sent to a client, as lines in `got`
 ***************************************************************************/
PRIVATE void parse_rx(hgobj gobj, hgobj gobj_tcp)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL is_a = (gobj_tcp == priv->tcp_a);
    gbuffer_t *rx = is_a? priv->rx_a : priv->rx_b;

    while(gbuffer_leftbytes(rx) >= 2) {
        const uint8_t *p = gbuffer_cur_rd_pointer(rx);
        size_t avail = gbuffer_leftbytes(rx);
        size_t remaining = p[1];    // < 128 here
        if(2 + remaining > avail) {
            return; // incomplete
        }
        uint8_t type = p[0] >> 4;
        const uint8_t *v = p + 2;
        char line[64];

        switch(type) {
            case 2:     // CONNACK
                snprintf(line, sizeof(line), "%s CONNACK %d", is_a? "A":"B", remaining >= 2? v[1] : -1);
                break;
            case 14:    // DISCONNECT, with its reason code
                snprintf(line, sizeof(line), "%s DISCONNECT %d", is_a? "A":"B", remaining >= 1? v[0] : 0);
                break;
            default:
                snprintf(line, sizeof(line), "%s TYPE %d", is_a? "A":"B", type);
                break;
        }
        json_array_append_new(priv->got, json_string(line));
        gbuffer_get(rx, 2 + remaining);

        if(type == 2) {
            if(is_a) {
                gobj_start(priv->tcp_b);    // B: CONNECT at its EV_CONNECTED
            } else {
                static const uint8_t disconnect[] = {0xE0, 0x00};   // normal disconnection
                send_bytes(gobj, priv->tcp_b, disconnect, sizeof(disconnect));
                gobj_post_event(gobj, EV_STOP_CLIENT, json_object(), gobj);  // and B closes
            }
        }
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The start, the end, and the guard
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->started) {
        priv->started = TRUE;
        gobj_start(priv->tcp_a);        // A: CONNECT at its EV_CONNECTED
        set_timeout(priv->timer, 5000); // the guard
        KW_DECREF(kw)
        return 0;
    }

    /*
     *  A's DISCONNECT and B's CONNACK come on two connections: their
     *  order is not checked. A closes last.
     */
    json_t *expected = json_pack("[s,s,s,s,s]",
        "A CONNACK 0",
        "A DISCONNECT 142", // the take-over: 0x8E Session taken over
        "B CONNACK 0",
        "B CLOSED",
        "A CLOSED"          // timeout_close
    );
    BOOL ok = (json_array_size(priv->got) == json_array_size(expected));
    int idx; json_t *jn_line;
    json_array_foreach(expected, idx, jn_line) {
        BOOL found = FALSE;
        int i; json_t *jn_got;
        json_array_foreach(priv->got, i, jn_got) {
            if(json_equal(jn_got, jn_line)) {
                found = TRUE;
            }
        }
        if(!found) {
            ok = FALSE;
        }
    }
    const char *last = json_string_value(json_array_get(priv->got, json_array_size(priv->got) - 1));
    if(!last || strcmp(last, "A CLOSED") != 0) {
        ok = FALSE;
    }
    if(!ok) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: not what was expected",
            "got",          "%j", priv->got,
            "expected",     "%j", expected,
            NULL
        );
    } else {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "TEST: broker take-over done",
            NULL
        );
    }
    JSON_DECREF(expected)
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A client is connected: CONNECT
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    send_connect(gobj, src);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the broker sent
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gbuf) {
        gbuffer_append_gbuf((src == priv->tcp_a)? priv->rx_a : priv->rx_b, gbuf);
    }
    parse_rx(gobj, src);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A connection closed. When it is A's, the broker has its EV_ON_CLOSE
 *  now: the end is checked a moment later.
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->tcp_a) {
        json_array_append_new(priv->got, json_string("A CLOSED"));
        gobj_post_event(gobj, EV_STOP_CLIENT, json_pack("{s:b}", "a", 1), gobj);
    } else {
        json_array_append_new(priv->got, json_string("B CLOSED"));
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Stop a client, out of its own publication
 ***************************************************************************/
PRIVATE int ac_stop_client(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(kw_get_bool(gobj, kw, "a", 0, 0)) {
        if(gobj_is_running(priv->tcp_a)) {
            gobj_stop(priv->tcp_a);
        }
        set_timeout(priv->timer, 300);
    } else if(gobj_is_running(priv->tcp_b)) {
        gobj_stop(priv->tcp_b);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The transports' other events: nothing to do
 ***************************************************************************/
PRIVATE int ac_transport_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}




                    /***************************
                     *      FSM
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_BROKER_TAKEOVER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_STOP_CLIENT);

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
        {EV_CONNECTED,          ac_connected,       0},
        {EV_RX_DATA,            ac_rx_data,         0},
        {EV_DISCONNECTED,       ac_disconnected,    0},
        {EV_STOP_CLIENT,        ac_stop_client,     0},
        {EV_TX_READY,           ac_transport_event, 0},
        {EV_STOPPED,            ac_transport_event, 0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_CONNECTED,          0},
        {EV_RX_DATA,            0},
        {EV_DISCONNECTED,       0},
        {EV_STOP_CLIENT,        0},
        {EV_TX_READY,           0},
        {EV_STOPPED,            0},
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
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // authz_table
        0,              // command_table
        s_user_trace_level,
        0               // gcflag_t
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
PUBLIC int register_c_broker_takeover(void)
{
    return create_gclass(C_BROKER_TAKEOVER);
}
