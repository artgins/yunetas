/***********************************************************************
 *          C_TASK.C
 *          Task GClass.
 *

        "jobs": [
            {
                "exec_action", "$local_method",
                "exec_timeout", 10000,
                "exec_result", "$local_method",
            },
            ...
        ]
                being "$local_method" a local method of gobj_jobs
                "exec_timeout" in milliseconds

        "input_data": tasks can use for they want
        "output_data": tasks can use for they want
        "gobj_jobs": json integer with a hgobj or json string with a unique gobj
        "gobj_results": json integer with a hgobj or json string with a unique gobj


    if exec_action() or exec_result() return -1 then the job is stopped

            Tasks
                - gobj_jobs: gobj defining the jobs (local methods)
                - gobj_results: GObj executing the jobs,
                    - set as bottom gobj
                    - inform of results of the jobs with the api
                        EV_ON_OPEN
                        EV_ON_CLOSE
                        EV_ON_MESSAGE ->
                            {
                                result: 0 or -1,
                                data:
                            }

 *  Task End (Event EV_END_TASK) will be called with next result values:

    switch(result) {
        case -1:
            // Error from some task action
            gobj_log_error(gobj, 0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Task End with error",
                "comment",      "%s", comment,
                "src",          "%s", gobj_full_name(src),
                NULL
            );
            gobj_trace_json(gobj, kw, "Task End with error");
            break;
        case -2:
            // Error from task manager: timeout, incomplete task
            gobj_log_error(gobj, 0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Task End by timeout",
                "comment",      "%s", comment,
                "src",          "%s", gobj_full_name(src),
                NULL
            );
            gobj_trace_json(gobj, kw, "Task End by timeout");
            break;
        case 0:
            // Task end ok
            break;
    }

 *
 *          HACK if gobj is volatil then will self auto-destroy on stop
 *
 *          Copyright (c) 2021 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <kwid.h>
#include <command_parser.h>

#include "msg_ievent.h"
#include "c_timer.h"
#include "c_task.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int execute_action(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,  "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING, "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,        cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "authzs",           0,          pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag------------default---------description---------- */
SDATA (DTP_JSON,        "jobs",             SDF_RD,         0,              "Jobs"),
SDATA (DTP_JSON,        "input_data",       SDF_RD,         "{}",           "Input Jobs Data. Use as you want. Available in exec_action() and exec_result() action methods."),
SDATA (DTP_JSON,        "output_data",      SDF_RD,         "{}",           "Output Jobs Data. Use as you want. Available in exec_action() and exec_result() action methods."),
SDATA (DTP_JSON,        "gobj_jobs",        SDF_RD,         0,              "GObj defining the jobs: json integer with a hgobj or json string with a unique gobj"),
SDATA (DTP_JSON,        "gobj_results",     SDF_RD,         0,              "GObj executing the jobs: json integer with a hgobj or json string with a unique gobj"),
SDATA (DTP_INTEGER,     "timeout",          SDF_PERSIST|SDF_WR,"10000",     "Action Timeout"),
SDATA (DTP_POINTER,     "user_data",        0,              0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,              0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,              0,              "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendent value!
 *  required paired correlative strings
 *  in s_user_trace_level
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
    TRACE_MESSAGES2 = 0x0002,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{"messages2",       "Trace messages with data"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t pm_authz_sample[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "param sample",    0,              0,          "Param ..."),

/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATA_END()
};

PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name------------flag----alias---items---------------description--*/
SDATAAUTHZ (DTP_SCHEMA, "sample",       0,      0,      pm_authz_sample,    "Permission to ..."),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int32_t timeout;
    hgobj timer;

    json_t *jobs;
    int max_job;
    int idx_job;

    hgobj gobj_jobs;
    hgobj gobj_results;
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

    priv->timer = gobj_create(gobj_name(gobj), C_TIMER, 0, gobj);

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    json_t *jn_gobj_results = gobj_read_json_attr(gobj, "gobj_results");
    if(json_is_integer(jn_gobj_results)) {
        priv->gobj_results = (hgobj)(size_t)json_integer_value(jn_gobj_results);
        if(gobj_is_volatil(priv->gobj_results)) {
            gobj_log_error(gobj, 0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "WARNING don't use volatil gobjs",
                "dst",          "%s", gobj_name(priv->gobj_results),
                NULL
            );
        }

    } else if(json_is_string(jn_gobj_results)) {
        const char *sdst = json_string_value(jn_gobj_results);
        priv->gobj_results = gobj_find_service(sdst, TRUE);

    } else {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gobj_results must be a json integer or string",
            "gobj_results", "%j", jn_gobj_results,
            NULL
        );
    }

    json_t *jn_gobj_jobs = gobj_read_json_attr(gobj, "gobj_jobs");
    if(json_is_integer(jn_gobj_jobs)) {
        priv->gobj_jobs = (hgobj)(size_t)json_integer_value(jn_gobj_jobs);
        if(gobj_is_volatil(priv->gobj_jobs)) {
            gobj_log_error(gobj, 0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "WARNING don't use volatil gobjs",
                "dst",          "%s", gobj_name(priv->gobj_jobs),
                NULL
            );
        }

    } else if(json_is_string(jn_gobj_jobs)) {
        const char *sdst = json_string_value(jn_gobj_jobs);
        priv->gobj_jobs = gobj_find_service(sdst, TRUE);

    } else {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gobj_jobs must be a json integer or string",
            "gobj_jobs", "%j", jn_gobj_jobs,
            NULL
        );
    }

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
    SET_PRIV(jobs,                  gobj_read_json_attr)

    priv->max_job = json_array_size(priv->jobs);
    gobj_set_bottom_gobj(gobj, priv->gobj_results);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,             gobj_read_integer_attr)
    END_EQ_SET_PRIV()
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Subscribe to gobj_results events
     *  TODO gobj_results puede dar soporte a varias tasks,
     *  hay que suscribirse con algún tipo de id.
     *  Si tienes multi tasks puedes solucionar este problema
     *  usando Object with __queries_in_queue__, mira el ejemplo de Dba_postgres y Postgres
     */
    gobj_subscribe_event(priv->gobj_results, NULL, 0, gobj);

    gobj_start(priv->timer);

    priv->idx_job = 0; // First job at start
    if(gobj_read_bool_attr(gobj, "connected")) {
        execute_action(gobj);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->idx_job = 0; // Reset First job
    gobj_unsubscribe_event(priv->gobj_results, NULL, NULL, gobj);
    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    return 0;
}




            /***************************
             *      Commands
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw);
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_authzs_doc(gobj, cmd, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Task End will be called with next result values:

    switch(result) {
        case -1:
            // Error from some task action
            gobj_log_error(gobj, 0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Task End with error",
                "comment",      "%s", comment,
                "src",          "%s", gobj_full_name(src),
                NULL
            );
            gobj_trace_json(gobj, kw, "Task End with error");
            break;
        case -2:
            // Error from task manager: timeout, incomplete task
            gobj_log_error(gobj, 0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP_ERROR,
                "msg",          "%s", "Task End by timeout",
                "comment",      "%s", comment,
                "src",          "%s", gobj_full_name(src),
                NULL
            );
            gobj_trace_json(gobj, kw, "Task End by timeout");
            break;
        case 0:
            // Task end ok
            break;
    }
 ***************************************************************************/
