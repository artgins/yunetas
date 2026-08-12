/***********************************************************************
 *          C_TEST_SCHEMA_FIDELITY.C
 *
 *          Every REAL schema of this tree, through the projection.
 *
 *          The __system__ treedb stores a schema as data, and a treedb
 *          opened from that projection gets whatever the projection kept.
 *          What it drops, it drops in silence: a column that lost its
 *          `enum` list keeps its `enum` FLAG, so it declares an
 *          enumeration it no longer has and every value passes.
 *
 *          So the schemas that ship here are swept: each one is opened
 *          from its projection, and every attribute the literal declares
 *          must come back with the same value. Attributes the literal
 *          does NOT declare are ignored — the projection is allowed to
 *          fill in the defaults a column gets anyway.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <limits.h>

#include "c_test_schema_fidelity.h"

/*
 *  The real schemas. Included as source, the way the yunos that own them
 *  do it: they are file-scope literals, not a library.
 */
#include "../../../../kernel/c/root-linux/src/treedb_schema_authzs.c"
#include "../../../../modules/c/mqtt/src/treedb_schema_mqtt_broker.c"
#include "../../../../yunos/c/controlcenter/src/treedb_schema_controlcenter.c"
#include "../../../../yunos/c/yuno_agent/src/treedb_schema_yuneta_agent.c"

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char *name;
    char *literal;
} schema_under_test_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level of help"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-------json_fn---------description--*/
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,    cmd_help,       "Command's help"),
SDATA_END()
};

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
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name--------------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *      Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_treedbs;
    hgobj timer;
    char path_database[PATH_MAX];
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

    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(
        priv->path_database,
        sizeof(priv->path_database),
        path_root,
        "treedb_schema_fidelity",
        NULL
    );
    rmrdir(priv->path_database);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
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
    clear_timeout(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  ONE store for all of them, on purpose: `users` is a topic of three of
     *  these four schemas, and holding them together is what used to break —
     *  `topics` was keyed by the bare topic name, so the second treedb
     *  declaring `users` collided with the first, lost its topic node, and its
     *  rebuilt schema failed to parse while the code fell back to the literal
     *  in silence.
     */
    json_t *kw_treedbs = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i}",
        "path", priv->path_database,
        "filename_mask", "%Y",
        "master", 1,
        "xpermission", 02770,
        "rpermission", 0660,
        "exit_on_error", LOG_OPT_TRACE_STACK
    );
    priv->gobj_treedbs = gobj_create_service(
        "treedbs",
        C_TREEDB,
        kw_treedbs,
        gobj
    );
    gobj_start_tree(priv->gobj_treedbs);

    set_timeout(priv->timer, 100);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->gobj_treedbs) {
        gobj_stop_tree(priv->gobj_treedbs);
    }
    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The column of `topic_name` named `col_name`, as the treedb serves it
 *  now — which is what the projection rebuilt. Return is NOT yours.
 ***************************************************************************/
PRIVATE json_t *rebuilt_col(json_t *desc, const char *col_name)
{
    hgobj gobj = 0;
    int idx; json_t *col;
    json_array_foreach(json_object_get(desc, "cols"), idx, col) {
        if(strcmp(kw_get_str(gobj, col, "id", "", 0), col_name)==0) {
            return col;
        }
    }
    return NULL;
}

/***************************************************************************
 *  Open one schema from its own projection and compare it, attribute by
 *  attribute, with the literal it came from.
 ***************************************************************************/
