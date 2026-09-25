/***********************************************************************
 *          C_TEST_PEER_SUBS.C
 *
 *          GClasses to test what a remote peer may put in a subscription,
 *          and what it may hold (security review 19).
 *
 *          C_TEST_PEER_SINK: a local subscriber, keeps what it gets under
 *          its name.
 *
 *          C_TEST_PEER_PUB (service `publisher`) publishes:
 *              EV_TEST_FEED    {topic_name, node, secret}
 *              EV_TEST_OPEN    {what}
 *              EV_TEST_SECRET  EVF_AUTHZ_SUBSCRIBE: only `bob` may
 *
 *          C_TEST_PEER_SUBS (service `tester`) drives the test through two
 *          C_IEVENT_CLI connected by websocket to the C_IEVENT_SRV gate,
 *          whose channels allow 4 subscriptions of 512 bytes at most.
 *          `alice` writes her frames by hand, as a hostile peer would;
 *          `bob` is a plain client.
 *
 *              1. The subscribers of EV_TEST_FEED, in this order:
 *                  - `strip`, local, with __local__ {secret} and
 *                    __global__ {sink: strip}
 *                  - `alice`, with __global__ {topic_name: users, node:
 *                    FORGED, __md_iev__, __service__}, __local__ {secret}
 *                    and a key that is none of them
 *                  - `bob`, with __filter__ {topic_name: devices} and
 *                    __global__ {tag: bob}
 *                  - `local`, local, plain
 *                 and `alice` subscribes EV_TEST_OPEN with __global__
 *                 {gbuffer: 1}.
 *                 The publisher publishes {topic_name: devices, node: dev1,
 *                 secret}. Each one gets what IT asked, and nobody else's:
 *                 `bob` and `local` the event whole (up to 7.25.4 the
 *                 publish shared one kw, and `bob` got nothing -- alice's
 *                 topic_name failed his filter -- and `local` got alice's
 *                 forgery without the secret, and her __md_iev__), `alice`
 *                 her keys but not the framework's, and the secret (a peer
 *                 may not remove keys), and the publisher's kw is left as it
 *                 was. The keys dropped are logged once.
 *              2. `alice` holds 2; she asks 4 more of EV_TEST_OPEN: 2 are
 *                 accepted, 2 refused (max_subscriptions, logged once), 2
 *                 with a __filter__ over 512 bytes are refused (logged
 *                 once); she withdraws 5 times what she never had, with a
 *                 2 KB key (logged once, capped); she asks EV_TEST_SECRET 3
 *                 times (refused, logged once). Up to 7.25.4 there was no
 *                 cap, and each of these frames wrote its log line, as big
 *                 as the peer wanted.
 *              3. `bob` withdraws his subscription, with its __global__: it
 *                 goes (up to 7.25.4 the server compared it with the one it
 *                 had stored, which carries the back-metadata of the gate,
 *                 and it stayed until the channel closed).
 *              3b. `alice` sends a command, a stats request and an event,
 *                 and a subscription, each with its own `__username__`
 *                 ("admin") and, in the routing stack, `__username__`,
 *                 `input_channel` and `input_service`: the service sees the
 *                 gate's values, never the peer's (up to 7.25.4
 *                 kw_set_dict_value() kept a key that was already there, so
 *                 the peer's `__username__` reached command_parser's authz).
 *              4. The publisher publishes EV_TEST_OPEN: `alice`'s
 *                 subscription without filter gets it, without the
 *                 `gbuffer` she set (up to 7.25.4 the gate took her integer
 *                 for a gbuffer when it serialized the event: a crash).
 *
 *          A wrong result is logged as an error, which the expected-logs
 *          check of main.c does not expect.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "c_test_peer_subs.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_raw_iev(
    hgobj gobj,
    hgobj cli,
    const char *msg_type,
    gobj_event_t event,
    json_t *kw      // owned
);
PRIVATE int check(hgobj gobj, const char *what, BOOL ok);
PRIVATE json_t *received_one(hgobj gobj, const char *name);
PRIVATE int count_subscriptions_of(hgobj gobj, hgobj publisher, const char *username);
PRIVATE void record_feed(const char *name, json_t *kw);
PRIVATE void check_stamped(hgobj gobj, const char *what);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
int test_peer_subs_failed = 0;
PRIVATE json_t *received = 0;   // subscriber name -> [kw, ...] of EV_TEST_FEED
PRIVATE json_t *stamped = 0;    // "command", "stats", "event" -> the kw the service saw

GOBJ_DEFINE_EVENT(EV_TEST_FEED);
GOBJ_DEFINE_EVENT(EV_TEST_OPEN);
GOBJ_DEFINE_EVENT(EV_TEST_SECRET);
GOBJ_DEFINE_EVENT(EV_TEST_EMIT);
GOBJ_DEFINE_EVENT(EV_TEST_PEER_MSG);

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      Authz of the publisher: its `read` is also what a subscription asks
 *---------------------------------------------*/
