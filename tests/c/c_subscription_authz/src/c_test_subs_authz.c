/***********************************************************************
 *          C_TEST_SUBS_AUTHZ.C
 *
 *          GClasses to test the authorization of an external subscription
 *
 *          An external subscription enters through C_IEVENT_SRV. When the
 *          yuno sets `enable_subscription_authz` and the event is declared
 *          EVF_AUTHZ_SUBSCRIBE, the subscriber needs the permission of the
 *          publisher's gclass aliased `__subscribe_event__` -- here `read`,
 *          as in C_NODE -- or the subscription is refused and logged. Up to
 *          7.25.4 the check was commented out, so any authenticated user
 *          got the feed.
 *
 *          C_TEST_PUB_AUTHZ (service `publisher`) publishes:
 *              EV_TEST_FEED    EVF_AUTHZ_SUBSCRIBE
 *              EV_TEST_OPEN    no authz
 *
 *          C_TEST_TREEDB_HOST (service `treedb_host`) opens a treedb in a
 *          real C_NODE, the service TREEDB_SERVICE, whose five
 *          EV_TREEDB_NODE_* are EVF_AUTHZ_SUBSCRIBE and whose `read` carries
 *          the `__subscribe_event__` alias; and a real C_TRANGER, the
 *          service TRANGER_NAME, whose realtime feed EV_TRANGER_RECORD_ADDED
 *          is EVF_AUTHZ_SUBSCRIBE too, with the same alias on its `read`
 *          (up to 7.25.4 it was open to any authenticated user).
 *
 *          Before anything, the six channel commands of the gate C_IOGATE
 *          are asked with a channel_name that matches no channel: each
 *          answers with no channel (up to 7.25.4 each looped for ever).
 *
 *          C_TEST_SUBS_AUTHZ (service `subscriber`) drives the test through
 *          three C_IEVENT_CLI of the same yuno, connected by websocket over
 *          loopback (the stack of the agent and of the SPAs) to
 *          the C_IEVENT_SRV gate, each authenticated as the user named by
 *          its `jwt` (see the authentication parser of main.c):
 *
 *              1. gate OFF, `nobody` subscribes EV_TEST_FEED   -> accepted
 *                           `nobody` subscribes EV_TREEDB_NODE_UPDATED
 *                                                          -> accepted
 *              2. gate ON,  `nobody` subscribes EV_TEST_FEED   -> REFUSED
 *                           `nobody` subscribes EV_TEST_OPEN   -> accepted
 *                           `reader` subscribes EV_TEST_FEED   -> accepted,
 *                           `nobody` subscribes EV_TREEDB_NODE_UPDATED
 *                                                          -> REFUSED
 *                           `reader` subscribes EV_TREEDB_NODE_UPDATED
 *                                                          -> accepted,
 *                           `nobody` subscribes EV_TRANGER_RECORD_ADDED
 *                                                          -> REFUSED
 *                           `reader` subscribes EV_TRANGER_RECORD_ADDED
 *                                                          -> accepted,
 *                           the checker being asked `read` every time,
 *                           with the global `authzs` trace on (up to
 *                           7.25.4 it printed a kw already freed)
 *              3. the publisher publishes both events and a node of the
 *                 treedb is updated: EV_TEST_FEED and EV_TREEDB_NODE_UPDATED
 *                 arrive twice each (the subscriptions of 1 and of
 *                 `reader`), and EV_TEST_OPEN once.
 *              4. every subscription but those of `reader` is withdrawn, the
 *                 refused ones too: they were never made, and their
 *                 withdrawal is not an error.
 *              5. `reader` is stopped with its two remote subscriptions
 *                 still open, and they are withdrawn right after, before the
 *                 close of its transport arrives. Nothing may be sent to the
 *                 transport that is stopping (up to 7.25.4: "Event NOT
 *                 DEFINED in state" from C_WEBSOCKET or C_TCP); the server
 *                 drops them when the channel closes.
 *
 *              6. `cli_hard` (a fourth client, `reader`) subscribes EV_TEST_FEED
 *                 with a __config__ that asks __hard_subscription__,
 *                 __own_event__ and __rename_event_name__, and
 *                 __first_shot__: the server keeps only __first_shot__
 *                 (logged), so the subscription is a plain one. Up to
 *                 7.25.4 a peer could make it hard: it outlived the session,
 *                 every publish broke on it ("Not in session") before a
 *                 later subscriber, and it went to the next user of the
 *                 channel, who never asked for it.
 *              7. `cli_hard` leaves: its subscription goes with it, a local
 *                 subscriber added after it gets the feed, and `nobody`,
 *                 connecting next on the same channel and subscribing
 *                 nothing, gets nothing.
 *              8. every channel of the gate is disabled and enabled again
 *                 (C_IOGATE disable-channel, enable-channel): C_CHANNEL,
 *                 C_IEVENT_SRV and C_WEBSOCKET run again, and `nobody`
 *                 opens a session. In the tree before the fix of 7.25.5 the
 *                 channel came back with its protocol gobj stopped, every
 *                 client was accepted and never read, and the disable logged
 *                 "GObj NOT RUNNING".
 *
 *          The gate `__input_side__` is an autostart service: the yuno
 *          starts its tree and stops it with a plain gobj_stop(), and every
 *          protocol gobj of its channels has to stop with it (up to 7.25.4
 *          each C_WEBSOCKET stayed running: "Destroying a RUNNING gobj").
 *
 *          A wrong result is logged as an error, which the expected-logs
 *          check of main.c does not expect.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include "c_test_subs_authz.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TREEDB_NAME     "treedb_subs_authz"
#define TRANGER_NAME    "tranger_subs_authz"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int check_subscriptions(
    hgobj gobj,
    hgobj publisher,
    gobj_event_t event,
    const char *usernames   // expected, sorted, joined by ','
);
PRIVATE int check_count(hgobj gobj, const char *what, int expected, int got);
PRIVATE int check_channel_commands(hgobj gobj, hgobj input_side);
PRIVATE json_t *kw_treedb_service(void);
PRIVATE json_t *kw_tranger_service(void);
PRIVATE int check_peer_subscription(hgobj gobj, hgobj publisher);
PRIVATE int check_channels_running(hgobj gobj, hgobj input_side);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
int authz_asked_read = 0;
int authz_asked_other = 0;

GOBJ_DEFINE_EVENT(EV_TEST_FEED);
GOBJ_DEFINE_EVENT(EV_TEST_OPEN);
GOBJ_DEFINE_EVENT(EV_TEST_EMIT);

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
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj publisher;
    hgobj treedb;
    hgobj tranger;
    hgobj cli_off;
    hgobj cli_nobody;
    hgobj cli_reader;
    hgobj cli_hard;
    int clients_opened;
    int local_feeds;
    int feeds_to_next_user;
    int nobody_opened;
    int step;
    int feeds_received;
    int opens_received;
    int nodes_received;
} PRIVATE_DATA;

typedef struct _PRIVATE_DATA_HOST {
    hgobj gobj_node;
    hgobj gobj_tranger;
    json_t *tranger;
} PRIVATE_DATA_HOST;

/***************************************************************************
 *  The treedb of C_TEST_TREEDB_HOST: one topic, one node to update
 ***************************************************************************/
