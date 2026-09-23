/***********************************************************************
 *          C_TREEDB.C
 *          Treedb GClass.
 *
 *          Management of treedb's, all in the same root path
 *
 *          - Create a __system__ timeranger
 *          - Create a treedb_system_schema (C_NODE) over the __system__ timeranger
 *
 *          - With commands (by events not yet ready) you can open/close services of treedb
 *
 *          "open-treedb"   -> create a timeranger and a treedb (C_NODE) with the schema passed;
 *                             refused, before anything is touched, for a treedb already open here
 *          "close-treedb"
 *          "delete-treedb"
 *          "create-topic"
 *          "delete-topic"
 *          "diff-schema"   -> what the __system__ projection says that the schema from C does not
 *          "treedbs"       -> the treedbs opened here, and for each one whether
 *                             it opens with its schema from C and who decided it
 *          "save-schema"   -> publish the draft edited in __system__: the versions of
 *                             what differs from the file in use + 1, written to
 *                             saved_schemas/ under the __system__ tranger. A draft
 *                             that is the file in use again withdraws that file
 *          "saved-schema"  -> that saved schema, what it changes, whether it applies
 *          "apply-schema"  -> put it in place of the file in use (master, not imposed)
 *
 *          __SYSTEM__ IS WHERE A SCHEMA IS EDITED, NOT WHERE A TREEDB OPENS
 *          FROM. The master projects into it what the treedb runs: a literal
 *          newer than the schema file in use wins WHOLE -- it replaces the
 *          file, and __system__ is projected from it whole, saying what it
 *          withdrew of the operator's work (`withdrawn_at_open`) -- and a
 *          literal that is not newer leaves the file and __system__ as they
 *          are (user decision, 2026-09-23). An operator edits it there, and an
 *          edit is a DRAFT: it moves no version and reaches no treedb until
 *          save-schema publishes it and apply-schema puts it in the file the
 *          treedb opens from. Until the owner's design of M36 (2026-09-21) it
 *          was the source: every write raised the versions, so an edit half
 *          made was the schema of the next start.
 *
 *          A projection is written whole or it says it is not: a delete that
 *          a snapshot of __system__ refuses (or a write that fails) leaves it
 *          UNFINISHED, said in a warning and in `unfinished_projection`, with
 *          c_schema_version 0. What it could not do is RECORDED in
 *          saved_schemas/<treedb>.unfinished.json: while the record is there
 *          every open retries the projection, what it names is nobody's
 *          draft on any path, and save-schema refuses. An operator's draft
 *          the projection could not replace is not in it: it stays a draft,
 *          and the open that replaces it says it.
 *
 *          Copyright (c) 2021 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <command_parser.h>
#include <tr_treedb.h>

#include "msg_ievent.h"
#include "c_tranger.h"
#include "c_node.h"
#include "c_treedb.h"

#include "treedb_system_schema.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
/*
 *  Attributes of a __system__ node that say how it is STORED, never what it
 *  declares: the qualified id, the name (it is the identity being compared),
 *  the links to parent and children, the version stamp, the editor geometry
 *  and the treedb metadata. Shared by diff-schema and the projector, so both
 *  agree on what "the same" means.
 */
