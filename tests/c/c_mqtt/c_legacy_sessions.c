/****************************************************************************
 *          C_LEGACY_SESSIONS.C
 *
 *          Driver of the test of the client records of C_PROT_MQTT, the
 *          deprecated MQTT protocol gclass, as a server. No broker and no
 *          socket: four protocol gobjs are children of this driver, each
 *          with a C_FAKE_TRANSPORT as its bottom, and the driver writes the
 *          clients' packets into them by hand. The clients are kept in a
 *          persistent C_RESOURCE2, as an MQTT gate keeps them. The
 *          protocol's limits are its defaults: 100 subscriptions per
 *          client, 1000 persistent sessions.
 *
 *              1. A device "dev" (clean session 0) subscribes "dev/t", and
 *                 "pub" (clean session 1) publishes to it 1000 times: the
 *                 time it takes is the reference.
 *                 Then 300 clients with clean session (MQTT 3.1.1 clean
 *                 session 1; MQTT 5 clean start with no session expiry;
 *                 MQTT 5 no clean start with no session expiry) connect,
 *                 subscribe and go: none is left, in memory or on disk.
 *                 One of them gets a QoS 1 message: no file either.
 *                 One of those ids comes back with clean session 0: no
 *                 session present, a record and its file; and again with
 *                 clean session 1: the record and its file are removed.
 *                 A clean session taken over by a second connection of the
 *                 same id: the first one is dropped, the record belongs to
 *                 the second, and is removed when the second goes.
 *              2. A clean session taken over with clean session 0 is a
 *                 session with its file, which a later clean session
 *                 removes.
 *                 Re-subscribing to a filter the client holds replaces it:
 *                 SUBACK granted, the client is kept, also after a
 *                 reconnection with clean session 0.
 *                 A client subscribes 160 filters in two packets: 100 are
 *                 granted, the other 60 are refused in the SUBACK (0x80),
 *                 the client is kept, and a re-subscription of one it holds
 *                 is still granted. An MQTT 5 client with 101: the last is
 *                 refused with 0x97 (Quota exceeded). One WARNING each.
 *              3. New ids with clean session 0, each with 100 filters, until
 *                 1000 persistent sessions: the next one is refused at
 *                 CONNECT (3.1.1: 3, Server unavailable; MQTT 5: 0x97), one
 *                 WARNING. A new id with clean session 1 and an id that has
 *                 a session are still accepted.
 *              4. The 1000 publications of 1 again, with the 1000 sessions
 *                 and their 100000 subscriptions: what they take is bounded
 *                 (within 3 s of the reference). A third new id is refused,
 *                 and says nothing more within the window of the WARNING.
 *
 *          Up to 7.30.21 every client id made a record that was never
 *          removed, on disk and in memory, a client could hold any number
 *          of subscriptions, each PUBLISH walked every subscription of
 *          every record, and a re-subscription disconnected the client.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <dirent.h>
#include <yunetas.h>
#include <c_prot_mqtt.h>
#include "c_fake_transport.h"
#include "c_legacy_sessions.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MAX_SUBSCRIPTIONS   100     // default max_subscriptions_per_client
#define MAX_SESSIONS        1000    // default max_persistent_sessions
#define CHURN               300
#define PUBLICATIONS        1000
#define PUBLISH_MARGIN_MS   3000

#define STORE_SERVICE       "mqtt"
#define STORE_DATABASE      "legacy_sessions_clients"

enum {
    DEV = 0,
    PUB,
    ATT,
    ATT2,
    N_PEERS
};

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    hgobj prot;
    hgobj fake;
    gbuffer_t *rx;          // what the protocol wrote, not parsed yet
    int version;            // 4: MQTT 3.1.1, 5: MQTT 5
    int connack_flags;      // -1 none
    int connack_reason;     // -1 none
    json_t *suback_codes;
    int publications;       // PUBLISH received
} peer_t;

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
    hgobj clients;
    hgobj topics;
    hgobj users;
    int phase;
    peer_t peers[N_PEERS];
    uint64_t reference_ms;  // the publications before the sessions
    json_int_t drops[N_PEERS];
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

    priv->clients = gobj_create_service(
        "legacy_sessions_clients",
        C_RESOURCE2,
        json_pack("{s:s, s:s, s:b}",
            "service", STORE_SERVICE,
            "database", STORE_DATABASE,
            "persistent", 1
        ),
        gobj
    );
    priv->topics = gobj_create_service(
        "legacy_sessions_topics",
        C_RESOURCE2,
        json_pack("{s:b}", "persistent", 0),
        gobj
    );
    priv->users = gobj_create_service(
        "legacy_sessions_users",
        C_RESOURCE2,
        json_pack("{s:b}", "persistent", 0),
        gobj
    );

    static const char *names[N_PEERS] = {"dev", "pub", "att", "att2"};
    for(int i=0; i<N_PEERS; i++) {
        peer_t *peer = &priv->peers[i];
        peer->rx = gbuffer_create(64*1024, 64*1024);
        peer->suback_codes = json_array();
        peer->connack_flags = -1;
        peer->connack_reason = -1;

        char name[NAME_MAX];
        snprintf(name, sizeof(name), "prot_%s", names[i]);
        peer->prot = gobj_create(
            name,
            C_PROT_MQTT,
            json_pack("{s:b}", "iamServer", 1),
            gobj
        );
        gobj_write_pointer_attr(peer->prot, "gobj_mqtt_clients", priv->clients);
        gobj_write_pointer_attr(peer->prot, "gobj_mqtt_topics", priv->topics);
        gobj_write_pointer_attr(peer->prot, "gobj_mqtt_users", priv->users);

        snprintf(name, sizeof(name), "fake_%s", names[i]);
        peer->fake = gobj_create(
            name,
            C_FAKE_TRANSPORT,
            json_pack("{s:I}", "sink", (json_int_t)(uintptr_t)gobj),
            peer->prot
        );
        gobj_set_bottom_gobj(peer->prot, peer->fake);
    }
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<N_PEERS; i++) {
        GBUFFER_DECREF(priv->peers[i].rx)
        JSON_DECREF(priv->peers[i].suback_codes)
    }
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->clients);
    gobj_start(priv->topics);
    gobj_start(priv->users);
    for(int i=0; i<N_PEERS; i++) {
        gobj_start(priv->peers[i].fake);
        gobj_start(priv->peers[i].prot);
    }

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

    /*
     *  The protocols first: their stop writes the record of their client
     */
    for(int i=0; i<N_PEERS; i++) {
        if(gobj_is_running(priv->peers[i].prot)) {
            gobj_stop(priv->peers[i].prot);  // it stops its bottom
        }
    }
    if(gobj_is_running(priv->clients)) {
        gobj_stop(priv->clients);
    }
    if(gobj_is_running(priv->topics)) {
        gobj_stop(priv->topics);
    }
    if(gobj_is_running(priv->users)) {
        gobj_stop(priv->users);
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
 *
 ***************************************************************************/
PRIVATE void check_int(hgobj gobj, const char *msg, json_int_t got, json_int_t expected)
{
    if(got != expected) {
        json_t *jn_got = json_integer(got);
        json_t *jn_expected = json_integer(expected);
        test_error(gobj, msg, jn_got, jn_expected);
        JSON_DECREF(jn_got)
        JSON_DECREF(jn_expected)
    }
}

/***************************************************************************
 *  The records the store keeps in memory
 ***************************************************************************/
PRIVATE int records_in_memory(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *list = gobj_list_resource(priv->clients, "", 0, 0);
    int n = (int)json_array_size(list);
    JSON_DECREF(list)
    return n;
}

/***************************************************************************
 *  The records the store keeps on disk: the files of its directory
 ***************************************************************************/
PRIVATE int records_on_disk(hgobj gobj)
{
    char path_service[PATH_MAX];
    char dir[PATH_MAX];
    if(!build_path(path_service, sizeof(path_service),
            STORE_SERVICE, gobj_yuno_role_plus_name(), STORE_DATABASE, NULL) ||
            !yuneta_store_dir(dir, sizeof(dir), "resources", path_service, FALSE)) {
        return -1;  // Error already logged
    }
    DIR *d = opendir(dir);
    if(!d) {
        return 0;
    }
    int n = 0;
    struct dirent *de;
    while((de = readdir(d))) {
        if(strcmp(de->d_name, ".")==0 || strcmp(de->d_name, "..")==0) {
            continue;
        }
        n++;
    }
    closedir(d);
    return n;
}

/***************************************************************************
 *  The subscriptions of a record
 ***************************************************************************/
PRIVATE int subscriptions_of(hgobj gobj, const char *client_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *client = gobj_get_resource(priv->clients, client_id, 0, 0);
    if(!client) {
        return -1;
    }
    return (int)json_object_size(json_object_get(client, "subscriptions"));
}

/***************************************************************************
 *  Variable byte integer of MQTT
 ***************************************************************************/
PRIVATE size_t put_varint(uint8_t *bf, uint32_t value)
{
    size_t n = 0;
    do {
        uint8_t b = value % 128;
        value /= 128;
        if(value > 0) {
            b |= 0x80;
        }
        bf[n++] = b;
    } while(value > 0);
    return n;
}

/***************************************************************************
 *  A string of the packet: two-byte length, then the bytes
 ***************************************************************************/
PRIVATE void put_string(gbuffer_t *gbuf, const char *s)
{
    size_t len = strlen(s);
    uint8_t l[2] = {(uint8_t)(len >> 8), (uint8_t)(len & 0xFF)};
    gbuffer_append(gbuf, l, 2);
    gbuffer_append(gbuf, (void *)s, len);
}

/***************************************************************************
 *  Write the client's packet into its protocol
 ***************************************************************************/
PRIVATE void peer_sends(hgobj gobj, peer_t *peer, uint8_t type, gbuffer_t *body)
{
    size_t len = gbuffer_leftbytes(body);
    uint8_t head[8];
    head[0] = type;
    size_t n = 1 + put_varint(head + 1, (uint32_t)len);

    gbuffer_t *gbuf = gbuffer_create(n + len, n + len);
    gbuffer_append(gbuf, head, n);
    gbuffer_append_gbuf(gbuf, body);
    GBUFFER_DECREF(body)

    gobj_send_event(
        peer->prot,
        EV_RX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        peer->fake
    );
}

/***************************************************************************
 *  A new connection and its CONNECT
 *  expiry: the Session Expiry Interval property of MQTT 5, -1 none
 ***************************************************************************/
PRIVATE void mqtt_connect(
    hgobj gobj,
    peer_t *peer,
    int version,
    BOOL clean,
    const char *client_id,
    json_int_t expiry
) {
    peer->version = version;
    peer->connack_flags = -1;
    peer->connack_reason = -1;

    gobj_send_event(peer->prot, EV_CONNECTED, 0, peer->fake);

    gbuffer_t *body = gbuffer_create(256, 256);
    uint8_t head[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', (uint8_t)version,
        clean? 0x02 : 0x00,                     // flags
        0x00, 0x3C                              // keep alive 60 s
    };
    gbuffer_append(body, head, sizeof(head));
    if(version == 5) {
        if(expiry >= 0) {
            uint8_t props[] = {
                0x05,                                   // properties length
                0x11,                                   // session expiry interval
                (uint8_t)(expiry >> 24), (uint8_t)(expiry >> 16),
                (uint8_t)(expiry >> 8), (uint8_t)expiry
            };
            gbuffer_append(body, props, sizeof(props));
        } else {
            uint8_t props[] = {0x00};
            gbuffer_append(body, props, sizeof(props));
        }
    }
    put_string(body, client_id);
    peer_sends(gobj, peer, 0x10, body);
}

/***************************************************************************
 *  SUBSCRIBE of filters "<prefix><n>", n from `from` (-1: the prefix alone)
 ***************************************************************************/
PRIVATE void mqtt_subscribe_qos(
    hgobj gobj,
    peer_t *peer,
    uint16_t mid,
    const char *prefix,
    int from,
    int count,
    uint8_t qos
) {
    json_array_clear(peer->suback_codes);

    gbuffer_t *body = gbuffer_create(32*1024, 32*1024);
    uint8_t m[2] = {(uint8_t)(mid >> 8), (uint8_t)(mid & 0xFF)};
    gbuffer_append(body, m, 2);
    if(peer->version == 5) {
        uint8_t props[] = {0x00};
        gbuffer_append(body, props, sizeof(props));
    }
    for(int i=from; i<from+count; i++) {
        char filter[NAME_MAX];
        if(count == 1 && from < 0) {
            snprintf(filter, sizeof(filter), "%s", prefix);
        } else {
            snprintf(filter, sizeof(filter), "%s%d", prefix, i);
        }
        put_string(body, filter);
        uint8_t options = qos;
        gbuffer_append(body, &options, 1);
    }
    peer_sends(gobj, peer, 0x82, body);
}

/***************************************************************************
 *  SUBSCRIBE QoS 0
 ***************************************************************************/
PRIVATE void mqtt_subscribe(
    hgobj gobj,
    peer_t *peer,
    uint16_t mid,
    const char *prefix,
    int from,
    int count
) {
    mqtt_subscribe_qos(gobj, peer, mid, prefix, from, count, 0);
}

/***************************************************************************
 *  PUBLISH, QoS 0, or QoS 1 with the packet id `mid`
 ***************************************************************************/
PRIVATE void mqtt_publish_qos(
    hgobj gobj,
    peer_t *peer,
    const char *topic,
    const char *payload,
    uint16_t mid
) {
    gbuffer_t *body = gbuffer_create(256, 256);
    put_string(body, topic);
    if(mid) {
        uint8_t m[2] = {(uint8_t)(mid >> 8), (uint8_t)(mid & 0xFF)};
        gbuffer_append(body, m, 2);
    }
    if(peer->version == 5) {
        uint8_t props[] = {0x00};
        gbuffer_append(body, props, sizeof(props));
    }
    gbuffer_append(body, (void *)payload, strlen(payload));
    peer_sends(gobj, peer, mid? 0x32 : 0x30, body);
}

/***************************************************************************
 *  PUBLISH QoS 0
 ***************************************************************************/
PRIVATE void mqtt_publish(hgobj gobj, peer_t *peer, const char *topic, const char *payload)
{
    mqtt_publish_qos(gobj, peer, topic, payload, 0);
}

/***************************************************************************
 *  The "socket" of the client closes
 ***************************************************************************/
PRIVATE void mqtt_close(hgobj gobj, peer_t *peer)
{
    gobj_send_event(peer->prot, EV_DISCONNECTED, 0, peer->fake);
}

/***************************************************************************
 *  Parse what the protocol wrote
 ***************************************************************************/
PRIVATE void parse_rx(hgobj gobj, peer_t *peer)
{
    while(gbuffer_leftbytes(peer->rx) >= 2) {
        const uint8_t *p = gbuffer_cur_rd_pointer(peer->rx);
        size_t avail = gbuffer_leftbytes(peer->rx);
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
                    peer->connack_flags = v[0];
                    peer->connack_reason = v[1];
                }
                break;
            case 3:     // PUBLISH
                peer->publications++;
                break;
            case 9:     // SUBACK
                {
                    uint32_t i = 2;     // packet id
                    if(peer->version == 5 && remaining > 2) {
                        i += 1 + v[2];  // properties length (one byte), properties
                    }
                    for(; i < remaining; i++) {
                        json_array_append_new(peer->suback_codes, json_integer(v[i]));
                    }
                }
                break;
            default:
                break;
        }
        gbuffer_get(peer->rx, hdr + remaining);
    }
    if(gbuffer_leftbytes(peer->rx) == 0) {
        gbuffer_clear(peer->rx);    // all parsed: room again for what comes
    }
}

