/****************************************************************************
 *          C_LEGACY_SESSIONS.C
 *
 *          Driver of the test of the client records of C_PROT_MQTT, the
 *          deprecated MQTT protocol gclass, as a server. No broker and no
 *          socket: twenty-two protocol gobjs are children of this driver, each
 *          with a C_FAKE_TRANSPORT as its bottom, and the driver writes the
 *          clients' packets into them by hand. The clients are kept in a
 *          persistent C_RESOURCE2, as an MQTT gate keeps them. The
 *          protocol's limits are its defaults (100 subscriptions per
 *          client, 1000 persistent sessions, filters of 256 bytes, a
 *          session kept one day after its disconnection, 20 messages in
 *          flight, 1000 queued), but in the two protocols of the expiry,
 *          "exp" and "exp2", and in "asg" (allow_zero_length_clientid).
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
 *                 a session are still accepted; the CONNACK of the one that
 *                 has a session removes its "disconnected_t".
 *              4. The 1000 publications of 1 again, with the 1000 sessions
 *                 and their 100000 subscriptions: what they take is bounded
 *                 (within 3 s of the reference). A third new id is refused,
 *                 and says nothing more within the window of the WARNING.
 *                 A record as 7.30.22 wrote it (no "disconnected_t").
 *              5. 2.5 s later, a protocol whose persistent_session_expiry is
 *                 1 s: a new id with clean session 0 is accepted, the
 *                 sessions disconnected longer than that are removed, in
 *                 memory and on disk (the connected "dev" stays); the record
 *                 with no "disconnected_t" is dated, not removed. An MQTT 5
 *                 session with a Session Expiry Interval of 1 s is kept.
 *              6. 2.5 s later, a protocol with max_persistent_sessions 3 and
 *                 persistent_session_expiry 0 (never): the MQTT 5 session
 *                 expired by its own interval makes room for a new id; the
 *                 next one is refused, as a 3.1.1 session never expires.
 *                 50 persistent subscribers of "w/t" (QoS 1), disconnected,
 *                 and "dev" (connected) subscribed too: 200 QoS 1 PUBLISH
 *                 write nothing (the bytes the process writes), "dev" gets
 *                 them with the packet ids 1..200 (20 in flight, each PUBACK
 *                 sends the next).
 *                 A filter of 257 bytes is refused in the SUBACK (3.1.1
 *                 0x80, MQTT 5 0x8F), one of 256 is granted, the client is
 *                 kept; one WARNING each connection. An UNSUBSCRIBE of a
 *                 filter of 257 bytes that the client holds removes it.
 *                 Refused CONNECTs (keep alive 0) leave no record: a new id
 *                 with clean session 1, a new id with clean session 0, and a
 *                 clean session 0 or 1 that takes over a connected clean
 *                 session 1. The connection taken over, still open,
 *                 subscribes: granted, no error. The id taken over with
 *                 clean session 0 connects again, clean session 1, and goes
 *                 with no error.
 *                 A record as reloaded after an unclean stop (it says
 *                 connected, with no connection): a QoS 1 PUBLISH to its
 *                 subscription and its client's CONNECT log no error.
 *              7. The refused and taken over connections were dropped.
 *              8. A QoS 2 PUBLISH, again with DUP, then its PUBREL twice:
 *                 two PUBREC, two PUBCOMP, the subscriber gets it once.
 *                 20 QoS 2 with no PUBREL are kept, the 21st drops the
 *                 client (MQTT 5: DISCONNECT 0x93), one WARNING each.
 *                 A subscriber that does not ack gets 20 of 1100 QoS 1, 1000
 *                 wait, 80 are dropped (one WARNING); its PUBACKs bring the
 *                 1000, in order. A DISCONNECT of MQTT 5 with a Reason
 *                 String and no Session Expiry Interval: no error. An
 *                 assigned id gets QoS 1 messages with ids 1, 2, no error.
 *                 "saver" holds 50 filters of 200 bytes; 401 SUBSCRIBE and
 *                 UNSUBSCRIBE inside one second write nothing more.
 *              9. Past the second, a PINGREQ writes the last change.
 *             10. A re-subscription with the same options, a refused filter
 *                 and an UNSUBSCRIBE of a filter not held write nothing; an
 *                 UNSUBSCRIBE past the second is written at once, a SUBSCRIBE
 *                 inside it at the disconnection; the session comes back
 *                 with its 51 filters.
 *             11. "big" (message_size_limit 16, MQTT 5): a QoS 1 and a QoS 2
 *                 past it get PUBACK and PUBREC 0x95, one under it its
 *                 PUBACK, the client is kept. "rm5" (Receive Maximum 5)
 *                 that does not ack gets 5 of 1015 QoS 1, 1000 wait and 10
 *                 are dropped (one WARNING); one more per PUBACK. "rm0" (Receive Maximum 0): no CONNACK, dropped.
 *
 *          Up to 7.30.21 every client id made a record that was never
 *          removed, on disk and in memory, a client could hold any number
 *          of subscriptions, each PUBLISH walked every subscription of
 *          every record, and a re-subscription disconnected the client.
 *          Up to 7.30.22 a persistent session never expired, a topic filter
 *          had no limit of length, each QoS 1 PUBLISH rewrote the file of
 *          every subscriber with a session (and every packet id was 1), a
 *          refused CONNECT of a session left its record, the SUBSCRIBE of a
 *          connection taken over logged "client not found", and a record
 *          reloaded after an unclean stop logged an ERROR per message.
 *          Up to 7.30.23 a duplicate QoS 2 PUBLISH freed the message still
 *          waiting for its PUBREL (a crash), max_inflight_messages was not
 *          applied, every (UN)SUBSCRIBE rewrote the record, an UNSUBSCRIBE
 *          was capped by max_topic_filter_length, a DISCONNECT of MQTT 5
 *          with properties and no Session Expiry Interval logged an ERROR,
 *          and an assigned id logged "path NOT FOUND" at its first QoS 1;
 *          a PUBLISH past message_size_limit was freed twice, and the
 *          Receive Maximum of an MQTT 5 client was never kept.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
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
#define MAX_FILTER_LENGTH   256     // default max_topic_filter_length
#define W_SUBSCRIBERS       50
#define W_PUBLICATIONS      200
#define W_WRITE_MARGIN      (64*1024)   // bytes; 7.30.22 wrote ~2.5 MB
#define EXPIRY_WAIT_MS      2500
#define MAX_INFLIGHT        20      // default max_inflight_messages
#define MAX_QUEUED          1000    // default max_queued_messages
#define SLOW_PUBLICATIONS   1100
#define SAVE_FILTERS        50
#define SAVE_BURST          200
#define SAVE_WINDOW_WAIT_MS 1200    // past the window of one second
#define SAVE_NOOP_MARGIN    4096    // bytes; a record of saver is ~30 KB
#define BIG_LIMIT           16      // message_size_limit of "big"
#define RM_CLIENT           5       // Receive Maximum of "rm5"
#define RM_PUBLICATIONS     (RM_CLIENT + MAX_QUEUED + 10)

#define STORE_SERVICE       "mqtt"
#define STORE_DATABASE      "legacy_sessions_clients"

enum {
    DEV = 0,
    PUB,
    ATT,
    ATT2,
    EXP,        // persistent_session_expiry 1
    EXP2,       // persistent_session_expiry 0, max_persistent_sessions 3
    R1,
    R2,
    R3,
    R4,
    R5,
    LONGF,
    Q2,         // a QoS 2 PUBLISH twice, then its PUBREL
    IN4,        // QoS 2 unreleased past max_inflight_messages, MQTT 3.1.1
    IN5,        // the same, MQTT 5
    SLOW,       // a subscriber that does not ack
    DISC5,      // DISCONNECT of MQTT 5 with properties
    ASG,        // allow_zero_length_clientid 1: an assigned id
    SAVER,      // the writes of its (UN)SUBSCRIBEs
    BIG,        // message_size_limit 16
    RM5,        // MQTT 5, Receive Maximum 5
    RM0,        // MQTT 5, Receive Maximum 0
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
    json_t *suback_codes;   // the reason codes of the last SUBACK or UNSUBACK
    int publications;       // PUBLISH received
    int last_mid;           // packet id of the last PUBLISH QoS > 0 received
    int mids_out_of_order;  // PUBLISH QoS > 0 whose id is not the last one + 1
    int pubrecs;            // PUBREC received
    int pubcomps;           // PUBCOMP received
    int disconnect_reason;  // reason code of the last DISCONNECT received, -1 none
    int pubacks;            // PUBACK received
    int ack_reason;         // reason code of the last PUBACK or PUBREC, -1 none
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

    static const char *names[N_PEERS] = {
        "dev", "pub", "att", "att2", "exp", "exp2", "r1", "r2", "r3", "r4", "r5",
        "longf", "q2", "in4", "in5", "slow", "disc5", "asg", "saver",
        "big", "rm5", "rm0"
    };
    for(int i=0; i<N_PEERS; i++) {
        peer_t *peer = &priv->peers[i];
        peer->rx = gbuffer_create(64*1024, 64*1024);
        peer->suback_codes = json_array();
        peer->connack_flags = -1;
        peer->connack_reason = -1;
        peer->disconnect_reason = -1;
        peer->ack_reason = -1;

        json_t *kw_prot = json_pack("{s:b}", "iamServer", 1);
        if(i == EXP) {
            json_object_set_new(kw_prot, "persistent_session_expiry", json_integer(1));
        } else if(i == EXP2) {
            json_object_set_new(kw_prot, "persistent_session_expiry", json_integer(0));
            json_object_set_new(kw_prot, "max_persistent_sessions", json_integer(3));
        } else if(i == ASG) {
            json_object_set_new(kw_prot, "allow_zero_length_clientid", json_true());
        } else if(i == BIG) {
            json_object_set_new(kw_prot, "message_size_limit", json_integer(BIG_LIMIT));
        }

        char name[NAME_MAX];
        snprintf(name, sizeof(name), "prot_%s", names[i]);
        peer->prot = gobj_create(
            name,
            C_PROT_MQTT,
            kw_prot,
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
    if(len > 0) {   // an empty client id
        gbuffer_append(gbuf, (void *)s, len);
    }
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
 *  A new connection and its CONNECT, with a keep alive of `keepalive` s
 *  expiry: the Session Expiry Interval property of MQTT 5, -1 none
 ***************************************************************************/
