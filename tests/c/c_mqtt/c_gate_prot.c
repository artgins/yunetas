/****************************************************************************
 *          C_GATE_PROT.C
 *
 *          Driver of the test of C_PROT_MQTT2 as the input of a GATE: no
 *          broker, no queues (tranger_queues NULL), allow_anonymous. This
 *          driver is the upper level of the protocol, as the decoder of a
 *          gate is: it answers the CONNECT (no session), writes the reason
 *          codes of a (UN)SUBSCRIBE, takes the messages, and sends one to
 *          the client. The protocol is its child, its bottom a
 *          C_FAKE_TRANSPORT, and the client's packets are written by hand.
 *
 *          Config of the protocol: max_subscriptions_per_client 3,
 *          max_topic_filter_length 16, max_inflight_messages 2.
 *
 *              1. MQTT 3.1.1, as a Tasmota: CONNECT with a username and a
 *                 password nobody checks: accepted.
 *              2. PUBLISH QoS 0, 1 and 2: each delivered once; PUBACK and
 *                 PUBREC. The QoS 2 sent again (DUP) before its PUBREL is
 *                 answered PUBREC and not delivered again; its PUBREL gets
 *                 PUBCOMP. Up to 7.30.25 a protocol without queues took no
 *                 QoS 1 or 2 PUBLISH (max_qos went to 0 at the CONNACK and
 *                 the client was disconnected).
 *              3. A PUBREL, PUBACK, PUBREC and PUBCOMP of packet ids the
 *                 server never used: answered, and said once (one WARNING
 *                 per connection), never a crash. Up to 7.30.25 each one
 *                 walked the NULL queue (SIGSEGV); up to 7.30.26 each one
 *                 was a WARNING.
 *              4. SUBSCRIBE of 5 filters: the one longer than 16 bytes and
 *                 the one past 3 are refused (0x80), the rest granted in
 *                 QoS 0 (without queues the server sends QoS 0 only). A
 *                 filter held is replaced, not counted again; one freed by
 *                 UNSUBSCRIBE makes room.
 *              5. The upper level sends the client a QoS 1 message: it goes
 *                 in QoS 0.
 *              6. Two QoS 2 PUBLISH without PUBREL, and a third: past
 *                 max_inflight_messages, the client is disconnected.
 *              7. MQTT 5: the refusals are 0x8F and 0x97, the counts are of
 *                 the connection (the previous one's are gone), and a QoS 1
 *                 PUBLISH taken by the upper level gets PUBACK Success.
 *              8. A PUBLISH with a property that is not of MQTT 5 (127): a
 *                 malformed packet, said with ONE WARNING "Mqtt unknown
 *                 property" (with the peer), DISCONNECT 0x81, the drop. Up
 *                 to 7.30.26 it was two ERRORs ("Mqtt unknown property",
 *                 "Mqtt empty property", with no peer) and a WARNING, for
 *                 each such packet of any peer, before its authentication.
 *
 *          No log of an ACL: there is no broker, and an anonymous client
 *          has nothing to ask it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_fake_transport.h"
#include "c_gate_prot.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CLIENT_ID           "gate_cl"
#define CLIENT_ID5          "gate_cl5"
#define USERNAME            "DVES_USER"
#define PASSWORD            "nobody-checks-it"
#define LONG_FILTER         "a/filter/past/16/bytes"

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

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
    hgobj prot;
    hgobj fake;
    int phase;
    BOOL mqtt5;             // the protocol of the connection, to parse what the server writes
    gbuffer_t *rx;          // what the protocol wrote, not parsed yet
    json_t *got;            // what the server wrote and what the upper level got, in order
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
    priv->rx = gbuffer_create(4*1024, 64*1024);
    priv->got = json_array();

    priv->prot = gobj_create(
        "gate_prot",
        C_PROT_MQTT2,
        json_pack("{s:b, s:b, s:i, s:i, s:i}",
            "iamServer", 1,
            "allow_anonymous", 1,
            "max_subscriptions_per_client", 3,
            "max_topic_filter_length", 16,
            "max_inflight_messages", 2
        ),
        gobj
    );

    priv->fake = gobj_create(
        "fake_transport",
        C_FAKE_TRANSPORT,
        json_pack("{s:I}", "sink", (json_int_t)(uintptr_t)gobj),
        priv->prot
    );
    gobj_set_bottom_gobj(priv->prot, priv->fake);
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->rx)
    JSON_DECREF(priv->got)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->fake);
    gobj_start(priv->prot);

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
    if(gobj_is_running(priv->prot)) {
        gobj_stop(priv->prot);  // it stops its bottom
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 100);

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
 *  Write the client's bytes into the protocol
 ***************************************************************************/