/***************************************************************************
 *  The SUBACK codes: `granted` times 0x00, then `refused` times `code`
 ***************************************************************************/
PRIVATE void check_suback(
    hgobj gobj,
    peer_t *peer,
    const char *msg,
    int granted,
    int refused,
    int code
) {
    json_t *expected = json_array();
    for(int i=0; i<granted; i++) {
        json_array_append_new(expected, json_integer(0));
    }
    for(int i=0; i<refused; i++) {
        json_array_append_new(expected, json_integer(code));
    }
    if(!json_equal(peer->suback_codes, expected)) {
        test_error(gobj, msg, peer->suback_codes, expected);
    }
    JSON_DECREF(expected)
}

/***************************************************************************
 *  The protocol of the peer did not drop it since the last look
 ***************************************************************************/
PRIVATE void check_kept(hgobj gobj, int idx, const char *msg)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t drops = gobj_read_integer_attr(priv->peers[idx].fake, "drops");
    check_int(gobj, msg, drops, priv->drops[idx]);
}

/***************************************************************************
 *  The protocol of the peer dropped it once since the last look
 ***************************************************************************/
PRIVATE void check_dropped(hgobj gobj, int idx, const char *msg)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t drops = gobj_read_integer_attr(priv->peers[idx].fake, "drops");
    check_int(gobj, msg, drops, priv->drops[idx] + 1);
    priv->drops[idx] = drops;
}