PRIVATE int sweep_schema(hgobj gobj, const char *name, char *literal)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;


    helper_quote2doublequote(literal);
    json_t *jn_schema = legalstring2json(literal, TRUE);
    if(!jn_schema) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: schema literal does not parse",
            "schema",       "%s", name,
            NULL
        );
        return -1;
    }

    const char *treedb_name = kw_get_str(gobj, jn_schema, "id", "", 0);
    if(empty_string(treedb_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: schema without id",
            "schema",       "%s", name,
            NULL
        );
        JSON_DECREF(jn_schema)
        return -1;
    }

    json_t *kw_treedb = json_pack("{s:s, s:i, s:s, s:O, s:b}",
        "filename_mask", "%Y",
        "exit_on_error", 0,
        "treedb_name", treedb_name,
        "treedb_schema", jn_schema,
        "use_internal_schema", 0
    );
    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "open-treedb", kw_treedb, gobj);
    int ret = (int)kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    JSON_DECREF(jn_resp)
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: cannot open the schema",
            "schema",       "%s", name,
            NULL
        );
        JSON_DECREF(jn_schema)
        return -1;
    }

    hgobj gobj_client_node = gobj_find_service(treedb_name, FALSE);
    if(!gobj_client_node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: treedb service not found",
            "schema",       "%s", name,
            NULL
        );
        JSON_DECREF(jn_schema)
        return -1;
    }

    /*
     *  Compare, for every column, every attribute the LITERAL declares
     */
    json_t *jn_topics = kwid_new_list(gobj, jn_schema, 0, "topics");
    int idx; json_t *jn_topic;
    json_array_foreach(jn_topics, idx, jn_topic) {
        const char *topic_name = kw_get_str(gobj, jn_topic, "id", "", 0);
        if(empty_string(topic_name)) {
            topic_name = kw_get_str(gobj, jn_topic, "topic_name", "", 0);
        }
        if(empty_string(topic_name)) {
            continue;
        }

        json_t *desc = gobj_topic_desc(gobj_client_node, topic_name);
        if(!desc) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: topic did not survive the projection",
                "schema",       "%s", name,
                "topic",        "%s", topic_name,
                NULL
            );
            result += -1;
            continue;
        }

        json_t *jn_cols = kwid_new_dict(gobj, jn_topic, 0, "cols");
        const char *col_name; json_t *jn_col;
        json_object_foreach(jn_cols, col_name, jn_col) {
            json_t *now = rebuilt_col(desc, col_name);
            if(!now) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST FAIL: column did not survive the projection",
                    "schema",       "%s", name,
                    "topic",        "%s", topic_name,
                    "col",          "%s", col_name,
                    NULL
                );
                result += -1;
                continue;
            }

            const char *attr; json_t *want;
            json_object_foreach(jn_col, attr, want) {
                if(strcmp(attr, "id")==0) {
                    continue;   /*  the column's own name  */
                }
                json_t *got = json_object_get(now, attr);
                if(!got || !json_equal(got, want)) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "TEST FAIL: attribute lost or changed by the projection",
                        "schema",       "%s", name,
                        "topic",        "%s", topic_name,
                        "col",          "%s", col_name,
                        "attr",         "%s", attr,
                        "want",         "%j", want,
                        "got",          "%j", got,
                        NULL
                    );
                    result += -1;
                }
            }
        }
        JSON_DECREF(jn_cols)
        JSON_DECREF(desc)
    }
    JSON_DECREF(jn_topics)
    JSON_DECREF(jn_schema)

    /*
     *  This driver holds nothing of the treedb it opened
     */
    jn_resp = gobj_command(
        priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", treedb_name, "force", 1),
        gobj
    );
    JSON_DECREF(jn_resp)

    return result;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  EV_TIMEOUT — the sweep, inside the event loop
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    schema_under_test_t schemas[] = {
        {"authzs",          treedb_schema_authzs},
        {"mqtt_broker",     treedb_schema_mqtt_broker},
        {"controlcenter",   treedb_schema_controlcenter},
        {"yuneta_agent",    treedb_schema_yuneta_agent},
        {NULL, NULL}
    };

    for(int i=0; schemas[i].name; i++) {
        sweep_schema(gobj, schemas[i].name, schemas[i].literal);
    }

    set_yuno_must_die();

    KW_DECREF(kw)
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
    KW_INCREF(kw)
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

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_SCHEMA_FIDELITY);

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

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,         0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,        0},
        {0, 0}
    };

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  //lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0  // gclass_flag
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
PUBLIC int register_c_test_schema_fidelity(void)
{
    return create_gclass(C_TEST_SCHEMA_FIDELITY);
}