PRIVATE const char *schema_topic_skip[] = {
    "id", "value", "treedbs", "cols", "topic_version", "_geometry", "__md_treedb__", NULL
};
PRIVATE const char *schema_col_skip[] = {
    "id", "value", "topics", "_geometry", "__md_treedb__", NULL
};

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE BOOL configured_to_impose(hgobj gobj, const char *treedb_name);
PRIVATE json_t *get_client_treedb_schema(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_client_treedb_schema, // not owned
    json_t *tranger_client          // not owned, the tranger the treedb opens on
);
PRIVATE json_t *get_c_schema_to_impose(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_c_schema,    // not owned
    json_t *tranger_client  // not owned, the tranger the treedb opens on
);
PRIVATE json_t *get_treedb_schema(
    hgobj gobj,
    const char *treedb_name
);
PRIVATE const char *build_schema_node_id(
    hgobj gobj,
    char *bf,
    int bfsize,
    const char *parent_id,
    const char *name
);
PRIVATE int delete_client_treedb_schema(
    hgobj gobj,
    const char *treedb_name
);
PRIVATE BOOL is_treedb_opened_here(
    hgobj gobj,
    hgobj gobj_node
);
PRIVATE json_t *build_readonly_response(
    hgobj gobj,
    const char *treedb_name,
    json_t *tranger,    // not owned, the tranger that refuses, may be NULL
    json_t *kw          // owned
);
PRIVATE BOOL tranger_is_stopped(json_t *tranger);
PRIVATE BOOL tranger_writes_now(hgobj gobj, json_t *tranger);
PRIVATE json_t *diff_treedb_schema(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_schema,  // the schema from C, not owned
    json_t *rows        // not owned, where the differences are appended
);
PRIVATE json_t *schema_topics_as_list(hgobj gobj, json_t *jn_schema);
PRIVATE json_t *apply_every_saved_schema(hgobj gobj, const char *cmd, json_t *kw);
PRIVATE BOOL treedb_is_written_here(hgobj gobj, const char *treedb_name);
PRIVATE BOOL system_is_written_here(hgobj gobj);
PRIVATE json_t *draft_changed_from_rows(hgobj gobj, json_t *rows);
PRIVATE int remove_saved_schema(hgobj gobj, const char *treedb_name, json_int_t *p_version);
PRIVATE int record_apply(
    hgobj gobj,
    const char *treedb_name,
    json_t *applied,
    json_t *replaced_file,
    json_t **p_previous
);
PRIVATE void restore_apply_record(hgobj gobj, const char *treedb_name, json_t *previous);
PRIVATE void remove_apply_record(hgobj gobj, const char *treedb_name);
PRIVATE void settle_apply_record(hgobj gobj, const char *treedb_name);
PRIVATE json_t *load_unfinished_record(hgobj gobj, const char *treedb_name);
PRIVATE void remove_unfinished_record(hgobj gobj, const char *treedb_name);
PRIVATE json_t *rows_without_leftovers(
    hgobj gobj,
    const char *treedb_name,
    json_t *rows,       // owned
    json_t *leftovers   // not owned, ids of __system__, may be NULL
);
PRIVATE json_t *leftovers_as_left(
    hgobj gobj,
    const char *treedb_name,
    json_t *record,
    json_t *edited
);
PRIVATE json_t *orphan_nodes(hgobj gobj, const char *treedb_name, json_t *tree);
PRIVATE json_t *topic_versions_in_use(
    hgobj gobj,
    const char *treedb_name,
    json_t *file_in_use,    // not owned, may be NULL
    json_t *jn_schema       // not owned, the literal, may be NULL
);
PRIVATE json_int_t topic_version_in_use(json_t *in_use, const char *topic_name);
PRIVATE int diff_node_attrs(
    hgobj gobj,
    json_t *rows,           // not owned
    const char *treedb_name,
    const char *topic_name,
    const char *col_name,   // NULL at topic level
    json_t *projected,      // not owned
    json_t *stored,         // not owned
    const char **skip,      // attributes that say how a node is STORED
    json_t *desc            // not owned, descriptor of the node's topic, may be NULL
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_open_treedb(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_close_treedb(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_treedb(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_create_topic(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_topic(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_diff_schema(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_treedbs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_save_schema(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_saved_schema(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_apply_schema(hgobj gobj, const char *cmd, json_t *kw, hgobj src);


PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,  "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_open_treedb[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "filename_mask",0,              "%Y-%m-%d", "Organization of tables (file name format, see strftime())"),
SDATAPM (DTP_INTEGER,   "exit_on_error",0,              0,          "exit on error"),
SDATAPM (DTP_STRING,    "treedb_name",  0,              0,          "Treedb name"),
SDATAPM (DTP_JSON,      "treedb_schema",0,              0,          "Initial treedb schema, projected into __system__ and reconciled there"),
SDATAPM (DTP_BOOLEAN,   "impose_c_schema",0,            0,          "Set by the yuno's CODE: impose treedb_schema whatever the impose_c_schema attribute says, persistent value included"),
SDATAPM (DTP_JSON,      "initial_load", 0,              0,          "Seed records, created if missing and marked immutable"),
SDATAPM (DTP_STRING,    "import_root",  0,              0,          "C_NODE attr: root that 'import-assets' is confined to. Empty: refused"),
SDATAPM (DTP_INTEGER,   "files_max_size",0,             0,          "C_NODE attr: largest file a 'file' column accepts. 0: the default"),
SDATAPM (DTP_JSON,      "files_content_types",0,        0,          "C_NODE attr: mime types a 'file' column may hold. Empty: the default"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_close_treedb[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "treedb_name",  0,              0,          "Treedb name"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Close although the yuno plays (only if nothing cached the treedb's handles)"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_delete_treedb[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "treedb_name",  0,              0,          "Treedb name"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Force delete treedb"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_create_topic[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "treedb_name",  0,              0,          "Treedb name"),
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),

SDATAPM (DTP_INTEGER,   "topic_version",0,              "1",        "topic version"),
SDATAPM (DTP_STRING,    "topic_tkey",   0,              "",         "Time key"),
SDATAPM (DTP_JSON,      "pkey2s",       0,              0,          "Secondary keys: string or dict of string or [strings]"),
SDATAPM (DTP_JSON,      "cols",         0,              0,          "Columns"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_delete_topic[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "treedb_name",  0,              0,          "Treedb name"),
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_diff_schema[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "treedb_name",  0,              0,          "Treedb name (empty: every treedb opened with a schema from C)"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_save_schema[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "treedb_name",  0,              0,          "Treedb name (empty: every treedb opened here)"),
SDATAPM (DTP_BOOLEAN,   "dry_run",      0,              0,          "Answer the schema a save would write, and write nothing"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_saved_schema[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "treedb_name",  0,              0,          "Treedb name (empty: every treedb opened here)"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_treedbs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,        cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "authzs",           0,          pm_authzs,      cmd_authzs,     "Authorization's help"),

/*-CMD2---type----------name------------flag------------ali-items-----------json_fn-------------description--*/
SDATACM2 (DTP_SCHEMA,   "open-treedb",  SDF_AUTHZ_X,    0, pm_open_treedb,  cmd_open_treedb, "Open treedb (auto create if not exist)"),
SDATACM2 (DTP_SCHEMA,   "close-treedb", SDF_AUTHZ_X,    0, pm_close_treedb, cmd_close_treedb, "Close treedb"),
SDATACM2 (DTP_SCHEMA,   "delete-treedb",SDF_AUTHZ_X,    0, pm_delete_treedb,cmd_delete_treedb, "Delete treedb"),
SDATACM2 (DTP_SCHEMA,   "create-topic", SDF_AUTHZ_X,    0, pm_create_topic, cmd_create_topic, "Create new topic"),
SDATACM2 (DTP_SCHEMA,   "delete-topic", SDF_AUTHZ_X,    0, pm_delete_topic, cmd_delete_topic, "Delete topic"),
SDATACM2 (DTP_SCHEMA,   "diff-schema",  SDF_AUTHZ_X,    0, pm_diff_schema,  cmd_diff_schema, "Differences between the stored schema and the schema compiled in C"),
SDATACM2 (DTP_SCHEMA,   "treedbs",      SDF_AUTHZ_X,    0, pm_treedbs,      cmd_treedbs, "The treedbs opened here: whether each one opens with its schema from C, who decided it, and its schema versions"),
SDATACM2 (DTP_SCHEMA,   "save-schema",  SDF_AUTHZ_X,    0, pm_save_schema,  cmd_save_schema, "Publish the draft of a schema edited in __system__: raise the versions of what differs from the schema file in use, and write it to saved_schemas/, never over the file in use"),
SDATACM2 (DTP_SCHEMA,   "saved-schema", SDF_AUTHZ_X,    0, pm_saved_schema, cmd_saved_schema, "The schema save-schema wrote, what it changes against the file in use, and whether it can be applied"),
SDATACM2 (DTP_SCHEMA,   "apply-schema", SDF_AUTHZ_X,    0, pm_saved_schema, cmd_apply_schema, "Put the saved schema in place of the file in use (master, impose_c_schema off). It is read at the next open of the treedb"),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (DTP_STRING,      "path",             SDF_RD|SDF_REQUIRED,"",             "Path of treedbs"),
SDATA (DTP_STRING,      "filename_mask",    SDF_RD|SDF_REQUIRED,"%Y-%m",        "System organization of tables (file name format, see strftime())"),
SDATA (DTP_BOOLEAN,     "master",           SDF_RD,             "0",            "the master is the only that can write"),
SDATA (DTP_INTEGER,     "xpermission",      SDF_RD,             "02770",        "Use in creation, default 02770"),
SDATA (DTP_INTEGER,     "rpermission",      SDF_RD,             "0660",         "Use in creation, default 0660"),
SDATA (DTP_INTEGER,     "exit_on_error",    0,                  "2",            "exit on error, 2=LOG_OPT_EXIT_ZERO"),
SDATA (DTP_BOOLEAN,     "with_link_events", SDF_RD,             0,              "Publish EV_TREEDB_NODE_LINKED/UNLINKED events"),
SDATA (DTP_BOOLEAN,     "impose_c_schema",  SDF_RD,     "1",            "Open every treedb with its schema from C, over a newer schema file on disk. 0: open from the schema FILE, which apply-schema replaces with what save-schema published from __system__; the literal is installed only when it is newer. Either way __system__ is not read at open, and the MASTER projects into it when it has no projection or a lower schema_version. NOT persistent and no command changes it: it is configuration, set in the yuno's main.c ('global': {'treedbs.impose_c_schema': false}) or its config file, read when a treedb opens. It is the DEFAULT of every treedb: dynamic_schema_treedbs names the ones that open from their file, and the yuno's code can force it per treedb (open-treedb impose_c_schema=1)"),
SDATA (DTP_LIST,        "dynamic_schema_treedbs",SDF_RD,"[]",           "Treedbs that open from their schema FILE whatever impose_c_schema says, so their schema can be changed dynamically (save-schema + apply-schema). Configuration, NOT persistent, no command changes it: the yuno's main.c ('global': {'treedbs.dynamic_schema_treedbs': ['treedb_x']}) or its config file. The yuno's code still wins (open-treedb impose_c_schema=1)"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,              "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
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
PRIVATE sdata_desc_t pm_authz_open[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATA_END()
};
PRIVATE sdata_desc_t pm_authz_create[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATAPM0 (DTP_STRING,       "treedb_name",      0,          "",             "Treedb name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authz_write[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATAPM0 (DTP_STRING,       "treedb_name",      0,          "",             "Treedb name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authz_read[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATAPM0 (DTP_STRING,       "treedb_name",      0,          "",             "Treedb name"),
SDATA_END()
};

PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name------------flag----alias---items---------------description--*/
SDATAAUTHZ (DTP_SCHEMA, "open-close",   0,      0,      pm_authz_open,      "Permission to open-close treedb's"),
SDATAAUTHZ (DTP_SCHEMA, "create-delete",0,      0,      pm_authz_create,    "Permission to create-delete topics"),
SDATAAUTHZ (DTP_SCHEMA, "write",        0,      0,      pm_authz_write,     "Permission to write"),
SDATAAUTHZ (DTP_SCHEMA, "read",         0,      0,      pm_authz_read,      "Permission to read"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_tranger_system;
    hgobj gobj_node_system;
    json_t *tranger_system_;
    json_t *jn_c_schemas;               // schema from C by treedb_name, see diff-schema
    json_t *jn_forced_treedbs;          // treedbs whose yuno's code imposes the schema from C
    json_t *jn_withdrawn_at_open;       // what the last open of each treedb withdrew, see reconcile
    json_t *jn_apply_record_at_open;    // {treedb: "remove"|"ran"} done once the open succeeds
    json_t *jn_not_opened;              // {treedb: true} whose services exist but treedb_open_db() refused it
    json_int_t system_schema_version;   // of treedb_system_schema, see reconcile
    int32_t exit_on_error;
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
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(exit_on_error,             gobj_read_integer_attr)

    /*
     *  The schema a treedb was opened with is dropped once projected, and
     *  then nothing can say what the projection stored on top of it. Keep it
     *  by treedb_name: that is the other half `diff-schema` compares.
     */
    priv->jn_c_schemas = json_object();
    priv->jn_forced_treedbs = json_object();
    priv->jn_withdrawn_at_open = json_object();
    priv->jn_apply_record_at_open = json_object();
    priv->jn_not_opened = json_object();

    /*-----------------------------------*
     *      Create System Timeranger
     *-----------------------------------*/
    const char *filename_mask = gobj_read_str_attr(gobj, "filename_mask");
    BOOL master = gobj_read_bool_attr(gobj, "master");
    int exit_on_error = (int)gobj_read_integer_attr(gobj, "exit_on_error");
    int xpermission = (int)gobj_read_integer_attr(gobj, "xpermission");
    int rpermission = (int)gobj_read_integer_attr(gobj, "rpermission");

    char path[PATH_MAX];
    build_path(
        path,
        sizeof(path),
        gobj_read_str_attr(gobj, "path"),
        "__system__",
        NULL
    );

    json_t *kw_tranger = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i}",
        "path", path,
        "filename_mask", filename_mask,
        "master", master,
        "on_critical_error", exit_on_error,
        "xpermission", xpermission,
        "rpermission", rpermission
    );
    priv->gobj_tranger_system = gobj_create_service(
        "tranger_system_schema",
        C_TRANGER,
        kw_tranger,
        gobj
    );

    priv->tranger_system_ = gobj_read_pointer_attr(priv->gobj_tranger_system, "tranger");
    if(!priv->tranger_system_) {
        gobj_log_critical(gobj, priv->exit_on_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger NULL",
            NULL
        );
    }

    /*-------------------------------*
     *      Create System Treedb
     *-------------------------------*/
    json_t *jn_treedb_system_schema = treedb_create_system_schema();
    if(!jn_treedb_system_schema) {
        exit(-1);   // Error already logged
    }

    /*
     *  How a schema is STORED is this schema's business, so its version is
     *  what says whether a projection made earlier is still worth keeping.
     */
    priv->system_schema_version = kw_get_int(
        gobj,
        jn_treedb_system_schema,
        "schema_version",
        0,
        KW_WILD_NUMBER
    );

    const char *treedb_name = kw_get_str(
        gobj,
        jn_treedb_system_schema,
        "id",
        "treedb_system_schema",
        KW_REQUIRED
    );
    BOOL with_link_events = gobj_read_bool_attr(gobj, "with_link_events");
    json_t *kw_resource = json_pack("{s:I, s:s, s:o, s:i, s:b}",
        "tranger", (json_int_t)(uintptr_t)priv->tranger_system_,
        "treedb_name", treedb_name,
        "treedb_schema", jn_treedb_system_schema,
        "exit_on_error", LOG_OPT_EXIT_ZERO,
        "with_link_events", with_link_events
    );

    priv->gobj_node_system = gobj_create_service(
        treedb_name,
        C_NODE,
        kw_resource,
        gobj
    );

    /*
     *  HACK pipe inheritance
     */
    gobj_set_bottom_gobj(priv->gobj_node_system, priv->gobj_tranger_system);
    gobj_set_bottom_gobj(gobj, priv->gobj_node_system);

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

    JSON_DECREF(priv->jn_c_schemas)
    JSON_DECREF(priv->jn_forced_treedbs)
    JSON_DECREF(priv->jn_withdrawn_at_open)
    JSON_DECREF(priv->jn_apply_record_at_open)
    JSON_DECREF(priv->jn_not_opened)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
     PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!gobj_is_running(priv->gobj_tranger_system)) {
        gobj_start(priv->gobj_tranger_system);
    }
    if(!gobj_is_running(priv->gobj_node_system)) {
        gobj_start(priv->gobj_node_system);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
     PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->gobj_node_system);
    gobj_stop(priv->gobj_tranger_system);
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_treedbs(
    hgobj gobj,
    json_t *kw,
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *
     *  A framework method answers what its contract says -- here the list of
     *  treedb names -- so a refusal is NULL, and says so in the log: it used
     *  to answer a msg_iev_build_response envelope, which a caller of
     *  gobj_treedbs() reads as a dict of one treedb named "result".
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "No permission to list the treedbs",
            "permission",   "%s", permission,
            "service",      "%s", gobj_name(gobj),
            NULL
        );
        KW_DECREF(kw)
        return NULL;
    }

    return treedb_list_treedb(
        priv->tranger_system_,
        kw
    );
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

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_open_treedb(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *filename_mask = kw_get_str(gobj, kw, "filename_mask", "", 0);
    int exit_on_error = (int)kw_get_int(gobj, kw, "exit_on_error", 0, KW_WILD_NUMBER);
    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    json_t *_jn_treedb_schema = kw_get_dict(gobj, kw, "treedb_schema", 0, 0);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "open-close";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("%s: No permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------------*
     *      Check parameters
     *-----------------------------------*/
    if(empty_string(treedb_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: what treedb_name?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(filename_mask)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: what filename_mask?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------------*
     *  A treedb already open here is refused
     *  BEFORE anything else: the open that
     *  follows reconciles __system__, and a
     *  second open must change nothing. 7.25.4
     *  reconciled it first (creates, updates)
     *  and then failed on the name of its
     *  tranger, taken by the open one ("Internal
     *  error, tranger client NULL").
     *-----------------------------------*/
    char tranger_name[NAME_MAX];
    snprintf(tranger_name, sizeof(tranger_name), "tranger_%s", treedb_name);
    hgobj gobj_already = gobj_find_service(treedb_name, FALSE);
    if(gobj_already || gobj_find_service(tranger_name, FALSE)) {
        json_t *comment;
        if(!is_treedb_opened_here(gobj, gobj_already)) {
            comment = json_sprintf("%s: cannot open treedb '%s': a service named '%s' exists "
                "already, nothing was changed", gobj_yuno_role_plus_name(), treedb_name,
                gobj_already? treedb_name : tranger_name);
        } else if(json_object_get(priv->jn_not_opened, treedb_name)) {
            comment = json_sprintf("%s: treedb '%s' did not open at its last open-treedb, "
                "its schema was refused (see the log): close-treedb it before opening it "
                "again, nothing was changed", gobj_yuno_role_plus_name(), treedb_name);
        } else {
            comment = json_sprintf("%s: treedb '%s' is already open here: close-treedb first, "
                "nothing was changed", gobj_yuno_role_plus_name(), treedb_name);
        }
        return msg_iev_build_response(
            gobj,
            -1,
            comment,
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------------*
     *      Create Client Timeranger
     *
     *  BEFORE the schema is decided: whether
     *  the literal is installed, and so what
     *  __system__ must say, depends on this
     *  tranger being the master of its store
     *  (another process may hold it, and then
     *  the treedb runs its file as a replica).
     *-----------------------------------*/
    BOOL master = gobj_read_bool_attr(gobj, "master");
    int xpermission = (int)gobj_read_integer_attr(gobj, "xpermission");
    int rpermission = (int)gobj_read_integer_attr(gobj, "rpermission");

    char path[PATH_MAX];
    build_path(
        path,
        sizeof(path),
        gobj_read_str_attr(gobj, "path"),
        treedb_name,
        NULL
    );

    json_t *kw_client_tranger = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i}",
        "path", path,
        "filename_mask", filename_mask,
        "master", master,
        "on_critical_error", exit_on_error,
        "xpermission", xpermission,
        "rpermission", rpermission
    );
    // TODO crea como servicio hasta que se pueda integrar el bottom como propio
    hgobj gobj_client_tranger = gobj_create_service(
        tranger_name,
        C_TRANGER,
        kw_client_tranger,
        gobj
    );

    json_t *tranger_client = gobj_client_tranger?
        gobj_read_pointer_attr(gobj_client_tranger, "tranger") : NULL;
    if(!tranger_client) {
        gobj_log_critical(gobj, exit_on_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger client NULL",
            NULL
        );
        if(gobj_client_tranger) {
            gobj_destroy(gobj_client_tranger);
        }
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot open treedb '%s': its tranger could not be created "
                "(see the log)", gobj_yuno_role_plus_name(), treedb_name),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------------*
     *      Get the schema to open with
     *-----------------------------------*/
    BOOL forced_by_code = kw_get_bool(gobj, kw, "impose_c_schema", 0, KW_WILD_NUMBER);
    BOOL configured = configured_to_impose(gobj, treedb_name);
    BOOL impose_c_schema = forced_by_code || configured;
    if(forced_by_code && !configured) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "impose_c_schema forced by the code of the yuno, over the attribute",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
    }
    json_t *jn_client_treedb_schema = impose_c_schema?
        get_c_schema_to_impose(gobj, treedb_name, _jn_treedb_schema, tranger_client):
        get_client_treedb_schema(gobj, treedb_name, _jn_treedb_schema, tranger_client);
    if(!jn_client_treedb_schema) {
        json_object_del(priv->jn_apply_record_at_open, treedb_name);
        gobj_destroy(gobj_client_tranger);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot open treedb '%s': no valid treedb_schema (see the log)",
                gobj_yuno_role_plus_name(), treedb_name),
            0,
            0,
            kw  // owned
        );
    }

    /*-------------------------------*
     *      Create Client Treedb
     *-------------------------------*/
    json_t *kw_resource = json_pack("{s:I, s:s, s:o, s:i, s:b, s:b}",
        "tranger", (json_int_t)(uintptr_t)tranger_client,
        "treedb_name", treedb_name,
        "treedb_schema", jn_client_treedb_schema,
        "exit_on_error", exit_on_error,
        "with_link_events", gobj_read_bool_attr(gobj, "with_link_events"),
        "impose_c_schema", impose_c_schema
    );

    /*
     *  Asked without a default: kw_get_dict() decrefs the default on the path
     *  where it finds the key, so a fallback here would be spent every call.
     */
    json_t *jn_initial_load = kw_get_dict(gobj, kw, "initial_load", 0, 0);
    if(jn_initial_load) {
        json_object_set(kw_resource, "initial_load", jn_initial_load);
    }

    /*
     *  The `file` columns' three, forwarded by NAME and not by sweeping the
     *  kw: they are SDF_RD on C_NODE, so they can only be set at creation,
     *  and this command is how every real yuno opens a treedb -- declared
     *  on C_NODE and unreachable through here, they were attributes nobody
     *  could set. A whitelist and not a passthrough, because the kw of a
     *  command carries the caller's own keys too (`__username__`, the
     *  routing metadata), and none of them is an attribute of a treedb.
     */
    static const char *file_attrs[] = {
        "import_root", "files_max_size", "files_content_types", 0
    };
    for(int i = 0; file_attrs[i]; i++) {
        json_t *jn_value = kw_get_dict_value(gobj, kw, file_attrs[i], 0, 0);
        if(jn_value) {
            json_object_set(kw_resource, file_attrs[i], jn_value);
        }
    }

    hgobj gobj_client_node = gobj_create_service(
        treedb_name,
        C_NODE,
        kw_resource,
        gobj
    );

    int started = -1;
    if(gobj_client_node) {
        /*
         *  HACK pipe inheritance
         */
        gobj_set_bottom_gobj(gobj_client_node, gobj_client_tranger);

        gobj_start(gobj_client_tranger);
        started = gobj_start(gobj_client_node);
    } else {
        gobj_destroy(gobj_client_tranger);  // Error already logged
    }

    /*
     *  The record of an apply is settled only by an open that OPENED:
     *  removed when the literal replaced the apply, marked "in_use" when
     *  the apply runs now. An open that failed leaves it as it is.
     */
    if(started == 0) {
        settle_apply_record(gobj, treedb_name);
    } else {
        json_object_del(priv->jn_apply_record_at_open, treedb_name);
    }

    if(forced_by_code && gobj_client_node) {
        json_object_set_new(priv->jn_forced_treedbs, treedb_name, json_true());
    } else {
        json_object_del(priv->jn_forced_treedbs, treedb_name);
    }
    if(gobj_client_node && started < 0) {
        json_object_set_new(priv->jn_not_opened, treedb_name, json_true());
    } else {
        json_object_del(priv->jn_not_opened, treedb_name);
    }

    /*
     *  A treedb whose start failed (treedb_open_db() refused it) is not
     *  open, whatever services were created (7.25.4 answered "Treedb
     *  opened!"). Its services stay, as they are after any open, for
     *  close-treedb to take away; until then a second open-treedb says it
     *  did not open, and `treedbs` answers it `opened` false.
     */
    json_t *comment;
    if(!gobj_client_node) {
        comment = json_sprintf("%s: cannot open treedb '%s' (see the log)",
            gobj_yuno_role_plus_name(), treedb_name);
    } else if(started < 0) {
        comment = json_sprintf("%s: treedb '%s' did not open, its schema was refused "
            "(see the log): close-treedb it before opening it again",
            gobj_yuno_role_plus_name(), treedb_name);
    } else {
        comment = json_sprintf("%s: treedb opened: '%s'",
            gobj_yuno_role_plus_name(), treedb_name);
    }
    return msg_iev_build_response(gobj,
        (gobj_client_node && started == 0)? 0 : -1,
        comment,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_close_treedb(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "open-close";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("%s: No permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------------*
     *      Check parameters
     *-----------------------------------*/
    if(empty_string(treedb_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: what treedb_name?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------------------------------*
     *  Closing destroys the treedb's C_NODE and its
     *  C_TRANGER, and whoever opened them keeps raw
     *  handles that no framework cleanup can reach: the
     *  service pointer, the `tranger` json_t read from it,
     *  copies of both on a hot path, and whatever else was
     *  opened on that same tranger (a msg2db, another
     *  treedb). Freed underneath, the next record processed
     *  writes into released memory.
     *
     *  The owner closes and reopens through its own
     *  lifecycle — mt_pause closes, mt_play reopens and
     *  re-acquires every handle — and by then the yuno is
     *  already out of play. From outside, that is
     *  `pause-yuno` + `play-yuno`.
     *
     *  `force` is for a caller that opened the treedb and
     *  holds nothing of it.
     *-----------------------------------------------------*/
    if(gobj_is_playing(gobj_yuno()) && !force) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: cannot close a treedb while the yuno plays. Use "
                "pause-yuno + play-yuno, which reopens it through the yuno's "
                "own lifecycle (force=1 only if nothing cached its handles)",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------*
     *  Close gclass Node
     *----------------------*/
    hgobj gobj_client_node = gobj_find_service(treedb_name, FALSE);
    if(!gobj_client_node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: treedb '%s' not found", gobj_yuno_role_plus_name(), treedb_name),
            0,
            0,
            kw  // owned
        );
    }
    if(!is_treedb_opened_here(gobj, gobj_client_node)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: '%s' is not a treedb opened by this service",
                gobj_yuno_role_plus_name(), treedb_name
            ),
            0,
            0,
            kw  // owned
        );
    }

    hgobj gobj_client_tranger = gobj_bottom_gobj(gobj_client_node);

    gobj_stop(gobj_client_node);
    gobj_stop(gobj_client_tranger);

    gobj_destroy(gobj_client_tranger);
    gobj_destroy(gobj_client_node);

    json_object_del(priv->jn_c_schemas, treedb_name);
    json_object_del(priv->jn_forced_treedbs, treedb_name);
    json_object_del(priv->jn_not_opened, treedb_name);

    return msg_iev_build_response(gobj,
        0,
        json_sprintf("%s: treedb closed: '%s'", gobj_yuno_role_plus_name(), treedb_name),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_treedb(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    BOOL force = kw_get_bool(gobj, kw, "force", 0, KW_WILD_NUMBER);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "open-close";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("%s: No permission to '%s' in service '%s'",
                gobj_yuno_role_plus_name(), permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------------*
     *      Check parameters
     *-----------------------------------*/
    if(empty_string(treedb_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: what treedb_name?", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(!force) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: delete-treedb must be used with force=1", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  The schema of the schemas is not a treedb's schema to delete
     */
    if(strcmp(treedb_name, gobj_name(priv->gobj_node_system))==0) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: '%s' is the system schema, it cannot be deleted",
                gobj_yuno_role_plus_name(), treedb_name
            ),
            0,
            0,
            kw  // owned
        );
    }
    /*-----------------------------------------------------*
     *  Not while it is OPEN, and `force` does not lift this.
     *
     *  This command deletes the SCHEMA of a treedb -- its
     *  projection in __system__ -- and an open treedb goes on
     *  answering from the copy it holds in memory. What the
     *  delete leaves behind is a running treedb whose schema
     *  no longer exists anywhere, and the damage shows at the
     *  next `open-treedb`: the C_TRANGER service of the old
     *  one is still alive under its name, the create collides
     *  with it, and the open dies with an internal "tranger
     *  client NULL" that names nothing an operator can act on.
     *  The store on disk is then orphaned -- data with no
     *  schema to read it by.
     *
     *  `force` already means something else here ("yes, delete
     *  the schema"), and it is the sibling command that says
     *  how to close: `close-treedb`, or the yuno's own
     *  lifecycle with pause-yuno + play-yuno.
     *-----------------------------------------------------*/
    hgobj gobj_opened = gobj_find_service(treedb_name, FALSE);
    if(gobj_opened && is_treedb_opened_here(gobj, gobj_opened) &&
            json_object_get(priv->jn_not_opened, treedb_name)) {
        /*
         *  Its last open-treedb did not open it, but its services are
         *  there: the next open would collide with them
         */
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: cannot delete the schema of '%s': its last open-treedb did not "
                "open it, and its services are still there. close-treedb it first, "
                "then delete-treedb",
                gobj_yuno_role_plus_name(), treedb_name
            ),
            0,
            0,
            kw  // owned
        );
    }
    if(gobj_opened && is_treedb_opened_here(gobj, gobj_opened)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: cannot delete the schema of '%s' while it is OPEN. "
                "Close it first (close-treedb, or pause-yuno + play-yuno): "
                "deleting it now would leave a treedb running with no schema "
                "and a store nothing can read",
                gobj_yuno_role_plus_name(), treedb_name
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  What __system__'s tranger IS, not the attribute: a master that lost
     *  its lock goes on as a replica, and this deletes nodes of __system__.
     */
    if(!system_is_written_here(gobj)) {
        return build_readonly_response(gobj, gobj_name(priv->gobj_node_system), priv->tranger_system_, kw);
    }

    int ret = delete_client_treedb_schema(gobj, treedb_name);
    json_object_del(priv->jn_c_schemas, treedb_name);
    json_object_del(priv->jn_forced_treedbs, treedb_name);

    /*
     *  Its saved schema goes with it, the record of an apply not opened
     *  yet, and the record of an unfinished projection: left in
     *  saved_schemas/, a treedb created again under the name found them.
     */
    if(ret == 0) {
        json_int_t removed_version;
        ret = remove_saved_schema(gobj, treedb_name, &removed_version);  // Error already logged
        remove_apply_record(gobj, treedb_name);     // Error already logged
        remove_unfinished_record(gobj, treedb_name);    // Error already logged
    }

    if(ret < 0) {
        return msg_iev_build_response(gobj,
            ret,
            json_sprintf(
                "%s: cannot delete the schema of '%s': not projected in "
                "__system__, one of its nodes refused the delete, or its saved "
                "schema could not be removed (see the log)",
                gobj_yuno_role_plus_name(), treedb_name
            ),
            0,
            0,
            kw  // owned
        );
    }
    return msg_iev_build_response(gobj,
        0,
        json_sprintf("%s: schema of '%s' deleted", gobj_yuno_role_plus_name(), treedb_name),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_create_topic(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    int topic_version = kw_get_int(gobj, kw, "topic_version", 1, KW_WILD_NUMBER);
    const char *topic_tkey = kw_get_str(gobj, kw, "topic_tkey", "", 0);
    json_t *pkey2s_ = kw_get_dict_value(gobj, kw, "pkey2s", 0, 0);
    json_t *cols_ = kw_get_dict_value(gobj, kw, "cols", 0, 0);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "create-delete";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    hgobj gobj_client_node = gobj_find_service(treedb_name, FALSE);
    if(!gobj_client_node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Treedb_name not found: '%s'", treedb_name),
            0,
            0,
            kw  // owned
        );
    }
    if(!is_treedb_opened_here(gobj, gobj_client_node)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: '%s' is not a treedb opened by this service",
                gobj_yuno_role_plus_name(), treedb_name
            ),
            0,
            0,
            kw  // owned
        );
    }

    json_t *tranger = gobj_read_pointer_attr(gobj_client_node, "tranger");
    if(!tranger_writes_now(gobj, tranger)) {
        return build_readonly_response(gobj, treedb_name, tranger, kw);
    }

    json_t *topic = treedb_create_topic( // WARNING Return is NOT YOURS
        tranger,
        treedb_name,
        topic_name,
        topic_version,
        topic_tkey,
        json_incref(pkey2s_), // owned, string or dict of string | [strings]
        json_incref(cols_), // owned
        0,          // snap_tag
        FALSE,      // system_topic: client-created topics are never system
        FALSE       // create_schema
    );

    return msg_iev_build_response(gobj,
        topic?0:-1,
        topic?json_sprintf("Topic created!"):json_sprintf("Cannot create new topic"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_topic(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "create-delete";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    hgobj gobj_client_node = gobj_find_service(treedb_name, FALSE);
    if(!gobj_client_node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Treedb_name not found: '%s'", treedb_name),
            0,
            0,
            kw  // owned
        );
    }
    if(!is_treedb_opened_here(gobj, gobj_client_node)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: '%s' is not a treedb opened by this service",
                gobj_yuno_role_plus_name(), treedb_name
            ),
            0,
            0,
            kw  // owned
        );
    }

    json_t *tranger = gobj_read_pointer_attr(gobj_client_node, "tranger");
    if(!tranger_writes_now(gobj, tranger)) {
        return build_readonly_response(gobj, treedb_name, tranger, kw);
    }

    int ret = treedb_delete_topic(
        tranger,
        treedb_name,
        topic_name
    );

    return msg_iev_build_response(gobj,
        ret,
        ret<0?json_sprintf("Cannot delete topic"):json_sprintf("Topic deleted!"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *  What the CONFIGURATION says for one treedb: named in
 *  dynamic_schema_treedbs, it opens from its file; otherwise the attribute
 *  impose_c_schema, the default of every treedb, decides.
 ***************************************************************************/
PRIVATE BOOL configured_to_impose(hgobj gobj, const char *treedb_name)
{
    json_t *dynamic = gobj_read_json_attr(gobj, "dynamic_schema_treedbs");
    if(json_list_str_index(dynamic, treedb_name, FALSE) >= 0) {
        return FALSE;
    }
    return gobj_read_bool_attr(gobj, "impose_c_schema");
}

/***************************************************************************
 *  Does C impose the schema of this treedb? The configuration, or the yuno's
 *  code for this one treedb (open-treedb impose_c_schema=1).
 ***************************************************************************/
PRIVATE BOOL treedb_schema_imposed(hgobj gobj, const char *treedb_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return configured_to_impose(gobj, treedb_name) ||
        json_object_get(priv->jn_forced_treedbs, treedb_name);
}

/***************************************************************************
 *  Who decided whether a treedb imposes: the yuno's code, its place in
 *  dynamic_schema_treedbs, or the default (impose_c_schema).
 ***************************************************************************/
PRIVATE const char *impose_decided_by(hgobj gobj, const char *treedb_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(json_object_get(priv->jn_forced_treedbs, treedb_name)) {
        return "code";
    }
    json_t *dynamic = gobj_read_json_attr(gobj, "dynamic_schema_treedbs");
    if(json_list_str_index(dynamic, treedb_name, FALSE) >= 0) {
        return "dynamic_schema_treedbs";
    }
    return "impose_c_schema";
}

/***************************************************************************
 *  The schema_version of a schema file, 0 when there is none. The file is
 *  often absent -- no saved schema yet, the normal case -- and asking the
 *  kw_* readers about a NULL json logs "kw must be list or dict" on every
 *  saved-schema call of the console.
 ***************************************************************************/
PRIVATE json_int_t schema_version_of(hgobj gobj, json_t *jn_schema)
{
    if(!json_is_object(jn_schema)) {
        return 0;
    }
    return kw_get_int(gobj, jn_schema, "schema_version", 0, KW_WILD_NUMBER);
}

/***************************************************************************
 *  The directory of the schema file IN USE by a treedb opened here: the
 *  directory of its tranger, where treedb_open_db() writes it.
 ***************************************************************************/
PRIVATE int in_use_schema_dir(hgobj gobj, const char *treedb_name, char *bf, size_t bfsize)
{
    char tranger_name[NAME_MAX];
    snprintf(tranger_name, sizeof(tranger_name), "tranger_%s", treedb_name);
    hgobj gobj_tranger = gobj_find_service(tranger_name, FALSE);
    if(gobj_tranger &&
            (!gobj_typeof_gclass(gobj_tranger, C_TRANGER) || gobj_parent(gobj_tranger) != gobj)) {
        gobj_tranger = NULL;    /*  a service of that name, but not the tranger we opened  */
    }
    json_t *tranger = gobj_tranger? gobj_read_pointer_attr(gobj_tranger, "tranger"): NULL;
    if(!tranger) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB,
            "msg",          "%s", "Treedb not open here",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        return -1;
    }
    snprintf(bf, bfsize, "%s", kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED));
    return 0;
}

/***************************************************************************
 *  Where save-schema writes: `saved_schemas/` under the __system__
 *  tranger, never beside the file in use.
 ***************************************************************************/
PRIVATE void saved_schema_dir(hgobj gobj, char *bf, size_t bfsize)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    build_path(bf, bfsize,
        kw_get_str(gobj, priv->tranger_system_, "directory", "", KW_REQUIRED),
        "saved_schemas",
        NULL
    );
}

/***************************************************************************
 *  A schema rebuilt from __system__ carries every attribute of every node,
 *  the empty ones too ("", {}, []), plus what only the projection keeps:
 *  `_geometry` (where a GUI drew the node) and the bookkeeping versions.
 *  A literal says none of that, and a schema file should read like one:
 *  what is saved, compared and exported is the schema, nothing else.
 ***************************************************************************/
PRIVATE void prune_schema_node(json_t *jn) // not owned, MUTATED
{
    if(json_is_object(jn)) {
        const char *key; json_t *value; void *tmp;
        json_object_foreach_safe(jn, tmp, key, value) {
            /*  A column's `default` is what the author wrote, `[]` and null
             *  included: an empty container IS a default (a list column
             *  declares `[]`), and pruned it read as "no default".
             *  Except `{}`: it is what the meta-schema stores for a column
             *  that declares NO default (the attribute is a blob, see
             *  get_treedb_schema), and it means nothing in any type -- a
             *  scalar column never keeps it, a list/array one takes `[]` for
             *  a value that is not a list, and dict/object/template/blob fill
             *  `{}` when there is no default at all (set_field_value() in
             *  tr_treedb.c). Kept, every list column of a saved schema read
             *  as "default added" against the file in use.
             *  On a `required` column too. The meta-schema cannot tell that
             *  `{}` from a `default: {}` the author wrote, and a default
             *  fills the field, so kept it would turn `required` off for
             *  every required dict/list/array/blob column declared with NO
             *  default. The trade-off, the lesser one (as in 7.25.4): a
             *  required column that really declared `default: {}` loses it
             *  through save + apply, and a record created without the field
             *  is refused.  */
            if(strcmp(key, "default")==0) {
                if(json_is_object(value) && json_object_size(value)==0) {
                    json_object_del(jn, key);
                }
                continue;
            }
            if(strcmp(key, "_geometry")==0 ||
                    json_is_null(value) ||
                    (json_is_string(value) && empty_string(json_string_value(value))) ||
                    (json_is_object(value) && json_object_size(value)==0) ||
                    (json_is_array(value) && json_array_size(value)==0)) {
                json_object_del(jn, key);
                continue;
            }
            prune_schema_node(value);
        }
    } else if(json_is_array(jn)) {
        size_t idx; json_t *value;
        json_array_foreach(jn, idx, value) {
            prune_schema_node(value);
        }
    }
}

/***************************************************************************
 *  A column's `fkey` DICT is a mark parse_schema() derives from the hooks
 *  that point at it, never written by an author: treedb_open_db() strips
 *  it from the file in use. Left in, a literal that went through
 *  parse_schema() read as another schema than its own file, and a file
 *  written from a parsed schema carried it.
 ***************************************************************************/
PRIVATE void strip_derived_fkey_marks(json_t *jn_schema) // not owned, MUTATED
{
    int idx; json_t *topic;
    json_t *topics = json_object_get(jn_schema, "topics");
    if(json_is_object(topics)) {
        const char *topic_name;
        json_object_foreach(topics, topic_name, topic) {
            json_t *cols = json_object_get(topic, "cols");
            const char *col_name; json_t *col;
            json_object_foreach(cols, col_name, col) {
                if(json_is_object(json_object_get(col, "fkey"))) {
                    json_object_del(col, "fkey");
                }
            }
        }
        return;
    }
    json_array_foreach(topics, idx, topic) {
        json_t *cols = json_object_get(topic, "cols");
        if(json_is_object(cols)) {
            const char *col_name; json_t *col;
            json_object_foreach(cols, col_name, col) {
                if(json_is_object(json_object_get(col, "fkey"))) {
                    json_object_del(col, "fkey");
                }
            }
        } else if(json_is_array(cols)) {
            int idx2; json_t *col;
            json_array_foreach(cols, idx2, col) {
                if(json_is_object(json_object_get(col, "fkey"))) {
                    json_object_del(col, "fkey");
                }
            }
        }
    }
}

PRIVATE void prune_schema(json_t *jn_schema) // not owned, MUTATED
{
    json_object_del(jn_schema, "c_schema_version");
    json_object_del(jn_schema, "system_schema_version");
    prune_schema_node(jn_schema);

    /*
     *  A literal lists its topics; get_treedb_schema() keys them by name.
     *  Listed in the order they come, which is the declared one.
     */
    json_t *topics = json_object_get(jn_schema, "topics");
    if(json_is_object(topics)) {
        json_t *list = json_array();
        const char *name; json_t *topic;
        json_object_foreach(topics, name, topic) {
            json_array_append(list, topic);
        }
        json_object_set_new(jn_schema, "topics", list);
    }

    strip_derived_fkey_marks(jn_schema);
}

/***************************************************************************
 *  A schema file as a table, to say what changed: `topics` is a list, so
 *  it is keyed by name first -- by position, a reordered topic would read
 *  as every column of it changed. Pruned, and a `false` is the absence it
 *  stands for: a literal leaves `system_topic` out, a projection says it.
 ***************************************************************************/
PRIVATE json_t *names_in_order(json_t *nodes) // not owned, a list of {id} or a dict
{
    json_t *joined = json_string("");
    int n = 0;
    if(json_is_array(nodes)) {
        int idx; json_t *node;
        json_array_foreach(nodes, idx, node) {
            const char *name = json_string_value(json_object_get(node, "id"));
            if(name) {
                json_t *next = json_sprintf("%s%s%s", json_string_value(joined), n? ", " : "", name);
                JSON_DECREF(joined)
                joined = next;
                n++;
            }
        }
    } else if(json_is_object(nodes)) {
        const char *name; json_t *node;
        json_object_foreach(nodes, name, node) {
            json_t *next = json_sprintf("%s%s%s", json_string_value(joined), n? ", " : "", name);
            JSON_DECREF(joined)
            joined = next;
            n++;
        }
    }
    return joined;
}

PRIVATE json_t *schema_to_flat(json_t *jn_schema) // not owned
{
    json_t *copy = json_deep_copy(jn_schema);
    prune_schema(copy);

    /*
     *  Keyed by name, a MOVED topic or column is no difference at all, and
     *  the order is part of a schema: it is what a table paints (in 7.25.4
     *  a save that only moved a column showed its versions raised and
     *  nothing else). So the order is a leaf of its own, the names joined
     *  as they come:
     *
     *      __topics_order__                "users, departments"
     *      topics`users`__cols_order__     "id, username, email, departments"
     *
     *  Two underscores each side, as nothing a schema declares is named.
     */
    json_object_set_new(copy, "__topics_order__", names_in_order(json_object_get(copy, "topics")));

    json_t *topics = json_object_get(copy, "topics");
    if(json_is_array(topics)) {
        json_t *by_name = json_object();
        int idx; json_t *topic;
        json_array_foreach(topics, idx, topic) {
            const char *name = json_string_value(json_object_get(topic, "id"));
            if(name) {
                json_object_set(by_name, name, topic);
            }
        }
        json_object_set_new(copy, "topics", by_name);
    }

    /*
     *  The cols too: a literal may list them, a file written from __system__
     *  keys them by name, and each col of the dict form carries its name as
     *  `id` or not. The same schema in two forms read as "another content"
     *  at every open in 7.25.4.
     */
    const char *topic_name; json_t *topic;
    json_object_foreach(json_object_get(copy, "topics"), topic_name, topic) {
        json_t *cols = json_object_get(topic, "cols");
        json_object_set_new(topic, "__cols_order__", names_in_order(cols));
        json_t *by_name = json_object();
        if(json_is_array(cols)) {
            int idx; json_t *col;
            json_array_foreach(cols, idx, col) {
                const char *name = json_string_value(json_object_get(col, "id"));
                if(name) {
                    json_object_set(by_name, name, col);
                }
            }
        } else if(json_is_object(cols)) {
            const char *col_name; json_t *col;
            json_object_foreach(cols, col_name, col) {
                json_object_set(by_name, col_name, col);
            }
        } else {
            JSON_DECREF(by_name)
            continue;
        }
        const char *col_name; json_t *col;
        json_object_foreach(by_name, col_name, col) {
            if(json_is_object(col) && !json_object_get(col, "id")) {
                json_object_set_new(col, "id", json_string(col_name));
            }
        }
        json_object_set_new(topic, "cols", by_name);
    }
    json_t *flat = json2flat(copy);
    JSON_DECREF(copy)

    const char *id; json_t *value; void *tmp;
    json_object_foreach_safe(flat, tmp, id, value) {
        if(json_is_false(value)) {
            json_object_del(flat, id);
        }
    }
    return flat;
}

/***************************************************************************
 *  The names of a list of {id} or of a dict, in their order. Return YOURS.
 ***************************************************************************/
PRIVATE json_t *names_list(json_t *nodes) // not owned
{
    json_t *names = json_array();
    if(json_is_array(nodes)) {
        int idx; json_t *node;
        json_array_foreach(nodes, idx, node) {
            json_t *name = json_object_get(node, "id");
            if(!json_is_string(name)) {
                name = json_object_get(node, "topic_name");
            }
            if(json_is_string(name)) {
                json_array_append(names, name);
            }
        }
    } else if(json_is_object(nodes)) {
        const char *name; json_t *node;
        json_object_foreach(nodes, name, node) {
            json_array_append_new(names, json_string(name));
        }
    }
    return names;
}

/***************************************************************************
 *  Do the names that `a` and `b` share come in the same order in both?
 ***************************************************************************/
PRIVATE BOOL same_order_of_common_names(json_t *a, json_t *b) // not owned, lists of names
{
    json_t *common_a = json_array();
    json_t *common_b = json_array();
    int idx; json_t *name;
    json_array_foreach(a, idx, name) {
        if(json_list_str_index(b, json_string_value(name), FALSE) >= 0) {
            json_array_append(common_a, name);
        }
    }
    json_array_foreach(b, idx, name) {
        if(json_list_str_index(a, json_string_value(name), FALSE) >= 0) {
            json_array_append(common_b, name);
        }
    }
    BOOL same = json_equal(common_a, common_b)? TRUE : FALSE;
    JSON_DECREF(common_a)
    JSON_DECREF(common_b)
    return same;
}

/***************************************************************************
 *  Does a schema declare any topic? A treedb with none does not open
 *  (treedb_open_db() refuses it: "No topics found"), so a schema without
 *  topics is neither saved nor applied.
 ***************************************************************************/
PRIVATE BOOL schema_has_topics(json_t *jn_schema) // not owned
{
    json_t *topics = json_object_get(jn_schema, "topics");
    if(json_is_array(topics)) {
        return json_array_size(topics) > 0;
    }
    if(json_is_object(topics)) {
        return json_object_size(topics) > 0;
    }
    return FALSE;
}

/***************************************************************************
 *  The topic `topic_name` of a schema whose topics are a list or a dict.
 ***************************************************************************/
PRIVATE json_t *schema_topic(json_t *jn_schema, const char *topic_name) // not owned, return not owned
{
    json_t *topics = json_object_get(jn_schema, "topics");
    if(json_is_object(topics)) {
        return json_object_get(topics, topic_name);
    }
    int idx; json_t *topic;
    json_array_foreach(topics, idx, topic) {
        json_t *name = json_object_get(topic, "id");
        if(!json_is_string(name)) {
            name = json_object_get(topic, "topic_name");
        }
        if(json_is_string(name) && strcmp(json_string_value(name), topic_name)==0) {
            return topic;
        }
    }
    return NULL;
}

/***************************************************************************
 *  What changes from one schema to another, as a table: flat_diff() of the
 *  two schema_to_flat(), with ONE rule about the order leaves
 *  (`__topics_order__`, `topics`<topic>`__cols_order__`): an order leaf is a
 *  difference only when the names BOTH sides declare come in another order.
 *  Compared as plain strings, a column added or removed would read as a
 *  moved one too. When it is a difference, the row carries the whole of
 *  both orders, added and removed names included. Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *schema_diff(json_t *from_schema, json_t *to_schema) // not owned
{
    json_t *flat_from = schema_to_flat(from_schema);
    json_t *flat_to = schema_to_flat(to_schema);

    json_t *topics_from = names_list(json_object_get(from_schema, "topics"));
    json_t *topics_to = names_list(json_object_get(to_schema, "topics"));
    if(same_order_of_common_names(topics_from, topics_to)) {
        json_object_del(flat_from, "__topics_order__");
        json_object_del(flat_to, "__topics_order__");
    }

    int idx; json_t *jn_name;
    json_array_foreach(topics_from, idx, jn_name) {
        const char *topic_name = json_string_value(jn_name);
        json_t *topic_to = schema_topic(to_schema, topic_name);
        if(!topic_to) {
            continue;
        }
        json_t *cols_from = names_list(json_object_get(schema_topic(from_schema, topic_name), "cols"));
        json_t *cols_to = names_list(json_object_get(topic_to, "cols"));
        if(same_order_of_common_names(cols_from, cols_to)) {
            /*
             *  The id of the leaf, escaped as json2flat() escapes it: asked
             *  of json2flat() itself
             */
            json_t *probe = json_pack("{s:{s:{s:s}}}", "topics", topic_name, "__cols_order__", "");
            json_t *flat_probe = json2flat(probe);
            const char *leaf_id; json_t *v;
            json_object_foreach(flat_probe, leaf_id, v) {
                json_object_del(flat_from, leaf_id);
                json_object_del(flat_to, leaf_id);
            }
            JSON_DECREF(flat_probe)
            JSON_DECREF(probe)
        }
        JSON_DECREF(cols_from)
        JSON_DECREF(cols_to)
    }
    JSON_DECREF(topics_from)
    JSON_DECREF(topics_to)

    json_t *diff = flat_diff(flat_from, flat_to);
    JSON_DECREF(flat_from)
    JSON_DECREF(flat_to)
    return diff;
}

/***************************************************************************
 *  The topic_version a schema file gives a topic, 0 when it has none.
 ***************************************************************************/
PRIVATE json_int_t schema_topic_version(hgobj gobj, json_t *jn_schema, const char *topic_name)
{
    json_t *topics = json_object_get(jn_schema, "topics");
    if(json_is_object(topics)) {
        json_t *topic = json_object_get(topics, topic_name);
        if(!topic) {
            return 0;   // a topic the file does not hold, as the list below answers
        }
        return kw_get_int(gobj, topic, "topic_version", 0, KW_WILD_NUMBER);
    }
    int idx; json_t *topic;
    json_array_foreach(topics, idx, topic) {
        if(strcmp(kw_get_str(gobj, topic, "id", "", 0), topic_name)==0) {
            return kw_get_int(gobj, topic, "topic_version", 0, KW_WILD_NUMBER);
        }
    }
    return 0;
}

/***************************************************************************
 *  What the last open of `treedb_name` withdrew (see reconcile), as the API
 *  answers it: {} when nothing. Return is YOURS.
 *
 *      {"schema_version": 22,          // the literal that did it
 *       "saved_schema_version": 21,    // the saved schema it withdrew, 0: none
 *       "topics": {"users": "saved"}}  // "applied" | "in_use" | "saved" | "unsaved"
 ***************************************************************************/
PRIVATE json_t *withdrawn_at_open(hgobj gobj, const char *treedb_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *w = json_object_get(priv->jn_withdrawn_at_open, treedb_name);
    return w? json_deep_copy(w) : json_object();
}

/***************************************************************************
 *  What the projection of a treedb could NOT remove or write, as its record
 *  says (ids of __system__: a snapshot holds them, or an error was logged):
 *  [] when the projection is complete. Until it is, what __system__ shows
 *  of these over the file is nobody's draft, as long as it stays as the
 *  projection left it (see leftovers_as_left). Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *unfinished_projection(hgobj gobj, const char *treedb_name)
{
    json_t *ids = json_array();
    json_t *record = load_unfinished_record(gobj, treedb_name);
    if(record) {
        json_array_extend(ids, kw_get_list(gobj, record, "not_removed", 0, 0));
        json_array_extend(ids, kw_get_list(gobj, record, "not_written", 0, 0));
    }
    JSON_DECREF(record)
    return ids;
}

/***************************************************************************
 *  The treedbs this service opened, one row each: whether it opens with
 *  its schema from C and who decided it (the yuno's code, the configuration
 *  dynamic_schema_treedbs, or the default impose_c_schema), the version of
 *  the literal, of the schema file in use and of the saved one.
 *  `treedb_system_schema` comes first: it is opened by this service too,
 *  always from C. `opened` is FALSE for a treedb whose last open-treedb
 *  created its services but did not open it (treedb_open_db() refused its
 *  schema): the row is there until close-treedb takes them away.
 ***************************************************************************/
PRIVATE json_t *cmd_treedbs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  `master` is what each tranger IS: a master that lost its lock goes
     *  on as a replica, and the `master` attribute says what was configured.
     */
    json_t *jn_data = json_array();
    json_array_append_new(jn_data, json_pack("{s:s, s:b, s:b, s:s, s:I, s:I, s:I, s:b, s:b, s:{}, s:[]}",
        "treedb_name", TREEDB_SYSTEM_SCHEMA_NAME,
        "opened", 1,
        "impose_c_schema", 1,
        "decided_by", "system",
        "c_schema_version", priv->system_schema_version,
        "in_use_schema_version", priv->system_schema_version,
        "saved_schema_version", (json_int_t)0,
        "master", system_is_written_here(gobj),
        "stopped", tranger_is_stopped(priv->tranger_system_),
        "withdrawn_at_open",
        "unfinished_projection"
    ));

    char saved_dir[PATH_MAX];
    saved_schema_dir(gobj, saved_dir, sizeof(saved_dir));

    const char *name; json_t *jn_schema;
    json_object_foreach(priv->jn_c_schemas, name, jn_schema) {
        char filename[NAME_MAX];
        snprintf(filename, sizeof(filename), "%s.treedb_schema.json", name);

        json_int_t in_use_version = 0;
        char in_use_dir[PATH_MAX];
        if(in_use_schema_dir(gobj, name, in_use_dir, sizeof(in_use_dir)) == 0) {
            json_t *in_use = load_json_from_file(gobj, in_use_dir, filename, 0);
            in_use_version = schema_version_of(gobj, in_use);
            JSON_DECREF(in_use)
        }
        json_int_t saved_version = 0;
        if(file_exists(saved_dir, filename)) {
            json_t *saved = load_json_from_file(gobj, saved_dir, filename, 0);
            saved_version = schema_version_of(gobj, saved);
            JSON_DECREF(saved)
        }

        hgobj gobj_node = gobj_find_service(name, FALSE);
        json_t *tranger = is_treedb_opened_here(gobj, gobj_node)?
            gobj_read_pointer_attr(gobj_node, "tranger") : NULL;
        BOOL opened = (is_treedb_opened_here(gobj, gobj_node) &&
            !json_object_get(priv->jn_not_opened, name))? TRUE : FALSE;
        json_array_append_new(jn_data, json_pack("{s:s, s:b, s:b, s:s, s:I, s:I, s:I, s:b, s:b, s:o, s:o}",
            "treedb_name", name,
            "opened", opened,
            "impose_c_schema", treedb_schema_imposed(gobj, name),
            "decided_by", impose_decided_by(gobj, name),
            "c_schema_version", kw_get_int(gobj, jn_schema, "schema_version", 0, KW_WILD_NUMBER),
            "in_use_schema_version", in_use_version,
            "saved_schema_version", saved_version,
            "master", treedb_is_written_here(gobj, name),
            "stopped", tranger_is_stopped(tranger),
            "withdrawn_at_open", withdrawn_at_open(gobj, name),
            "unfinished_projection", unfinished_projection(gobj, name)
        ));
    }

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *  A schema command with no `treedb_name` acts on every treedb opened here
 *  with a schema from C, as diff-schema does: one request per C_TREEDB is
 *  what a console holding a whole yuno wants. Each treedb answers as if it
 *  had been asked alone, and the answer lists them. apply-schema has its
 *  own (apply_every_saved_schema): it is all or none.
 ***************************************************************************/
PRIVATE json_t *for_every_treedb(
    hgobj gobj,
    const char *cmd,
    json_t *kw,     // owned
    hgobj src,
    json_t *(*one)(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *answers = json_array();
    int result = 0;
    int done = 0;
    json_t *failed = json_array();
    const char *name; json_t *jn_schema;
    json_object_foreach(priv->jn_c_schemas, name, jn_schema) {
        json_t *kw_one = json_deep_copy(kw);
        json_object_set_new(kw_one, "treedb_name", json_string(name));
        json_t *answer = one(gobj, cmd, kw_one, src);
        int r = (int)kw_get_int(gobj, answer, "result", -1, 0);
        if(r < 0) {
            result = r;
            json_array_append_new(failed, json_string(name));
        }
        json_array_append_new(answers, json_pack("{s:s, s:i, s:O, s:O}",
            "treedb_name", name,
            "result", r,
            "comment", json_object_get(answer, "comment")?json_object_get(answer, "comment"):json_null(),
            "data", json_object_get(answer, "data")?json_object_get(answer, "data"):json_null()
        ));
        JSON_DECREF(answer)
        done++;
    }

    /*
     *  No atomicity across treedbs: each one answered for itself, and the
     *  ones that succeeded are done. The comment names the ones that were
     *  not, so a -1 does not hide a completed write.
     */
    json_t *comment;
    if(json_array_size(failed) > 0) {
        json_t *jn_names = json_string("");
        size_t i; json_t *jn_name;
        json_array_foreach(failed, i, jn_name) {
            json_t *joined = json_sprintf("%s%s%s",
                json_string_value(jn_names), i? ", " : "", json_string_value(jn_name));
            JSON_DECREF(jn_names)
            jn_names = joined;
        }
        comment = json_sprintf("%s: %s, %d treedb(s), FAILED for: %s",
            gobj_yuno_role_plus_name(), cmd, done, json_string_value(jn_names));
        JSON_DECREF(jn_names)
    } else {
        comment = json_sprintf("%s: %s, %d treedb(s)", gobj_yuno_role_plus_name(), cmd, done);
    }
    JSON_DECREF(failed)

    return msg_iev_build_response(gobj,
        result,
        comment,
        0,
        answers,
        kw  // owned
    );
}

/***************************************************************************
 *  Can the file in use of `treedb_name`'s schema be written here? The
 *  tranger's EFFECTIVE flag when the treedb is open (a master that could
 *  not take the store in exclusive opened as a replica -- what reconcile
 *  reads), this service's `master` attribute when it is not.
 *
 *  The name comes from the wire: only a treedb THIS service opened is read.
 *  Found by name alone it was any service, and its `tranger` attribute was
 *  read on whatever gclass it had.
 ***************************************************************************/
PRIVATE BOOL treedb_is_written_here(hgobj gobj, const char *treedb_name)
{
    hgobj gobj_node = gobj_find_service(treedb_name, FALSE);
    json_t *tranger = is_treedb_opened_here(gobj, gobj_node)?
        gobj_read_pointer_attr(gobj_node, "tranger") : NULL;
    if(tranger) {
        return tranger_writes_now(gobj, tranger);
    }
    return gobj_read_bool_attr(gobj, "master");
}

/***************************************************************************
 *  Is `tranger` STOPPED? tranger2_stop() marks it `__closed__` and gives
 *  its lock back; the first write or topic open after it revives the
 *  handle and takes the lock again (the next start of its service).
 ***************************************************************************/
PRIVATE BOOL tranger_is_stopped(json_t *tranger)
{
    return json_is_true(json_object_get(tranger, "__closed__"))? TRUE: FALSE;
}

/***************************************************************************
 *  May `tranger` write NOW? Its `master` flag -- except while it is
 *  STOPPED: it holds no lock then, and its `master` still says what it
 *  was before the stop, TRUE even when another process takes the store
 *  meanwhile: a precheck that read that stale TRUE would go on to a
 *  __system__ whose treedb is closed.
 ***************************************************************************/
PRIVATE BOOL tranger_writes_now(hgobj gobj, json_t *tranger)
{
    if(!tranger || tranger_is_stopped(tranger)) {
        return FALSE;
    }
    return kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED);
}

/***************************************************************************
 *  Can __system__ be written here? What save-schema writes -- the versions
 *  of the draft, and `saved_schemas/` -- is under the __system__ tranger,
 *  not the client treedb's: the two can be a master and a replica apart.
 ***************************************************************************/
PRIVATE BOOL system_is_written_here(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return tranger_writes_now(gobj, priv->tranger_system_);
}

/***************************************************************************
 *  Publish the draft of a schema: the owner's design of M36 (2026-09-21
 *  review).
 *
 *  An edit of __system__ is a draft and moves no version. This compares
 *  the draft with the schema file the treedb is USING, raises the
 *  topic_version of every topic that differs and the schema_version, writes
 *  them into __system__, and writes the result to `saved_schemas/` under the
 *  __system__ tranger -- never over the file in use: apply-schema does that.
 *  A version is the one in use + 1, so a second save of the same draft
 *  publishes the same numbers.
 *
 *  A draft that is the file in use again (an edit taken back after a save)
 *  has nothing to save, and WITHDRAWS a saved schema newer than the file in
 *  use: the file is removed and the answer says `withdrawn`.
 ***************************************************************************/
PRIVATE json_t *cmd_save_schema(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *permission = "write";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    BOOL dry_run = kw_get_bool(gobj, kw, "dry_run", 0, KW_WILD_NUMBER);
    if(empty_string(treedb_name)) {
        return for_every_treedb(gobj, cmd, kw, src, cmd_save_schema);
    }
    if(!dry_run && !system_is_written_here(gobj)) {
        return build_readonly_response(gobj, gobj_name(priv->gobj_node_system), priv->tranger_system_, kw);
    }

    char in_use_dir[PATH_MAX];
    if(in_use_schema_dir(gobj, treedb_name, in_use_dir, sizeof(in_use_dir))<0) {
        return msg_iev_build_response(gobj, -1,
            json_sprintf("%s: treedb '%s' is not open here", gobj_yuno_role_plus_name(), treedb_name),
            0, 0, kw
        );
    }

    /*
     *  What an unfinished projection could not remove or write is still
     *  in __system__ as it was, and a save would publish it: a topic the
     *  projection could not remove would come back with the next apply
     */
    json_t *unfinished = unfinished_projection(gobj, treedb_name);
    if(json_array_size(unfinished) > 0) {
        return msg_iev_build_response(gobj, -1,
            json_sprintf("%s: the projection of '%s' into __system__ is not complete, "
                "%d node(s) could not be removed or written (see the log): a save would "
                "publish them",
                gobj_yuno_role_plus_name(), treedb_name, (int)json_array_size(unfinished)),
            0,
            json_pack("{s:s, s:o}", "treedb_name", treedb_name, "unfinished_projection", unfinished),
            kw
        );
    }
    JSON_DECREF(unfinished)

    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.treedb_schema.json", treedb_name);
    json_t *in_use = load_json_from_file(gobj, in_use_dir, filename, 0);
    if(!in_use) {
        return msg_iev_build_response(gobj, -1,
            json_sprintf("%s: cannot read the schema in use of '%s'", gobj_yuno_role_plus_name(), treedb_name),
            0, 0, kw
        );
    }

    /*
     *  What the draft changes against the file in use, by topic
     */
    json_t *rows = json_array();
    json_t *summary = diff_treedb_schema(gobj, treedb_name, in_use, rows);
    JSON_DECREF(summary)
    json_t *changed = draft_changed_from_rows(gobj, rows);

    char saved_dir[PATH_MAX];
    saved_schema_dir(gobj, saved_dir, sizeof(saved_dir));
    char saved_path[PATH_MAX];
    build_path(saved_path, sizeof(saved_path), saved_dir, filename, NULL);

    if(json_object_size(changed) == 0) {
        /*
         *  The draft IS the file in use. A saved schema newer than that
         *  file is a save the operator has taken back in the editor, and it
         *  is withdrawn. 7.25.4 left it in place: saved-schema went on
         *  diffing the draft against it (the mark of an unsaved change that
         *  never cleared) and apply-schema would have installed the change
         *  taken back. The versions
         *  that save wrote into __system__ stay: a number there never goes
         *  down, and the next save publishes the one in use + 1 anyway.
         */
        json_int_t in_use_version = schema_version_of(gobj, in_use);
        json_int_t saved_version = 0;
        if(file_exists(saved_dir, filename)) {
            json_t *saved = load_json_from_file(gobj, saved_dir, filename, 0);
            saved_version = schema_version_of(gobj, saved);
            JSON_DECREF(saved)
        }
        BOOL withdraw = (saved_version > in_use_version)? TRUE: FALSE;
        JSON_DECREF(changed)
        JSON_DECREF(in_use)

        if(withdraw && !dry_run) {
            if(file_remove(saved_dir, filename) < 0) {
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_SYSTEM,
                    "msg",              "%s", "Cannot remove the saved schema of a reverted draft",
                    "treedb_name",      "%s", treedb_name,
                    "path",             "%s", saved_path,
                    "errno",            "%s", strerror(errno),
                    NULL
                );
                JSON_DECREF(rows)
                return msg_iev_build_response(gobj, -1,
                    json_sprintf("%s: the draft of '%s' is the schema in use, but its saved "
                        "schema_version %d could not be withdrawn from %s (see the log)",
                        gobj_yuno_role_plus_name(), treedb_name, (int)saved_version, saved_path),
                    0, 0, kw
                );
            }
            gobj_log_info(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INFO,
                "msg",              "%s", "Saved schema withdrawn, the draft is the schema in use",
                "treedb_name",      "%s", treedb_name,
                "schema_version",   "%d", (int)saved_version,
                "in_use_version",   "%d", (int)in_use_version,
                "path",             "%s", saved_path,
                NULL
            );
        }

        json_t *comment;
        if(withdraw) {
            comment = json_sprintf("%s: the draft of '%s' is the schema in use: the saved "
                "schema_version %d %s withdrawn",
                gobj_yuno_role_plus_name(), treedb_name, (int)saved_version,
                dry_run? "would be": "is");
        } else {
            comment = json_sprintf("%s: nothing to save, the draft of '%s' is the schema in use",
                gobj_yuno_role_plus_name(), treedb_name);
        }
        return msg_iev_build_response(gobj, 0,
            comment,
            0,
            json_pack("{s:s, s:b, s:I, s:s, s:o}",
                "treedb_name", treedb_name,
                "withdrawn", withdraw,
                "schema_version", withdraw? saved_version : in_use_version,
                "path", saved_path,
                "changes", rows
            ),
            kw
        );
    }

    json_t *schema = get_treedb_schema(gobj, treedb_name);
    if(schema) {
        prune_schema(schema);
    }
    if(!schema) {
        JSON_DECREF(changed)
        JSON_DECREF(in_use)
        JSON_DECREF(rows)
        return msg_iev_build_response(gobj, -1,
            json_sprintf("%s: cannot rebuild the draft of '%s' from __system__",
                gobj_yuno_role_plus_name(), treedb_name),
            0, 0, kw
        );
    }

    /*
     *  A draft with no topic left is not a schema a treedb can open with:
     *  saved and applied, the treedb would not open again
     */
    if(!schema_has_topics(schema)) {
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_TREEDB,
            "msg",              "%s", "Draft of a treedb schema with no topics: not saved, a treedb without topics does not open",
            "treedb_name",      "%s", treedb_name,
            NULL
        );
        JSON_DECREF(schema)
        JSON_DECREF(changed)
        JSON_DECREF(in_use)
        return msg_iev_build_response(gobj, -1,
            json_sprintf("%s: cannot save the schema of '%s': its draft in __system__ has "
                "no topics, and a treedb without topics does not open",
                gobj_yuno_role_plus_name(), treedb_name),
            0,
            json_pack("{s:s, s:o}", "treedb_name", treedb_name, "changes", rows),
            kw
        );
    }

    /*
     *  The versions: the one in use + 1, or the draft's when it is already
     *  ahead of that
     */
    json_int_t in_use_version = schema_version_of(gobj, in_use);
    json_int_t schema_version = kw_get_int(gobj, schema, "schema_version", 0, KW_WILD_NUMBER);
    if(schema_version < in_use_version + 1) {
        schema_version = in_use_version + 1;
    }
    json_object_set_new(schema, "schema_version", json_integer(schema_version));

    json_t *versions = json_object();
    int idx; json_t *topic;
    json_array_foreach(json_object_get(schema, "topics"), idx, topic) {
        const char *topic_name = kw_get_str(gobj, topic, "id", "", 0);
        if(!json_object_get(changed, topic_name)) {
            continue;
        }
        json_int_t v = kw_get_int(gobj, topic, "topic_version", 0, KW_WILD_NUMBER);
        json_int_t in_use_v = schema_topic_version(gobj, in_use, topic_name);
        if(v < in_use_v + 1) {
            v = in_use_v + 1;
        }
        json_object_set_new(topic, "topic_version", json_integer(v));
        json_object_set_new(versions, topic_name, json_integer(v));
    }
    JSON_DECREF(changed)
    JSON_DECREF(in_use)

    if(!dry_run) {
        /*
         *  Into __system__ first: the draft IS the saved schema, versions
         *  included, so the next save of it publishes the same numbers
         */
        int ret = 0;
        json_t *jn_v;
        const char *topic_name;
        json_object_foreach(versions, topic_name, jn_v) {
            char topic_id[RECORD_KEY_VALUE_MAX];
            if(!build_schema_node_id(gobj, topic_id, sizeof(topic_id), treedb_name, topic_name)) {
                ret = -1;   // Error already logged
                continue;
            }
            json_t *node = gobj_update_node(
                priv->gobj_node_system,
                "topics",
                json_pack("{s:s, s:O}", "id", topic_id, "topic_version", jn_v),
                0,
                gobj
            );
            if(!node) {
                ret = -1;   // Error already logged
            }
            JSON_DECREF(node)
        }
        json_t *node = gobj_update_node(
            priv->gobj_node_system,
            "treedbs",
            json_pack("{s:s, s:I}", "id", treedb_name, "schema_version", schema_version),
            0,
            gobj
        );
        if(!node) {
            ret = -1;   // Error already logged
        }
        JSON_DECREF(node)

        if(ret == 0) {
            mkrdir(saved_dir, (int)gobj_read_integer_attr(gobj, "xpermission"));
            ret = save_json_to_file(
                gobj,
                saved_dir,
                filename,
                (int)gobj_read_integer_attr(gobj, "xpermission"),
                (int)gobj_read_integer_attr(gobj, "rpermission"),
                0,
                TRUE,   // create or overwrite
                FALSE,
                json_incref(schema)
            );
        }
        if(ret < 0) {
            JSON_DECREF(schema)
            JSON_DECREF(versions)
            JSON_DECREF(rows)
            return msg_iev_build_response(gobj, -1,
                json_sprintf("%s: cannot save the schema of '%s': the versions of its draft "
                    "or the file %s could not be written (see the log)",
                    gobj_yuno_role_plus_name(), treedb_name, saved_path),
                0, 0, kw
            );
        }
        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "Schema saved",
            "treedb_name",      "%s", treedb_name,
            "schema_version",   "%d", (int)schema_version,
            "path",             "%s", saved_path,
            NULL
        );
    }

    return msg_iev_build_response(gobj, 0,
        json_sprintf("%s: %s '%s', schema_version %d%s",
            gobj_yuno_role_plus_name(),
            dry_run? "would save": "saved",
            treedb_name,
            (int)schema_version,
            dry_run? "": "; apply-schema puts it in use"),
        0,
        json_pack("{s:s, s:I, s:o, s:s, s:o, s:o}",
            "treedb_name", treedb_name,
            "schema_version", schema_version,
            "topic_versions", versions,
            "path", saved_path,
            "changes", rows,
            "schema", schema
        ),
        kw
    );
}

/***************************************************************************
 *  The schema save-schema wrote, what it changes against the file in use
 *  (as a table: `json2flat` ids, see flat_diff), and whether it can be
 *  applied here -- the question the Apply dialog asks.
 ***************************************************************************/
PRIVATE json_t *cmd_saved_schema(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    if(empty_string(treedb_name)) {
        return for_every_treedb(gobj, cmd, kw, src, cmd_saved_schema);
    }
    char in_use_dir[PATH_MAX];
    if(in_use_schema_dir(gobj, treedb_name, in_use_dir, sizeof(in_use_dir))<0) {
        return msg_iev_build_response(gobj, -1,
            json_sprintf("%s: treedb '%s' is not open here", gobj_yuno_role_plus_name(), treedb_name),
            0, 0, kw
        );
    }
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.treedb_schema.json", treedb_name);
    char saved_dir[PATH_MAX];
    saved_schema_dir(gobj, saved_dir, sizeof(saved_dir));
    char saved_path[PATH_MAX];
    build_path(saved_path, sizeof(saved_path), saved_dir, filename, NULL);

    json_t *in_use = load_json_from_file(gobj, in_use_dir, filename, 0);
    json_t *saved = file_exists(saved_dir, filename)?
        load_json_from_file(gobj, saved_dir, filename, 0): NULL;

    json_int_t in_use_version = schema_version_of(gobj, in_use);
    json_int_t saved_version = schema_version_of(gobj, saved);
    BOOL imposed = treedb_schema_imposed(gobj, treedb_name);
    BOOL master = treedb_is_written_here(gobj, treedb_name);

    /*
     *  A saved schema not newer than the file in use is not a pending save:
     *  it is STALE (one left by an older release, or a remove that failed),
     *  and it is neither diffed nor applicable. It used to answer `saved`
     *  with the diff of a schema already in use.
     *
     *  One that cannot be READ is BROKEN: its version is unknown, so it is
     *  no pending save either, and apply-schema leaves it out.
     */
    BOOL pending = (saved && saved_version > in_use_version)? TRUE: FALSE;
    BOOL broken = (!saved && file_exists(saved_dir, filename))? TRUE: FALSE;
    BOOL stale = (file_exists(saved_dir, filename) && !pending && !broken)? TRUE: FALSE;

    /*
     *  A pending save with no topics is refused by apply-schema (a treedb
     *  without topics does not open): it cannot be applied, and the answer
     *  says why.
     */
    BOOL no_topics = (pending && !schema_has_topics(saved))? TRUE: FALSE;

    json_t *diff = json_object();
    if(in_use && pending) {
        JSON_DECREF(diff)
        diff = schema_diff(in_use, saved);
    }

    /*
     *  The topics whose DRAFT in __system__ is not saved: what the schema
     *  editor marks as unsaved. It kept that mark in the memory of one
     *  session only, so a reload of the page lost it while __system__ still
     *  differed from the file (N13 of the 2026-09-22 review); this is the
     *  data it is rebuilt from.
     *
     *  Not saved means different from the SAVED schema when there is one
     *  newer than the file in use: diffed against the file in use, every
     *  topic a save had just published was still "changed", and the editor
     *  said "unsaved changes" until an Apply -- for ever on an imposed
     *  treedb (M1 of the 2026-09-23 review). Saved but not in use is the
     *  other question, and `diff` / `can_apply` already answer it.
     */
    json_t *draft_base = pending? saved : in_use;
    json_t *draft_changed = json_object();
    if(draft_base) {
        json_t *rows = json_array();
        json_t *summary = diff_treedb_schema(gobj, treedb_name, draft_base, rows);
        JSON_DECREF(summary)

        /*
         *  What an unfinished projection left, as it left it, is nobody's
         *  draft: an edit of it is (see leftovers_as_left), an unlink or a
         *  delete included
         */
        json_t *record = load_unfinished_record(gobj, treedb_name);
        json_t *edited = json_object();
        json_t *leftovers = leftovers_as_left(gobj, treedb_name, record, edited);
        rows = rows_without_leftovers(gobj, treedb_name, rows, leftovers);
        JSON_DECREF(leftovers)
        JSON_DECREF(record)

        JSON_DECREF(draft_changed)
        draft_changed = draft_changed_from_rows(gobj, rows);
        JSON_DECREF(rows)

        const char *edited_id; json_t *jn_topic;
        json_object_foreach(edited, edited_id, jn_topic) {
            json_object_set_new(draft_changed, json_string_value(jn_topic), json_true());
        }
        JSON_DECREF(edited)
    }
    JSON_DECREF(in_use)
    JSON_DECREF(saved)

    json_t *comment = 0;
    if(broken) {
        comment = json_sprintf("%s: the saved schema of '%s' cannot be read (see the log): "
            "it is left out of apply-schema, save again to replace it",
            gobj_yuno_role_plus_name(), treedb_name);
    } else if(no_topics) {
        comment = json_sprintf("%s: the saved schema of '%s' has no topics: apply-schema "
            "refuses it, a treedb without topics does not open",
            gobj_yuno_role_plus_name(), treedb_name);
    }

    return msg_iev_build_response(gobj, 0,
        comment,
        0,
        json_pack("{s:s, s:b, s:b, s:b, s:b, s:b, s:I, s:I, s:b, s:s, s:o, s:o, s:o, s:o}",
            "treedb_name", treedb_name,
            "impose_c_schema", imposed,
            "master", master,
            "saved", pending,
            "stale", stale,
            "broken", broken,
            "in_use_schema_version", in_use_version,
            "saved_schema_version", saved_version,
            "can_apply", master && !imposed && saved_version > in_use_version && !no_topics,
            "path", saved_path,
            "diff", diff,
            "draft_changed", draft_changed,
            "withdrawn_at_open", withdrawn_at_open(gobj, treedb_name),
            "unfinished_projection", unfinished_projection(gobj, treedb_name)
        ),
        kw
    );
}

/***************************************************************************
 *  The topics the rows of diff_treedb_schema() name as changed, as a dict
 *  {topic: true}: a difference of a column or of the topic itself, not a
 *  `version` row (a version behind is the projection that never landed,
 *  not an edit). Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *draft_changed_from_rows(hgobj gobj, json_t *rows)
{
    json_t *changed = json_object();
    int idx; json_t *row;
    json_array_foreach(rows, idx, row) {
        const char *kind = kw_get_str(gobj, row, "kind", "", 0);
        const char *topic_name = kw_get_str(gobj, row, "topic", "", 0);
        if(!empty_string(topic_name) && strcmp(kind, "version")!=0) {
            json_object_set_new(changed, topic_name, json_true());
        }
    }
    return changed;
}

/***************************************************************************
 *  Write `jn_schema` to a TEMPORARY file beside the schema file in use,
 *  flushed to the disk. The file in use is replaced by renaming this one
 *  over it (commit_schema_file): it used to be truncated and rewritten in
 *  place, so a crash or a full disk halfway left it empty -- read at the
 *  next open as version 0, recreated from the literal, and the dynamic
 *  schema of the operator lost with nothing said. Mode `rpermission` (0660),
 *  as treedb_open_db() writes it: never read-only, the next apply rewrites it.
 ***************************************************************************/
PRIVATE int write_schema_tmp(
    hgobj gobj,
    const char *directory,
    const char *filename,
    json_t *jn_schema,  // not owned
    char *tmp_path,
    size_t tmp_path_size
)
{
    char tmp_name[NAME_MAX];
    int written = snprintf(tmp_name, sizeof(tmp_name), ".%s.tmp", filename);
    if(written < 0 || written >= (int)sizeof(tmp_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Temporary schema filename too long",
            "filename",     "%s", filename,
            NULL
        );
        return -1;
    }
    build_path(tmp_path, tmp_path_size, directory, tmp_name, NULL);

    int fd = newfile(tmp_path, (int)gobj_read_integer_attr(gobj, "rpermission"), TRUE);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot create the temporary schema file",
            "path",         "%s", tmp_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    /*
     *  What apply-schema hands here is PARSED (to validate it), and parsing
     *  adds the derived `fkey` marks: written as it was (7.25.4 did), the
     *  file in use carried them.
     */
    json_t *jn_file = json_deep_copy(jn_schema);
    strip_derived_fkey_marks(jn_file);
    int dumped = json_dumpfd(jn_file, fd, JSON_INDENT(4));
    JSON_DECREF(jn_file)
    if(dumped < 0 || fsync(fd) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot write the temporary schema file",
            "path",         "%s", tmp_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        close(fd);
        unlink(tmp_path);
        return -1;
    }
    if(close(fd) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot close the temporary schema file",
            "path",         "%s", tmp_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        unlink(tmp_path);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Rename the temporary file over the schema file in use: atomic, the file
 *  in use is the old one or the new one and never half of either. The
 *  directory is flushed so the rename survives a crash too.
 ***************************************************************************/
PRIVATE int commit_schema_file(
    hgobj gobj,
    const char *directory,
    const char *filename,
    const char *tmp_path
)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), directory, filename, NULL);

    if(rename(tmp_path, path) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot put the schema file in place",
            "from",         "%s", tmp_path,
            "to",           "%s", path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        unlink(tmp_path);
        return -1;
    }

    int dir_fd = open(directory, O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(dir_fd < 0 || fsync(dir_fd) < 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot flush the directory of the schema file",
            "directory",    "%s", directory,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }
    if(dir_fd >= 0) {
        close(dir_fd);
    }
    return 0;
}

/***************************************************************************
 *  Can the saved schema of `treedb_name` be put in use here? NULL when it
 *  can: then `*p_saved` is the saved schema (YOURS), parsed, and `in_use_dir`
 *  the directory of the file in use. Otherwise the reason, as a comment
 *  (YOURS), and `*p_applicable` says whether there was something to apply at
 *  all: an imposed treedb, a replica, no saved schema or none newer than
 *  the one in use are not applicable -- asked for every treedb, that is not
 *  a failure -- while a saved schema that does not parse is. One that cannot
 *  be READ is not applicable and `*p_broken`: its version is unknown, so
 *  nothing says it is a pending save, and saved-schema answers it `broken`.
 ***************************************************************************/
PRIVATE json_t *check_saved_schema_to_apply(
    hgobj gobj,
    const char *treedb_name,
    json_t **p_saved,           // YOURS when NULL is returned
    char *in_use_dir,
    size_t in_use_dir_size,
    json_int_t *p_saved_version,
    json_int_t *p_in_use_version,
    BOOL *p_applicable,
    BOOL *p_broken              // the saved file exists and cannot be read
)
{
    *p_saved = NULL;
    *p_saved_version = 0;
    *p_in_use_version = 0;
    *p_applicable = FALSE;
    *p_broken = FALSE;

    if(!treedb_is_written_here(gobj, treedb_name)) {
        return json_sprintf("%s: treedb '%s' is READ-ONLY, this yuno is not the master of its "
            "tranger, or the tranger is stopped", gobj_yuno_role_plus_name(), treedb_name);
    }
    if(treedb_schema_imposed(gobj, treedb_name)) {
        return json_sprintf("%s: the schema of '%s' is imposed by the binary (impose_c_schema)",
            gobj_yuno_role_plus_name(), treedb_name);
    }
    if(in_use_schema_dir(gobj, treedb_name, in_use_dir, in_use_dir_size)<0) {
        return json_sprintf("%s: treedb '%s' is not open here", gobj_yuno_role_plus_name(), treedb_name);
    }

    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.treedb_schema.json", treedb_name);
    char saved_dir[PATH_MAX];
    saved_schema_dir(gobj, saved_dir, sizeof(saved_dir));
    if(!file_exists(saved_dir, filename)) {
        return json_sprintf("%s: no saved schema of '%s' (save-schema)", gobj_yuno_role_plus_name(), treedb_name);
    }

    json_t *in_use = load_json_from_file(gobj, in_use_dir, filename, 0);
    *p_in_use_version = schema_version_of(gobj, in_use);
    JSON_DECREF(in_use)

    json_t *saved = load_json_from_file(gobj, saved_dir, filename, 0);
    *p_saved_version = schema_version_of(gobj, saved);
    if(saved && *p_saved_version <= *p_in_use_version) {
        JSON_DECREF(saved)
        return json_sprintf("%s: the saved schema of '%s' (%d) is not newer than the one in use (%d)",
            gobj_yuno_role_plus_name(), treedb_name, (int)*p_saved_version, (int)*p_in_use_version);
    }

    if(!saved) {
        *p_broken = TRUE;
        return json_sprintf("%s: the saved schema of '%s' cannot be read (see the log)",
            gobj_yuno_role_plus_name(), treedb_name);
    }
    *p_applicable = TRUE;
    if(!schema_has_topics(saved)) {
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_TREEDB,
            "msg",              "%s", "Saved treedb schema with no topics: not applied, a treedb without topics does not open",
            "treedb_name",      "%s", treedb_name,
            "schema_version",   "%d", (int)*p_saved_version,
            NULL
        );
        JSON_DECREF(saved)
        return json_sprintf("%s: the saved schema of '%s' has no topics: a treedb without "
            "topics does not open", gobj_yuno_role_plus_name(), treedb_name);
    }
    if(parse_schema(saved)<0) {
        JSON_DECREF(saved)
        return json_sprintf("%s: the saved schema of '%s' does not parse (see the log)",
            gobj_yuno_role_plus_name(), treedb_name);
    }

    *p_saved = saved;
    return NULL;
}

/***************************************************************************
 *  The `data` of an apply-schema answer, the same for a named apply and for
 *  every row of an unnamed one: `applied` is TRUE only when the file in use
 *  of that treedb was replaced -- what says whether its yuno must restart.
 ***************************************************************************/
PRIVATE json_t *apply_schema_data(
    const char *treedb_name,
    BOOL applied,
    json_int_t saved_version,
    json_int_t in_use_version
)
{
    return json_pack("{s:s, s:b, s:I, s:I}",
        "treedb_name", treedb_name,
        "applied", applied,
        "saved_schema_version", saved_version,
        "in_use_schema_version", in_use_version
    );
}

/***************************************************************************
 *  Put the saved schema in place of the file in use. Only on the master,
 *  only for a treedb whose schema C does not impose (the literal would
 *  overwrite it at the next open), and only a saved schema NEWER than the
 *  one in use. It is read at the next open of the treedb: whoever calls
 *  this restarts the yuno -- when `data.applied` says so.
 ***************************************************************************/
PRIVATE json_t *cmd_apply_schema(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *permission = "create-delete";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    if(empty_string(treedb_name)) {
        return apply_every_saved_schema(gobj, cmd, kw);
    }

    json_t *saved;
    char in_use_dir[PATH_MAX];
    json_int_t saved_version, in_use_version;
    BOOL applicable, broken;
    json_t *refused = check_saved_schema_to_apply(
        gobj, treedb_name, &saved, in_use_dir, sizeof(in_use_dir),
        &saved_version, &in_use_version, &applicable, &broken
    );
    if(refused) {
        json_t *data = apply_schema_data(treedb_name, FALSE, saved_version, in_use_version);
        if(broken) {
            json_object_set_new(data, "broken", json_true());
        }
        return msg_iev_build_response(gobj, -1,
            refused,
            0,
            data,
            kw
        );
    }

    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.treedb_schema.json", treedb_name);
    char tmp_path[PATH_MAX];
    json_t *replaced_file = load_json_from_file(gobj, in_use_dir, filename, 0);
    json_t *previous_record = NULL;
    int ret = write_schema_tmp(gobj, in_use_dir, filename, saved, tmp_path, sizeof(tmp_path));
    if(ret == 0) {
        /*
         *  The record first: an apply nobody could report is not made
         */
        ret = record_apply(gobj, treedb_name, saved, replaced_file, &previous_record);
        if(ret < 0) {
            unlink(tmp_path);   // Error already logged
        }
    }
    if(ret == 0) {
        ret = commit_schema_file(gobj, in_use_dir, filename, tmp_path);
        if(ret < 0) {
            restore_apply_record(gobj, treedb_name, previous_record);   // Error already logged
        }
    }
    JSON_DECREF(previous_record)
    JSON_DECREF(replaced_file)
    JSON_DECREF(saved)
    if(ret < 0) {
        return msg_iev_build_response(gobj, -1,
            json_sprintf("%s: cannot write the schema of '%s' or the record of its apply, "
                "the file in use is unchanged (see the log)",
                gobj_yuno_role_plus_name(), treedb_name),
            0,
            apply_schema_data(treedb_name, FALSE, saved_version, in_use_version),
            kw
        );
    }
    gobj_log_info(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_INFO,
        "msg",              "%s", "Schema applied",
        "treedb_name",      "%s", treedb_name,
        "schema_version",   "%d", (int)saved_version,
        NULL
    );

    /*
     *  Applied, the saved schema IS the file in use. Kept, saved-schema
     *  answered `saved` with a diff for a schema already in use.
     */
    json_int_t removed_version;
    remove_saved_schema(gobj, treedb_name, &removed_version);  // Error already logged

    return msg_iev_build_response(gobj, 0,
        json_sprintf("%s: schema %d of '%s' in place, read at the next open of the treedb",
            gobj_yuno_role_plus_name(), (int)saved_version, treedb_name),
        0,
        apply_schema_data(treedb_name, TRUE, saved_version, in_use_version),
        kw
    );
}

/***************************************************************************
 *  apply-schema with no `treedb_name`: every treedb opened here whose saved
 *  schema can be applied, ALL OR NONE UP TO THE RENAMES (M2 of the
 *  2026-09-23 review). Phase 3 below can be partial, and each row's
 *  `applied` is what says so.
 *
 *  It applied them one by one: A replaced, B refused, answer -1 -- and a
 *  console that reads -1 as "nothing happened" did not restart, so A went
 *  in use silently at some later start. Now:
 *
 *    1. every applicable treedb is checked (master, not imposed, a saved
 *       schema newer than the one in use, that loads and parses); one that
 *       fails refuses them ALL and nothing is written;
 *    2. every new file is written to a temporary one, flushed; one that
 *       cannot be written refuses them all, and the temporaries go;
 *    3. then each is recorded (record_apply) and renamed over its file in use.
 *
 *  Only the record of an apply (record_apply) or a rename can fail after
 *  that, and it fails on its own: its row says applied=false while the
 *  others say true.
 *
 *  A treedb with nothing to apply is left out, as it always was: an apply
 *  with nothing applicable answers 0 and no row, and nobody restarts.
 *
 *  A treedb whose saved file cannot be READ is left out too, and SAID: its
 *  row says `applied: false, broken: true` and the answer is -1. Its
 *  version is unknown, so it is no pending save (saved-schema answers it
 *  `broken`), and it does not refuse the others.
 ***************************************************************************/
PRIVATE json_t *apply_every_saved_schema(hgobj gobj, const char *cmd, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *plan = json_array();    // [{treedb_name, saved, in_use_dir, versions, refused}]
    json_t *failed = json_array();  // names
    json_t *broken_rows = json_array();

    const char *name; json_t *jn_c_schema;
    json_object_foreach(priv->jn_c_schemas, name, jn_c_schema) {
        json_t *saved;
        char in_use_dir[PATH_MAX];
        json_int_t saved_version, in_use_version;
        BOOL applicable, broken;
        json_t *refused = check_saved_schema_to_apply(
            gobj, name, &saved, in_use_dir, sizeof(in_use_dir),
            &saved_version, &in_use_version, &applicable, &broken
        );
        if(broken) {
            /*
             *  Left out, and said: its version is unknown, so it is no
             *  pending save, and it does not decide the others
             */
            json_t *data = apply_schema_data(name, FALSE, saved_version, in_use_version);
            json_object_set_new(data, "broken", json_true());
            json_array_append_new(broken_rows, json_pack("{s:s, s:i, s:o, s:o}",
                "treedb_name", name,
                "result", -1,
                "comment", json_sprintf("%s: the saved schema of '%s' cannot be read (see the "
                    "log): left out, save again to replace it", gobj_yuno_role_plus_name(), name),
                "data", data
            ));
            JSON_DECREF(refused)
            continue;
        }
        if(!applicable) {
            JSON_DECREF(refused)
            continue;
        }
        json_t *entry = json_pack("{s:s, s:s, s:I, s:I}",
            "treedb_name", name,
            "in_use_dir", in_use_dir,
            "saved_schema_version", saved_version,
            "in_use_schema_version", in_use_version
        );
        if(refused) {
            json_object_set_new(entry, "refused", refused);
            json_array_append_new(failed, json_string(name));
        } else {
            json_object_set_new(entry, "saved", saved);
        }
        json_array_append_new(plan, entry);
    }

    /*
     *  Phase 2: the temporaries, only when every treedb passed phase 1
     */
    int idx; json_t *entry;
    if(json_array_size(failed) == 0) {
        json_array_foreach(plan, idx, entry) {
            const char *treedb_name = kw_get_str(gobj, entry, "treedb_name", "", 0);
            char filename[NAME_MAX];
            snprintf(filename, sizeof(filename), "%s.treedb_schema.json", treedb_name);
            char tmp_path[PATH_MAX];
            if(write_schema_tmp(gobj,
                    kw_get_str(gobj, entry, "in_use_dir", "", 0),
                    filename,
                    json_object_get(entry, "saved"),
                    tmp_path, sizeof(tmp_path))<0) {
                json_object_set_new(entry, "refused",
                    json_sprintf("%s: cannot write the schema of '%s' (see the log)",
                        gobj_yuno_role_plus_name(), treedb_name));
                json_array_append_new(failed, json_string(treedb_name));
                continue;
            }
            json_object_set_new(entry, "tmp_path", json_string(tmp_path));
        }
    }

    json_t *names = json_string("");
    size_t i; json_t *jn_name;
    json_array_foreach(failed, i, jn_name) {
        json_t *joined = json_sprintf("%s%s%s",
            json_string_value(names), i? ", " : "", json_string_value(jn_name));
        JSON_DECREF(names)
        names = joined;
    }

    /*
     *  Phase 3: all or none
     */
    json_t *rows = json_array();
    int result = 0;
    int applied_count = 0;
    json_array_foreach(plan, idx, entry) {
        const char *treedb_name = kw_get_str(gobj, entry, "treedb_name", "", 0);
        json_int_t saved_version = kw_get_int(gobj, entry, "saved_schema_version", 0, 0);
        json_int_t in_use_version = kw_get_int(gobj, entry, "in_use_schema_version", 0, 0);
        const char *tmp_path = kw_get_str(gobj, entry, "tmp_path", "", 0);
        json_t *comment;
        BOOL applied = FALSE;

        if(json_array_size(failed) > 0) {
            if(!empty_string(tmp_path)) {
                unlink(tmp_path);
            }
            comment = json_object_get(entry, "refused")?
                json_incref(json_object_get(entry, "refused")) :
                json_sprintf("%s: '%s' not applied: %s cannot be applied, nothing was written",
                    gobj_yuno_role_plus_name(), treedb_name, json_string_value(names));
        } else {
            char filename[NAME_MAX];
            snprintf(filename, sizeof(filename), "%s.treedb_schema.json", treedb_name);
            json_t *replaced_file = load_json_from_file(gobj,
                kw_get_str(gobj, entry, "in_use_dir", "", 0), filename, 0);
            json_t *previous_record = NULL;
            int committed = record_apply(gobj, treedb_name,
                json_object_get(entry, "saved"), replaced_file, &previous_record);
            if(committed < 0) {
                unlink(tmp_path);   // Error already logged
            } else {
                committed = commit_schema_file(gobj,
                    kw_get_str(gobj, entry, "in_use_dir", "", 0),
                    filename,
                    tmp_path);
                if(committed < 0) {
                    restore_apply_record(gobj, treedb_name, previous_record);   // Error already logged
                }
            }
            JSON_DECREF(previous_record)
            JSON_DECREF(replaced_file)
            if(committed<0) {
                comment = json_sprintf("%s: cannot write the schema of '%s' or the record of its apply, "
                    "the file in use is unchanged (see the log)",
                    gobj_yuno_role_plus_name(), treedb_name);
            } else {
                applied = TRUE;
                applied_count++;
                gobj_log_info(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_INFO,
                    "msg",              "%s", "Schema applied",
                    "treedb_name",      "%s", treedb_name,
                    "schema_version",   "%d", (int)saved_version,
                    NULL
                );
                comment = json_sprintf("%s: schema %d of '%s' in place, read at the next open of the treedb",
                    gobj_yuno_role_plus_name(), (int)saved_version, treedb_name);
                json_int_t removed_version;
                remove_saved_schema(gobj, treedb_name, &removed_version);  // Error already logged
            }
        }
        if(!applied) {
            result = -1;
        }
        json_array_append_new(rows, json_pack("{s:s, s:i, s:o, s:o}",
            "treedb_name", treedb_name,
            "result", applied? 0 : -1,
            "comment", comment,
            "data", apply_schema_data(treedb_name, applied, saved_version, in_use_version)
        ));
    }

    json_t *comment;
    if(json_array_size(failed) > 0) {
        comment = json_sprintf("%s: %s refused, nothing was written: %s cannot be applied",
            gobj_yuno_role_plus_name(), cmd, json_string_value(names));
    } else if(json_array_size(plan) == 0) {
        comment = json_sprintf("%s: %s, nothing to apply", gobj_yuno_role_plus_name(), cmd);
    } else if(result < 0) {
        comment = json_sprintf("%s: %s, %d of %d treedb(s) applied, see each one",
            gobj_yuno_role_plus_name(), cmd, applied_count, (int)json_array_size(plan));
    } else {
        comment = json_sprintf("%s: %s, %d treedb(s) applied",
            gobj_yuno_role_plus_name(), cmd, applied_count);
    }

    /*
     *  The broken ones go last, each with its row, and the answer is -1:
     *  something that looked saved was not applied
     */
    if(json_array_size(broken_rows) > 0) {
        json_t *jn_broken = json_string("");
        int idx2; json_t *row;
        json_array_foreach(broken_rows, idx2, row) {
            json_t *joined = json_sprintf("%s%s%s", json_string_value(jn_broken), idx2? ", " : "",
                kw_get_str(gobj, row, "treedb_name", "", 0));
            JSON_DECREF(jn_broken)
            jn_broken = joined;
        }
        json_t *full = json_sprintf("%s; left out, their saved schema cannot be read: %s",
            json_string_value(comment), json_string_value(jn_broken));
        JSON_DECREF(jn_broken)
        JSON_DECREF(comment)
        comment = full;
        json_array_extend(rows, broken_rows);
        result = -1;
    }
    JSON_DECREF(broken_rows)
    JSON_DECREF(names)
    JSON_DECREF(failed)
    JSON_DECREF(plan)

    return msg_iev_build_response(gobj,
        result,
        comment,
        0,
        rows,
        kw  // owned
    );
}

/***************************************************************************
 *  What the stored schema says that the schema compiled in C does not.
 *
 *  The projection in __system__ is seeded from the schema in C and re-made
 *  whenever that moves ahead, so the two are the same thing until somebody
 *  edits the projection -- and an edit is a draft there until save-schema
 *  and apply-schema. The version numbers of the `treedbs` node say that
 *  SOMETHING was published, never what. This answers what.
 ***************************************************************************/
PRIVATE json_t *cmd_diff_schema(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    /*--------------------------------------------------*
     *  Only a treedb opened with a schema from C can be
     *  compared: that schema is the other half.
     *--------------------------------------------------*/
    json_t *jn_treedbs = json_array();
    if(!empty_string(treedb_name)) {
        if(json_object_get(priv->jn_c_schemas, treedb_name)) {
            json_array_append_new(jn_treedbs, json_string(treedb_name));
        }
    } else {
        const char *name; json_t *jn_schema;
        json_object_foreach(priv->jn_c_schemas, name, jn_schema) {
            json_array_append_new(jn_treedbs, json_string(name));
        }
    }
    if(json_array_size(jn_treedbs) == 0) {
        JSON_DECREF(jn_treedbs)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf(
                "%s: no treedb opened with a schema from C%s%s",
                gobj_yuno_role_plus_name(),
                empty_string(treedb_name)?"":": ",
                empty_string(treedb_name)?"":treedb_name
            ),
            0,
            0,
            kw  // owned
        );
    }

    /*--------------------------------------------------*
     *      Compare
     *--------------------------------------------------*/
    json_t *rows = json_array();
    gbuffer_t *gbuf = gbuffer_create(256, 32*1024);
    gbuffer_printf(gbuf, "%s:", gobj_yuno_role_plus_name());

    int idx; json_t *jn_name;
    json_array_foreach(jn_treedbs, idx, jn_name) {
        const char *name = json_string_value(jn_name);
        size_t differences = json_array_size(rows);

        json_t *summary = diff_treedb_schema(
            gobj,
            name,
            json_object_get(priv->jn_c_schemas, name),
            rows
        );
        if(!summary) {
            gbuffer_printf(gbuf, " %s: cannot be read;", name);
            continue;   // Error already logged
        }
        differences = json_array_size(rows) - differences;

        if(!kw_get_bool(gobj, summary, "projected", 0, 0)) {
            gbuffer_printf(gbuf, " %s: not projected yet;", name);
        } else {
            gbuffer_printf(gbuf,
                " %s: stored schema_version=%d c_schema_version=%d"
                " system_schema_version=%d, in C schema_version=%d"
                " system_schema_version=%d, %d differences;",
                name,
                (int)kw_get_int(gobj, summary, "schema_version", 0, 0),
                (int)kw_get_int(gobj, summary, "c_schema_version", 0, 0),
                (int)kw_get_int(gobj, summary, "system_schema_version", 0, 0),
                (int)kw_get_int(gobj, summary, "c_schema_version_in_c", 0, 0),
                (int)kw_get_int(gobj, summary, "system_schema_version_in_c", 0, 0),
                (int)differences
            );
        }
        json_decref(summary);
    }
    JSON_DECREF(jn_treedbs)

    json_t *jn_comment = json_string(gbuffer_cur_rd_pointer(gbuf));
    GBUFFER_DECREF(gbuf)

    return msg_iev_build_response(gobj,
        0,
        jn_comment,
        0,
        rows,
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  The id of a node of the projection: the id of its parent, a dot, and
 *  its own name.
 *
 *      treedbs   treedb_yunovatioscodb
 *      topics    treedb_yunovatioscodb.yunos
 *      cols      treedb_yunovatioscodb.yunos.yuno_role
 *
 *  A topic name is unique only inside its treedb, and a column name only
 *  inside its topic: keyed by the bare name, the second treedb of a store
 *  declaring `users` collided with the first and lost its schema. The
 *  parent's id is what makes the name whole, and it carries its own.
 *
 *  Same rule tr_treedb applies to a pkey flagged `qualified`, written here
 *  too because the projector knows both halves and does not have to
 *  compose an fkey to say so.
 *
 *  A name that does not FIT is refused, never trimmed: the id is the address
 *  of the node, so a truncated one silently addresses something else — and
 *  two long names sharing a prefix would land on the same record.
 *
 *  Return `bf`, or NULL when it does not fit.
 ***************************************************************************/
PRIVATE const char *build_schema_node_id(
    hgobj gobj,
    char *bf,
    int bfsize,
    const char *parent_id,
    const char *name
)
{
    int written = snprintf(bf, bfsize, "%s.%s", parent_id, name);
    if(written < 0 || written >= bfsize) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB,
            "msg",          "%s", "Qualified 'id' does not fit in a record key",
            "parent_id",    "%s", parent_id,
            "name",         "%s", name,
            "max",          "%d", bfsize - 1,
            NULL
        );
        *bf = 0;
        return NULL;
    }

    return bf;
}

/***************************************************************************
 *  Find a node of a projection by the name it holds in `value`.
 *
 *  The projector addresses a node by its qualified id, which it composes;
 *  `diff-schema` cannot, because it also reads projections made before the
 *  key was qualified — and what it compares is what a name resolves to,
 *  under whichever convention wrote it.
 *
 *  Return the node, or NULL. Return is NOT yours.
 ***************************************************************************/
PRIVATE json_t *find_node_by_name(
    hgobj gobj,
    json_t *siblings,       // a `topics` or `cols` hook expanded, NOT owned
    const char *name
)
{
    const char *node_id; json_t *node;
    json_object_foreach(siblings, node_id, node) {
        if(strcmp(kw_get_str(gobj, node, "value", "", 0), name)==0) {
            return node;
        }
    }

    return NULL;
}

/***************************************************************************
 *  Rewrite one node of the projection under a new id, content and all.
 *
 *  An operator's addition lives in these topics too and it is not the
 *  projector's to drop, so the move copies what is stored instead of
 *  re-deriving it: only the identity changes.
 *
 *  What it drops is asked of the DESCRIPTOR, never written down here: the
 *  links are rebuilt by the link that follows, and a hook or fkey added to
 *  the meta-schema later would otherwise travel as a stale reference.
 *
 *  Return the new node, or NULL. Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *move_schema_node(
    hgobj gobj,
    const char *topic_name, // `topics` or `cols`
    json_t *stored,         // not owned, the node as the tree holds it
    const char *new_id,
    const char *hook,       // hook of the parent it hangs from
    const char *parent_topic_name,
    json_t *parent          // not owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_node = json_deep_copy(stored);
    json_object_del(kw_node, "__md_treedb__");

    json_t *desc = gobj_topic_desc(priv->gobj_node_system, topic_name);
    int idx; json_t *col;
    json_array_foreach(kw_get_list(gobj, desc, "cols", 0, 0), idx, col) {
        json_t *flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        if(kw_has_word(gobj, flag, "hook", 0) || kw_has_word(gobj, flag, "fkey", 0)) {
            json_object_del(kw_node, kw_get_str(gobj, col, "id", "", 0));
        }
    }
    JSON_DECREF(desc)

    json_object_set_new(kw_node, "id", json_string(new_id));

    json_t *node = gobj_create_node(
        priv->gobj_node_system,
        topic_name,
        kw_node,
        json_pack("{s:b}", "refs", 1),      // fkey,hook options
        gobj
    );
    if(!node) {
        return NULL;    // Error already logged
    }

    gobj_link_nodes(
        priv->gobj_node_system,
        hook,
        parent_topic_name,
        json_incref(parent),    // parent_record, owned
        topic_name,
        json_incref(node),      // child_record, owned
        gobj
    );

    return node;
}

/***************************************************************************
 *  Move a projection made with rowid keys to qualified ones.
 *
 *  `topics` and `cols` used to be keyed by a rowid handed out from the
 *  topic size, with the name in `value`. They are keyed by the qualified
 *  name now, and the two conventions cannot live side by side: the schema
 *  is rebuilt by `value`, so a legacy node and its qualified twin are the
 *  same topic twice and which of them wins is iteration order.
 *
 *  A column moves before its topic: deleting a parent only UNLINKS its
 *  children, so a column left behind would end up orphaned under an id
 *  nothing points at any more.
 *
 *  This runs when the store was written with an older meta-schema
 *  (`system_schema_version`), before the projection is read. It moves ids
 *  and re-projects nothing.
 *
 *  Return the number of nodes moved, or -1.
 ***************************************************************************/
PRIVATE int migrate_schema_ids_to_qualified(
    hgobj gobj,
    const char *treedb_name,
    json_t *current     // not owned, the projection already stored
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *current_topics = kw_get_dict(gobj, current, "topics", 0, 0);
    if(!current_topics) {
        return 0;
    }

    /*
     *  The loop writes to the very hook it walks, so take the keys first.
     */
    json_t *legacy_topic_ids = json_array();
    const char *stored_topic_id; json_t *stored_topic;
    json_object_foreach(current_topics, stored_topic_id, stored_topic) {
        const char *topic_name = kw_get_str(gobj, stored_topic, "value", "", 0);
        if(empty_string(topic_name)) {
            continue;
        }
        char topic_id[RECORD_KEY_VALUE_MAX];
        if(!build_schema_node_id(gobj, topic_id, sizeof(topic_id), treedb_name, topic_name)) {
            continue;   // Error already logged
        }
        if(strcmp(stored_topic_id, topic_id)!=0) {
            json_array_append_new(legacy_topic_ids, json_string(stored_topic_id));
        }
    }
    if(json_array_size(legacy_topic_ids) == 0) {
        JSON_DECREF(legacy_topic_ids)
        return 0;
    }

    json_t *treedb = gobj_get_node(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        0,
        gobj
    );
    if(!treedb) {
        JSON_DECREF(legacy_topic_ids)
        return -1;  // Error already logged
    }

    int moved = 0;
    int idx; json_t *jn_legacy_topic_id;
    json_array_foreach(legacy_topic_ids, idx, jn_legacy_topic_id) {
        const char *legacy_topic_id = json_string_value(jn_legacy_topic_id);
        json_t *legacy_topic = json_object_get(current_topics, legacy_topic_id);
        const char *topic_name = kw_get_str(gobj, legacy_topic, "value", "", 0);

        char topic_id[RECORD_KEY_VALUE_MAX];
        if(!build_schema_node_id(gobj, topic_id, sizeof(topic_id), treedb_name, topic_name)) {
            continue;   // Error already logged
        }

        json_t *topic = move_schema_node(
            gobj, "topics", legacy_topic, topic_id, "topics", "treedbs", treedb
        );
        if(!topic) {
            continue;   // Error already logged
        }
        moved++;

        /*
         *  Same reason as above: the delete below takes columns out of the
         *  very hook this walks, so hold them before touching any.
         */
        json_t *legacy_cols = json_array();
        const char *legacy_col_id; json_t *legacy_col;
        json_object_foreach(kw_get_dict(gobj, legacy_topic, "cols", 0, 0),
            legacy_col_id, legacy_col
        ) {
            json_array_append(legacy_cols, legacy_col);
        }

        int idx2; json_t *stored_col;
        json_array_foreach(legacy_cols, idx2, stored_col) {
            const char *col_name = kw_get_str(gobj, stored_col, "value", "", 0);
            if(empty_string(col_name)) {
                continue;
            }
            char col_id[RECORD_KEY_VALUE_MAX];
            if(!build_schema_node_id(gobj, col_id, sizeof(col_id), topic_id, col_name)) {
                continue;   // Error already logged
            }

            json_t *col = move_schema_node(
                gobj, "cols", stored_col, col_id, "cols", "topics", topic
            );
            if(!col) {
                continue;   // Error already logged
            }
            JSON_DECREF(col)
            moved++;

            gobj_delete_node(
                priv->gobj_node_system,
                "cols",
                json_pack("{s:s}", "id", kw_get_str(gobj, stored_col, "id", "", 0)),
                json_pack("{s:b}", "force", 1),
                gobj
            );
        }
        JSON_DECREF(legacy_cols)

        gobj_delete_node(
            priv->gobj_node_system,
            "topics",
            json_pack("{s:s}", "id", legacy_topic_id),
            json_pack("{s:b}", "force", 1),
            gobj
        );

        JSON_DECREF(topic)
    }

    JSON_DECREF(treedb)
    JSON_DECREF(legacy_topic_ids)

    gobj_log_info(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_INFO,
        "msg",              "%s", "TreeDB schema ids moved to qualified names",
        "treedb_name",      "%s", treedb_name,
        "moved",            "%d", moved,
        NULL
    );

    return moved;
}

/***************************************************************************
 *  Build what the __system__ projection stores for one TOPIC.
 *
 *  Shared by the projector and by `diff-schema`: comparing a schema with its
 *  projection means projecting it the same way first, or every default the
 *  projector fills in reads as a difference nobody made.
 *
 *  `topic_version` comes from the caller: the projector does not always write
 *  the number the schema carries (see upsert_treedb_schema).
 *
 *  Return the node kw, or NULL. Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *build_topic_projection(
    hgobj gobj,
    json_t *jn_topic,   // not owned
    const char *topic_name,
    json_int_t topic_version,
    int order
)
{
    const char *pkey = kw_get_str(gobj, jn_topic, "pkey", "id", 0);
    const char *tkey = kw_get_str(gobj, jn_topic, "tkey", "", 0);
    const char *system_flag = kw_get_str(gobj, jn_topic, "system_flag", "sf_string_key", 0);
    json_t *topic_pkey2s_ = kw_get_dict_value(gobj, jn_topic, "pkey2s", 0, 0);
    BOOL system_topic = kw_get_bool(gobj, jn_topic, "system_topic", 0, 0);
    BOOL main_topic = kw_get_bool(gobj, jn_topic, "main_topic", 0, 0);

    json_t *kw_topic = json_pack("{s:s, s:s, s:s, s:s, s:I, s:b, s:b}",
        "value", topic_name,
        "pkey", pkey,
        "system_flag", system_flag,
        "tkey", tkey,
        "topic_version", (json_int_t )topic_version,
        "system_topic", system_topic,
        "main_topic", main_topic
    );
    if(!kw_topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON,
            "msg",          "%s", "json_pack() FAILED",
            "topic_name",   "%s", topic_name,
            NULL
        );
        return NULL;
    }
    if(topic_pkey2s_) {
        json_object_set(kw_topic, "pkey2s", topic_pkey2s_);
    }

    /*
     *  Where the topic sits in the schema. See build_col_projection().
     */
    json_object_set_new(kw_topic, "order", json_integer(order));

    return kw_topic;
}

/***************************************************************************
 *  Build what the __system__ projection stores for one COLUMN.
 *
 *  Copy every attribute the DESCRIPTOR declares, never a list written by
 *  hand here: that list is why `enum`, `template` and `pkey2s` never reached
 *  the projection. An attribute added to the descriptor did not add itself
 *  here, and the loss showed up only later, as a schema that had quietly
 *  changed — a column keeping its `enum` flag while its enumeration was
 *  gone.
 *
 *  The column's name travels in `value`; the qualified `id` is the
 *  caller's business, it is what addresses the column.
 *
 *  `order` is the position the column occupies in the schema, and it is
 *  copied from nowhere: in a schema the order IS the sequence of the `cols`
 *  dict, so the caller's index is the only place it exists. Stored, it is
 *  what lets the schema be rebuilt in the order it was written, whatever
 *  order the projection is read back in.
 *
 *  Return the node kw, or NULL. Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *build_col_projection(
    hgobj gobj,
    json_t *jn_col,     // not owned
    json_t *cols_desc,  // not owned
    int order
)
{
    const char *col_name = kw_get_str(gobj, jn_col, "id", "", KW_REQUIRED);
    if(empty_string(col_name)) {
        return NULL;    // Error already logged
    }
    const char *header = kw_get_str(gobj, jn_col, "header", col_name, 0);
    const char *type = kw_get_str(gobj, jn_col, "type", "", KW_REQUIRED);
    if(empty_string(type)) {
        return NULL;    // Error already logged
    }

    json_t *kw_col = json_object();
    json_object_set_new(kw_col, "value", json_string(col_name));

    int idx; json_t *desc_entry;
    json_array_foreach(cols_desc, idx, desc_entry) {
        const char *attr = kw_get_str(gobj, desc_entry, "id", "", 0);
        if(empty_string(attr) || strcmp(attr, "id")==0) {
            continue;   /*  the column's name, already in `value`  */
        }
        json_t *v = json_object_get(jn_col, attr);
        if(v) {
            json_object_set(kw_col, attr, v);
        }
    }

    if(!json_object_get(kw_col, "header")) {
        json_object_set_new(kw_col, "header", json_string(header));
    }

    json_object_set_new(kw_col, "order", json_integer(order));

    return kw_col;
}

/***************************************************************************
 *  Whether writing a projected node over the stored one would change it,
 *  when the projection is WHOLE: the stored node must end up saying what
 *  the projection says and nothing else.
 *
 *  The comparison of diff-schema, read from the side of the write. An
 *  update merges, so an attribute the stored node holds and the projection
 *  does not declare -- an operator's addition -- would stay: it is written
 *  back empty instead (its declared default, or the empty value of its
 *  type), which is how the store says "not declared". `projected` is
 *  MUTATED to carry those.
 ***************************************************************************/
PRIVATE BOOL projection_rewrites_node(
    hgobj gobj,
    json_t *projected,  // not owned, MUTATED
    json_t *stored,     // not owned
    const char **skip,  // attributes that say how a node is STORED
    json_t *desc        // not owned, descriptor of the node's topic, may be NULL
)
{
    BOOL changes = FALSE;

    json_t *rows = json_array();
    diff_node_attrs(gobj, rows, "", "", NULL, projected, stored, skip, desc);

    int idx; json_t *row;
    json_array_foreach(rows, idx, row) {
        const char *kind = kw_get_str(gobj, row, "kind", "", 0);
        if(strcmp(kind, "changed")==0 || strcmp(kind, "only_in_c")==0) {
            changes = TRUE;
            continue;
        }
        if(strcmp(kind, "only_in_stored")!=0) {
            continue;
        }
        changes = TRUE;

        const char *attr = kw_get_str(gobj, row, "attr", "", 0);
        json_t *stored_value = json_object_get(row, "stored");
        json_t *cleared = NULL;
        int idx2; json_t *entry;
        json_array_foreach(desc, idx2, entry) {
            if(strcmp(kw_get_str(gobj, entry, "id", "", 0), attr)==0 &&
                    json_object_get(entry, "default")) {
                cleared = json_deep_copy(json_object_get(entry, "default"));
            }
        }
        if(!cleared) {
            if(json_is_string(stored_value)) {
                cleared = json_string("");
            } else if(json_is_object(stored_value)) {
                cleared = json_object();
            } else if(json_is_array(stored_value)) {
                cleared = json_array();
            } else if(json_is_boolean(stored_value)) {
                cleared = json_false();
            } else if(json_is_real(stored_value)) {
                cleared = json_real(0);
            } else {
                cleared = json_integer(0);
            }
        }
        json_object_set_new(projected, attr, cleared);
    }
    JSON_DECREF(rows)

    return changes;
}

/***************************************************************************
 *  What the operator had in a topic of __system__ that a projection
 *  replaces, or NULL when it was nobody's work: a DRAFT is a topic that
 *  differs from the schema file in use (`drafts`). It is "saved" when a
 *  pending save published it, "unsaved" otherwise:
 *
 *      - a topic still in __system__ was saved when the pending saved
 *        schema declares it and its version there is above the one in use
 *        (save-schema raises it). A topic the saved schema does not
 *        declare is in __system__ against that save (added after it, or
 *        deleted by it and made again): it is unsaved. Its version says
 *        nothing there: a topic new to the file is above its version 0;
 *      - a topic the draft DELETED from __system__ was saved when the
 *        pending saved schema does not declare it either;
 *      - a draft that an earlier open could not replace keeps the kind it
 *        had then (`kinds_before`, the record's `draft_kinds`): that open
 *        withdrew the saved schema, so nothing else says it now.
 *
 *  A save taken back is the file again: no draft. (An applied topic that
 *  never ran is judged against the file, in reconcile_treedb_schema().)
 ***************************************************************************/
PRIVATE const char *draft_kind(
    hgobj gobj,
    json_t *drafts,             // not owned, {topic: true}, may be NULL
    json_t *saved,              // not owned, the pending saved schema, or NULL
    json_t *kinds_before,       // not owned, {topic: kind} of an unfinished projection, or NULL
    const char *topic_name,
    json_t *stored_topic,       // not owned, the topic in __system__, NULL when the draft deleted it
    json_int_t in_use_topic_version
)
{
    if(!json_object_get(drafts, topic_name)) {
        return NULL;
    }
    const char *kind_before = json_string_value(json_object_get(kinds_before, topic_name));
    if(kind_before && strcmp(kind_before, "saved")==0) {
        return "saved";
    }
    if(!saved) {
        return "unsaved";
    }
    if(!stored_topic) {
        return schema_topic(saved, topic_name)? "unsaved" : "saved";
    }
    if(!schema_topic(saved, topic_name)) {
        return "unsaved";
    }
    json_int_t stored_topic_version = kw_get_int(
        gobj, stored_topic, "topic_version", 0, KW_WILD_NUMBER
    );
    return (stored_topic_version > in_use_topic_version)? "saved" : "unsaved";
}

/***************************************************************************
 *  What a projection could not do, as the record of an unfinished
 *  projection keeps it (see reconcile_treedb_schema):
 *
 *      {"schema_version": 2,                   // of the schema projected
 *       "not_removed": ["tw.departments"],     // deletes refused
 *       "not_written": [],                     // writes that failed
 *       "leftovers": ["tw.departments", "tw.departments.id",
 *                     "tw.departments.name"],
 *       "draft_kinds": {"users": "saved"},     // drafts it could not replace
 *       "leftover_nodes": {"tw.departments": {...}, ...},
 *       "system_schema_version": 18}           // meta-schema of those nodes
 *
 *  `leftovers` is every id of __system__ the projection left unlike the
 *  schema: the ids of the two lists, and the columns of a topic that it
 *  could not remove or write. They are nobody's draft while they stay as
 *  the projection left them: `leftover_nodes` keeps what was there, the
 *  attributes a projection writes (added when the record is written, with
 *  the meta-schema version, see keep_leftover_nodes), and an EDIT of one
 *  of those is the operator's work (see leftovers_as_left). An id that
 *  carries an operator's draft is NOT a leftover, although it is in a
 *  list: the projection could not replace the draft, so it is still one
 *  (see upsert_treedb_schema). `draft_kinds` keeps what kind of draft each of
 *  those topics was ("saved" or "unsaved", see draft_kind), for the open
 *  that replaces it to say. Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *new_unfinished(json_int_t schema_version)
{
    return json_pack("{s:I, s:[], s:[], s:[], s:{}}",
        "schema_version", schema_version,
        "not_removed",
        "not_written",
        "leftovers",
        "draft_kinds"
    );
}

/***************************************************************************
 *  A draft of `topic_name` that the projection could not replace: its
 *  kind goes into the record, for the open that replaces it to say
 ***************************************************************************/
PRIVATE void keep_draft_kind(json_t *unfinished, const char *topic_name, const char *kind)
{
    if(!kind) {
        return;
    }
    json_object_set_new(json_object_get(unfinished, "draft_kinds"), topic_name, json_string(kind));
}

/***************************************************************************
 *  Add an id of __system__ to what a projection could not do: to `list`
 *  ("not_removed" or "not_written", NULL for none), and to the leftovers
 *  unless it carries an operator's draft (`draft_ids`). TRUE when it does:
 *  the draft is still there.
 ***************************************************************************/
PRIVATE BOOL add_unfinished(
    json_t *unfinished,
    const char *list,
    const char *id,
    json_t *draft_ids   // not owned, {id: true}, may be NULL
)
{
    if(list) {
        json_array_append_new(json_object_get(unfinished, list), json_string(id));
    }
    if(json_object_get(draft_ids, id)) {
        return TRUE;
    }
    json_array_append_new(json_object_get(unfinished, "leftovers"), json_string(id));
    return FALSE;
}

/***************************************************************************
 *  A topic could not be written: it, and the columns it would have
 *  written or removed with it, are leftovers -- except what carries a
 *  draft (see add_unfinished). Nothing of the topic was replaced, so its
 *  draft is not said now.
 ***************************************************************************/
PRIVATE void add_unfinished_topic(
    hgobj gobj,
    json_t *unfinished,
    const char *topic_id,
    json_t *kw_cols,        // not owned, [col projection]
    json_t *removed_cols,   // not owned, [col id]
    json_t *draft_ids       // not owned, {id: true}, may be NULL
)
{
    add_unfinished(unfinished, "not_written", topic_id, draft_ids);
    int idx; json_t *jn;
    json_array_foreach(kw_cols, idx, jn) {
        add_unfinished(unfinished, NULL, kw_get_str(gobj, jn, "id", "", 0), draft_ids);
    }
    json_array_foreach(removed_cols, idx, jn) {
        add_unfinished(unfinished, NULL, json_string_value(jn), draft_ids);
    }
}

/***************************************************************************
 *  Project a schema into the __system__ treedb, WHOLE: afterwards the
 *  projection of the treedb says what `kw` says, and nothing else.
 *
 *      - a topic or column that is new is created;
 *      - one that differs is updated, attributes it no longer declares
 *        included (projection_rewrites_node), and so is a topic whose
 *        topic_version moved;
 *      - a topic or column `kw` does not declare is DELETED, with force
 *        (it is linked), its columns with it;
 *      - a topic or column the tree of the treedb no longer reaches (an
 *        unlink, see orphan_nodes) is TAKEN when `kw` declares it --
 *        written as `kw` says and linked again -- and deleted when it
 *        does not. Created anew, it failed at every open.
 *
 *  That is the rule the user decided on 2026-09-23 for a literal that wins
 *  (see reconcile_treedb_schema): it replaces the schema file whole, and
 *  __system__ is projected from it whole. The upsert of 7.25.4 never
 *  deleted: it kept a topic the developer had removed, and the next
 *  save-schema published it again. A delete drops the history of the
 *  node, and it is refused on a node a snapshot holds (logged by the
 *  delete). The projection is then NOT complete, and it is not passed off
 *  as one: what it could not remove (or write: a create, update or link
 *  that failed, logged) goes into `unfinished` (see new_unfinished), it
 *  says so in a WARNING (what, why, how to finish), it answers -1, and it
 *  leaves the numbers of the treedb node as they were. The caller records
 *  `unfinished`, and the next open retries the projection. The numbers are written LAST, on full success
 *  (7.25.4 wrote them first): a projection that did not finish must not
 *  look like one that did.
 *
 *  What that replaces of the operator's work -- a draft of a topic, saved
 *  or not (`drafts`, `saved`, see draft_kind), a topic the draft deleted
 *  included -- is added to `replaced` as {topic: "saved" | "unsaved"}; the
 *  caller says it. A draft is reported by the open that REPLACES it, once:
 *  a projection that cannot replace a part of it (a write that fails, a
 *  delete a snapshot refuses) leaves it a draft -- not a leftover,
 *  `draft_ids` says which ids carry one -- says nothing of that topic, and
 *  keeps its kind in `unfinished` (`draft_kinds`); the open that completes
 *  it says it. Taken for a leftover, a draft would be deleted in silence
 *  by that open: the report does not depend on whether a write succeeded,
 *  only on whether the draft is still in __system__.
 *
 *  Only what a write would change is written: an identical topic or column
 *  adds no record. The number of __system__'s `schema_version` never goes
 *  down (a save may have raised it past the literal), and
 *  `c_schema_version` records which schema the projection came from: the
 *  version of the literal, or 0 when what is projected is not the literal
 *  (a seed from a dynamic file, or a projection left unfinished).
 ***************************************************************************/
PRIVATE int upsert_treedb_schema(
    hgobj gobj,
    const char *treedb_name,
    json_t *kw,     // not owned, the schema to project
    json_int_t c_schema_version, // the literal's version, 0 when `kw` is not the literal
    json_t *current,// not owned, the projection already stored, or NULL
    json_t *file_in_use, // not owned, the schema file in use before this open, or NULL
    json_t *drafts, // not owned, {topic: true} whose draft differs from the file, or NULL
    json_t *draft_ids, // not owned, {id: true} of __system__ that carry those drafts, or NULL
    json_t *saved,  // not owned, the saved schema newer than the file in use, or NULL
    json_t *kinds_before, // not owned, {topic: kind} of the drafts an earlier open kept, or NULL
    json_t *replaced, // not owned, {topic: kind} the projection replaced is added here, or NULL
    json_t *unfinished  // not owned, what the projection could not do is added (new_unfinished)
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t kw_version = kw_get_int(gobj, kw, "schema_version", 1, KW_WILD_NUMBER);
    json_object_set_new(unfinished, "schema_version", json_integer(kw_version));
    json_int_t schema_version = kw_version;
    if(current) {
        json_int_t stored_version = kw_get_int(gobj, current, "schema_version", 0, KW_WILD_NUMBER);
        if(stored_version > schema_version) {
            schema_version = stored_version;
        }
    }

    int failed = 0;

    /*
     *  The node of the treedb is only READ, or created WITHOUT its numbers
     *  (0): they are written at the end, as for one that exists. Created
     *  with them, it would say "projection of this literal" before a topic
     *  is written, and if the process died here the next open would take a
     *  projection with no topics as done.
     */
    json_t *treedb;
    if(current) {
        treedb = gobj_get_node(
            priv->gobj_node_system,
            "treedbs",
            json_pack("{s:s}", "id", treedb_name),
            json_pack("{s:b}", "refs", 1),      // fkey,hook options
            gobj
        );
    } else {
        treedb = gobj_create_node(
            priv->gobj_node_system,
            "treedbs",
            json_pack("{s:s, s:I, s:I, s:I}",
                "id", treedb_name,
                "schema_version", (json_int_t )0,
                "c_schema_version", (json_int_t )0,
                "system_schema_version", (json_int_t )priv->system_schema_version
            ),
            json_pack("{s:b}", "refs", 1),      // fkey,hook options
            gobj
        );
    }
    if(!treedb) {
        add_unfinished(unfinished, "not_written", treedb_name, NULL);
        return -1;  // Error already logged
    }

    json_t *current_topics = current? kw_get_dict(gobj, current, "topics", 0, 0): NULL;

    /*
     *  The nodes of the treedb its tree does not reach (orphan_nodes): one
     *  the schema declares is TAKEN, written as the schema says and linked
     *  again, and the rest go below, as anything else the schema does not
     *  declare. Created anew, the node failed at every open ("Node already
     *  exists"). One that is the operator's work (`draft_ids`) is said, as
     *  "unsaved": no save carries a node that is in no topic.
     */
    json_t *orphans = orphan_nodes(gobj, treedb_name, current);

    /*
     *  The version each topic is IN USE at, to tell a saved draft from an
     *  unsaved one (see draft_kind)
     */
    json_t *in_use = topic_versions_in_use(gobj, treedb_name, file_in_use, kw);

    /*
     *  What a column may declare, read once for the whole projection
     */
    json_t *cols_desc = _treedb_create_topic_cols_desc();

    json_t *declared_topics = json_object();

    json_t *jn_topics = kw_get_list(gobj, kw, "topics", 0, 0);
    int idx; json_t *jn_topic;
    json_array_foreach(jn_topics, idx, jn_topic) {
        const char *topic_name = kw_get_str(gobj, jn_topic, "id", "", 0);
        if(empty_string(topic_name)) {
            topic_name = kw_get_str(gobj, jn_topic, "topic_name", "", KW_REQUIRED);
        }
        if(empty_string(topic_name)) {
            continue;
        }
        json_int_t topic_version = kw_get_int(gobj, jn_topic, "topic_version", 1, KW_WILD_NUMBER);

        char topic_id[RECORD_KEY_VALUE_MAX];
        if(!build_schema_node_id(gobj, topic_id, sizeof(topic_id), treedb_name, topic_name)) {
            continue;   // Error already logged
        }
        json_object_set_new(declared_topics, topic_id, json_true());

        json_t *current_topic = current_topics?
            json_object_get(current_topics, topic_id):
            NULL;
        json_t *current_cols = current_topic?
            kw_get_dict(gobj, current_topic, "cols", 0, 0):
            NULL;

        json_t *kw_topic = build_topic_projection(
            gobj, jn_topic, topic_name, topic_version, idx
        );
        if(!kw_topic) {
            continue;   // Error already logged
        }
        json_object_set_new(kw_topic, "id", json_string(topic_id));

        json_t *jn_cols = kwid_new_list(gobj, jn_topic, 0, "cols");
        if(!jn_cols) {
            json_decref(kw_topic);
            continue;
        }

        /*
         *  Only what a write would change is written: the columns that are
         *  new or differ, and their topic. A column the topic no longer
         *  declares goes.
         */
        json_t *kw_cols = json_array();
        json_t *declared_cols = json_object();
        int idx2; json_t *jn_col;
        json_array_foreach(jn_cols, idx2, jn_col) {
            json_t *kw_col = build_col_projection(gobj, jn_col, cols_desc, idx2);
            if(!kw_col) {
                continue;   // Error already logged
            }
            const char *col_name = json_string_value(json_object_get(kw_col, "value"));

            char col_id[RECORD_KEY_VALUE_MAX];
            if(!build_schema_node_id(gobj, col_id, sizeof(col_id), topic_id, col_name)) {
                JSON_DECREF(kw_col)
                continue;   // Error already logged
            }
            json_object_set_new(kw_col, "id", json_string(col_id));
            json_object_set_new(declared_cols, col_id, json_true());

            json_t *current_col = current_cols? json_object_get(current_cols, col_id): NULL;
            if(current_col &&
                !projection_rewrites_node(gobj, kw_col, current_col, schema_col_skip, cols_desc)
            ) {
                JSON_DECREF(kw_col)
                continue;
            }
            json_t *orphan = current_col? NULL : json_object_get(orphans, col_id);
            if(orphan && !json_is_true(json_object_get(orphan, "is_topic"))) {
                projection_rewrites_node(
                    gobj, kw_col, json_object_get(orphan, "node"), schema_col_skip, cols_desc
                );
            }
            json_array_append_new(kw_cols, kw_col);
        }
        json_decref(jn_cols);

        json_t *removed_cols = json_array();
        const char *current_col_id; json_t *current_col;
        json_object_foreach(current_cols, current_col_id, current_col) {
            if(!json_object_get(declared_cols, current_col_id)) {
                json_array_append_new(removed_cols, json_string(current_col_id));
            }
        }
        JSON_DECREF(declared_cols)

        /*
         *  The operator's draft this replaces (see draft_kind). A topic the
         *  draft DELETED is re-created, and that is its draft too.
         */
        const char *kind = NULL;
        if(current_topic) {
            json_int_t stored_topic_version = kw_get_int(
                gobj, current_topic, "topic_version", 0, KW_WILD_NUMBER
            );
            /*
             *  The rewrite is asked FIRST: it is what clears in `kw_topic`
             *  an attribute the topic has and the schema does not declare,
             *  and asked after the columns it was skipped whenever they
             *  changed
             */
            BOOL topic_rewrites = projection_rewrites_node(
                gobj, kw_topic, current_topic, schema_topic_skip, NULL
            );
            BOOL topic_changes = (json_array_size(kw_cols) > 0 ||
                json_array_size(removed_cols) > 0 ||
                topic_rewrites
            )? TRUE: FALSE;

            if(!topic_changes && stored_topic_version == topic_version) {
                JSON_DECREF(removed_cols)
                JSON_DECREF(kw_cols)
                json_decref(kw_topic);
                continue;   /*  __system__ says it already  */
            }

            kind = topic_changes? draft_kind(
                gobj, drafts, saved, kinds_before, topic_name, current_topic,
                topic_version_in_use(in_use, topic_name)
            ) : NULL;
        } else {
            kind = draft_kind(
                gobj, drafts, saved, kinds_before, topic_name, NULL,
                topic_version_in_use(in_use, topic_name)
            );
        }

        /*
         *  A part of the draft that is not replaced keeps it a draft: the
         *  topic is said by the open that replaces it
         */
        BOOL draft_left = FALSE;
        BOOL orphan_work = FALSE;   /*  an orphan of the operator taken over  */

        json_t *topic;
        if(current_topic) {
            topic = gobj_update_node(
                priv->gobj_node_system,
                "topics",
                kw_topic,
                json_pack("{s:b}", "refs", 1),      // fkey,hook options
                gobj
            );
            if(!topic) {
                failed++;   // Error already logged
                add_unfinished_topic(gobj, unfinished, topic_id, kw_cols, removed_cols, draft_ids);
                keep_draft_kind(unfinished, topic_name, kind);
                JSON_DECREF(removed_cols)
                JSON_DECREF(kw_cols)
                continue;   /*  nothing of it replaced: not said  */
            }
        } else {
            json_t *orphan = json_object_get(orphans, topic_id);
            if(orphan && json_is_true(json_object_get(orphan, "is_topic"))) {
                projection_rewrites_node(
                    gobj, kw_topic, json_object_get(orphan, "node"), schema_topic_skip, NULL
                );
                topic = gobj_update_node(
                    priv->gobj_node_system,
                    "topics",
                    kw_topic,
                    json_pack("{s:b}", "refs", 1),      // fkey,hook options
                    gobj
                );
                if(topic) {
                    if(json_object_get(draft_ids, topic_id)) {
                        orphan_work = TRUE;
                    }
                    json_object_del(orphans, topic_id);
                }
            } else {
                topic = gobj_create_node(
                    priv->gobj_node_system,
                    "topics",
                    kw_topic,
                    json_pack("{s:b}", "refs", 1),      // fkey,hook options
                    gobj
                );
            }
            if(!topic) {
                failed++;   // Error already logged
                add_unfinished_topic(gobj, unfinished, topic_id, kw_cols, removed_cols, draft_ids);
                keep_draft_kind(unfinished, topic_name, kind);
                JSON_DECREF(removed_cols)
                JSON_DECREF(kw_cols)
                continue;   /*  nothing of it replaced: not said  */
            }

            if(gobj_link_nodes(
                    priv->gobj_node_system,
                    "topics",               // hook
                    "treedbs",              // parent_topic_name,
                    json_incref(treedb),    // parent_record,owned
                    "topics",               // child_topic_name,
                    json_incref(topic),     // child_record,owned
                    gobj
                ) < 0) {
                failed++;   // Error already logged
                if(add_unfinished(unfinished, "not_written", topic_id, draft_ids)) {
                    draft_left = TRUE;
                }
            }
        }

        json_t *kw_col;
        json_array_foreach(kw_cols, idx2, kw_col) {
            const char *col_id = kw_get_str(gobj, kw_col, "id", "", 0);
            json_t *stored_col = current_cols? json_object_get(current_cols, col_id): NULL;

            json_t *col;
            json_t *orphan = stored_col? NULL : json_object_get(orphans, col_id);
            if(orphan && json_is_true(json_object_get(orphan, "is_topic"))) {
                orphan = NULL;
            }
            if(orphan) {
                /*
                 *  A column node no topic of the tree holds: taken, and
                 *  linked to its topic unless it still is (the columns of
                 *  a topic taken above)
                 */
                char ref[RECORD_KEY_VALUE_MAX + 16];
                snprintf(ref, sizeof(ref), "topics^%s^cols", topic_id);
                json_t *refs = json_object_get(json_object_get(orphan, "node"), "topics");
                BOOL linked = (json_is_array(refs) && json_list_str_index(refs, ref, FALSE) >= 0)?
                    TRUE : FALSE;
                BOOL operator_work = json_object_get(draft_ids, col_id)? TRUE : FALSE;
                json_object_del(orphans, col_id);   /*  `orphan` is not used below  */

                col = gobj_update_node(
                    priv->gobj_node_system,
                    "cols",
                    json_incref(kw_col),
                    json_pack("{s:b}", "refs", 1),  // fkey,hook options
                    gobj
                );
                if(!col) {
                    failed++;   // Error already logged
                    if(add_unfinished(unfinished, "not_written", col_id, draft_ids)) {
                        draft_left = TRUE;
                    }
                    continue;
                }
                if(!linked && gobj_link_nodes(
                        priv->gobj_node_system,
                        "cols",                 // hook
                        "topics",               // parent_topic_name,
                        json_incref(topic),     // parent_record,owned
                        "cols",                 // child_topic_name,
                        json_incref(col),       // child_record,owned
                        gobj
                    ) < 0) {
                    failed++;   // Error already logged
                    if(add_unfinished(unfinished, "not_written", col_id, draft_ids)) {
                        draft_left = TRUE;
                    }
                } else if(operator_work) {
                    orphan_work = TRUE;
                }
            } else if(stored_col) {
                col = gobj_update_node(
                    priv->gobj_node_system,
                    "cols",
                    json_incref(kw_col),
                    json_pack("{s:b}", "refs", 1),  // fkey,hook options
                    gobj
                );
                if(!col) {
                    failed++;   // Error already logged
                    if(add_unfinished(unfinished, "not_written", col_id, draft_ids)) {
                        draft_left = TRUE;
                    }
                    continue;
                }
            } else {
                col = gobj_create_node(
                    priv->gobj_node_system,
                    "cols",
                    json_incref(kw_col),
                    json_pack("{s:b}", "refs", 1),  // fkey,hook options
                    gobj
                );
                if(!col) {
                    failed++;   // Error already logged
                    if(add_unfinished(unfinished, "not_written", col_id, draft_ids)) {
                        draft_left = TRUE;
                    }
                    continue;
                }

                if(gobj_link_nodes(
                        priv->gobj_node_system,
                        "cols",                 // hook
                        "topics",               // parent_topic_name,
                        json_incref(topic),     // parent_record,owned
                        "cols",                 // child_topic_name,
                        json_incref(col),       // child_record,owned
                        gobj
                    ) < 0) {
                    failed++;   // Error already logged
                    if(add_unfinished(unfinished, "not_written", col_id, draft_ids)) {
                        draft_left = TRUE;
                    }
                }
            }

            /*
             *  free
             */
            json_decref(col);
        }

        json_t *jn_col_id;
        json_array_foreach(removed_cols, idx2, jn_col_id) {
            if(gobj_delete_node(
                    priv->gobj_node_system,
                    "cols",
                    json_pack("{s:s}", "id", json_string_value(jn_col_id)),
                    json_pack("{s:b}", "force", 1),     // it is linked to its topic
                    gobj
                ) < 0) {
                failed++;   // Error already logged
                if(add_unfinished(unfinished, "not_removed", json_string_value(jn_col_id), draft_ids)) {
                    draft_left = TRUE;
                }
            }
        }

        if(draft_left) {
            keep_draft_kind(unfinished, topic_name, kind? kind : (orphan_work? "unsaved" : NULL));
        } else if(kind && replaced) {
            json_object_set_new(replaced, topic_name, json_string(kind));
        } else if(orphan_work && replaced) {
            json_object_set_new(replaced, topic_name, json_string("unsaved"));
        }

        /*
         *  free
         */
        JSON_DECREF(removed_cols)
        JSON_DECREF(kw_cols)
        json_decref(topic);
    }

    /*
     *  A topic `kw` does not declare goes, and its columns with it: the
     *  topic first (with force it unlinks them), then each column. A topic
     *  that cannot go keeps its columns: deleting them would leave half a
     *  topic, and what holds the topic holds them too.
     */
    const char *current_topic_id; json_t *current_topic;
    json_object_foreach(current_topics, current_topic_id, current_topic) {
        if(json_object_get(declared_topics, current_topic_id)) {
            continue;
        }
        const char *topic_name = kw_get_str(gobj, current_topic, "value", current_topic_id, 0);
        const char *kind = draft_kind(
            gobj, drafts, saved, kinds_before, topic_name, current_topic,
            topic_version_in_use(in_use, topic_name)
        );

        if(gobj_delete_node(
                priv->gobj_node_system,
                "topics",
                json_pack("{s:s}", "id", current_topic_id),
                json_pack("{s:b}", "force", 1),     // it is linked to its treedb and its cols
                gobj
            ) < 0) {
            failed++;   // Error already logged
            add_unfinished(unfinished, "not_removed", current_topic_id, draft_ids);
            const char *col_id; json_t *col;
            json_object_foreach(kw_get_dict(gobj, current_topic, "cols", 0, 0), col_id, col) {
                add_unfinished(unfinished, NULL, col_id, draft_ids);
            }
            keep_draft_kind(unfinished, topic_name, kind);
            continue;   /*  its draft, if any, is still there: not said  */
        }

        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "Topic not declared by the schema from C: removed from __system__",
            "treedb_name",      "%s", treedb_name,
            "topic_name",       "%s", topic_name,
            "schema_version",   "%d", (int)kw_version,
            NULL
        );
        if(kind && replaced) {
            json_object_set_new(replaced, topic_name, json_string(kind));
        }

        const char *col_id; json_t *col;
        json_object_foreach(kw_get_dict(gobj, current_topic, "cols", 0, 0), col_id, col) {
            if(gobj_delete_node(
                    priv->gobj_node_system,
                    "cols",
                    json_pack("{s:s}", "id", col_id),
                    json_pack("{s:b}", "force", 1),
                    gobj
                ) < 0) {
                failed++;   // Error already logged
                add_unfinished(unfinished, "not_removed", col_id, NULL);
            }
        }
    }

    /*
     *  The orphans nobody declared go, as anything the schema does not
     *  declare: the topics first (with force it unlinks their columns),
     *  then the columns. One that is the operator's work is said as a
     *  draft of its topic, "unsaved"; a delete refused keeps it a draft.
     */
    for(int pass = 0; pass < 2; pass++) {
        const char *orphan_id; json_t *orphan;
        json_object_foreach(orphans, orphan_id, orphan) {
            BOOL is_topic = json_is_true(json_object_get(orphan, "is_topic"));
            if(is_topic != (pass == 0)) {
                continue;
            }
            const char *topic_name = kw_get_str(gobj, orphan, "topic", "", 0);
            BOOL operator_work = json_object_get(draft_ids, orphan_id)? TRUE : FALSE;
            if(gobj_delete_node(
                    priv->gobj_node_system,
                    is_topic? "topics" : "cols",
                    json_pack("{s:s}", "id", orphan_id),
                    json_pack("{s:b}", "force", 1),
                    gobj
                ) < 0) {
                failed++;   // Error already logged
                if(add_unfinished(unfinished, "not_removed", orphan_id, draft_ids) &&
                        !json_object_get(json_object_get(unfinished, "draft_kinds"), topic_name)) {
                    keep_draft_kind(unfinished, topic_name, "unsaved");
                }
                continue;
            }

            gobj_log_info(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INFO,
                "msg",              "%s", "Node of the treedb that its tree does not reach: removed from __system__",
                "treedb_name",      "%s", treedb_name,
                "id",               "%s", orphan_id,
                "topic_name",       "%s", topic_name,
                "operator_work",    "%d", (int)operator_work,
                NULL
            );
            if(operator_work && replaced && !json_object_get(replaced, topic_name)) {
                json_object_set_new(replaced, topic_name, json_string("unsaved"));
            }
        }
    }
    JSON_DECREF(orphans)

    /*
     *  The numbers, LAST: only a whole projection says which schema it came
     *  from. One left in part says it came from none, c_schema_version 0,
     *  and keeps the schema_version it had (0 for a node created here): the
     *  next open finds it unfinished and completes it.
     */
    if(failed == 0) {
        json_t *stamped = gobj_update_node(
            priv->gobj_node_system,
            "treedbs",
            json_pack("{s:s, s:I, s:I, s:I}",
                "id", treedb_name,
                "schema_version", (json_int_t )schema_version,
                "c_schema_version", (json_int_t )c_schema_version,
                "system_schema_version", (json_int_t )priv->system_schema_version
            ),
            json_pack("{s:b}", "refs", 1),      // fkey,hook options
            gobj
        );
        if(!stamped) {
            failed++;   // Error already logged
            add_unfinished(unfinished, "not_written", treedb_name, NULL);
        }
        JSON_DECREF(stamped)
    } else if(current) {
        json_t *kw_reset = json_pack("{s:s, s:I, s:I}",
            "id", treedb_name,
            "c_schema_version", (json_int_t )0,
            "system_schema_version", (json_int_t )priv->system_schema_version
        );
        json_t *reset = gobj_update_node(   // Error already logged
            priv->gobj_node_system,
            "treedbs",
            kw_reset,
            json_pack("{s:b}", "refs", 1),      // fkey,hook options
            gobj
        );
        JSON_DECREF(reset)
    }

    if(failed > 0) {
        json_t *not_removed = json_object_get(unfinished, "not_removed");
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_TREEDB,
            "msg",              "%s", "Schema projected into __system__ only in part: its version is not recorded, and every open of the treedb retries it",
            "treedb_name",      "%s", treedb_name,
            "schema_version",   "%d", (int)kw_version,
            "failed",           "%d", failed,
            "not_removed",      "%j", not_removed,
            "not_written",      "%j", json_object_get(unfinished, "not_written"),
            "how",              "%s", json_array_size(not_removed) > 0?
                "a node a snapshot of __system__ holds cannot be deleted (see the error before): "
                "delete that snapshot (delete-node of its row in __snaps__ of treedb_system_schema) "
                "and open the treedb again; until then save-schema refuses, it would publish them again" :
                "see the errors logged before this, fix their cause and open the treedb again; "
                "until then save-schema refuses",
            NULL
        );
    }

    /*
     *  free
     */
    JSON_DECREF(declared_topics)
    JSON_DECREF(in_use)
    JSON_DECREF(cols_desc)
    json_decref(treedb);

    return failed > 0? -1 : 0;
}

/***************************************************************************
 *  The schema file IN USE by a treedb, read from where its tranger keeps it
 *  -- also before the treedb is open, which is when reconcile asks. NULL
 *  when there is none yet (a first open). Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *load_schema_file_in_use(hgobj gobj, const char *treedb_name)
{
    char directory[PATH_MAX];
    build_path(directory, sizeof(directory), gobj_read_str_attr(gobj, "path"), treedb_name, NULL);
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.treedb_schema.json", treedb_name);
    if(!file_exists(directory, filename)) {
        return NULL;
    }
    return load_json_from_file(gobj, directory, filename, 0);
}

/***************************************************************************
 *  The topic_version a topic RUNS with: the one of its `topic_var.json` in
 *  the store, which tranger2 rewrites only when a schema raises it. 0 when
 *  the topic has never been opened. Read from disk, as reconcile asks
 *  before the treedb is open.
 ***************************************************************************/
PRIVATE json_int_t running_topic_version(hgobj gobj, const char *treedb_name, const char *topic_name)
{
    char directory[PATH_MAX];
    build_path(directory, sizeof(directory),
        gobj_read_str_attr(gobj, "path"), treedb_name, topic_name, NULL);
    if(!file_exists(directory, "topic_var.json")) {
        return 0;
    }
    json_t *topic_var = load_json_from_file(gobj, directory, "topic_var.json", 0);
    json_int_t v = kw_get_int(gobj, topic_var, "topic_version", 0, KW_WILD_NUMBER);
    JSON_DECREF(topic_var)
    return v;
}

/***************************************************************************
 *  The versions IN USE of every topic of the schema file, and of every
 *  topic of `jn_schema` (the literal): what the file says (`file`) and what
 *  the store runs (`running`), which part ways in two cases:
 *
 *    file > running   an APPLIED schema not opened yet: apply-schema wrote
 *                     the file, the next open installs it;
 *    file < running   a file a literal wrote whole over topics the store
 *                     had ahead of it (7.25.4 did): tranger2 kept its
 *                     own.
 *
 *  A topic is in use at the higher of the two. Return is YOURS:
 *      {"file": {topic: v}, "running": {topic: v}}
 ***************************************************************************/
PRIVATE json_t *topic_versions_in_use(
    hgobj gobj,
    const char *treedb_name,
    json_t *file_in_use,    // not owned, may be NULL
    json_t *jn_schema       // not owned, the literal, may be NULL
)
{
    json_t *file = json_object();
    json_t *running = json_object();

    json_t *file_topics = file_in_use? schema_topics_as_list(gobj, file_in_use) : json_array();
    json_t *literal_topics = jn_schema? schema_topics_as_list(gobj, jn_schema) : json_array();
    json_t *all[] = {file_topics, literal_topics};
    for(size_t i = 0; i < sizeof(all)/sizeof(all[0]); i++) {
        int idx; json_t *topic;
        json_array_foreach(all[i], idx, topic) {
            const char *topic_name = kw_get_str(gobj, topic, "id", "", 0);
            if(empty_string(topic_name)) {
                topic_name = kw_get_str(gobj, topic, "topic_name", "", 0);
            }
            if(empty_string(topic_name) || json_object_get(running, topic_name)) {
                continue;
            }
            json_object_set_new(file, topic_name,
                json_integer(file_in_use? schema_topic_version(gobj, file_in_use, topic_name) : 0));
            json_object_set_new(running, topic_name,
                json_integer(running_topic_version(gobj, treedb_name, topic_name)));
        }
    }
    JSON_DECREF(file_topics)
    JSON_DECREF(literal_topics)

    return json_pack("{s:o, s:o}", "file", file, "running", running);
}

PRIVATE json_int_t topic_version_in_use(json_t *in_use, const char *topic_name) // not owned
{
    json_int_t f = json_integer_value(json_object_get(json_object_get(in_use, "file"), topic_name));
    json_int_t r = json_integer_value(json_object_get(json_object_get(in_use, "running"), topic_name));
    return (f > r)? f : r;
}

/***************************************************************************
 *  Does topic `topic_name` say something else in `a` than in `b`? Its
 *  topic_version is not compared: a number moved is no content moved. A
 *  topic missing from one side differs. With `cols_only`, only the columns
 *  are compared, and a change of their ORDER alone is no difference
 *  (tranger2 re-orders topic_cols.json by itself).
 ***************************************************************************/
PRIVATE BOOL schema_topic_differs(
    json_t *a,              // not owned, a schema
    json_t *b,              // not owned, a schema
    const char *topic_name,
    BOOL cols_only
)
{
    json_t *ta = schema_topic(a, topic_name);
    json_t *tb = schema_topic(b, topic_name);
    if(!ta || !tb) {
        return (ta || tb)? TRUE : FALSE;
    }

    json_t *ca = cols_only?
        json_pack("{s:s, s:O}", "id", topic_name, "cols", json_object_get(ta, "cols")) :
        json_deep_copy(ta);
    json_t *cb = cols_only?
        json_pack("{s:s, s:O}", "id", topic_name, "cols", json_object_get(tb, "cols")) :
        json_deep_copy(tb);
    json_object_del(ca, "topic_version");
    json_object_del(cb, "topic_version");
    json_t *sa = json_pack("{s:[o]}", "topics", ca);
    json_t *sb = json_pack("{s:[o]}", "topics", cb);
    json_t *diff = schema_diff(sa, sb);

    BOOL differs = FALSE;
    const char *section[] = {"added", "removed", "changed", NULL};
    for(int i = 0; section[i]; i++) {
        const char *leaf; json_t *v;
        json_object_foreach(json_object_get(diff, section[i]), leaf, v) {
            if(cols_only && strstr(leaf, "__cols_order__")) {
                continue;
            }
            differs = TRUE;
        }
    }
    JSON_DECREF(diff)
    JSON_DECREF(sa)
    JSON_DECREF(sb)
    return differs;
}

/***************************************************************************
 *  The record of the dynamic schema in the file in use, kept beside the
 *  saved schemas: `saved_schemas/<treedb>.applied.json`,
 *
 *      {"schema_version": 13, "topics": {"users": "applied"}}
 *
 *  the version apply-schema put in use and the topics an apply changed
 *  (their topic_version went up, or they are new), each with what it is:
 *
 *      applied     no open has read it yet;
 *      in_use      an open read it: the treedb RUNS it.
 *
 *  It lives while that file is in use. The open that reads the apply marks
 *  its topics "in_use"; a literal that replaces the file withdraws them,
 *  says each one (as "applied" or "in_use") and removes the record. Both
 *  happen only once the open has OPENED (settle_apply_record); used up
 *  before, an open that then failed lost the record.
 *
 *  A record written for a file that did not reach the disk -- the process
 *  died between the record and the rename of the file, which is the order
 *  apply-schema writes them in -- keeps the record it replaced in
 *  `previous`, and that one is read when its version is the one in use.
 *
 *  It is recorded, never inferred from the store: a topic whose directory
 *  or topic_var.json is missing is no apply, and the record stays while
 *  its file is in use.
 ***************************************************************************/
PRIVATE void apply_record_filename(const char *treedb_name, char *bf, size_t bfsize)
{
    snprintf(bf, bfsize, "%s.applied.json", treedb_name);
}

/***************************************************************************
 *  The topics of a record as {topic: kind}. A record written as a list of
 *  topics (before "in_use" existed) is all "applied". Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *apply_record_topics(json_t *record) // not owned
{
    json_t *topics = json_object_get(record, "topics");
    if(json_is_object(topics)) {
        return json_deep_copy(topics);
    }
    json_t *kinds = json_object();
    int idx; json_t *jn_topic;
    json_array_foreach(topics, idx, jn_topic) {
        if(json_is_string(jn_topic)) {
            json_object_set_new(kinds, json_string_value(jn_topic), json_string("applied"));
        }
    }
    return kinds;
}

/***************************************************************************
 *  Does any topic of {topic: kind} have `kind`?
 ***************************************************************************/
PRIVATE BOOL record_has_kind(json_t *topics, const char *kind) // not owned
{
    const char *topic_name; json_t *jn_kind;
    json_object_foreach(topics, topic_name, jn_kind) {
        if(json_is_string(jn_kind) && strcmp(json_string_value(jn_kind), kind)==0) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  The record, as it is on disk, or NULL. Return is YOURS
 ***************************************************************************/
PRIVATE json_t *load_apply_record(hgobj gobj, const char *treedb_name)
{
    char saved_dir[PATH_MAX];
    saved_schema_dir(gobj, saved_dir, sizeof(saved_dir));
    char filename[NAME_MAX];
    apply_record_filename(treedb_name, filename, sizeof(filename));
    if(!file_exists(saved_dir, filename)) {
        return NULL;
    }
    return load_json_from_file(gobj, saved_dir, filename, 0);
}

/***************************************************************************
 *  Write the record. -1 when it could not be written (logged)
 ***************************************************************************/
PRIVATE int write_apply_record(hgobj gobj, const char *treedb_name, json_t *record) // owned
{
    char saved_dir[PATH_MAX];
    saved_schema_dir(gobj, saved_dir, sizeof(saved_dir));
    char filename[NAME_MAX];
    apply_record_filename(treedb_name, filename, sizeof(filename));

    return save_json_to_file(   // Error already logged
        gobj,
        saved_dir,
        filename,
        (int)gobj_read_integer_attr(gobj, "xpermission"),
        (int)gobj_read_integer_attr(gobj, "rpermission"),
        0,
        TRUE,   // Create file if not exists or overwrite.
        FALSE,  // only_read
        record  // owned
    );
}

/***************************************************************************
 *  Remove the record, if there is one (a failure is logged)
 ***************************************************************************/
PRIVATE void remove_apply_record(hgobj gobj, const char *treedb_name)
{
    char saved_dir[PATH_MAX];
    saved_schema_dir(gobj, saved_dir, sizeof(saved_dir));
    char filename[NAME_MAX];
    apply_record_filename(treedb_name, filename, sizeof(filename));
    if(!file_exists(saved_dir, filename)) {
        return;
    }
    if(file_remove(saved_dir, filename) < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_SYSTEM,
            "msg",              "%s", "Cannot remove the record of an apply",
            "treedb_name",      "%s", treedb_name,
            "directory",        "%s", saved_dir,
            "filename",         "%s", filename,
            "errno",            "%s", strerror(errno),
            NULL
        );
    }
}

/***************************************************************************
 *  Record an apply BEFORE the file it puts in use is renamed in place:
 *  an apply that cannot be recorded is not made, because nothing could say
 *  it when a literal withdraws it. `*p_previous` is the record this one
 *  replaces (YOURS, NULL when there was none), for restore_apply_record()
 *  when the rename fails. -1 when the record could not be written (logged).
 *
 *  The topics of the record of the file it replaces go on: that file is
 *  still under this one, so what it applied is part of what runs. That
 *  record is the one on disk, or its `previous` when the one on disk is of
 *  an apply whose rename never happened (the process died between the two):
 *  the one on disk alone does not name the topics of the file in use, and
 *  a later literal would not report them. It is also what goes into
 *  `previous`.
 ***************************************************************************/
PRIVATE int record_apply(
    hgobj gobj,
    const char *treedb_name,
    json_t *applied,        // not owned, the schema put in use
    json_t *replaced_file,  // not owned, the file it replaces, may be NULL
    json_t **p_previous     // YOURS, the record replaced, or NULL
)
{
    *p_previous = load_apply_record(gobj, treedb_name);

    json_t *of_replaced = NULL;     // the record of the file replaced, not owned
    if(*p_previous && replaced_file) {
        json_int_t replaced_version = schema_version_of(gobj, replaced_file);
        json_t *older = json_object_get(*p_previous, "previous");
        if(schema_version_of(gobj, *p_previous) == replaced_version) {
            of_replaced = *p_previous;
        } else if(older && schema_version_of(gobj, older) == replaced_version) {
            of_replaced = older;
        }
    }
    json_t *topics = of_replaced? apply_record_topics(of_replaced) : json_object();

    json_t *applied_topics = schema_topics_as_list(gobj, applied);
    int idx; json_t *topic;
    json_array_foreach(applied_topics, idx, topic) {
        const char *topic_name = kw_get_str(gobj, topic, "id", "", 0);
        if(empty_string(topic_name)) {
            topic_name = kw_get_str(gobj, topic, "topic_name", "", 0);
        }
        if(empty_string(topic_name)) {
            continue;
        }
        json_int_t before = replaced_file? schema_topic_version(gobj, replaced_file, topic_name) : 0;
        if(kw_get_int(gobj, topic, "topic_version", 0, KW_WILD_NUMBER) > before) {
            json_object_set_new(topics, topic_name, json_string("applied"));
        }
    }
    JSON_DECREF(applied_topics)

    json_t *record = json_pack("{s:I, s:o}",
        "schema_version", schema_version_of(gobj, applied),
        "topics", topics
    );
    if(of_replaced) {
        json_t *previous = json_deep_copy(of_replaced);
        json_object_del(previous, "previous");
        json_object_set_new(record, "previous", previous);
    }
    int ret = write_apply_record(gobj, treedb_name, record);
    if(ret < 0) {
        JSON_DECREF(*p_previous)
    }
    return ret;
}

/***************************************************************************
 *  The rename of an apply failed: put back the record it replaced, or
 *  remove the new one when there was none (failures are logged)
 ***************************************************************************/
PRIVATE void restore_apply_record(hgobj gobj, const char *treedb_name, json_t *previous) // not owned
{
    if(previous) {
        write_apply_record(gobj, treedb_name, json_deep_copy(previous));    // Error already logged
    } else {
        remove_apply_record(gobj, treedb_name);     // Error already logged
    }
}

/***************************************************************************
 *  The {topic: kind} of the record of the file in use (`in_use_version`),
 *  NULL when there is none. A record of another file is not this file's:
 *  it is dropped, and said.  Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *read_apply_record(hgobj gobj, const char *treedb_name, json_int_t in_use_version)
{
    json_t *record = load_apply_record(gobj, treedb_name);
    if(!record) {
        return NULL;
    }

    json_t *topics = NULL;
    json_t *previous = json_object_get(record, "previous");
    if(schema_version_of(gobj, record) == in_use_version) {
        topics = apply_record_topics(record);
    } else if(previous && schema_version_of(gobj, previous) == in_use_version) {
        topics = apply_record_topics(previous);
    } else {
        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "Record of an apply of another schema file than the one in use: dropped",
            "treedb_name",      "%s", treedb_name,
            "record_version",   "%d", (int)schema_version_of(gobj, record),
            "in_use_version",   "%d", (int)in_use_version,
            NULL
        );
        remove_apply_record(gobj, treedb_name);     // Error already logged
    }
    JSON_DECREF(record)
    return topics;
}

/***************************************************************************
 *  The RECORD of an unfinished projection, `<treedb>.unfinished.json` in
 *  saved_schemas/ (what new_unfinished describes). It is written when a
 *  projection fails, and removed when one succeeds: while it is there,
 *  every open retries the projection, what it left is nobody's draft
 *  while it stays as it was left, and save-schema refuses.
 *
 *  It is what says a projection is unfinished, and nothing else can:
 *  c_schema_version 0 is also a projection seeded from a dynamic file, and
 *  "__system__ holds it and the file does not" is also a column the
 *  operator added. Guessed from those, a draft would be taken for a
 *  leftover and deleted in silence, and a leftover would be reported as
 *  withdrawn work.
 ***************************************************************************/
PRIVATE void unfinished_record_filename(const char *treedb_name, char *bf, size_t bfsize)
{
    snprintf(bf, bfsize, "%s.unfinished.json", treedb_name);
}

/***************************************************************************
 *  The record of an unfinished projection, or NULL. Return is YOURS
 *
 *  A record that is there but cannot be read (not json, or not the shape
 *  new_unfinished() writes) still says the projection is UNFINISHED: what
 *  it left is unknown, and it is answered as a record whose `not_written`
 *  is the treedb itself and whose `leftovers` are none, with "unreadable"
 *  true. So every open retries the projection and save-schema refuses, as
 *  for any record, and the retry writes it again (or removes it). With no
 *  leftovers known, what __system__ holds over the file is taken for the
 *  operator's drafts: reported as withdrawn by the open that replaces it,
 *  never deleted in silence -- and as "unsaved", the kinds it kept are
 *  lost with it. It is said as ONE WARNING at every read, with the cause:
 *  the file is read here, not with load_json_from_file(), which logs bad
 *  json as a CRITICAL of its own.
 ***************************************************************************/
PRIVATE json_t *load_unfinished_record(hgobj gobj, const char *treedb_name)
{
    char saved_dir[PATH_MAX];
    saved_schema_dir(gobj, saved_dir, sizeof(saved_dir));
    char filename[NAME_MAX];
    unfinished_record_filename(treedb_name, filename, sizeof(filename));
    if(!file_exists(saved_dir, filename)) {
        return NULL;
    }

    json_t *record = NULL;
    json_error_t error;
    memset(&error, 0, sizeof(error));
    char path[PATH_MAX];
    if(build_path(path, sizeof(path), saved_dir, filename, NULL)) {
        int fd = open(path, O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
        if(fd < 0) {
            snprintf(error.text, sizeof(error.text), "cannot open it: %s", strerror(errno));
        } else {
            record = json_loadfd(fd, 0, &error);
            close(fd);
        }
    } else {
        snprintf(error.text, sizeof(error.text), "path too long");   // Error already logged
    }
    if(json_is_object(record) &&
            json_is_array(json_object_get(record, "not_removed")) &&
            json_is_array(json_object_get(record, "not_written")) &&
            json_is_array(json_object_get(record, "leftovers"))) {
        return record;
    }
    if(record) {
        snprintf(error.text, sizeof(error.text), "not the shape of a record");
    }
    JSON_DECREF(record)

    gobj_log_warning(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_TREEDB,
        "msg",              "%s", "Record of an unfinished projection cannot be read: the projection is unfinished, what it left is unknown; every open retries it, save-schema refuses, and what __system__ holds over the file is taken for drafts",
        "treedb_name",      "%s", treedb_name,
        "directory",        "%s", saved_dir,
        "filename",         "%s", filename,
        "error",            "%s", error.text,
        NULL
    );
    record = new_unfinished(0);
    add_unfinished(record, "not_written", treedb_name, NULL);
    json_array_clear(json_object_get(record, "leftovers"));
    json_object_set_new(record, "unreadable", json_true());
    return record;
}

/***************************************************************************
 *  Write the record of an unfinished projection WHOLE: through a temporary
 *  `<filename>.new` and a rename(), so the file on disk is the old record
 *  or the new one, never a torn one. The temporary is unlinked first and
 *  created O_EXCL|O_NOFOLLOW (one left behind by a process that died is
 *  not reused, one that is a symlink is not followed), fsync'ed before the
 *  rename, and the directory after it.
 *
 *  -1 when it could not be written (logged), with the old one untouched.
 ***************************************************************************/
PRIVATE int write_unfinished_record(hgobj gobj, const char *treedb_name, json_t *record) // not owned
{
    char saved_dir[PATH_MAX];
    saved_schema_dir(gobj, saved_dir, sizeof(saved_dir));
    char filename[NAME_MAX];
    unfinished_record_filename(treedb_name, filename, sizeof(filename));
    char filename_new[NAME_MAX];
    int written = snprintf(filename_new, sizeof(filename_new), "%s.new", filename);
    if(written < 0 || written >= (int)sizeof(filename_new)) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PARAMETER,
            "msg",              "%s", "Cannot write the record of an unfinished projection, its name is too long",
            "treedb_name",      "%s", treedb_name,
            "filename",         "%s", filename,
            NULL
        );
        return -1;
    }

    char path[PATH_MAX];
    char path_new[PATH_MAX];
    if(!build_path(path, sizeof(path), saved_dir, filename, NULL) ||
            !build_path(path_new, sizeof(path_new), saved_dir, filename_new, NULL)) {
        return -1;  // Error already logged
    }

    if(!is_directory(saved_dir) &&
            mkrdir(saved_dir, (int)gobj_read_integer_attr(gobj, "xpermission")) < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_SYSTEM,
            "msg",              "%s", "Cannot write the record of an unfinished projection, cannot create its directory",
            "treedb_name",      "%s", treedb_name,
            "directory",        "%s", saved_dir,
            "errno",            "%d", errno,
            "serrno",           "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    if(unlink(path_new) < 0 && errno != ENOENT) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_SYSTEM,
            "msg",              "%s", "Cannot write the record of an unfinished projection, cannot remove a temporary file left behind",
            "treedb_name",      "%s", treedb_name,
            "path",             "%s", path_new,
            "errno",            "%d", errno,
            "serrno",           "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    int mode = (int)gobj_read_integer_attr(gobj, "rpermission");
    int fd = open(path_new, O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW|O_CLOEXEC, mode);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_SYSTEM,
            "msg",              "%s", "Cannot write the record of an unfinished projection, cannot create the temporary file",
            "treedb_name",      "%s", treedb_name,
            "path",             "%s", path_new,
            "errno",            "%d", errno,
            "serrno",           "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    const char *failed = NULL;
    if(fchmod(fd, (mode_t)mode) < 0) {
        failed = "fchmod() FAILED";
    } else if(json_dumpfd(record, fd, JSON_INDENT(4)) < 0) {
        failed = "write FAILED";
    } else if(fsync(fd) < 0) {
        failed = "fsync() FAILED";
    }
    int err = errno;
    if(close(fd) < 0 && !failed) {
        failed = "close() FAILED";
        err = errno;
    }
    if(!failed && rename(path_new, path) < 0) {
        failed = "rename() FAILED";
        err = errno;
    }
    if(failed) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_SYSTEM,
            "msg",              "%s", "Cannot write the record of an unfinished projection",
            "treedb_name",      "%s", treedb_name,
            "path",             "%s", path,
            "failed",           "%s", failed,
            "errno",            "%d", err,
            "serrno",           "%s", strerror(err),
            NULL
        );
        unlink(path_new);
        return -1;
    }

    int dir_fd = open(saved_dir, O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(dir_fd < 0 || fsync(dir_fd) < 0) {
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_SYSTEM,
            "msg",              "%s", "Record of an unfinished projection written, but its directory cannot be flushed",
            "treedb_name",      "%s", treedb_name,
            "directory",        "%s", saved_dir,
            "errno",            "%d", errno,
            "serrno",           "%s", strerror(errno),
            NULL
        );
    }
    if(dir_fd >= 0) {
        close(dir_fd);
    }
    return 0;
}

/***************************************************************************
 *  Remove the record of an unfinished projection, if there is one (a
 *  failure is logged)
 ***************************************************************************/
PRIVATE void remove_unfinished_record(hgobj gobj, const char *treedb_name)
{
    char saved_dir[PATH_MAX];
    saved_schema_dir(gobj, saved_dir, sizeof(saved_dir));
    char filename[NAME_MAX];
    unfinished_record_filename(treedb_name, filename, sizeof(filename));
    if(!file_exists(saved_dir, filename)) {
        return;
    }
    if(file_remove(saved_dir, filename) < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_SYSTEM,
            "msg",              "%s", "Cannot remove the record of an unfinished projection",
            "treedb_name",      "%s", treedb_name,
            "directory",        "%s", saved_dir,
            "filename",         "%s", filename,
            "errno",            "%s", strerror(errno),
            NULL
        );
    }
}

/***************************************************************************
 *  What reconcile decided for the record, done now that the open OPENED:
 *  "remove" (a literal replaced the file) or the topics all "in_use" (the
 *  open read the apply).
 ***************************************************************************/
PRIVATE void settle_apply_record(hgobj gobj, const char *treedb_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *todo = json_object_get(priv->jn_apply_record_at_open, treedb_name);
    if(!todo) {
        return;
    }
    const char *action = kw_get_str(gobj, todo, "action", "", 0);
    if(strcmp(action, "remove")==0) {
        remove_apply_record(gobj, treedb_name);     // Error already logged
    } else if(strcmp(action, "ran")==0) {
        json_t *topics = json_object();
        const char *topic_name; json_t *kind;
        json_object_foreach(kw_get_dict(gobj, todo, "topics", 0, 0), topic_name, kind) {
            json_object_set_new(topics, topic_name, json_string("in_use"));
        }
        write_apply_record(gobj, treedb_name,   // Error already logged
            json_pack("{s:I, s:o}",
                "schema_version", kw_get_int(gobj, todo, "schema_version", 0, KW_WILD_NUMBER),
                "topics", topics
            )
        );
    } else {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "Unknown action for the record of an apply",
            "treedb_name",      "%s", treedb_name,
            "action",           "%s", action,
            NULL
        );
    }
    json_object_del(priv->jn_apply_record_at_open, treedb_name);
}