PRIVATE char schema_subs_authz[] = "\
{                                                                   \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'items',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','writable']               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";





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

    gobj_stop(priv->timer);

    /*
     *  The gate `__input_side__` is not stopped here: it is an autostart
     *  service, and the yuno stops it.
     */
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->publisher = gobj_find_service("publisher", TRUE);
    priv->treedb = gobj_find_service(TREEDB_NAME, TRUE);
    priv->tranger = gobj_find_service(TRANGER_NAME, TRUE);
    priv->cli_off = gobj_find_service("cli_off", TRUE);
    priv->cli_nobody = gobj_find_service("cli_nobody", TRUE);
    priv->cli_reader = gobj_find_service("cli_reader", TRUE);
    priv->cli_hard = gobj_find_service("cli_hard", TRUE);

    /*
     *  All events (event NULL) is a local subscription: C_IEVENT_CLI sends
     *  only explicit events to the remote side. The gate is already
     *  running: it is an autostart service.
     */
    hgobj input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(input_side, NULL, 0, gobj);
    gobj_subscribe_event(priv->cli_off, NULL, 0, gobj);
    gobj_subscribe_event(priv->cli_nobody, NULL, 0, gobj);
    gobj_subscribe_event(priv->cli_reader, NULL, 0, gobj);
    gobj_subscribe_event(priv->cli_hard, NULL, 0, gobj);

    gobj_start_tree(priv->cli_off);
    gobj_start_tree(priv->cli_nobody);
    gobj_start_tree(priv->cli_reader);

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
 *  The users whose channels hold a subscription to `event` of `publisher`
 ***************************************************************************/
