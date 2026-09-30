/****************************************************************************
 *          C_LEGACY_PROT.C
 *
 *          Driver of the test of C_PROT_MQTT, the deprecated MQTT protocol
 *          gclass, as a server. No broker and no socket: the protocol gobj
 *          is a child of this driver, its bottom is a C_FAKE_TRANSPORT, and
 *          this driver writes the client's MQTT 5 packets into it by hand.
 *          It is also the "resource" gobj of the protocol (the clients it
 *          keeps, with their subscriptions): a json dict in memory.
 *
 *              1. CONNECT, then SUBSCRIBE to two topics: "a/b", and one of
 *                 300 bytes, whose length has a non-zero high byte.
 *              2. UNSUBSCRIBE of both topics in ONE packet. The protocol
 *                 must remove both: UNSUBACK with two 0x00 (Success), the
 *                 "unsubscribing" EV_ON_MESSAGE with the two topics as they
 *                 were sent, and no subscription left. Up to 7.25.20 the
 *                 topic was read in the packet as a C string: "a/b" went on
 *                 with the length of the next topic, so it was not found
 *                 (0x11, No subscription existed) and stayed subscribed.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include <c_prot_mqtt.h>
#include "c_fake_transport.h"
#include "c_legacy_prot.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CLIENT_ID           "legacy_cl"
#define SHORT_TOPIC         "a/b"
#define LONG_TOPIC_LEN      300     // > 255: its length has a non-zero high byte

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
    json_t *resources;      // the clients of the protocol, by client_id
    char long_topic[LONG_TOPIC_LEN + 1];

    int connack_reason;     // -1 none
    json_t *suback_codes;
    json_t *unsuback_codes;
    json_t *unsubscribed;   // the list of the "unsubscribing" EV_ON_MESSAGE
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
    priv->rx = gbuffer_create(4*1024, 4*1024);
    priv->resources = json_object();
    priv->connack_reason = -1;
    priv->suback_codes = json_array();
    priv->unsuback_codes = json_array();

    memcpy(priv->long_topic, "l/", 2);
    memset(priv->long_topic + 2, 'x', LONG_TOPIC_LEN - 2);
    priv->long_topic[LONG_TOPIC_LEN] = 0;

    priv->prot = gobj_create(
        "legacy_prot",
        C_PROT_MQTT,
        json_pack("{s:b}", "iamServer", 1),
        gobj
    );
    gobj_write_pointer_attr(priv->prot, "gobj_mqtt_clients", gobj);
    gobj_write_pointer_attr(priv->prot, "gobj_mqtt_topics", gobj);
    gobj_write_pointer_attr(priv->prot, "gobj_mqtt_users", gobj);

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
    JSON_DECREF(priv->resources)
    JSON_DECREF(priv->suback_codes)
    JSON_DECREF(priv->unsuback_codes)
    JSON_DECREF(priv->unsubscribed)
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

/***************************************************************************
 *      Framework Method create_resource: the clients, in memory
 ***************************************************************************/
PRIVATE json_t *mt_create_resource(
    hgobj gobj,
    const char *resource,
    json_t *kw,         // owned
    json_t *jn_options  // owned
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!kw) {
        kw = json_object();
    }
    json_object_set_new(priv->resources, resource, kw);
    JSON_DECREF(jn_options)
    return kw;  // NOT yours
}

/***************************************************************************
 *      Framework Method get_resource
 ***************************************************************************/
PRIVATE json_t *mt_get_resource(
    hgobj gobj,
    const char *resource,
    json_t *jn_filter,  // owned
    json_t *jn_options  // owned
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(jn_filter)
    JSON_DECREF(jn_options)
    return json_object_get(priv->resources, resource);  // NOT yours
}

/***************************************************************************
 *      Framework Method save_resource
 ***************************************************************************/