/***************************************************************************
 *  Do two schemas say different things? The comparison of CONTENT that
 *  the tie is judged by (schema_diff): cols listed or keyed by name, each
 *  carrying its `id` or not, are the same schema.
 ***************************************************************************/
PRIVATE BOOL schemas_differ(json_t *a, json_t *b) // not owned
{
    json_t *diff = schema_diff(a, b);
    BOOL differs = json_object_size(json_object_get(diff, "added")) > 0 ||
        json_object_size(json_object_get(diff, "removed")) > 0 ||
        json_object_size(json_object_get(diff, "changed")) > 0;
    JSON_DECREF(diff)
    return differs;
}

/***************************************************************************
 *  Say why a literal that is not installed is not: behind the file in
 *  use, or at its number with another content (said at every open until
 *  the literal moves on). Nothing when it IS the file.
 ***************************************************************************/
PRIVATE void say_literal_not_installed(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_schema,          // not owned, the literal
    json_t *file_in_use,        // not owned
    json_int_t stored_version,
    json_int_t stored_c_version
)
{
    json_int_t new_version = schema_version_of(gobj, jn_schema);
    json_int_t in_use_version = schema_version_of(gobj, file_in_use);

    if(new_version < in_use_version) {
        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "TreeDB schema from C is behind the schema in use, not applied",
            "treedb_name",      "%s", treedb_name,
            "schema_version",   "%d", (int)new_version,
            "in_use_version",   "%d", (int)in_use_version,
            "stored_version",   "%d", (int)stored_version,
            NULL
        );
    } else if(stored_c_version != new_version) {
        /*
         *  Two schemas under one number: the file wins, as ties always
         *  do, and it is said at every open until the literal moves on
         */
        json_t *diff = schema_diff(file_in_use, jn_schema);
        BOOL differs = json_object_size(json_object_get(diff, "added")) > 0 ||
            json_object_size(json_object_get(diff, "removed")) > 0 ||
            json_object_size(json_object_get(diff, "changed")) > 0;
        if(differs) {
            gobj_log_warning(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_TREEDB,
                "msg",              "%s", "Schema from C has the schema_version of the dynamic schema in use but another content: NOT applied, raise its schema_version to publish it",
                "treedb_name",      "%s", treedb_name,
                "schema_version",   "%d", (int)new_version,
                "c_schema_version", "%d", (int)stored_c_version,
                "diff",             "%j", diff,
                NULL
            );
        }
        JSON_DECREF(diff)
    }
}

