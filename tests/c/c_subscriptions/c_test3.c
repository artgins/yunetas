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
 *      5) A renamed subscription is not the plain one: EV_ON_MESSAGE
 *         plain, then EV_ON_MESSAGE renamed to EV_TEST_RENAMED2, are two
 *         subscriptions (each event arrives once under each name), and the
 *         renamed kw withdraws only the renamed one. 7.25.5 found the plain
 *         one as a repeat of the renamed kw, and replaced it.
 *
 *      6) Two renames of one event are two subscriptions: EV_ON_MESSAGE
 *         renamed to EV_TEST_RENAMED and to EV_TEST_RENAMED2 both stay, and
 *         each rename withdraws its own. 7.25.5 collapsed them into one,
 *         and withdrawing either removed both.
 *
 *      7) A renamed `__own_event__` subscription repeated with the same kw
 *         is still one.
 *
 *      8) gobj_unsubscribe_list() removes the subscription it is GIVEN. A
 *         plain one and one with a `__filter__` coexist; removing the plain
 *         one leaves the filtered one in the publisher's and in the
 *         subscriber's list. A subscription already removed (a stale
 *         reference) removes nothing and is logged. Up to 7.25.20 the
 *         entry was looked up by its fields, and the first entry that
 *         matched was removed: the stale plain one took a live one with it.
 *
 *      9) A top-level `renamed_event` in the kw of gobj_find_subscriptions()
 *         is not a key of a subscription kw, and filters nothing: the
 *         plain subscription is found. Only a `__rename_event_name__` in
 *         the `__config__` of a (un)subscription selects by renamed event.
 *
 *     10) The same two, in the other order. A plain kw is a WILDCARD over
 *         the kw keys it does not set: a plain subscription over a renamed
 *         one matches it as a repeat, and replaces it (the REPEATED
 *         warning, one subscription, the plain one); a plain unsubscribe
 *         removes the plain and the renamed one alike.
 *
 *     11) A subscription withdrawn by the mt_subscription_deleted() of an
 *         entry before it in the same unsubscribe is gone as asked: no
 *         warning, and it is not taken for a hard subscription kept
 *         ("Hard subscription not removed").
 *
 *     12) A subscription the publisher refuses (mt_subscription_added()
 *         answers -1) is not made, and leaks nothing. Up to 7.25.20 its
 *         creation reference was never dropped.
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
PRIVATE int count_subscribings(hgobj gobj);
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
    int renamed2;                   // EV_TEST_RENAMED2 received, from EV_ON_MESSAGE
    json_t *withdraw_on_delete;     // (11) withdrawn when another is deleted
    BOOL refuse_subscriptions;      // (12) mt_subscription_added() answers -1
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

/***************************************************************************
 *      Framework Method subscription_added
 ***************************************************************************/
PRIVATE int mt_subscription_added(hgobj gobj, json_t *subs)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->refuse_subscriptions) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "subscription refused by the test",
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *      Framework Method subscription_deleted
 ***************************************************************************/
PRIVATE int mt_subscription_deleted(hgobj gobj, json_t *subs)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *withdraw = priv->withdraw_on_delete;
    if(withdraw && withdraw != subs) {
        priv->withdraw_on_delete = NULL;
        gobj_unsubscribe_list(gobj, json_pack("[O]", withdraw), FALSE);
    }
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
 *  The subscribings of this gobj to its own EV_ON_MESSAGE
 ***************************************************************************/
