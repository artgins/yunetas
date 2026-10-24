/***********************************************************************
 *          C_TEST4.C
 *
 *          Test of the records of the subscriptions (subs_rec_t in
 *          gobj.c): what a publication reads of each subscription, read
 *          once when it is made, and kept index for index with
 *          dl_subscriptions. The publisher (C_TEST4) checks, in one action
 *          posted from mt_play():
 *
 *      1) A publication to more subscribers than the records kept on the
 *         stack (32): 40 C_TEST4_SUB children subscribed to EV_ON_MESSAGE,
 *         each one gets it once.
 *
 *      2) Subscriptions changed DURING a publication, from a subscriber's
 *         action: the 6th subscriber unsubscribes the 21st and subscribes
 *         a 41st. In that publication the 21st does not get the event (its
 *         record is zeroed) and the 41st does not either (it was not a
 *         subscriber when the publication began); in the next one the 21st
 *         does not and the 41st does.
 *
 *      3) EV_STATE_CHANGED, which is published only when somebody would
 *         see it (state_changed_has_audience()), to subscribers of all the
 *         events of the publisher, which does not declare it: one that
 *         declares it gets every change of state, and one with a __filter__
 *         gets the changes its filter matches.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include "c_test4.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define N_SUBSCRIBERS   40      // more than SUBS_RECS_ON_STACK

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

PRIVATE sdata_desc_t attrs_table_sub[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_INTEGER,     "index",            SDF_RD,             "0",        "Index of the subscriber"),
SDATA (DTP_INTEGER,     "messages",         SDF_RD,             "0",        "EV_ON_MESSAGE received"),
SDATA (DTP_INTEGER,     "state_changes",    SDF_RD,             "0",        "EV_STATE_CHANGED received"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
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
    hgobj subs[N_SUBSCRIBERS + 1];
} PRIVATE_DATA;

typedef struct _PRIVATE_DATA_SUB {
    int dummy;
} PRIVATE_DATA_SUB;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    for(int i = 0; i <= N_SUBSCRIBERS; i++) {
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "sub-%d", i);
        priv->subs[i] = gobj_create_pure_child(
            name, C_TEST4_SUB, json_pack("{s:i}", "index", i), gobj
        );
    }
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
    return gobj_post_event(gobj, EV_TEST4_RUN, 0, gobj);
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method create of the subscriber
 ***************************************************************************/
PRIVATE void mt_create_sub(hgobj gobj)
{
    /*
     *  CHILD subscription model
     */
    hgobj subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);
}



                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Every subscriber's count of `attr` must be `expected[i]`, else logged
 ***************************************************************************/
PRIVATE int check_counts(hgobj gobj, const char *case_name, const char *attr, const int *expected)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;
    for(int i = 0; i <= N_SUBSCRIBERS; i++) {
        json_int_t count = gobj_read_integer_attr(priv->subs[i], attr);
        if(count != expected[i]) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "wrong count of events received",
                "case",         "%s", case_name,
                "attr",         "%s", attr,
                "subscriber",   "%d", i,
                "count",        "%d", (int)count,
                "expected",     "%d", expected[i],
                NULL
            );
            ret = -1;
        }
    }
    return ret;
}

/***************************************************************************
 *  1 and 2: more subscribers than records on the stack, changed meanwhile
 ***************************************************************************/
PRIVATE int test_publication(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int expected[N_SUBSCRIBERS + 1];
    int ret = 0;

    for(int i = 0; i < N_SUBSCRIBERS; i++) {
        gobj_subscribe_event(gobj, EV_ON_MESSAGE, 0, priv->subs[i]);
    }

    gobj_publish_event(gobj, EV_ON_MESSAGE, json_object());
    for(int i = 0; i <= N_SUBSCRIBERS; i++) {
        expected[i] = (i < N_SUBSCRIBERS)? 1 : 0;
    }
    if(check_counts(gobj, "1. more subscribers than on the stack", "messages", expected) == 0) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "publication to 40 subscribers ok",
            NULL
        );
    } else {
        ret = -1;
    }

    gobj_publish_event(gobj, EV_ON_MESSAGE, json_pack("{s:i, s:i, s:i}",
        "actor", 5,
        "unsubscribe", 20,
        "subscribe", N_SUBSCRIBERS
    ));
    for(int i = 0; i <= N_SUBSCRIBERS; i++) {
        expected[i] = (i == 20)? 1 : (i < N_SUBSCRIBERS)? 2 : 0;
    }
    int r = check_counts(gobj, "2. changed during a publication", "messages", expected);

    gobj_publish_event(gobj, EV_ON_MESSAGE, json_object());
    for(int i = 0; i <= N_SUBSCRIBERS; i++) {
        expected[i] = (i == 20)? 1 : (i < N_SUBSCRIBERS)? 3 : 1;
    }
    r += check_counts(gobj, "2. the publication after", "messages", expected);
    if(r == 0) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "subscriptions changed during a publication ok",
            NULL
        );
    } else {
        ret = -1;
    }

    for(int i = 0; i <= N_SUBSCRIBERS; i++) {
        if(i != 20) {
            gobj_unsubscribe_event(gobj, EV_ON_MESSAGE, 0, priv->subs[i]);
        }
    }
    return ret;
}

