/***********************************************************************
 *          C_TEST_IOGATE.C
 *
 *          Test of what C_IOGATE writes in a message, and says when it
 *          cannot send one. A C_IOGATE child (a pure child: it publishes
 *          to this gobj) with no channel below it:
 *
 *      1) A message whose __temp__ is not a dict, as EV_ON_MESSAGE,
 *         EV_ON_OPEN, EV_ON_CLOSE and EV_ON_IEV_MESSAGE: the gate replaces
 *         it with the channel it came in by, and says it once (an ERROR:
 *         __temp__ is local, a peer's is dropped when its frame is
 *         decoded, so only code of this yuno can put one). Up to 7.30.26
 *         each was two ERRORs ("path holds a value that is not a json
 *         dict") and the other value reached the subscriber.
 *
 *      2) 2500 sends with no channel to go to: "No channel FOUND to send"
 *         is said for the 1st, the 1001st and the 2001st, with `repeated`
 *         the ones not said since (0, 999, 999). Up to 7.30.26 its brake
 *         never counted: 2500 ERRORs.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_iogate.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define N_SENDS     2500    // with no channel: said at 1, 1001 and 2001

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int fail(hgobj gobj, const char *what, const char *detail);

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
    hgobj iogate;               // with no channel below it
    json_t *published;          // the __temp__ of each message it published, in order
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

    priv->published = json_array();
    priv->iogate = gobj_create_pure_child("iogate", C_IOGATE, 0, gobj);

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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->published)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->iogate);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->iogate);

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
 *  A check that failed: an ERROR the expected logs do not have
 ***************************************************************************/
PRIVATE int fail(hgobj gobj, const char *what, const char *detail)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST FAIL",
        "what",         "%s", what,
        "detail",       "%s", detail? detail : "",
        NULL
    );
    return -1;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The whole test, once the loop runs
 ***************************************************************************/
PRIVATE int ac_test_run(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  1) A __temp__ that is not a dict, from this gobj as the channel
     */
    gobj_event_t events[] = {
        EV_ON_MESSAGE,
        EV_ON_OPEN,
        EV_ON_CLOSE,
        EV_ON_IEV_MESSAGE,
        0
    };
    for(int i=0; events[i]; i++) {
        json_t *kw_msg = json_pack("{s:i, s:[I]}",
            "n", i,
            "__temp__", (json_int_t)1094795585
        );
        if(events[i] == EV_ON_IEV_MESSAGE) {
            json_object_set_new(kw_msg, "__iev_event__",
                json_integer((json_int_t)(uintptr_t)EV_ON_MESSAGE));
        }
        gobj_send_event(priv->iogate, events[i], kw_msg, gobj);
    }
    if(json_array_size(priv->published) != 4) {
        fail(gobj, "the gate publishes the four messages", "");
    }
    int idx; json_t *jn_temp;
    json_array_foreach(priv->published, idx, jn_temp) {
        json_int_t channel_gobj = json_integer_value(json_object_get(jn_temp, "channel_gobj"));
        const char *channel = json_string_value(json_object_get(jn_temp, "channel"));
        if(!json_is_object(jn_temp) || channel_gobj != (json_int_t)(uintptr_t)gobj ||
                !channel || strcmp(channel, gobj_name(gobj))!=0) {
            char *s = json_dumps(jn_temp, JSON_COMPACT|JSON_ENCODE_ANY);
            fail(gobj, "the __temp__ published names the channel the message came in by", s);
            gbmem_free(s);
        }
    }

    /*
     *  2) No channel to send to
     */
    for(int i=0; i<N_SENDS; i++) {
        gobj_send_event(priv->iogate, EV_SEND_MESSAGE, json_pack("{s:i}", "n", i), gobj);
    }

    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the gate publishes: its __temp__ kept
 ***************************************************************************/
PRIVATE int ac_published(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_temp = json_object_get(kw, "__temp__");
    json_array_append_new(priv->published, jn_temp? json_deep_copy(jn_temp) : json_null());

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
GOBJ_DEFINE_GCLASS(C_TEST_IOGATE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TEST_RUN);

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
        {EV_ON_MESSAGE,             ac_published,           0},
        {EV_ON_OPEN,                ac_published,           0},
        {EV_ON_CLOSE,               ac_published,           0},
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
        {EV_ON_MESSAGE,             0},
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
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
PUBLIC int register_c_test_iogate(void)
{
    return create_gclass(C_TEST_IOGATE);
}