PRIVATE int check_subscriptions(
    hgobj gobj,
    hgobj publisher,
    gobj_event_t event,
    const char *usernames   // expected, sorted, joined by ','
)
{
    json_t *dl_subs = gobj_find_subscriptions(publisher, event, NULL, NULL);
    json_t *jn_names = json_array();
    size_t idx; json_t *subs;
    json_array_foreach(dl_subs, idx, subs) {
        hgobj subscriber = (hgobj)(uintptr_t)kw_get_int(gobj, subs, "subscriber", 0, KW_REQUIRED);
        json_array_append_new(
            jn_names,
            json_string(gobj_read_str_attr(subscriber, "__username__"))
        );
    }
    JSON_DECREF(dl_subs)

    json_t *jn_sorted = json_array();
    while(json_array_size(jn_names) > 0) {
        size_t min = 0;
        for(size_t i=1; i<json_array_size(jn_names); i++) {
            if(strcmp(
                json_string_value(json_array_get(jn_names, i)),
                json_string_value(json_array_get(jn_names, min))) < 0
            ) {
                min = i;
            }
        }
        json_array_append(jn_sorted, json_array_get(jn_names, min));
        json_array_remove(jn_names, min);
    }
    JSON_DECREF(jn_names)

    char got[256] = {0};
    size_t len = 0;
    json_t *jn_name;
    json_array_foreach(jn_sorted, idx, jn_name) {
        len += (size_t)snprintf(got + len, sizeof(got) - len, "%s%s",
            idx?",":"", json_string_value(jn_name)
        );
        if(len >= sizeof(got)) {
            break;
        }
    }
    JSON_DECREF(jn_sorted)

    if(strcmp(got, usernames)!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: wrong subscribers",
            "event",        "%s", event,
            "expected",     "%s", usernames,
            "got",          "%s", got,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int check_count(hgobj gobj, const char *what, int expected, int got)
{
    if(expected != got) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: wrong count",
            "what",         "%s", what,
            "expected",     "%d", expected,
            "got",          "%d", got,
            NULL
        );
        return -1;
    }
    return 0;
}




/***************************************************************************
 *  The channel commands of C_IOGATE with a channel_name that names no
 *  channel. Up to 7.25.4 each of the six looped for ever (the `continue` of
 *  a regexec mismatch skipped gobj_next_child), blocking the event loop.
 ***************************************************************************/