PRIVATE int stop_task(hgobj gobj, int result)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_task = json_pack("{s:i, s:i, s:O, s:O}",
        "result", result,
        "last_job", priv->idx_job,
        "input_data", gobj_read_json_attr(gobj, "input_data"),
        "output_data", gobj_read_json_attr(gobj, "output_data")
    );
    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        if(result < 0) {
            if(result == -2) {
                gobj_trace_msg(gobj, "💎💎Task END ⏫⏫ ⏳ ERROR, gobj %s", gobj_name(gobj));
            } else {
                gobj_trace_msg(gobj, "💎💎Task END ⏫⏫ 🔴 ERROR, gobj %s", gobj_name(gobj));
            }
        } else {
            gobj_trace_msg(gobj, "💎💎Task END ⏫⏫ 🔵 OK, gobj %s", gobj_name(gobj));
        }
    }
    if(gobj_trace_level(gobj) & TRACE_MESSAGES2) {
        if(result < 0) {
            if(result == -2) {
                gobj_trace_json(gobj, kw_task, "💎💎Task END ⏫⏫ ⏳ ERROR, gobj %s", gobj_name(gobj));
            } else {
                gobj_trace_json(gobj, kw_task, "💎💎Task END ⏫⏫ 🔴 ERROR, gobj %s", gobj_name(gobj));
            }
        } else {
            gobj_trace_json(gobj, kw_task, "💎💎Task END ⏫⏫ 🔵 OK, gobj %s", gobj_name(gobj));
        }
    }

    gobj_publish_event(gobj,
        EV_END_TASK,
        kw_task
    );
    gobj_stop(gobj);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int execute_action(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->idx_job > priv->max_job) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "cur_job index overflow",
            "cur_job",      "%d", priv->idx_job,
            "max_job",      "%d", priv->max_job,
            NULL
        );
        stop_task(gobj, -2); // -2 WARNING about incomplete task
        return 0;
    }
    json_t *jn_job_ = json_array_get(priv->jobs, priv->idx_job);
    if(!jn_job_) {
        stop_task(gobj, 0);
        return 0;
    }

    const char *action = kw_get_str(gobj, jn_job_, "exec_action", "", KW_REQUIRED);
    json_int_t exec_timeout = kw_get_int(gobj, jn_job_, "exec_timeout", priv->timeout, 0);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "💎💎Task ⏩ exec ACTION %s(%d '%s')", gobj_name(gobj), priv->idx_job, action);
    }

    int ret = (int)(size_t)gobj_local_method(
        priv->gobj_jobs,
        action,
        0,
        gobj
    );

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        if(ret < 0) {
            gobj_trace_msg(gobj, "💎💎Task ⏪ exec ACTION %s(%d '%s') 🔴 ERROR",
                gobj_name(gobj), priv->idx_job, action
            );
        } else {
            gobj_trace_msg(gobj, "💎💎Task ⏪ exec ACTION %s(%d '%s') 🔵 OK",
                gobj_name(gobj), priv->idx_job, action
            );
        }
    }
    if(gobj_trace_level(gobj) & TRACE_MESSAGES2) {
        json_t *kw_task = json_pack("{s:O, s:O}",
            "input_data", gobj_read_json_attr(gobj, "input_data"),
            "output_data", gobj_read_json_attr(gobj, "output_data")
        );
        if(ret < 0) {
            gobj_trace_json(gobj, kw_task,
                "💎💎Task ⏪ exec ACTION %s(%d '%s') 🔴 ERROR",
                gobj_name(gobj), priv->idx_job, action
            );
        } else {
            gobj_trace_json(gobj, kw_task,
                "💎💎Task ⏪ exec ACTION %s(%d '%s') 🔵 OK",
                gobj_name(gobj), priv->idx_job, action
            );
        }
        json_decref(kw_task);
    }

    if(ret < 0) {
        stop_task(gobj, -1);
    } else {
        set_timeout(priv->timer, exec_timeout);
    }

    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Connected
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    execute_action(gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Disconnected
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_job_ = json_array_get(priv->jobs, priv->idx_job);

    const char *action = kw_get_str(gobj, jn_job_, "exec_result", "", KW_REQUIRED);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "💎💎Task ⏩ exec RESULT %s(%d '%s')", gobj_name(gobj), priv->idx_job, action);
    }

    int ret = (int)(size_t)gobj_local_method(
        priv->gobj_jobs,
        action,
        json_incref(kw),
        gobj
    );

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        if(ret < 0) {
            gobj_trace_msg(gobj, "💎💎Task ⏪ exec RESULT %s(%d '%s') 🔴 ERROR",
                gobj_name(gobj), priv->idx_job, action
            );
        } else {
            gobj_trace_msg(gobj, "💎💎Task ⏪ exec RESULT %s(%d '%s') 🔵 OK",
                gobj_name(gobj), priv->idx_job, action
            );
        }
    }
    if(gobj_trace_level(gobj) & TRACE_MESSAGES2) {
        json_t *kw_task = json_pack("{s:O, s:O}",
            "input_data", gobj_read_json_attr(gobj, "input_data"),
            "output_data", gobj_read_json_attr(gobj, "output_data")
        );
        if(ret < 0) {
            gobj_trace_json(gobj, kw_task,
                "💎💎Task ⏪ exec RESULT %s(%d '%s') 🔴 ERROR",
                gobj_name(gobj), priv->idx_job, action
            );
        } else {
            gobj_trace_json(gobj, kw_task,
                "💎💎Task ⏪ exec RESULT %s(%d '%s') 🔵 OK",
                gobj_name(gobj), priv->idx_job, action
            );
        }
        json_decref(kw_task);
    }

    if(ret < 0) {
        stop_task(gobj, -1);
    } else {
        priv->idx_job++;
        execute_action(gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    if(gobj_is_volatil(gobj)) {
        gobj_destroy(gobj);
    }
    return 0;
}

/***************************************************************************
 *  Timeout waiting result
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    json_t *output_data = gobj_read_json_attr(gobj, "output_data");
    json_object_set_new(
        output_data,
        "comment",
        json_sprintf("Task failed by timeout")
    );

    KW_DECREF(kw)
    stop_task(gobj, -2); // -2 WARNING about incomplete task
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
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TASK);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_END_TASK);


/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        {EV_ON_OPEN,                ac_on_open,             0},
        {EV_ON_CLOSE,               ac_on_close,            0},
        {EV_TIMEOUT,                ac_timeout,             0},
        {EV_STOPPED,                ac_stopped,             0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_END_TASK,       EVF_OUTPUT_EVENT},

        {EV_ON_MESSAGE,     0},
        {EV_ON_OPEN,        0},
        {EV_ON_CLOSE,       0},
        {EV_TIMEOUT,        0},
        {EV_STOPPED,        0},

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
        0,  //lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,  // authz_table,
        command_table,  // command_table,
        s_user_trace_level,  // s_user_trace_level
        0   // gclass_flag
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
PUBLIC int register_c_task(void)
{
    return create_gclass(C_TASK);
}
