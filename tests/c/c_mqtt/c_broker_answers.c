/****************************************************************************
 *          C_BROKER_ANSWERS.C
 *
 *          Driver of the test of the bound on the answers (max_unwritten_
 *          bytes of C_PROT_MQTT2) against clients of the broker that READ:
 *          none of them is ever dropped. Two RAW MQTT 3.1.1 clients (C_TCPs
 *          of this driver that write the packets by hand):
 *
 *              1. S subscribes to fc/# and publishes 30000 QoS 0 messages
 *                 to fc/x in ONE write, reading everything: it gets the
 *                 30000 back from the broker, and is not dropped. The
 *                 messages the broker forwards are not answers: they fill
 *                 the send queue faster than the C_TCP writes it (one
 *                 packet per turn of the loop), and do not count.
 *              2. L behaves as a client of a library: it subscribes to
 *                 lib/#, publishes 2000 QoS 1 messages with an in-flight
 *                 window of 20 (a new one at each PUBACK), then 2000 QoS 2
 *                 with a window of 20 (PUBREC, its PUBREL, PUBCOMP). Its
 *                 answers wait behind the messages forwarded to it: it gets
 *                 every PUBACK and PUBCOMP and the 4000 messages back, and
 *                 is not dropped.
 *              3. P publishes 3000 QoS 1 messages in ONE write, reading
 *                 (a client with no in-flight window, as mqtt.js has by
 *                 default): it gets the 3000 PUBACK, and is not dropped. Its
 *                 reading is paused while its answers wait (backpressure).
 *                 QoS 2 cannot be sent so: past max_inflight_messages (the
 *                 Receive Maximum) the broker disconnects the client.
 *              4. K sends 30000 PINGREQ in ONE write, reading: it gets the
 *                 30000 PINGRESP, slowed, and is not dropped.
 *
 *          The bound that came after 7.30.26 counted the packets read since
 *          the send queue was last empty, and read the whole queue, the
 *          forwarded messages included: case 1 was dropped ("Mqtt: the peer
 *          does not read its answers") while reading everything. The bound
 *          in bytes that came next dropped the readers of cases 3 and 4 (P
 *          after 4 PUBACKs): a reader that pipelines cannot show progress
 *          inside one read, as the C_TCP writes one packet per turn.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include "c_broker_answers.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BROKER_URL          "tcp://127.0.0.1:18123"

#define BURST_QOS0          30000
#define WINDOW              20
#define LIB_MESSAGES        2000        // of each QoS
#define PIPELINED_QOS1      3000
#define PIPELINED_PINGREQ   30000

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
    hgobj tcp;              // the client of the case
    gbuffer_t *rx;          // what it got, not parsed yet
    int phase;
    BOOL closed;            // its EV_DISCONNECTED: never before the end of its case
    int forwarded;          // PUBLISH the broker sent to it
    int sent;               // case 2: PUBLISH sent in the QoS of the step
    int acked;              // case 2: PUBACK / PUBCOMP got
    int qos;                // case 2: the QoS of the step, 1 then 2
    BOOL case_done;         // the client got all it waited for, and was stopped
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
    priv->tcp = gobj_create(
        "raw_client",
        C_TCP,
        json_pack("{s:s}", "url", BROKER_URL),
        gobj
    );
    priv->rx = gbuffer_create(4*1024, 16*1024*1024);
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->rx)
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
    if(gobj_is_running(priv->tcp)) {
        gobj_stop(priv->tcp);
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
 *  Write the client's bytes to the broker (gbuf owned)
 ***************************************************************************/