/***************************************************************************
 *  `PUBLICATIONS` messages of "pub" to "dev/t": the milliseconds they take
 ***************************************************************************/
PRIVATE uint64_t publish_to_dev(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    peer_t *dev = &priv->peers[DEV];
    peer_t *pub = &priv->peers[PUB];

    dev->publications = 0;
    uint64_t t0 = time_in_milliseconds_monotonic();
    for(int i=0; i<PUBLICATIONS; i++) {
        mqtt_publish(gobj, pub, "dev/t", "on");
    }
    uint64_t t = time_in_milliseconds_monotonic() - t0;
    check_int(gobj, "TEST: the device did not get every publication",
        dev->publications, PUBLICATIONS
    );
    return t;
}

/***************************************************************************
 *  1. The reference, the clean sessions, the take over
 ***************************************************************************/
PRIVATE void phase_clean_sessions(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    peer_t *dev = &priv->peers[DEV];
    peer_t *pub = &priv->peers[PUB];
    peer_t *att = &priv->peers[ATT];
    peer_t *att2 = &priv->peers[ATT2];

    mqtt_connect(gobj, dev, 4, FALSE, "dev", -1);
    check_int(gobj, "TEST: CONNECT of dev not accepted", dev->connack_reason, 0);
    mqtt_subscribe(gobj, dev, 1, "dev/t", -1, 1);
    check_suback(gobj, dev, "TEST: SUBACK of dev", 1, 0, 0);

    mqtt_connect(gobj, pub, 4, TRUE, "pub", -1);
    check_int(gobj, "TEST: CONNECT of pub not accepted", pub->connack_reason, 0);

    priv->reference_ms = publish_to_dev(gobj);

    /*
     *  Clean sessions: none is left
     */
    for(int i=0; i<CHURN; i++) {
        char client_id[NAME_MAX];
        snprintf(client_id, sizeof(client_id), "clean-%d", i);
        switch(i % 3) {
            case 0:
                mqtt_connect(gobj, att, 4, TRUE, client_id, -1);
                break;
            case 1:
                mqtt_connect(gobj, att, 5, TRUE, client_id, -1);
                break;
            default:
                mqtt_connect(gobj, att, 5, FALSE, client_id, 0);
                break;
        }
        check_int(gobj, "TEST: CONNECT with clean session not accepted", att->connack_reason, 0);
        mqtt_subscribe(gobj, att, 1, "c/", i, 1);
        check_suback(gobj, att, "TEST: SUBACK of a clean session", 1, 0, 0);
        mqtt_close(gobj, att);
    }
    check_int(gobj, "TEST: records in memory after the clean sessions (dev, pub)",
        records_in_memory(gobj), 2
    );
    check_int(gobj, "TEST: records on disk after the clean sessions (dev)",
        records_on_disk(gobj), 1
    );

    /*
     *  A clean session that gets a QoS 1 message: still no file
     */
    mqtt_connect(gobj, att, 4, TRUE, "q1", -1);
    mqtt_subscribe_qos(gobj, att, 1, "q/t", -1, 1, 1);
    check_suback(gobj, att, "TEST: SUBACK QoS 1 of a clean session", 0, 1, 1);   // one code 1: granted QoS 1
    att->publications = 0;
    mqtt_publish_qos(gobj, pub, "q/t", "on", 1);
    check_int(gobj, "TEST: the clean session did not get the QoS 1 message", att->publications, 1);
    mqtt_close(gobj, att);
    check_int(gobj, "TEST: records on disk after a clean session got a QoS 1 message (dev)",
        records_on_disk(gobj), 1
    );

    /*
     *  The id of a clean session, with clean session 0: a new session
     */
    mqtt_connect(gobj, att, 4, FALSE, "clean-0", -1);
    check_int(gobj, "TEST: CONNECT of clean-0, clean session 0", att->connack_reason, 0);
    check_int(gobj, "TEST: a session present for clean-0", att->connack_flags, 0);
    check_int(gobj, "TEST: subscriptions of clean-0", subscriptions_of(gobj, "clean-0"), 0);
    mqtt_close(gobj, att);
    check_int(gobj, "TEST: records on disk with the session of clean-0",
        records_on_disk(gobj), 2
    );

    /*
     *  And with clean session 1 again: the session ends with the connection
     */
    mqtt_connect(gobj, att, 4, TRUE, "clean-0", -1);
    check_int(gobj, "TEST: CONNECT of clean-0, clean session 1", att->connack_reason, 0);
    mqtt_close(gobj, att);
    check_int(gobj, "TEST: records in memory after clean-0 ended its session",
        records_in_memory(gobj), 2
    );
    check_int(gobj, "TEST: records on disk after clean-0 ended its session",
        records_on_disk(gobj), 1
    );

    /*
     *  A clean session taken over: att is dropped, att2 owns the record
     */
    mqtt_connect(gobj, att, 4, TRUE, "tk", -1);
    check_int(gobj, "TEST: CONNECT of tk", att->connack_reason, 0);
    mqtt_connect(gobj, att2, 4, TRUE, "tk", -1);
    check_int(gobj, "TEST: CONNECT of tk, again", att2->connack_reason, 0);
    mqtt_close(gobj, att2);
}