PRIVATE void peer_sends(hgobj gobj, const uint8_t *bf, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(len, len);
    gbuffer_append(gbuf, (void *)bf, len);
    gobj_send_event(
        priv->prot,
        EV_RX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        priv->fake
    );
}

/***************************************************************************
 *  Fixed header of a packet: type and a two-byte remaining length
 ***************************************************************************/
PRIVATE size_t put_fixed_header(uint8_t *bf, uint8_t type, size_t remaining)
{
    bf[0] = type;
    bf[1] = (uint8_t)((remaining & 0x7F) | 0x80);
    bf[2] = (uint8_t)(remaining >> 7);
    return 3;
}

/***************************************************************************
 *  A string of the packet: two-byte length, then the bytes
 ***************************************************************************/
PRIVATE size_t put_string(uint8_t *bf, const char *s)
{
    size_t len = strlen(s);
    bf[0] = (uint8_t)(len >> 8);
    bf[1] = (uint8_t)(len & 0xFF);
    memcpy(bf + 2, s, len);
    return 2 + len;
}

/***************************************************************************
 *  CONNECT, MQTT 3.1.1 with a username and a password, or MQTT 5 without
 ***************************************************************************/
PRIVATE void peer_connect(hgobj gobj, BOOL mqtt5)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t bf[512];
    size_t n;

    priv->mqtt5 = mqtt5;
    gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);

    if(mqtt5) {
        static const uint8_t head5[] = {
            0x00, 0x04, 'M', 'Q', 'T', 'T', 0x05,   // protocol name, level 5
            0x02,                                   // flags: clean start
            0x00, 0x3C,                             // keep alive 60 s
            0x00                                    // properties length
        };
        n = put_fixed_header(bf, 0x10, sizeof(head5) + 2 + strlen(CLIENT_ID5));
        memcpy(bf + n, head5, sizeof(head5));
        n += sizeof(head5);
        n += put_string(bf + n, CLIENT_ID5);
    } else {
        static const uint8_t head311[] = {
            0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04,   // protocol name, level 4 (3.1.1)
            0xC2,                                   // flags: username, password, clean session
            0x00, 0x3C                              // keep alive 60 s
        };
        n = put_fixed_header(bf, 0x10,
            sizeof(head311) + 2 + strlen(CLIENT_ID) + 2 + strlen(USERNAME) + 2 + strlen(PASSWORD)
        );
        memcpy(bf + n, head311, sizeof(head311));
        n += sizeof(head311);
        n += put_string(bf + n, CLIENT_ID);
        n += put_string(bf + n, USERNAME);
        n += put_string(bf + n, PASSWORD);
    }
    peer_sends(gobj, bf, n);
}

/***************************************************************************
 *  PUBLISH
 ***************************************************************************/
PRIVATE void peer_publish(hgobj gobj, int qos, BOOL dup, uint16_t mid, const char *topic, const char *payload)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t bf[512];
    size_t n;

    size_t remaining = 2 + strlen(topic) + (qos? 2 : 0) + (priv->mqtt5? 1 : 0) + strlen(payload);
    n = put_fixed_header(bf, (uint8_t)(0x30 | (dup? 0x08 : 0) | (qos << 1)), remaining);
    n += put_string(bf + n, topic);
    if(qos) {
        bf[n++] = (uint8_t)(mid >> 8);
        bf[n++] = (uint8_t)(mid & 0xFF);
    }
    if(priv->mqtt5) {
        bf[n++] = 0x00;     // properties length
    }
    memcpy(bf + n, payload, strlen(payload));
    n += strlen(payload);
    peer_sends(gobj, bf, n);
}

/***************************************************************************
 *  PUBACK (0x40), PUBREC (0x50), PUBREL (0x62), PUBCOMP (0x70)
 ***************************************************************************/
