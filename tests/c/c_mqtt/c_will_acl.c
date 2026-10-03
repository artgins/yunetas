/****************************************************************************
 *          C_WILL_ACL.C
 *
 *          Driver of the test of the will under the ACL of the broker.
 *
 *          A will is a publish of the client, made by the broker for it
 *          when the client goes away without a DISCONNECT: the publish ACL
 *          of the client's group applies to it as to any publish.
 *
 *          The broker has enable_acl on, and its treedb (authored here):
 *
 *              g_will    publish_acl=["allowed/#"]
 *              g_open    (no patterns: allow-all)
 *              will_bad, will_ok                 -> g_will
 *              will_sub, will_ret, will_late     -> g_open
 *
 *          The clients are RAW: C_TCPs of this gclass that write the MQTT
 *          3.1.1 packets by hand.
 *
 *              1. will_sub connects and subscribes `forbidden/will` and
 *                 `allowed/will`.
 *              2. will_bad connects (clean session) with a will to
 *                 `forbidden/will`, and its connection is closed with no
 *                 DISCONNECT: the broker must refuse the will -- the
 *                 WARNING "Will message refused by the ACL: not published"
 *                 -- and will_sub must get nothing.
 *              3. will_ok does the same with a will to `allowed/will`:
 *                 will_sub must get it.
 *              4. will_ret publishes a RETAINED message with a payload to
 *                 `allowed/retained` (QoS 1), and goes away; will_late
 *                 subscribes it and must get it, flagged retained, with its
 *                 payload. The broker stores it in its treedb from the kw of
 *                 the publish, shared by every layer that published it: up
 *                 to 918c3d687 the treedb took its gbuffer from under them
 *                 and the payload leaked (the memory check at the end).
 *
 *          Each step waits for the packet it depends on (CONNACK, SUBACK,
 *          PUBACK, the PUBLISH that the subscriber gets), read from the
 *          bytes each client receives. The timer is only a guard: it
 *          fires when a step does not come, and the test fails naming it.
 *
 *          Up to 7.25.22 the will bypassed the ACL: will_sub got the will
 *          to `forbidden/will` too.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include <c_prot_mqtt2.h>
#include "c_will_acl.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BROKER_URL      "tcp://127.0.0.1:18118"
#define TREEDB_SERVICE  "treedb_mqtt_broker"
#define GUARD_MSEC      15000   // a step that does not come in this time fails the test

#define RETAINED_TOPIC  "allowed/retained"
#define RETAINED_DATA   "kept"

/*
 *  The clients, and the index of each one's transport and receive buffer
 */
typedef enum {
    CL_SUB = 0,     // will_sub: subscribes the two will topics
    CL_BAD,         // will_bad: a will to a refused topic
    CL_OK,          // will_ok: a will to an allowed topic
    CL_RET,         // will_ret: publishes a retained message
    CL_LATE,        // will_late: subscribes the retained topic
    CL_MAX
} client_t;

PRIVATE const char *client_ids[CL_MAX] = {
    "will_sub", "will_bad", "will_ok", "will_ret", "will_late"
};

/*
 *  MQTT 3.1.1 packet types
 */