/***************************************************************************
 *  2. Re-subscription, subscriptions of a client
 ***************************************************************************/
PRIVATE void phase_subscriptions(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    peer_t *att = &priv->peers[ATT];
    peer_t *att2 = &priv->peers[ATT2];

    check_dropped(gobj, ATT, "TEST: the first connection of tk was not dropped");
    check_int(gobj, "TEST: records in memory after the take over (dev, pub)",
        records_in_memory(gobj), 2
    );
    check_int(gobj, "TEST: records on disk after the take over (dev)",
        records_on_disk(gobj), 1
    );

    /*
     *  A clean session taken over with clean session 0: a session, with its
     *  file; ended later by a clean session: no file
     */
    mqtt_connect(gobj, att2, 4, TRUE, "tk2", -1);
    mqtt_connect(gobj, att, 4, FALSE, "tk2", -1);
    check_int(gobj, "TEST: CONNECT of tk2 with clean session 0", att->connack_reason, 0);
    mqtt_close(gobj, att);
    check_int(gobj, "TEST: records on disk with the session of tk2",
        records_on_disk(gobj), 2
    );
    mqtt_connect(gobj, att, 4, TRUE, "tk2", -1);
    mqtt_close(gobj, att);
    check_int(gobj, "TEST: records on disk after tk2 ended its session",
        records_on_disk(gobj), 1
    );

    /*
     *  Re-subscription replaces
     */
    mqtt_connect(gobj, att, 4, FALSE, "resub", -1);
    mqtt_subscribe(gobj, att, 1, "r/1", -1, 1);
    mqtt_subscribe(gobj, att, 2, "r/1", -1, 1);
    check_suback(gobj, att, "TEST: SUBACK of a re-subscription", 1, 0, 0);
    check_kept(gobj, ATT, "TEST: a re-subscription dropped the client");
    mqtt_close(gobj, att);

    mqtt_connect(gobj, att, 4, FALSE, "resub", -1);
    check_int(gobj, "TEST: CONNECT of resub, again", att->connack_reason, 0);
    mqtt_subscribe(gobj, att, 1, "r/1", -1, 1);
    check_suback(gobj, att, "TEST: SUBACK of a re-subscription after reconnecting", 1, 0, 0);
    check_kept(gobj, ATT, "TEST: a re-subscription after reconnecting dropped the client");
    check_int(gobj, "TEST: subscriptions of resub", subscriptions_of(gobj, "resub"), 1);
    mqtt_close(gobj, att);

    /*
     *  160 filters in two packets: 100
     */
    mqtt_connect(gobj, att, 4, FALSE, "many", -1);
    mqtt_subscribe(gobj, att, 1, "m/", 0, 80);
    check_suback(gobj, att, "TEST: SUBACK of the first 80", 80, 0, 0);
    mqtt_subscribe(gobj, att, 2, "m/", 80, 80);
    check_suback(gobj, att, "TEST: SUBACK past max_subscriptions_per_client", 20, 60, 0x80);
    mqtt_subscribe(gobj, att, 3, "m/", 0, 1);
    check_suback(gobj, att, "TEST: SUBACK of a re-subscription at the limit", 1, 0, 0);
    check_kept(gobj, ATT, "TEST: the limit of subscriptions dropped the client");
    check_int(gobj, "TEST: subscriptions of many",
        subscriptions_of(gobj, "many"), MAX_SUBSCRIPTIONS
    );
    mqtt_close(gobj, att);

    /*
     *  MQTT 5: 101 filters, the last one refused with Quota exceeded
     */
    mqtt_connect(gobj, att, 5, FALSE, "many5", 0xFFFFFFFF);
    check_int(gobj, "TEST: CONNECT of many5", att->connack_reason, 0);
    mqtt_subscribe(gobj, att, 1, "m5/", 0, MAX_SUBSCRIPTIONS + 1);
    check_suback(gobj, att, "TEST: SUBACK of MQTT 5 past max_subscriptions_per_client",
        MAX_SUBSCRIPTIONS, 1, 0x97
    );
    check_kept(gobj, ATT, "TEST: the limit of subscriptions dropped the MQTT 5 client");
    mqtt_close(gobj, att);
}