PRIVATE void peer_ack(hgobj gobj, uint8_t type, uint16_t mid)
{
    uint8_t bf[4] = {type, 0x02, (uint8_t)(mid >> 8), (uint8_t)(mid & 0xFF)};
    peer_sends(gobj, bf, sizeof(bf));
}

/***************************************************************************
 *  SUBSCRIBE (options = QoS 1) or UNSUBSCRIBE of a list of filters
 ***************************************************************************/
PRIVATE void peer_subscribe(hgobj gobj, BOOL unsubscribe, uint16_t mid, const char **filters)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t bf[1024];
    size_t n;

    size_t remaining = 2 + (priv->mqtt5? 1 : 0);
    for(int i=0; filters[i]; i++) {
        remaining += 2 + strlen(filters[i]) + (unsubscribe? 0 : 1);
    }
    n = put_fixed_header(bf, unsubscribe? 0xA2 : 0x82, remaining);
    bf[n++] = (uint8_t)(mid >> 8);
    bf[n++] = (uint8_t)(mid & 0xFF);
    if(priv->mqtt5) {
        bf[n++] = 0x00;     // properties length
    }
    for(int i=0; filters[i]; i++) {
        n += put_string(bf + n, filters[i]);
        if(!unsubscribe) {
            bf[n++] = 0x01;
        }
    }
    peer_sends(gobj, bf, n);
}

/***************************************************************************
 *  What the server wrote, as lines in `got`
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
        int mid = (remaining >= 2)? ((v[0] << 8) | v[1]) : 0;
        char line[256];
        line[0] = 0;

        switch(type) {
            case 2:     // CONNACK
                snprintf(line, sizeof(line), "CONNACK %d", remaining >= 2? v[1] : -1);
                break;
            case 3:     // PUBLISH
                {
                    int qos = (p[0] >> 1) & 0x03;
                    int tlen = (v[0] << 8) | v[1];
                    uint32_t i = 2 + tlen + (qos? 2 : 0);
                    if(priv->mqtt5) {
                        i += 1 + v[i];
                    }
                    snprintf(line, sizeof(line), "PUBLISH %.*s q%d %.*s",
                        tlen, (const char *)v + 2, qos, (int)(remaining - i), (const char *)v + i
                    );
                }
                break;
            case 4:     // PUBACK
                snprintf(line, sizeof(line), "PUBACK %d %d", mid, remaining > 2? v[2] : 0);
                break;
            case 5:     // PUBREC
                snprintf(line, sizeof(line), "PUBREC %d", mid);
                break;
            case 6:     // PUBREL
                snprintf(line, sizeof(line), "PUBREL %d", mid);
                break;
            case 7:     // PUBCOMP
                snprintf(line, sizeof(line), "PUBCOMP %d", mid);
                break;
            case 9:     // SUBACK
            case 11:    // UNSUBACK
                {
                    int l = snprintf(line, sizeof(line), "%s %d", type==9? "SUBACK":"UNSUBACK", mid);
                    uint32_t i = 2;
                    if(priv->mqtt5) {
                        i += 1 + v[2];
                    }
                    for(; i < remaining && l < (int)sizeof(line) - 8; i++) {
                        l += snprintf(line + l, sizeof(line) - l, " %d", v[i]);
                    }
                }
                break;
            case 14:    // DISCONNECT
                snprintf(line, sizeof(line), "DISCONNECT %d", remaining >= 1? v[0] : 0);
                break;
            default:
                snprintf(line, sizeof(line), "TYPE %d", type);
                break;
        }
        json_array_append_new(priv->got, json_string(line));
        gbuffer_get(priv->rx, hdr + remaining);
    }
}

/***************************************************************************
 *  Compare what was got with the lines expected, and forget it
 ***************************************************************************/
PRIVATE void expect(hgobj gobj, const char *step, const char **lines)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    parse_rx(gobj);

    json_t *expected = json_array();
    for(int i=0; lines[i]; i++) {
        json_array_append_new(expected, json_string(lines[i]));
    }
    if(!json_equal(priv->got, expected)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: not what was expected",
            "step",         "%s", step,
            "got",          "%j", priv->got,
            "expected",     "%j", expected,
            NULL
        );
    }
    JSON_DECREF(expected)
    json_array_clear(priv->got);
}

/***************************************************************************
 *  The first connection, MQTT 3.1.1
 ***************************************************************************/