/***************************************************************************
 *  Does a topic of __system__ hold a column that is not a leftover? Then
 *  somebody added it after the projection that left the topic.
 ***************************************************************************/
PRIVATE BOOL topic_holds_other_cols(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_id,
    json_t *leftover_ids    // not owned, {id: true}
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *tree = gobj_node_tree(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        json_object(),
        gobj
    );
    if(!tree) {
        return FALSE;   // Error already logged
    }
    BOOL holds = FALSE;
    json_t *topic = json_object_get(kw_get_dict(gobj, tree, "topics", 0, 0), topic_id);
    const char *col_id; json_t *col;
    json_object_foreach(kw_get_dict(gobj, topic, "cols", 0, 0), col_id, col) {
        if(!json_object_get(leftover_ids, col_id)) {
            holds = TRUE;
            break;
        }
    }
    JSON_DECREF(tree)
    return holds;
}

/***************************************************************************
 *  The id of __system__ a row of diff_treedb_schema() is about: the
 *  topic's, or the column's when the row names one. NULL when it names
 *  none (logged when the id does not fit).
 ***************************************************************************/
PRIVATE const char *row_node_id(
    hgobj gobj,
    const char *treedb_name,
    json_t *row,    // not owned
    char *bf,
    int bfsize
)
{
    const char *topic_name = kw_get_str(gobj, row, "topic", "", 0);
    const char *col_name = kw_get_str(gobj, row, "col", "", 0);
    if(empty_string(topic_name)) {
        return NULL;
    }
    if(empty_string(col_name)) {
        return build_schema_node_id(gobj, bf, bfsize, treedb_name, topic_name);
    }
    char topic_id[RECORD_KEY_VALUE_MAX];
    if(!build_schema_node_id(gobj, topic_id, sizeof(topic_id), treedb_name, topic_name)) {
        return NULL;    // Error already logged
    }
    return build_schema_node_id(gobj, bf, bfsize, topic_id, col_name);
}

