/***********************************************************************
 *          C_TEST_TASK.C
 *
 *          Test of how a C_TASK ends: the gobj_jobs of its tasks, with
 *          one job each (job_action, then job_result).
 *
 *      A) The job's timeout fires: the task's own C_TIMER publishes
 *         EV_TIMEOUT. EV_END_TASK arrives with result -2, once, and the
 *         volatile task is gone afterwards. Up to 7.26.9 the task ended
 *         INSIDE that publication of its timer, and its destroy freed the
 *         timer under it: the kernel and C_TIMER then read freed memory
 *         (run it under ASan to see it), and the kernel now says it with
 *         an ERROR, which would fail this test.
 *
 *      B) The task is timed out from outside (an EV_TIMEOUT sent to it, as
 *         C_AUTH_BFF's watchdog does) and its result arrives right after,
 *         before the end of the task was delivered. The late result is
 *         not processed, and EV_END_TASK still arrives once, with -2.
 *
 *      C) The job's result arrives: EV_END_TASK with result 0, once, on the
 *         next turn of the loop. Every end of a task runs there since
 *         7.26.10, not inside the publication of gobj_results.
 *
 *      D) The job's result arrives INSIDE the job's action (a local service
 *         that answers in the call). The task ends inside an inner dispatch
 *         of its own (ac_on_message) while an outer one (ac_on_open ->
 *         execute_action) is still running on it. Up to 7.26.9 the inner
 *         one destroyed the task and the outer one went on on freed memory
 *         (ASan), which the kernel now says with an ERROR. EV_END_TASK with
 *         result 0 arrives once, on the next turn, and the result is
 *         processed once.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_results.h"
#include "c_test_task.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SHORT_TIMEOUT_MS    300     // case A: fires on the timer of the task
#define LONG_TIMEOUT_MS     60000   // cases B and C: never fires

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE hgobj create_task(hgobj gobj, const char *name, json_int_t exec_timeout);
PRIVATE int start_case_a(hgobj gobj);
PRIVATE int start_case_b(hgobj gobj);
PRIVATE int start_case_c(hgobj gobj);
PRIVATE int start_case_d(hgobj gobj);
PRIVATE int check_case(
    hgobj gobj,
    const char *case_name,
    int expected_result,
    int expected_last_job,
    const char *expected_comment,
    int expected_results
);

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
    hgobj results;              // the gobj_results of every task

    char cur_case[2];           // "A", "B", "C"
    int ends;                   // EV_END_TASK of the current case
    int last_result;
    int last_job;
    char last_comment[NAME_MAX];
    int results_processed;      // job_result calls of the current case
    BOOL answer_in_action;      // case D: the results answers inside job_action
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

    priv->results = gobj_create_service("results", C_TEST_RESULTS, 0, gobj);

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
 *  The action of the job: ask, and wait for the answer.
 ***************************************************************************/
PRIVATE json_t *job_action(hgobj gobj, const char *lmethod, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    KW_DECREF(kw)

    if(priv->answer_in_action) {
        gobj_send_event(priv->results, EV_TEST_ANSWER, 0, gobj);
    }
    CONTINUE_TASK()
}

/***************************************************************************
 *  The result of the job.
 ***************************************************************************/
PRIVATE json_t *job_result(hgobj gobj, const char *lmethod, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->results_processed++;

    KW_DECREF(kw)
    CONTINUE_TASK()
}

/***************************************************************************
 *  A volatile task with one job, that waits for EV_ON_OPEN of the results.
 ***************************************************************************/
PRIVATE hgobj create_task(hgobj gobj, const char *name, json_int_t exec_timeout)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->ends = 0;
    priv->last_result = 1;      // no end task yet
    priv->last_job = -1;
    priv->last_comment[0] = 0;
    priv->results_processed = 0;

    json_t *kw_task = json_pack("{s:I, s:I, s:[{s:s, s:s, s:I}]}",
        "gobj_jobs",    (json_int_t)(uintptr_t)gobj,
        "gobj_results", (json_int_t)(uintptr_t)priv->results,
        "jobs",
            "exec_action",  "job_action",
            "exec_result",  "job_result",
            "exec_timeout", exec_timeout
    );

    hgobj task = gobj_create(name, C_TASK, kw_task, gobj);
    if(!task) {
        // Error already logged
        return NULL;
    }
    gobj_subscribe_event(task, EV_END_TASK, 0, gobj);
    gobj_set_volatil(task, TRUE);
    gobj_start(task);

    /*
     *  The task runs its job when the results says it is open
     */
    gobj_send_event(priv->results, EV_TEST_OPEN, 0, gobj);

    return task;
}

/***************************************************************************
 *  Case A: the timer of the task fires.
 ***************************************************************************/
PRIVATE int start_case_a(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    snprintf(priv->cur_case, sizeof(priv->cur_case), "%s", "A");
    create_task(gobj, "task_a", SHORT_TIMEOUT_MS);
    return 0;
}

