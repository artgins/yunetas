/****************************************************************************
 *          C_GATE_LIMITS.C
 *
 *          Driver of the test of what a peer can do to C_PROT_MQTT2 as the
 *          input of a gate: no broker, no queues (tranger_queues NULL),
 *          allow_anonymous. As in c_gate_prot.c, this driver is the upper
 *          level of the protocol, its bottom is a C_FAKE_TRANSPORT, and the
 *          client's packets are written by hand.
 *
 *          Config of the protocol: max_subscriptions_per_client 3,
 *          message_size_limit 16, max_packet_size 70000, max_qos 1,
 *          timeout_close 2000 ms.
 *
 *          First connection, MQTT 5:
 *              1. CONNACK announces Maximum QoS 1. Up to 7.30.26 it was
 *                 added with the int16 adder, which refuses that property:
 *                 an ERROR per CONNECT and the property not sent.
 *              2. A QoS 1 PUBLISH with user properties and a payload past
 *                 message_size_limit: PUBACK 0x95. Up to 7.30.26 its
 *                 properties leaked (the memory check at the end).
 *              3. A SUBSCRIBE of 13000 filters (the same one): one reason
 *                 code each. Up to 7.30.26 the upper level wrote its codes
 *                 in a buffer of 12 KB: two ERRORs per code past it, and a
 *                 SUBACK with 0x80 for filters the upper level granted.
 *              4. A filter the upper level refuses (>= 0x80) is not held:
 *                 two more fit under max_subscriptions_per_client.
 *              5. An UNSUBSCRIBE of 20000 filters: one code each in the
 *                 UNSUBACK (up to 7.30.26, 12287 of them and the ERRORs).
 *              6. An upper level that misses a code of an UNSUBSCRIBE: an
 *                 ERROR, and 0x80 for it. Up to 7.30.26 the UNSUBACK went
 *                 short of codes (malformed) and nothing was said.
 *              7. A message to a topic with a wildcard is refused (-1 and
 *                 an ERROR); up to 7.30.26 it was published.
 *              8. Acks of packet ids never used: said ONCE per connection.
 *              9. The take-over: EV_DROP with session_take_over gives
 *                 DISCONNECT 0x8E. While it waits timeout_close, a message
 *                 is refused (-1, one WARNING for the connection) and
 *                 another EV_DROP drops the transport at once. Up to
 *                 7.30.26 both were "Event NOT DEFINED" ERRORs, with a
 *                 stack.
 *          Second connection, MQTT 5:
 *              10. A PUBLISH of 20000 bytes cut after its 1st, 2nd, 3rd, 4th
 *                  and 5th byte: delivered each time. Up to 7.30.26, cut
 *                  after the 3rd byte, the second length byte was read
 *                  twice and the connection ended as malformed.
 *              11. A DISCONNECT with a reason code and empty properties,
 *                  and a PUBLISH behind it in the same segment: no ERROR,
 *                  and the PUBLISH is not read (nor any later byte). Up to 7.30.26 an ERROR with
 *                  a stack ("kw must be list or dict") and the PUBLISH
 *                  delivered.
 *          Third connection:
 *              12. A CONNECT refused by policy (client id ".") gets CONNACK
 *                  0x85 and is not dumped as a malformed packet (main scans
 *                  the log); up to 7.30.26 it was, and with max_qos 1 it
 *                  also logged the ERROR "Mqtt proplist NULL".
 *          Fourth connection:
 *              13. A CONNECT of MQTT 5 with properties and a will of QoS 2,
 *                  past max_qos: CONNACK 0x9B. Up to 7.30.26 its properties
 *                  leaked (the memory check at the end).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_fake_transport.h"
#include "c_gate_limits.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CLIENT_A            "lim_a"
#define CLIENT_B            "lim_b"
#define N_SUBSCRIBE         13000
#define N_UNSUBSCRIBE       20000
#define BIG_PAYLOAD         20000

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
    priv->rx = gbuffer_create(4*1024, 256*1024);
    priv->got = json_array();

    priv->prot = gobj_create(
        "gate_prot",
        C_PROT_MQTT2,
        json_pack("{s:b, s:b, s:i, s:i, s:i, s:i, s:i}",
            "iamServer", 1,
            "allow_anonymous", 1,
            "max_subscriptions_per_client", 3,
            "message_size_limit", 16,
            "max_packet_size", 70000,
            "max_qos", 1,
            "timeout_close", 2000
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
 *  Fixed header of a packet: type and the remaining length (1 to 4 bytes)
 ***************************************************************************/
