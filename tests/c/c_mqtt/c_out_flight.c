/****************************************************************************
 *          C_OUT_FLIGHT.C
 *
 *          Driver of the test of the OUTGOING in-flight window of the
 *          broker, and of the acks a client answers it with.
 *
 *          The client is RAW: a C_TCP of this gclass that writes the MQTT 5
 *          packets by hand, so every packet the broker sends is seen as it
 *          is on the wire.
 *
 *              1. CONNECT (MQTT 5, Receive Maximum 2, Session Expiry 60),
 *                 SUBSCRIBE t/o QoS 1, and four QoS 1 PUBLISH to t/o: the
 *                 broker delivers them to the client itself. The broker's
 *                 max_inflight_messages is 0 ("no maximum"), so the only
 *                 limit is the client's Receive Maximum: the broker must
 *                 send TWO, and hold the other two until an ack frees a
 *                 slot [MQTT-3.3.4-9]. Up to 7.25.4 db__ready_for_flight()
 *                 answered TRUE before it looked at the queue's limit when
 *                 the broker had no maximum of its own, and all four went.
 *              2. PUBACK of the two: the other two come. PUBACK of the
 *                 third. `list-queues queue=<client>-OUT qos=1` must list
 *                 ONE message: the pending one. Up to 7.25.4 `qos=`
 *                 replaced the pending mask, and the three delivered ones
 *                 were listed too.
 *              3. The fourth (QoS 1) is acked with a PUBCOMP, the ack of
 *                 QoS 2: a protocol error of the peer. The broker must say
 *                 it as a WARNING (MSGSET_MQTT), answer a DISCONNECT 0x82
 *                 (Protocol Error) and keep the message pending, as
 *                 mosquitto does (MOSQ_ERR_PROTOCOL). Up to 7.25.4 it
 *                 logged an ERROR "QoS mismatch" and removed the message
 *                 as delivered.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_out_flight.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BROKER_URL      "tcp://127.0.0.1:18114"
#define BROKER_SERVICE  "mqtt_broker"
#define OUT_QUEUE       "oflight_cl-OUT"
#define MAX_MIDS        16

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void send_bytes(hgobj gobj, const uint8_t *bf, size_t len);
PRIVATE void send_ack(hgobj gobj, uint8_t type, uint16_t mid);

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
    hgobj gobj_tcp;
    int phase;
    gbuffer_t *rx;          // what the broker sent, not parsed yet

    uint16_t mids[MAX_MIDS];    // packet ids of the PUBLISH of the broker
    int n_mids;
    int disconnect_reason;      // -1 none
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
    priv->gobj_tcp = gobj_create(
        "raw_client",
        C_TCP,
        json_pack("{s:s}", "url", BROKER_URL),
        gobj
    );
    priv->rx = gbuffer_create(1024, 1024);
    priv->disconnect_reason = -1;
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
    if(gobj_is_running(priv->gobj_tcp)) {
        gobj_stop(priv->gobj_tcp);
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
PRIVATE void send_bytes(hgobj gobj, const uint8_t *bf, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(len, len);
    gbuffer_append(gbuf, (void *)bf, len);
    gobj_send_event(
        priv->gobj_tcp,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
}

/***************************************************************************
 *  PUBACK (0x40) or PUBCOMP (0x70) of a packet id, MQTT 5 short form
 ***************************************************************************/
PRIVATE void send_ack(hgobj gobj, uint8_t type, uint16_t mid)
{
    uint8_t ack[] = {type, 0x02, (uint8_t)(mid >> 8), (uint8_t)(mid & 0xFF)};
    send_bytes(gobj, ack, sizeof(ack));
}

/***************************************************************************
 *  Parse what the broker sent: the packet ids of its PUBLISH, and the
 *  reason of a DISCONNECT
 ***************************************************************************/
PRIVATE void parse_rx(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    while(gbuffer_leftbytes(priv->rx) >= 2) {
        const uint8_t *p = gbuffer_cur_rd_pointer(priv->rx);
        size_t avail = gbuffer_leftbytes(priv->rx);
        uint32_t remaining = 0;
        uint32_t multiplier = 1;
        size_t hdr = 1;
        while(hdr < avail) {
            uint8_t b = p[hdr++];
            remaining += (b & 0x7F) * multiplier;
            multiplier *= 128;
            if(!(b & 0x80)) {
                break;
            }
        }
        if(hdr + remaining > avail) {
            return; // incomplete
        }
        uint8_t type = p[0] >> 4;
        const uint8_t *v = p + hdr;
        if(type == 3) {
            int qos = (p[0] >> 1) & 0x03;
            uint16_t topic_len = (uint16_t)((v[0] << 8) | v[1]);
            if(qos > 0 && priv->n_mids < MAX_MIDS) {
                const uint8_t *m = v + 2 + topic_len;
                priv->mids[priv->n_mids++] = (uint16_t)((m[0] << 8) | m[1]);
            }
        } else if(type == 14) {
            priv->disconnect_reason = remaining > 0? v[0] : 0;
        }
        gbuffer_get(priv->rx, hdr + remaining);
    }
}

/***************************************************************************
 *  How many messages `list-queues queue=<out> qos=1` lists
 ***************************************************************************/