PRIVATE void client_sends_gbuf(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_send_event(
        priv->tcp,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
}

/***************************************************************************
 *  CONNECT, MQTT 3.1.1, clean session, no user
 ***************************************************************************/
PRIVATE void client_connect(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    static const uint8_t head311[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04,   // protocol name, level 4 (3.1.1)
        0x02,                                   // flags: clean session
        0x00, 0x3C                              // keep alive 60 s
    };
    static const char *client_ids[] = {"", "answers_s", "answers_l", "answers_p", "answers_k"};
    const char *client_id = client_ids[priv->phase];
    size_t remaining = sizeof(head311) + 2 + strlen(client_id);
    gbuffer_t *gbuf = gbuffer_create(remaining + 2, remaining + 2);

    gbuffer_append_char(gbuf, 0x10);
    gbuffer_append_char(gbuf, (char)remaining);
    gbuffer_append(gbuf, (void *)head311, sizeof(head311));
    gbuffer_append_char(gbuf, 0x00);
    gbuffer_append_char(gbuf, (char)strlen(client_id));
    gbuffer_append_string(gbuf, client_id);
    client_sends_gbuf(gobj, gbuf);
}

/***************************************************************************
 *  SUBSCRIBE of one filter, QoS 0
 ***************************************************************************/
PRIVATE void client_subscribe(hgobj gobj, const char *filter)
{
    size_t flen = strlen(filter);
    size_t remaining = 2 + 2 + flen + 1;
    gbuffer_t *gbuf = gbuffer_create(remaining + 2, remaining + 2);

    gbuffer_append_char(gbuf, (char)0x82);
    gbuffer_append_char(gbuf, (char)remaining);
    gbuffer_append_char(gbuf, 0x00);
    gbuffer_append_char(gbuf, 0x01);            // packet id 1
    gbuffer_append_char(gbuf, 0x00);
    gbuffer_append_char(gbuf, (char)flen);
    gbuffer_append(gbuf, (void *)filter, flen);
    gbuffer_append_char(gbuf, 0x00);            // QoS 0
    client_sends_gbuf(gobj, gbuf);
}

/***************************************************************************
 *  `count` QoS 0 PUBLISH to fc/x, payload "x", in one write
 ***************************************************************************/
PRIVATE void client_sends_qos0_burst(hgobj gobj, int count)
{
    size_t len = 9 * (size_t)count;
    gbuffer_t *gbuf = gbuffer_create(len, len);
    for(int i=0; i<count; i++) {
        gbuffer_append_char(gbuf, 0x30);        // PUBLISH QoS 0
        gbuffer_append_char(gbuf, 7);
        gbuffer_append_char(gbuf, 0x00);
        gbuffer_append_char(gbuf, 0x04);
        gbuffer_append(gbuf, "fc/x", 4);
        gbuffer_append_char(gbuf, 'x');
    }
    client_sends_gbuf(gobj, gbuf);
}

/***************************************************************************
 *  `count` QoS 1 PUBLISH to pl/x, payload "z", in one write
 ***************************************************************************/
PRIVATE void client_sends_qos1_burst(hgobj gobj, int count)
{
    size_t len = 11 * (size_t)count;
    gbuffer_t *gbuf = gbuffer_create(len, len);
    for(int i=0; i<count; i++) {
        uint16_t mid = (uint16_t)(i + 1);
        gbuffer_append_char(gbuf, 0x32);        // PUBLISH QoS 1
        gbuffer_append_char(gbuf, 9);
        gbuffer_append_char(gbuf, 0x00);
        gbuffer_append_char(gbuf, 0x04);
        gbuffer_append(gbuf, "pl/x", 4);
        gbuffer_append_char(gbuf, (char)(mid >> 8));
        gbuffer_append_char(gbuf, (char)(mid & 0xFF));
        gbuffer_append_char(gbuf, 'z');
    }
    client_sends_gbuf(gobj, gbuf);
}

/***************************************************************************
 *  `count` PINGREQ in one write
 ***************************************************************************/
PRIVATE void client_sends_pingreqs(hgobj gobj, int count)
{
    size_t len = 2 * (size_t)count;
    gbuffer_t *gbuf = gbuffer_create(len, len);
    for(int i=0; i<count; i++) {
        gbuffer_append_char(gbuf, (char)0xC0);  // PINGREQ
        gbuffer_append_char(gbuf, 0x00);
    }
    client_sends_gbuf(gobj, gbuf);
}

/***************************************************************************
 *  The next PUBLISH of case 2, QoS 1 or 2, to lib/x, payload "y"
 ***************************************************************************/
PRIVATE void client_sends_lib_publish(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->sent++;
    uint16_t mid = (uint16_t)priv->sent;    // 1 .. LIB_MESSAGES in each step
    gbuffer_t *gbuf = gbuffer_create(12, 12);
    gbuffer_append_char(gbuf, (char)(0x30 | (priv->qos << 1)));
    gbuffer_append_char(gbuf, 10);
    gbuffer_append_char(gbuf, 0x00);
    gbuffer_append_char(gbuf, 0x05);
    gbuffer_append(gbuf, "lib/x", 5);
    gbuffer_append_char(gbuf, (char)(mid >> 8));
    gbuffer_append_char(gbuf, (char)(mid & 0xFF));
    gbuffer_append_char(gbuf, 'y');
    client_sends_gbuf(gobj, gbuf);
}

/***************************************************************************
 *  A PUBREL of case 2
 ***************************************************************************/
PRIVATE void client_sends_pubrel(hgobj gobj, const uint8_t *mid)
{
    gbuffer_t *gbuf = gbuffer_create(4, 4);
    gbuffer_append_char(gbuf, 0x62);
    gbuffer_append_char(gbuf, 0x02);
    gbuffer_append_char(gbuf, (char)mid[0]);
    gbuffer_append_char(gbuf, (char)mid[1]);
    client_sends_gbuf(gobj, gbuf);
}

/***************************************************************************
 *  The end of a case: the client disconnects, and is stopped
 ***************************************************************************/
PRIVATE void end_case(hgobj gobj)
{
    gbuffer_t *gbuf = gbuffer_create(2, 2);
    gbuffer_append_char(gbuf, (char)0xE0);      // DISCONNECT
    gbuffer_append_char(gbuf, 0x00);
    client_sends_gbuf(gobj, gbuf);
    gobj_post_event(gobj, EV_STOP_CLIENT, 0, gobj);
}

/***************************************************************************
 *  A packet the test did not expect
 ***************************************************************************/
PRIVATE void unexpected_packet(hgobj gobj, uint8_t type)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST: a packet not expected",
        "phase",        "%d", priv->phase,
        "type",         "%d", (int)type,
        NULL
    );
}