#define PKT_CONNACK     2
#define PKT_PUBLISH     3
#define PKT_PUBACK      4
#define PKT_SUBACK      9

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void send_bytes(hgobj gobj, hgobj gobj_tcp, const uint8_t *bf, size_t len);
PRIVATE void send_connect(hgobj gobj, hgobj gobj_tcp, const char *client_id, const char *will_topic);
PRIVATE void on_packet(hgobj gobj, client_t cl, uint8_t first, const uint8_t *v, uint32_t len);

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
    hgobj tcp[CL_MAX];
    gbuffer_t *rx[CL_MAX];  // what the broker sent to each client, not parsed yet
    BOOL started;           // the model authored, the first client started
    const char *waiting;    // the step the guard names when it fires
    BOOL done;
    int got_forbidden;      // PUBLISH of `forbidden/will` that will_sub got
    int got_allowed;        // PUBLISH of `allowed/will` that will_sub got
    int got_retained;       // retained PUBLISH of RETAINED_TOPIC, with its payload, that will_late got
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
    for(int cl = 0; cl < CL_MAX; cl++) {
        priv->tcp[cl] = gobj_create(client_ids[cl], C_TCP, json_pack("{s:s}", "url", BROKER_URL), gobj);
        priv->rx[cl] = gbuffer_create(1024, 1024);
    }
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int cl = 0; cl < CL_MAX; cl++) {
        GBUFFER_DECREF(priv->rx[cl])
    }
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
    for(int cl = 0; cl < CL_MAX; cl++) {
        if(gobj_is_running(priv->tcp[cl])) {
            gobj_stop(priv->tcp[cl]);
        }
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 500);  // once the broker and its treedb are up

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
 *  CONNECT, MQTT 3.1.1, clean session, with a QoS 0 will when `will_topic`
 ***************************************************************************/
PRIVATE void send_connect(hgobj gobj, hgobj gobj_tcp, const char *client_id, const char *will_topic)
{
    static const char will_payload[] = "gone";
    uint8_t bf[256];
    size_t len_id = strlen(client_id);
    size_t len_topic = will_topic? strlen(will_topic) : 0;
    size_t remaining = 10 + 2 + len_id;
    if(will_topic) {
        remaining += 2 + len_topic + 2 + strlen(will_payload);
    }
    if(remaining > 127) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: CONNECT too long for one byte of length",
            "client_id",    "%s", client_id,
            NULL
        );
        return;
    }

    size_t n = 0;
    bf[n++] = 0x10;                     // CONNECT
    bf[n++] = (uint8_t)remaining;
    bf[n++] = 0x00; bf[n++] = 0x04;
    memcpy(bf + n, "MQTT", 4); n += 4;
    bf[n++] = 0x04;                     // protocol level 3.1.1
    bf[n++] = will_topic? 0x06 : 0x02;  // clean session [+ will flag, will QoS 0, no retain]
    bf[n++] = 0x00; bf[n++] = 0x3C;     // keep alive 60 s
    bf[n++] = (uint8_t)(len_id >> 8); bf[n++] = (uint8_t)(len_id & 0xFF);
    memcpy(bf + n, client_id, len_id); n += len_id;
    if(will_topic) {
        size_t len_payload = strlen(will_payload);
        bf[n++] = (uint8_t)(len_topic >> 8); bf[n++] = (uint8_t)(len_topic & 0xFF);
        memcpy(bf + n, will_topic, len_topic); n += len_topic;
        bf[n++] = (uint8_t)(len_payload >> 8); bf[n++] = (uint8_t)(len_payload & 0xFF);
        memcpy(bf + n, will_payload, len_payload); n += len_payload;
    }
    send_bytes(gobj, gobj_tcp, bf, n);
}

/***************************************************************************
 *  SUBSCRIBE of `topics` (QoS 0), packet id 1
 ***************************************************************************/
PRIVATE void send_subscribe(hgobj gobj, hgobj gobj_tcp, const char **topics, int n_topics)
{
    uint8_t bf[256];
    size_t remaining = 2;
    for(int i = 0; i < n_topics; i++) {
        remaining += 2 + strlen(topics[i]) + 1;
    }
    if(remaining > 127) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: SUBSCRIBE too long for one byte of length",
            NULL
        );
        return;
    }
    size_t n = 0;
    bf[n++] = 0x82;                     // SUBSCRIBE
    bf[n++] = (uint8_t)remaining;
    bf[n++] = 0x00; bf[n++] = 0x01;     // packet id
    for(int i = 0; i < n_topics; i++) {
        size_t len = strlen(topics[i]);
        bf[n++] = (uint8_t)(len >> 8); bf[n++] = (uint8_t)(len & 0xFF);
        memcpy(bf + n, topics[i], len); n += len;
        bf[n++] = 0x00;                 // QoS 0
    }
    send_bytes(gobj, gobj_tcp, bf, n);
}

