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
 *         The same with a __temp__ that is a string or an integer (each
 *         replaced, one ERROR); one that is a dict without the channel
 *         gets the channel written in it, its other keys kept, no log.
 *      2) 2500 sends with no channel to go to: "No channel FOUND to send"
 *         is said for the 1st, the 1001st and the 2001st, with `repeated`
 *         the ones not said since (0, 999, 999). Up to 7.30.26 its brake
 *         never counted: 2500 ERRORs.
 *      3) A send that finds a channel (this gobj, named in its __temp__)
 *         ends the run: an INFO says it, with the 499 the brake did not
 *         say, and the count starts again: the next send with no channel
 *         is said at once (`repeated` 0).
 *      4) The same brake for a send to all channels (1500 with no
 *         channel: said at 1 and 1001) and for a drop of a channel that
 *         does not exist (1200: said at 1 and 1001); the INFO of the end
 *         of each run, the last one when the gate stops (199 not said).
 *         Up to 7.30.26 both said every one, the send to all with a
 *         stack.
 *      5) The gate counts the messages it could not send
 *         (txMsgsNoChannel: 2500 + 1 + 1500; a drop is not a message).
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
#define N_SENDS_ALL 1500    // to all, with no channel: said at 1 and 1001
#define N_DROPS     1200    // of a channel that does not exist: said at 1 and 1001

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int fail(hgobj gobj, const char *what, const char *detail);
PRIVATE void send_to_me(hgobj gobj);

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
    int sent_to_me;             // EV_SEND_MESSAGE the gate sent to this gobj as its channel
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




/***************************************************************************
 *  A send to the gate whose channel is this gobj: it finds its channel
 ***************************************************************************/
PRIVATE void send_to_me(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_send = json_pack("{s:{s:s, s:I}}",
        "__temp__",
            "channel", gobj_name(gobj),
            "channel_gobj", (json_int_t)(uintptr_t)gobj
    );
    gobj_send_event(priv->iogate, EV_SEND_MESSAGE, kw_send, gobj);
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
    json_t *temps[] = {
        json_string("a string"),
        json_integer(1094795585),
        json_pack("{s:s}", "other", "kept"),
        0
    };
    for(int i=0; temps[i]; i++) {
        json_t *kw_msg = json_pack("{s:i}", "n", 4+i);
        json_object_set_new(kw_msg, "__temp__", temps[i]);
        gobj_send_event(priv->iogate, EV_ON_MESSAGE, kw_msg, gobj);
    }
    if(json_array_size(priv->published) != 7) {
        fail(gobj, "the gate publishes the seven messages", "");
    }
    if(!json_is_string(json_object_get(json_array_get(priv->published, 6), "other"))) {
        fail(gobj, "a __temp__ dict without the channel keeps its keys", "");
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

    /*
     *  3) A send that finds its channel (this gobj) ends the run, and the
     *  count starts again
     */
    send_to_me(gobj);
    gobj_send_event(priv->iogate, EV_SEND_MESSAGE, json_pack("{s:s}", "n", "after"), gobj);
    send_to_me(gobj);

    /*
     *  4) The same brake for a send to all and a drop
     */
    for(int i=0; i<N_SENDS_ALL; i++) {
        gobj_send_event(priv->iogate, EV_SEND_MESSAGE,
            json_pack("{s:i, s:i}", "n", i, "__send_type__", 1), gobj);
    }
    send_to_me(gobj);
    for(int i=0; i<N_DROPS; i++) {
        gobj_send_event(priv->iogate, EV_DROP,
            json_pack("{s:{s:s}}", "__temp__", "channel", "nobody"), gobj);
    }
    if(priv->sent_to_me != 3) {
        fail(gobj, "the sends with a channel reach it", "");
    }

    /*
     *  5) What the gate could not send, counted
     */
    json_t *jn_stats = gobj_stats(priv->iogate, NULL, 0, gobj);
    json_t *jn_data = kw_get_dict(gobj, jn_stats, "data", 0, 0);
    if(kw_get_int(gobj, jn_data, "txMsgsNoChannel", -1, 0) != N_SENDS + 1 + N_SENDS_ALL) {
        fail(gobj, "txMsgsNoChannel counts the messages with no channel", "");
    }
    if(kw_get_int(gobj, jn_data, "txMsgs", -1, 0) != 3) {
        fail(gobj, "txMsgs counts the messages sent", "");
    }
    JSON_DECREF(jn_stats)

    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What the gate sends to this gobj, its channel
 ***************************************************************************/
PRIVATE int ac_sent_to_me(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->sent_to_me++;

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
        {EV_SEND_MESSAGE,           ac_sent_to_me,          0},
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
        {EV_SEND_MESSAGE,           0},
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
