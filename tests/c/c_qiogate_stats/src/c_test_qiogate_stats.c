/***********************************************************************
 *          C_TEST_QIOGATE_STATS.C
 *
 *          GClass to test the queue gauges of C_QIOGATE
 *
 *          C_QIOGATE said `msgs_in_queue` and `pending_acks` only from its
 *          own mt_stats, and a C_MQIOGATE asks each child with
 *          build_stats(), which reads the SDF_STATS attrs and never calls
 *          the child's mt_stats: through `stats-yuno service=__output_side__`
 *          the size of a persistent link's queue could not be read. They
 *          are SDF_STATS gauges now, read live (mt_reading).
 *
 *          Tasks
 *          - Start __output_side__: a C_MQIOGATE (broadcast) with two
 *            C_QIOGATE, whose peer does not listen
 *          - Send N_MSGS messages to the C_MQIOGATE: N_MSGS in each queue
 *          - The stats of the C_MQIOGATE say msgs_in_queue N_MSGS for each
 *            child, and pending_acks 0; so does each child's attr
 *          - A timeout_poll of 0 or less (-5 in one gate, 0 in the other)
 *            is raised to 1000 ms, with a WARNING: a timer of 0 is no
 *            timer, and the gate never resent what waits for its ack. So
 *            is one written once the gate runs
 *          - The stats of a C_QIOGATE, and of its C_IOGATE, asked directly
 *            (what `stats-yuno service=<it>` does) are the response
 *            envelope, result 0 and the data in `data`: up to 7.25.21 both
 *            answered the bare data, and stats-yuno said -1
 *          - A queue that cannot save a message refuses it and holds on: with
 *            min_free_disk_percentage 101 (a disk always too full) the gate
 *            refuses N_REFUSED messages (the sender gets -1), counts them in
 *            dropped_msgs, logs one ERROR, keeps what it had, and the other
 *            gate of the broadcast takes them. With 0 it accepts them again,
 *            with one INFO. Up to 7.28.3 a message the queue could not save
 *            aborted the yuno
 *          - A gate whose queue cannot be opened at its start (its path is
 *            under a regular file) goes on: ONE ERROR, queue_ready false,
 *            the messages refused (-1) and counted in dropped_msgs, and the
 *            open tried again on its timer. The file removed, the next try
 *            opens the queue (one INFO) and the next message goes in. Up to
 *            7.30.17 the first message aborted the yuno ("trq_msgs NULL")
 *          - queue_mark_pending / queue_mark_notpending answer -1 on a gate
 *            whose queue is not open, and the gate goes on; on an open one
 *            they mark its messages and leave it open and working. Up to
 *            7.30.17 they opened the queue again over the open one ("tranger
 *            NOT NULL", the live store shut down, and closed after them: the
 *            gate was left with no queue), and on a queue that could not be
 *            opened they read a NULL queue (SIGSEGV)
 *          - A message that carries a gbuffer is queued without keeping a
 *            reference of it: the queue keeps json, and frees it with
 *            json_decref(). Up to 7.30.17 the copy for the queue increfed
 *            the gbuffer and nobody gave it back: one gbuffer leaked per
 *            message
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "c_test_qiogate_stats.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define N_MSGS      5
#define N_REFUSED   3
#define N_HELD      2

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void check(hgobj gobj, const char *what, BOOL ok);
PRIVATE int mark_queue(hgobj gobj, hgobj qiogate, const char *command, const char *expected);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
int test_qiogate_stats_failed = 0;

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
    hgobj gobj_q3;
    int phase;
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
 *  A queue_mark_* command: its result, and its comment checked to hold
 *  `expected`
 ***************************************************************************/