/***************************************************************************
 *  Case B: timed out from outside, and the answer arrives right after.
 ***************************************************************************/
PRIVATE int start_case_b(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    snprintf(priv->cur_case, sizeof(priv->cur_case), "%s", "B");
    hgobj task = create_task(gobj, "task_b", LONG_TIMEOUT_MS);
    if(!task) {
        // Error already logged
        return -1;
    }

    gobj_send_event(task, EV_TIMEOUT, 0, gobj);
    gobj_send_event(priv->results, EV_TEST_ANSWER, 0, gobj);
    return 0;
}

/***************************************************************************
 *  Case C: the answer arrives, the task ends inside it.
 ***************************************************************************/
PRIVATE int start_case_c(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    snprintf(priv->cur_case, sizeof(priv->cur_case), "%s", "C");
    if(!create_task(gobj, "task_c", LONG_TIMEOUT_MS)) {
        // Error already logged
        return -1;
    }

    gobj_send_event(priv->results, EV_TEST_ANSWER, 0, gobj);

    if(priv->ends != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The end of a task came inside the publication of its gobj_results",
            "ends",         "%d", priv->ends,
            NULL
        );
    }
    return 0;
}

/***************************************************************************
 *  Case D: the answer comes inside the job's action.
 ***************************************************************************/
PRIVATE int start_case_d(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    snprintf(priv->cur_case, sizeof(priv->cur_case), "%s", "D");
    priv->answer_in_action = TRUE;
    if(!create_task(gobj, "task_d", LONG_TIMEOUT_MS)) {
        // Error already logged
        return -1;
    }
    priv->answer_in_action = FALSE;
    return 0;
}

/***************************************************************************
 *  What the end of a case must have left.
 ***************************************************************************/
PRIVATE int check_case(
    hgobj gobj,
    const char *case_name,
    int expected_result,
    int expected_last_job,
    const char *expected_comment,
    int expected_results
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    if(priv->ends != 1 ||
        priv->last_result != expected_result ||
        priv->last_job != expected_last_job ||
        strcmp(priv->last_comment, expected_comment) != 0 ||
        priv->results_processed != expected_results
    ) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "The end of the task is not what it should be",
            "case",             "%s", case_name,
            "ends",             "%d", priv->ends,
            "result",           "%d", priv->last_result,
            "last_job",         "%d", priv->last_job,
            "comment",          "%s", priv->last_comment,
            "results",          "%d", priv->results_processed,
            "expected_result",  "%d", expected_result,
            "expected_last_job","%d", expected_last_job,
            "expected_results", "%d", expected_results,
            NULL
        );
        ret = -1;
    }

    json_t *jn_tasks = gobj_match_children(gobj, json_pack("{s:s}",
        "__gclass_name__", C_TASK
    ));
    if(json_array_size(jn_tasks) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A volatile task is still there after its end",
            "case",         "%s", case_name,
            "tasks",        "%d", (int)json_array_size(jn_tasks),
            NULL
        );
        ret = -1;
    }
    gobj_free_iter(jn_tasks);

    return ret;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_run(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    start_case_a(gobj);
    return 0;
}

/***************************************************************************
 *  EV_END_TASK. What it says is checked on the next cycle, when the
 *  volatile task must be gone.
 ***************************************************************************/
PRIVATE int ac_end_task(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->ends++;
    priv->last_result = (int)kw_get_int(gobj, kw, "result", 1, KW_REQUIRED);
    priv->last_job = (int)kw_get_int(gobj, kw, "last_job", -1, KW_REQUIRED);
    snprintf(priv->last_comment, sizeof(priv->last_comment), "%s",
        kw_get_str(gobj, kw, "output_data`comment", "", 0)
    );

    if(priv->ends == 1) {
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
        check_case(gobj, "A", -2, 0, "Task failed by timeout", 0);  // Error already logged
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "case A ok",
            NULL
        );
        start_case_b(gobj);

    } else if(strcmp(priv->cur_case, "B")==0) {
        check_case(gobj, "B", -2, 0, "Task failed by timeout", 0);  // Error already logged
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "case B ok",
            NULL
        );
        start_case_c(gobj);

    } else if(strcmp(priv->cur_case, "C")==0) {
        check_case(gobj, "C", 0, 1, "", 1);    // Error already logged
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "case C ok",
            NULL
        );
        start_case_d(gobj);

    } else {
        check_case(gobj, "D", 0, 1, "", 1);    // Error already logged
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "case D ok",
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

/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {"job_action",      job_action,     0},
    {"job_result",      job_result,     0},
    {0, 0, 0}
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_TASK);

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
        {EV_END_TASK,               ac_end_task,            0},
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
        {EV_END_TASK,               0},
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
        lmt,
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
PUBLIC int register_c_test_task(void)
{
    return create_gclass(C_TEST_TASK);
}
