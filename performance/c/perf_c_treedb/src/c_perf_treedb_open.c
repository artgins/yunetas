/***********************************************************************
 *          c_perf_treedb_open.c
 *          Perf_treedb_open GClass.
 *
 *          Benchmark of the open of a dynamic-schema treedb by C_TREEDB,
 *          in a store of many treedbs: what an open costs when the schema
 *          from C (the literal) is seeded, when it is newer than the schema
 *          file in use (a projection into __system__), and when it is the
 *          same (nothing to project).
 *
 *          `treedbs` treedbs of `topics` topics of `cols` columns each
 *          (40 x 10 x 20 by default). One step per timeout of its timer,
 *          so the loop runs between two:
 *            seed            open every treedb with the literal 1 (a first
 *                            projection), close it
 *            newer_literal   open every treedb with the literal 2 (every
 *                            header changed), close it
 *            same_literal    open every treedb with the literal 2 again,
 *                            close it
 *          Each step prints one line of JSON (see main.c) and the yuno
 *          dies after the last one. An open that fails is logged as an
 *          ERROR, and main.c counts them. The store lives in
 *          ~/tests_yuneta/perf_c_treedb and is removed at the end.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#include "c_perf_treedb_open.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BENCH   "perf_c_treedb"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM2 (DTP_SCHEMA,   "authzs",           0,                  0,              pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag----------------default-----description---------- */
SDATA (DTP_POINTER, "subscriber",       0,                  0,          "Subscriber of output-events"),
SDATA (DTP_INTEGER, "treedbs",          SDF_RD,             "40",       "Treedbs in the store"),
SDATA (DTP_INTEGER, "topics",           SDF_RD,             "10",       "Topics of each treedb"),
SDATA (DTP_INTEGER, "cols",             SDF_RD,             "20",       "Columns of each topic (and its id)"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
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
    hgobj timer;
    hgobj gobj_treedbs;
    char path_database[PATH_MAX];
    int treedbs;
    int topics;
    int cols;
    int step;
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

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    } else if(gobj_is_pure_child(gobj)) {
        subscriber = gobj_parent(gobj);
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(priv->path_database, sizeof(priv->path_database), path_root, BENCH, NULL);
    rmrdir(priv->path_database);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(treedbs,               (int)gobj_read_integer_attr)
    SET_PRIV(topics,                (int)gobj_read_integer_attr)
    SET_PRIV(cols,                  (int)gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The children, C_TREEDB and its trangers, are destroyed before this
     */
    rmrdir(priv->path_database);
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
    gobj_stop(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  impose_c_schema off: a newer literal is projected into __system__,
     *  which is the open this benchmark measures
     */
    json_t *kw_treedbs = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i, s:b}",
        "path", priv->path_database,
        "filename_mask", "%Y",
        "master", 1,
        "xpermission", 02770,
        "rpermission", 0660,
        "exit_on_error", LOG_OPT_TRACE_STACK,
        "impose_c_schema", 0
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

    clear_timeout(priv->timer);
    gobj_stop_tree(priv->gobj_treedbs);

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
        0,
        0,
        jn_resp,
        kw  // owned
    );
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The literal of a treedb: `topics` topics t0.. of an id and `cols`
 *  string columns c0.., whose header says the version
 ***************************************************************************/
PRIVATE json_t *literal_of(hgobj gobj, const char *treedb_name, int version)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *topics = json_array();
    for(int t = 0; t < priv->topics; t++) {
        json_t *cols = json_object();
        json_object_set_new(cols, "id", json_pack("{s:s, s:i, s:s, s:[s,s]}",
            "header", "Id", "fillspace", 10, "type", "string", "flag", "persistent", "required"));
        for(int c = 0; c < priv->cols; c++) {
            char col_name[NAME_MAX];
            snprintf(col_name, sizeof(col_name), "c%d", c);
            json_object_set_new(cols, col_name, json_pack("{s:s, s:i, s:s, s:[s]}",
                "header", version > 1? "H2" : "H", "fillspace", 10, "type", "string",
                "flag", "persistent"));
        }
        char topic_name[NAME_MAX];
        snprintf(topic_name, sizeof(topic_name), "t%d", t);
        json_array_append_new(topics, json_pack("{s:s, s:s, s:s, s:i, s:o}",
            "id", topic_name, "pkey", "id", "system_flag", "sf_string_key",
            "topic_version", version, "cols", cols));
    }
    return json_pack("{s:s, s:i, s:o}",
        "id", treedb_name, "schema_version", version, "topics", topics);
}

/***************************************************************************
 *  open-treedb with a literal, then close-treedb. -1 if the open failed
 ***************************************************************************/
PRIVATE int open_and_close(hgobj gobj, const char *treedb_name, int version)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = json_pack("{s:s, s:i, s:s, s:o}",
        "filename_mask", "%Y",
        "exit_on_error", 0,
        "treedb_name", treedb_name,
        "treedb_schema", literal_of(gobj, treedb_name, version)
    );
    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "open-treedb", kw, gobj);
    int result = (int)kw_get_int(gobj, jn_resp, "result", -1, 0);
    if(result < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "open-treedb failed",
            "treedb_name",  "%s", treedb_name,
            "comment",      "%s", kw_get_str(gobj, jn_resp, "comment", "", 0),
            NULL
        );
    }
    JSON_DECREF(jn_resp)

    jn_resp = gobj_command(priv->gobj_treedbs, "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", treedb_name, "force", 1), gobj);
    JSON_DECREF(jn_resp)

    return result < 0? -1 : 0;
}

/***************************************************************************
 *  One phase: every treedb opened with the literal `version`, and closed
 ***************************************************************************/
PRIVATE void run_phase(hgobj gobj, const char *phase, int version)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for(int i = 0; i < priv->treedbs; i++) {
        char treedb_name[NAME_MAX];
        snprintf(treedb_name, sizeof(treedb_name), "perf_%d", i);
        open_and_close(gobj, treedb_name, version);  // Error already logged
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double seconds = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec)/1e9;

    printf("{\"bench\": \"%s\", \"case\": \"%s\", \"seconds\": %.6f, \"ops\": %d, \"us_per_op\": %.3f, "
        "\"treedbs\": %d, \"topics\": %d, \"cols\": %d}\n",
        BENCH, phase, seconds, priv->treedbs,
        priv->treedbs > 0? seconds*1e6/priv->treedbs : 0.0,
        priv->treedbs, priv->topics, priv->cols
    );
    fflush(stdout);
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  ONE phase per timeout: the loop runs between two, and completes what
 *  the closes of the treedbs cancelled
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->step) {
        case 0:
            run_phase(gobj, "seed", 1);
            break;
        case 1:
            run_phase(gobj, "newer_literal", 2);
            break;
        case 2:
            run_phase(gobj, "same_literal", 2);
            break;
        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Timeout after the last phase",
                "step",         "%d", priv->step,
                NULL
            );
            KW_DECREF(kw)
            return -1;
    }
    priv->step++;

    if(priv->step < 3) {
        set_timeout(priv->timer, 10);
    } else {
        set_yuno_must_die();
    }

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
    .mt_writing = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_PERF_TREEDB_OPEN);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

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
        {EV_TIMEOUT,                ac_timeout,              0},
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
        {EV_TIMEOUT,                0},
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
        command_table,
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_perf_treedb_open(void)
{
    return create_gclass(C_PERF_TREEDB_OPEN);
}
