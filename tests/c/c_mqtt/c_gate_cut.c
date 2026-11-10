/****************************************************************************
 *          C_GATE_CUT.C
 *
 *          Driver of the test of the cut of a gate's MQTT 3.1.1 connection,
 *          over a real socket. This driver is the upper level of a server
 *          C_PROT_MQTT2 (no broker, no queues, allow_anonymous), as the
 *          decoder of a gate is, and its C_TCP_S accepts on that protocol.
 *          It cuts a connection, as a gate does with what is not a device's,
 *          by posting EV_DROP to the protocol: on the PUBLISH whose payload
 *          is "cut", and on the SUBSCRIBE of the filter "cut/#". After the
 *          cut it takes nothing (every filter refused, no message taken).
 *          The client is RAW: a C_TCP of this driver that writes MQTT 3.1.1
 *          packets by hand, each case in ONE write after its CONNACK.
 *
 *              1. Three QoS 1 PUBLISH and the marker: the client gets the
 *                 four PUBACK, then the close.
 *              2. SUBSCRIBE of a/b and SUBSCRIBE of cut/#: the client gets
 *                 SUBACK 0x00 and SUBACK 0x80, then the close.
 *              3. Three QoS 1 PUBLISH and a DISCONNECT with its flags not 0
 *                 (a protocol error): the three PUBACK, then the close.
 *              4. Three QoS 1 PUBLISH, the marker and a malformed packet
 *                 (type 0, reserved): the four PUBACK, then the close. The
 *                 malformed packet starts the close, and the EV_DROP the
 *                 gate posted on the marker comes while it waits to write.
 *              5. As 3, in MQTT 5: the three PUBACK, DISCONNECT 0x82, then
 *                 the close.
 *
 *          A server of MQTT 3.1.1 sends no DISCONNECT: the drop went to the
 *          C_TCP at once, which discards what waits behind its write in
 *          flight. Up to 7.30.26 the client got the first answer only
 *          ([PUBACK 1, close]; [SUBACK 0x00, close]): the readings taken
 *          were not acknowledged, and the client would send them again.
 *          And up to 7.30.26 the cases 3 to 5 lost them too ([PUBACK 1,
 *          close]): the bad DISCONNECT dropped the connection at once, and
 *          the EV_DROP that came while the close waited dropped it at once.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_gate_cut.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define GATE_URL            "tcp://127.0.0.1:18115"
#define CLIENT_ID           "cut_cl"
#define MARKER_PAYLOAD      "cut"
#define MARKER_FILTER       "cut/#"

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
    hgobj prot;             // the gate's side: C_PROT_MQTT2 server, its bottom a C_TCP
    hgobj tcp_s;
    hgobj client;           // the raw client
    int phase;
    BOOL mqtt5;             // the client of the case speaks MQTT 5
    BOOL cut;               // the gate cut this connection: it takes nothing more
    gbuffer_t *rx;          // what the client got, not parsed yet
    json_t *got;            // what the client got, in order, and its close
    json_t *taken;          // what the upper level took
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
    priv->taken = json_array();

    priv->prot = gobj_create(
        "gate_input",
        C_PROT_MQTT2,
        json_pack("{s:b, s:b}",
            "iamServer", 1,
            "allow_anonymous", 1
        ),
        gobj
    );
    hgobj tcp = gobj_create("gate_input", C_TCP, 0, priv->prot);
    gobj_set_bottom_gobj(priv->prot, tcp);

    priv->tcp_s = gobj_create(
        "gate_server",
        C_TCP_S,
        json_pack("{s:s, s:{s:{s:s}}}",
            "url", GATE_URL,
            "child_tree_filter",
                "kw",
                    "__gclass_name__", C_PROT_MQTT2
        ),
        gobj
    );

    priv->client = gobj_create(
        "raw_client",
        C_TCP,
        json_pack("{s:s}", "url", GATE_URL),
        gobj
    );
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->rx)
    JSON_DECREF(priv->got)
    JSON_DECREF(priv->taken)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->prot);     // a server waits for the C_TCP_S to give it its connection
    gobj_start(priv->tcp_s);

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
    if(gobj_is_running(priv->client)) {
        gobj_stop(priv->client);
    }
    if(gobj_is_running(priv->tcp_s)) {
        gobj_stop(priv->tcp_s);
    }
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

    set_timeout(priv->timer, 300);      // once the server listens

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
 *  Write the client's bytes to the gate
 ***************************************************************************/
