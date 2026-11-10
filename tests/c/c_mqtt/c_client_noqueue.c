/****************************************************************************
 *          C_CLIENT_NOQUEUE.C
 *
 *          Driver of the test of C_PROT_MQTT2 as a CLIENT, first with no
 *          queues (tranger_queues NULL: QoS 0 only, as its attribute says),
 *          then with them. The protocol gobj is a child of this driver, its
 *          bottom is a C_FAKE_TRANSPORT, and this driver plays the broker,
 *          writing its MQTT 5 packets by hand. max_packet_size is 1000.
 *
 *          Without queues:
 *              1. The client CONNECTs, announcing Maximum Packet Size 1000
 *                 (up to 7.30.26 a client did not use max_packet_size), the
 *                 "broker" accepts, and the user publishes a QoS 1 message
 *                 as soon as it hears EV_ON_OPEN: refused (-1, ERROR "qos
 *                 not supported"). Up to 7.30.26 max_qos went to 0 only
 *                 AFTER EV_ON_OPEN, and the message was appended to the NULL
 *                 queue: the yuno crashed.
 *              2. A QoS 0 message of the user goes out.
 *              3. The "broker" sends a QoS 1 PUBLISH: delivered, PUBACK.
 *              4. The user subscribes in QoS 2: the SUBSCRIBE asks QoS 0,
 *                 so a broker sends nothing it cannot keep. Up to 7.30.26
 *                 it asked QoS 2.
 *              5. The "broker" sends a QoS 2 PUBLISH all the same: a client
 *                 without queues cannot keep it until its PUBREL, so it is
 *                 disconnected (DISCONNECT 0x93) with a WARNING, and the
 *                 message is not delivered. Up to 7.30.26 its payload was
 *                 released twice (heap corruption), and nothing was said.
 *          With queues (a C_TRANGER of the driver):
 *              6. The user publishes a QoS 1 message as soon as it hears
 *                 EV_ON_OPEN: it goes out. Up to 7.30.26 the queues opened
 *                 after EV_ON_OPEN, and the message went to the NULL queue
 *                 (SIGSEGV).
 *              7. The "broker" announces a PUBLISH of 300000 bytes: refused
 *                 at its header (WARNING, DISCONNECT 0x82), before a byte
 *                 of it is kept. Up to 7.30.26 a client kept whatever its
 *                 broker announced, up to 256 MB.
 *          A hostile "broker", each step a connection of its own:
 *              8. With queues, a second CONNACK in session: a protocol
 *                 error (WARNING, DISCONNECT 0x82). Up to 7.30.26 the
 *                 client opened its queues again (the first handles
 *                 leaked) and told its user EV_ON_OPEN again.
 *              9. A DISCONNECT with properties and a PUBLISH behind it in
 *                 the same read: the properties are freed, and nothing
 *                 after the DISCONNECT is read. Up to 7.30.26 the
 *                 properties leaked and the PUBLISH was delivered.
 *             10. A PUBLISH of QoS 3: malformed (WARNING, DISCONNECT
 *                 0x81). Up to 7.30.26 it was dropped without a word.
 *             11. A PUBLISH whose topic has a wildcard (`#` here, `+` in
 *                 16): malformed (WARNING, DISCONNECT 0x81). Up to 7.30.26
 *                 it was delivered.
 *             12. A CONNACK of MQTT 5 that refuses (0x87): the user hears
 *                 EV_ON_CLOSE, one WARNING, no DISCONNECT. Up to 7.30.26
 *                 it was taken for a protocol error: DISCONNECT 0x82, a
 *                 dump, and no EV_ON_CLOSE.
 *             13. Without queues, a second CONNACK in session: as 8. Up to
 *                 7.30.26 the user heard EV_ON_OPEN again.
 *             14. A PUBLISH of QoS 0 with DUP set: malformed (WARNING,
 *                 DISCONNECT 0x81), as the server refuses it. Up to 7.30.26
 *                 the client delivered it.
 *             15. A PUBLISH whose topic has 250 levels: delivered. MQTT puts
 *                 no limit on the levels of a topic; the client refused more
 *                 than 200 separators (DISCONNECT 0x81), the limit of the
 *                 server, so a broker's valid topic ended the connection.
 *                 15b. Its user publishes to that topic (EV_MQTT_PUBLISH and
 *                 EV_SEND_MESSAGE): it goes out; to a wildcard: -1 and an
 *                 ERROR. The client refused more than 200 separators
 *                 there too, so it could not publish a deep topic to a
 *                 broker that takes it.
 *             16. A PUBLISH whose topic has the wildcard `+`: as 11.
 *          MQTT 3.1.1:
 *             17. A CONNACK with a code past 5 (reserved in 3.1.1): a
 *                 refusal, as 1 to 5 and as the MQTT 5 ones: the user hears
 *                 EV_ON_CLOSE with the code, one WARNING, nothing sent. Up
 *                 to 7.30.26 it was a protocol error: the user heard
 *                 nothing.
 *          MQTT 5:
 *             18. A CONNACK with the code 0x06, and 19. with 0x7F: MQTT 5
 *                 defines no CONNACK code below 0x80 but Success, so it is
 *                 a protocol error (one WARNING that says so, with a text
 *                 reason, no dump, DISCONNECT 0x82), and the user hears
 *                 EV_ON_CLOSE with the code, as for a refusal. Up to
 *                 7.30.26 the user heard nothing; then the WARNING said
 *                 "Mqtt connection refused" with a number for reason, and
 *                 the packet was dumped as malformed.
 *             20. A CONNACK in the form of MQTT 3.1.1 (no properties) with
 *                 the code 1, and 21. with the code 4: a broker of 3.1.1
 *                 that refuses. The user hears EV_ON_CLOSE with the MQTT 5
 *                 code of the same meaning (0x84, 0x86), one WARNING,
 *                 nothing sent. Up to 7.30.26 the code 1 was told with a
 *                 DISCONNECT 0x82 and a dump, and 2 to 5 were a protocol
 *                 error the user never heard of.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_fake_transport.h"
#include "c_client_noqueue.h"
#include "test_work_dir.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CLIENT_ID           "noqueue_cl"

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
    hgobj gobj_tranger;
    hgobj prot;
    hgobj fake;
    int phase;
    gbuffer_t *rx;          // what the client wrote, not parsed yet
    json_t *got;            // what the client wrote and what its user got, in order
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

    char queues_dir[PATH_MAX];
    build_path(queues_dir, sizeof(queues_dir), test_work_dir(), "qmsgs", NULL);
    priv->gobj_tranger = gobj_create_service(
        "tranger_queues",
        C_TRANGER,
        json_pack("{s:s, s:b}",
            "path", queues_dir,
            "master", 1
        ),
        gobj
    );

    priv->prot = gobj_create(
        "mqtt_client",
        C_PROT_MQTT2,
        json_pack("{s:b, s:s, s:i, s:i}",
            "iamServer", 0,
            "mqtt_client_id", CLIENT_ID,
            "timeout_close", 2000,
            "max_packet_size", 1000
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
    gobj_start(priv->gobj_tranger);
    gobj_start(priv->prot);     // it starts its bottom

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
    if(gobj_is_running(priv->gobj_tranger)) {
        gobj_stop(priv->gobj_tranger);
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
 *  Write the broker's bytes into the client
 ***************************************************************************/