/***************************************************************************
 *  PUBLISH, QoS 1, RETAIN, packet id 1
 ***************************************************************************/
PRIVATE void send_retained_publish(hgobj gobj, hgobj gobj_tcp)
{
    uint8_t bf[128];
    size_t len_topic = strlen(RETAINED_TOPIC);
    size_t len_data = strlen(RETAINED_DATA);
    size_t n = 0;
    bf[n++] = 0x33;                     // PUBLISH, QoS 1, RETAIN
    bf[n++] = (uint8_t)(2 + len_topic + 2 + len_data);
    bf[n++] = (uint8_t)(len_topic >> 8); bf[n++] = (uint8_t)(len_topic & 0xFF);
    memcpy(bf + n, RETAINED_TOPIC, len_topic); n += len_topic;
    bf[n++] = 0x00; bf[n++] = 0x01;     // packet id
    memcpy(bf + n, RETAINED_DATA, len_data); n += len_data;
    send_bytes(gobj, gobj_tcp, bf, n);
}

/***************************************************************************
 *  Parse the whole packets a client received, each one to on_packet()
 ***************************************************************************/
PRIVATE void parse_rx(hgobj gobj, client_t cl)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *rx = priv->rx[cl];

    while(gbuffer_leftbytes(rx) >= 2) {
        const uint8_t *p = gbuffer_cur_rd_pointer(rx);
        size_t avail = gbuffer_leftbytes(rx);
        uint32_t remaining = 0;
        uint32_t multiplier = 1;
        size_t hdr = 1;
        BOOL whole_length = FALSE;
        while(hdr < avail) {
            uint8_t b = p[hdr++];
            remaining += (b & 0x7F) * multiplier;
            multiplier *= 128;
            if(!(b & 0x80)) {
                whole_length = TRUE;
                break;
            }
        }
        if(!whole_length || hdr + remaining > avail) {
            return; // incomplete: the rest comes with the next EV_RX_DATA
        }
        uint8_t first = p[0];
        uint8_t packet[256];
        uint32_t len = remaining < sizeof(packet)? remaining : (uint32_t)sizeof(packet);
        if(len < remaining) {
            /*
             *  No packet of this test is that long: one that is breaks
             *  what on_packet() reads, and is said
             */
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST: a packet longer than the parse buffer, truncated",
                "remaining",    "%d", (int)remaining,
                NULL
            );
        }
        memcpy(packet, p + hdr, len);
        gbuffer_get(rx, hdr + remaining);
        on_packet(gobj, cl, first, packet, len);
        if(priv->done) {
            return;
        }
    }
}

/***************************************************************************
 *  Start a client, and name the step the guard waits for
 ***************************************************************************/
PRIVATE void start_client(hgobj gobj, client_t cl, const char *waiting)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->waiting = waiting;
    gobj_start(priv->tcp[cl]);
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

/***************************************************************************
 *  The end: the counts, then the yuno goes
 ***************************************************************************/
PRIVATE void finish(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->done = TRUE;
    clear_timeout(priv->timer);

    if(priv->got_forbidden != 0) {
        test_error(gobj,
            "TEST: a will to a topic the ACL refuses was published",
            priv->got_forbidden, 0
        );
    }
    if(priv->got_allowed != 1) {
        test_error(gobj,
            "TEST: a will to a topic the ACL allows was not published once",
            priv->got_allowed, 1
        );
    }
    if(priv->got_retained != 1) {
        test_error(gobj,
            "TEST: the retained message did not reach the late subscriber once",
            priv->got_retained, 1
        );
    }
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "TEST: will under the ACL done",
        "forbidden",    "%d", priv->got_forbidden,
        "allowed",      "%d", priv->got_allowed,
        "retained",     "%d", priv->got_retained,
        NULL
    );
    for(int cl = 0; cl < CL_MAX; cl++) {
        if(gobj_is_running(priv->tcp[cl])) {
            gobj_stop(priv->tcp[cl]);
        }
    }
    set_yuno_must_die();
}