/***************************************************************************
 *  3: who gets EV_STATE_CHANGED
 ***************************************************************************/
PRIVATE int test_state_changed(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int expected[N_SUBSCRIBERS + 1] = {0};

    gobj_subscribe_event(gobj, NULL, 0, priv->subs[0]);
    gobj_subscribe_event(gobj, NULL,
        json_pack("{s:{s:s}}", "__filter__", "current_state", ST_SESSION),
        priv->subs[1]
    );

    gobj_change_state(gobj, ST_SESSION);
    gobj_change_state(gobj, ST_IDLE);

    expected[0] = 2;
    expected[1] = 1;
    int ret = check_counts(gobj, "3. EV_STATE_CHANGED", "state_changes", expected);
    if(ret == 0) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "state changed delivered ok",
            NULL
        );
    }

    gobj_unsubscribe_event(gobj, NULL, 0, priv->subs[0]);
    gobj_unsubscribe_event(gobj, NULL,
        json_pack("{s:{s:s}}", "__filter__", "current_state", ST_SESSION),
        priv->subs[1]
    );
    return ret;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_test_run(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    test_publication(gobj);
    test_state_changed(gobj);

    set_yuno_must_die();

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A subscriber: count it; the actor changes the subscriptions of the
 *  publisher while it publishes
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_write_integer_attr(gobj, "messages", gobj_read_integer_attr(gobj, "messages") + 1);

    json_int_t index = gobj_read_integer_attr(gobj, "index");
    if(kw_has_key(kw, "actor") && kw_get_int(gobj, kw, "actor", -1, 0) == index) {
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "sub-%d", (int)kw_get_int(gobj, kw, "unsubscribe", 0, KW_REQUIRED));
        gobj_unsubscribe_event(src, EV_ON_MESSAGE, 0, gobj_child_by_name(src, name));
        snprintf(name, sizeof(name), "sub-%d", (int)kw_get_int(gobj, kw, "subscribe", 0, KW_REQUIRED));
        gobj_subscribe_event(src, EV_ON_MESSAGE, 0, gobj_child_by_name(src, name));
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_state_changed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_write_integer_attr(gobj, "state_changes", gobj_read_integer_attr(gobj, "state_changes") + 1);

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

PRIVATE const GMETHODS gmt_sub = {
    .mt_create = mt_create_sub,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST4);
GOBJ_DEFINE_GCLASS(C_TEST4_SUB);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TEST4_RUN);

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
        {EV_TEST4_RUN,              ac_test_run,            0},
        {0,0,0}
    };
    ev_action_t st_session[] = {
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {ST_SESSION,                st_session},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TEST4_RUN,              0},
        {EV_ON_MESSAGE,             EVF_OUTPUT_EVENT},
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
PRIVATE int create_gclass_sub(gclass_name_t gclass_name)
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
        {EV_ON_MESSAGE,             ac_on_message,          0},
        {EV_STATE_CHANGED,          ac_state_changed,       0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_MESSAGE,             0},
        {EV_STATE_CHANGED,          0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt_sub,
        0,  // lmt,
        attrs_table_sub,
        sizeof(PRIVATE_DATA_SUB),
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level,
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
PUBLIC int register_c_test4(void)
{
    int ret = create_gclass(C_TEST4);
    ret += create_gclass_sub(C_TEST4_SUB);
    return ret;
}