PRIVATE void broker_sends(hgobj gobj, const uint8_t *bf, size_t len)
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
 *  CONNACK of the broker, MQTT 5, no session present, no properties
 ***************************************************************************/
PRIVATE void broker_connack(hgobj gobj, uint8_t reason)
{
    const uint8_t connack[] = {
        0x20, 0x03,     // CONNACK
        0x00,           // no session present
        reason,
        0x00            // properties length
    };
    broker_sends(gobj, connack, sizeof(connack));
}

/***************************************************************************
 *  PUBLISH of the broker, MQTT 5, no properties
 ***************************************************************************/
PRIVATE void broker_publish(hgobj gobj, int qos, uint16_t mid, const char *topic, const char *payload)
{
    uint8_t bf[1024];
    size_t n = 0;
    size_t tlen = strlen(topic);
    size_t plen = strlen(payload);
    size_t remaining = 2 + tlen + (qos? 2 : 0) + 1 + plen;     // < 16384 here

    bf[n++] = (uint8_t)(0x30 | (qos << 1));
    if(remaining < 128) {
        bf[n++] = (uint8_t)remaining;
    } else {
        bf[n++] = (uint8_t)((remaining % 128) | 0x80);
        bf[n++] = (uint8_t)(remaining / 128);
    }
    bf[n++] = (uint8_t)(tlen >> 8);
    bf[n++] = (uint8_t)(tlen & 0xFF);
    memcpy(bf + n, topic, tlen);
    n += tlen;
    if(qos) {
        bf[n++] = (uint8_t)(mid >> 8);
        bf[n++] = (uint8_t)(mid & 0xFF);
    }
    bf[n++] = 0x00;     // properties length
    memcpy(bf + n, payload, plen);
    n += plen;
    broker_sends(gobj, bf, n);
}