PRIVATE int check_channel_commands(hgobj gobj, hgobj input_side)
{
    int ret = 0;
    const char *commands[] = {
        "view-channels",
        "enable-channel",
        "disable-channel",
        "trace-on-channel",
        "trace-off-channel",
        "reset-stats-channel",
        0
    };

    for(int i=0; commands[i]; i++) {
        json_t *jn_resp = gobj_command(
            input_side,
            commands[i],
            json_pack("{s:s}", "channel_name", "nomatch"),
            gobj
        );
        int result = (int)kw_get_int(gobj, jn_resp, "result", -1, 0);
        json_t *jn_data = kw_get_list(gobj, jn_resp, "data", 0, 0);
        /*
         *  Each answers with the view of the channels it touched: the two
         *  header rows and no channel.
         */
        ret += check_count(gobj, commands[i], 0, result);
        ret += check_count(gobj, commands[i], 2, (int)json_array_size(jn_data));
        JSON_DECREF(jn_resp)
    }

    struct {
        const char *channel_name;
        int rows;
    } views[] = {
        {"input-2",         3},
        {"^input-[0-9]$",   5},
        {0, 0}
    };
    for(int i=0; views[i].channel_name; i++) {
        json_t *jn_resp = gobj_command(
            input_side,
            "view-channels",
            json_pack("{s:s}", "channel_name", views[i].channel_name),
            gobj
        );
        json_t *jn_data = kw_get_list(gobj, jn_resp, "data", 0, 0);
        ret += check_count(gobj, views[i].channel_name,
            views[i].rows, (int)json_array_size(jn_data)
        );
        JSON_DECREF(jn_resp)
    }

    return ret;
}

/***************************************************************************
 *  The one subscription of `publisher` to EV_TEST_FEED, made by a peer that
 *  asked internal options: only __first_shot__ is kept, it is not hard, it
 *  does not stop the publish loop, and the event keeps its name.
 ***************************************************************************/
PRIVATE int check_peer_subscription(hgobj gobj, hgobj publisher)
{
    int ret = 0;
    json_t *dl_subs = gobj_find_subscriptions(publisher, EV_TEST_FEED, NULL, NULL);
    if(json_array_size(dl_subs) != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: one subscription expected",
            "got",          "%d", (int)json_array_size(dl_subs),
            NULL
        );
        JSON_DECREF(dl_subs)
        return -1;
    }
    json_t *subs = json_array_get(dl_subs, 0);
    json_int_t subs_flag = kw_get_int(gobj, subs, "subs_flag", -1, 0);
    json_int_t renamed_event = kw_get_int(gobj, subs, "renamed_event", 0, 0);
    json_t *expected_config = json_pack("{s:b}", "__first_shot__", 0);
    json_t *__config__ = json_object_get(subs, "__config__");
    if(subs_flag != 0 || renamed_event != 0 || !json_equal(__config__, expected_config)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a peer set internal subscription options",
            "subs_flag",    "%d", (int)subs_flag,
            "renamed",      "%s", renamed_event?"yes":"no",
            "__config__",   "%j", __config__,
            NULL
        );
        ret = -1;
    }
    JSON_DECREF(expected_config)
    JSON_DECREF(dl_subs)
    return ret;
}

/***************************************************************************
 *  Every channel of the gate runs, with its C_IEVENT_SRV and its protocol
 *  gobj
 ***************************************************************************/
PRIVATE int check_channels_running(hgobj gobj, hgobj input_side)
{
    int ret = 0;
    hgobj channel = gobj_first_child(input_side);
    while(channel) {
        if(strcmp(gobj_gclass_name(channel), "C_CHANNEL")==0) {
            hgobj ievent_srv = gobj_bottom_gobj(channel);
            hgobj prot = ievent_srv?gobj_bottom_gobj(ievent_srv):0;
            if(!gobj_is_running(channel) ||
                !ievent_srv || !gobj_is_running(ievent_srv) ||
                !prot || !gobj_is_running(prot)
            ) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST FAIL: a channel enabled again is not running whole",
                    "channel",      "%s", gobj_name(channel),
                    "channel_running", "%d", gobj_is_running(channel),
                    "ievent_srv_running", "%d", ievent_srv?gobj_is_running(ievent_srv):0,
                    "protocol_running", "%d", prot?gobj_is_running(prot):0,
                    NULL
                );
                ret = -1;
            }
        }
        channel = gobj_next_child(channel);
    }
    return ret;
}

/***************************************************************************
 *  The kw of a remote (un)subscription to the treedb service instead of the
 *  client's `remote_yuno_service`
 ***************************************************************************/