PRIVATE size_t put_fixed_header(uint8_t *bf, uint8_t type, size_t remaining)
{
    size_t n = 0;
    bf[n++] = type;
    do {
        uint8_t b = (uint8_t)(remaining % 128);
        remaining /= 128;
        if(remaining > 0) {
            b |= 0x80;
        }
        bf[n++] = b;
    } while(remaining > 0);
    return n;
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
 *  CONNECT, MQTT 5, clean start
 ***************************************************************************/
PRIVATE void peer_connect(hgobj gobj, const char *client_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t bf[256];
    size_t n;
    static const uint8_t head5[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x05,   // protocol name, level 5
        0x02,                                   // flags: clean start
        0x00, 0x3C,                             // keep alive 60 s
        0x00                                    // properties length
    };

    gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);

    n = put_fixed_header(bf, 0x10, sizeof(head5) + 2 + strlen(client_id));
    memcpy(bf + n, head5, sizeof(head5));
    n += sizeof(head5);
    n += put_string(bf + n, client_id);
    peer_sends(gobj, bf, n);
}

/***************************************************************************
 *  PUBLISH, MQTT 5, with `n_user_props` user properties "k<i>"="v<i>",
 *  into `bf`. Return its length.
 ***************************************************************************/
PRIVATE size_t build_publish(
    uint8_t *bf,
    int qos,
    uint16_t mid,
    const char *topic,
    const uint8_t *payload,
    size_t payload_len,
    int n_user_props
) {
    uint8_t props[256];
    size_t lp = 0;
    for(int i=0; i<n_user_props; i++) {
        char k[8], v[8];
        snprintf(k, sizeof(k), "k%d", i);
        snprintf(v, sizeof(v), "v%d", i);
        props[lp++] = 0x26;     // user property
        lp += put_string(props + lp, k);
        lp += put_string(props + lp, v);
    }

    size_t remaining = 2 + strlen(topic) + (qos? 2 : 0) + 1 + lp + payload_len;
    size_t n = put_fixed_header(bf, (uint8_t)(0x30 | (qos << 1)), remaining);
    n += put_string(bf + n, topic);
    if(qos) {
        bf[n++] = (uint8_t)(mid >> 8);
        bf[n++] = (uint8_t)(mid & 0xFF);
    }
    bf[n++] = (uint8_t)lp;      // properties length, < 128 here
    memcpy(bf + n, props, lp);
    n += lp;
    memcpy(bf + n, payload, payload_len);
    n += payload_len;
    return n;
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
 *  SUBSCRIBE (options = QoS 1) or UNSUBSCRIBE, MQTT 5, of the filters
 *  `filters`, or of `repeat` copies of filters[0] when repeat > 0
 ***************************************************************************/
PRIVATE void peer_subscribe(
    hgobj gobj,
    BOOL unsubscribe,
    uint16_t mid,
    const char **filters,
    int repeat
) {
    int n_filters = 0;
    size_t remaining = 2 + 1;
    if(repeat > 0) {
        n_filters = repeat;
        remaining += (size_t)repeat * (2 + strlen(filters[0]) + (unsubscribe? 0 : 1));
    } else {
        for(; filters[n_filters]; n_filters++) {
            remaining += 2 + strlen(filters[n_filters]) + (unsubscribe? 0 : 1);
        }
    }

    uint8_t *bf = gbmem_malloc(remaining + 8);
    size_t n = put_fixed_header(bf, unsubscribe? 0xA2 : 0x82, remaining);
    bf[n++] = (uint8_t)(mid >> 8);
    bf[n++] = (uint8_t)(mid & 0xFF);
    bf[n++] = 0x00;     // properties length
    for(int i=0; i<n_filters; i++) {
        n += put_string(bf + n, repeat > 0? filters[0] : filters[i]);
        if(!unsubscribe) {
            bf[n++] = 0x01;
        }
    }
    peer_sends(gobj, bf, n);
    gbmem_free(bf);
}

/***************************************************************************
 *  The reason codes of a SUBACK / UNSUBACK as a line: each one, or the
 *  count and the code when there are many
 ***************************************************************************/
PRIVATE void codes_line(char *line, size_t size, const char *name, int mid, const uint8_t *codes, uint32_t n)
{
    int l = snprintf(line, size, "%s %d", name, mid);
    if(n > 8) {
        BOOL same = TRUE;
        for(uint32_t i=1; i<n; i++) {
            if(codes[i] != codes[0]) {
                same = FALSE;
                break;
            }
        }
        if(same) {
            snprintf(line + l, size - l, " x%u %d", (unsigned)n, codes[0]);
        } else {
            snprintf(line + l, size - l, " x%u mixed", (unsigned)n);
        }
        return;
    }
    for(uint32_t i=0; i<n && l < (int)size - 8; i++) {
        l += snprintf(line + l, size - l, " %d", codes[i]);
    }
}

/***************************************************************************
 *  The Maximum QoS (0x24) of the properties of a CONNACK, -1 if absent
 ***************************************************************************/
PRIVATE int connack_max_qos(const uint8_t *p, uint32_t len)
{
    uint32_t i = 0;
    while(i < len) {
        uint8_t id = p[i++];
        switch(id) {
            case 0x24:      // maximum qos, byte
                return i < len? p[i] : -1;
            case 0x25:      // retain available, byte
            case 0x28:      // wildcard subscription available, byte
            case 0x29:      // subscription identifiers available, byte
            case 0x2A:      // shared subscription available, byte
                i += 1;
                break;
            case 0x13:      // server keep alive, int16
            case 0x21:      // receive maximum, int16
            case 0x22:      // topic alias maximum, int16
                i += 2;
                break;
            case 0x27:      // maximum packet size, int32
                i += 4;
                break;
            case 0x12:      // assigned client identifier, string
                if(i + 2 > len) {
                    return -1;
                }
                i += 2 + (uint32_t)((p[i] << 8) | p[i+1]);
                break;
            default:
                return -1;  // not a property this test reads
        }
    }
    return -1;
}

/***************************************************************************
 *  What the server wrote, as lines in `got` (every connection is MQTT 5)
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
                {
                    int l = snprintf(line, sizeof(line), "CONNACK %d", remaining >= 2? v[1] : -1);
                    if(remaining >= 3) {
                        int max_qos = connack_max_qos(v + 3, remaining - 3);
                        if(max_qos >= 0) {
                            snprintf(line + l, sizeof(line) - l, " maxqos=%d", max_qos);
                        }
                    }
                }
                break;
            case 3:     // PUBLISH
                {
                    int qos = (p[0] >> 1) & 0x03;
                    int tlen = (v[0] << 8) | v[1];
                    uint32_t i = 2 + tlen + (qos? 2 : 0);
                    i += 1 + v[i];
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
                    uint32_t i = 2 + 1 + v[2];
                    codes_line(line, sizeof(line), type==9? "SUBACK":"UNSUBACK", mid,
                        v + i, remaining > i? remaining - i : 0
                    );
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
 *  A message to the client, and what the protocol answered
 ***************************************************************************/
PRIVATE void send_message(hgobj gobj, const char *topic)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(8, 8);
    gbuffer_append_string(gbuf, "on");
    int ret = gobj_send_event(priv->prot, EV_SEND_MESSAGE,
        json_pack("{s:s, s:I, s:i, s:b}",
            "topic", topic,
            "gbuffer", (json_int_t)(uintptr_t)gbuf,
            "qos", 0,
            "retain", 0
        ),
        gobj
    );
    parse_rx(gobj);
    json_array_append_new(priv->got, json_sprintf("SEND %d", ret));
}