/***************************************************************************
 *  A packet the broker sent to client `cl`: the step it unblocks
 ***************************************************************************/
PRIVATE void on_packet(hgobj gobj, client_t cl, uint8_t first, const uint8_t *v, uint32_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t type = first >> 4;

    switch(cl) {
        case CL_SUB:
            if(type == PKT_CONNACK) {
                const char *topics[] = {"forbidden/will", "allowed/will"};
                priv->waiting = "the SUBACK of will_sub";
                send_subscribe(gobj, priv->tcp[CL_SUB], topics, 2);

            } else if(type == PKT_SUBACK) {
                /*
                 *  2. A will to a topic the ACL refuses
                 */
                start_client(gobj, CL_BAD, "the CONNACK of will_bad");

            } else if(type == PKT_PUBLISH && len >= 2) {
                uint16_t topic_len = (uint16_t)((v[0] << 8) | v[1]);
                if(2 + (uint32_t)topic_len <= len) {
                    char topic[128];
                    snprintf(topic, sizeof(topic), "%.*s", (int)topic_len, (const char *)(v + 2));
                    if(strcmp(topic, "forbidden/will") == 0) {
                        priv->got_forbidden++;
                    } else if(strcmp(topic, "allowed/will") == 0) {
                        priv->got_allowed++;
                        /*
                         *  4. A retained message, read by a late subscriber
                         */
                        start_client(gobj, CL_RET, "the CONNACK of will_ret");
                    }
                }
            }
            break;

        case CL_BAD:
            if(type == PKT_CONNACK) {
                gobj_stop(priv->tcp[CL_BAD]);   // gone with no DISCONNECT: the will is due
                /*
                 *  3. A will to a topic the ACL allows; its arrival at
                 *  will_sub is also the sign that the first one was
                 *  decided (refused, or published before it)
                 */
                start_client(gobj, CL_OK, "the CONNACK of will_ok");
            }
            break;

        case CL_OK:
            if(type == PKT_CONNACK) {
                priv->waiting = "the will of will_ok at will_sub";
                gobj_stop(priv->tcp[CL_OK]);
            }
            break;

        case CL_RET:
            if(type == PKT_CONNACK) {
                priv->waiting = "the PUBACK of will_ret";
                send_retained_publish(gobj, priv->tcp[CL_RET]);
            } else if(type == PKT_PUBACK) {
                gobj_stop(priv->tcp[CL_RET]);
                start_client(gobj, CL_LATE, "the CONNACK of will_late");
            }
            break;

        case CL_LATE:
            if(type == PKT_CONNACK) {
                const char *topics[] = {RETAINED_TOPIC};
                priv->waiting = "the retained message at will_late";
                send_subscribe(gobj, priv->tcp[CL_LATE], topics, 1);

            } else if(type == PKT_PUBLISH && len >= 2) {
                uint16_t topic_len = (uint16_t)((v[0] << 8) | v[1]);
                size_t len_data = strlen(RETAINED_DATA);
                BOOL retained = (first & 0x01)? TRUE : FALSE;
                BOOL qos0 = ((first >> 1) & 0x03) == 0? TRUE : FALSE;
                if(retained && qos0 &&
                        topic_len == strlen(RETAINED_TOPIC) &&
                        memcmp(v + 2, RETAINED_TOPIC, topic_len) == 0 &&
                        len == 2 + (uint32_t)topic_len + len_data &&
                        memcmp(v + 2 + topic_len, RETAINED_DATA, len_data) == 0) {
                    priv->got_retained++;
                } else {
                    test_error(gobj,
                        "TEST: will_late got a PUBLISH that is not the retained message",
                        (int)first, 0x31
                    );
                }
                finish(gobj);
            }
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST: a packet of an unknown client",
                NULL
            );
            break;
    }
}