PRIVATE json_t *kw_treedb_service(void)
{
    return json_pack("{s:s}", "__service__", TREEDB_NAME);
}

/***************************************************************************
 *  The same, to the tranger service
 ***************************************************************************/
PRIVATE json_t *kw_tranger_service(void)
{
    return json_pack("{s:s}", "__service__", TRANGER_NAME);
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A client in session. With the three, the test begins.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->step >= 5) {
        /*
         *  The later sessions (6. to 8.): each open is a step
         */
        if(src == priv->cli_nobody) {
            priv->nobody_opened++;
        }
        clear_timeout(priv->timer);
        set_timeout(priv->timer, 300);

    } else if(src == priv->cli_off || src == priv->cli_nobody || src == priv->cli_reader) {
        priv->clients_opened++;
        if(priv->clients_opened == 3) {
            /*
             *  1. gate OFF: a flagged event is subscribed as today
             */
            gobj_subscribe_event(priv->cli_off, EV_TEST_FEED, 0, gobj);
            gobj_subscribe_event(
                priv->cli_off, EV_TREEDB_NODE_UPDATED, kw_treedb_service(), gobj
            );
            set_timeout(priv->timer, 300);
        }
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A client or a server channel closed (at shutdown)
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The feed, from the remote publisher
 ***************************************************************************/
PRIVATE int ac_test_feed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->publisher) {
        priv->local_feeds++;
    } else if(src == priv->cli_nobody && priv->step >= 5) {
        priv->feeds_to_next_user++;     // under any name: a rename travels too
    } else if(event == EV_TEST_FEED) {
        priv->feeds_received++;
    } else if(event == EV_TREEDB_NODE_UPDATED) {
        priv->nodes_received++;
    } else {
        priv->opens_received++;
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Each timeout is one step of the test
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *node;
    json_t *subs;

    switch(priv->step++) {
        case 0:
            check_channel_commands(gobj, gobj_find_service("__input_side__", TRUE));

            check_subscriptions(gobj, priv->publisher, EV_TEST_FEED, "nobody");
            check_subscriptions(gobj, priv->treedb, EV_TREEDB_NODE_UPDATED, "nobody");
            check_count(gobj, "authz asked with the gate off",
                0, authz_asked_read + authz_asked_other
            );

            /*
             *  2. gate ON, with the `authzs` trace on: it prints the kw of
             *  each check after the verdict, and up to 7.25.4 that kw had
             *  already been freed by the checker (main() makes glibc
             *  scribble on every free, so a read of it crashes).
             */
            gobj_set_global_trace("authzs", TRUE);
            gobj_write_bool_attr(gobj_yuno(), "enable_subscription_authz", TRUE);
            gobj_subscribe_event(priv->cli_nobody, EV_TEST_FEED, 0, gobj);
            gobj_subscribe_event(priv->cli_nobody, EV_TEST_OPEN, 0, gobj);
            gobj_subscribe_event(priv->cli_reader, EV_TEST_FEED, 0, gobj);
            gobj_subscribe_event(
                priv->cli_nobody, EV_TREEDB_NODE_UPDATED, kw_treedb_service(), gobj
            );
            gobj_subscribe_event(
                priv->cli_reader, EV_TREEDB_NODE_UPDATED, kw_treedb_service(), gobj
            );
            gobj_subscribe_event(
                priv->cli_nobody, EV_TRANGER_RECORD_ADDED, kw_tranger_service(), gobj
            );
            gobj_subscribe_event(
                priv->cli_reader, EV_TRANGER_RECORD_ADDED, kw_tranger_service(), gobj
            );
            set_timeout(priv->timer, 300);
            break;

        case 1:
            gobj_set_global_trace("authzs", FALSE);
            check_subscriptions(gobj, priv->publisher, EV_TEST_FEED, "nobody,reader");
            check_subscriptions(gobj, priv->publisher, EV_TEST_OPEN, "nobody");
            check_subscriptions(gobj, priv->treedb, EV_TREEDB_NODE_UPDATED, "nobody,reader");
            check_subscriptions(gobj, priv->tranger, EV_TRANGER_RECORD_ADDED, "reader");
            check_count(gobj, "authz asked for 'read'", 6, authz_asked_read);
            check_count(gobj, "authz asked for another permission", 0, authz_asked_other);

            /*
             *  3. the feeds reach only the authorized subscriptions
             */
            gobj_send_event(priv->publisher, EV_TEST_EMIT, json_object(), gobj);
            node = gobj_update_node(
                priv->treedb,
                "items",
                json_pack("{s:s, s:s}", "id", "item-1", "name", "updated"),
                0,
                gobj
            );
            if(!node) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST FAIL: cannot update the node",
                    NULL
                );
            }
            JSON_DECREF(node)
            set_timeout(priv->timer, 300);
            break;

        case 2:
            check_count(gobj, "EV_TEST_FEED received", 2, priv->feeds_received);
            check_count(gobj, "EV_TEST_OPEN received", 1, priv->opens_received);
            check_count(gobj, "EV_TREEDB_NODE_UPDATED received", 2, priv->nodes_received);

            /*
             *  4. the remote subscriptions are withdrawn while the sessions
             *  are up, all but those of `reader`
             */
            gobj_unsubscribe_event(priv->cli_off, EV_TEST_FEED, 0, gobj);
            gobj_unsubscribe_event(
                priv->cli_off, EV_TREEDB_NODE_UPDATED, kw_treedb_service(), gobj
            );
            gobj_unsubscribe_event(priv->cli_nobody, EV_TEST_FEED, 0, gobj);
            gobj_unsubscribe_event(priv->cli_nobody, EV_TEST_OPEN, 0, gobj);
            gobj_unsubscribe_event(
                priv->cli_nobody, EV_TREEDB_NODE_UPDATED, kw_treedb_service(), gobj
            );
            gobj_unsubscribe_event(
                priv->cli_nobody, EV_TRANGER_RECORD_ADDED, kw_tranger_service(), gobj
            );
            set_timeout(priv->timer, 300);
            break;

        case 3:
            check_subscriptions(gobj, priv->publisher, EV_TEST_FEED, "reader");
            check_subscriptions(gobj, priv->publisher, EV_TEST_OPEN, "");
            check_subscriptions(gobj, priv->treedb, EV_TREEDB_NODE_UPDATED, "reader");
            check_subscriptions(gobj, priv->tranger, EV_TRANGER_RECORD_ADDED, "reader");

            /*
             *  5. `reader` goes with its subscriptions open, and they are
             *  withdrawn before the close of its transport arrives.
             */
            gobj_stop_tree(priv->cli_off);
            gobj_stop_tree(priv->cli_nobody);
            gobj_stop_tree(priv->cli_reader);
            gobj_unsubscribe_event(priv->cli_reader, EV_TEST_FEED, 0, gobj);
            gobj_unsubscribe_event(
                priv->cli_reader, EV_TREEDB_NODE_UPDATED, kw_treedb_service(), gobj
            );
            gobj_unsubscribe_event(
                priv->cli_reader, EV_TRANGER_RECORD_ADDED, kw_tranger_service(), gobj
            );
            set_timeout(priv->timer, 300);
            break;

        case 4:
            /*
             *  The server dropped the subscriptions of the closed channel
             */
            check_subscriptions(gobj, priv->publisher, EV_TEST_FEED, "");
            check_subscriptions(gobj, priv->treedb, EV_TREEDB_NODE_UPDATED, "");
            check_subscriptions(gobj, priv->tranger, EV_TRANGER_RECORD_ADDED, "");

            /*
             *  6. a peer asks internal options of its subscription: the
             *  __config__ that travels is the one of the client's own
             *  subscription, patched before the client starts.
             */
            subs = gobj_subscribe_event(priv->cli_hard, EV_TEST_FEED, 0, gobj);
            json_object_set_new(subs, "__config__",
                json_pack("{s:b, s:b, s:s, s:b}",
                    "__hard_subscription__", 1,
                    "__own_event__", 1,
                    "__rename_event_name__", EV_TEST_OPEN,
                    "__first_shot__", 0
                )
            );
            gobj_start_tree(priv->cli_hard);
            break;  // its open is the next step

        case 5:
            check_subscriptions(gobj, priv->publisher, EV_TEST_FEED, "reader");
            check_peer_subscription(gobj, priv->publisher);

            /*
             *  7. the peer leaves
             */
            gobj_stop_tree(priv->cli_hard);
            set_timeout(priv->timer, 300);
            break;

        case 6:
            {
                /*
                 *  By count: the channel of a closed session has no user
                 *  any more, so a name would not tell it
                 */
                json_t *dl_subs = gobj_find_subscriptions(priv->publisher, EV_TEST_FEED, NULL, NULL);
                check_count(gobj, "subscriptions left by the peer that left",
                    0, (int)json_array_size(dl_subs)
                );
                JSON_DECREF(dl_subs)
            }

            /*
             *  A local subscriber, added after the peer's: the publish loop
             *  reaches it
             */
            gobj_subscribe_event(priv->publisher, EV_TEST_FEED, 0, gobj);
            priv->local_feeds = 0;
            gobj_send_event(priv->publisher, EV_TEST_EMIT, json_object(), gobj);
            check_count(gobj, "EV_TEST_FEED to a local subscriber after the peer left",
                1, priv->local_feeds
            );
            gobj_unsubscribe_event(priv->publisher, EV_TEST_FEED, 0, gobj);

            /*
             *  The next user of the channel, who subscribes nothing
             */
            gobj_start_tree(priv->cli_nobody);
            break;  // its open is the next step

        case 7:
            priv->feeds_to_next_user = 0;
            gobj_send_event(priv->publisher, EV_TEST_EMIT, json_object(), gobj);
            set_timeout(priv->timer, 300);
            break;

        case 8:
            check_count(gobj, "EV_TEST_FEED to the next user of the channel",
                0, priv->feeds_to_next_user
            );
            gobj_stop_tree(priv->cli_nobody);
            set_timeout(priv->timer, 300);
            break;

        case 9:
            {
                /*
                 *  8. every channel disabled and enabled again
                 */
                hgobj input_side = gobj_find_service("__input_side__", TRUE);
                json_t *jn_resp = gobj_command(input_side, "disable-channel", json_object(), gobj);
                JSON_DECREF(jn_resp)
                jn_resp = gobj_command(input_side, "enable-channel", json_object(), gobj);
                JSON_DECREF(jn_resp)
                check_channels_running(gobj, input_side);

                priv->nobody_opened = 0;
                gobj_start_tree(priv->cli_nobody);
                set_timeout(priv->timer, 3000); // its open cuts it short
            }
            break;

        case 10:
            check_count(gobj, "sessions opened on a channel disabled and enabled again",
                1, priv->nobody_opened
            );
            gobj_stop_tree(priv->cli_nobody);
            set_timeout(priv->timer, 300);
            break;

        default:
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "subscription authz test done",
                NULL
            );
            set_yuno_must_die();
            break;
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The publisher: publish one of each
 ***************************************************************************/
