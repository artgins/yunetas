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
 *              will_bad, will_ok  -> g_will
 *              will_sub           -> g_open
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

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void send_bytes(hgobj gobj, hgobj gobj_tcp, const uint8_t *bf, size_t len);
PRIVATE void send_connect(hgobj gobj, hgobj gobj_tcp, const char *client_id, const char *will_topic);

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
    hgobj tcp_sub;
    hgobj tcp_bad;
    hgobj tcp_ok;
    int phase;
    gbuffer_t *rx;          // what the broker sent to will_sub, not parsed yet
    int got_forbidden;      // PUBLISH of `forbidden/will` that will_sub got
    int got_allowed;        // PUBLISH of `allowed/will` that will_sub got
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
    priv->tcp_sub = gobj_create("will_sub", C_TCP, json_pack("{s:s}", "url", BROKER_URL), gobj);
    priv->tcp_bad = gobj_create("will_bad", C_TCP, json_pack("{s:s}", "url", BROKER_URL), gobj);
    priv->tcp_ok = gobj_create("will_ok", C_TCP, json_pack("{s:s}", "url", BROKER_URL), gobj);
    priv->rx = gbuffer_create(1024, 1024);
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
    hgobj tcps[] = {priv->tcp_sub, priv->tcp_bad, priv->tcp_ok};
    for(size_t i = 0; i < sizeof(tcps)/sizeof(tcps[0]); i++) {
        if(gobj_is_running(tcps[i])) {
            gobj_stop(tcps[i]);
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
 *  Parse what the broker sent to will_sub: the topics of its PUBLISH
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
        if(type == 3 && remaining >= 2) {
            uint16_t topic_len = (uint16_t)((v[0] << 8) | v[1]);
            if(2 + (uint32_t)topic_len <= remaining) {
                char topic[128];
                snprintf(topic, sizeof(topic), "%.*s", (int)topic_len, (const char *)(v + 2));
                if(strcmp(topic, "forbidden/will") == 0) {
                    priv->got_forbidden++;
                } else if(strcmp(topic, "allowed/will") == 0) {
                    priv->got_allowed++;
                }
            }
        }
        gbuffer_get(priv->rx, hdr + remaining);
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
    const char *clients[] = {"will_bad", "will_ok", "will_sub"};
    for(size_t i = 0; i < sizeof(clients)/sizeof(clients[0]); i++) {
        json_t *client = gobj_create_node(
            treedb,
            "clients",
            json_pack("{s:s}", "id", clients[i]),
            NULL,
            gobj
        );
        if(!client) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", "TEST: cannot author a client",
                "client_id",    "%s", clients[i],
                NULL
            );
            ret = -1;
            continue;
        }
        json_t *group = (i < 2)? g_will : g_open;
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
            {
                hgobj treedb = gobj_find_service(TREEDB_SERVICE, TRUE);
                if(!treedb || author_acl_model(gobj, treedb) < 0) {
                    // Error already logged
                    set_yuno_must_die();
                    break;
                }
                gobj_start(priv->tcp_sub);  // CONNECT and SUBSCRIBE go at EV_CONNECTED
                set_timeout(priv->timer, 300);
            }
            break;

        case 2:
            /*
             *  2. A will to a topic the ACL refuses
             */
            gobj_start(priv->tcp_bad);
            set_timeout(priv->timer, 300);
            break;

        case 3:
            gobj_stop(priv->tcp_bad);       // gone with no DISCONNECT: the will is due
            set_timeout(priv->timer, 300);
            break;

        case 4:
            parse_rx(gobj);
            if(priv->got_forbidden != 0) {
                test_error(gobj,
                    "TEST: a will to a topic the ACL refuses was published",
                    priv->got_forbidden, 0
                );
            }

            /*
             *  3. A will to a topic the ACL allows
             */
            gobj_start(priv->tcp_ok);
            set_timeout(priv->timer, 300);
            break;

        case 5:
            gobj_stop(priv->tcp_ok);
            set_timeout(priv->timer, 300);
            break;

        default:
            parse_rx(gobj);
            if(priv->got_allowed != 1) {
                test_error(gobj,
                    "TEST: a will to a topic the ACL allows was not published",
                    priv->got_allowed, 1
                );
            }
            if(priv->got_forbidden != 0) {
                test_error(gobj,
                    "TEST: a will to a topic the ACL refuses was published",
                    priv->got_forbidden, 0
                );
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: will under the ACL done",
                "forbidden",    "%d", priv->got_forbidden,
                "allowed",      "%d", priv->got_allowed,
                NULL
            );
            if(gobj_is_running(priv->tcp_sub)) {
                gobj_stop(priv->tcp_sub);
            }
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Connected: CONNECT of each client; will_sub subscribes too
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->tcp_sub) {
        send_connect(gobj, src, "will_sub", NULL);
        static const uint8_t subscribe[] = {
            0x82, 34,                           // SUBSCRIBE, remaining length
            0x00, 0x01,                         // packet id
            0x00, 14, 'f', 'o', 'r', 'b', 'i', 'd', 'd', 'e', 'n', '/', 'w', 'i', 'l', 'l',
            0x00,                               // QoS 0
            0x00, 12, 'a', 'l', 'l', 'o', 'w', 'e', 'd', '/', 'w', 'i', 'l', 'l',
            0x00                                // QoS 0
        };
        send_bytes(gobj, src, subscribe, sizeof(subscribe));
    } else if(src == priv->tcp_bad) {
        send_connect(gobj, src, "will_bad", "forbidden/will");
    } else if(src == priv->tcp_ok) {
        send_connect(gobj, src, "will_ok", "allowed/will");
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: EV_CONNECTED of an unknown transport",
            "src",          "%s", gobj_short_name(src),
            NULL
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the broker sends: kept for will_sub, the CONNACKs of the others
 *  dropped
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->tcp_sub) {
        gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
        if(gbuf) {
            gbuffer_append_gbuf(priv->rx, gbuf);
        }
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