/***************************************************************************
 *  {id: true} of a list of ids. Return is YOURS
 ***************************************************************************/
PRIVATE json_t *ids_as_dict(json_t *ids) // not owned, may be NULL
{
    json_t *dict = json_object();
    int idx; json_t *jn_id;
    json_array_foreach(ids, idx, jn_id) {
        if(json_is_string(jn_id)) {
            json_object_set_new(dict, json_string_value(jn_id), json_true());
        }
    }
    return dict;
}

/***************************************************************************
 *  The topic of `treedb_name` a node of __system__ belongs to, read from
 *  its id, which is its parent's id and its name (`value`, see
 *  build_schema_node_id): "<treedb>.<topic>" for a topic (`is_topic`),
 *  and "<treedb>.<topic>.<column>" for a column. NULL when the id is not
 *  of that treedb.
 ***************************************************************************/
PRIVATE const char *topic_of_schema_id(
    const char *treedb_name,
    const char *id,
    const char *value,
    BOOL is_topic,
    char *bf,
    size_t bfsize
)
{
    size_t tlen = strlen(treedb_name);
    if(empty_string(value) || strncmp(id, treedb_name, tlen)!=0 || id[tlen] != '.') {
        return NULL;
    }
    const char *rest = id + tlen + 1;
    if(is_topic) {
        if(strcmp(rest, value)!=0 || strlen(rest) >= bfsize) {
            return NULL;
        }
        snprintf(bf, bfsize, "%s", rest);
        return bf;
    }
    size_t rlen = strlen(rest);
    size_t vlen = strlen(value);
    if(rlen < vlen + 2 || rest[rlen - vlen - 1] != '.' || strcmp(rest + rlen - vlen, value)!=0) {
        return NULL;
    }
    if(rlen - vlen - 1 >= bfsize) {
        return NULL;
    }
    snprintf(bf, bfsize, "%.*s", (int)(rlen - vlen - 1), rest);
    return bf;
}