PRIVATE int mark_queue(hgobj gobj, hgobj qiogate, const char *command, const char *expected)
{
    json_t *jn_resp = gobj_command(qiogate, command, json_pack("{s:i}", "from-rowid", 1), gobj);
    int result = (int)kw_get_int(gobj, jn_resp, "result", -99, 0);
    const char *comment = kw_get_str(gobj, jn_resp, "comment", "", 0);
    if(!strstr(comment, expected)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: a queue_mark command answered another thing",
            "command",      "%s", command,
            "comment",      "%s", comment,
            "expected",     "%s", expected,
            NULL
        );
        test_qiogate_stats_failed++;
    }
    JSON_DECREF(jn_resp)
    return result;
}

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
        test_qiogate_stats_failed++;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases: send the messages, then read the stats
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase++) {
        case 0:
            for(int i = 0; i < N_MSGS; i++) {
                gobj_send_event(
                    priv->gobj_output_side,
                    EV_SEND_MESSAGE,
                    json_pack("{s:s, s:i}", "id", "1", "n", i),
                    gobj
                );
            }
            set_timeout(priv->timer, 300);
            break;

        case 1:
            {
                json_t *jn_stats = gobj_stats(priv->gobj_output_side, "", 0, gobj);
                json_t *jn_data = kw_get_dict(gobj, jn_stats, "data", 0, 0);
                int children = 0;
                const char *child_name; json_t *jn_child;
                json_object_foreach(jn_data, child_name, jn_child) {
                    children++;
                    check(gobj, "the C_MQIOGATE stats say msgs_in_queue of each child",
                        json_integer_value(json_object_get(jn_child, "msgs_in_queue")) == N_MSGS
                    );
                    check(gobj, "and its pending_acks",
                        json_object_get(jn_child, "pending_acks") &&
                        json_integer_value(json_object_get(jn_child, "pending_acks")) == 0
                    );
                    hgobj child = gobj_find_service(child_name, FALSE);
                    check(gobj, "the child's attr msgs_in_queue",
                        child && gobj_read_integer_attr(child, "msgs_in_queue") == N_MSGS
                    );
                }
                check(gobj, "two C_QIOGATE children in the stats", children == 2);

                check(gobj, "a timeout_poll of -5 and one of 0 are raised to the floor",
                    gobj_read_integer_attr(gobj_find_service("output-q1", TRUE), "timeout_poll") == 1000 &&
                    gobj_read_integer_attr(gobj_find_service("output-q2", TRUE), "timeout_poll") == 1000
                );
                gobj_write_integer_attr(gobj_find_service("output-q1", TRUE), "timeout_poll", 0);
                check(gobj, "and so is a timeout_poll of 0 written once started",
                    gobj_read_integer_attr(gobj_find_service("output-q1", TRUE), "timeout_poll") == 1000
                );

                json_t *jn_q = gobj_stats(gobj_find_service("output-q1", TRUE), "", 0, gobj);
                check(gobj, "the stats of a C_QIOGATE are the envelope: result 0",
                    json_object_get(jn_q, "result") && kw_get_int(gobj, jn_q, "result", -1, 0) == 0
                );
                check(gobj, "and their data say msgs_in_queue",
                    kw_get_int(gobj, jn_q, "data`msgs_in_queue", -1, 0) == N_MSGS
                );
                check(gobj, "and the data of its C_IOGATE below",
                    json_object_get(kw_get_dict(gobj, jn_q, "data", 0, 0), "txMsgs") != NULL
                );
                JSON_DECREF(jn_q)

                json_t *jn_io = gobj_stats(gobj_find_service("output-iogate-q1", TRUE), "", 0, gobj);
                check(gobj, "the stats of a C_IOGATE are the envelope: result 0",
                    json_object_get(jn_io, "result") && kw_get_int(gobj, jn_io, "result", -1, 0) == 0
                );
                check(gobj, "and their data say txMsgs",
                    json_object_get(kw_get_dict(gobj, jn_io, "data", 0, 0), "txMsgs") != NULL
                );
                JSON_DECREF(jn_io)

                /*
                 *  view-channels of the C_MQIOGATE asks each child with the
                 *  same kw: kw_incref(), so a kw carrying a gbuffer keeps
                 *  one reference of it per child (with json_incref, up to
                 *  7.25.4, the second child's answer dropped it twice: "BAD
                 *  gbuf_decref()")
                 */
                gbuffer_t *gbuf = gbuffer_create(32, 32);
                gbuffer_append_string(gbuf, "carried along");
                json_t *jn_vc = gobj_command(priv->gobj_output_side, "view-channels",
                    json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf), gobj);
                check(gobj, "view-channels of the C_MQIOGATE, a kw with a gbuffer",
                    kw_get_int(gobj, jn_vc, "result", -1, 0) >= 0
                );
                JSON_DECREF(jn_vc)

                if(test_qiogate_stats_failed) {
                    gobj_trace_json(gobj, jn_stats, "the stats of __output_side__");
                }
                JSON_DECREF(jn_stats)

                hgobj q1 = gobj_find_service("output-q1", TRUE);
                hgobj q2 = gobj_find_service("output-q2", TRUE);
                gobj_write_integer_attr(q1, "min_free_disk_percentage", 101);
                int refused = 0;
                for(int i = 0; i < N_REFUSED; i++) {
                    int ret = gobj_send_event(
                        priv->gobj_output_side,
                        EV_SEND_MESSAGE,
                        json_pack("{s:s, s:i}", "id", "1", "n", N_MSGS + i),
                        gobj
                    );
                    if(ret < 0) {
                        refused++;
                    }
                }
                check(gobj, "a queue that cannot save a message refuses it: the sender gets -1",
                    refused == N_REFUSED
                );
                check(gobj, "and counts it in dropped_msgs",
                    gobj_read_integer_attr(q1, "dropped_msgs") == N_REFUSED
                );
                check(gobj, "and keeps what it had",
                    gobj_read_integer_attr(q1, "msgs_in_queue") == N_MSGS
                );
                check(gobj, "the other gate of the broadcast takes them",
                    gobj_read_integer_attr(q2, "msgs_in_queue") == N_MSGS + N_REFUSED &&
                    gobj_read_integer_attr(q2, "dropped_msgs") == 0
                );
                json_t *jn_stats2 = gobj_stats(priv->gobj_output_side, "", 0, gobj);
                check(gobj, "the C_MQIOGATE stats say dropped_msgs of each child",
                    kw_get_int(gobj, jn_stats2, "data`output-q1`dropped_msgs", -1, 0) == N_REFUSED
                );
                JSON_DECREF(jn_stats2)

                gobj_write_integer_attr(q1, "min_free_disk_percentage", 0);
                set_timeout(priv->timer, 1500);     // the poll of the gate accepts again
            }
            break;

        case 2:
            {
                hgobj q1 = gobj_find_service("output-q1", TRUE);
                int ret = gobj_send_event(
                    priv->gobj_output_side,
                    EV_SEND_MESSAGE,
                    json_pack("{s:s, s:i}", "id", "1", "n", N_MSGS + N_REFUSED),
                    gobj
                );
                check(gobj, "the queue accepts messages again",
                    ret == 0 && gobj_read_integer_attr(q1, "msgs_in_queue") == N_MSGS + 1
                );
                check(gobj, "and dropped_msgs keeps its count",
                    gobj_read_integer_attr(q1, "dropped_msgs") == N_REFUSED
                );

                gobj_stop_tree(priv->gobj_output_side);

                /*
                 *  A gate whose queue cannot be opened: its path is under a
                 *  regular file
                 */
                int fd = open(QUEUES_PATH "/blocked", O_CREAT|O_WRONLY, 0660);
                check(gobj, "the file that blocks the path of the queue", fd >= 0);
                if(fd >= 0) {
                    close(fd);
                }
                priv->gobj_q3 = gobj_create_service(
                    "output-q3",
                    C_QIOGATE,
                    json_pack("{s:s, s:s, s:s, s:s, s:s}",
                        "tranger_path", QUEUES_PATH "/blocked/q3",
                        "tranger_database", "q3",
                        "topic_name", "msgs",
                        "tkey", "tm",
                        "system_flag", "sf_string_key"
                    ),
                    gobj
                );
                gobj_start(priv->gobj_q3);
                check(gobj, "a queue that cannot be opened: queue_ready false",
                    !gobj_read_bool_attr(priv->gobj_q3, "queue_ready")
                );
                int refused = 0;
                for(int i = 0; i < N_HELD; i++) {
                    if(gobj_send_event(
                        priv->gobj_q3,
                        EV_SEND_MESSAGE,
                        json_pack("{s:s, s:i}", "id", "1", "n", i),
                        gobj
                    ) < 0) {
                        refused++;
                    }
                }
                check(gobj, "its messages are refused, and the yuno goes on", refused == N_HELD);
                check(gobj, "and counted in dropped_msgs",
                    gobj_read_integer_attr(priv->gobj_q3, "dropped_msgs") == N_HELD
                );
                check(gobj, "queue_mark_pending answers -1 on a queue that is not open",
                    mark_queue(gobj, priv->gobj_q3, "queue_mark_pending", "queue is not open") == -1
                );
                check(gobj, "and the gate goes on, its queue still not open",
                    !gobj_read_bool_attr(priv->gobj_q3, "queue_ready")
                );
                unlink(QUEUES_PATH "/blocked");
                set_timeout(priv->timer, 2000);     // its try at 1 s, ample on a busy box; the next at 3 s
            }
            break;

        default:
            {
                check(gobj, "the try of the gate opens the queue: queue_ready true",
                    gobj_read_bool_attr(priv->gobj_q3, "queue_ready")
                );
                int ret = gobj_send_event(
                    priv->gobj_q3,
                    EV_SEND_MESSAGE,
                    json_pack("{s:s, s:i}", "id", "1", "n", N_HELD),
                    gobj
                );
                check(gobj, "and takes the next message",
                    ret == 0 && gobj_read_integer_attr(priv->gobj_q3, "msgs_in_queue") == 1
                );

                gbuffer_t *gbuf = gbuffer_create(32, 32);
                gbuffer_append_string(gbuf, "carried along");
                gbuffer_incref(gbuf);   // the test's own, to read its count after
                ret = gobj_send_event(
                    priv->gobj_q3,
                    EV_SEND_MESSAGE,
                    json_pack("{s:s, s:i, s:I}",
                        "id", "1",
                        "n", N_HELD + 1,
                        "gbuffer", (json_int_t)(uintptr_t)gbuf
                    ),
                    gobj
                );
                check(gobj, "a message with a gbuffer is queued",
                    ret == 0 && gobj_read_integer_attr(priv->gobj_q3, "msgs_in_queue") == 2
                );
                check(gobj, "and the queue keeps no reference of its gbuffer",
                    gbuf->refcount == 1
                );
                if(gbuf->refcount != 1) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "TEST: gbuffer references",
                        "refcount",     "%d", (int)gbuf->refcount,
                        NULL
                    );
                }
                GBUFFER_DECREF(gbuf)

                check(gobj, "queue_mark_pending marks the messages of the open queue",
                    mark_queue(gobj, priv->gobj_q3, "queue_mark_pending", "2 messages marked as PENDING") == 0
                );
                check(gobj, "queue_mark_notpending too",
                    mark_queue(gobj, priv->gobj_q3, "queue_mark_notpending", "2 messages marked as NOT-PENDING") == 0
                );
                ret = gobj_send_event(
                    priv->gobj_q3,
                    EV_SEND_MESSAGE,
                    json_pack("{s:s, s:i}", "id", "1", "n", N_HELD + 2),
                    gobj
                );
                check(gobj, "and the gate keeps its queue open and working",
                    ret == 0 &&
                    gobj_read_bool_attr(priv->gobj_q3, "queue_ready") &&
                    gobj_read_integer_attr(priv->gobj_q3, "msgs_in_queue") == 3
                );
                gobj_stop(priv->gobj_q3);
                set_yuno_must_die();
            }
            break;
    }

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
GOBJ_DEFINE_GCLASS(C_TEST_QIOGATE_STATS);

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
PUBLIC int register_c_test_qiogate_stats(void)
{
    return create_gclass(C_TEST_QIOGATE_STATS);
}