/***************************************************************************
 *  What the broker wrote to the client
 ***************************************************************************/
PRIVATE void parse_rx(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    while(gbuffer_leftbytes(priv->rx) >= 2) {
        const uint8_t *p = gbuffer_cur_rd_pointer(priv->rx);
        size_t avail = gbuffer_leftbytes(priv->rx);
        size_t remaining = 0;
        size_t multiplier = 1;
        size_t hdr = 1;
        BOOL complete = FALSE;
        while(hdr < avail && hdr < 5) {
            uint8_t b = p[hdr++];
            remaining += (b & 0x7F) * multiplier;
            multiplier *= 128;
            if(!(b & 0x80)) {
                complete = TRUE;
                break;
            }
        }
        if(!complete || hdr + remaining > avail) {
            break; // incomplete
        }
        uint8_t type = p[0] >> 4;
        uint8_t mid[2] = {0, 0};
        if(remaining >= 2) {
            mid[0] = p[hdr];
            mid[1] = p[hdr + 1];
        }
        gbuffer_get(priv->rx, hdr + remaining);

        switch(type) {
            case 2:     // CONNACK
                if(priv->phase == 3) {
                    client_sends_qos1_burst(gobj, PIPELINED_QOS1);
                } else if(priv->phase == 4) {
                    client_sends_pingreqs(gobj, PIPELINED_PINGREQ);
                } else {
                    client_subscribe(gobj, (priv->phase == 1)? "fc/#" : "lib/#");
                }
                break;

            case 9:     // SUBACK
                if(priv->phase == 1) {
                    client_sends_qos0_burst(gobj, BURST_QOS0);
                } else {
                    priv->qos = 1;
                    for(int i=0; i<WINDOW; i++) {
                        client_sends_lib_publish(gobj);
                    }
                }
                break;

            case 3:     // PUBLISH forwarded
                priv->forwarded++;
                if(priv->phase == 1 && priv->forwarded == BURST_QOS0) {
                    end_case(gobj);
                }
                if(priv->phase == 2 && priv->forwarded == 2 * LIB_MESSAGES &&
                        priv->qos == 2 && priv->acked == LIB_MESSAGES) {
                    end_case(gobj);
                }
                break;

            case 13:    // PINGRESP
                if(priv->phase != 4) {
                    unexpected_packet(gobj, type);
                    break;
                }
                priv->acked++;
                if(priv->acked == PIPELINED_PINGREQ) {
                    end_case(gobj);
                }
                break;

            case 4:     // PUBACK
                if(priv->phase == 3) {
                    priv->acked++;
                    if(priv->acked == PIPELINED_QOS1) {
                        end_case(gobj);
                    }
                    break;
                }
                // fall through
            case 7:     // PUBCOMP
                if(priv->phase != 2 || (type == 4) != (priv->qos == 1)) {
                    unexpected_packet(gobj, type);
                    break;
                }
                priv->acked++;
                if(priv->sent < LIB_MESSAGES) {
                    client_sends_lib_publish(gobj);
                } else if(priv->acked == LIB_MESSAGES && priv->qos == 1) {
                    priv->qos = 2;
                    priv->sent = 0;
                    priv->acked = 0;
                    for(int i=0; i<WINDOW; i++) {
                        client_sends_lib_publish(gobj);
                    }
                } else if(priv->acked == LIB_MESSAGES && priv->forwarded == 2 * LIB_MESSAGES) {
                    end_case(gobj);
                }
                break;

            case 5:     // PUBREC
                if(priv->phase != 2 || priv->qos != 2) {
                    unexpected_packet(gobj, type);
                    break;
                }
                client_sends_pubrel(gobj, mid);
                break;

            default:
                unexpected_packet(gobj, type);
                break;
        }
    }
    if(gbuffer_leftbytes(priv->rx) == 0) {
        gbuffer_clear(priv->rx);    // what was parsed is not kept
    }
}