PRIVATE int mt_save_resource(
    hgobj gobj,
    const char *resource,
    json_t *record,     // NOT owned
    json_t *jn_options  // owned
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(record && json_object_get(priv->resources, resource) != record) {
        json_object_set(priv->resources, resource, record);
    }
    JSON_DECREF(jn_options)
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
 *  The reason codes of a SUBACK/UNSUBACK, MQTT 5
 ***************************************************************************/
PRIVATE void get_ack_codes(json_t *codes, const uint8_t *v, uint32_t remaining)
{
    if(remaining < 3) {
        return;
    }
    uint32_t i = 3 + v[2];  // packet id, properties length (one byte), properties
    for(; i < remaining; i++) {
        json_array_append_new(codes, json_integer(v[i]));
    }
}

/***************************************************************************
 *  Parse what the protocol wrote
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
        switch(type) {
            case 2:     // CONNACK
                if(remaining >= 2) {
                    priv->connack_reason = v[1];
                }
                break;
            case 9:     // SUBACK
                get_ack_codes(priv->suback_codes, v, remaining);
                break;
            case 11:    // UNSUBACK
                get_ack_codes(priv->unsuback_codes, v, remaining);
                break;
            default:
                break;
        }
        gbuffer_get(priv->rx, hdr + remaining);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_error(hgobj gobj, const char *msg, json_t *got, json_t *expected)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", msg,
        "got",          "%j", got,
        "expected",     "%j", expected,
        NULL
    );
}

/***************************************************************************
 *  1. CONNECT, SUBSCRIBE to the two topics
 ***************************************************************************/
PRIVATE void connect_and_subscribe(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t bf[1024];
    size_t n;

    gobj_send_event(priv->prot, EV_CONNECTED, 0, priv->fake);

    static const uint8_t connect_head[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', 0x05,   // protocol name, level 5
        0x02,                                   // flags: clean start
        0x00, 0x3C,                             // keep alive 60 s
        0x00                                    // properties length
    };
    n = put_fixed_header(bf, 0x10, sizeof(connect_head) + 2 + strlen(CLIENT_ID));
    memcpy(bf + n, connect_head, sizeof(connect_head));
    n += sizeof(connect_head);
    n += put_string(bf + n, CLIENT_ID);
    peer_sends(gobj, bf, n);

    /*
     *  Options 0x21: QoS 1, retain handling 2 (do not send retained)
     */
    n = put_fixed_header(bf, 0x82, 2 + 1 + 2 + strlen(SHORT_TOPIC) + 1 + 2 + LONG_TOPIC_LEN + 1);
    bf[n++] = 0x00;     // packet id 1
    bf[n++] = 0x01;
    bf[n++] = 0x00;     // properties length
    n += put_string(bf + n, SHORT_TOPIC);
    bf[n++] = 0x21;
    n += put_string(bf + n, priv->long_topic);
    bf[n++] = 0x21;
    peer_sends(gobj, bf, n);
}

/***************************************************************************
 *  2. UNSUBSCRIBE of the two topics in one packet
 ***************************************************************************/
PRIVATE void unsubscribe_both(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint8_t bf[1024];
    size_t n;

    n = put_fixed_header(bf, 0xA2, 2 + 1 + 2 + strlen(SHORT_TOPIC) + 2 + LONG_TOPIC_LEN);
    bf[n++] = 0x00;     // packet id 2
    bf[n++] = 0x02;
    bf[n++] = 0x00;     // properties length
    n += put_string(bf + n, SHORT_TOPIC);
    n += put_string(bf + n, priv->long_topic);
    peer_sends(gobj, bf, n);
}

/***************************************************************************
 *  What the unsubscription must leave
 ***************************************************************************/
PRIVATE void check_unsubscribed(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *expected = json_pack("[i,i]", 0, 0);
    if(!json_equal(priv->unsuback_codes, expected)) {
        test_error(gobj,
            "TEST: UNSUBACK of the two topics is not Success twice",
            priv->unsuback_codes, expected
        );
    }
    JSON_DECREF(expected)

    expected = json_pack("[s,s]", SHORT_TOPIC, priv->long_topic);
    if(!priv->unsubscribed || !json_equal(priv->unsubscribed, expected)) {
        test_error(gobj,
            "TEST: the unsubscribed topics are not the ones sent",
            priv->unsubscribed, expected
        );
    }
    JSON_DECREF(expected)

    json_t *client = json_object_get(priv->resources, CLIENT_ID);
    json_t *subscriptions = client? json_object_get(client, "subscriptions") : NULL;
    if(!subscriptions || json_object_size(subscriptions) != 0) {
        expected = json_object();
        test_error(gobj,
            "TEST: subscriptions left after the UNSUBSCRIBE",
            subscriptions, expected
        );
        JSON_DECREF(expected)
    }
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
                connect_and_subscribe(gobj);
                parse_rx(gobj);
                if(priv->connack_reason != 0) {
                    json_t *got = json_integer(priv->connack_reason);
                    json_t *expected = json_integer(0);
                    test_error(gobj, "TEST: CONNECT not accepted", got, expected);
                    JSON_DECREF(got)
                    JSON_DECREF(expected)
                }
                json_t *expected = json_pack("[i,i]", 1, 1);
                if(!json_equal(priv->suback_codes, expected)) {
                    test_error(gobj,
                        "TEST: SUBACK of the two topics is not QoS 1 twice",
                        priv->suback_codes, expected
                    );
                }
                JSON_DECREF(expected)

                unsubscribe_both(gobj);
                parse_rx(gobj);
                check_unsubscribed(gobj);
                set_timeout(priv->timer, 100);
            }
            break;

        default:
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: legacy prot done",
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

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the protocol publishes: keep the unsubscribed topics
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *mqtt_action = kw_get_str(gobj, kw, "mqtt_action", "", 0);
    if(strcmp(mqtt_action, "unsubscribing")==0) {
        JSON_DECREF(priv->unsubscribed)
        priv->unsubscribed = json_deep_copy(kw_get_list(gobj, kw, "list", 0, 0));
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The protocol's other output events: nothing to do
 ***************************************************************************/
PRIVATE int ac_prot_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
    .mt_create          = mt_create,
    .mt_destroy         = mt_destroy,
    .mt_start           = mt_start,
    .mt_stop            = mt_stop,
    .mt_play            = mt_play,
    .mt_pause           = mt_pause,
    .mt_create_resource = mt_create_resource,
    .mt_get_resource    = mt_get_resource,
    .mt_save_resource   = mt_save_resource,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_LEGACY_PROT);

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
        {EV_ON_MESSAGE,         ac_on_message,      0},
        {EV_ON_OPEN,            ac_prot_event,      0},
        {EV_ON_CLOSE,           ac_prot_event,      0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_RX_DATA,            0},
        {EV_ON_MESSAGE,         0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
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
PUBLIC int register_c_legacy_prot(void)
{
    return create_gclass(C_LEGACY_PROT);
}
