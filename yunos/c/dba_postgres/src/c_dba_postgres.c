/***********************************************************************
 *          C_DBA_POSTGRES.C
 *          Dba_postgres GClass.
 *
 *          DBA Dba_postgres
 *
 *          Copyright (c) 2021 by Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <c_postgres.h>
#include "c_dba_postgres.h"


/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *record2createtable(
    hgobj gobj,
    json_t *schema // not owned
);
PRIVATE json_t *record2insertsql(
    hgobj gobj,
    json_t *schema, // not owned
    json_t *record // not owned
);

PRIVATE int send_ack(
    hgobj gobj,
    json_t *kw_ack,  // owned
    json_t *__temp__ // channel info
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-------json_fn---------description--*/
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,    cmd_help,       "Command's help"),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name------------flag--------------------default-----description--*/
SDATA (DTP_STRING,      "filename_mask",SDF_RD|SDF_REQUIRED,    "%Y-%m",    "System organization of tables (file name format, see strftime())"),
SDATA (DTP_BOOLEAN,     "master",       SDF_RD,                 "1",       "the master is the only that can write"),
SDATA (DTP_INTEGER,     "xpermission",  SDF_RD,                 "02770",    "Use in creation, default 02770"),
SDATA (DTP_INTEGER,     "rpermission",  SDF_RD,                 "0660",     "Use in creation, default 0660"),
SDATA (DTP_INTEGER,     "exit_on_error",0,                      "2",        "exit on error"),
SDATA (DTP_INTEGER,     "txMsgs",       SDF_RD|SDF_RSTATS,      0,          "Messages transmitted"),
SDATA (DTP_INTEGER,     "rxMsgs",       SDF_RD|SDF_RSTATS,      0,          "Messages receiveds"),

SDATA (DTP_INTEGER,     "txMsgsec",     SDF_RD|SDF_RSTATS,      0,          "Messages by second"),
SDATA (DTP_INTEGER,     "rxMsgsec",     SDF_RD|SDF_RSTATS,      0,          "Messages by second"),
SDATA (DTP_INTEGER,     "maxtxMsgsec",  SDF_WR|SDF_RSTATS,      0,          "Max Tx Messages by second"),
SDATA (DTP_INTEGER,     "maxrxMsgsec",  SDF_WR|SDF_RSTATS,      0,          "Max Rx Messages by second"),