PRIVATE int ac_test_emit(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_publish_event(gobj, EV_TEST_FEED, json_pack("{s:s}", "what", "feed"));
    gobj_publish_event(gobj, EV_TEST_OPEN, json_pack("{s:s}", "what", "open"));

    JSON_DECREF(kw)
    return 0;
}




/***************************************************************************
 *      Framework Methods of C_TEST_TREEDB_HOST: a C_NODE service on its
 *      own tranger
 ***************************************************************************/
PRIVATE void host_mt_create(hgobj gobj)
{
    PRIVATE_DATA_HOST *priv = gobj_priv_data(gobj);

    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), path_root, "c_subscription_authz", NULL);
    rmrdir(path_database);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", "c_subscription_authz",
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    priv->tranger = tranger2_startup(0, jn_tranger, 0);

    helper_quote2doublequote(schema_subs_authz);
    json_t *jn_schema = legalstring2json(schema_subs_authz, TRUE);

    json_t *kw_node = json_pack("{s:I, s:s, s:o, s:i}",
        "tranger", (json_int_t)(uintptr_t)priv->tranger,
        "treedb_name", TREEDB_NAME,
        "treedb_schema", jn_schema,
        "exit_on_error", LOG_OPT_TRACE_STACK
    );

    /*
     *  A service: C_IEVENT_SRV routes a remote subscription by service name
     */
    priv->gobj_node = gobj_create_service(TREEDB_NAME, C_NODE, kw_node, gobj);

    /*
     *  A C_TRANGER service on a store of its own, for its realtime feed
     */
    char path_tranger[PATH_MAX];
    build_path(path_tranger, sizeof(path_tranger), path_root, TRANGER_NAME, NULL);
    rmrdir(path_tranger);
    json_t *kw_tranger = json_pack("{s:s, s:s, s:b}",
        "path", path_root,
        "database", TRANGER_NAME,
        "master", 1
    );
    priv->gobj_tranger = gobj_create_service(TRANGER_NAME, C_TRANGER, kw_tranger, gobj);
}