/***************************************************************************
 *  3. Persistent sessions up to the limit, and past it
 ***************************************************************************/
PRIVATE void phase_persistent_sessions(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    peer_t *att = &priv->peers[ATT];
    peer_t *att2 = &priv->peers[ATT2];

    check_dropped(gobj, ATT2, "TEST: the first connection of tk2 was not dropped");

    int persistent = records_on_disk(gobj);  // dev, resub, many, many5
    check_int(gobj, "TEST: persistent sessions before the flood", persistent, 4);

    int accepted = 0;
    for(int i=0; i<MAX_SESSIONS - persistent; i++) {
        char client_id[NAME_MAX];
        char prefix[NAME_MAX];
        snprintf(client_id, sizeof(client_id), "p-%04d", i);
        snprintf(prefix, sizeof(prefix), "p/%d/", i);
        mqtt_connect(gobj, att, 4, FALSE, client_id, -1);
        if(att->connack_reason != 0) {
            break;
        }
        accepted++;
        mqtt_subscribe(gobj, att, 1, prefix, 0, MAX_SUBSCRIPTIONS);
        mqtt_close(gobj, att);
    }
    check_int(gobj, "TEST: new persistent sessions accepted up to the limit",
        accepted, MAX_SESSIONS - persistent
    );
    check_kept(gobj, ATT, "TEST: a persistent session under the limit was dropped");

    mqtt_connect(gobj, att, 4, FALSE, "p-over-1", -1);
    check_int(gobj, "TEST: a persistent session past the limit, MQTT 3.1.1 (Server unavailable)",
        att->connack_reason, 3
    );

    mqtt_connect(gobj, att2, 4, TRUE, "c-new", -1);
    check_int(gobj, "TEST: a new clean session with the sessions full", att2->connack_reason, 0);
    mqtt_close(gobj, att2);

    mqtt_connect(gobj, att2, 4, FALSE, "resub", -1);
    check_int(gobj, "TEST: a session that exists, with the sessions full", att2->connack_reason, 0);
    check_int(gobj, "TEST: session present of resub", att2->connack_flags, 0);
    mqtt_close(gobj, att2);

    mqtt_connect(gobj, att2, 5, FALSE, "p-over-2", 3600);
    check_int(gobj, "TEST: a persistent session past the limit, MQTT 5 (Quota exceeded)",
        att2->connack_reason, 0x97
    );
}