PRIVATE const char *read_alias[] = {"__subscribe_event__", 0};

PRIVATE sdata_desc_t pub_authz_table[] = {
/*-AUTHZ-- type---------name------------flag----alias-------items---description--*/
SDATAAUTHZ (DTP_SCHEMA, "read",         0,      read_alias, 0,      "Permission to read the feed"),
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
    hgobj publisher;
    hgobj cli_alice;
    hgobj cli_bob;
    int opened;
    int step;
    int opens_to_alice;
    hgobj sink_strip;
    hgobj sink_local;
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
    priv->sink_strip = gobj_create_pure_child("strip", C_TEST_PEER_SINK, 0, gobj);
    priv->sink_local = gobj_create_pure_child("local", C_TEST_PEER_SINK, 0, gobj);
    received = json_object();
    stamped = json_object();
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    JSON_DECREF(received)
    JSON_DECREF(stamped)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->sink_strip);
    gobj_start(priv->sink_local);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
    gobj_stop(priv->sink_strip);
    gobj_stop(priv->sink_local);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->publisher = gobj_find_service("publisher", TRUE);
    priv->cli_alice = gobj_find_service("cli_alice", TRUE);
    priv->cli_bob = gobj_find_service("cli_bob", TRUE);

    gobj_write_bool_attr(gobj_yuno(), "enable_subscription_authz", TRUE);

    /*
     *  The first subscriber: removes `secret` and adds `sink` in ITS event
     */
    gobj_subscribe_event(
        priv->publisher,
        EV_TEST_FEED,
        json_pack("{s:{s:i}, s:{s:s}}",
            "__local__", "secret", 1,
            "__global__", "sink", "strip"
        ),
        priv->sink_strip
    );

    /*
     *  All events (event NULL) is a local subscription: C_IEVENT_CLI sends
     *  only explicit events to the remote side
     */
    gobj_subscribe_event(gobj_find_service("__input_side__", TRUE), NULL, 0, gobj);
    gobj_subscribe_event(priv->cli_alice, NULL, 0, gobj);
    gobj_subscribe_event(priv->cli_bob, NULL, 0, gobj);
    gobj_start_tree(priv->cli_alice);
    gobj_start_tree(priv->cli_bob);

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
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  A frame of a peer, written by hand: what a hostile client can send, and
 *  C_IEVENT_CLI never would (a __local__, a key that is none of the
 *  subscription's, a repeated withdrawal of nothing).
 ***************************************************************************/
PRIVATE int send_raw_iev(
    hgobj gobj,
    hgobj cli,
    const char *msg_type,
    gobj_event_t event,
    json_t *kw      // owned
)
{
    json_t *jn_ievent_id = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "dst_yuno", "",
        "dst_role", "",
        "dst_service", "publisher",
        "src_yuno", gobj_yuno_name(),
        "src_role", gobj_yuno_role(),
        "src_service", gobj_name(gobj),
        "user", "",
        "host", ""
    );
    /*
     *  What the gate stamps, forged: it must overwrite them
     */
    json_object_set_new(jn_ievent_id, "__username__", json_string("admin"));
    json_object_set_new(jn_ievent_id, "input_channel", json_string("forged"));
    json_object_set_new(jn_ievent_id, "input_service", json_string("forged"));
    msg_iev_push_stack(gobj, kw, IEVENT_STACK_ID, jn_ievent_id);
    msg_iev_set_msg_type(gobj, kw, msg_type);

    gbuffer_t *gbuf = iev_create_to_gbuffer(gobj, event, kw);
    if(!gbuf) {
        // Error already logged
        return -1;
    }
    return gobj_send_event(
        gobj_bottom_gobj(cli),
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        cli
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int check(hgobj gobj, const char *what, BOOL ok)
{
    if(!ok) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL",
            "what",         "%s", what,
            NULL
        );
        test_peer_subs_failed++;
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  The one EV_TEST_FEED that `name` got (NULL and a failure if not one)
 ***************************************************************************/
PRIVATE json_t *received_one(hgobj gobj, const char *name)
{
    json_t *jn_list = json_object_get(received, name);
    if(json_array_size(jn_list) != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: not one EV_TEST_FEED",
            "subscriber",   "%s", name,
            "received",     "%j", received,
            NULL
        );
        test_peer_subs_failed++;
        return NULL;
    }
    return json_array_get(jn_list, 0);
}

/***************************************************************************
 *  The kw a service got from `alice` carries the gate's stamps, not hers
 ***************************************************************************/
PRIVATE void check_stamped(hgobj gobj, const char *what)
{
    char name[80];
    json_t *kw = json_object_get(stamped, what);
    snprintf(name, sizeof(name), "%s reached the service", what);
    if(check(gobj, name, kw != NULL) < 0) {
        return;
    }
    snprintf(name, sizeof(name), "%s: __username__ is the gate's", what);
    check(gobj, name, strcmp(kw_get_str(gobj, kw, "__username__", "", 0), "alice")==0);

    json_t *jn_stack = kw_get_list(gobj, kw, "__md_iev__`" IEVENT_STACK_ID, 0, 0);
    json_t *jn_top = json_array_get(jn_stack, 0);
    snprintf(name, sizeof(name), "%s: the stack's __username__ is the gate's", what);
    check(gobj, name, strcmp(kw_get_str(gobj, jn_top, "__username__", "", 0), "alice")==0);
    snprintf(name, sizeof(name), "%s: the stack's input_channel is the gate's", what);
    check(gobj, name, strncmp(kw_get_str(gobj, jn_top, "input_channel", "", 0), "input-", 6)==0);
    snprintf(name, sizeof(name), "%s: the stack's input_service is the gate's", what);
    check(gobj, name, strcmp(kw_get_str(gobj, jn_top, "input_service", "", 0), "forged")!=0);
}

/***************************************************************************
 *  Keep a copy of the EV_TEST_FEED that `name` got
 ***************************************************************************/
PRIVATE void record_feed(const char *name, json_t *kw)
{
    json_t *jn_list = json_object_get(received, name);
    if(!jn_list) {
        jn_list = json_array();
        json_object_set_new(received, name, jn_list);
    }
    json_array_append_new(jn_list, json_deep_copy(kw));
}

/***************************************************************************
 *  The subscriptions of `publisher` whose subscriber is a channel of
 *  `username`
 ***************************************************************************/
PRIVATE int count_subscriptions_of(hgobj gobj, hgobj publisher, const char *username)
{
    json_t *dl_subs = gobj_find_subscriptions(publisher, NULL, NULL, NULL);
    int count = 0;
    size_t idx; json_t *subs;
    json_array_foreach(dl_subs, idx, subs) {
        hgobj subscriber = (hgobj)(uintptr_t)kw_get_int(gobj, subs, "subscriber", 0, KW_REQUIRED);
        if(gobj_has_attr(subscriber, "__username__") &&
                strcmp(gobj_read_str_attr(subscriber, "__username__"), username)==0) {
            count++;
        }
    }
    JSON_DECREF(dl_subs)
    return count;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A client in session. With the two, the test begins.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(++priv->opened == 2) {
        /*
         *  1. The remote subscribers
         */
        send_raw_iev(gobj, priv->cli_alice, "__subscribing__", EV_TEST_FEED,
            json_pack("{s:{s:s, s:{s:s}, s:{s:i}, s:s}, s:{s:i}, s:i}",
                "__global__",
                    "topic_name", "users",
                    "node", "id", "FORGED",
                    "__md_iev__", "forged", 1,
                    "__service__", "tester",
                "__local__", "secret", 1,
                "junk", 1
            )
        );
        send_raw_iev(gobj, priv->cli_alice, "__subscribing__", EV_TEST_OPEN,
            json_pack("{s:{s:i}}", "__global__", "gbuffer", 1)
        );
        gobj_subscribe_event(
            priv->cli_bob,
            EV_TEST_FEED,
            json_pack("{s:{s:s}, s:{s:s}}",
                "__filter__", "topic_name", "devices",
                "__global__", "tag", "bob"
            ),
            gobj
        );
        set_timeout(priv->timer, 300);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What each subscriber got
 ***************************************************************************/
PRIVATE int ac_test_feed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(event == EV_TEST_OPEN) {
        if(src == priv->cli_alice) {
            priv->opens_to_alice++;
            check(gobj, "no gbuffer in alice's EV_TEST_OPEN", !kw_has_key(kw, "gbuffer"));
        }
        JSON_DECREF(kw)
        return 0;
    }

    record_feed(src == priv->cli_alice? "alice": src == priv->cli_bob? "bob": "?", kw);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  An event a peer sent, as the gate hands it on (through its channel and
 *  the C_IOGATE, which this gobj subscribes)
 ***************************************************************************/
PRIVATE int ac_peer_msg(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    json_object_set_new(stamped, "event", json_deep_copy(kw));

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The answers of the command and the stats that alice asked
 ***************************************************************************/
PRIVATE int ac_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The publisher's command and stats: keep the kw they got
 ***************************************************************************/
PRIVATE json_t *pub_mt_command_parser(hgobj gobj, const char *command, json_t *kw, hgobj src)
{
    json_object_set_new(stamped, "command", json_deep_copy(kw));
    KW_DECREF(kw)
    return build_command_response(gobj, 0, 0, 0, 0);
}

PRIVATE json_t *pub_mt_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    json_object_set_new(stamped, "stats", json_deep_copy(kw));
    KW_DECREF(kw)
    return build_command_response(gobj, 0, 0, 0, 0);
}

/***************************************************************************
 *  A sink: keeps what it gets under its name
 ***************************************************************************/
PRIVATE int ac_sink_feed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    record_feed(gobj_name(gobj), kw);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Each timeout is one step of the test
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *jn;
    char big[2048];

    switch(priv->step++) {
        case 0:
            /*
             *  The last subscriber, local and plain; then the feed
             */
            gobj_subscribe_event(priv->publisher, EV_TEST_FEED, 0, priv->sink_local);
            gobj_send_event(priv->publisher, EV_TEST_EMIT,
                json_pack("{s:s}", "event", "feed"), gobj
            );
            set_timeout(priv->timer, 300);
            break;

        case 1:
            jn = received_one(gobj, "bob");
            if(jn) {
                check(gobj, "bob: topic_name", strcmp(kw_get_str(gobj, jn, "topic_name", "", 0), "devices")==0);
                check(gobj, "bob: node", strcmp(kw_get_str(gobj, jn, "node`id", "", 0), "dev1")==0);
                check(gobj, "bob: secret", kw_has_key(jn, "secret"));
                check(gobj, "bob: his tag", strcmp(kw_get_str(gobj, jn, "tag", "", 0), "bob")==0);
                check(gobj, "bob: no sink", !kw_has_key(jn, "sink"));
            }
            jn = received_one(gobj, "local");
            if(jn) {
                check(gobj, "local: topic_name", strcmp(kw_get_str(gobj, jn, "topic_name", "", 0), "devices")==0);
                check(gobj, "local: node", strcmp(kw_get_str(gobj, jn, "node`id", "", 0), "dev1")==0);
                check(gobj, "local: secret", kw_has_key(jn, "secret"));
                check(gobj, "local: no tag", !kw_has_key(jn, "tag"));
                check(gobj, "local: no __md_iev__ of a peer", !kw_has_key(jn, "__md_iev__"));
            }
            jn = received_one(gobj, "strip");
            if(jn) {
                check(gobj, "strip: topic_name", strcmp(kw_get_str(gobj, jn, "topic_name", "", 0), "devices")==0);
                check(gobj, "strip: no secret", !kw_has_key(jn, "secret"));
            }
            jn = received_one(gobj, "alice");
            if(jn) {
                check(gobj, "alice: her topic_name", strcmp(kw_get_str(gobj, jn, "topic_name", "", 0), "users")==0);
                check(gobj, "alice: the secret, her __local__ dropped", kw_has_key(jn, "secret"));
            }

            /*
             *  2. What alice may hold, and what she repeats
             */
            for(int i=0; i<4; i++) {
                send_raw_iev(gobj, priv->cli_alice, "__subscribing__", EV_TEST_OPEN,
                    json_pack("{s:{s:i}}", "__filter__", "n", i)
                );
            }
            memset(big, 'z', sizeof(big));
            big[600] = 0;
            for(int i=0; i<2; i++) {
                send_raw_iev(gobj, priv->cli_alice, "__subscribing__", EV_TEST_OPEN,
                    json_pack("{s:{s:s, s:i}}", "__filter__", "big", big, "n", i)
                );
            }
            big[sizeof(big)-1] = 0;
            for(int i=0; i<5; i++) {
                send_raw_iev(gobj, priv->cli_alice, "__unsubscribing__", EV_TEST_FEED,
                    json_pack("{s:{s:i}, s:s}", "__filter__", "n", 99, "junk", big)
                );
            }
            for(int i=0; i<3; i++) {
                send_raw_iev(gobj, priv->cli_alice, "__subscribing__", EV_TEST_SECRET,
                    json_pack("{s:{s:i}}", "__filter__", "n", i)
                );
            }

            /*
             *  3b. What the gate stamps, sent by the peer
             */
            send_raw_iev(gobj, priv->cli_alice, "__subscribing__", EV_TEST_SECRET,
                json_pack("{s:{s:i}, s:s}", "__filter__", "n", 9, "__username__", "bob")
            );
            send_raw_iev(gobj, priv->cli_alice, "__command__", EV_MT_COMMAND,
                json_pack("{s:s, s:s}", "__command__", "whoami", "__username__", "admin")
            );
            send_raw_iev(gobj, priv->cli_alice, "__stats__", EV_MT_STATS,
                json_pack("{s:s, s:s}", "__stats__", "whoami", "__username__", "admin")
            );
            send_raw_iev(gobj, priv->cli_alice, "__message__", EV_TEST_PEER_MSG,
                json_pack("{s:s}", "__username__", "admin")
            );

            /*
             *  3. bob withdraws his subscription, as C_IEVENT_CLI does it
             */
            gobj_unsubscribe_event(
                priv->cli_bob,
                EV_TEST_FEED,
                json_pack("{s:{s:s}, s:{s:s}}",
                    "__filter__", "topic_name", "devices",
                    "__global__", "tag", "bob"
                ),
                gobj
            );
            set_timeout(priv->timer, 300);
            break;

        case 2:
            check(gobj, "alice holds max_subscriptions",
                count_subscriptions_of(gobj, priv->publisher, "alice") == 4
            );
            check(gobj, "bob withdrew his subscription with a __global__",
                count_subscriptions_of(gobj, priv->publisher, "bob") == 0
            );
            check_stamped(gobj, "command");
            check_stamped(gobj, "stats");
            check_stamped(gobj, "event");

            /*
             *  4. alice's subscription with a `gbuffer`
             */
            gobj_send_event(priv->publisher, EV_TEST_EMIT,
                json_pack("{s:s}", "event", "open"), gobj
            );
            set_timeout(priv->timer, 300);
            break;

        case 3:
            check(gobj, "EV_TEST_OPEN to alice", priv->opens_to_alice == 1);
            gobj_unsubscribe_event(priv->publisher, EV_TEST_FEED, 0, priv->sink_local);
            gobj_unsubscribe_event(priv->publisher, EV_TEST_FEED, 0, priv->sink_strip);
            gobj_stop_tree(priv->cli_alice);
            gobj_stop_tree(priv->cli_bob);
            set_timeout(priv->timer, 300);
            break;

        default:
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "peer subscriptions test done",
                NULL
            );
            set_yuno_must_die();
            break;
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The publisher: publishes, and checks that its kw comes back as it went
 ***************************************************************************/
PRIVATE int ac_test_emit(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    const char *what = kw_get_str(gobj, kw, "event", "", 0);

    if(strcmp(what, "feed")==0) {
        json_t *kw_feed = json_pack("{s:s, s:{s:s}, s:s}",
            "topic_name", "devices",
            "node", "id", "dev1",
            "secret", "s3cr3t"
        );
        json_t *kw_before = json_deep_copy(kw_feed);
        json_t *kw_kept = json_incref(kw_feed);
        gobj_publish_event(gobj, EV_TEST_FEED, kw_feed);
        check(gobj, "the publisher's kw is left as it was", json_equal(kw_kept, kw_before));
        JSON_DECREF(kw_kept)
        JSON_DECREF(kw_before)
    } else {
        gobj_publish_event(gobj, EV_TEST_OPEN, json_pack("{s:s}", "what", "open"));
    }

    JSON_DECREF(kw)
    return 0;
}




/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};
PRIVATE const GMETHODS gmt_pub = {
    .mt_stats = pub_mt_stats,
    .mt_command_parser = pub_mt_command_parser,
};
PRIVATE const GMETHODS gmt_sink = {
    0
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_PEER_SUBS);
GOBJ_DEFINE_GCLASS(C_TEST_PEER_PUB);
GOBJ_DEFINE_GCLASS(C_TEST_PEER_SINK);

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
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_ON_OPEN,                ac_on_open,                 0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_TEST_FEED,              ac_test_feed,               0},
        {EV_TEST_OPEN,              ac_test_feed,               0},
        {EV_TEST_PEER_MSG,          ac_peer_msg,                0},
        {EV_MT_COMMAND_ANSWER,      ac_answer,                  0},
        {EV_MT_STATS_ANSWER,        ac_answer,                  0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_TEST_FEED,              EVF_PUBLIC_EVENT},
        {EV_TEST_OPEN,              EVF_PUBLIC_EVENT},
        {EV_TEST_PEER_MSG,          0},
        {EV_MT_COMMAND_ANSWER,      EVF_PUBLIC_EVENT},
        {EV_MT_STATS_ANSWER,        EVF_PUBLIC_EVENT},
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
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
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
PRIVATE int create_gclass_pub(gclass_name_t gclass_name)
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

    ev_action_t st_idle[] = {
        {EV_TEST_EMIT,              ac_test_emit,               0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TEST_EMIT,              0},
        {EV_TEST_FEED,              EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_TEST_OPEN,              EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_TEST_SECRET,            EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS|EVF_AUTHZ_SUBSCRIBE},
        {EV_TEST_PEER_MSG,          EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},  // public: the gate must know it
        {0, 0}
    };

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt_pub,
        0,  // lmt,
        attrs_table,
        0,  // priv size
        pub_authz_table,
        0,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
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
PRIVATE int create_gclass_sink(gclass_name_t gclass_name)
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

    ev_action_t st_idle[] = {
        {EV_TEST_FEED,              ac_sink_feed,               0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TEST_FEED,              0},
        {0, 0}
    };

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt_sink,
        0,  // lmt,
        attrs_table,
        0,  // priv size
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
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
PUBLIC int register_c_test_peer_subs(void)
{
    int ret = create_gclass(C_TEST_PEER_SUBS);
    ret += create_gclass_pub(C_TEST_PEER_PUB);
    ret += create_gclass_sink(C_TEST_PEER_SINK);
    return ret;
}