/***************************************************************************
 *  The drops the transport got
 ***************************************************************************/
PRIVATE void say_drops(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    parse_rx(gobj);
    json_array_append_new(priv->got,
        json_sprintf("DROPS %d", (int)gobj_read_integer_attr(priv->fake, "drops"))
    );
}

/***************************************************************************
 *  The first connection
 ***************************************************************************/
PRIVATE void run_connection_a(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t bf[512];

    peer_connect(gobj, CLIENT_A);
    expect(gobj, "1. CONNACK with Maximum QoS", (const char *[]){
        "OPEN " CLIENT_A,
        "CONNACK 0 maxqos=1",
        NULL
    });

    static const uint8_t too_large[] = "0123456789abcdefXYZ";
    size_t n = build_publish(bf, 1, 5, "t/1", too_large, sizeof(too_large) - 1, 3);
    peer_sends(gobj, bf, n);
    expect(gobj, "2. PUBLISH with properties past message_size_limit", (const char *[]){
        "PUBACK 5 149",
        NULL
    });

    peer_subscribe(gobj, FALSE, 1, (const char *[]){"a", NULL}, N_SUBSCRIBE);
    expect(gobj, "3. SUBSCRIBE of 13000 filters", (const char *[]){
        "SUB x13000 a q0",
        "SUBACK 1 x13000 0",
        NULL
    });

    peer_subscribe(gobj, FALSE, 2, (const char *[]){"deny/1", NULL}, 0);
    peer_subscribe(gobj, FALSE, 3, (const char *[]){"b/1", "b/2", "b/3", NULL}, 0);
    expect(gobj, "4. a filter the upper level refuses is not held", (const char *[]){
        "SUB deny/1 q0",
        "SUBACK 2 135",
        "SUB b/1 q0",
        "SUB b/2 q0",
        "SUBACK 3 0 0 151",
        NULL
    });

    peer_subscribe(gobj, TRUE, 4, (const char *[]){"z", NULL}, N_UNSUBSCRIBE);
    expect(gobj, "5. UNSUBSCRIBE of 20000 filters", (const char *[]){
        "UNSUB x20000 z",
        "UNSUBACK 4 x20000 0",
        NULL
    });

    peer_subscribe(gobj, TRUE, 5, (const char *[]){"a", "nocode", NULL}, 0);
    expect(gobj, "6. a reason code the upper level did not write", (const char *[]){
        "UNSUB a",
        "UNSUB nocode",
        "UNSUBACK 5 0 128",
        NULL
    });

    send_message(gobj, "cmnd/x/#");
    send_message(gobj, "cmnd/x");
    expect(gobj, "7. a message to a topic with a wildcard", (const char *[]){
        "SEND -1",
        "PUBLISH cmnd/x q0 on",
        "SEND 0",
        NULL
    });

    peer_ack(gobj, 0x62, 21);
    peer_ack(gobj, 0x40, 30);
    peer_ack(gobj, 0x50, 31);
    peer_ack(gobj, 0x70, 32);
    peer_ack(gobj, 0x40, 33);
    expect(gobj, "8. acks of packet ids never used", (const char *[]){
        "PUBCOMP 21",
        "PUBREL 31",
        NULL
    });

    gobj_send_event(priv->prot, EV_DROP, json_pack("{s:b}", "session_take_over", 1), gobj);
    say_drops(gobj);
    send_message(gobj, "cmnd/x");
    send_message(gobj, "cmnd/x");     // said once
    gobj_send_event(priv->prot, EV_DROP, 0, gobj);
    say_drops(gobj);
    expect(gobj, "9. the take-over, and the closing window", (const char *[]){
        "DISCONNECT 142",
        "DROPS 0",
        "SEND -1",
        "SEND -1",
        "DROPS 1",
        NULL
    });
}

