/****************************************************************************
 *          C_CLIENT_QUEUES.C
 *
 *          Driver of the test of the persistent queues of an MQTT CLIENT
 *          (C_PROT_MQTT2 with `tranger_queues`), against a RAW broker: a
 *          C_PROT_RAW channel of this test that writes the MQTT 3.1.1
 *          packets by hand, so the packets of the client are seen as they
 *          are on the wire.
 *
 *          A.  An incoming QoS 2 message sent again with DUP=1 (its PUBREC
 *              was lost): the copy waiting for its PUBREL is replaced, the
 *              message is delivered ONCE, and a later message that reuses
 *              the packet id is delivered as itself.
 *
 *                  PUBLISH 5 'a'           -> PUBREC 5
 *                  PUBLISH 5 'a' DUP       -> PUBREC 5
 *                  PUBREL 5                -> PUBCOMP 5, 'a' delivered
 *                  PUBLISH 5 'b'           -> PUBREC 5
 *                  PUBREL 5                -> PUBCOMP 5, 'b' delivered
 *
 *              Up to 7.25.4 the duplicate was searched by the ROWID of the
 *              queue record, not by the packet id: the old copy stayed in
 *              flight for ever, and the PUBREL of the next message with
 *              packet id 5 delivered the stale 'a' again instead of 'b'.
 *
 *          B.  An outgoing message that expires while it is QUEUED (the 20
 *              in-flight slots taken by messages not acknowledged): when
 *              a slot frees it goes in flight, it is discarded as expired,
 *              and the next queued message must take the slot.
 *
 *                  22 QoS 1 PUBLISH by the user, 'm01'..'m22', 'm21' with
 *                  expiry_interval 1: 'm01'..'m20' are sent, 'm21' and
 *                  'm22' are queued. 2.5 s later, PUBACK 1: 'm21' is
 *                  discarded, 'm22' must be sent.
 *
 *              Up to 7.25.4 nothing was moved from the queued list after
 *              an expiry: 'm22' waited for more traffic that might never
 *              come.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_client_queues.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define QUEUES_PATH     "/tmp/test_mqtt_client_queues"
#define OUT_MESSAGES    22
#define EXPIRING        21      // the message that expires while queued

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void broker_send(hgobj gobj, const uint8_t *bf, size_t len);
PRIVATE void client_publish(hgobj gobj, int n);

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
    hgobj raw_broker;       // the IOGATE of the raw broker
    hgobj output_side;      // the IOGATE of the mqtt client
    json_t *tranger;        // the queues of the client
    int phase;
    BOOL client_open;
    BOOL client_closed;
    gbuffer_t *rx;          // what the client sent to the raw broker
    char delivered[64];     // the payloads delivered to the user, in order
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
    priv->rx = gbuffer_create(4096, 4096);
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

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->raw_broker = gobj_find_service("__raw_broker__", TRUE);
    gobj_subscribe_event(priv->raw_broker, NULL, 0, gobj);
    priv->output_side = gobj_find_service("__output_side__", TRUE);
    gobj_subscribe_event(priv->output_side, NULL, 0, gobj);

    /*
     *  The queues of the client
     */
    rmrdir(QUEUES_PATH);
    priv->tranger = tranger2_startup(0, json_pack("{s:s, s:s, s:b}",
        "path", QUEUES_PATH,
        "database", "queues",
        "master", 1
    ), 0);
    hgobj channel = gobj_child_by_name(priv->output_side, "mqtt_client");
    hgobj prot = channel? gobj_child_by_name(channel, "mqtt_client"):0;
    if(!prot || !priv->tranger) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: no client protocol or no tranger",
            NULL
        );
    } else {
        gobj_write_pointer_attr(prot, "tranger_queues", priv->tranger);
    }

    gobj_start_tree(priv->raw_broker);      // it listens at its start
    gobj_start_tree(priv->output_side);
    set_timeout(priv->timer, 500);

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
 *  Raw bytes from the broker to the client
 ***************************************************************************/