SDATA (DTP_INTEGER,     "timeout",      SDF_RD,                 "1000",     "Timeout"),
SDATA (DTP_POINTER,     "user_data",    0,                      0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",   0,                      0,          "more user data"),
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
    int32_t timeout;
    hgobj timer;

    hgobj gobj_input_side;
    hgobj gobj_postgres;

    int32_t exit_on_error;

    uint64_t txMsgs;
    uint64_t rxMsgs;
    uint64_t txMsgsec;
    uint64_t rxMsgsec;
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

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
    SET_PRIV(exit_on_error,             gobj_read_integer_attr)

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,         gobj_read_integer_attr)
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

    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 *  Yuneta rule:
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Start services
     */
    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    priv->gobj_postgres = gobj_find_service("__postgres__", TRUE);
    // HACK Don't subscribe events, will do the tasks
    gobj_start_tree(priv->gobj_postgres);

    /*
     *  Periodic timer
     */
    set_timeout_periodic(priv->timer, priv->timeout);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Stop services
     */
    gobj_unsubscribe_event(priv->gobj_input_side, 0, 0, gobj);
    EXEC_AND_RESET(gobj_stop_tree, priv->gobj_input_side);

    EXEC_AND_RESET(gobj_stop_tree, priv->gobj_postgres);

    clear_timeout(priv->timer);

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




            /***************************
             *      Jobs
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *action_create_table_if_not_exists(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src // Source is the GCLASS_TASK
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *input_data = gobj_read_json_attr(src, "input_data");
    json_t *schema_ = kw_get_dict(gobj, input_data, "_dba_postgres`schema", 0, KW_REQUIRED);

    char id[NAME_MAX];
    snprintf(id, sizeof(id), "%s", gobj_name(src));

    json_t *query = json_pack("{s:s, s:s, s:o}",
        "id", id,
        "dst", gobj_name(src),
        "query",
        record2createtable(
            gobj,
            schema_
        )
    );
    gobj_send_event(priv->gobj_postgres, EV_SEND_QUERY, query, gobj);

    KW_DECREF(kw);
    CONTINUE_TASK();
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *result_create_table_if_not_exists(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src // Source is the GCLASS_TASK
)
{
    int result = kw_get_int(gobj, kw, "result", -1, KW_REQUIRED);
    if(result == 0) {
        KW_DECREF(kw);
        CONTINUE_TASK();
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_POSTGRES_ERROR,
            "msg",          "%s", "Cannot create table",
            NULL
        );
        gobj_trace_json(gobj, kw, "Cannot create table");
        KW_DECREF(kw);
        STOP_TASK();
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *action_add_row(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src // Source is the GCLASS_TASK
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *input_data = gobj_read_json_attr(src, "input_data");
    json_t *schema_ = kw_get_dict(gobj, input_data, "_dba_postgres`schema", 0, KW_REQUIRED);

    char id[NAME_MAX];
    snprintf(id, sizeof(id), "%s", gobj_name(src));

    json_t *query = json_pack("{s:s, s:s, s:o}",
        "id", id,
        "dst", gobj_name(src),
        "query",
        record2insertsql(
            gobj,
            schema_,
            input_data
        )
    );
    gobj_send_event(priv->gobj_postgres, EV_SEND_QUERY, query, gobj);

    KW_DECREF(kw);
    CONTINUE_TASK();
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *result_add_row(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src // Source is the GCLASS_TASK
)
{
    int result = kw_get_int(gobj, kw, "result", -1, KW_REQUIRED);
    if(result == 0 || 1) { // Send ack always
        // TODO quito el send ack always? y compruebo tipo de error?
        json_t *input_data = gobj_read_json_attr(src, "input_data");
        json_t *__temp__ = kw_get_dict_value(gobj, input_data, "__temp__", 0, KW_REQUIRED|KW_EXTRACT);

        json_t *kw_ack = trq_answer(
            input_data,  // not owned
            0
        );

        send_ack(
            gobj,
            kw_ack, // owned
            __temp__ // owned, Set the channel
        );

        if(result < 0) {
            char temp[512];
            const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
            snprintf(temp, sizeof(temp), "Postgres: cannot add row -> %s", comment);
            char *p = strchr(temp, '\n');
            if(p) {
                *p = 0;
            }
            left_justify(temp);
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_POSTGRES_ERROR,
                "msg",          "%s", temp,
                NULL
            );
            gobj_trace_json(gobj, kw, "%s", temp);
        }
    }

    KW_DECREF(kw);
    CONTINUE_TASK();
}

// /***************************************************************************
//  *
//  ***************************************************************************/
// PRIVATE json_t *action_list_rows(
//     hgobj gobj,
//     const char *lmethod,
//     json_t *kw,
//     hgobj src // Source is the GCLASS_TASK
// )
// {
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);
//
//     json_t *query = json_pack("{s:s, s:s, s:o}",
//         "id", ???
//         "dst", gobj_name(src),
//         "query", "SELECT * from tracks_geodb2;"
//     );
//     gobj_send_event(priv->gobj_postgres, EV_SEND_QUERY, query, gobj);
//
//     KW_DECREF(kw);
//     return (void *)0; // continue
// }
//
// /***************************************************************************
//  *
//  ***************************************************************************/
// PRIVATE json_t *result_list_rows(
//     hgobj gobj,
//     const char *lmethod,
//     json_t *kw,
//     hgobj src // Source is the GCLASS_TASK
// )
// {
//
//     KW_DECREF(kw);
//     return (void *)0; // continue
// }




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Send ack to __input_side__
 ***************************************************************************/