/***************************************************************************
 *  The second connection
 ***************************************************************************/
PRIVATE void run_connection_b(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    expect(gobj, "9. the drop", (const char *[]){
        "CLOSE " CLIENT_A,
        NULL
    });

    gobj_write_integer_attr(priv->prot, "message_size_limit", 0);
    peer_connect(gobj, CLIENT_B);

    uint8_t *payload = gbmem_malloc(BIG_PAYLOAD);
    memset(payload, 'x', BIG_PAYLOAD);
    uint8_t *bf = gbmem_malloc(BIG_PAYLOAD + 64);
    size_t n = build_publish(bf, 0, 0, "t/big", payload, BIG_PAYLOAD, 0);
    for(size_t cut = 1; cut <= 5; cut++) {
        peer_sends(gobj, bf, cut);
        peer_sends(gobj, bf + cut, n - cut);
    }
    gbmem_free(payload);
    gbmem_free(bf);
    expect(gobj, "10. a PUBLISH of 20000 bytes cut in its fixed header", (const char *[]){
        "OPEN " CLIENT_B,
        "CONNACK 0 maxqos=1",
        "MSG t/big q0 len=20000",
        "MSG t/big q0 len=20000",
        "MSG t/big q0 len=20000",
        "MSG t/big q0 len=20000",
        "MSG t/big q0 len=20000",
        NULL
    });

    uint8_t seg[64];
    static const uint8_t disconnect5[] = {0xE0, 0x02, 0x00, 0x00};
    memcpy(seg, disconnect5, sizeof(disconnect5));
    n = sizeof(disconnect5);
    n += build_publish(seg + n, 0, 0, "t/after", (const uint8_t *)"late", 4, 0);
    peer_sends(gobj, seg, n);
    n = build_publish(seg, 0, 0, "t/later", (const uint8_t *)"late", 4, 0);
    peer_sends(gobj, seg, n);   // in a later read
    gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the client closes
    expect(gobj, "11. DISCONNECT, and a PUBLISH behind it", (const char *[]){
        "CLOSE " CLIENT_B,
        NULL
    });
}

/***************************************************************************
 *  The fourth connection: a CONNECT with properties and a will of QoS 2
 ***************************************************************************/