PRIVATE void broker_send(hgobj gobj, const uint8_t *bf, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(len, len);
    gbuffer_append(gbuf, (void *)bf, len);
    gobj_send_event(
        priv->raw_broker,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
}

/***************************************************************************
 *  A QoS 2 PUBLISH to the client, packet id 5, topic 't/d'
 ***************************************************************************/
PRIVATE void broker_publish_qos2(hgobj gobj, char payload, BOOL dup)
{
    uint8_t publish[] = {
        0x34, 8,                        // PUBLISH, QoS 2, remaining length
        0x00, 3, 't', '/', 'd',         // topic
        0x00, 0x05,                     // packet id 5
        0                               // payload
    };
    if(dup) {
        publish[0] |= 0x08;
    }
    publish[9] = (uint8_t)payload;
    broker_send(gobj, publish, sizeof(publish));
}

/***************************************************************************
 *  A QoS 1 PUBLISH of the user of the client, payload 'mNN'
 ***************************************************************************/
PRIVATE void client_publish(hgobj gobj, int n)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char payload[8];
    snprintf(payload, sizeof(payload), "m%02d", n);
    gbuffer_t *gbuf = gbuffer_create(8, 8);
    gbuffer_append_string(gbuf, payload);

    json_t *kw_pub = json_pack("{s:s, s:i, s:i, s:b, s:I}",
        "topic",            "t/o",
        "qos",              1,
        "expiry_interval",  (n == EXPIRING)? 1: 0,
        "retain",           0,
        "gbuffer",          (json_int_t)(uintptr_t)gbuf
    );
    json_t *kw_iev = iev_create(gobj, EV_MQTT_PUBLISH, kw_pub);
    gobj_send_event(priv->output_side, EV_SEND_IEV, kw_iev, gobj);
}

/***************************************************************************
 *  A printable dump of what was received
 ***************************************************************************/
PRIVATE void print_bytes(char *bf, size_t bfsize, const uint8_t *p, size_t len)
{
    bf[0] = 0;
    size_t ln = 0;
    for(size_t i = 0; i < len && ln + 4 < bfsize; i++) {
        ln += (size_t)snprintf(bf + ln, bfsize - ln, "%02X ", p[i]);
    }
}

/***************************************************************************
 *  What the raw broker received must be `expected`; the rx is cleared
 ***************************************************************************/
PRIVATE void expect_rx(hgobj gobj, const char *what, const uint8_t *expected, size_t expected_len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t len = gbuffer_leftbytes(priv->rx);
    const uint8_t *p = gbuffer_cur_rd_pointer(priv->rx);
    if(len != expected_len || memcmp(p, expected, len) != 0) {
        char dump[512];
        char exp[128];
        print_bytes(dump, sizeof(dump), p, len);
        print_bytes(exp, sizeof(exp), expected, expected_len);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: the client did not answer as expected",
            "step",         "%s", what,
            "received",     "%s", dump,
            "expected",     "%s", exp,
            NULL
        );
    }
    gbuffer_clear(priv->rx);
}

/***************************************************************************
 *  TRUE if the payload 'mNN' was sent by the client
 ***************************************************************************/