/***************************************************************************
 *  Start a case: a new connection of the client
 ***************************************************************************/
PRIVATE void start_case(hgobj gobj, int phase)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = phase;
    gbuffer_clear(priv->rx);
    priv->closed = FALSE;
    priv->forwarded = 0;
    priv->sent = 0;
    priv->acked = 0;
    priv->qos = 0;
    priv->case_done = FALSE;
    gobj_start(priv->tcp);      // CONNECT at its EV_CONNECTED
    set_timeout(priv->timer, 30000);    // the guard
}

/***************************************************************************
 *  A case went wrong: say it, and end
 ***************************************************************************/
PRIVATE void case_failed(hgobj gobj, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "phase",        "%d", priv->phase,
        "closed",       "%d", priv->closed,
        "forwarded",    "%d", priv->forwarded,
        "qos",          "%d", priv->qos,
        "sent",         "%d", priv->sent,
        "acked",        "%d", priv->acked,
        NULL
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases. A case ends when the client has all it waits for (it then
 *  disconnects); the timer is the guard that names the case that did not.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase) {
        case 0:
            start_case(gobj, 1);
            break;

        case 1:
        case 2:
        case 3:
        case 4:
            if(priv->case_done) {
                if(priv->phase < 4) {
                    start_case(gobj, priv->phase + 1);
                } else {
                    gobj_log_info(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INFO,
                        "msg",          "%s", "TEST: broker answers done",
                        NULL
                    );
                    set_yuno_must_die();
                }
                break;
            }
            {
                static const char *what[] = {
                    "",
                    "TEST: 1. a subscriber that reads a burst of its own messages did not get them all",
                    "TEST: 2. a client with a window of 20 did not get all its acks and messages",
                    "TEST: 3. a reader that pipelines 3000 QoS 1 did not get all its PUBACKs",
                    "TEST: 4. a reader that pipelines 30000 PINGREQ did not get all its PINGRESPs"
                };
                case_failed(gobj, what[priv->phase]);
            }
            set_yuno_must_die();
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST: phase unknown",
                "phase",        "%d", priv->phase,
                NULL
            );
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client is connected: CONNECT
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    client_connect(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the broker wrote to the client
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gbuf) {
        gbuffer_append_gbuf(priv->rx, gbuf);
    }
    parse_rx(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The client's connection closed before the end of its case: the broker
 *  dropped it
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->closed) {
        priv->closed = TRUE;
        case_failed(gobj, "TEST: a client that reads was dropped by the broker");
        set_yuno_must_die();
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The end of a case: stop the client, out of its own publication; the
 *  next case starts on the timer
 ***************************************************************************/
PRIVATE int ac_stop_client(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->closed = TRUE;    // its close is the end of its case, not a drop
    priv->case_done = TRUE;
    if(gobj_is_running(priv->tcp)) {
        gobj_stop(priv->tcp);
    }
    set_timeout(priv->timer, 300);  // the next case, once its stop has ended

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The transport's other events: nothing to do
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
GOBJ_DEFINE_GCLASS(C_BROKER_ANSWERS);

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
PUBLIC int register_c_broker_answers(void)
{
    return create_gclass(C_BROKER_ANSWERS);
}
