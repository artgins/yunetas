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
 *          C_TEST_SUBS_AUTHZ (service `subscriber`) drives the test through
 *          three C_IEVENT_CLI of the same yuno, connected by websocket over
 *          loopback (the stack of the agent and of the SPAs) to
 *          the C_IEVENT_SRV gate, each authenticated as the user named by
 *          its `jwt` (see the authentication parser of main.c):
 *
 *              1. gate OFF, `nobody` subscribes EV_TEST_FEED   -> accepted
 *              2. gate ON,  `nobody` subscribes EV_TEST_FEED   -> REFUSED
 *                           `nobody` subscribes EV_TEST_OPEN   -> accepted
 *                           `reader` subscribes EV_TEST_FEED   -> accepted,
 *                           the checker being asked `read`
 *              3. the publisher publishes both events: EV_TEST_FEED arrives
 *                 twice (the subscriptions of 1 and of `reader`), and
 *                 EV_TEST_OPEN once.
 *              4. every subscription is withdrawn, the refused one too: it
 *                 was never made, and its withdrawal is not an error.
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
    hgobj cli_off;
    hgobj cli_nobody;
    hgobj cli_reader;
    int clients_opened;
    int step;
    int feeds_received;
    int opens_received;
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
     *  The gate is this service's, like the agent's __input_side__: a
     *  channel whose peer left keeps its protocol gobj running until then.
     */
    gobj_stop_tree(gobj_find_service("__input_side__", TRUE));
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->publisher = gobj_find_service("publisher", TRUE);
    priv->cli_off = gobj_find_service("cli_off", TRUE);
    priv->cli_nobody = gobj_find_service("cli_nobody", TRUE);
    priv->cli_reader = gobj_find_service("cli_reader", TRUE);

    /*
     *  All events (event NULL) is a local subscription: C_IEVENT_CLI sends
     *  only explicit events to the remote side.
     */
    hgobj input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(input_side, NULL, 0, gobj);
    gobj_subscribe_event(priv->cli_off, NULL, 0, gobj);
    gobj_subscribe_event(priv->cli_nobody, NULL, 0, gobj);
    gobj_subscribe_event(priv->cli_reader, NULL, 0, gobj);

    gobj_start_tree(input_side);
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




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A client in session. With the three, the test begins.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->cli_off || src == priv->cli_nobody || src == priv->cli_reader) {
        priv->clients_opened++;
        if(priv->clients_opened == 3) {
            /*
             *  1. gate OFF: a flagged event is subscribed as today
             */
            gobj_subscribe_event(priv->cli_off, EV_TEST_FEED, 0, gobj);
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

    if(event == EV_TEST_FEED) {
        priv->feeds_received++;
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

    switch(priv->step++) {
        case 0:
            check_subscriptions(gobj, priv->publisher, EV_TEST_FEED, "nobody");
            check_count(gobj, "authz asked with the gate off",
                0, authz_asked_read + authz_asked_other
            );

            /*
             *  2. gate ON
             */
            gobj_write_bool_attr(gobj_yuno(), "enable_subscription_authz", TRUE);
            gobj_subscribe_event(priv->cli_nobody, EV_TEST_FEED, 0, gobj);
            gobj_subscribe_event(priv->cli_nobody, EV_TEST_OPEN, 0, gobj);
            gobj_subscribe_event(priv->cli_reader, EV_TEST_FEED, 0, gobj);
            set_timeout(priv->timer, 300);
            break;

        case 1:
            check_subscriptions(gobj, priv->publisher, EV_TEST_FEED, "nobody,reader");
            check_subscriptions(gobj, priv->publisher, EV_TEST_OPEN, "nobody");
            check_count(gobj, "authz asked for 'read'", 2, authz_asked_read);
            check_count(gobj, "authz asked for another permission", 0, authz_asked_other);

            /*
             *  3. the feed reaches only the authorized subscriptions
             */
            gobj_send_event(priv->publisher, EV_TEST_EMIT, json_object(), gobj);
            set_timeout(priv->timer, 300);
            break;

        case 2:
            check_count(gobj, "EV_TEST_FEED received", 2, priv->feeds_received);
            check_count(gobj, "EV_TEST_OPEN received", 1, priv->opens_received);

            /*
             *  Orderly end: the remote subscriptions are withdrawn while the
             *  sessions are up, then the clients go, then the gate.
             */
            gobj_unsubscribe_event(priv->cli_off, EV_TEST_FEED, 0, gobj);
            gobj_unsubscribe_event(priv->cli_nobody, EV_TEST_FEED, 0, gobj);
            gobj_unsubscribe_event(priv->cli_nobody, EV_TEST_OPEN, 0, gobj);
            gobj_unsubscribe_event(priv->cli_reader, EV_TEST_FEED, 0, gobj);
            set_timeout(priv->timer, 300);
            break;

        case 3:
            check_subscriptions(gobj, priv->publisher, EV_TEST_FEED, "");
            check_subscriptions(gobj, priv->publisher, EV_TEST_OPEN, "");
            gobj_stop_tree(priv->cli_off);
            gobj_stop_tree(priv->cli_nobody);
            gobj_stop_tree(priv->cli_reader);
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

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_SUBS_AUTHZ);
GOBJ_DEFINE_GCLASS(C_TEST_PUB_AUTHZ);

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
PUBLIC int register_c_test_subs_authz(void)
{
    int ret = create_gclass(C_TEST_SUBS_AUTHZ);
    ret += create_gclass_pub(C_TEST_PUB_AUTHZ);
    return ret;
}