PRIVATE BOOL rx_has_payload(hgobj gobj, int n)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char payload[8];
    snprintf(payload, sizeof(payload), "m%02d", n);
    size_t len = gbuffer_leftbytes(priv->rx);
    const char *p = gbuffer_cur_rd_pointer(priv->rx);
    for(size_t i = 0; i + 3 <= len; i++) {
        if(memcmp(p + i, payload, 3) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_fail(hgobj gobj, const char *what, const char *detail)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "detail",       "%s", detail,
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

    static const uint8_t pubrec5[]  = {0x50, 0x02, 0x00, 0x05};
    static const uint8_t pubcomp5[] = {0x70, 0x02, 0x00, 0x05};
    static const uint8_t pubrel5[]  = {0x62, 0x02, 0x00, 0x05};

    priv->phase++;

    switch(priv->phase) {
        case 1:
            {
                /*
                 *  The CONNECT of the client: CONNACK
                 */
                size_t len = gbuffer_leftbytes(priv->rx);
                const uint8_t *p = gbuffer_cur_rd_pointer(priv->rx);
                if(len == 0 || p[0] != 0x10) {
                    test_fail(gobj, "TEST: the client did not send its CONNECT", "");
                }
                gbuffer_clear(priv->rx);
                static const uint8_t connack[] = {0x20, 0x02, 0x00, 0x00};
                broker_send(gobj, connack, sizeof(connack));
                set_timeout(priv->timer, 300);
            }
            break;

        case 2:
            /*
             *  A. PUBLISH 5 'a'
             */
            if(!priv->client_open) {
                test_fail(gobj, "TEST: the client did not open", "");
            }
            broker_publish_qos2(gobj, 'a', FALSE);
            set_timeout(priv->timer, 300);
            break;

        case 3:
            /*
             *  Its PUBREC is 'lost': PUBLISH 5 'a' again, DUP
             */
            expect_rx(gobj, "PUBREC of PUBLISH 5", pubrec5, sizeof(pubrec5));
            broker_publish_qos2(gobj, 'a', TRUE);
            set_timeout(priv->timer, 300);
            break;

        case 4:
            expect_rx(gobj, "PUBREC of the DUP PUBLISH 5", pubrec5, sizeof(pubrec5));
            broker_send(gobj, pubrel5, sizeof(pubrel5));
            set_timeout(priv->timer, 300);
            break;

        case 5:
            /*
             *  A later message with the same packet id
             */
            expect_rx(gobj, "PUBCOMP of PUBREL 5", pubcomp5, sizeof(pubcomp5));
            broker_publish_qos2(gobj, 'b', FALSE);
            set_timeout(priv->timer, 300);
            break;

        case 6:
            expect_rx(gobj, "PUBREC of the second PUBLISH 5", pubrec5, sizeof(pubrec5));
            broker_send(gobj, pubrel5, sizeof(pubrel5));
            set_timeout(priv->timer, 300);
            break;

        case 7:
            expect_rx(gobj, "PUBCOMP of the second PUBREL 5", pubcomp5, sizeof(pubcomp5));
            if(strcmp(priv->delivered, "a,b,") != 0) {
                test_fail(gobj,
                    "TEST: the QoS 2 messages were not delivered once each, as themselves",
                    priv->delivered
                );
            }

            /*
             *  B. 22 QoS 1 messages of the user: 20 in flight, 2 queued
             */
            for(int n = 1; n <= OUT_MESSAGES; n++) {
                client_publish(gobj, n);
            }
            set_timeout(priv->timer, 2500);   // 'm21' expires
            break;

        case 8:
            if(!rx_has_payload(gobj, 20) || rx_has_payload(gobj, 21) || rx_has_payload(gobj, 22)) {
                test_fail(gobj, "TEST: the client did not send 20 messages and queue 2", "");
            }
            gbuffer_clear(priv->rx);
            {
                static const uint8_t puback1[] = {0x40, 0x02, 0x00, 0x01};
                broker_send(gobj, puback1, sizeof(puback1));
            }
            set_timeout(priv->timer, 500);
            break;

        case 9:
            if(rx_has_payload(gobj, EXPIRING)) {
                test_fail(gobj, "TEST: the expired message was sent", "");
            }
            if(!rx_has_payload(gobj, 22)) {
                test_fail(gobj,
                    "TEST: the message queued after an expired one was not sent when a slot freed",
                    ""
                );
            } else {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "TEST: the queued message took the slot of the expired one",
                    NULL
                );
            }
            gbuffer_clear(priv->rx);
            gobj_stop_tree(priv->output_side);
            set_timeout(priv->timer, 500);
            break;

        default:
            if(!priv->client_closed) {
                test_fail(gobj, "TEST: the client did not close", "");
            }
            EXEC_AND_RESET(tranger2_shutdown, priv->tranger)
            set_yuno_must_die();
            break;
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

    if(src == priv->output_side) {
        priv->client_open = TRUE;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->output_side) {
        priv->client_closed = TRUE;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the client sends to the raw broker
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
 *  A message delivered by the client to its user
 ***************************************************************************/
PRIVATE int ac_mqtt_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gbuf && gbuffer_leftbytes(gbuf) > 0) {
        size_t ln = strlen(priv->delivered);
        if(ln + 3 < sizeof(priv->delivered)) {
            priv->delivered[ln] = *(char *)gbuffer_cur_rd_pointer(gbuf);
            priv->delivered[ln+1] = ',';
            priv->delivered[ln+2] = 0;
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The other events of the gates: nothing to do
 ***************************************************************************/
PRIVATE int ac_other_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
GOBJ_DEFINE_GCLASS(C_CLIENT_QUEUES);

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
        {EV_ON_OPEN,            ac_on_open,         0},
        {EV_ON_CLOSE,           ac_on_close,        0},
        {EV_ON_MESSAGE,         ac_on_message,      0},
        {EV_MQTT_MESSAGE,       ac_mqtt_message,    0},
        {EV_MQTT_PUBLISH,       ac_other_event,     0},
        {EV_ON_ID,              ac_other_event,     0},
        {EV_ON_ID_NAK,          ac_other_event,     0},
        {EV_STOPPED,            ac_other_event,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
        {EV_ON_MESSAGE,         0},
        {EV_MQTT_MESSAGE,       0},
        {EV_MQTT_PUBLISH,       0},
        {EV_ON_ID,              0},
        {EV_ON_ID_NAK,          0},
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
PUBLIC int register_c_client_queues(void)
{
    return create_gclass(C_CLIENT_QUEUES);
}
