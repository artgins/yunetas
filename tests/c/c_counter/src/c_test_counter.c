/***********************************************************************
 *          C_TEST_COUNTER.C
 *
 *          Test of how a C_COUNTER ends. Every counter here is volatile,
 *          counts EV_TEST_TICK, and its EV_FINAL_COUNT comes to this gobj.
 *
 *      A) It expires: the counter's own C_TIMER publishes EV_TIMEOUT.
 *         EV_FINAL_COUNT arrives once, with the count so far, and the
 *         counter is gone afterwards. Up to 7.26.9 the counter published
 *         and destroyed itself INSIDE that publication of its timer, and
 *         the destroy freed the timer under it: the kernel and C_TIMER then
 *         read freed memory (run it under ASan to see it), and the kernel
 *         now says it with an ERROR, which would fail this test.
 *
 *      B) It is expired from outside (an EV_TIMEOUT sent to it), and counts
 *         arrive right after, before its final count was delivered. They do
 *         not count: the final count says 0.
 *
 *      C) It reaches its count: EV_FINAL_COUNT comes inside the last count,
 *         as it always did. Only an expiration is deferred.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_counter.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SHORT_EXPIRATION_MS     300     // case A: fires on the timer of the counter
#define LONG_EXPIRATION_MS      60000   // cases B and C: never fires

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE hgobj create_counter(hgobj gobj, const char *name, int max_count, json_int_t expiration);
PRIVATE int send_count(hgobj gobj, const char *name);
PRIVATE int check_case(hgobj gobj, const char *case_name, int expected_count);

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
    char cur_case[2];           // "A", "B", "C"
    int finals;                 // EV_FINAL_COUNT of the current case
    int last_count;
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
    gobj_post_event(gobj, EV_TEST_RUN, 0, gobj);
    return 0;
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
 *  A volatile counter of EV_TEST_TICK, whose final count comes here.
 ***************************************************************************/
PRIVATE hgobj create_counter(hgobj gobj, const char *name, int max_count, json_int_t expiration)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->finals = 0;
    priv->last_count = -1;

    json_t *kw_counter = json_pack("{s:s, s:i, s:I, s:[{s:s}]}",
        "info", name,
        "max_count", max_count,
        "expiration_timeout", expiration,
        "input_schema",
            "event", "EV_TEST_TICK"
    );

    hgobj counter = gobj_create_volatil(name, C_COUNTER, kw_counter, gobj);
    if(!counter) {
        // Error already logged
        return NULL;
    }
    gobj_subscribe_event(counter, EV_FINAL_COUNT, 0, gobj);
    gobj_start(counter);
    return counter;
}

/***************************************************************************
 *  One count, the way a renamed subscription delivers it.
 ***************************************************************************/
PRIVATE int send_count(hgobj gobj, const char *name)
{
    hgobj counter = gobj_child_by_name(gobj, name);
    if(!counter) {
        return 0;   // already gone: nothing counts any more
    }
    return gobj_send_event(counter, EV_COUNT,
        json_pack("{s:s}", "__original_event_name__", "EV_TEST_TICK"),
        gobj
    );
}

/***************************************************************************
 *  What the end of a case must have left.
 ***************************************************************************/
PRIVATE int check_case(hgobj gobj, const char *case_name, int expected_count)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    if(priv->finals != 1 || priv->last_count != expected_count) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "The final count is not what it should be",
            "case",             "%s", case_name,
            "finals",           "%d", priv->finals,
            "count",            "%d", priv->last_count,
            "expected_count",   "%d", expected_count,
            NULL
        );
        ret = -1;
    }

    json_t *jn_counters = gobj_match_children(gobj, json_pack("{s:s}",
        "__gclass_name__", C_COUNTER
    ));
    if(json_array_size(jn_counters) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A volatile counter is still there after its final count",
            "case",         "%s", case_name,
            "counters",     "%d", (int)json_array_size(jn_counters),
            NULL
        );
        ret = -1;
    }
    gobj_free_iter(jn_counters);

    return ret;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Case A: one count, then the timer of the counter expires it.
 ***************************************************************************/
PRIVATE int ac_run(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    KW_DECREF(kw)

    snprintf(priv->cur_case, sizeof(priv->cur_case), "%s", "A");
    create_counter(gobj, "counter_a", 3, SHORT_EXPIRATION_MS);
    send_count(gobj, "counter_a");
    return 0;
}

/***************************************************************************
 *  EV_FINAL_COUNT. What it says is checked on the next cycle, when the
 *  volatile counter must be gone.
 ***************************************************************************/
PRIVATE int ac_final_count(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->finals++;
    priv->last_count = (int)kw_get_int(gobj, kw, "cur_count", -1, KW_REQUIRED);

    if(priv->finals == 1) {
        gobj_post_event(gobj, EV_TEST_CHECK, 0, gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_check(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    KW_DECREF(kw)

    if(strcmp(priv->cur_case, "A")==0) {
        check_case(gobj, "A", 1);   // Error already logged
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "case A ok",
            NULL
        );

        /*
         *  Case B: expired from outside, and three counts right after
         */
        snprintf(priv->cur_case, sizeof(priv->cur_case), "%s", "B");
        hgobj counter = create_counter(gobj, "counter_b", 3, LONG_EXPIRATION_MS);
        if(counter) {
            gobj_send_event(counter, EV_TIMEOUT, 0, gobj);
            send_count(gobj, "counter_b");
            send_count(gobj, "counter_b");
            send_count(gobj, "counter_b");
        }

    } else if(strcmp(priv->cur_case, "B")==0) {
        check_case(gobj, "B", 0);   // Error already logged
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "case B ok",
            NULL
        );

        /*
         *  Case C: the count is reached, the final count comes inside it
         */
        snprintf(priv->cur_case, sizeof(priv->cur_case), "%s", "C");
        create_counter(gobj, "counter_c", 2, LONG_EXPIRATION_MS);
        send_count(gobj, "counter_c");
        send_count(gobj, "counter_c");
        if(priv->finals != 1) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The final count of a count reached was not synchronous",
                "finals",       "%d", priv->finals,
                NULL
            );
        }

    } else {
        check_case(gobj, "C", 2);   // Error already logged
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "case C ok",
            NULL
        );
        set_yuno_must_die();
    }

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
GOBJ_DEFINE_GCLASS(C_TEST_COUNTER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TEST_RUN);
GOBJ_DEFINE_EVENT(EV_TEST_CHECK);

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
        {EV_TEST_RUN,               ac_run,                 0},
        {EV_FINAL_COUNT,            ac_final_count,         0},
        {EV_TEST_CHECK,             ac_check,               0},
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
        {EV_FINAL_COUNT,            0},
        {EV_TEST_CHECK,             0},
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
PUBLIC int register_c_test_counter(void)
{
    return create_gclass(C_TEST_COUNTER);
}
