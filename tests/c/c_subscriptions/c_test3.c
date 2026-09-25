/***********************************************************************
 *          C_TEST3.C
 *
 *          Test of the HARD subscriptions (`__hard_subscription__` in the
 *          __config__ of gobj_subscribe_event()): a subscription that only
 *          gobj_unsubscribe_list() with force removes.
 *
 *          The gobj subscribes itself to its own output event, and checks,
 *          in one action posted from mt_play():
 *
 *      1) A repeated hard subscription is ONE subscription: the second
 *         gobj_subscribe_event() logs a warning and returns the one that
 *         is there, and so does a plain subscription that matches it.
 *         Each event arrives once. Up to 7.25.4 the same kw matched
 *         nothing (the flag is taken out of the stored __config__), and a
 *         second hard subscription was made with no log: each event
 *         arrived twice.
 *
 *      2) gobj_unsubscribe_event() does not remove a hard subscription and
 *         says so with a warning; gobj_unsubscribe_list() with force
 *         removes it.
 *
 *      3) A plain subscription repeated is overridden as before (a
 *         warning, one subscription), and a hard one made over a plain one
 *         replaces it.
 *
 *      4) The same for the other two keys of __config__ the framework
 *         takes out of the stored one: a repeated `__own_event__`
 *         subscription, and a repeated `__rename_event_name__` one with a
 *         `__global__` (the stored one also gets __original_event_name__),
 *         are overridden (one subscription, each event once), and
 *         gobj_unsubscribe_event() with the same kw removes them. Up to
 *         7.25.4 only __hard_subscription__ was taken out of the kw that
 *         is matched: each repeat was a second subscription, each event
 *         arrived twice, and the withdrawal found nothing.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test3.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *kw_hard(void);
PRIVATE int count_subscriptions(hgobj gobj);
PRIVATE void check(hgobj gobj, BOOL ok, const char *what);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "Subscriber of output-events"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int received;                   // EV_ON_MESSAGE received
    int renamed;                    // EV_TEST_RENAMED received, from EV_ON_MESSAGE
} PRIVATE_DATA;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    return gobj_post_event(gobj, EV_TEST_RUN, 0, gobj);
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The kw of a hard subscription
 ***************************************************************************/
PRIVATE json_t *kw_hard(void)
{
    return json_pack("{s:{s:b}}", "__config__", "__hard_subscription__", 1);
}

/***************************************************************************
 *  The subscriptions of this gobj to its own EV_ON_MESSAGE
 ***************************************************************************/
PRIVATE int count_subscriptions(hgobj gobj)
{
    json_t *dl_subs = gobj_find_subscriptions(gobj, EV_ON_MESSAGE, NULL, gobj);
    int n = (int)json_array_size(dl_subs);
    JSON_DECREF(dl_subs)
    return n;
}

/***************************************************************************
 *  A check that fails is an error: the test expects none
 ***************************************************************************/