PRIVATE void client_sends(hgobj gobj, const uint8_t *bf, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(len, len);
    gbuffer_append(gbuf, (void *)bf, len);
    gobj_send_event(
        priv->client,
        EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        gobj
    );
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
 *  CONNECT, MQTT 3.1.1 (or MQTT 5, no properties), clean session, no user
 ***************************************************************************/
PRIVATE void client_connect(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    static const uint8_t head311[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04,   // protocol name, level 4 (3.1.1)
        0x02,                                   // flags: clean session
        0x00, 0x3C                              // keep alive 60 s
    };
    static const uint8_t head5[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x05,   // protocol name, level 5
        0x02,                                   // flags: clean start
        0x00, 0x3C,                             // keep alive 60 s
        0x00                                    // no properties
    };
    const uint8_t *head = priv->mqtt5? head5 : head311;
    size_t head_len = priv->mqtt5? sizeof(head5) : sizeof(head311);
    uint8_t bf[64];
    size_t n = 0;

    bf[n++] = 0x10;
    bf[n++] = (uint8_t)(head_len + 2 + strlen(CLIENT_ID));
    memcpy(bf + n, head, head_len);
    n += head_len;
    n += put_string(bf + n, CLIENT_ID);
    client_sends(gobj, bf, n);
}

/***************************************************************************
 *  A QoS 1 PUBLISH, appended to bf
 ***************************************************************************/
PRIVATE size_t put_publish_qos1(
    uint8_t *bf,
    BOOL mqtt5,
    uint16_t mid,
    const char *topic,
    const char *payload
) {
    size_t n = 0;

    bf[n++] = 0x32;     // PUBLISH QoS 1
    bf[n++] = (uint8_t)(2 + strlen(topic) + 2 + (mqtt5? 1:0) + strlen(payload));
    n += put_string(bf + n, topic);
    bf[n++] = (uint8_t)(mid >> 8);
    bf[n++] = (uint8_t)(mid & 0xFF);
    if(mqtt5) {
        bf[n++] = 0x00; // no properties
    }
    memcpy(bf + n, payload, strlen(payload));
    n += strlen(payload);
    return n;
}

/***************************************************************************
 *  A SUBSCRIBE of one filter (QoS 0), appended to bf
 ***************************************************************************/
PRIVATE size_t put_subscribe(uint8_t *bf, uint16_t mid, const char *filter)
{
    size_t n = 0;

    bf[n++] = 0x82;     // SUBSCRIBE
    bf[n++] = (uint8_t)(2 + 2 + strlen(filter) + 1);
    bf[n++] = (uint8_t)(mid >> 8);
    bf[n++] = (uint8_t)(mid & 0xFF);
    n += put_string(bf + n, filter);
    bf[n++] = 0x00;
    return n;
}

/***************************************************************************
 *  What the gate wrote, as lines in `got`
 ***************************************************************************/
PRIVATE void parse_rx(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    while(gbuffer_leftbytes(priv->rx) >= 2) {
        const uint8_t *p = gbuffer_cur_rd_pointer(priv->rx);
        size_t avail = gbuffer_leftbytes(priv->rx);
        size_t remaining = p[1];    // < 128 here
        if(2 + remaining > avail) {
            return; // incomplete
        }
        uint8_t type = p[0] >> 4;
        const uint8_t *v = p + 2;
        int mid = (remaining >= 2)? ((v[0] << 8) | v[1]) : 0;
        char line[128];

        switch(type) {
            case 2:     // CONNACK
                snprintf(line, sizeof(line), "CONNACK %d", remaining >= 2? v[1] : -1);
                break;
            case 4:     // PUBACK
                snprintf(line, sizeof(line), "PUBACK %d", mid);
                break;
            case 14:    // DISCONNECT
                snprintf(line, sizeof(line), "DISCONNECT %d", remaining >= 1? v[0] : 0);
                break;
            case 9:     // SUBACK
                {
                    int l = snprintf(line, sizeof(line), "SUBACK %d", mid);
                    for(size_t i = 2; i < remaining && l < (int)sizeof(line) - 8; i++) {
                        l += snprintf(line + l, sizeof(line) - l, " %d", v[i]);
                    }
                }
                break;
            default:
                snprintf(line, sizeof(line), "TYPE %d", type);
                break;
        }
        json_array_append_new(priv->got, json_string(line));
        gbuffer_get(priv->rx, 2 + remaining);

        if(type == 2) {
            /*
             *  In session: the packets of the case, in ONE write
             */
            uint8_t bf[256];
            size_t n = 0;
            if(priv->phase == 2) {
                n += put_subscribe(bf + n, 5, "a/b");
                n += put_subscribe(bf + n, 6, MARKER_FILTER);
            } else {
                n += put_publish_qos1(bf + n, priv->mqtt5, 1, "tele/x/SENSOR", "r1");
                n += put_publish_qos1(bf + n, priv->mqtt5, 2, "tele/x/SENSOR", "r2");
                n += put_publish_qos1(bf + n, priv->mqtt5, 3, "tele/x/SENSOR", "r3");
                if(priv->phase == 1 || priv->phase == 4) {
                    n += put_publish_qos1(bf + n, priv->mqtt5, 4, "tele/x/SENSOR", MARKER_PAYLOAD);
                }
                if(priv->phase == 3 || priv->phase == 5) {
                    bf[n++] = 0xE1;     // DISCONNECT, flags 1: a protocol error
                    bf[n++] = 0x00;
                }
                if(priv->phase == 4) {
                    bf[n++] = 0x00;     // packet type 0: reserved
                    bf[n++] = 0x00;
                }
            }
            client_sends(gobj, bf, n);
        }
    }
}

/***************************************************************************
 *  Compare a list with the lines expected, and forget it
 ***************************************************************************/
PRIVATE void expect(hgobj gobj, const char *step, json_t *got, const char **lines)
{
    json_t *expected = json_array();
    for(int i=0; lines[i]; i++) {
        json_array_append_new(expected, json_string(lines[i]));
    }
    if(!json_equal(got, expected)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: not what was expected",
            "step",         "%s", step,
            "got",          "%j", got,
            "expected",     "%j", expected,
            NULL
        );
    }
    JSON_DECREF(expected)
    json_array_clear(got);
}