PRIVATE int list_pending_qos1(hgobj gobj)
{
    hgobj broker = gobj_find_service(BROKER_SERVICE, TRUE);
    json_t *response = gobj_command(broker, "list-queues",
        json_pack("{s:s, s:i}", "queue", OUT_QUEUE, "qos", 1), gobj
    );
    int n = -1;
    if(kw_get_int(gobj, response, "result", -1, 0) == 0) {
        json_t *data = kw_get_list(gobj, response, "data", 0, 0);
        n = data? (int)json_array_size(data): -1;
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: list-queues FAILED",
            "comment",      "%s", kw_get_str(gobj, response, "comment", "", 0),
            NULL
        );
    }
    JSON_DECREF(response)
    return n;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_error(hgobj gobj, const char *msg, int got, int expected)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", msg,
        "got",          "%d", got,
        "expected",     "%d", expected,
        NULL
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase++;

    switch(priv->phase) {
        case 1:
            gobj_start(priv->gobj_tcp);     // CONNECT goes at EV_CONNECTED
            set_timeout(priv->timer, 500);
            break;

        case 2:
            /*
             *  1. Only the client's Receive Maximum in flight
             */
            parse_rx(gobj);
            if(priv->n_mids != 2) {
                test_error(gobj,
                    "TEST: the broker did not honour the client's Receive Maximum",
                    priv->n_mids, 2
                );
            }
            for(int i = 0; i < priv->n_mids; i++) {
                send_ack(gobj, 0x40, priv->mids[i]);    // PUBACK
            }
            set_timeout(priv->timer, 500);
            break;

        case 3:
            /*
             *  2. The other two came: one acked, one pending
             */
            parse_rx(gobj);
            if(priv->n_mids != 4) {
                test_error(gobj,
                    "TEST: the held messages did not come after the acks",
                    priv->n_mids, 4
                );
            }
            if(priv->n_mids >= 3) {
                send_ack(gobj, 0x40, priv->mids[2]);    // PUBACK of the third
            }
            set_timeout(priv->timer, 300);
            break;

        case 4:
            {
                int n = list_pending_qos1(gobj);
                if(n != 1) {
                    test_error(gobj,
                        "TEST: list-queues qos=1 did not list only the pending message",
                        n, 1
                    );
                }

                /*
                 *  3. The fourth acked with the ack of QoS 2
                 */
                if(priv->n_mids >= 4) {
                    send_ack(gobj, 0x70, priv->mids[3]);    // PUBCOMP
                }
                set_timeout(priv->timer, 500);
            }
            break;

        default:
            {
                parse_rx(gobj);
                if(priv->disconnect_reason != 0x82) {
                    test_error(gobj,
                        "TEST: a PUBCOMP of a QoS 1 message was not answered with DISCONNECT 0x82",
                        priv->disconnect_reason, 0x82
                    );
                }
                int n = list_pending_qos1(gobj);
                if(n != 1) {
                    test_error(gobj,
                        "TEST: the message acked with the wrong ack was not kept pending",
                        n, 1
                    );
                }
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "TEST: out flight done",
                    "publish",      "%d", priv->n_mids,
                    "disconnect",   "%d", priv->disconnect_reason,
                    "pending",      "%d", n,
                    NULL
                );
                if(gobj_is_running(priv->gobj_tcp)) {
                    gobj_stop(priv->gobj_tcp);
                }
                set_yuno_must_die();
            }
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Connected: CONNECT, SUBSCRIBE and four QoS 1 PUBLISH
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    static const uint8_t connect[] = {
        0x10, 31,                               // CONNECT, remaining length
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x05,   // protocol name, level 5
        0x00,                                   // flags: no clean start (the queue outlives the connection)
        0x00, 0x3C,                             // keep alive 60 s
        0x08,                                   // properties length
        0x11, 0x00, 0x00, 0x00, 0x3C,           // Session Expiry Interval 60
        0x21, 0x00, 0x02,                       // Receive Maximum 2
        0x00, 10, 'o', 'f', 'l', 'i', 'g', 'h', 't', '_', 'c', 'l'
    };
    send_bytes(gobj, connect, sizeof(connect));

    static const uint8_t subscribe[] = {
        0x82, 9,                                // SUBSCRIBE, remaining length
        0x00, 0x01,                             // packet id
        0x00,                                   // properties length
        0x00, 3, 't', '/', 'o',                 // topic filter
        0x01                                    // QoS 1
    };
    send_bytes(gobj, subscribe, sizeof(subscribe));

    uint8_t publish[] = {
        0x32, 9,                                // PUBLISH, QoS 1, remaining length
        0x00, 3, 't', '/', 'o',                 // topic
        0x00, 0x00,                             // packet id, set below
        0x00,                                   // properties length
        'p'                                     // payload
    };
    for(uint8_t mid = 10; mid < 14; mid++) {
        publish[8] = mid;
        send_bytes(gobj, publish, sizeof(publish));
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the broker sends
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gbuf) {
        gbuffer_append_gbuf(priv->rx, gbuf);
    }

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
GOBJ_DEFINE_GCLASS(C_OUT_FLIGHT);

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
        {EV_TIMEOUT,            ac_timeout,         0},
        {EV_CONNECTED,          ac_connected,       0},
        {EV_RX_DATA,            ac_rx_data,         0},
        {EV_DISCONNECTED,       ac_transport_event, 0},
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
PUBLIC int register_c_out_flight(void)
{
    return create_gclass(C_OUT_FLIGHT);
}