/***************************************************************************
 *  4. Publications with the sessions full
 ***************************************************************************/
PRIVATE void phase_publications(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    peer_t *att = &priv->peers[ATT];

    check_dropped(gobj, ATT, "TEST: a persistent session past the limit was not dropped");
    check_dropped(gobj, ATT2, "TEST: an MQTT 5 persistent session past the limit was not dropped");
    check_int(gobj, "TEST: records in memory with the sessions full (and pub)",
        records_in_memory(gobj), MAX_SESSIONS + 1
    );
    check_int(gobj, "TEST: records on disk with the sessions full",
        records_on_disk(gobj), MAX_SESSIONS
    );

    uint64_t t = publish_to_dev(gobj);
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "TEST: publications, before and with the sessions full",
        "publications", "%d", PUBLICATIONS,
        "reference_ms", "%lu", (unsigned long)priv->reference_ms,
        "full_ms",      "%lu", (unsigned long)t,
        NULL
    );
    if(t > priv->reference_ms + PUBLISH_MARGIN_MS) {
        json_t *got = json_integer((json_int_t)t);
        json_t *expected = json_integer((json_int_t)(priv->reference_ms + PUBLISH_MARGIN_MS));
        test_error(gobj, "TEST: the publications cost more than the margin, ms", got, expected);
        JSON_DECREF(got)
        JSON_DECREF(expected)
    }

    mqtt_connect(gobj, att, 4, FALSE, "p-over-3", -1);
    check_int(gobj, "TEST: a third persistent session past the limit",
        att->connack_reason, 3
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
            phase_clean_sessions(gobj);
            set_timeout(priv->timer, 100);
            break;

        case 2:
            phase_subscriptions(gobj);
            set_timeout(priv->timer, 100);
            break;

        case 3:
            phase_persistent_sessions(gobj);
            set_timeout(priv->timer, 100);
            break;

        case 4:
            phase_publications(gobj);
            set_timeout(priv->timer, 100);
            break;

        default:
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "TEST: legacy sessions done",
                NULL
            );
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What a protocol writes, through its fake transport
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    peer_t *peer = 0;
    for(int i=0; i<N_PEERS; i++) {
        if(priv->peers[i].fake == src) {
            peer = &priv->peers[i];
            break;
        }
    }
    if(!peer) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: data of an unknown transport",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(gbuf) {
        gbuffer_append_gbuf(peer->rx, gbuf);
    }
    parse_rx(gobj, peer);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The protocols' output events: nothing to do
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
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_LEGACY_SESSIONS);

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
        {EV_ON_MESSAGE,         ac_prot_event,      0},
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
PUBLIC int register_c_legacy_sessions(void)
{
    return create_gclass(C_LEGACY_SESSIONS);
}