PRIVATE int host_mt_start(hgobj gobj)
{
    PRIVATE_DATA_HOST *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_node);
    gobj_start(priv->gobj_tranger);

    json_t *node = gobj_create_node(
        priv->gobj_node,
        "items",
        json_pack("{s:s, s:s}", "id", "item-1", "name", "created"),
        0,
        gobj
    );
    if(!node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: cannot create the node",
            NULL
        );
        return -1;
    }
    JSON_DECREF(node)
    return 0;
}

PRIVATE int host_mt_stop(hgobj gobj)
{
    PRIVATE_DATA_HOST *priv = gobj_priv_data(gobj);

    gobj_stop(priv->gobj_node);
    gobj_stop(priv->gobj_tranger);
    return 0;
}

PRIVATE void host_mt_destroy(hgobj gobj)
{
    PRIVATE_DATA_HOST *priv = gobj_priv_data(gobj);

    /*
     *  The children, the C_NODE among them, are destroyed before this
     *  method, and C_NODE closes its treedb then.
     */
    if(priv->tranger) {
        tranger2_shutdown(priv->tranger);
        priv->tranger = NULL;
    }
}




/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};
PRIVATE const GMETHODS gmt_pub = {
    0
};
PRIVATE const GMETHODS gmt_host = {
    .mt_create = host_mt_create,
    .mt_start = host_mt_start,
    .mt_stop = host_mt_stop,
    .mt_destroy = host_mt_destroy,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_SUBS_AUTHZ);