/***************************************************************************
 *  A message of the user, and what the protocol answered
 ***************************************************************************/
PRIVATE void user_publishes(hgobj gobj, int qos, const char *payload)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(16, 16);
    gbuffer_append_string(gbuf, payload);
    int ret = gobj_send_event(
        priv->prot,
        EV_MQTT_PUBLISH,
        json_pack("{s:s, s:i, s:I}",
            "topic", "t/c",
            "qos", qos,
            "gbuffer", (json_int_t)(uintptr_t)gbuf  // the kw owns it
        ),
        gobj
    );
    json_array_append_new(priv->got, json_sprintf("USER PUBLISH q%d %d", qos, ret));
}

/***************************************************************************
 *  A QoS 0 message of the user to `topic`, by `event` (EV_MQTT_PUBLISH or
 *  EV_SEND_MESSAGE), and what the protocol answered. A deep topic is
 *  said as "l/l/.."
 ***************************************************************************/
PRIVATE void user_publishes_to(hgobj gobj, gobj_event_t event, const char *topic, const char *payload)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(16, 16);
    gbuffer_append_string(gbuf, payload);
    int ret = gobj_send_event(
        priv->prot,
        event,
        json_pack("{s:s, s:i, s:b, s:I}",
            "topic", topic,
            "qos", 0,
            "retain", 0,
            "gbuffer", (json_int_t)(uintptr_t)gbuf  // the kw owns it
        ),
        gobj
    );
    json_array_append_new(priv->got, json_sprintf("USER PUBLISH %s %d",
        strlen(topic) > 16? "l/l/.." : topic, ret
    ));
}