PRIVATE int count_subscribings(hgobj gobj)
{
    json_t *dl_subs = gobj_find_subscribings(gobj, EV_ON_MESSAGE, NULL, gobj);
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
    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_rename), gobj);
    check(gobj, count_subscriptions(gobj) == 0, "the same kw withdraws it (__rename_event_name__)");

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "own event and renamed event override ok",
        NULL
    );

    /*
     *  5) A renamed subscription over a plain one: two subscriptions
     */
    json_t *kw_rename_bare = json_pack("{s:{s:s}}",
        "__config__", "__rename_event_name__", EV_TEST_RENAMED2
    );
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_rename_bare), gobj);
    check(gobj, count_subscriptions(gobj) == 2, "a renamed subscription does not replace the plain one");
    priv->received = 0;
    priv->renamed2 = 0;
    gobj_publish_event(gobj, EV_ON_MESSAGE, json_object());
    check(gobj, priv->received == 1 && priv->renamed2 == 1, "each event arrives once, plain and renamed");

    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_rename_bare), gobj);
    check(gobj, count_subscriptions(gobj) == 1, "the renamed kw withdraws only the renamed one");
    priv->received = 0;
    priv->renamed2 = 0;
    gobj_publish_event(gobj, EV_ON_MESSAGE, json_object());
    check(gobj, priv->received == 1 && priv->renamed2 == 0, "the plain one is the one left");

    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, count_subscriptions(gobj) == 0, "the plain kw withdraws the plain one");

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "renamed over plain ok",
        NULL
    );

    /*
     *  6) Two renames of one event: two subscriptions
     */
    json_t *kw_rename2 = json_pack("{s:{s:s}, s:{s:s}}",
        "__config__", "__rename_event_name__", EV_TEST_RENAMED2,
        "__global__", "tag", "renamed"
    );
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_rename), gobj);
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_rename2), gobj);
    check(gobj, count_subscriptions(gobj) == 2, "two renames of one event are two subscriptions");
    priv->renamed = 0;
    priv->renamed2 = 0;
    gobj_publish_event(gobj, EV_ON_MESSAGE, json_object());
    check(gobj, priv->renamed == 1 && priv->renamed2 == 1, "each event arrives once under each name");

    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, kw_rename, gobj);
    check(gobj, count_subscriptions(gobj) == 1, "a rename withdraws only its own");
    priv->renamed = 0;
    priv->renamed2 = 0;
    gobj_publish_event(gobj, EV_ON_MESSAGE, json_object());
    check(gobj, priv->renamed == 0 && priv->renamed2 == 1, "the other rename is the one left");

    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_rename2), gobj);
    check(gobj, count_subscriptions(gobj) == 0, "the other rename withdraws its own");

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "two renames ok",
        NULL
    );

    /*
     *  7) A renamed __own_event__ subscription repeated is one
     */
    json_t *kw_own_rename = json_pack("{s:{s:s, s:b}}",
        "__config__", "__rename_event_name__", EV_TEST_RENAMED2, "__own_event__", 1
    );
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_own_rename), gobj);
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_own_rename), gobj);
    check(gobj, count_subscriptions(gobj) == 1, "a repeated renamed __own_event__ subscription is one");
    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, kw_own_rename, gobj);
    check(gobj, count_subscriptions(gobj) == 0, "the same kw withdraws it (renamed __own_event__)");
    JSON_DECREF(kw_rename2)

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "renamed own event override ok",
        NULL
    );

    /*
     *  8) gobj_unsubscribe_list() removes exactly what it is given
     */
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    json_t *dl_stale = gobj_find_subscriptions(gobj, EV_ON_MESSAGE, NULL, gobj);
    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, count_subscriptions(gobj) == 0, "the plain one is removed");

    json_t *subs_plain = json_incref(   // held: a wrong removal must not free it
        gobj_subscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj)
    );
    json_t *kw_filter = json_pack("{s:{s:b}}", "__filter__", "wanted", 1);
    json_t *subs_filter = gobj_subscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_filter), gobj);
    check(gobj, count_subscriptions(gobj) == 2, "a plain and a filtered subscription coexist");

    gobj_unsubscribe_list(gobj, dl_stale, FALSE);
    check(gobj, count_subscriptions(gobj) == 2 && count_subscribings(gobj) == 2,
        "a stale subscription removes no live one");

    gobj_unsubscribe_list(gobj, json_pack("[o]", subs_plain), FALSE);
    check(gobj, count_subscriptions(gobj) == 1 && count_subscribings(gobj) == 1,
        "removing the plain one leaves one");
    json_t *dl_left = gobj_find_subscriptions(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, json_array_get(dl_left, 0) == subs_filter,
        "the filtered one is left in the publisher's list");
    JSON_DECREF(dl_left)
    dl_left = gobj_find_subscribings(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, json_array_get(dl_left, 0) == subs_filter,
        "the filtered one is left in the subscriber's list");
    JSON_DECREF(dl_left)

    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, kw_filter, gobj);
    check(gobj, count_subscriptions(gobj) == 0 && count_subscribings(gobj) == 0,
        "the filter kw withdraws the filtered one");

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "unsubscribe list by identity ok",
        NULL
    );

    /*
     *  9) A top-level renamed_event in a find kw is no filter
     */
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    json_t *dl_found = gobj_find_subscriptions(
        gobj, EV_ON_MESSAGE, json_pack("{s:I}", "renamed_event", (json_int_t)1), gobj
    );
    check(gobj, json_array_size(dl_found) == 1, "a top-level renamed_event does not filter");
    JSON_DECREF(dl_found)
    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, count_subscriptions(gobj) == 0, "the plain one is removed (9)");

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "user renamed_event ignored ok",
        NULL
    );

    /*
     *  10) The other order: plain over renamed replaces it, and a plain
     *      unsubscribe removes both
     */
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_rename_bare), gobj);
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, count_subscriptions(gobj) == 1, "a plain subscription over a renamed one replaces it");
    priv->received = 0;
    priv->renamed2 = 0;
    gobj_publish_event(gobj, EV_ON_MESSAGE, json_object());
    check(gobj, priv->received == 1 && priv->renamed2 == 0, "the one left is the plain one");
    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, count_subscriptions(gobj) == 0, "the plain one is removed (10)");

    gobj_subscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, json_incref(kw_rename_bare), gobj);
    check(gobj, count_subscriptions(gobj) == 2, "plain then renamed are two (10)");
    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, count_subscriptions(gobj) == 0, "a plain unsubscribe removes the plain and the renamed one");
    JSON_DECREF(kw_rename_bare)

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "plain over renamed ok",
        NULL
    );

    /*
     *  11) Withdrawn by the mt_subscription_deleted() of an entry before it
     */
    gobj_subscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    priv->withdraw_on_delete = gobj_subscribe_event(
        gobj, EV_ON_MESSAGE, json_pack("{s:{s:b}}", "__filter__", "wanted", 1), gobj
    );
    check(gobj, count_subscriptions(gobj) == 2, "a plain and a filtered subscription (11)");
    gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    check(gobj, count_subscriptions(gobj) == 0 && count_subscribings(gobj) == 0,
        "both are gone, the second one withdrawn by the first one's deletion");
    check(gobj, priv->withdraw_on_delete == NULL, "mt_subscription_deleted() withdrew it");

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "withdrawn meanwhile ok",
        NULL
    );

    /*
     *  12) A refused subscription is not made, and leaks nothing
     */
    priv->refuse_subscriptions = TRUE;
    json_t *subs_refused = gobj_subscribe_event(gobj, EV_ON_MESSAGE, NULL, gobj);
    priv->refuse_subscriptions = FALSE;
    check(gobj, subs_refused == NULL, "a refused subscription is not returned");
    check(gobj, count_subscriptions(gobj) == 0 && count_subscribings(gobj) == 0,
        "a refused subscription is in no list");

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "refused subscription ok",
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
 *  EV_ON_MESSAGE of a subscription that renames it to EV_TEST_RENAMED2
 ***************************************************************************/
PRIVATE int ac_renamed2(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    check(gobj,
        strcmp(kw_get_str(gobj, kw, "__original_event_name__", "", 0), EV_ON_MESSAGE)==0,
        "the second renamed event carries its original name"
    );
    priv->renamed2++;

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
    .mt_subscription_added = mt_subscription_added,
    .mt_subscription_deleted = mt_subscription_deleted,
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
GOBJ_DEFINE_EVENT(EV_TEST_RENAMED2);

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
        {EV_TEST_RENAMED2,          ac_renamed2,            0},
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
        {EV_TEST_RENAMED2,          0},
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