GOBJ_DEFINE_GCLASS(C_TEST_PUB_AUTHZ);
GOBJ_DEFINE_GCLASS(C_TEST_TREEDB_HOST);

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
        {EV_TREEDB_NODE_UPDATED,    ac_test_feed,               0},
        {EV_TRANGER_RECORD_ADDED,   ac_test_feed,               0},
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
        {EV_TREEDB_NODE_UPDATED,    EVF_PUBLIC_EVENT},
        {EV_TRANGER_RECORD_ADDED,   EVF_PUBLIC_EVENT},
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
        s_user_trace_level,  // s_user_trace_level,
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
        {EV_TEST_FEED,              EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS|EVF_AUTHZ_SUBSCRIBE},
        {EV_TEST_OPEN,              EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
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
        s_user_trace_level,  // s_user_trace_level,
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
PRIVATE int create_gclass_host(gclass_name_t gclass_name)
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
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {0, 0}
    };

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt_host,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA_HOST),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,  // s_user_trace_level,
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
PUBLIC int register_c_test_subs_authz(void)
{
    int ret = create_gclass(C_TEST_SUBS_AUTHZ);
    ret += create_gclass_pub(C_TEST_PUB_AUTHZ);
    ret += create_gclass_host(C_TEST_TREEDB_HOST);
    return ret;
}