/***************************************************************************
 *  What the client wrote, as lines in `got`
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

        switch(type) {
            case 1:     // CONNECT, MQTT 5: its Maximum Packet Size (0x27), if any
                {
                    int l = snprintf(line, sizeof(line), "CONNECT");
                    uint32_t i = 2 + 4 + 1 + 1 + 2;     // name, level, flags, keep alive
                    uint32_t plen = (i < remaining)? v[i] : 0;   // < 128 here
                    uint32_t end = i + 1 + plen;
                    i++;
                    while(i < end && i < remaining) {
                        uint8_t id = v[i++];
                        if(id == 0x27 && i + 4 <= end) {
                            snprintf(line + l, sizeof(line) - l, " maxpkt=%u",
                                (unsigned)((v[i] << 24) | (v[i+1] << 16) | (v[i+2] << 8) | v[i+3]));
                            break;
                        } else if(id == 0x11 || id == 0x27) {
                            i += 4;
                        } else if(id == 0x21 || id == 0x22) {
                            i += 2;
                        } else {
                            break;      // not a property this test reads
                        }
                    }
                }
                break;
            case 8:     // SUBSCRIBE, MQTT 5, one filter
                {
                    uint32_t i = 2;
                    i += 1 + v[i];      // properties
                    int tlen = (v[i] << 8) | v[i+1];
                    snprintf(line, sizeof(line), "SUBSCRIBE %.*s q%d",
                        tlen, (const char *)v + i + 2, v[i + 2 + tlen] & 0x03
                    );
                }
                break;
            case 3:     // PUBLISH
                {
                    int qos = (p[0] >> 1) & 0x03;
                    int tlen = (v[0] << 8) | v[1];
                    uint32_t i = 2 + tlen + (qos? 2 : 0);
                    i += 1 + v[i];
                    if(tlen > 64) {
                        int levels = 1;
                        for(int k=0; k<tlen; k++) {
                            if(v[2 + k] == '/') {
                                levels++;
                            }
                        }
                        snprintf(line, sizeof(line), "PUBLISH (%d levels) q%d %.*s",
                            levels, qos, (int)(remaining - i), (const char *)v + i
                        );
                    } else {
                        snprintf(line, sizeof(line), "PUBLISH %.*s q%d %.*s",
                            tlen, (const char *)v + 2, qos, (int)(remaining - i), (const char *)v + i
                        );
                    }
                }
                break;
            case 4:     // PUBACK
                snprintf(line, sizeof(line), "PUBACK %d", mid);
                break;
            case 6:     // PUBREL
                snprintf(line, sizeof(line), "PUBREL %d", mid);
                break;
            case 5:     // PUBREC
                snprintf(line, sizeof(line), "PUBREC %d", mid);
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
            {
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                parse_rx(gobj);
                static const uint8_t connack[] = {
                    0x20, 0x03,     // CONNACK
                    0x00,           // no session present
                    0x00,           // Success
                    0x00            // properties length
                };
                broker_sends(gobj, connack, sizeof(connack));
                expect(gobj, "1. CONNECT, and a QoS 1 message at EV_ON_OPEN", (const char *[]){
                    "CONNECT maxpkt=1000",
                    "OPEN",
                    "USER PUBLISH q1 -1",
                    NULL
                });

                user_publishes(gobj, 0, "zero");
                expect(gobj, "2. a QoS 0 message", (const char *[]){
                    "PUBLISH t/c q0 zero",
                    "USER PUBLISH q0 0",
                    NULL
                });

                broker_publish(gobj, 1, 8, "t/b", "one");
                expect(gobj, "3. a QoS 1 PUBLISH of the broker", (const char *[]){
                    "MSG t/b q1 one",
                    "PUBACK 8",
                    NULL
                });

                gobj_send_event(
                    priv->prot,
                    EV_MQTT_SUBSCRIBE,
                    json_pack("{s:[s], s:i}", "subs", "t/s", "qos", 2),
                    gobj
                );
                expect(gobj, "4. a SUBSCRIBE in QoS 2", (const char *[]){
                    "SUBSCRIBE t/s q0",
                    NULL
                });

                broker_publish(gobj, 2, 9, "t/b", "two");
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj, "5. a QoS 2 PUBLISH of the broker", (const char *[]){
                    "DISCONNECT 147",
                    "CLOSE",
                    NULL
                });
                set_timeout(priv->timer, 100);
            }
            break;

        case 2:
            {
                gobj_write_pointer_attr(
                    priv->prot,
                    "tranger_queues",
                    gobj_read_pointer_attr(priv->gobj_tranger, "tranger")
                );
                gobj_write_integer_attr(priv->prot, "max_qos", 2);  // the queues allow it again

                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                parse_rx(gobj);
                static const uint8_t connack[] = {
                    0x20, 0x03,     // CONNACK
                    0x00,           // no session present
                    0x00,           // Success
                    0x00            // properties length
                };
                broker_sends(gobj, connack, sizeof(connack));
                expect(gobj, "6. with queues, a QoS 1 message at EV_ON_OPEN", (const char *[]){
                    "CONNECT maxpkt=1000",
                    "OPEN",
                    "PUBLISH t/c q1 early",
                    "USER PUBLISH q1 0",
                    NULL
                });

                static const uint8_t big_header[] = {
                    0x30,               // PUBLISH QoS 0
                    0xE0, 0xA7, 0x12    // remaining length 300000
                };
                broker_sends(gobj, big_header, sizeof(big_header));
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj, "7. a PUBLISH past max_packet_size", (const char *[]){
                    "DISCONNECT 130",
                    "CLOSE",
                    NULL
                });
                set_timeout(priv->timer, 100);
            }
            break;

        case 3:
            {
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                broker_connack(gobj, 0x00);
                expect(gobj, "8a. with queues, in session: the message of 6 again", (const char *[]){
                    "CONNECT maxpkt=1000",
                    "PUBLISH t/c q1 early",
                    "OPEN",
                    NULL
                });
                static const uint8_t puback[] = {
                    0x40, 0x02,     // PUBACK
                    0x00, 0x01      // mid 1, the message of 6
                };
                broker_sends(gobj, puback, sizeof(puback));
                broker_connack(gobj, 0x00);
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj, "8. with queues, a second CONNACK", (const char *[]){
                    "DISCONNECT 130",
                    "CLOSE",
                    NULL
                });
                set_timeout(priv->timer, 100);
            }
            break;

        case 4:
            {
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                broker_connack(gobj, 0x00);
                expect(gobj, "9a. in session", (const char *[]){
                    "CONNECT maxpkt=1000",
                    "OPEN",
                    NULL
                });
                static const uint8_t disconnect_and_publish[] = {
                    0xE0, 0x08,             // DISCONNECT
                    0x8B,                   // Server shutting down
                    0x06,                   // properties length
                    0x1F, 0x00, 0x03,       // Reason String
                    'b', 'y', 'e',
                    0x30, 0x0A,             // PUBLISH QoS 0, behind it
                    0x00, 0x03, 't', '/', 'x',
                    0x00,                   // properties length
                    'a', 'f', 't', 'r'
                };
                broker_sends(gobj, disconnect_and_publish, sizeof(disconnect_and_publish));
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj, "9. a DISCONNECT with properties, a PUBLISH behind it", (const char *[]){
                    "CLOSE",
                    NULL
                });
                set_timeout(priv->timer, 100);
            }
            break;

        case 5:
            {
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                broker_connack(gobj, 0x00);
                expect(gobj, "10a. in session", (const char *[]){
                    "CONNECT maxpkt=1000",
                    "OPEN",
                    NULL
                });
                broker_publish(gobj, 3, 11, "t/b", "three");
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj, "10. a PUBLISH of QoS 3", (const char *[]){
                    "DISCONNECT 129",
                    "CLOSE",
                    NULL
                });
                set_timeout(priv->timer, 100);
            }
            break;

        case 6:
            {
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                broker_connack(gobj, 0x00);
                expect(gobj, "11a. in session", (const char *[]){
                    "CONNECT maxpkt=1000",
                    "OPEN",
                    NULL
                });
                broker_publish(gobj, 0, 0, "a/#", "wild");
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj, "11. a PUBLISH whose topic has the wildcard #", (const char *[]){
                    "DISCONNECT 129",
                    "CLOSE",
                    NULL
                });
                set_timeout(priv->timer, 100);
            }
            break;

        case 7:
            {
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                broker_connack(gobj, 0x87);     // Not authorized
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj, "12. a CONNACK of MQTT 5 that refuses", (const char *[]){
                    "CONNECT maxpkt=1000",
                    "CLOSE 135",
                    NULL
                });
                set_timeout(priv->timer, 100);
            }
            break;

        case 8:
            {
                gobj_write_pointer_attr(priv->prot, "tranger_queues", 0);

                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                broker_connack(gobj, 0x00);
                expect(gobj, "13a. without queues, in session", (const char *[]){
                    "CONNECT maxpkt=1000",
                    "OPEN",
                    NULL
                });
                broker_connack(gobj, 0x00);
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj, "13. without queues, a second CONNACK", (const char *[]){
                    "DISCONNECT 130",
                    "CLOSE",
                    NULL
                });
                set_timeout(priv->timer, 100);
            }
            break;

        case 9:
            {
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                broker_connack(gobj, 0x00);
                expect(gobj, "14a. in session", (const char *[]){
                    "CONNECT maxpkt=1000",
                    "OPEN",
                    NULL
                });
                static const uint8_t dup_qos0[] = {
                    0x38, 0x09,             // PUBLISH, DUP 1, QoS 0
                    0x00, 0x03, 't', '/', 'd',
                    0x00,                   // properties length
                    'd', 'u', 'p'
                };
                broker_sends(gobj, dup_qos0, sizeof(dup_qos0));
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj, "14. a PUBLISH of QoS 0 with DUP", (const char *[]){
                    "DISCONNECT 129",
                    "CLOSE",
                    NULL
                });
                set_timeout(priv->timer, 100);
            }
            break;

        case 10:
            {
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                broker_connack(gobj, 0x00);
                expect(gobj, "15a. in session", (const char *[]){
                    "CONNECT maxpkt=1000",
                    "OPEN",
                    NULL
                });
                char topic[256*2];
                size_t n = 0;
                for(int level=0; level<250; level++) {
                    if(level > 0) {
                        topic[n++] = '/';
                    }
                    topic[n++] = 'l';
                }
                topic[n] = 0;
                char line[sizeof(topic) + 32];
                snprintf(line, sizeof(line), "MSG %s q0 deep", topic);
                broker_publish(gobj, 0, 0, topic, "deep");
                expect(gobj, "15. a PUBLISH whose topic has 250 levels", (const char *[]){
                    line,
                    NULL
                });

                user_publishes_to(gobj, EV_MQTT_PUBLISH, topic, "up1");
                user_publishes_to(gobj, EV_SEND_MESSAGE, topic, "up2");
                user_publishes_to(gobj, EV_MQTT_PUBLISH, "a/#", "wild1");
                user_publishes_to(gobj, EV_SEND_MESSAGE, "a/+", "wild2");
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj, "15b. the user publishes to a topic of 250 levels, and to wildcards", (const char *[]){
                    "PUBLISH (250 levels) q0 up1",
                    "USER PUBLISH l/l/.. 0",
                    "PUBLISH (250 levels) q0 up2",
                    "USER PUBLISH l/l/.. 0",
                    "USER PUBLISH a/# -1",
                    "USER PUBLISH a/+ -1",
                    "CLOSE",
                    NULL
                });
                set_timeout(priv->timer, 100);
            }
            break;

        case 11:
            {
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                broker_connack(gobj, 0x00);
                expect(gobj, "16a. in session", (const char *[]){
                    "CONNECT maxpkt=1000",
                    "OPEN",
                    NULL
                });
                broker_publish(gobj, 0, 0, "a/+/b", "wild");
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj, "16. a PUBLISH whose topic has the wildcard +", (const char *[]){
                    "DISCONNECT 129",
                    "CLOSE",
                    NULL
                });
                set_timeout(priv->timer, 100);
            }
            break;

        case 12:
            {
                gobj_write_str_attr(priv->prot, "mqtt_protocol", "mqttv311");
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                static const uint8_t connack311[] = {
                    0x20, 0x02,     // CONNACK, MQTT 3.1.1
                    0x00,           // no session present
                    0x06            // a code past 5: reserved
                };
                broker_sends(gobj, connack311, sizeof(connack311));
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj, "17. a CONNACK of MQTT 3.1.1 with a code past 5", (const char *[]){
                    "CONNECT",
                    "CLOSE 6",
                    NULL
                });
                set_timeout(priv->timer, 100);
            }
            break;

        case 13:
        case 14:
            {
                uint8_t code = (priv->phase == 13)? 0x06 : 0x7F;
                gobj_write_str_attr(priv->prot, "mqtt_protocol", "mqttv5");
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                broker_connack(gobj, code);
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                char close_line[32];
                snprintf(close_line, sizeof(close_line), "CLOSE %d", (int)code);
                expect(gobj,
                    (priv->phase == 13)?
                        "18. a CONNACK of MQTT 5 with the code 0x06" :
                        "19. a CONNACK of MQTT 5 with the code 0x7F",
                    (const char *[]){
                        "CONNECT maxpkt=1000",
                        close_line,
                        "DISCONNECT 130",
                        NULL
                    }
                );
                set_timeout(priv->timer, 100);
            }
            break;

        case 15:
        case 16:
            {
                uint8_t code = (priv->phase == 15)? 0x01 : 0x04;
                gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);
                const uint8_t connack311[] = {
                    0x20, 0x02,     // CONNACK in the form of MQTT 3.1.1: no properties
                    0x00,           // no session present
                    code
                };
                broker_sends(gobj, connack311, sizeof(connack311));
                gobj_send_event(priv->prot, EV_DISCONNECTED, 0, priv->fake);   // the broker closes
                expect(gobj,
                    (priv->phase == 15)?
                        "20. an MQTT 5 client, a CONNACK of MQTT 3.1.1 with the code 1" :
                        "21. an MQTT 5 client, a CONNACK of MQTT 3.1.1 with the code 4",
                    (const char *[]){
                        "CONNECT maxpkt=1000",
                        (priv->phase == 15)? "CLOSE 132" : "CLOSE 134",
                        NULL
                    }
                );
                set_timeout(priv->timer, 100);
            }
            break;

        default:
            expect(gobj, "the end", (const char *[]){
                NULL
            });
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: client without queues done",
                NULL
            );
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the client writes, through the fake transport
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
 *  The client is in session: its user publishes at once
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_array_append_new(priv->got, json_string("OPEN"));
    if(priv->phase <= 2) {
        user_publishes(gobj, 1, "early");
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

    json_int_t reason_code = kw_get_int(gobj, kw, "reason_code", 0, 0);
    if(reason_code) {
        json_array_append_new(priv->got, json_sprintf("CLOSE %d", (int)reason_code));    // a refused CONNACK
    } else {
        json_array_append_new(priv->got, json_string("CLOSE"));
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the client tells its user: a message, an ack of a publish
 ***************************************************************************/
PRIVATE int ac_on_iev_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_event_t iev_event = (gobj_event_t)(uintptr_t)kw_get_int(
        gobj, kw, "__iev_event__", 0, KW_REQUIRED
    );

    if(iev_event == EV_MQTT_MESSAGE) {
        gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
        json_array_append_new(priv->got, json_sprintf("MSG %s q%d %.*s",
            kw_get_str(gobj, kw, "topic", "", KW_REQUIRED),
            (int)kw_get_int(gobj, kw, "qos", 0, KW_REQUIRED),
            gbuf? (int)gbuffer_leftbytes(gbuf) : 0,
            gbuf? (const char *)gbuffer_cur_rd_pointer(gbuf) : ""
        ));
    }

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
GOBJ_DEFINE_GCLASS(C_CLIENT_NOQUEUE);

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
PUBLIC int register_c_client_noqueue(void)
{
    return create_gclass(C_CLIENT_NOQUEUE);
}