PRIVATE void run_mqtt311(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    peer_connect(gobj, FALSE);
    expect(gobj, "1. CONNECT 3.1.1 with credentials", (const char *[]){
        "OPEN " CLIENT_ID,
        "CONNACK 0",
        NULL
    });

    peer_publish(gobj, 0, FALSE, 0, "t/0", "p0");
    peer_publish(gobj, 1, FALSE, 10, "t/1", "p1");
    peer_publish(gobj, 2, FALSE, 20, "t/2", "p2");
    peer_publish(gobj, 2, TRUE, 20, "t/2", "p2");
    peer_ack(gobj, 0x62, 20);
    expect(gobj, "2. PUBLISH QoS 0, 1, 2 and the QoS 2 again", (const char *[]){
        "MSG t/0 q0 p0",
        "MSG t/1 q1 p1",
        "PUBACK 10 0",
        "MSG t/2 q2 p2",
        "PUBREC 20",
        "PUBREC 20",
        "PUBCOMP 20",
        NULL
    });

    peer_ack(gobj, 0x62, 21);
    peer_ack(gobj, 0x40, 30);
    peer_ack(gobj, 0x50, 31);
    peer_ack(gobj, 0x70, 32);
    expect(gobj, "3. acks of packet ids never used", (const char *[]){
        "PUBCOMP 21",
        "PUBREL 31",
        NULL
    });

    peer_subscribe(gobj, FALSE, 1, (const char *[]){
        "a/1", "a/2", LONG_FILTER, "a/3", "a/4", NULL
    });
    peer_subscribe(gobj, FALSE, 2, (const char *[]){"a/1", "a/5", NULL});
    peer_subscribe(gobj, TRUE, 3, (const char *[]){"a/2", NULL});
    peer_subscribe(gobj, FALSE, 4, (const char *[]){"a/5", NULL});
    expect(gobj, "4. SUBSCRIBE limits", (const char *[]){
        "SUB a/1 q0",
        "SUB a/2 q0",
        "SUB a/3 q0",
        "SUBACK 1 0 0 128 0 128",
        "SUB a/1 q0",
        "SUBACK 2 0 128",
        "UNSUB a/2",
        "UNSUBACK 3",
        "SUB a/5 q0",
        "SUBACK 4 0",
        NULL
    });

    gbuffer_t *gbuf = gbuffer_create(8, 8);
    gbuffer_append_string(gbuf, "on");
    gobj_send_event(priv->prot, EV_SEND_MESSAGE,
        json_pack("{s:s, s:I, s:i, s:b}",
            "topic", "cmnd/x",
            "gbuffer", (json_int_t)(uintptr_t)gbuf,
            "qos", 1,
            "retain", 0
        ),
        gobj
    );
    expect(gobj, "5. a QoS 1 message to the client", (const char *[]){
        "PUBLISH cmnd/x q0 on",
        NULL
    });

    peer_publish(gobj, 2, FALSE, 40, "t/2", "a");
    peer_publish(gobj, 2, FALSE, 41, "t/2", "b");
    peer_publish(gobj, 2, FALSE, 42, "t/2", "c");
    expect(gobj, "6. QoS 2 past max_inflight_messages", (const char *[]){
        "MSG t/2 q2 a",
        "PUBREC 40",
        "MSG t/2 q2 b",
        "PUBREC 41",
        NULL
    });
}

/***************************************************************************
 *  The second connection, MQTT 5
 ***************************************************************************/