/***************************************************************************
 *  The ACL model in the broker's treedb (see the file header)
 ***************************************************************************/
PRIVATE int author_acl_model(hgobj gobj, hgobj treedb)
{
    json_t *g_will = gobj_create_node(
        treedb,
        "client_groups",
        json_pack("{s:s, s:s, s:[s], s:[]}",
            "id",               "g_will",
            "language",         "en",
            "publish_acl",      "allowed/#",
            "subscribe_acl"
        ),
        NULL,
        gobj
    );
    json_t *g_open = gobj_create_node(
        treedb,
        "client_groups",
        json_pack("{s:s, s:s, s:[], s:[]}",
            "id",               "g_open",
            "language",         "en",
            "publish_acl",
            "subscribe_acl"
        ),
        NULL,
        gobj
    );
    if(!g_will || !g_open) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "TEST: cannot author the groups",
            NULL
        );
        JSON_DECREF(g_will)
        JSON_DECREF(g_open)
        return -1;
    }

    int ret = 0;
    for(int cl = 0; cl < CL_MAX; cl++) {
        json_t *client = gobj_create_node(
            treedb,
            "clients",
            json_pack("{s:s}", "id", client_ids[cl]),
            NULL,
            gobj
        );
        if(!client) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", "TEST: cannot author a client",
                "client_id",    "%s", client_ids[cl],
                NULL
            );
            ret = -1;
            continue;
        }
        json_t *group = (cl == CL_BAD || cl == CL_OK)? g_will : g_open;
        ret += gobj_link_nodes(
            treedb,
            "clients",          // hook (on parent client_groups)
            "client_groups",    // parent topic
            json_incref(group), // owned
            "clients",          // child topic
            client,             // owned
            gobj
        );
    }
    JSON_DECREF(g_will)
    JSON_DECREF(g_open)

    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "TEST: cannot link the clients to their groups",
            NULL
        );
    }
    return ret < 0? -1 : 0;
}

/***************************************************************************
 *  The client of a transport
 ***************************************************************************/
PRIVATE int client_of(hgobj gobj, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int cl = 0; cl < CL_MAX; cl++) {
        if(priv->tcp[cl] == src) {
            return cl;
        }
    }
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST: an event of an unknown transport",
        "src",          "%s", gobj_short_name(src),
        NULL
    );
    return -1;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The first timeout starts the test; any other is a step that did not
 *  come
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->started) {
        priv->started = TRUE;
        hgobj treedb = gobj_find_service(TREEDB_SERVICE, TRUE);
        if(!treedb || author_acl_model(gobj, treedb) < 0) {
            // Error already logged
            set_yuno_must_die();
            KW_DECREF(kw)
            return 0;
        }
        /*
         *  1. will_sub subscribes the two will topics
         */
        start_client(gobj, CL_SUB, "the CONNACK of will_sub");
        set_timeout(priv->timer, GUARD_MSEC);
        KW_DECREF(kw)
        return 0;
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST: a step did not come",
        "waiting",      "%s", priv->waiting? priv->waiting : "",
        NULL
    );
    finish(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Connected: the CONNECT of each client
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int cl = client_of(gobj, src);
    if(cl >= 0) {
        const char *will_topic = NULL;
        if(cl == CL_BAD) {
            will_topic = "forbidden/will";
        } else if(cl == CL_OK) {
            will_topic = "allowed/will";
        }
        send_connect(gobj, priv->tcp[cl], client_ids[cl], will_topic);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the broker sends: each client's bytes, parsed into packets
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int cl = client_of(gobj, src);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(cl >= 0 && gbuf && !priv->done) {
        gbuffer_append_gbuf(priv->rx[cl], gbuf);
        parse_rx(gobj, (client_t)cl);
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
GOBJ_DEFINE_GCLASS(C_WILL_ACL);

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
PUBLIC int register_c_will_acl(void)
{
    return create_gclass(C_WILL_ACL);
}