PRIVATE void mqtt_connect_ka(
    hgobj gobj,
    peer_t *peer,
    int version,
    BOOL clean,
    const char *client_id,
    json_int_t expiry,
    uint16_t keepalive
) {
    peer->version = version;
    peer->connack_flags = -1;
    peer->connack_reason = -1;

    gobj_send_event(peer->prot, EV_CONNECTED, 0, peer->fake);

    gbuffer_t *body = gbuffer_create(256, 256);
    uint8_t head[] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T', (uint8_t)version,
        clean? 0x02 : 0x00,                     // flags
        (uint8_t)(keepalive >> 8), (uint8_t)(keepalive & 0xFF)
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
 *  A new connection and its CONNECT, keep alive 60 s
 ***************************************************************************/
PRIVATE void mqtt_connect(
    hgobj gobj,
    peer_t *peer,
    int version,
    BOOL clean,
    const char *client_id,
    json_int_t expiry
) {
    mqtt_connect_ka(gobj, peer, version, clean, client_id, expiry, 60);
}

/***************************************************************************
 *  A new connection and its CONNECT of MQTT 5, clean start, keep alive
 *  60 s, with the properties given (`props` starts with their length)
 ***************************************************************************/
PRIVATE void mqtt_connect5_props(
    hgobj gobj,
    peer_t *peer,
    const char *client_id,
    const uint8_t *props,
    size_t props_len
) {
    peer->version = 5;
    peer->connack_flags = -1;
    peer->connack_reason = -1;

    gobj_send_event(peer->prot, EV_CONNECTED, 0, peer->fake);

    gbuffer_t *body = gbuffer_create(256, 256);
    uint8_t head[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', 5, 0x02, 0x00, 60};
    gbuffer_append(body, head, sizeof(head));
    gbuffer_append(body, (void *)props, props_len);
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
 *  SUBSCRIBE (0x82) or UNSUBSCRIBE (0xA2) of the filters given, QoS 0
 ***************************************************************************/
PRIVATE void mqtt_filters(
    hgobj gobj,
    peer_t *peer,
    uint8_t type,
    uint16_t mid,
    const char **filters,
    int count
) {
    json_array_clear(peer->suback_codes);

    gbuffer_t *body = gbuffer_create(32*1024, 32*1024);
    uint8_t m[2] = {(uint8_t)(mid >> 8), (uint8_t)(mid & 0xFF)};
    gbuffer_append(body, m, 2);
    if(peer->version == 5) {
        uint8_t props[] = {0x00};
        gbuffer_append(body, props, sizeof(props));
    }
    for(int i=0; i<count; i++) {
        put_string(body, filters[i]);
        if(type == 0x82) {
            uint8_t options = 0;
            gbuffer_append(body, &options, 1);
        }
    }
    peer_sends(gobj, peer, type, body);
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
 *  PUBLISH QoS 2 with the packet id `mid`, DUP set or not
 ***************************************************************************/
PRIVATE void mqtt_publish_qos2(
    hgobj gobj,
    peer_t *peer,
    const char *topic,
    const char *payload,
    uint16_t mid,
    BOOL dup
) {
    gbuffer_t *body = gbuffer_create(256, 256);
    put_string(body, topic);
    uint8_t m[2] = {(uint8_t)(mid >> 8), (uint8_t)(mid & 0xFF)};
    gbuffer_append(body, m, 2);
    if(peer->version == 5) {
        uint8_t props[] = {0x00};
        gbuffer_append(body, props, sizeof(props));
    }
    gbuffer_append(body, (void *)payload, strlen(payload));
    peer_sends(gobj, peer, dup? 0x3C : 0x34, body);
}

/***************************************************************************
 *  A packet with only a packet id: PUBACK (0x40), PUBREL (0x62)
 ***************************************************************************/
PRIVATE void mqtt_ack(hgobj gobj, peer_t *peer, uint8_t type, uint16_t mid)
{
    gbuffer_t *body = gbuffer_create(2, 2);
    uint8_t m[2] = {(uint8_t)(mid >> 8), (uint8_t)(mid & 0xFF)};
    gbuffer_append(body, m, 2);
    peer_sends(gobj, peer, type, body);
}

/***************************************************************************
 *  PINGREQ
 ***************************************************************************/
PRIVATE void mqtt_pingreq(hgobj gobj, peer_t *peer)
{
    uint8_t pingreq[] = {0xC0, 0x00};   // no body: not through peer_sends()
    gbuffer_t *gbuf = gbuffer_create(sizeof(pingreq), sizeof(pingreq));
    gbuffer_append(gbuf, pingreq, sizeof(pingreq));
    gobj_send_event(
        peer->prot,
        EV_RX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),    // the kw owns it
        peer->fake
    );
}

/***************************************************************************
 *  The record of a client as it is on disk, NULL if it has no file
 ***************************************************************************/
PRIVATE json_t *record_on_disk(hgobj gobj, const char *client_id)
{
    char path_service[PATH_MAX];
    char dir[PATH_MAX];
    char filename[NAME_MAX];
    if(!build_path(path_service, sizeof(path_service),
            STORE_SERVICE, gobj_yuno_role_plus_name(), STORE_DATABASE, NULL) ||
            !yuneta_store_dir(dir, sizeof(dir), "resources", path_service, FALSE)) {
        return NULL;  // Error already logged
    }
    snprintf(filename, sizeof(filename), "%s.json", client_id);
    char path[PATH_MAX];
    if(!build_path(path, sizeof(path), dir, filename, NULL)) {
        return NULL;  // Error already logged
    }
    if(access(path, F_OK) != 0) {
        return NULL;
    }
    return load_json_from_file(gobj, dir, filename, 0);
}

/***************************************************************************
 *  Has the record of the client on disk this subscription?
 ***************************************************************************/
PRIVATE BOOL subscribed_on_disk(hgobj gobj, const char *client_id, const char *filter)
{
    json_t *record = record_on_disk(gobj, client_id);
    BOOL found = json_is_object(json_object_get(json_object_get(record, "subscriptions"), filter));
    JSON_DECREF(record)
    return found;
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
                if(((p[0] >> 1) & 0x03) && remaining >= 2) {
                    uint32_t tlen = ((uint32_t)v[0] << 8) | v[1];
                    if(2 + tlen + 2 <= remaining) {
                        int mid = (v[2 + tlen] << 8) | v[2 + tlen + 1];
                        if(mid != peer->last_mid + 1) {
                            peer->mids_out_of_order++;
                        }
                        peer->last_mid = mid;
                    }
                }
                break;
            case 4:     // PUBACK
                peer->pubacks++;
                peer->ack_reason = (remaining >= 3)? v[2] : 0;
                break;
            case 5:     // PUBREC
                peer->pubrecs++;
                peer->ack_reason = (remaining >= 3)? v[2] : 0;
                break;
            case 7:     // PUBCOMP
                peer->pubcomps++;
                break;
            case 14:    // DISCONNECT
                peer->disconnect_reason = (remaining >= 1)? v[0] : 0;
                break;
            case 9:     // SUBACK
            case 11:    // UNSUBACK
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
 *  The bytes this process has written so far, to files or anything else
 ***************************************************************************/
PRIVATE json_int_t bytes_written(hgobj gobj)
{
    FILE *f = fopen("/proc/self/io", "r");
    if(!f) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot open /proc/self/io",
            "errno",        "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    json_int_t wchar = -1;
    char line[256];
    while(fgets(line, sizeof(line), f)) {
        long long v;
        if(sscanf(line, "wchar: %lld", &v) == 1) {
            wchar = v;
            break;
        }
    }
    fclose(f);
    return wchar;
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
    json_t *resub = gobj_get_resource(priv->clients, "resub", 0, 0);
    check_int(gobj, "TEST: the record of resub, connected again, keeps disconnected_t",
        json_object_get(resub, "disconnected_t")? 1 : 0, 0
    );
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

    /*
     *  A record as 7.30.22 wrote it: disconnected, with no "disconnected_t"
     */
    json_t *legacy = gobj_create_resource(
        priv->clients,
        "legacy",
        json_pack("{s:s, s:b, s:i, s:b, s:{}}",
            "id", "legacy",
            "assigned_id", 0,
            "last_mid", 0,
            "isConnected", 0,
            "subscriptions"
        ),
        0
    );
    check_int(gobj, "TEST: the legacy record not created", legacy? 1 : 0, 1);
}

/***************************************************************************
 *  5. The sessions disconnected longer than persistent_session_expiry
 ***************************************************************************/
PRIVATE void phase_expiry(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    peer_t *att = &priv->peers[ATT];
    peer_t *exp = &priv->peers[EXP];

    check_dropped(gobj, ATT, "TEST: a third persistent session past the limit was not dropped");

    /*
     *  persistent_session_expiry 1 s: the sessions of 3 and before are gone
     */
    mqtt_connect(gobj, exp, 4, FALSE, "p-after", -1);
    check_int(gobj, "TEST: a new persistent session once the others expired",
        exp->connack_reason, 0
    );
    mqtt_close(gobj, exp);
    check_int(gobj, "TEST: records in memory after the expiry (dev, pub, p-after, legacy)",
        records_in_memory(gobj), 4
    );
    check_int(gobj, "TEST: records on disk after the expiry (dev, p-after, legacy)",
        records_on_disk(gobj), 3
    );

    /*
     *  The record with no date was dated by that pass, not removed; then
     *  its client ends the session
     */
    json_t *legacy = gobj_get_resource(priv->clients, "legacy", 0, 0);
    check_int(gobj, "TEST: the legacy record removed at its first pass",
        legacy? 1 : 0, 1
    );
    check_int(gobj, "TEST: the legacy record not dated by its first pass",
        json_integer_value(json_object_get(legacy, "disconnected_t")) > 0? 1 : 0, 1
    );
    mqtt_connect(gobj, att, 4, TRUE, "legacy", -1);
    check_int(gobj, "TEST: CONNECT of legacy, clean session 1", att->connack_reason, 0);
    mqtt_close(gobj, att);
    check_int(gobj, "TEST: records on disk after legacy ended its session",
        records_on_disk(gobj), 2
    );

    /*
     *  An MQTT 5 session that asks to be kept 1 s
     */
    mqtt_connect(gobj, att, 5, FALSE, "e5", 1);
    check_int(gobj, "TEST: CONNECT of e5", att->connack_reason, 0);
    mqtt_close(gobj, att);
    check_int(gobj, "TEST: records on disk with the session of e5",
        records_on_disk(gobj), 3
    );
}

/***************************************************************************
 *  6. The session's own expiry, the writes of a publication, the length of
 *  a filter, the refused CONNECTs
 ***************************************************************************/
PRIVATE void phase_records(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    peer_t *dev = &priv->peers[DEV];
    peer_t *pub = &priv->peers[PUB];
    peer_t *att = &priv->peers[ATT];
    peer_t *att2 = &priv->peers[ATT2];
    peer_t *exp2 = &priv->peers[EXP2];
    peer_t *r1 = &priv->peers[R1];
    peer_t *r2 = &priv->peers[R2];
    peer_t *r3 = &priv->peers[R3];
    peer_t *r4 = &priv->peers[R4];
    peer_t *r5 = &priv->peers[R5];
    peer_t *longf = &priv->peers[LONGF];

    /*
     *  max_persistent_sessions 3 (dev, p-after, e5), no expiry of its own:
     *  e5 expired by its Session Expiry Interval, p-after never expires
     */
    mqtt_connect(gobj, exp2, 4, FALSE, "e-new", -1);
    check_int(gobj, "TEST: a new persistent session once e5 expired by its own interval",
        exp2->connack_reason, 0
    );
    mqtt_close(gobj, exp2);
    check_int(gobj, "TEST: records on disk after e5 expired (dev, p-after, e-new)",
        records_on_disk(gobj), 3
    );
    mqtt_connect(gobj, exp2, 4, FALSE, "e-new2", -1);
    check_int(gobj, "TEST: a persistent session past 3, none expires",
        exp2->connack_reason, 3
    );

    /*
     *  QoS 1 publications to persistent subscribers, most of them
     *  disconnected: nothing is written, the connected one gets them in order
     */
    for(int i=0; i<W_SUBSCRIBERS; i++) {
        char client_id[NAME_MAX];
        snprintf(client_id, sizeof(client_id), "w-%d", i);
        mqtt_connect(gobj, att, 4, FALSE, client_id, -1);
        mqtt_subscribe_qos(gobj, att, 1, "w/t", -1, 1, 1);
        mqtt_close(gobj, att);
    }
    check_int(gobj, "TEST: records on disk with the subscribers of w/t",
        records_on_disk(gobj), 3 + W_SUBSCRIBERS
    );
    mqtt_subscribe_qos(gobj, dev, 2, "w/t", -1, 1, 1);
    check_suback(gobj, dev, "TEST: SUBACK QoS 1 of dev", 0, 1, 1);

    dev->publications = 0;
    dev->last_mid = 0;
    dev->mids_out_of_order = 0;
    json_int_t w0 = bytes_written(gobj);
    uint64_t t0 = time_in_milliseconds_monotonic();
    for(int i=0; i<W_PUBLICATIONS; i++) {
        mqtt_publish_qos(gobj, pub, "w/t", "on", (uint16_t)(i + 1));
    }
    for(int i=0; i<W_PUBLICATIONS; i++) {
        mqtt_ack(gobj, dev, 0x40, (uint16_t)(i + 1));   // each ack sends one more of the queue
    }
    uint64_t t = time_in_milliseconds_monotonic() - t0;
    json_int_t written = bytes_written(gobj) - w0;
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "TEST: QoS 1 publications to persistent subscribers",
        "subscribers",  "%d", W_SUBSCRIBERS,
        "publications", "%d", W_PUBLICATIONS,
        "ms",           "%lu", (unsigned long)t,
        "written",      "%lld", (long long)written,
        NULL
    );
    if(written > W_WRITE_MARGIN) {
        json_t *got = json_integer(written);
        json_t *expected = json_integer(W_WRITE_MARGIN);
        test_error(gobj, "TEST: the QoS 1 publications wrote the records, bytes", got, expected);
        JSON_DECREF(got)
        JSON_DECREF(expected)
    }
    if(t > priv->reference_ms + PUBLISH_MARGIN_MS) {
        json_t *got = json_integer((json_int_t)t);
        json_t *expected = json_integer((json_int_t)(priv->reference_ms + PUBLISH_MARGIN_MS));
        test_error(gobj, "TEST: the QoS 1 publications cost more than the margin, ms", got, expected);
        JSON_DECREF(got)
        JSON_DECREF(expected)
    }
    check_int(gobj, "TEST: the QoS 1 publications dev got",
        dev->publications, W_PUBLICATIONS
    );
    check_int(gobj, "TEST: the packet ids of dev not 1..200 in order",
        dev->mids_out_of_order, 0
    );
    check_int(gobj, "TEST: the last packet id of dev", dev->last_mid, W_PUBLICATIONS);

    /*
     *  The length of a topic filter
     */
    char long_filter[MAX_FILTER_LENGTH + 2];
    memset(long_filter, 'x', MAX_FILTER_LENGTH + 1);
    long_filter[MAX_FILTER_LENGTH + 1] = 0;
    char limit_filter[MAX_FILTER_LENGTH + 1];
    memset(limit_filter, 'y', MAX_FILTER_LENGTH);
    limit_filter[MAX_FILTER_LENGTH] = 0;

    mqtt_connect(gobj, longf, 4, TRUE, "longf", -1);
    const char *f1[] = {long_filter, "ok/1", limit_filter};
    mqtt_filters(gobj, longf, 0x82, 1, f1, 3);
    json_t *expected = json_pack("[i,i,i]", 0x80, 0, 0);
    if(!json_equal(longf->suback_codes, expected)) {
        test_error(gobj, "TEST: SUBACK of a filter past max_topic_filter_length",
            longf->suback_codes, expected
        );
    }
    JSON_DECREF(expected)
    const char *f2[] = {long_filter};
    mqtt_filters(gobj, longf, 0x82, 2, f2, 1);
    check_suback(gobj, longf, "TEST: SUBACK of a filter past the length, again", 0, 1, 0x80);
    check_int(gobj, "TEST: subscriptions of longf", subscriptions_of(gobj, "longf"), 2);
    check_kept(gobj, LONGF, "TEST: a filter past the length dropped the client");
    mqtt_close(gobj, longf);

    mqtt_connect(gobj, longf, 5, TRUE, "longf5", -1);
    mqtt_filters(gobj, longf, 0x82, 1, f2, 1);
    check_suback(gobj, longf, "TEST: SUBACK of MQTT 5 of a filter past the length", 0, 1, 0x8F);
    /*  A filter past the length that it holds (from 7.30.22, or a cap
     *  lowered since): the UNSUBSCRIBE removes it  */
    json_t *subs = json_object_get(gobj_get_resource(priv->clients, "longf5", 0, 0), "subscriptions");
    json_object_set_new(subs, long_filter, json_pack("{s:s, s:i, s:I, s:b, s:b}",
        "id", long_filter, "qos", 0, "identifier", (json_int_t)0,
        "no_local", 0, "retain_as_published", 0
    ));
    mqtt_filters(gobj, longf, 0xA2, 2, f2, 1);
    check_suback(gobj, longf, "TEST: UNSUBACK of MQTT 5 of a filter past the length it holds", 1, 0, 0);
    check_int(gobj, "TEST: subscriptions of longf5 after the UNSUBSCRIBE",
        subscriptions_of(gobj, "longf5"), 0
    );
    check_kept(gobj, LONGF, "TEST: a filter past the length dropped the MQTT 5 client");
    mqtt_close(gobj, longf);

    /*
     *  Refused CONNECTs (keep alive 0: CONNACK 2) leave no record
     */
    int in_memory = records_in_memory(gobj);
    int on_disk = records_on_disk(gobj);

    mqtt_connect_ka(gobj, r1, 4, TRUE, "r1", -1, 0);
    check_int(gobj, "TEST: CONNECT of r1 (keep alive 0)", r1->connack_reason, 2);
    mqtt_connect_ka(gobj, r2, 4, FALSE, "r0", -1, 0);
    check_int(gobj, "TEST: CONNECT of r0 (keep alive 0)", r2->connack_reason, 2);
    check_int(gobj, "TEST: records in memory after the refused CONNECTs of new ids",
        records_in_memory(gobj), in_memory
    );
    check_int(gobj, "TEST: records on disk after the refused CONNECTs of new ids",
        records_on_disk(gobj), on_disk
    );

    mqtt_connect(gobj, att, 4, TRUE, "vp", -1);
    check_int(gobj, "TEST: CONNECT of vp", att->connack_reason, 0);
    mqtt_connect_ka(gobj, att2, 4, FALSE, "vp", -1, 0);
    check_int(gobj, "TEST: CONNECT of vp with clean session 0 (keep alive 0)",
        att2->connack_reason, 2
    );
    check_int(gobj, "TEST: records in memory after the refused take over of vp",
        records_in_memory(gobj), in_memory
    );
    check_int(gobj, "TEST: records on disk after the refused take over of vp",
        records_on_disk(gobj), on_disk
    );

    /*  The connection taken over, its drop not arrived yet  */
    mqtt_subscribe(gobj, att, 3, "race/1", -1, 1);
    check_suback(gobj, att, "TEST: SUBACK of the connection of vp taken over", 1, 0, 0);

    mqtt_connect(gobj, r3, 4, TRUE, "vq", -1);
    check_int(gobj, "TEST: CONNECT of vq", r3->connack_reason, 0);
    mqtt_connect_ka(gobj, r4, 4, TRUE, "vq", -1, 0);
    check_int(gobj, "TEST: CONNECT of vq again (keep alive 0)", r4->connack_reason, 2);
    mqtt_subscribe(gobj, r3, 1, "race/2", -1, 1);
    check_suback(gobj, r3, "TEST: SUBACK of the connection of vq taken over", 1, 0, 0);
    check_int(gobj, "TEST: records in memory after the refused take over of vq",
        records_in_memory(gobj), in_memory
    );

    mqtt_connect(gobj, r5, 4, TRUE, "vp", -1);
    check_int(gobj, "TEST: CONNECT of vp again", r5->connack_reason, 0);
    mqtt_close(gobj, r5);
    check_int(gobj, "TEST: records in memory after vp went",
        records_in_memory(gobj), in_memory
    );
    check_int(gobj, "TEST: records on disk after vp went",
        records_on_disk(gobj), on_disk
    );

    /*
     *  A record as it is reloaded from disk after an unclean stop: written
     *  at its CONNACK, it says connected, and has no connection
     */
    json_t *stale = gobj_create_resource(
        priv->clients,
        "stale",
        json_pack("{s:s, s:b, s:i, s:b, s:{s:{s:s, s:i, s:I, s:b, s:b}}}",
            "id", "stale",
            "assigned_id", 0,
            "last_mid", 0,
            "isConnected", 1,
            "subscriptions",
                "z/t", "id", "z/t", "qos", 1, "identifier", (json_int_t)0,
                    "no_local", 0, "retain_as_published", 0
        ),
        0
    );
    check_int(gobj, "TEST: the stale record not created", stale? 1 : 0, 1);
    mqtt_publish_qos(gobj, pub, "z/t", "on", 1);
    mqtt_connect(gobj, r5, 4, FALSE, "stale", -1);
    check_int(gobj, "TEST: CONNECT of the stale record", r5->connack_reason, 0);
    mqtt_close(gobj, r5);
}

/***************************************************************************
 *  7. The connections refused or taken over were dropped
 ***************************************************************************/
PRIVATE void phase_drops(hgobj gobj)
{
    check_dropped(gobj, EXP2, "TEST: the session past 3 was not dropped");
    check_dropped(gobj, R1, "TEST: r1 (keep alive 0) was not dropped");
    check_dropped(gobj, R2, "TEST: r0 (keep alive 0) was not dropped");
    check_dropped(gobj, ATT, "TEST: the connection of vp taken over was not dropped");
    check_dropped(gobj, ATT2, "TEST: vp (keep alive 0) was not dropped");
    check_dropped(gobj, R3, "TEST: the connection of vq taken over was not dropped");
    check_dropped(gobj, R4, "TEST: vq (keep alive 0) was not dropped");
    check_kept(gobj, R5, "TEST: vp again was dropped");
    check_kept(gobj, LONGF, "TEST: longf was dropped");
}

/***************************************************************************
 *  The long prefix of the filters of saver: ~200 bytes each
 ***************************************************************************/
PRIVATE const char *saver_prefix(void)
{
    static char prefix[200];
    if(!prefix[0]) {
        memset(prefix, 's', sizeof(prefix) - 2);
        prefix[sizeof(prefix) - 2] = '/';
        prefix[sizeof(prefix) - 1] = 0;
    }
    return prefix;
}

/***************************************************************************
 *  8. A QoS 2 PUBLISH twice; QoS 2 unreleased past max_inflight_messages;
 *  a subscriber that does not ack; the DISCONNECT of MQTT 5 with
 *  properties; an assigned id; a burst of (UN)SUBSCRIBE
 ***************************************************************************/
PRIVATE void phase_flight(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    peer_t *dev = &priv->peers[DEV];
    peer_t *pub = &priv->peers[PUB];
    peer_t *q2 = &priv->peers[Q2];
    peer_t *in4 = &priv->peers[IN4];
    peer_t *in5 = &priv->peers[IN5];
    peer_t *slow = &priv->peers[SLOW];
    peer_t *disc5 = &priv->peers[DISC5];
    peer_t *asg = &priv->peers[ASG];
    peer_t *saver = &priv->peers[SAVER];

    /*
     *  The same QoS 2 PUBLISH twice (the second with DUP), then its PUBREL:
     *  the subscriber gets it once
     */
    mqtt_subscribe(gobj, dev, 3, "q2/t", -1, 1);
    check_suback(gobj, dev, "TEST: SUBACK of dev to q2/t", 1, 0, 0);
    mqtt_connect(gobj, q2, 4, TRUE, "q2", -1);
    check_int(gobj, "TEST: CONNECT of q2", q2->connack_reason, 0);
    dev->publications = 0;
    mqtt_publish_qos2(gobj, q2, "q2/t", "twice", 7, FALSE);
    mqtt_publish_qos2(gobj, q2, "q2/t", "twice", 7, TRUE);
    check_int(gobj, "TEST: PUBREC of the QoS 2 PUBLISH and of its duplicate", q2->pubrecs, 2);
    mqtt_ack(gobj, q2, 0x62, 7);
    check_int(gobj, "TEST: PUBCOMP of the PUBREL", q2->pubcomps, 1);
    check_int(gobj, "TEST: the QoS 2 message dev got", dev->publications, 1);
    mqtt_ack(gobj, q2, 0x62, 7);
    check_int(gobj, "TEST: PUBCOMP of the PUBREL again", q2->pubcomps, 2);
    check_int(gobj, "TEST: the QoS 2 message dev got, after its PUBREL again", dev->publications, 1);
    check_kept(gobj, Q2, "TEST: the duplicate QoS 2 dropped the client");
    mqtt_close(gobj, q2);

    /*
     *  QoS 2 waiting for their PUBREL: max_inflight_messages of them, and
     *  one more is a protocol error, the client is dropped
     */
    mqtt_connect(gobj, in4, 4, TRUE, "in4", -1);
    for(int i=0; i<MAX_INFLIGHT; i++) {
        mqtt_publish_qos2(gobj, in4, "in/t", "x", (uint16_t)(i + 1), FALSE);
    }
    check_int(gobj, "TEST: PUBREC of the QoS 2 up to max_inflight_messages", in4->pubrecs, MAX_INFLIGHT);
    check_kept(gobj, IN4, "TEST: QoS 2 up to max_inflight_messages dropped the client");
    mqtt_publish_qos2(gobj, in4, "in/t", "x", MAX_INFLIGHT + 1, FALSE);
    check_int(gobj, "TEST: PUBREC of a QoS 2 past max_inflight_messages", in4->pubrecs, MAX_INFLIGHT);
    check_dropped(gobj, IN4, "TEST: a QoS 2 past max_inflight_messages did not drop the client");

    mqtt_connect(gobj, in5, 5, TRUE, "in5", -1);
    for(int i=0; i<MAX_INFLIGHT + 1; i++) {
        mqtt_publish_qos2(gobj, in5, "in/t", "x", (uint16_t)(i + 1), FALSE);
    }
    check_int(gobj, "TEST: DISCONNECT of MQTT 5 past the Receive Maximum (0x93)",
        in5->disconnect_reason, 0x93
    );
    check_dropped(gobj, IN5, "TEST: a QoS 2 past the Receive Maximum did not drop the MQTT 5 client");

    /*
     *  A subscriber that does not ack: max_inflight_messages in flight,
     *  max_queued_messages queued, the rest dropped; each ack sends the next
     */
    mqtt_connect(gobj, slow, 4, TRUE, "slow", -1);
    mqtt_subscribe_qos(gobj, slow, 1, "s/t", -1, 1, 1);
    check_suback(gobj, slow, "TEST: SUBACK QoS 1 of slow", 0, 1, 1);
    for(int i=0; i<SLOW_PUBLICATIONS; i++) {
        mqtt_publish_qos(gobj, pub, "s/t", "x", (uint16_t)(i + 1));
    }
    check_int(gobj, "TEST: messages in flight to a subscriber that does not ack",
        slow->publications, MAX_INFLIGHT
    );
    for(int i=0; i<MAX_INFLIGHT + MAX_QUEUED + 5; i++) {
        mqtt_ack(gobj, slow, 0x40, (uint16_t)(i + 1));
    }
    check_int(gobj, "TEST: messages slow got, after acking them",
        slow->publications, MAX_INFLIGHT + MAX_QUEUED
    );
    check_int(gobj, "TEST: the packet ids of slow not in order", slow->mids_out_of_order, 0);
    check_kept(gobj, SLOW, "TEST: a subscriber that does not ack was dropped");
    mqtt_close(gobj, slow);

    /*
     *  DISCONNECT of MQTT 5 with a property and no Session Expiry Interval
     */
    mqtt_connect(gobj, disc5, 5, TRUE, "disc5", -1);
    check_int(gobj, "TEST: CONNECT of disc5", disc5->connack_reason, 0);
    gbuffer_t *body = gbuffer_create(16, 16);
    uint8_t disconnect[] = {0x00, 0x06, 0x1F, 0x00, 0x03, 'b', 'y', 'e'};  // reason string
    gbuffer_append(body, disconnect, sizeof(disconnect));
    peer_sends(gobj, disc5, 0xE0, body);
    check_dropped(gobj, DISC5, "TEST: the DISCONNECT of MQTT 5 did not drop the client");

    /*
     *  An assigned id gets QoS 1 messages
     */
    mqtt_connect(gobj, asg, 4, TRUE, "", -1);
    check_int(gobj, "TEST: CONNECT with an empty client id", asg->connack_reason, 0);
    mqtt_subscribe_qos(gobj, asg, 1, "asg/t", -1, 1, 1);
    check_suback(gobj, asg, "TEST: SUBACK QoS 1 of the assigned id", 0, 1, 1);
    mqtt_publish_qos(gobj, pub, "asg/t", "on", 1);
    mqtt_publish_qos(gobj, pub, "asg/t", "on", 2);
    check_int(gobj, "TEST: the QoS 1 messages of the assigned id", asg->publications, 2);
    check_int(gobj, "TEST: the last packet id of the assigned id", asg->last_mid, 2);
    mqtt_close(gobj, asg);

    /*
     *  A session with SAVE_FILTERS filters of ~200 bytes, then a burst of
     *  SUBSCRIBE and UNSUBSCRIBE inside the window of one second: nothing
     *  more is written; the last change waits
     */
    mqtt_connect(gobj, saver, 4, FALSE, "saver", -1);
    check_int(gobj, "TEST: CONNECT of saver", saver->connack_reason, 0);
    mqtt_subscribe(gobj, saver, 1, saver_prefix(), 0, SAVE_FILTERS);
    char filter0[NAME_MAX];
    snprintf(filter0, sizeof(filter0), "%s0", saver_prefix());
    check_int(gobj, "TEST: the first SUBSCRIBE of saver not written at once",
        subscribed_on_disk(gobj, "saver", filter0)? 1 : 0, 1
    );
    const char *fx[] = {"sv/x"};
    const char *flast[] = {"sv/last"};
    json_int_t w0 = bytes_written(gobj);
    for(int i=0; i<SAVE_BURST; i++) {
        mqtt_filters(gobj, saver, 0x82, (uint16_t)(2*i + 2), fx, 1);
        mqtt_filters(gobj, saver, 0xA2, (uint16_t)(2*i + 3), fx, 1);
    }
    mqtt_filters(gobj, saver, 0x82, 1, flast, 1);
    json_int_t written = bytes_written(gobj) - w0;
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "TEST: a burst of SUBSCRIBE and UNSUBSCRIBE",
        "packets",      "%d", 2*SAVE_BURST + 1,
        "written",      "%lld", (long long)written,
        NULL
    );
    if(written > W_WRITE_MARGIN) {
        json_t *got = json_integer(written);
        json_t *expected = json_integer(W_WRITE_MARGIN);
        test_error(gobj, "TEST: a burst of (UN)SUBSCRIBE wrote the record, bytes", got, expected);
        JSON_DECREF(got)
        JSON_DECREF(expected)
    }
    check_int(gobj, "TEST: a change inside the window written at once",
        subscribed_on_disk(gobj, "saver", "sv/last")? 1 : 0, 0
    );
}

/***************************************************************************
 *  9. Past the window: the next frame writes the change
 ***************************************************************************/
PRIVATE void phase_save_window(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    peer_t *saver = &priv->peers[SAVER];

    mqtt_pingreq(gobj, saver);
    check_int(gobj, "TEST: the change of saver not written by the first frame past the window",
        subscribed_on_disk(gobj, "saver", "sv/last")? 1 : 0, 1
    );
}

/***************************************************************************
 *  10. What changes nothing writes nothing; the disconnection writes the
 *  last change; the session comes back with its subscriptions
 ***************************************************************************/
PRIVATE void phase_save_noop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    peer_t *saver = &priv->peers[SAVER];

    char long_filter[MAX_FILTER_LENGTH + 2];
    memset(long_filter, 'x', MAX_FILTER_LENGTH + 1);
    long_filter[MAX_FILTER_LENGTH + 1] = 0;
    const char *flast[] = {"sv/last"};
    const char *flong[] = {long_filter};
    const char *fnone[] = {"sv/none"};
    const char *ffinal[] = {"sv/final"};

    json_int_t w0 = bytes_written(gobj);
    mqtt_filters(gobj, saver, 0x82, 1, flast, 1);   // the same filter, the same options
    check_suback(gobj, saver, "TEST: SUBACK of the same subscription", 1, 0, 0);
    mqtt_filters(gobj, saver, 0x82, 2, flong, 1);   // refused
    check_suback(gobj, saver, "TEST: SUBACK of a filter past the length", 0, 1, 0x80);
    mqtt_filters(gobj, saver, 0xA2, 3, fnone, 1);   // not held
    mqtt_pingreq(gobj, saver);
    json_int_t written = bytes_written(gobj) - w0;
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "TEST: (UN)SUBSCRIBE that change nothing",
        "written",      "%lld", (long long)written,
        NULL
    );
    if(written > SAVE_NOOP_MARGIN) {
        json_t *got = json_integer(written);
        json_t *expected = json_integer(SAVE_NOOP_MARGIN);
        test_error(gobj, "TEST: (UN)SUBSCRIBE that change nothing wrote the record, bytes", got, expected);
        JSON_DECREF(got)
        JSON_DECREF(expected)
    }

    mqtt_filters(gobj, saver, 0xA2, 4, flast, 1);   // written at once: out of the window
    check_int(gobj, "TEST: an UNSUBSCRIBE past the window not written at once",
        subscribed_on_disk(gobj, "saver", "sv/last")? 1 : 0, 0
    );
    mqtt_filters(gobj, saver, 0x82, 5, ffinal, 1);  // inside the window: waits
    check_int(gobj, "TEST: a SUBSCRIBE inside the window written at once",
        subscribed_on_disk(gobj, "saver", "sv/final")? 1 : 0, 0
    );
    mqtt_close(gobj, saver);
    check_int(gobj, "TEST: the disconnection did not write the last change",
        subscribed_on_disk(gobj, "saver", "sv/final")? 1 : 0, 1
    );
    json_t *record = record_on_disk(gobj, "saver");
    check_int(gobj, "TEST: the subscriptions of saver on disk",
        (json_int_t)json_object_size(json_object_get(record, "subscriptions")), SAVE_FILTERS + 1
    );
    JSON_DECREF(record)

    mqtt_connect(gobj, saver, 4, FALSE, "saver", -1);
    check_int(gobj, "TEST: CONNECT of saver again", saver->connack_reason, 0);
    check_int(gobj, "TEST: the subscriptions of saver after reconnecting",
        subscriptions_of(gobj, "saver"), SAVE_FILTERS + 1
    );
    mqtt_close(gobj, saver);
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  11. message_size_limit; the Receive Maximum of an MQTT 5 client
 ***************************************************************************/
PRIVATE void phase_client_limits(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    peer_t *pub = &priv->peers[PUB];
    peer_t *big = &priv->peers[BIG];
    peer_t *rm5 = &priv->peers[RM5];
    peer_t *rm0 = &priv->peers[RM0];

    /*
     *  A PUBLISH past message_size_limit is answered 0x95 (Packet too
     *  large), not delivered, and the client stays
     */
    char payload[2*BIG_LIMIT + 1];
    memset(payload, 'p', sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = 0;
    mqtt_connect(gobj, big, 5, TRUE, "big", -1);
    check_int(gobj, "TEST: CONNECT of big", big->connack_reason, 0);
    mqtt_publish_qos(gobj, big, "big/t", payload, 1);
    check_int(gobj, "TEST: PUBACK of a QoS 1 past message_size_limit", big->pubacks, 1);
    check_int(gobj, "TEST: reason of the PUBACK past message_size_limit", big->ack_reason, 0x95);
    mqtt_publish_qos2(gobj, big, "big/t", payload, 2, FALSE);
    check_int(gobj, "TEST: PUBREC of a QoS 2 past message_size_limit", big->pubrecs, 1);
    check_int(gobj, "TEST: reason of the PUBREC past message_size_limit", big->ack_reason, 0x95);
    mqtt_publish_qos(gobj, big, "big/t", "small", 3);
    check_int(gobj, "TEST: PUBACK of a QoS 1 under message_size_limit", big->pubacks, 2);
    check_kept(gobj, BIG, "TEST: a PUBLISH past message_size_limit dropped the client");
    mqtt_close(gobj, big);

    /*
     *  An MQTT 5 client with Receive Maximum 5 that does not ack: 5 in
     *  flight, the rest queued; a PUBACK sends one more
     */
    uint8_t rm5_props[] = {0x03, 0x21, 0x00, RM_CLIENT};
    mqtt_connect5_props(gobj, rm5, "rm5", rm5_props, sizeof(rm5_props));
    check_int(gobj, "TEST: CONNECT of rm5", rm5->connack_reason, 0);
    mqtt_subscribe_qos(gobj, rm5, 1, "rm/t", -1, 1, 1);
    check_suback(gobj, rm5, "TEST: SUBACK QoS 1 of rm5", 0, 1, 1);
    for(int i=0; i<RM_PUBLICATIONS; i++) {
        mqtt_publish_qos(gobj, pub, "rm/t", "x", (uint16_t)(i + 1));
    }
    check_int(gobj, "TEST: messages in flight to a client with Receive Maximum 5",
        rm5->publications, RM_CLIENT
    );
    mqtt_ack(gobj, rm5, 0x40, 1);
    check_int(gobj, "TEST: messages to a client with Receive Maximum 5 after a PUBACK",
        rm5->publications, RM_CLIENT + 1
    );
    for(int i=1; i<RM_PUBLICATIONS; i++) {
        mqtt_ack(gobj, rm5, 0x40, (uint16_t)(i + 1));
    }
    check_int(gobj, "TEST: messages a client with Receive Maximum 5 got, its queue past max_queued_messages",
        rm5->publications, RM_CLIENT + MAX_QUEUED
    );
    check_kept(gobj, RM5, "TEST: the client with Receive Maximum 5 was dropped");
    mqtt_close(gobj, rm5);

    /*
     *  Receive Maximum 0 is a protocol error: no CONNACK, dropped
     */
    uint8_t rm0_props[] = {0x03, 0x21, 0x00, 0x00};
    mqtt_connect5_props(gobj, rm0, "rm0", rm0_props, sizeof(rm0_props));
    check_int(gobj, "TEST: a CONNACK to a CONNECT with Receive Maximum 0", rm0->connack_reason, -1);
    check_dropped(gobj, RM0, "TEST: a CONNECT with Receive Maximum 0 was not dropped");
}

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
            set_timeout(priv->timer, EXPIRY_WAIT_MS);
            break;

        case 5:
            phase_expiry(gobj);
            set_timeout(priv->timer, EXPIRY_WAIT_MS);
            break;

        case 6:
            phase_records(gobj);
            set_timeout(priv->timer, 100);
            break;

        case 7:
            phase_drops(gobj);
            set_timeout(priv->timer, 100);
            break;

        case 8:
            phase_flight(gobj);
            set_timeout(priv->timer, SAVE_WINDOW_WAIT_MS);
            break;

        case 9:
            phase_save_window(gobj);
            set_timeout(priv->timer, SAVE_WINDOW_WAIT_MS);
            break;

        case 10:
            phase_save_noop(gobj);
            set_timeout(priv->timer, 100);
            break;

        case 11:
            phase_client_limits(gobj);
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