/***************************************************************************
 *  The nodes of a treedb in __system__ that its tree (`tree`, the node
 *  tree of the treedb, NULL when it has no node) does not reach: a topic
 *  unlinked from the treedb, a column unlinked from its topic, and the
 *  columns of such a topic. An unlink by the operator leaves them, and so
 *  does a link of a projection that failed. They are still nodes, with
 *  their id: a projection that creates one of those ids fails on it
 *  ("Node already exists"), at every open.
 *
 *  Return is YOURS, {id: {"topic": <topic name>, "is_topic": bool,
 *  "node": <the node>}}.
 ***************************************************************************/
PRIVATE json_t *orphan_nodes(hgobj gobj, const char *treedb_name, json_t *tree)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *reached = json_object();
    const char *topic_id; json_t *topic;
    json_t *tree_topics = tree? kw_get_dict(gobj, tree, "topics", 0, 0) : NULL;
    json_object_foreach(tree_topics, topic_id, topic) {
        json_object_set_new(reached, topic_id, json_true());
        const char *col_id; json_t *col;
        json_object_foreach(kw_get_dict(gobj, topic, "cols", 0, 0), col_id, col) {
            json_object_set_new(reached, col_id, json_true());
        }
    }

    json_t *orphans = json_object();
    const char *system_topics[] = {"topics", "cols", NULL};
    for(int i = 0; system_topics[i]; i++) {
        BOOL is_topic = (i == 0)? TRUE : FALSE;
        json_t *nodes = gobj_list_nodes(
            priv->gobj_node_system,
            system_topics[i],
            json_object(),
            json_pack("{s:b}", "refs", 1),
            gobj
        );
        int idx; json_t *node;
        json_array_foreach(nodes, idx, node) {
            const char *id = kw_get_str(gobj, node, "id", "", 0);
            if(json_object_get(reached, id)) {
                continue;
            }
            char topic_name_[NAME_MAX];
            const char *topic_name = topic_of_schema_id(
                treedb_name, id, kw_get_str(gobj, node, "value", "", 0), is_topic,
                topic_name_, sizeof(topic_name_)
            );
            if(!topic_name) {
                continue;   /*  of another treedb  */
            }
            json_object_set_new(orphans, id, json_pack("{s:s, s:b, s:O}",
                "topic", topic_name,
                "is_topic", is_topic,
                "node", node
            ));
        }
        JSON_DECREF(nodes)
    }
    JSON_DECREF(reached)
    return orphans;
}

/***************************************************************************
 *  The node tree of a treedb in __system__, or NULL when the treedb has
 *  no node there (a treedb never projected: not an error). Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *system_tree_of(hgobj gobj, const char *treedb_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *nodes = gobj_list_nodes(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        json_object(),
        gobj
    );
    BOOL exists = json_array_size(nodes) > 0? TRUE : FALSE;
    JSON_DECREF(nodes)
    if(!exists) {
        return NULL;
    }
    return gobj_node_tree(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        json_object(),
        gobj
    );
}

/***************************************************************************
 *  The attributes a projection WRITES in a topic (`is_topic`) or in a
 *  column of __system__, {attr: true}: the fields a schema declares. They
 *  are read from build_topic_projection() and build_col_projection()
 *  themselves, projecting a topic or a column that carries every field
 *  they know (the columns': every attribute of `cols_desc`), so a field
 *  they learn is learned here too. The rest of a node says how it is
 *  STORED -- its id, its links, its editor geometry, its metadata -- and a
 *  change there is no edit of the schema. Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *projection_attrs(hgobj gobj, BOOL is_topic, json_t *cols_desc)
{
    json_t *projected;
    if(is_topic) {
        json_t *jn_topic = json_pack("{s:{}}", "pkey2s");
        projected = build_topic_projection(gobj, jn_topic, "-", 0, 0);
        JSON_DECREF(jn_topic)
    } else {
        json_t *jn_col = json_pack("{s:s, s:s}", "id", "-", "type", "-");
        int idx; json_t *desc_entry;
        json_array_foreach(cols_desc, idx, desc_entry) {
            const char *attr = kw_get_str(gobj, desc_entry, "id", "", 0);
            if(!empty_string(attr) && !json_object_get(jn_col, attr)) {
                json_object_set_new(jn_col, attr, json_null());
            }
        }
        projected = build_col_projection(gobj, jn_col, cols_desc, 0);
        JSON_DECREF(jn_col)
    }

    json_t *attrs = json_object();
    const char *attr; json_t *v;
    json_object_foreach(projected, attr, v) {
        json_object_set_new(attrs, attr, json_true());
    }
    JSON_DECREF(projected)
    return attrs;
}

/***************************************************************************
 *  What is at the id `id` of __system__, a topic or a column in `tree`
 *  (the node tree of the treedb), as the schema declares it: only the
 *  attributes a projection writes (`topic_attrs` or `col_attrs`, see
 *  projection_attrs). json null when nothing is there. Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *leftover_node(
    hgobj gobj,
    json_t *tree,
    const char *id,
    json_t *topic_attrs,    // not owned
    json_t *col_attrs       // not owned
)
{
    json_t *topics = kw_get_dict(gobj, tree, "topics", 0, 0);
    json_t *node = json_object_get(topics, id);
    json_t *attrs = topic_attrs;
    if(!node) {
        attrs = col_attrs;
        const char *topic_id; json_t *topic;
        json_object_foreach(topics, topic_id, topic) {
            node = json_object_get(kw_get_dict(gobj, topic, "cols", 0, 0), id);
            if(node) {
                break;
            }
        }
    }
    if(!node) {
        return json_null();
    }
    json_t *copy = json_object();
    const char *attr; json_t *v;
    json_object_foreach(node, attr, v) {
        if(json_object_get(attrs, attr)) {
            json_object_set_new(copy, attr, json_deep_copy(v));
        }
    }
    return copy;
}

/***************************************************************************
 *  Keep in the record of an unfinished projection what it LEFT at each
 *  leftover id (`leftover_nodes`, {id: node or null}, see leftover_node),
 *  and the version of the meta-schema that says what those nodes hold
 *  (`system_schema_version`), so a later open can tell the leftover from
 *  an operator's edit of it (see leftovers_as_left).
 *
 *  No node is kept for an id the projection failed to WRITE
 *  (`not_written`): a write updates the node in memory before it saves
 *  it, and is not taken back when the save fails, so memory may say what
 *  the disk does not, and after a restart the disk is what is read. The
 *  record cannot say what is on disk there: the id is taken as left.
 *  Without the tree of the treedb (logged) no node is kept, and every
 *  leftover is then taken as left.
 ***************************************************************************/