PRIVATE int send_ack(
    hgobj gobj,
    json_t *kw_ack,  // owned
    json_t *__temp__ // owned, channel info
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(gobj, kw_ack,
            "🔄🔄🔄🔄Dba_postgres 👈👈 %"JSON_INTEGER_FORMAT" %s ==> %s",
            kw_get_int(gobj, kw_ack, __MD_TRQ__"`__msg_key__", 0, KW_REQUIRED),
            gobj_short_name(gobj),
            gobj_short_name(priv->gobj_input_side)
        );
    }

    gbuffer_t *gbuf = json2gbuf(0, kw_ack, JSON_COMPACT);
    json_t *kw_send = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );

    json_object_set_new(kw_send, "__temp__", __temp__);  // Set the channel

    priv->txMsgs++;
    priv->txMsgsec++;

    return gobj_send_event(priv->gobj_input_side, EV_SEND_MESSAGE, kw_send, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *record2createtable(
    hgobj gobj,
    json_t *schema // not owned
)
{
    const char *topic_name = kw_get_str(gobj, schema, "id", "", KW_REQUIRED);
    const char *pkey = kw_get_str(gobj, schema, "pkey", "", KW_REQUIRED);
    // TODO add secondary keys
    //const char *pkey2s = kw_get_str(gobj, schema, "pkey2s", "", KW_REQUIRED);
    BOOL use_header = kw_get_bool(gobj, schema, "use_header", 0, KW_REQUIRED);

    gbuffer_t *gbuf = gbuffer_create(8*1024, 8*1024);

    gbuffer_printf(gbuf,
        "CREATE TABLE IF NOT EXISTS %s (",
        topic_name
    );
    json_t *cols = kw_get_dict(gobj, schema, "cols", 0, KW_REQUIRED);

    int idx = 0;
    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        if(idx > 0) {
            gbuffer_printf(gbuf, "," );
        }
        const char *header = kw_get_str(gobj, col, "header", "", KW_REQUIRED);
        const char *type = kw_get_str(gobj, col, "type", "", KW_REQUIRED);
        SWITCHS(type) {
            CASES("str")
            CASES("string")
                gbuffer_printf(gbuf, "%s %s",
                    use_header?header:col_name,
                    "text"
                );
                break;

            CASES("int")
            CASES("integer")
                gbuffer_printf(gbuf, "%s %s",
                    use_header?header:col_name,
                    "bigint"
                );
                break;

            CASES("time")
                gbuffer_printf(gbuf, "%s %s",
                    use_header?header:col_name,
                    "timestamp"
                );
                break;

            CASES("real")
                gbuffer_printf(gbuf, "%s %s",
                    use_header?header:col_name,
                    "double precision"
                );
                break;

            CASES("bool")
            CASES("boolean")
                gbuffer_printf(gbuf, "%s %s",
                    use_header?header:col_name,
                    "boolean"
                );
                break;

            DEFAULTS
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_POSTGRES_ERROR,
                    "msg",          "%s", "Type header UNKNOWN",
                    "type",         "%s", type,
                    NULL
                );
                break;
        } SWITCHS_END;

        if(strcmp(col_name, pkey)==0) {
            gbuffer_printf(gbuf, " PRIMARY KEY");
        }
        idx++;
    }

    if(1) {
        // Añade automaticamente campo creacion
        if(idx > 0) {
            gbuffer_printf(gbuf, "," );
        }
        gbuffer_printf(gbuf, "__created_at__ TIMESTAMPTZ NOT NULL DEFAULT NOW()");
    }

    gbuffer_printf(gbuf,
        ");"
    );

    char *p = gbuffer_cur_rd_pointer(gbuf);
    json_t *jn_query = json_string(p);

    gbuffer_decref(gbuf);

    return jn_query;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *record2insertsql(
    hgobj gobj,
    json_t *schema, // not owned
    json_t *record // not owned
)
{
    const char *topic_name = kw_get_str(gobj, schema, "id", "", KW_REQUIRED);
    BOOL use_header = kw_get_bool(gobj, schema, "use_header", 0, KW_REQUIRED);

    gbuffer_t *gbuf = gbuffer_create(32*1024, 32*1024);

    gbuffer_printf(gbuf, "INSERT INTO %s (", topic_name);

    json_t *cols = kw_get_dict(gobj, schema, "cols", 0, KW_REQUIRED);

    int idx = 0;
    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        if(idx > 0) {
            gbuffer_printf(gbuf, "," );
        }
        if(use_header) {
            const char *header = kw_get_str(gobj, col, "header", "", KW_REQUIRED);
            gbuffer_printf(gbuf, "%s", header);
        } else {
            gbuffer_printf(gbuf, "%s", col_name);
        }

        idx++;
    }

    gbuffer_printf(gbuf, ") VALUES (");

    idx = 0;
    json_object_foreach(cols, col_name, col) {
        if(idx > 0) {
            gbuffer_printf(gbuf, "," );
        }

        const char *type = kw_get_str(gobj, col, "type", "", KW_REQUIRED);
        SWITCHS(type) {
            CASES("str")
            CASES("string")
                json_t *value = kw_get_dict_value(gobj, record, col_name, 0, KW_REQUIRED);
                if(value) {
                    char *s = json2uglystr(value);
                    change_char(s, '"', '\'');
                    // TODO IMPORTANTE char *ss = PQescapeLiteral(priv->conn, s, strlen(s));
                    gbuffer_append_string(gbuf, s);
                    gbmem_free(s);
                }
                break;

            CASES("int")
            CASES("integer")
                json_int_t value = kw_get_int(gobj, record, col_name, 0, KW_REQUIRED|KW_WILD_NUMBER);
                gbuffer_printf(gbuf, "%"JSON_INTEGER_FORMAT, value);
                break;

            CASES("time")
                json_t *value = kw_get_dict_value(gobj, record, col_name, 0, KW_REQUIRED);
                if(value) {
                    char *s = json2uglystr(value);
                    char temp[256];
                    snprintf(temp, sizeof(temp),
                        "('epoch'::timestamp + %s * '1 second'::interval)", s
                    );
                    gbmem_free(s);

                    s = gbmem_strdup(temp);
                    gbuffer_append_string(gbuf, s);
                    gbmem_free(s);
                }
                break;

            CASES("real")
                double value = kw_get_real(gobj, record, col_name, 0, KW_REQUIRED|KW_WILD_NUMBER);
                gbuffer_printf(gbuf, "%f", value);
                break;

            CASES("bool")
            CASES("boolean")
                BOOL value = kw_get_bool(gobj, record, col_name, 0, KW_REQUIRED|KW_WILD_NUMBER);
                gbuffer_printf(gbuf, "%s", value?"TRUE":"FALSE");
                break;

            DEFAULTS
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_POSTGRES_ERROR,
                    "msg",          "%s", "Type header UNKNOWN",
                    "type",         "%s", type,
                    NULL
                );
                break;
        } SWITCHS_END;

        idx++;
    }

    gbuffer_printf(gbuf, ");");
    char *p = gbuffer_cur_rd_pointer(gbuf);
    json_t *jn_query = json_string(p);

    gbuffer_decref(gbuf);

    return jn_query;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int process_msg(
    hgobj gobj,
    json_t *kw,  // NOT owned
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------*
     *      Build task name
     *-----------------------------*/
    const char *id = kw_get_str(gobj, kw, "id", "", KW_REQUIRED);
    json_int_t __msg_key__ = kw_get_int(gobj, kw, "__md_trq__`__msg_key__", 0, KW_REQUIRED);
    if(!__msg_key__) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Not __msg_key__, free queue's msg",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
        return 0; // free the queue's msg
    }

    char task_name[NAME_MAX];
    snprintf(task_name, sizeof(task_name), "task-%s-%"JSON_INTEGER_FORMAT, id, __msg_key__);

    /*-----------------------------*
     *      Check if exists task
     *-----------------------------*/
    if(gobj_find_service(task_name, FALSE)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "task already active",
            "task_name",    "%s", task_name,
            NULL
        );
        return -1; // Don't send ack
    }

    /*-----------------------------*
     *      Create the task
     *-----------------------------*/
    json_t *kw_task = json_pack(
        "{s:o, s:o, s:O, s:["
            "{s:s, s:s, s:i}"
            "]}",
        "gobj_jobs", json_integer((json_int_t)(uintptr_t)gobj),
        "gobj_results", json_integer((json_int_t)(uintptr_t)priv->gobj_postgres),
        "input_data", kw,
        "jobs",
            "exec_action", "action_add_row",
            "exec_result", "result_add_row",
            "exec_timeout", 20*1000
    );

    hgobj gobj_task = gobj_create_service(task_name, C_TASK, kw_task, gobj);
    gobj_subscribe_event(gobj_task, EV_END_TASK, 0, gobj);
    gobj_set_volatil(gobj_task, TRUE); // auto-destroy

    /*-----------------------*
     *      Start task
     *-----------------------*/
    gobj_start(gobj_task);

    return -1; // Don't send ack
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Connection from
 *      Input gates (__input_side__)
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "What fuck from?",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Disconnection from
 *      Input gates (__input_side__)
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "What fuck from?",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Message from input gates
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int ret = 0;

    priv->rxMsgs++;
    priv->rxMsgsec++;

    if(src == priv->gobj_input_side ) {
        gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

        gbuffer_incref(gbuf);
        json_t *jn_msg = gbuf2json(gbuf, 2);

        if(jn_msg) {
            hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
            if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
                gobj_trace_json(gobj, jn_msg,
                        "🔄🔄🔄🔄Dba_postgres 👉👉 %"JSON_INTEGER_FORMAT" %s <== %s <== %s",
                    kw_get_int(gobj, jn_msg, __MD_TRQ__"`__msg_key__", 0, KW_REQUIRED),
                    gobj_short_name(gobj),
                    gobj_short_name(src),
                    gobj_short_name(channel_gobj)
                );
            }

            json_t *__temp__ = kw_get_dict_value(gobj, kw, "__temp__", 0, KW_REQUIRED|KW_EXTRACT);
            json_object_set_new(jn_msg, "__temp__", __temp__);
            process_msg(gobj, jn_msg, src);
            JSON_DECREF(jn_msg);

        } else {
            ret = -1;
        }
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "What fuck from?",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
    }


    KW_DECREF(kw);
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_end_task(
    hgobj gobj,
    const char *event,
    json_t *kw,
    hgobj src // Source is the GCLASS_TASK
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int result = kw_get_int(gobj, kw, "result", -1, KW_REQUIRED);
    int last_job = kw_get_int(gobj, kw, "last_job", 0, KW_REQUIRED);
    const char *comment = kw_get_str(gobj, kw, "comment", "", 0);

    switch(result) {
        case -1:
            // Error from some task action
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_POSTGRES_ERROR,
                "msg",          "%s", "Task End with error",
                "comment",      "%s", comment,
                "last_job",     "%d", last_job,
                "src",          "%s", gobj_full_name(src),
                NULL
            );
            gobj_trace_json(gobj, kw, "Task End with error");
            break;
        case -2:
            // Error from task manager: timeout, incomplete task
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_POSTGRES_ERROR,
                "msg",          "%s", "Task End by timeout",
                "comment",      "%s", comment,
                "last_job",     "%d", last_job,
                "src",          "%s", gobj_full_name(src),
                NULL
            );
            gobj_trace_json(gobj, kw, "Task End by timeout");

            json_t *kw_clear = json_object();
            json_object_set_new(kw_clear, "id", json_string(gobj_name(src)));
            gobj_send_event(priv->gobj_postgres, EV_CLEAR_QUEUE, kw_clear, gobj);

            break;
        case 0:
            // Task end ok
            break;
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint64_t maxtxMsgsec = gobj_read_integer_attr(gobj, "maxtxMsgsec");
    uint64_t maxrxMsgsec = gobj_read_integer_attr(gobj, "maxrxMsgsec");
    if(priv->txMsgsec > maxtxMsgsec) {
        gobj_write_integer_attr(gobj, "maxtxMsgsec", priv->txMsgsec);
    }
    if(priv->rxMsgsec > maxrxMsgsec) {
        gobj_write_integer_attr(gobj, "maxrxMsgsec", priv->rxMsgsec);
    }

    gobj_write_integer_attr(gobj, "txMsgsec", priv->txMsgsec);
    gobj_write_integer_attr(gobj, "rxMsgsec", priv->rxMsgsec);

    priv->rxMsgsec = 0;
    priv->txMsgsec = 0;

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create      = mt_create,
    .mt_destroy     = mt_destroy,
    .mt_start       = mt_start,
    .mt_stop        = mt_stop,
    .mt_play        = mt_play,
    .mt_pause       = mt_pause,
    .mt_writing     = mt_writing
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_DBA_POSTGRES);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
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
        {EV_ON_MESSAGE,     ac_on_message,      0},
        {EV_ON_OPEN,        ac_on_open,         0},
        {EV_ON_CLOSE,       ac_on_close,        0},
        {EV_END_TASK,       ac_end_task,        0},
        {EV_TIMEOUT,        ac_timeout,         0},
        {EV_STOPPED,        0,                  0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_ON_MESSAGE,     0},
        {EV_END_TASK,       0},
        {EV_ON_OPEN,        0},
        {EV_ON_CLOSE,       0},
        {EV_TIMEOUT,        0},
        {EV_STOPPED,        0},
        {NULL, 0}
    };

    /*------------------------*
     *      Local methods
     *------------------------*/
    LMETHOD lmt[] = {
        {"action_add_row",                      action_add_row, 0},
        {"result_add_row",                      result_add_row, 0},
        {"action_create_table_if_not_exists",   action_create_table_if_not_exists, 0},
        {"result_create_table_if_not_exists",   result_create_table_if_not_exists, 0},
        {0, 0, 0}
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
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,              // acl
        command_table,
        s_user_trace_level,
        0               // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Local methods table
 ***************************************************************************/

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_dba_postgres(void)
{
    return create_gclass(C_DBA_POSTGRES);
}