PRIVATE void run_mqtt5(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_read_integer_attr(priv->fake, "drops") != 1) {
        json_t *got = json_integer(gobj_read_integer_attr(priv->fake, "drops"));
        json_t *expected = json_integer(1);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: the client past max_inflight_messages was not dropped",
            "got",          "%j", got,
            "expected",     "%j", expected,
            NULL
        );
        JSON_DECREF(got)
        JSON_DECREF(expected)
    }
    expect(gobj, "6. the drop", (const char *[]){
        "CLOSE " CLIENT_ID,
        NULL
    });

    peer_connect(gobj, TRUE);
    peer_subscribe(gobj, FALSE, 1, (const char *[]){
        "b/1", LONG_FILTER, "b/2", "b/3", "b/4", NULL
    });
    peer_publish(gobj, 1, FALSE, 7, "t/5", "p5");
    expect(gobj, "7. MQTT 5", (const char *[]){
        "OPEN " CLIENT_ID5,
        "CONNACK 0",
        "SUB b/1 q0",
        "SUB b/2 q0",
        "SUB b/3 q0",
        "SUBACK 1 0 143 0 0 151",
        "MSG t/5 q1 p5",
        "PUBACK 7 0",
        NULL
    });

    static const uint8_t publish_unknown_property[] = {
        0x30, 0x09,                 // PUBLISH QoS 0
        0x00, 0x03, 't', '/', '8',  // topic
        0x02,                       // properties length
        0x7F, 0x00,                 // property 127: not of MQTT 5
        'z'
    };
    peer_sends(gobj, publish_unknown_property, sizeof(publish_unknown_property));
    expect(gobj, "8. a PUBLISH with a property not of MQTT 5", (const char *[]){
        "DISCONNECT 129",
        NULL
    });
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
            run_mqtt311(gobj);
            set_timeout(priv->timer, 100);  // the drop comes on the next cycle
            break;

        case 2:
            run_mqtt5(gobj);
            set_timeout(priv->timer, 100);
            break;

        default:
            expect(gobj, "8. the drop", (const char *[]){
                "CLOSE " CLIENT_ID5,
                NULL
            });
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: gate prot done",
                NULL
            );
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the protocol writes, through the fake transport
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gbuf) {
        gbuffer_append_gbuf(priv->rx, gbuf);
    }
    parse_rx(gobj);     // keep the order of what is written and what is published

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A CONNECT: no session kept, so no session present
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_array_append_new(priv->got,
        json_sprintf("OPEN %s", kw_get_str(gobj, kw, "id", "", KW_REQUIRED))
    );

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_array_append_new(priv->got,
        json_sprintf("CLOSE %s", kw_get_str(gobj, kw, "client_id", "", KW_REQUIRED))
    );

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the protocol asks the upper level: a message, a (UN)SUBSCRIBE
 ***************************************************************************/
PRIVATE int ac_on_iev_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_event_t iev_event = (gobj_event_t)(uintptr_t)kw_get_int(
        gobj, kw, "__iev_event__", 0, KW_REQUIRED
    );
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    int ret = 0;

    if(iev_event == EV_MQTT_MESSAGE) {
        json_array_append_new(priv->got, json_sprintf("MSG %s q%d %.*s",
            kw_get_str(gobj, kw, "topic", "", KW_REQUIRED),
            (int)kw_get_int(gobj, kw, "qos", 0, KW_REQUIRED),
            gbuf? (int)gbuffer_leftbytes(gbuf) : 0,
            gbuf? (const char *)gbuffer_cur_rd_pointer(gbuf) : ""
        ));
        ret = 1;    // taken: a PUBACK of MQTT 5 says Success

    } else if(iev_event == EV_MQTT_SUBSCRIBE || iev_event == EV_MQTT_UNSUBSCRIBE) {
        BOOL sub = (iev_event == EV_MQTT_SUBSCRIBE);
        json_t *subs = kw_get_list(gobj, kw, "subs", 0, KW_REQUIRED);
        int idx; json_t *jn_sub;
        json_array_foreach(subs, idx, jn_sub) {
            int qos = (int)kw_get_int(gobj, jn_sub, "qos", 0, 0);
            if(sub) {
                json_array_append_new(priv->got, json_sprintf("SUB %s q%d",
                    kw_get_str(gobj, jn_sub, "sub", "", KW_REQUIRED), qos
                ));
            } else {
                json_array_append_new(priv->got, json_sprintf("UNSUB %s",
                    kw_get_str(gobj, jn_sub, "sub", "", KW_REQUIRED)
                ));
            }
            gbuffer_append_char(gbuf, (uint8_t)(sub? qos : 0));
        }
    }

    KW_DECREF(kw)
    return ret;
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
GOBJ_DEFINE_GCLASS(C_GATE_PROT);

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
        {EV_RX_DATA,            ac_rx_data,         0},
        {EV_ON_OPEN,            ac_on_open,         0},
        {EV_ON_CLOSE,           ac_on_close,        0},
        {EV_ON_IEV_MESSAGE,     ac_on_iev_message,  0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_RX_DATA,            0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
        {EV_ON_IEV_MESSAGE,     0},
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
PUBLIC int register_c_gate_prot(void)
{
    return create_gclass(C_GATE_PROT);
}