PRIVATE void keep_leftover_nodes(hgobj gobj, const char *treedb_name, json_t *unfinished)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *nodes = json_object();
    json_object_set_new(unfinished, "leftover_nodes", nodes);
    json_object_set_new(unfinished, "system_schema_version",
        json_integer(priv->system_schema_version)
    );

    json_t *leftovers = json_object_get(unfinished, "leftovers");
    if(json_array_size(leftovers) == 0) {
        return;
    }

    json_t *tree = gobj_node_tree(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        json_object(),
        gobj
    );
    if(!tree) {
        return;     // Error already logged
    }

    json_t *cols_desc = _treedb_create_topic_cols_desc();
    json_t *topic_attrs = projection_attrs(gobj, TRUE, cols_desc);
    json_t *col_attrs = projection_attrs(gobj, FALSE, cols_desc);
    json_t *not_written = ids_as_dict(json_object_get(unfinished, "not_written"));

    int idx; json_t *jn_id;
    json_array_foreach(leftovers, idx, jn_id) {
        const char *id = json_string_value(jn_id);
        if(!id) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Leftover id of an unfinished projection is not a string: no node kept for it",
                "treedb_name",  "%s", treedb_name,
                "id",           "%j", jn_id,
                NULL
            );
            continue;
        }
        if(json_object_get(not_written, id)) {
            continue;
        }
        json_object_set_new(nodes, id, leftover_node(gobj, tree, id, topic_attrs, col_attrs));
    }

    JSON_DECREF(not_written)
    JSON_DECREF(col_attrs)
    JSON_DECREF(topic_attrs)
    JSON_DECREF(cols_desc)
    JSON_DECREF(tree)
}

/***************************************************************************
 *  The leftovers of an unfinished projection (`record`, see
 *  load_unfinished_record) that are still as it LEFT them. An id whose
 *  node in __system__ is not what the record kept (`leftover_nodes`) was
 *  edited since -- an attribute the projection writes changed (see
 *  projection_attrs), the node deleted, or one created where the
 *  projection left nothing -- and that edit is the operator's work: a
 *  draft like any other, reported by the open that replaces it. A link or
 *  an editor geometry is not compared: it is how the node is stored.
 *
 *  An id the record kept no node for is taken as left (see
 *  keep_leftover_nodes). So is every id when the nodes were kept under
 *  another meta-schema (the record's `system_schema_version`): a field it
 *  added or changed reads as an edit of every node, and nobody made it.
 *  That is said, as a WARNING: an operator's edit of a leftover made
 *  meanwhile is not told apart any more.
 *
 *  The ids that are NOT as left -- edited, unlinked, deleted -- go into
 *  `edited` (when given) as {id: topic name}: each is a draft of its
 *  topic. An unlinked column shows nowhere else: it is in no topic of
 *  the tree, and a topic the literal removes is one row of the diff.
 *
 *  Return is YOURS, a list of ids, NULL when there is no record.
 ***************************************************************************/
PRIVATE json_t *leftovers_as_left(
    hgobj gobj,
    const char *treedb_name,
    json_t *record,
    json_t *edited  // not owned, may be NULL: {id: topic} of the leftovers edited
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *leftovers = record? json_object_get(record, "leftovers") : NULL;
    json_t *kept = record? json_object_get(record, "leftover_nodes") : NULL;
    if(!json_is_array(leftovers)) {
        return NULL;
    }
    if(!json_is_object(kept) || json_object_size(kept) == 0) {
        return json_incref(leftovers);
    }

    json_int_t kept_meta = kw_get_int(
        gobj, record, "system_schema_version", 0, KW_WILD_NUMBER
    );
    if(kept_meta != priv->system_schema_version) {
        gobj_log_warning(gobj, 0,
            "function",                 "%s", __FUNCTION__,
            "msgset",                   "%s", MSGSET_TREEDB,
            "msg",                      "%s", "Leftovers of an unfinished projection were kept under another meta-schema: every leftover is taken as left, an edit of one made meanwhile is not told apart",
            "treedb_name",              "%s", treedb_name,
            "record_system_schema_version", "%d", (int)kept_meta,
            "system_schema_version",    "%d", (int)priv->system_schema_version,
            NULL
        );
        return json_incref(leftovers);
    }

    json_t *tree = gobj_node_tree(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        json_object(),
        gobj
    );
    if(!tree) {
        return json_incref(leftovers);  // Error already logged
    }

    json_t *cols_desc = _treedb_create_topic_cols_desc();
    json_t *topic_attrs = projection_attrs(gobj, TRUE, cols_desc);
    json_t *col_attrs = projection_attrs(gobj, FALSE, cols_desc);

    json_t *left = json_array();
    int idx; json_t *jn_id;
    json_array_foreach(leftovers, idx, jn_id) {
        const char *id = json_string_value(jn_id);
        if(!id) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Leftover id of an unfinished projection is not a string: skipped",
                "treedb_name",  "%s", treedb_name,
                "id",           "%j", jn_id,
                NULL
            );
            continue;
        }
        json_t *node_then = json_object_get(kept, id);
        if(!node_then) {
            json_array_append(left, jn_id);
            continue;
        }
        json_t *node_now = leftover_node(gobj, tree, id, topic_attrs, col_attrs);
        if(json_equal(node_now, node_then)) {
            json_array_append(left, jn_id);
        } else if(edited) {
            const char *value = kw_get_str(gobj,
                json_is_object(node_then)? node_then : node_now, "value", "", 0
            );
            char topic_name_[NAME_MAX];
            const char *topic_name = topic_of_schema_id(
                treedb_name, id, value, TRUE, topic_name_, sizeof(topic_name_)
            );
            if(!topic_name) {
                topic_name = topic_of_schema_id(
                    treedb_name, id, value, FALSE, topic_name_, sizeof(topic_name_)
                );
            }
            if(topic_name) {
                json_object_set_new(edited, id, json_string(topic_name));
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "Edited leftover whose id names no topic of the treedb: not a draft of any topic",
                    "treedb_name",  "%s", treedb_name,
                    "id",           "%s", id,
                    NULL
                );
            }
        }
        JSON_DECREF(node_now)
    }

    JSON_DECREF(col_attrs)
    JSON_DECREF(topic_attrs)
    JSON_DECREF(cols_desc)
    JSON_DECREF(tree)
    return left;
}

/***************************************************************************
 *  The rows of diff_treedb_schema() without the rows of what an unfinished
 *  projection LEFT (`leftovers`, ids of __system__, as leftovers_as_left()
 *  answers them): that is nobody's draft. Only those: a topic or column
 *  the operator added or edited meanwhile is a draft like any other, and
 *  so is a leftover topic that holds a column the projection did not
 *  leave there.
 ***************************************************************************/
PRIVATE json_t *rows_without_leftovers(
    hgobj gobj,
    const char *treedb_name,
    json_t *rows,       // owned
    json_t *leftovers   // not owned, ids of __system__, may be NULL
)
{
    if(json_array_size(leftovers) == 0) {
        return rows;
    }

    json_t *leftover_ids = ids_as_dict(leftovers);

    json_t *kept = json_array();
    int idx; json_t *row;
    json_array_foreach(rows, idx, row) {
        char id_[RECORD_KEY_VALUE_MAX];
        const char *id = row_node_id(gobj, treedb_name, row, id_, sizeof(id_));
        if(!id || !json_object_get(leftover_ids, id)) {
            json_array_append(kept, row);
            continue;
        }
        if(empty_string(kw_get_str(gobj, row, "col", "", 0)) &&
                strcmp(kw_get_str(gobj, row, "kind", "", 0), "only_in_stored")==0 &&
                topic_holds_other_cols(gobj, treedb_name, id, leftover_ids)) {
            json_array_append(kept, row);
        }
    }

    JSON_DECREF(leftover_ids)
    JSON_DECREF(rows)
    return kept;
}

/***************************************************************************
 *  The topics of __system__ that differ from the schema file in use: the
 *  operator's drafts, {topic: true}. What an unfinished projection
 *  (`record`, may be NULL) left, as it left it, is not counted: it is
 *  nobody's work. A leftover edited since (see leftovers_as_left) is a
 *  draft of its topic.
 *
 *  `*p_draft_ids` is where those drafts ARE, {id: true}: the ids of
 *  __system__ that carry them, a leftover never among them. A projection
 *  that cannot replace one of these does not make it a leftover: it stays
 *  a draft, and the open that replaces it says it (see upsert). The nodes
 *  the tree does not reach (orphan_nodes) are among them too, unless the
 *  projection left them: an unlink the operator made, which the
 *  projection removes or takes over, and says.
 *
 *  Return is YOURS, and so is `*p_draft_ids`.
 ***************************************************************************/
PRIVATE json_t *drafts_over_file(
    hgobj gobj,
    const char *treedb_name,
    json_t *file_in_use,    // not owned
    json_t *record,         // not owned, the record of an unfinished projection, may be NULL
    json_t **p_draft_ids
)
{
    json_t *edited = json_object();
    json_t *leftovers = leftovers_as_left(gobj, treedb_name, record, edited);

    json_t *rows = json_array();
    json_t *summary = diff_treedb_schema(gobj, treedb_name, file_in_use, rows);
    JSON_DECREF(summary)
    rows = rows_without_leftovers(gobj, treedb_name, rows, leftovers);
    json_t *drafts = draft_changed_from_rows(gobj, rows);

    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *leftover_ids = ids_as_dict(leftovers);
    json_t *tree = NULL;    // __system__ of the treedb, read when a topic only there asks it
    *p_draft_ids = json_object();
    int idx; json_t *row;
    json_array_foreach(rows, idx, row) {
        const char *kind = kw_get_str(gobj, row, "kind", "", 0);
        if(strcmp(kind, "version")==0) {
            continue;   /*  not an edit, see draft_changed_from_rows()  */
        }
        char id_[RECORD_KEY_VALUE_MAX];
        const char *id = row_node_id(gobj, treedb_name, row, id_, sizeof(id_));
        if(!id) {
            continue;
        }
        if(!json_object_get(leftover_ids, id)) {
            json_object_set_new(*p_draft_ids, id, json_true());
        }

        /*
         *  A topic only in __system__ is ONE row: its columns carry the
         *  draft as much as it does, and diff_treedb_schema() names none
         */
        if(strcmp(kind, "only_in_stored")!=0 || !empty_string(kw_get_str(gobj, row, "col", "", 0))) {
            continue;
        }
        if(!tree) {
            tree = gobj_node_tree(
                priv->gobj_node_system,
                "treedbs",
                json_pack("{s:s}", "id", treedb_name),
                json_object(),
                gobj
            );
        }
        json_t *topic = json_object_get(kw_get_dict(gobj, tree, "topics", 0, 0), id);
        const char *col_id; json_t *col;
        json_object_foreach(kw_get_dict(gobj, topic, "cols", 0, 0), col_id, col) {
            if(!json_object_get(leftover_ids, col_id)) {
                json_object_set_new(*p_draft_ids, col_id, json_true());
            }
        }
    }
    JSON_DECREF(tree)

    const char *edited_id; json_t *jn_topic;
    json_object_foreach(edited, edited_id, jn_topic) {
        json_object_set_new(drafts, json_string_value(jn_topic), json_true());
        json_object_set_new(*p_draft_ids, edited_id, json_true());
    }

    json_t *system_tree = system_tree_of(gobj, treedb_name);
    json_t *orphans = orphan_nodes(gobj, treedb_name, system_tree);
    const char *orphan_id; json_t *orphan;
    json_object_foreach(orphans, orphan_id, orphan) {
        if(!json_object_get(leftover_ids, orphan_id)) {
            json_object_set_new(*p_draft_ids, orphan_id, json_true());
        }
    }
    JSON_DECREF(orphans)
    JSON_DECREF(system_tree)

    JSON_DECREF(leftover_ids)
    JSON_DECREF(rows)
    JSON_DECREF(leftovers)
    JSON_DECREF(edited)
    return drafts;
}

/***************************************************************************
 *  Keep the __system__ projection in step with what the treedb RUNS.
 *
 *  With impose_c_schema off, treedb_open_db() installs the literal only
 *  when it is newer than the schema file in use (or there is none), and
 *  then over the WHOLE file. So (the user's decision, 2026-09-23):
 *
 *      literal newer than the file, or no file
 *                  the literal wins WHOLE: __system__ is projected from
 *                  it whole (upsert_treedb_schema), a topic it does not
 *                  declare included, whatever version a save gave
 *                  __system__. What that replaces of the operator's
 *                  drafts goes into `replaced`, said by the caller.
 *      literal equal to the file, or behind it
 *                  the file runs, nothing is projected. Another content
 *                  under the same number is said, loudly; a literal
 *                  behind is said.
 *
 *  An UNFINISHED projection -- one that failed, and whose record
 *  (`unfinished_before`, see load_unfinished_record) says so -- is retried
 *  at every open, whatever path the open takes: projected again from what
 *  RUNS when nothing newer is installed (the literal when the file IS the
 *  literal, the file otherwise: a seed from the file that failed), and
 *  from the literal when one is installed or imposed. What it left, as
 *  it left it, is nobody's draft on any path (see leftovers_as_left); what
 *  the operator did meanwhile is, an edit of a leftover included, and a
 *  retry that replaces it says it.
 *
 *  With `imposing` the literal runs whatever the file says (unless it IS
 *  the file's version), and __system__ keeps the rule of the versions
 *  against itself: seeded when the treedb has none, re-made WHOLE when the
 *  literal is newer than __system__ (or the projection is unfinished),
 *  left as it is when it is not -- a dynamic change published there stays
 *  readable with diff-schema, and turning the flag off takes it back.
 *
 *  A treedb with no projection yet is seeded with what runs: the literal
 *  when it is installed or imposed, the FILE otherwise -- and then
 *  c_schema_version says the literal only when the file IS the literal,
 *  0 otherwise: the projection did not come from C.
 *
 *  `*p_projected` says whether a projection was written (upsert). -1 when
 *  it is not complete; what it could not do is in `unfinished`.
 *
 *  ONLY THE MASTER WRITES __system__, and its one caller,
 *  reconcile_treedb_schema(), holds the guard.
 ***************************************************************************/
PRIVATE int project_literal_into_system(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_schema,  // not owned, the literal
    BOOL imposing,      // the treedb is being opened with the schema from C
    json_t *file_in_use,// not owned, the schema file in use before this open, or NULL
    BOOL installed,     // treedb_open_db() writes the literal over the file
    json_t *unfinished_before, // not owned, the record of an unfinished projection, or NULL
    json_t *replaced,   // not owned, {topic: kind} of the drafts the literal replaced
    json_t *unfinished, // not owned, what the projection could not do (new_unfinished)
    BOOL *p_projected
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    *p_projected = FALSE;

    json_int_t new_version = schema_version_of(gobj, jn_schema);
    json_int_t in_use_version = schema_version_of(gobj, file_in_use);

    /*
     *  Ask with a list: it is silent when the treedb has no projection yet,
     *  which is the ordinary answer on a first open.
     */
    json_t *stored = gobj_list_nodes(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        0,
        gobj
    );
    if(json_array_size(stored) == 0) {
        JSON_DECREF(stored)
        BOOL from_file = (imposing || installed || !file_in_use)? FALSE : TRUE;
        json_t *seed = from_file? file_in_use : jn_schema;
        json_int_t c_stamp = new_version;
        if(from_file && (in_use_version != new_version || schemas_differ(file_in_use, jn_schema))) {
            c_stamp = 0;
        }
        *p_projected = TRUE;
        int ret = upsert_treedb_schema(
            gobj, treedb_name, seed, c_stamp, NULL, file_in_use, NULL, NULL, NULL, NULL,
            replaced, unfinished
        );
        if(from_file) {
            say_literal_not_installed(gobj, treedb_name, jn_schema, file_in_use, 0, c_stamp);
        }
        return ret;
    }

    json_t *stored_treedb = json_array_get(stored, 0);
    json_int_t stored_version = kw_get_int(
        gobj,
        stored_treedb,
        "schema_version",
        0,
        KW_WILD_NUMBER
    );
    json_int_t stored_meta = kw_get_int(
        gobj,
        stored_treedb,
        "system_schema_version",
        0,
        KW_WILD_NUMBER
    );
    json_int_t stored_c_version = kw_get_int(
        gobj,
        stored_treedb,
        "c_schema_version",
        0,
        KW_WILD_NUMBER
    );
    JSON_DECREF(stored)

    /*
     *  A projection that was never stamped: its node says 0 while the file
     *  in use has a version, and there is no record. That is a seed that
     *  died before its end (the node is created with 0 and stamped last,
     *  see upsert_treedb_schema): unfinished like one with a record, and
     *  nothing in it is anybody's draft. A complete projection of a file
     *  whose schema_version is 1 or more always stamps 1 or more.
     */
    BOOL never_stamped = (stored_version == 0 && in_use_version > 0 && !unfinished_before)?
        TRUE : FALSE;

    /*
     *  A projection written with an older meta-schema may still be keyed by
     *  rowid, and the qualified key cannot live beside it (see
     *  migrate_schema_ids_to_qualified). That is STRUCTURE, and it moves.
     *  Nothing is re-projected for a meta-schema change, though: the schema
     *  in use may be a dynamic one, and a projection of the literal would
     *  overwrite it.
     */
    if(stored_meta < priv->system_schema_version) {
        json_t *legacy = gobj_node_tree(
            priv->gobj_node_system,
            "treedbs",
            json_pack("{s:s}", "id", treedb_name),
            json_object(),
            gobj
        );
        if(!legacy) {
            return -1;  // Error already logged
        }
        int moved = migrate_schema_ids_to_qualified(gobj, treedb_name, legacy);
        JSON_DECREF(legacy)
        if(moved < 0) {
            return -1;  // Error already logged
        }
    }

    /*
     *  What is projected: the literal, unless an unfinished projection is
     *  completed from the file that runs
     */
    json_t *source = jn_schema;
    json_int_t source_c_version = new_version;
    BOOL completing = FALSE;

    if(imposing) {
        if(new_version <= stored_version && !unfinished_before) {
            if(new_version < stored_version) {
                gobj_log_info(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_INFO,
                    "msg",              "%s", "TreeDB schema from C is imposed, but it is behind __system__: the projection is kept",
                    "treedb_name",      "%s", treedb_name,
                    "schema_version",   "%d", (int)new_version,
                    "stored_version",   "%d", (int)stored_version,
                    NULL
                );
            }
            return 0;
        }

    } else if(!installed) {
        /*
         *  The file runs. __system__ keeps what it holds: the file, and
         *  the operator's drafts over it -- unless its projection was
         *  left unfinished, and then it is completed from what runs
         */
        if(!unfinished_before && !never_stamped) {
            say_literal_not_installed(
                gobj, treedb_name, jn_schema, file_in_use, stored_version, stored_c_version
            );
            return 0;
        }
        completing = TRUE;
        if(new_version != in_use_version || schemas_differ(file_in_use, jn_schema)) {
            source = file_in_use;
            source_c_version = 0;
        }
        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "Completing the projection into __system__, left unfinished by an earlier open",
            "treedb_name",      "%s", treedb_name,
            "why",              "%s", unfinished_before?
                "its record says so" : "never stamped (schema_version 0), no record",
            "source",           "%s", source == jn_schema? "schema from C" : "schema file in use",
            "schema_version",   "%d", (int)schema_version_of(gobj, source),
            "stored_version",   "%d", (int)stored_version,
            NULL
        );

    } else if(stored_c_version == new_version && stored_version == new_version &&
            !unfinished_before) {
        /*
         *  The projection is of this literal already: a schema file that
         *  was missing is written again from it, and the drafts over it
         *  are drafts over the same schema
         */
        return 0;

    } else if(!file_in_use) {
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_TREEDB,
            "msg",              "%s", "No schema file in use: the treedb opens with the schema from C, projected whole over __system__",
            "treedb_name",      "%s", treedb_name,
            "schema_version",   "%d", (int)new_version,
            "system_version",   "%d", (int)stored_version,
            NULL
        );
    }

    json_t *current = gobj_node_tree(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        json_object(),
        gobj
    );
    if(!current) {
        return -1;  // Error already logged
    }

    if(!completing) {
        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "Updating TreeDB schema in __system__",
            "treedb_name",      "%s", treedb_name,
            "schema_version",   "%d", (int)new_version,
            "stored_version",   "%d", (int)stored_version,
            "in_use_version",   "%d", (int)in_use_version,
            NULL
        );
    }

    /*
     *  The drafts: the topics of __system__ that differ from the schema
     *  file in use, which the projection replaces (said, see upsert), and
     *  the saved schema when a save of them is pending. What an unfinished
     *  projection left is no draft, and a projection never stamped holds
     *  none.
     */
    json_t *drafts = NULL;
    json_t *draft_ids = NULL;
    json_t *saved = NULL;
    if(file_in_use && !never_stamped) {
        drafts = drafts_over_file(gobj, treedb_name, file_in_use, unfinished_before, &draft_ids);

        char saved_dir[PATH_MAX];
        saved_schema_dir(gobj, saved_dir, sizeof(saved_dir));
        char filename[NAME_MAX];
        snprintf(filename, sizeof(filename), "%s.treedb_schema.json", treedb_name);
        if(file_exists(saved_dir, filename)) {
            saved = load_json_from_file(gobj, saved_dir, filename, 0);
            if(schema_version_of(gobj, saved) <= in_use_version) {
                JSON_DECREF(saved)  /*  not a pending save  */
            }
        }
    }

    *p_projected = TRUE;
    int ret = upsert_treedb_schema(
        gobj, treedb_name, source, source_c_version, current,
        file_in_use,
        drafts,
        draft_ids,
        saved,
        unfinished_before? json_object_get(unfinished_before, "draft_kinds") : NULL,
        replaced,
        unfinished
    );
    JSON_DECREF(saved)
    JSON_DECREF(drafts)
    JSON_DECREF(draft_ids)
    JSON_DECREF(current)

    if(completing && source != jn_schema) {
        say_literal_not_installed(
            gobj, treedb_name, jn_schema, file_in_use, stored_version, source_c_version
        );
    }

    return ret;
}

/***************************************************************************
 *  Remove the saved schema of a treedb, if there is one. `*p_version` is
 *  the schema_version it had, 0 when there was none. -1 when it could not
 *  be removed (logged).
 ***************************************************************************/
PRIVATE int remove_saved_schema(hgobj gobj, const char *treedb_name, json_int_t *p_version)
{
    *p_version = 0;

    char saved_dir[PATH_MAX];
    saved_schema_dir(gobj, saved_dir, sizeof(saved_dir));
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.treedb_schema.json", treedb_name);
    if(!file_exists(saved_dir, filename)) {
        return 0;
    }
    json_t *saved = load_json_from_file(gobj, saved_dir, filename, 0);
    *p_version = schema_version_of(gobj, saved);
    JSON_DECREF(saved)

    if(file_remove(saved_dir, filename) < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_SYSTEM,
            "msg",              "%s", "Cannot remove the saved schema",
            "treedb_name",      "%s", treedb_name,
            "directory",        "%s", saved_dir,
            "filename",         "%s", filename,
            "errno",            "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  A literal installed over the file hands tranger2 every topic, and
 *  tranger2 installs one only over a LOWER topic_version (or a different
 *  one, imposing): a topic the literal changes without raising its
 *  topic_version goes on running the columns of its topic_cols.json, while
 *  the file and __system__ say the literal. That is the classic change
 *  that reaches nothing, and it is said, as a warning, per topic.
 ***************************************************************************/
PRIVATE void warn_topics_not_raised(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_schema,  // not owned, the literal
    BOOL imposing
)
{
    json_t *topics = schema_topics_as_list(gobj, jn_schema);
    int idx; json_t *topic;
    json_array_foreach(topics, idx, topic) {
        const char *topic_name = kw_get_str(gobj, topic, "id", "", 0);
        if(empty_string(topic_name)) {
            topic_name = kw_get_str(gobj, topic, "topic_name", "", 0);
        }
        if(empty_string(topic_name)) {
            continue;
        }
        json_int_t running_version = running_topic_version(gobj, treedb_name, topic_name);
        json_int_t topic_version = kw_get_int(gobj, topic, "topic_version", 1, KW_WILD_NUMBER);
        if(running_version == 0) {
            continue;   /*  a topic the store never opened: created from the literal  */
        }
        if(imposing? (topic_version != running_version) : (topic_version > running_version)) {
            continue;   /*  tranger2 installs it  */
        }

        char directory[PATH_MAX];
        build_path(directory, sizeof(directory),
            gobj_read_str_attr(gobj, "path"), treedb_name, topic_name, NULL);
        if(!file_exists(directory, "topic_cols.json")) {
            continue;
        }
        json_t *running_cols = load_json_from_file(gobj, directory, "topic_cols.json", 0);
        json_t *running = json_pack("{s:[{s:s, s:o}]}",
            "topics", "id", topic_name, "cols", running_cols? running_cols : json_object());
        if(schema_topic_differs(running, jn_schema, topic_name, TRUE)) {
            gobj_log_warning(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_TREEDB,
                "msg",              "%s", "Topic from C declares other columns than the store runs, without raising its topic_version past it: the store keeps running its own",
                "treedb_name",      "%s", treedb_name,
                "topic_name",       "%s", topic_name,
                "topic_version",    "%d", (int)topic_version,
                "running_version",  "%d", (int)running_version,
                NULL
            );
        }
        JSON_DECREF(running)
    }
    JSON_DECREF(topics)
}

/***************************************************************************
 *  Keep the __system__ projection in step with what the treedb runs
 *  (project_literal_into_system), and retire what the open makes stale.
 *
 *  The literal is INSTALLED -- treedb_open_db() writes it over the whole
 *  schema file -- when there is no file, when it is newer (impose off), or
 *  when its version is another one (imposing). Then everything the
 *  operator had over the old file is withdrawn:
 *
 *      applied  an apply that never ran (read_apply_record) whose topic
 *               the literal says otherwise, or does not declare;
 *      in_use   the same, of an apply that RAN: the dynamic schema the
 *               treedb was running;
 *      saved    the saved schema (published against the file that goes)
 *               and the draft of each topic it carried;
 *      unsaved  a draft never saved.
 *
 *  It is said, ONE warning naming the treedb and the topics, and kept in
 *  `jn_withdrawn_at_open` for the API: `treedbs` and `saved-schema` answer
 *  it as `withdrawn_at_open` until the next open of the treedb. A save
 *  taken back (the draft is the file again) is nothing withdrawn. A topic
 *  keeps the first kind it is given, except "applied", which says more.
 *
 *  Whether the literal is installed is what treedb_open_db() will do, and
 *  that is decided by the CLIENT tranger: when another process holds its
 *  store it opens as a replica and runs its file as it is. Then nothing is
 *  reconciled, as on a replica: __system__ said the literal while the file
 *  ran, when this was decided with the lock of __system__.
 *
 *  ONLY THE MASTER writes __system__ and saved_schemas/.
 ***************************************************************************/
PRIVATE int reconcile_treedb_schema(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_schema,      // not owned
    BOOL imposing,          // the treedb is being opened with the schema from C
    json_t *tranger_client  // not owned, the tranger the treedb opens on
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The tranger's own flag, not the attribute: a master that could not
     *  take the store in exclusive opens as a replica (timeranger2.c), and
     *  what decides whether a write lands is what it ended up being.
     */
    json_object_del(priv->jn_withdrawn_at_open, treedb_name);
    json_object_del(priv->jn_apply_record_at_open, treedb_name);
    if(!tranger_writes_now(gobj, priv->tranger_system_)) {
        return 0;
    }
    if(!tranger_writes_now(gobj, tranger_client)) {
        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INFO,
            "msg",              "%s", "The store of the treedb is not written here: it opens as a replica and runs its schema file, __system__ is not reconciled",
            "treedb_name",      "%s", treedb_name,
            NULL
        );
        return 0;
    }

    json_int_t new_version = schema_version_of(gobj, jn_schema);
    json_t *file_in_use = load_schema_file_in_use(gobj, treedb_name);
    json_int_t in_use_version = schema_version_of(gobj, file_in_use);
    BOOL installed = (!file_in_use ||
        (imposing? new_version != in_use_version : new_version > in_use_version))? TRUE: FALSE;

    /*
     *  The dynamic topics of the file in use: settled once the open opens
     */
    json_t *record_topics = read_apply_record(gobj, treedb_name, in_use_version);

    /*
     *  The record of an unfinished projection: written when this one fails,
     *  removed when it succeeds; kept when nothing is projected
     */
    json_t *unfinished_before = load_unfinished_record(gobj, treedb_name);
    json_t *replaced = json_object();
    json_t *unfinished = new_unfinished(0);
    BOOL projected = FALSE;
    int ret = project_literal_into_system(
        gobj, treedb_name, jn_schema, imposing, file_in_use, installed,
        unfinished_before, replaced, unfinished, &projected
    );
    if(projected) {
        if(ret < 0) {
            keep_leftover_nodes(gobj, treedb_name, unfinished);
            write_unfinished_record(gobj, treedb_name, unfinished);    // Error already logged
        } else if(unfinished_before) {
            remove_unfinished_record(gobj, treedb_name);    // Error already logged
        }
    }
    JSON_DECREF(unfinished)
    JSON_DECREF(unfinished_before)

    json_int_t saved_version = 0;
    if(installed) {
        const char *topic_name; json_t *jn_kind;
        json_object_foreach(record_topics, topic_name, jn_kind) {
            const char *kind = json_string_value(jn_kind);
            if(!kind || !schema_topic_differs(file_in_use, jn_schema, topic_name, FALSE)) {
                continue;
            }
            if(strcmp(kind, "applied")==0 || !json_object_get(replaced, topic_name)) {
                json_object_set_new(replaced, topic_name, json_string(kind));
            }
        }
        if(record_topics) {
            json_object_set_new(priv->jn_apply_record_at_open, treedb_name,
                json_pack("{s:s}", "action", "remove"));
        }

        if(remove_saved_schema(gobj, treedb_name, &saved_version) < 0) {
            saved_version = 0;  // it could not be removed (logged): not withdrawn
        }

        warn_topics_not_raised(gobj, treedb_name, jn_schema, imposing);

    } else if(json_object_size(record_topics) > 0 && record_has_kind(record_topics, "applied")) {
        json_object_set_new(priv->jn_apply_record_at_open, treedb_name,
            json_pack("{s:s, s:I, s:O}",
                "action", "ran",
                "schema_version", in_use_version,
                "topics", record_topics
            )
        );
    }

    if(saved_version > 0 || json_object_size(replaced) > 0) {
        gobj_log_warning(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB,
            "msg",                  "%s", "Schema from C withdrew work on the schema at open",
            "treedb_name",          "%s", treedb_name,
            "schema_version",       "%d", (int)new_version,
            "in_use_version",       "%d", (int)in_use_version,
            "saved_schema_version", "%d", (int)saved_version,
            "topics",               "%j", replaced,
            NULL
        );
        json_object_set_new(priv->jn_withdrawn_at_open, treedb_name, json_pack("{s:I, s:I, s:O}",
            "schema_version", new_version,
            "saved_schema_version", saved_version,
            "topics", replaced
        ));
    }
    JSON_DECREF(replaced)
    JSON_DECREF(record_topics)
    JSON_DECREF(file_in_use)
    return ret;
}

/***************************************************************************
 *  The names a schema DECLARES, in the order it declares them: the topics
 *  of the treedb when `topic_name` is NULL, the columns of that topic
 *  otherwise.
 *
 *  Return a list of names, or NULL. Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *declared_names(
    hgobj gobj,
    json_t *c_schema,       // not owned, the schema compiled in C, or NULL
    const char *topic_name  // NULL for the topic names themselves
)
{
    json_t *jn_topics = c_schema? kw_get_list(gobj, c_schema, "topics", 0, 0): NULL;
    if(!jn_topics) {
        return NULL;
    }

    json_t *names = json_array();

    int idx; json_t *jn_topic;
    json_array_foreach(jn_topics, idx, jn_topic) {
        const char *name = kw_get_str(gobj, jn_topic, "id", "", 0);
        if(empty_string(name)) {
            name = kw_get_str(gobj, jn_topic, "topic_name", "", 0);
        }
        if(empty_string(name)) {
            continue;
        }
        if(!topic_name) {
            json_array_append_new(names, json_string(name));
            continue;
        }
        if(strcmp(name, topic_name)!=0) {
            continue;
        }

        json_t *jn_cols = kwid_new_list(gobj, jn_topic, 0, "cols");
        int idx2; json_t *jn_col;
        json_array_foreach(jn_cols, idx2, jn_col) {
            const char *col_name = kw_get_str(gobj, jn_col, "id", "", 0);
            if(!empty_string(col_name)) {
                json_array_append_new(names, json_string(col_name));
            }
        }
        JSON_DECREF(jn_cols)
        break;
    }

    return names;
}

/***************************************************************************
 *  Put the nodes of a rebuilt schema back in the order they were WRITTEN
 *  in, and take the index away again: `order` says where a node belongs
 *  while it is stored as a record, and a schema says the same thing by the
 *  sequence of its dict. Left in, it would reach every topic as a column
 *  attribute nobody declared.
 *
 *  A node with no `order` -- one an operator added, or one projected before
 *  the index existed -- falls back to where the schema compiled in C
 *  declares it, and goes last when C does not know it either. The sort is
 *  stable, so nodes that answer the same keep the order they arrived in.
 *
 *  Return a new dict holding the same nodes. Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *order_schema_nodes(
    json_t *nodes,      // not owned, {name: node}
    json_t *declared    // not owned, [name, ...] as declared in C, or NULL
)
{
    json_t *sorted = json_array();  // of [order, name]

    const char *name; json_t *node;
    json_object_foreach(nodes, name, node) {
        json_int_t order;
        json_t *jn_order = json_object_get(node, "order");
        if(json_is_integer(jn_order)) {
            order = json_integer_value(jn_order);
        } else {
            order = INT_MAX;
            int idx; json_t *jn_name;
            json_array_foreach(declared, idx, jn_name) {
                if(strcmp(json_string_value(jn_name), name)==0) {
                    order = idx;
                    break;
                }
            }
        }

        size_t pos = json_array_size(sorted);
        while(pos > 0) {
            json_t *prev = json_array_get(sorted, pos-1);
            if(json_integer_value(json_array_get(prev, 0)) <= order) {
                break;
            }
            pos--;
        }
        json_array_insert_new(sorted, pos, json_pack("[I,s]", order, name));
    }

    json_t *ordered = json_object();
    int idx; json_t *entry;
    json_array_foreach(sorted, idx, entry) {
        const char *node_name = json_string_value(json_array_get(entry, 1));
        json_t *node_ = json_object_get(nodes, node_name);
        json_object_del(node_, "order");
        json_object_set(ordered, node_name, node_);
    }
    JSON_DECREF(sorted)

    return ordered;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_treedb_schema(
    hgobj gobj,
    const char *treedb_name
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  A treedb opened for the first time has no schema here yet, and that
     *  is an ordinary answer, not a failure: ask with a list (silent when
     *  empty) instead of letting gobj_node_tree log a missing node.
     */
    json_t *found = gobj_list_nodes(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        0,
        gobj
    );
    size_t found_size = json_array_size(found);
    JSON_DECREF(found)
    if(found_size == 0) {
        return 0;
    }

    json_t *treedb = gobj_node_tree(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        json_object(),
        gobj
    );
    if(!treedb) {
        return 0;   // Error already logged
    }

    /*
     *  HACK Both `topics` and `cols` are keyed by their qualified name and
     *  carry the bare one in `value` (a name is unique only inside its
     *  parent), so the schema is re-keyed by the bare name on the way out —
     *  that is the shape a schema has.
     *
     *  And re-ORDERED on the way out too: the nodes come back in the order
     *  the store happens to hold them, while in a schema the order of the
     *  columns is what a table paints. The schema compiled in C is the
     *  fallback for whatever the projection cannot place by itself.
     */
    json_t *c_schema = json_object_get(priv->jn_c_schemas, treedb_name);

    json_t *stored_topics = kw_get_dict(gobj, treedb, "topics", 0, KW_EXTRACT);
    json_t *topics = json_object();

    const char *stored_topic_id; json_t *topic;
    json_object_foreach(stored_topics, stored_topic_id, topic) {
        const char *topic_name = kw_get_str(gobj, topic, "value", 0, KW_REQUIRED);
        if(empty_string(topic_name)) {
            continue;
        }
        json_object_set(topics, topic_name, topic);
        json_object_set_new(topic, "id", json_string(topic_name));
        json_object_del(topic, "value");

        json_t *cols = kw_get_dict(gobj, topic, "cols", 0, KW_EXTRACT|KW_REQUIRED);
        if(!cols) {
            continue;
        }
        /*
         *  TODO delete fkey's
         */
        json_object_del(topic, "treedbs");

        /*
         *  HACK A column is addressed by its qualified id and named by its
         *  pkey2, so the schema is re-keyed by the name on the way out.
         */
        json_t *new_cols = json_object();
        const char *col_name; json_t *col;
        json_object_foreach(cols, col_name, col) {
            const char *value = kw_get_str(gobj, col, "value", 0, KW_REQUIRED);
            if(empty_string(value)) {
                continue;
            }

            /*
             *  TODO delete fkey's
             */
            json_object_del(col, "topics");

            /*
             *  Add new column, by pkey2
             */
            json_object_set(new_cols, value, col);
            json_object_set_new(col, "id", json_string(value));
            json_object_del(col, "value");

            /*
             *  HACK a column with no `default` is stored with an EMPTY one:
             *  the attribute is a blob, and a blob with no value is {}. Left
             *  in the rebuilt schema that empty dict becomes a real default,
             *  and creating a record without that field then hands {} to a
             *  string column — "Value must be string", from a schema nobody
             *  wrote that way.
             *
             *  Only for a column that cannot HOLD a container, though: `[]` is
             *  a legitimate default for an array column, and mqtt_broker's
             *  publish_acl/subscribe_acl declare exactly that.
             */
            const char *col_type = kw_get_str(gobj, col, "type", "", 0);
            BOOL holds_container =
                strcmp(col_type, "dict")==0 || strcmp(col_type, "object")==0 ||
                strcmp(col_type, "array")==0 || strcmp(col_type, "list")==0 ||
                strcmp(col_type, "blob")==0;

            json_t *jn_default = json_object_get(col, "default");
            if(!holds_container && jn_default &&
                (json_is_object(jn_default) || json_is_array(jn_default)) &&
                json_object_size(jn_default)==0 && json_array_size(jn_default)==0
            ) {
                json_object_del(col, "default");
            }
        }

        /*
         *  Set new checked cols, in the order they were declared in.
         *
         *  The name comes from `id` and not from `topic_name`: that one
         *  points into the `value` deleted above, and the string it aims at
         *  may be gone by now.
         */
        json_t *declared_cols = declared_names(
            gobj, c_schema, kw_get_str(gobj, topic, "id", "", 0)
        );
        json_object_set_new(topic, "cols", order_schema_nodes(new_cols, declared_cols));
        JSON_DECREF(declared_cols)
        JSON_DECREF(new_cols)

        json_decref(cols);
    }
    JSON_DECREF(stored_topics)

    json_t *declared_topics = declared_names(gobj, c_schema, NULL);
    json_object_set_new(treedb, "topics", order_schema_nodes(topics, declared_topics));
    JSON_DECREF(declared_topics)
    JSON_DECREF(topics)

    return treedb;
}