PRIVATE void check(hgobj gobj, BOOL ok, const char *what)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(ok) {
        return;
    }
    gobj_log_error(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_INTERNAL,
        "msg",              "%s", "TEST FAIL",
        "what",             "%s", what,
        "subscriptions",    "%d", count_subscriptions(gobj),
        "received",         "%d", priv->received,
        NULL
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  All the checks, in order
 ***************************************************************************/
PRIVATE int ac_test_run(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  1) A repeated hard subscription is one
     */
    json_t *subs1 = gobj_subscribe_event(gobj, EV_ON_MESSAGE, kw_hard(), gobj);
    check(gobj, subs1 != NULL && count_subscriptions(gobj) == 1, "a hard subscription is made");

    json_t *subs2 = gobj_subscribe_event(gobj, EV_ON_MESSAGE, kw_hard(), gobj);
    check(gobj, subs2 == subs1 && count_subscriptions(gobj) == 1,
        "a repeated hard subscription returns the one there, and makes no other");

    json_t *subs3 = gobj_subscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, subs3 == subs1 && count_subscriptions(gobj) == 1,
        "a plain subscription that matches a hard one returns the hard one");

    priv->received = 0;
    gobj_publish_event(gobj, EV_ON_MESSAGE, json_object());
    check(gobj, priv->received == 1, "each event arrives once");

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "repeated hard subscription ok",
        NULL
    );

    /*
     *  2) gobj_unsubscribe_event() leaves a hard subscription, and says so
     */
    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, count_subscriptions(gobj) == 1, "gobj_unsubscribe_event() leaves the hard subscription");

    json_t *dl_subs = gobj_find_subscriptions(gobj, EV_ON_MESSAGE, NULL, gobj);
    gobj_unsubscribe_list(gobj, dl_subs, TRUE);
    check(gobj, count_subscriptions(gobj) == 0, "gobj_unsubscribe_list() with force removes it");

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "hard unsubscribe ok",
        NULL
    );

    /*
     *  3) A plain subscription repeated is overridden, as before; a hard
     *     one over a plain one replaces it
     */
    subs1 = gobj_subscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    subs2 = gobj_subscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, subs1 != NULL && subs2 != NULL && count_subscriptions(gobj) == 1,
        "a repeated plain subscription is overridden: one subscription");

    priv->received = 0;
    gobj_publish_event(gobj, EV_ON_MESSAGE, json_object());
    check(gobj, priv->received == 1, "each event arrives once (plain)");

    subs3 = gobj_subscribe_event(gobj, EV_ON_MESSAGE, kw_hard(), gobj);
    check(gobj, subs3 != NULL && count_subscriptions(gobj) == 1,
        "a hard subscription over a plain one replaces it");

    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, count_subscriptions(gobj) == 1, "the one that replaced it is hard");

    dl_subs = gobj_find_subscriptions(gobj, EV_ON_MESSAGE, NULL, gobj);
    gobj_unsubscribe_list(gobj, dl_subs, TRUE);
    check(gobj, count_subscriptions(gobj) == 0, "gobj_unsubscribe_list() with force removes it (2)");

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "plain subscription override ok",
        NULL
    );

    /*
     *  4) __own_event__ and __rename_event_name__ repeated: overridden,
     *     and withdrawn with the same kw
     */
    json_t *kw_own = json_pack("{s:{s:b}}", "__config__", "__own_event__", 1);
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_own), gobj);
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_own), gobj);
    check(gobj, count_subscriptions(gobj) == 1, "a repeated __own_event__ subscription is one");
    priv->received = 0;
    gobj_publish_event(gobj, EV_ON_MESSAGE, json_object());
    check(gobj, priv->received == 1, "each event arrives once (__own_event__)");
    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, kw_own, gobj);
    check(gobj, count_subscriptions(gobj) == 0, "the same kw withdraws it (__own_event__)");

    json_t *kw_rename = json_pack("{s:{s:s}, s:{s:s}}",
        "__config__", "__rename_event_name__", EV_TEST_RENAMED,
        "__global__", "tag", "renamed"
    );
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_rename), gobj);
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_rename), gobj);
    check(gobj, count_subscriptions(gobj) == 1, "a repeated __rename_event_name__ subscription is one");
    priv->renamed = 0;
    gobj_publish_event(gobj, EV_ON_MESSAGE, json_object());
    check(gobj, priv->renamed == 1, "each event arrives once, renamed");
    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, kw_rename, gobj);
    check(gobj, count_subscriptions(gobj) == 0, "the same kw withdraws it (__rename_event_name__)");

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "own event and renamed event override ok",
        NULL
    );

    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The event of this gobj, to itself
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->received++;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_ON_MESSAGE of a subscription that renames it
 ***************************************************************************/
PRIVATE int ac_renamed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    check(gobj,
        strcmp(kw_get_str(gobj, kw, "__original_event_name__", "", 0), EV_ON_MESSAGE)==0 &&
        strcmp(kw_get_str(gobj, kw, "tag", "", 0), "renamed")==0,
        "the renamed event carries its original name and the __global__"
    );
    priv->renamed++;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
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
GOBJ_DEFINE_GCLASS(C_TEST3);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TEST_RUN);
GOBJ_DEFINE_EVENT(EV_TEST_RENAMED);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_TEST_RUN,               ac_test_run,            0},
        {EV_ON_MESSAGE,             ac_on_message,          0},
        {EV_TEST_RENAMED,           ac_renamed,             0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_TEST_RUN,               0},
        {EV_TEST_RENAMED,           0},
        {EV_ON_MESSAGE,             EVF_OUTPUT_EVENT},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // local methods
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        0, // command_table
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_test3(void)
{
    return create_gclass(C_TEST3);
}