/***************************************************************************
 *  The gate cuts the connection, as a gate's decoder does
 ***************************************************************************/
PRIVATE void cut_connection(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->cut = TRUE;
    gobj_post_event(priv->prot, EV_DROP, 0, gobj);
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases. Each case waits for the close of its connection; the
 *  timer is the guard that names the case that did not end.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase) {
        case 0:
            priv->phase = 1;
            gobj_start(priv->client);       // CONNECT at its EV_CONNECTED
            set_timeout(priv->timer, 2000);
            break;

        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            {
                BOOL with_marker = (priv->phase == 1 || priv->phase == 2 || priv->phase == 4);
                if((with_marker && !priv->cut) || gobj_is_running(priv->client)) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "TEST: the case did not end in a close",
                        "phase",        "%d", priv->phase,
                        "got",          "%j", priv->got,
                        NULL
                    );
                    set_yuno_must_die();
                    break;
                }
            }
            switch(priv->phase) {
                case 1:
                    expect(gobj, "1. three QoS 1 PUBLISH and the marker: what the client got",
                        priv->got, (const char *[]){
                            "CONNACK 0",
                            "PUBACK 1",
                            "PUBACK 2",
                            "PUBACK 3",
                            "PUBACK 4",
                            "CLOSED",
                            NULL
                        }
                    );
                    expect(gobj, "1. three QoS 1 PUBLISH and the marker: what the gate took",
                        priv->taken, (const char *[]){
                            "MSG tele/x/SENSOR r1",
                            "MSG tele/x/SENSOR r2",
                            "MSG tele/x/SENSOR r3",
                            NULL
                        }
                    );
                    break;
                case 2:
                    expect(gobj, "2. two SUBSCRIBE, the second one cut: what the client got",
                        priv->got, (const char *[]){
                            "CONNACK 0",
                            "SUBACK 5 0",
                            "SUBACK 6 128",
                            "CLOSED",
                            NULL
                        }
                    );
                    expect(gobj, "2. two SUBSCRIBE, the second one cut: what the gate took",
                        priv->taken, (const char *[]){
                            "SUB a/b",
                            NULL
                        }
                    );
                    break;
                case 3:
                    expect(gobj, "3. three QoS 1 PUBLISH and a DISCONNECT with flags: what the client got",
                        priv->got, (const char *[]){
                            "CONNACK 0",
                            "PUBACK 1",
                            "PUBACK 2",
                            "PUBACK 3",
                            "CLOSED",
                            NULL
                        }
                    );
                    break;
                case 4:
                    expect(gobj, "4. three QoS 1 PUBLISH, the marker and a malformed packet: what the client got",
                        priv->got, (const char *[]){
                            "CONNACK 0",
                            "PUBACK 1",
                            "PUBACK 2",
                            "PUBACK 3",
                            "PUBACK 4",
                            "CLOSED",
                            NULL
                        }
                    );
                    break;
                case 5:
                    expect(gobj, "5. MQTT 5, three QoS 1 PUBLISH and a DISCONNECT with flags: what the client got",
                        priv->got, (const char *[]){
                            "CONNACK 0",
                            "PUBACK 1",
                            "PUBACK 2",
                            "PUBACK 3",
                            "DISCONNECT 130",
                            "CLOSED",
                            NULL
                        }
                    );
                    break;
            }
            if(priv->phase != 1 && priv->phase != 2) {
                expect(gobj, "3-5. what the gate took",
                    priv->taken, (const char *[]){
                        "MSG tele/x/SENSOR r1",
                        "MSG tele/x/SENSOR r2",
                        "MSG tele/x/SENSOR r3",
                        NULL
                    }
                );
            }
            if(priv->phase < 5) {
                priv->phase++;
                priv->mqtt5 = (priv->phase == 5);
                priv->cut = FALSE;
                gobj_start(priv->client);
                set_timeout(priv->timer, 2000);
                break;
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: gate cut done",
                NULL
            );
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->client) {
        gbuffer_clear(priv->rx);
        client_connect(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the gate wrote to the client
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
 *  The gate closed the client: the case is over. The client is stopped
 *  in the timeout (posted at once), not inside its own publication.
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->client) {
        parse_rx(gobj);
        json_array_append_new(priv->got, json_string("CLOSED"));
        gobj_post_event(gobj, EV_STOP_CLIENT, 0, gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Stop the client before it reconnects, and check the case
 ***************************************************************************/
PRIVATE int ac_stop_client(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->client);
    set_timeout(priv->timer, 100);  // the gate's EV_ON_CLOSE comes meanwhile

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

/***************************************************************************
 *  A CONNECT: no session kept
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the protocol asks the upper level: a message, a (UN)SUBSCRIBE.
 *  The marker cuts the connection; after the cut nothing is taken.
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
        int len = gbuf? (int)gbuffer_leftbytes(gbuf) : 0;
        const char *payload = gbuf? (const char *)gbuffer_cur_rd_pointer(gbuf) : "";
        if(priv->cut) {
            // nothing more is taken
        } else if(len == (int)strlen(MARKER_PAYLOAD) && memcmp(payload, MARKER_PAYLOAD, len) == 0) {
            cut_connection(gobj);
        } else {
            json_array_append_new(priv->taken, json_sprintf("MSG %s %.*s",
                kw_get_str(gobj, kw, "topic", "", KW_REQUIRED), len, payload
            ));
            ret = 1;
        }

    } else if(iev_event == EV_MQTT_SUBSCRIBE || iev_event == EV_MQTT_UNSUBSCRIBE) {
        json_t *subs = kw_get_list(gobj, kw, "subs", 0, KW_REQUIRED);
        int idx; json_t *jn_sub;
        json_array_foreach(subs, idx, jn_sub) {
            const char *filter = kw_get_str(gobj, jn_sub, "sub", "", KW_REQUIRED);
            if(!priv->cut && strcmp(filter, MARKER_FILTER) == 0) {
                cut_connection(gobj);
            }
            if(priv->cut) {
                gbuffer_append_char(gbuf, 0x80);
            } else {
                json_array_append_new(priv->taken, json_sprintf("SUB %s", filter));
                gbuffer_append_char(gbuf, 0x00);
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
GOBJ_DEFINE_GCLASS(C_GATE_CUT);

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
        {EV_CONNECTED,          0},
        {EV_RX_DATA,            0},
        {EV_DISCONNECTED,       0},
        {EV_STOP_CLIENT,        0},
        {EV_TX_READY,           0},
        {EV_STOPPED,            0},
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
PUBLIC int register_c_gate_cut(void)
{
    return create_gclass(C_GATE_CUT);
}