PRIVATE void run_connection_d(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t bf[128];
    size_t n;
    static const uint8_t head5[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x05,   // protocol name, level 5
        0x16,                                   // flags: will QoS 2, will, clean start
        0x00, 0x3C,                             // keep alive 60 s
        0x05, 0x11, 0x00, 0x00, 0x00, 0x0A      // properties: session expiry 10 s
    };

    gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);

    size_t remaining = sizeof(head5) + (2 + 2) + 1 + (2 + 3) + (2 + 1);
    n = put_fixed_header(bf, 0x10, remaining);
    memcpy(bf + n, head5, sizeof(head5));
    n += sizeof(head5);
    n += put_string(bf + n, "w5");
    bf[n++] = 0x00;     // will properties length
    n += put_string(bf + n, "w/t");
    n += put_string(bf + n, "x");
    peer_sends(gobj, bf, n);
    expect(gobj, "13. a will of QoS 2 past max_qos", (const char *[]){
        "CONNACK 155",
        NULL
    });
}

/***************************************************************************
 *  The third connection
 ***************************************************************************/
PRIVATE void run_connection_c(hgobj gobj)
{
    peer_connect(gobj, ".");
    expect(gobj, "12. a CONNECT refused by policy", (const char *[]){
        "CONNACK 133",
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
            run_connection_a(gobj);
            set_timeout(priv->timer, 100);  // the drop comes on the next cycle
            break;

        case 2:
            run_connection_b(gobj);
            set_timeout(priv->timer, 100);
            break;

        case 3:
            run_connection_c(gobj);
            set_timeout(priv->timer, 100);
            break;

        case 4:
            run_connection_d(gobj);
            set_timeout(priv->timer, 100);
            break;

        default:
            expect(gobj, "the end", (const char *[]){
                NULL
            });
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: gate limits done",
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
 *  A CONNECT
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
 *  What the protocol asks the upper level: a message, a (UN)SUBSCRIBE.
 *  A filter "deny/..." is refused (0x87), and a filter "nocode" of an
 *  UNSUBSCRIBE gets no code, as an upper level that breaks the contract.
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
        size_t len = gbuf? gbuffer_leftbytes(gbuf) : 0;
        if(len > 16) {
            json_array_append_new(priv->got, json_sprintf("MSG %s q%d len=%d",
                kw_get_str(gobj, kw, "topic", "", KW_REQUIRED),
                (int)kw_get_int(gobj, kw, "qos", 0, KW_REQUIRED),
                (int)len
            ));
        } else {
            json_array_append_new(priv->got, json_sprintf("MSG %s q%d %.*s",
                kw_get_str(gobj, kw, "topic", "", KW_REQUIRED),
                (int)kw_get_int(gobj, kw, "qos", 0, KW_REQUIRED),
                (int)len,
                gbuf? (const char *)gbuffer_cur_rd_pointer(gbuf) : ""
            ));
        }
        ret = 1;    // taken

    } else if(iev_event == EV_MQTT_SUBSCRIBE || iev_event == EV_MQTT_UNSUBSCRIBE) {
        BOOL sub = (iev_event == EV_MQTT_SUBSCRIBE);
        json_t *subs = kw_get_list(gobj, kw, "subs", 0, KW_REQUIRED);
        size_t n_subs = json_array_size(subs);
        if(n_subs > 8) {
            json_t *jn_first = json_array_get(subs, 0);
            if(sub) {
                json_array_append_new(priv->got, json_sprintf("SUB x%d %s q%d",
                    (int)n_subs,
                    kw_get_str(gobj, jn_first, "sub", "", KW_REQUIRED),
                    (int)kw_get_int(gobj, jn_first, "qos", 0, 0)
                ));
            } else {
                json_array_append_new(priv->got, json_sprintf("UNSUB x%d %s",
                    (int)n_subs,
                    kw_get_str(gobj, jn_first, "sub", "", KW_REQUIRED)
                ));
            }
        }
        int idx; json_t *jn_sub;
        json_array_foreach(subs, idx, jn_sub) {
            const char *filter = kw_get_str(gobj, jn_sub, "sub", "", KW_REQUIRED);
            int qos = (int)kw_get_int(gobj, jn_sub, "qos", 0, 0);
            if(n_subs <= 8) {
                if(sub) {
                    json_array_append_new(priv->got, json_sprintf("SUB %s q%d", filter, qos));
                } else {
                    json_array_append_new(priv->got, json_sprintf("UNSUB %s", filter));
                }
            }
            if(sub) {
                gbuffer_append_char(gbuf, (uint8_t)(strncmp(filter, "deny/", 5)==0? 0x87 : qos));
            } else if(strcmp(filter, "nocode") != 0) {
                gbuffer_append_char(gbuf, 0);
            }
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
GOBJ_DEFINE_GCLASS(C_GATE_LIMITS);

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
PUBLIC int register_c_gate_limits(void)
{
    return create_gclass(C_GATE_LIMITS);
}