/***************************************************************************
 *  Is `gobj_node` a treedb that THIS service opened with open-treedb?
 *
 *  close-treedb, create-topic and delete-topic take a name from the wire
 *  and act on the service it names. Found by name alone it could be any
 *  service of the yuno: this service's own __system__ treedb, whose
 *  handles live in priv (closing it left priv pointing at freed memory),
 *  a C_TRANGER, anybody's C_NODE. What open-treedb opens is a C_NODE whose
 *  parent is this service, and nothing else is ours to close or change.
 ***************************************************************************/
PRIVATE BOOL is_treedb_opened_here(
    hgobj gobj,
    hgobj gobj_node
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_node || gobj_node == priv->gobj_node_system) {
        return FALSE;
    }
    if(!gobj_typeof_gclass(gobj_node, C_NODE)) {
        return FALSE;
    }
    return (gobj_parent(gobj_node) == gobj)? TRUE : FALSE;
}

/***************************************************************************
 *  The response a write command gives when the tranger it would write is
 *  a replica's. Same words as C_NODE's: on a replica nobody can write,
 *  and the library refuses it anyway -- this says why, up front, before
 *  anything is closed or unlinked in memory on the way to that refusal.
 ***************************************************************************/
PRIVATE json_t *build_readonly_response(
    hgobj gobj,
    const char *treedb_name,
    json_t *tranger,    // not owned, the tranger that refuses, may be NULL
    json_t *kw          // owned
)
{
    if(tranger_is_stopped(tranger)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: treedb '%s' is STOPPED: its tranger holds no lock until its "
                "service starts again",
                gobj_yuno_role_plus_name(),
                treedb_name
            ),
            0,
            0,
            kw  // owned
        );
    }
    return msg_iev_build_response(
        gobj,
        -1,
        json_sprintf("%s: treedb '%s' is READ-ONLY, this yuno is not the master of its tranger",
            gobj_yuno_role_plus_name(),
            treedb_name
        ),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_client_treedb_schema(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_client_treedb_schema, // not owned
    json_t *tranger_client          // not owned, the tranger the treedb opens on
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Check input schema although is not used
     */
    BOOL input_schema_ok = TRUE;
    if(parse_schema(jn_client_treedb_schema)<0) {
        input_schema_ok = FALSE;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB,
            "msg",          "%s", "Input Schema fails",
            NULL
        );
        gobj_trace_json(gobj, jn_client_treedb_schema, "Input Schema fails");
    }

    /*
     *  The schema compiled in C also exists as data in the __system__
     *  treedb: that is what makes it listable and editable through the
     *  ordinary node commands. Project it the first time this treedb is
     *  seen, and afterwards only when the literal or the projector moved
     *  ahead — the version is what decides, so an edit made there survives
     *  every start until a higher `schema_version` arrives from C.
     *
     *  On a replica nothing is projected: it reads what the master wrote.
     */
    if(input_schema_ok) {
        /*
         *  Keep it: once projected it is gone, and it is the only thing that
         *  can tell an edit made in __system__ from what C declares.
         */
        json_object_set(priv->jn_c_schemas, treedb_name, jn_client_treedb_schema);

        reconcile_treedb_schema(gobj, treedb_name, jn_client_treedb_schema, FALSE, tranger_client);
    }

    /*
     *  With impose_c_schema off, a treedb opens from its schema FILE (with
     *  it on, get_c_schema_to_impose() opens it from the literal, over the
     *  file). The literal is handed to treedb_open_db() as it is, without
     *  `impose`: it is installed only when it is newer than the file, and
     *  then over the WHOLE file -- the rule the user decided on 2026-09-23,
     *  as it was in 7.25.4. The file wins on ties and when it is ahead,
     *  which is what apply-schema makes it.
     *
     *  __system__ is not read here. It is where a schema is EDITED: a draft
     *  there reaches a treedb only through save-schema + apply-schema (the
     *  owner's design of M36, 2026-09-21 review). It used to be the source,
     *  so every edit, half made or not, was the schema of the next start.
     */
    json_t *client_treedb_schema = json_incref(jn_client_treedb_schema);

    if(parse_schema(client_treedb_schema)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB,
            "msg",          "%s", "Schema fails",
            NULL
        );
        gobj_trace_json(gobj, client_treedb_schema, "Schema fails");
        json_decref(client_treedb_schema);
        return 0;
    }

    return client_treedb_schema;
}

/***************************************************************************
 *  The schema from C, for a treedb opened with `impose_c_schema`.
 *
 *  __system__ is not READ -- the treedb opens from the literal -- and it is
 *  written under the rule of the versions: it is seeded when the treedb has
 *  no projection yet, and re-made when the literal is strictly newer than
 *  what is stored. A dynamic change publishes itself by raising the version,
 *  so it is left where it is: that is what diff-schema reads, and what
 *  clearing the flag takes back.
 *
 *  It is written at all because __system__ is the only place a schema can be
 *  ASKED for -- from ytreedb, from gui_agent, from any node command. A
 *  treedb that only ever opened with `impose` had none, so the schema it
 *  runs could be read from its binary and nowhere else.
 *
 *  Written by the MASTER, that is. A replica reads the treedb from disk as
 *  it is at that moment and writes nothing.
 *
 *  Return the schema, or NULL. Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *get_c_schema_to_impose(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_c_schema,    // not owned
    json_t *tranger_client  // not owned, the tranger the treedb opens on
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!jn_c_schema || parse_schema(jn_c_schema)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB,
            "msg",          "%s", "Schema from C to impose fails",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        return NULL;
    }

    json_object_set(priv->jn_c_schemas, treedb_name, jn_c_schema);

    gobj_log_info(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_INFO,
        "msg",              "%s", "Opening TreeDB with the schema from C, __system__ not read",
        "treedb_name",      "%s", treedb_name,
        "schema_version",   "%d", (int)kw_get_int(gobj, jn_c_schema, "schema_version", 0, KW_WILD_NUMBER),
        NULL
    );

    reconcile_treedb_schema(gobj, treedb_name, jn_c_schema, TRUE, tranger_client);

    return json_incref(jn_c_schema);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int delete_client_treedb_schema(
    hgobj gobj,
    const char *treedb_name
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *treedb = gobj_node_tree(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        0,
        gobj
    );
    if(!treedb) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB,
            "msg",          "%s", "Treedb schema not projected in __system__",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        return -1;
    }

    int ret = 0;

    /*
     *  The PARENT first, and it is not an oversight: with `force`,
     *  treedb_delete_node() unlinks every child itself, so the topics
     *  below are orphans by the time this loop reaches them -- and a
     *  delete addresses a node by its `id`, which is all `mt_delete_node`
     *  reads of the collapsed view a tree hands it before re-resolving
     *  the pure node from the index.
     */
    ret += gobj_delete_node(
        priv->gobj_node_system,
        "treedbs",
        json_incref(treedb),
        json_pack("{s:b}", "force", 1),
        gobj
    );

    json_t *topics = kw_get_dict(gobj, treedb, "topics", 0, 0);
    const char *topic_name; json_t *topic;
    json_object_foreach(topics, topic_name, topic) {
        ret += gobj_delete_node(
            priv->gobj_node_system,
            "topics",
            json_incref(topic),
            json_pack("{s:b}", "force", 1),
            gobj
        );
        json_t *cols = kw_get_dict(gobj, topic, "cols", 0, KW_REQUIRED);
        if(!cols) {
            continue;
        }
        const char *col_name; json_t *col;
        json_object_foreach(cols, col_name, col) {
            ret += gobj_delete_node(
                priv->gobj_node_system,
                "cols",
                json_incref(col),
                json_pack("{s:b}", "force", 1),
                gobj
            );
        }
    }

    json_decref(treedb);

    return ret;
}

/***************************************************************************
 *  Is this value the same thing as declaring nothing?
 *
 *  A treedb record carries EVERY column of its topic, filled with the empty
 *  value of its type. So an attribute the schema does not declare is stored
 *  as "", {}, [], false or 0, and the store cannot tell that from one
 *  written empty on purpose. Read as differences, those 586 defaults buried
 *  the 6 that somebody made.
 ***************************************************************************/
PRIVATE BOOL is_unset_value(json_t *value)  // not owned, may be NULL
{
    if(json_absent(value)) {
        return TRUE;
    }
    if(json_is_string(value)) {
        return empty_string(json_string_value(value));
    }
    if(json_is_object(value) || json_is_array(value)) {
        return empty_json(value);
    }
    if(json_is_boolean(value)) {
        return json_is_true(value)?FALSE:TRUE;
    }
    if(json_is_integer(value)) {
        return json_integer_value(value)==0?TRUE:FALSE;
    }
    if(json_is_real(value)) {
        return json_real_value(value)==0.0?TRUE:FALSE;
    }

    return FALSE;
}

/***************************************************************************
 *  Is this stored value just the default the descriptor declares?
 *
 *  The projection of a column copies the attributes the schema DECLARES and
 *  no more, while the stored node went through treedb, which fills every
 *  attribute the descriptor gives a `default`. So an attribute the schema
 *  never mentions is absent on one side and holds its default on the other,
 *  and the comparison reads that as an operator addition.
 *
 *  It is not one, and there were 43 of them in a treedb of 45 columns:
 *  `fillspace` defaults to 10 and almost no schema writes it, so the panel
 *  answered 43 differences that no Apply could ever settle -- and buried the
 *  ones somebody did make, which is the same failure is_unset_value() was
 *  written for. That one only knows the EMPTY value of a type; this knows
 *  the DECLARED default, which is a different thing and the one that bit.
 *
 *  The projection is deliberately NOT filled with defaults instead: it is
 *  what the projector upserts, so a default written there would overwrite
 *  the value an operator set by hand on an attribute the schema does not
 *  declare. The asymmetry is real and belongs in the comparison.
 ***************************************************************************/
PRIVATE BOOL is_declared_default(
    hgobj gobj,
    json_t *desc,           // not owned, the descriptor list, may be NULL
    const char *attr,
    json_t *value           // not owned
)
{
    if(!desc || !json_is_array(desc)) {
        return FALSE;
    }

    int idx; json_t *entry;
    json_array_foreach(desc, idx, entry) {
        if(strcmp(kw_get_str(gobj, entry, "id", "", 0), attr)!=0) {
            continue;
        }
        json_t *def = json_object_get(entry, "default");
        if(!def) {
            return FALSE;   /*  nothing declared, so nothing to forgive  */
        }
        return json_equal(def, value)?TRUE:FALSE;
    }

    return FALSE;
}

/***************************************************************************
 *  Append one difference.
 ***************************************************************************/
PRIVATE int add_diff_row(
    json_t *rows,           // not owned
    const char *treedb_name,
    const char *kind,       // changed | only_in_stored | only_in_c | version
    const char *topic_name,
    const char *col_name,   // "" at topic level
    const char *attr,       // "" when the whole topic/column is the difference
    json_t *stored,         // not owned, NULL when there is nothing stored
    json_t *from_c          // not owned, NULL when C does not declare it
)
{
    json_t *row = json_object();

    json_object_set_new(row, "treedb", json_string(treedb_name));
    json_object_set_new(row, "kind", json_string(kind));
    json_object_set_new(row, "topic", json_string(topic_name?topic_name:""));
    json_object_set_new(row, "col", json_string(col_name?col_name:""));
    json_object_set_new(row, "attr", json_string(attr?attr:""));
    json_object_set(row, "stored", stored?stored:json_null());
    json_object_set(row, "from_c", from_c?from_c:json_null());

    json_array_append_new(rows, row);

    return 0;
}

/***************************************************************************
 *  Compare the attributes of one projected node with the stored one.
 *
 *  Both sides are compared, so an attribute added in __system__ that C does
 *  not declare is reported too: that is what an operator edit looks like.
 ***************************************************************************/
PRIVATE int diff_node_attrs(
    hgobj gobj,
    json_t *rows,           // not owned
    const char *treedb_name,
    const char *topic_name,
    const char *col_name,   // NULL at topic level
    json_t *projected,      // not owned
    json_t *stored,         // not owned
    const char **skip,      // attributes that say how a node is STORED
    json_t *desc            // not owned, descriptor of the node's topic, may be NULL
)
{
    const char *attr; json_t *v;

    json_object_foreach(projected, attr, v) {
        if(str_in_list(skip, attr, TRUE)) {
            continue;
        }
        json_t *stored_value = json_object_get(stored, attr);
        if(is_unset_value(v) && is_unset_value(stored_value)) {
            continue;
        }
        if(!stored_value) {
            add_diff_row(
                rows, treedb_name, "only_in_c", topic_name, col_name, attr, NULL, v
            );
            continue;
        }
        if(!json_equal(stored_value, v)) {
            add_diff_row(
                rows, treedb_name, "changed", topic_name, col_name, attr, stored_value, v
            );
        }
    }

    json_object_foreach(stored, attr, v) {
        if(str_in_list(skip, attr, TRUE)) {
            continue;
        }
        if(json_object_get(projected, attr) || is_unset_value(v)) {
            continue;
        }
        if(is_declared_default(gobj, desc, attr, v)) {
            continue;   /*  the store filling a default is nobody's edit  */
        }
        add_diff_row(
            rows, treedb_name, "only_in_stored", topic_name, col_name, attr, v, NULL
        );
    }

    return 0;
}

/***************************************************************************
 *  What the __system__ projection of a treedb says that its schema from C
 *  does not.
 *
 *  What the projection holds and the schema does not is an operator's draft
 *  (a literal that wins is projected WHOLE, so nothing of an older literal
 *  stays) -- invisible until somebody compares the two. That is this function: it projects the schema
 *  from C in memory, with the same builders the projector uses, and compares
 *  node by node.
 *
 *  The version stamps are NOT compared as content: they are raised by
 *  whoever publishes a change (the literal, or an edit made in __system__),
 *  so after an edit they differ by design and would bury the differences
 *  somebody actually made. What is reported is the anomaly: a projection
 *  that came from a release of the schema other than the one running, and a
 *  topic whose stored version is BEHIND the schema's — the literal raised it,
 *  but its treedb's `schema_version` was not, so it was never published.
 *
 *  Return the summary, or NULL. Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *diff_treedb_schema(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_schema,  // the schema from C, not owned
    json_t *rows        // not owned, where the differences are appended
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t c_schema_version = kw_get_int(gobj, jn_schema, "schema_version", 1, KW_WILD_NUMBER);

    /*
     *  Ask with a list: a treedb with no projection yet is an ordinary
     *  answer, not a failure.
     */
    json_t *found = gobj_list_nodes(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        0,
        gobj
    );
    size_t found_size = json_array_size(found);
    JSON_DECREF(found)
    if(found_size == 0) {
        return json_pack("{s:s, s:b, s:I}",
            "treedb", treedb_name,
            "projected", 0,
            "c_schema_version_in_c", (json_int_t )c_schema_version
        );
    }

    json_t *current = gobj_node_tree(
        priv->gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        json_object(),
        gobj
    );
    if(!current) {
        return NULL;    // Error already logged
    }

    json_int_t stored_schema_version = kw_get_int(
        gobj, current, "schema_version", 0, KW_WILD_NUMBER
    );
    json_int_t stored_c_schema_version = kw_get_int(
        gobj, current, "c_schema_version", 0, KW_WILD_NUMBER
    );
    json_int_t stored_system_schema_version = kw_get_int(
        gobj, current, "system_schema_version", 0, KW_WILD_NUMBER
    );

    /*
     *  The projection came from another release of the schema than the one
     *  this yuno runs: everything else reported below may be that, and not
     *  an edit.
     */
    if(stored_c_schema_version != c_schema_version) {
        json_t *stored_value = json_integer(stored_c_schema_version);
        json_t *from_c = json_integer(c_schema_version);
        add_diff_row(
            rows, treedb_name, "version", "", "", "c_schema_version", stored_value, from_c
        );
        json_decref(stored_value);
        json_decref(from_c);
    }

    json_t *current_topics = kw_get_dict(gobj, current, "topics", 0, 0);
    json_t *cols_desc = _treedb_create_topic_cols_desc();
    json_t *seen_topics = json_object();

    json_t *jn_topics = schema_topics_as_list(gobj, jn_schema);  // yours
    int idx; json_t *jn_topic;
    json_array_foreach(jn_topics, idx, jn_topic) {
        const char *topic_name = kw_get_str(gobj, jn_topic, "id", "", 0);
        if(empty_string(topic_name)) {
            topic_name = kw_get_str(gobj, jn_topic, "topic_name", "", KW_REQUIRED);
        }
        if(empty_string(topic_name)) {
            continue;   // Error already logged
        }
        json_object_set_new(seen_topics, topic_name, json_true());

        json_int_t topic_version = kw_get_int(gobj, jn_topic, "topic_version", 1, KW_WILD_NUMBER);
        json_t *projected_topic = build_topic_projection(
            gobj, jn_topic, topic_name, topic_version, idx
        );
        if(!projected_topic) {
            continue;   // Error already logged
        }

        json_t *stored_topic = find_node_by_name(gobj, current_topics, topic_name);
        if(!stored_topic) {
            add_diff_row(rows, treedb_name, "only_in_c", topic_name, "", "", NULL, NULL);
            json_decref(projected_topic);
            continue;
        }

        /*
         *  No descriptor at topic level, and none is needed:
         *  build_topic_projection() applies the topic defaults itself
         *  (pkey, tkey, order), so the two sides are already symmetric.
         */
        diff_node_attrs(
            gobj, rows, treedb_name, topic_name, NULL, projected_topic, stored_topic,
            schema_topic_skip, NULL
        );

        /*
         *  A stored topic_version BEHIND the schema's means the projection
         *  never reached this topic; ahead is what a dynamic edit does.
         */
        json_int_t stored_topic_version = kw_get_int(
            gobj, stored_topic, "topic_version", 0, KW_WILD_NUMBER
        );
        if(stored_topic_version < topic_version) {
            json_t *stored_value = json_integer(stored_topic_version);
            json_t *from_c = json_integer(topic_version);
            add_diff_row(
                rows, treedb_name, "version", topic_name, "", "topic_version",
                stored_value, from_c
            );
            json_decref(stored_value);
            json_decref(from_c);
        }

        json_decref(projected_topic);

        json_t *stored_cols = kw_get_dict(gobj, stored_topic, "cols", 0, 0);
        json_t *seen_cols = json_object();

        json_t *jn_cols = kwid_new_list(gobj, jn_topic, 0, "cols");
        int idx2; json_t *jn_col;
        json_array_foreach(jn_cols, idx2, jn_col) {
            json_t *projected_col = build_col_projection(gobj, jn_col, cols_desc, idx2);
            if(!projected_col) {
                continue;   // Error already logged
            }
            const char *col_name = json_string_value(json_object_get(projected_col, "value"));
            json_object_set_new(seen_cols, col_name, json_true());

            json_t *stored_col = find_node_by_name(gobj, stored_cols, col_name);
            if(!stored_col) {
                add_diff_row(
                    rows, treedb_name, "only_in_c", topic_name, col_name, "", NULL, NULL
                );
                json_decref(projected_col);
                continue;
            }

            diff_node_attrs(
                gobj,
                rows,
                treedb_name,
                topic_name,
                col_name,
                projected_col,
                stored_col,
                schema_col_skip,
                cols_desc
            );
            json_decref(projected_col);
        }
        JSON_DECREF(jn_cols)

        const char *stored_col_id; json_t *stored_col;
        json_object_foreach(stored_cols, stored_col_id, stored_col) {
            const char *col_name = kw_get_str(gobj, stored_col, "value", "", 0);
            if(empty_string(col_name) || json_object_get(seen_cols, col_name)) {
                continue;
            }
            add_diff_row(
                rows, treedb_name, "only_in_stored", topic_name, col_name, "", NULL, NULL
            );
        }
        JSON_DECREF(seen_cols)
    }

    const char *stored_topic_id; json_t *stored_topic;
    json_object_foreach(current_topics, stored_topic_id, stored_topic) {
        const char *topic_name = kw_get_str(gobj, stored_topic, "value", "", 0);
        if(empty_string(topic_name) || json_object_get(seen_topics, topic_name)) {
            continue;
        }
        add_diff_row(rows, treedb_name, "only_in_stored", topic_name, "", "", NULL, NULL);
    }

    json_t *summary = json_pack("{s:s, s:b, s:I, s:I, s:I, s:I, s:I}",
        "treedb", treedb_name,
        "projected", 1,
        "schema_version", (json_int_t )stored_schema_version,
        "c_schema_version", (json_int_t )stored_c_schema_version,
        "system_schema_version", (json_int_t )stored_system_schema_version,
        "c_schema_version_in_c", (json_int_t )c_schema_version,
        "system_schema_version_in_c", (json_int_t )priv->system_schema_version
    );
    if(!summary) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON,
            "msg",          "%s", "json_pack() FAILED",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
    }

    /*
     *  free
     */
    JSON_DECREF(jn_topics)
    JSON_DECREF(seen_topics)
    JSON_DECREF(cols_desc)
    JSON_DECREF(current)

    return summary;
}

/***************************************************************************
 *  The topics of a schema as a LIST, whatever shape the schema holds them
 *  in. A literal, and the file a save writes, carry a list; the file in use
 *  of a node opened with impose off before the draft model carries a DICT
 *  keyed by topic name, written from get_treedb_schema() as it was. Read as
 *  no topic at all, that file made every topic "changed" and every version
 *  was bumped and written again (N11 of the 2026-09-22 review). A dict's
 *  topic takes its name as `id` when it carries none. Return is YOURS.
 ***************************************************************************/
PRIVATE json_t *schema_topics_as_list(hgobj gobj, json_t *jn_schema)
{
    json_t *topics = json_object_get(jn_schema, "topics");
    json_t *list = json_array();
    if(json_is_array(topics)) {
        json_array_extend(list, topics);
        return list;
    }
    if(json_is_object(topics)) {
        const char *topic_name; json_t *topic;
        json_object_foreach(topics, topic_name, topic) {
            if(!json_is_object(topic)) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "A topic of the schema is not a dict",
                    "topic_name",   "%s", topic_name,
                    NULL
                );
                continue;
            }
            json_t *as_listed = json_copy(topic);   // shallow, yours
            if(!kw_has_key(as_listed, "id") && !kw_has_key(as_listed, "topic_name")) {
                json_object_set_new(as_listed, "id", json_string(topic_name));
            }
            json_array_append_new(list, as_listed);
        }
    }
    return list;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_open_treedb(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{

    KW_DECREF(kw)
    return 0; // TODO
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_close_treedb(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{

    KW_DECREF(kw)
    return -1; // TODO
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
    .mt_treedbs = mt_treedbs,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TREEDB);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_OPEN_TREEDB);
GOBJ_DEFINE_EVENT(EV_CLOSE_TREEDB);

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
        {EV_OPEN_TREEDB,            ac_open_treedb,         0},
        {EV_CLOSE_TREEDB,           ac_close_treedb,        0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_OPEN_TREEDB,        EVF_PUBLIC_EVENT},
        {EV_CLOSE_TREEDB,       EVF_PUBLIC_EVENT},
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
PUBLIC int register_c_treedb(void)
{
    return create_gclass(C_TREEDB);
}
