/***********************************************************************
 *          C_TEST_QIOGATE_ALERT.C
 *
 *          GClass to test the queue-size alerts of C_QIOGATE
 *
 *          A C_QIOGATE alerts (an ERROR "ALERT Queuing <size> msgs", and an
 *          email) at each step of alert_queue_size its queue climbs. Up to
 *          7.30.3 the test was "the size is a multiple of the step" at each
 *          enqueue: a queue taking and acking messages at one pace sits on
 *          a multiple, and every message landing on it alerted again --
 *          1116 ERRORs in 145 ms in one gate, draining a backlog. And the
 *          one-email-a-minute limit was ONE static timer for every gate of
 *          the yuno: one gate's alert silenced the others' emails.
 *
 *          Tasks (alert_queue_size 10, so a level is alerted again only
 *          after the queue went below it by half a step, 5)
 *          - Two C_QIOGATE under a C_MQIOGATE (broadcast), whose peer does
 *            not listen. 10 messages: both alert at 10, and both log that
 *            there is no emailsender (the email of each gate: up to 7.30.3
 *            only the first gate's)
 *          - 10 more to the first gate: an alert at 20
 *          - N_SWINGS times an ack (19) and a message (20): no alert (up to
 *            7.30.3, N_SWINGS alerts)
 *          - 6 acks (14, below 20 - 5) and 6 messages: an alert at 20 again
 *          - 16 acks (4, below 10 - 5) and 6 messages: an alert at 10
 *
 *          The acks are made here: what the peer would answer, sent to the
 *          gate as from its bottom gate. The queue is new, its messages
 *          have one key, so their rowids are 1, 2, 3...
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_qiogate_alert.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define N_SWINGS    50

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void check(hgobj gobj, const char *what, BOOL ok);
PRIVATE void send_messages(hgobj gobj, hgobj dst, int n);
PRIVATE void send_acks(hgobj gobj, hgobj q, int n);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
int test_qiogate_alert_failed = 0;

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
    hgobj gobj_output_side;
    int n_msg;
    uint64_t next_rowid;
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_output_side);
    set_timeout(priv->timer, 300);

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
 *
 ***************************************************************************/
PRIVATE void check(hgobj gobj, const char *what, BOOL ok)
{
    if(!ok) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL",
            "what",         "%s", what,
            NULL
        );
        test_qiogate_alert_failed++;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void send_messages(hgobj gobj, hgobj dst, int n)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i = 0; i < n; i++) {
        gobj_send_event(
            dst,
            EV_SEND_MESSAGE,
            json_pack("{s:s, s:i}", "id", "1", "n", priv->n_msg++),
            gobj
        );
    }
}

/***************************************************************************
 *  The acks of the n oldest messages, as the peer answers them
 ***************************************************************************/
PRIVATE void send_acks(hgobj gobj, hgobj q, int n)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i = 0; i < n; i++) {
        json_t *jn_ack = json_pack("{s:{s:I, s:I, s:i}}",
            "__md_trq__",
                "__msg_rowid__", (json_int_t)priv->next_rowid++,
                "__msg_t__", (json_int_t)0,
                "result", 0
        );
        gbuffer_t *gbuf = json2gbuf(0, jn_ack, JSON_COMPACT);
        gobj_send_event(
            q,
            EV_ON_MESSAGE,
            json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
            gobj_bottom_gobj(q)
        );
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The whole test, once the gates are started
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj q1 = gobj_find_service("output-q1", TRUE);
    hgobj q2 = gobj_find_service("output-q2", TRUE);
    priv->next_rowid = 1;

    send_messages(gobj, priv->gobj_output_side, 10);
    check(gobj, "10 messages in each gate",
        gobj_read_integer_attr(q1, "msgs_in_queue") == 10 &&
        gobj_read_integer_attr(q2, "msgs_in_queue") == 10
    );

    send_messages(gobj, q1, 10);
    check(gobj, "20 in the first gate", gobj_read_integer_attr(q1, "msgs_in_queue") == 20);

    for(int i = 0; i < N_SWINGS; i++) {
        send_acks(gobj, q1, 1);
        send_messages(gobj, q1, 1);
    }
    check(gobj, "still 20 after the swings", gobj_read_integer_attr(q1, "msgs_in_queue") == 20);

    send_acks(gobj, q1, 6);
    check(gobj, "14 after 6 acks", gobj_read_integer_attr(q1, "msgs_in_queue") == 14);
    send_messages(gobj, q1, 6);

    send_acks(gobj, q1, 16);
    check(gobj, "4 after 16 acks", gobj_read_integer_attr(q1, "msgs_in_queue") == 4);
    send_messages(gobj, q1, 6);
    check(gobj, "10 at the end", gobj_read_integer_attr(q1, "msgs_in_queue") == 10);

    gobj_stop_tree(priv->gobj_output_side);
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The output side's own events: the peer does not listen, nothing opens
 ***************************************************************************/
PRIVATE int ac_on_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
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
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_QIOGATE_ALERT);

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
        {EV_ON_OPEN,                ac_on_event,                0},
        {EV_ON_CLOSE,               ac_on_event,                0},
        {EV_ON_MESSAGE,             ac_on_event,                0},
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
        {EV_ON_MESSAGE,             0},
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
PUBLIC int register_c_test_qiogate_alert(void)
{
    return create_gclass(C_TEST_QIOGATE_ALERT);
}
