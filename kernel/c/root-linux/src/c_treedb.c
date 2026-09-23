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
 *          "open-treedb"   -> create a timeranger and a treedb (C_NODE) with the schema passed
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
 *          FROM. The master projects each literal into it (seeded, and re-made
 *          when the literal moves ahead), an operator edits it there, and an
 *          edit is a DRAFT: it moves no version and reaches no treedb until
 *          save-schema publishes it and apply-schema puts it in the file the
 *          treedb opens from. Until the owner's design of M36 (2026-09-21) it
 *          was the source: every write raised the versions, so an edit half
 *          made was the schema of the next start.
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
    json_t *jn_client_treedb_schema // not owned
);
PRIVATE json_t *get_c_schema_to_impose(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_c_schema // not owned
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
    json_t *kw  // owned
);
PRIVATE json_t *diff_treedb_schema(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_schema,  // the schema from C, not owned
    json_t *rows        // not owned, where the differences are appended
);
PRIVATE json_t *schema_topics_as_list(hgobj gobj, json_t *jn_schema);
PRIVATE json_t *apply_every_saved_schema(hgobj gobj, const char *cmd, json_t *kw);
PRIVATE BOOL treedb_is_written_here(hgobj gobj, const char *treedb_name);
PRIVATE json_t *draft_changed_from_rows(hgobj gobj, json_t *rows);
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
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
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
            json_sprintf("What treedb_name?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(filename_mask)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What filename_mask?"),
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
        get_c_schema_to_impose(gobj, treedb_name, _jn_treedb_schema):
        get_client_treedb_schema(gobj, treedb_name, _jn_treedb_schema);
    if(!jn_client_treedb_schema) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("treedb_schema not found: '%s'", treedb_name),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------------*
     *      Create Client Timeranger
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
    char tranger_name[NAME_MAX];
    snprintf(tranger_name, sizeof(tranger_name), "tranger_%s", treedb_name);
    hgobj gobj_client_tranger = gobj_create_service(
        tranger_name,
        C_TRANGER,
        kw_client_tranger,
        gobj
    );

    json_t *tranger_client = gobj_read_pointer_attr(gobj_client_tranger, "tranger");
    if(!tranger_client) {
        gobj_log_critical(gobj, exit_on_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger client NULL",
            NULL
        );

        JSON_DECREF(jn_client_treedb_schema);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Internal error, tranger client NULL"),
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

    /*
     *  HACK pipe inheritance
     */
    gobj_set_bottom_gobj(gobj_client_node, gobj_client_tranger);

    gobj_start(gobj_client_tranger);
    gobj_start(gobj_client_node);

    if(forced_by_code && gobj_client_node) {
        json_object_set_new(priv->jn_forced_treedbs, treedb_name, json_true());
    } else {
        json_object_del(priv->jn_forced_treedbs, treedb_name);
    }

    return msg_iev_build_response(gobj,
        gobj_client_node?0:-1,
        gobj_client_node?
            json_sprintf("Treedb opened!"):
            json_sprintf("%s: cannot open treedb '%s' (see the log)",
                gobj_yuno_role_plus_name(), treedb_name),
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
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
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
            json_sprintf("What treedb_name?"),
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

    hgobj gobj_client_tranger = gobj_bottom_gobj(gobj_client_node);

    gobj_stop(gobj_client_node);
    gobj_stop(gobj_client_tranger);

    gobj_destroy(gobj_client_tranger);
    gobj_destroy(gobj_client_node);

    json_object_del(priv->jn_c_schemas, treedb_name);
    json_object_del(priv->jn_forced_treedbs, treedb_name);

    return msg_iev_build_response(gobj,
        0,
        json_sprintf("Treedb closed!"),
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
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
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
            json_sprintf("What treedb_name?"),
            0,
            0,
            kw  // owned
        );
    }
    if(!force) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("This command must be use with force"),
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

    if(!gobj_read_bool_attr(gobj, "master")) {
        return build_readonly_response(gobj, gobj_name(priv->gobj_node_system), kw);
    }

    int ret = delete_client_treedb_schema(gobj, treedb_name);
    json_object_del(priv->jn_c_schemas, treedb_name);
    json_object_del(priv->jn_forced_treedbs, treedb_name);

    if(ret < 0) {
        return msg_iev_build_response(gobj,
            ret,
            json_sprintf(
                "%s: cannot delete the schema of '%s': not projected in "
                "__system__, or one of its nodes refused the delete (see the log)",
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
    if(!kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED)) {
        return build_readonly_response(gobj, treedb_name, kw);
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
    if(!kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED)) {
        return build_readonly_response(gobj, treedb_name, kw);
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
             *  as "default added" against the file in use.  */
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

    /*
     *  A column's `fkey` DICT is a mark parse_schema() derives from the
     *  hooks that point at it, never written by an author: treedb_open_db()
     *  strips it from the file in use. Left in, a literal that went through
     *  parse_schema() read as another schema than its own file.
     */
    int idx; json_t *topic;
    json_array_foreach(json_object_get(jn_schema, "topics"), idx, topic) {
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

/***************************************************************************
 *  A schema file as a table, to say what changed: `topics` is a list, so
 *  it is keyed by name first -- by position, a reordered topic would read
 *  as every column of it changed. Pruned, and a `false` is the absence it
 *  stands for: a literal leaves `system_topic` out, a projection says it.
 ***************************************************************************/
PRIVATE json_t *schema_to_flat(json_t *jn_schema) // not owned
{
    json_t *copy = json_deep_copy(jn_schema);
    prune_schema(copy);
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
     *  at every open (L-6 of the 2026-09-23 independent review).
     */
    const char *topic_name; json_t *topic;
    json_object_foreach(json_object_get(copy, "topics"), topic_name, topic) {
        json_t *cols = json_object_get(topic, "cols");
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
 *  The treedbs this service opened, one row each: whether it opens with
 *  its schema from C and who decided it (the yuno's code, the configuration
 *  dynamic_schema_treedbs, or the default impose_c_schema), the version of
 *  the literal, of the schema file in use and of the saved one.
 *  `treedb_system_schema` comes first: it is opened by this service too,
 *  always from C.
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

    json_t *jn_data = json_array();
    json_array_append_new(jn_data, json_pack("{s:s, s:b, s:s, s:I, s:I, s:I}",
        "treedb_name", TREEDB_SYSTEM_SCHEMA_NAME,
        "impose_c_schema", 1,
        "decided_by", "system",
        "c_schema_version", priv->system_schema_version,
        "in_use_schema_version", priv->system_schema_version,
        "saved_schema_version", (json_int_t)0
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

        json_array_append_new(jn_data, json_pack("{s:s, s:b, s:s, s:I, s:I, s:I}",
            "treedb_name", name,
            "impose_c_schema", treedb_schema_imposed(gobj, name),
            "decided_by", impose_decided_by(gobj, name),
            "c_schema_version", kw_get_int(gobj, jn_schema, "schema_version", 0, KW_WILD_NUMBER),
            "in_use_schema_version", in_use_version,
            "saved_schema_version", saved_version
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
        return kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED);
    }
    return gobj_read_bool_attr(gobj, "master");
}

/***************************************************************************
 *  Can __system__ be written here? What save-schema writes -- the versions
 *  of the draft, and `saved_schemas/` -- is under the __system__ tranger,
 *  not the client treedb's: the two can be a master and a replica apart.
 ***************************************************************************/
PRIVATE BOOL system_is_written_here(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return kw_get_bool(gobj, priv->tranger_system_, "master", 0, KW_REQUIRED);
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
        return build_readonly_response(gobj, gobj_name(priv->gobj_node_system), kw);
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
         *  is withdrawn: left in place, saved-schema went on diffing the
         *  draft against it (the mark of an unsaved change that never
         *  cleared) and apply-schema would have installed the change taken
         *  back (M-A of the 2026-09-23 independent review). The versions
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

    json_t *diff = json_object();
    if(in_use && saved) {
        json_t *flat_in_use = schema_to_flat(in_use);
        json_t *flat_saved = schema_to_flat(saved);
        JSON_DECREF(diff)
        diff = flat_diff(flat_in_use, flat_saved);
        JSON_DECREF(flat_in_use)
        JSON_DECREF(flat_saved)
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
    json_t *draft_base = (saved && saved_version > in_use_version)? saved : in_use;
    json_t *draft_changed = json_object();
    if(draft_base) {
        json_t *rows = json_array();
        json_t *summary = diff_treedb_schema(gobj, treedb_name, draft_base, rows);
        JSON_DECREF(summary)
        JSON_DECREF(draft_changed)
        draft_changed = draft_changed_from_rows(gobj, rows);
        JSON_DECREF(rows)
    }
    JSON_DECREF(in_use)
    JSON_DECREF(saved)

    return msg_iev_build_response(gobj, 0,
        0,
        0,
        json_pack("{s:s, s:b, s:b, s:b, s:I, s:I, s:b, s:s, s:o, s:o}",
            "treedb_name", treedb_name,
            "impose_c_schema", imposed,
            "master", master,
            "saved", saved_version > 0,
            "in_use_schema_version", in_use_version,
            "saved_schema_version", saved_version,
            "can_apply", master && !imposed && saved_version > in_use_version,
            "path", saved_path,
            "diff", diff,
            "draft_changed", draft_changed
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
    if(json_dumpfd(jn_schema, fd, JSON_INDENT(4)) < 0 || fsync(fd) < 0) {
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
 *  a failure -- while a saved schema that does not load or parse is.
 ***************************************************************************/
PRIVATE json_t *check_saved_schema_to_apply(
    hgobj gobj,
    const char *treedb_name,
    json_t **p_saved,           // YOURS when NULL is returned
    char *in_use_dir,
    size_t in_use_dir_size,
    json_int_t *p_saved_version,
    json_int_t *p_in_use_version,
    BOOL *p_applicable
)
{
    *p_saved = NULL;
    *p_saved_version = 0;
    *p_in_use_version = 0;
    *p_applicable = FALSE;

    if(!treedb_is_written_here(gobj, treedb_name)) {
        return json_sprintf("%s: treedb '%s' is READ-ONLY, this yuno is not the master of its tranger",
            gobj_yuno_role_plus_name(), treedb_name);
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

    *p_applicable = TRUE;
    if(!saved) {
        return json_sprintf("%s: the saved schema of '%s' cannot be read (see the log)",
            gobj_yuno_role_plus_name(), treedb_name);
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
    BOOL applicable;
    json_t *refused = check_saved_schema_to_apply(
        gobj, treedb_name, &saved, in_use_dir, sizeof(in_use_dir),
        &saved_version, &in_use_version, &applicable
    );
    if(refused) {
        return msg_iev_build_response(gobj, -1,
            refused,
            0,
            apply_schema_data(treedb_name, FALSE, saved_version, in_use_version),
            kw
        );
    }

    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.treedb_schema.json", treedb_name);
    char tmp_path[PATH_MAX];
    int ret = write_schema_tmp(gobj, in_use_dir, filename, saved, tmp_path, sizeof(tmp_path));
    JSON_DECREF(saved)
    if(ret == 0) {
        ret = commit_schema_file(gobj, in_use_dir, filename, tmp_path);
    }
    if(ret < 0) {
        return msg_iev_build_response(gobj, -1,
            json_sprintf("%s: cannot write the schema of '%s', the file in use is unchanged (see the log)",
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
 *  schema can be applied, ALL OR NONE (M2 of the 2026-09-23 review).
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
 *    3. then each is renamed over its file in use.
 *
 *  Only a rename can fail after that, and it fails on its own: its row says
 *  applied=false while the others say true.
 *
 *  A treedb with nothing to apply is left out, as it always was: an apply
 *  with nothing applicable answers 0 and no row, and nobody restarts.
 ***************************************************************************/
PRIVATE json_t *apply_every_saved_schema(hgobj gobj, const char *cmd, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *plan = json_array();    // [{treedb_name, saved, in_use_dir, versions, refused}]
    json_t *failed = json_array();  // names

    const char *name; json_t *jn_c_schema;
    json_object_foreach(priv->jn_c_schemas, name, jn_c_schema) {
        json_t *saved;
        char in_use_dir[PATH_MAX];
        json_int_t saved_version, in_use_version;
        BOOL applicable;
        json_t *refused = check_saved_schema_to_apply(
            gobj, name, &saved, in_use_dir, sizeof(in_use_dir),
            &saved_version, &in_use_version, &applicable
        );
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
            if(commit_schema_file(gobj,
                    kw_get_str(gobj, entry, "in_use_dir", "", 0),
                    filename,
                    tmp_path)<0) {
                comment = json_sprintf("%s: cannot write the schema of '%s', the file in use is unchanged (see the log)",
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
 *  and apply-schema. The projector never deletes, so an edit is invisible:
 *  the version numbers of the `treedbs` node say that SOMETHING was
 *  published, never what. This answers what.
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
 *  Whether writing a projected node over the stored one would change it.
 *
 *  The comparison of diff-schema, read from the side of the write: an update
 *  merges, so what the stored node holds and the projection does not declare
 *  stays as it is, and only what the projection would ADD or CHANGE counts.
 ***************************************************************************/
PRIVATE BOOL projection_changes_node(
    hgobj gobj,
    json_t *projected,  // not owned
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
            break;
        }
    }
    JSON_DECREF(rows)

    return changes;
}

/***************************************************************************
 *  Project a schema into the __system__ treedb: create what is missing,
 *  update what moved.
 *
 *  HACK Nothing is ever deleted here. A delete is the one destructive
 *  primitive of the store — it drops the schema's own history, which is the
 *  reason to keep a schema in a treedb at all, and it refuses a
 *  snapshot-tagged node. An update appends a new version instead, so what a
 *  column used to declare stays readable with `instances`. The one exception
 *  is migrate_schema_ids_to_qualified(), which runs before this and has to
 *  retire an id the store can no longer address a node by.
 *
 *  What exists here and not in the incoming schema is left alone: it is
 *  indistinguishable from an operator addition, and removing a topic or a
 *  column is a deliberate action, never a side effect of an upgrade.
 *
 *  With `imposing`, a topic is projected whatever its stored topic_version
 *  says. treedb_open_db() installs each topic of an imposed schema over a
 *  HIGHER stored topic_version too, so the ordinary rule would leave the
 *  projection saying something the store no longer holds -- in the one case
 *  `impose` exists to repair.
 *
 *  With `file_taken_over` (the schema file in use that a newer literal is
 *  about to replace, see literal_against_file_in_use), a topic is projected
 *  when the literal RAISED it past that file, whatever version a save gave
 *  it in __system__: that is the topic the treedb will run from the
 *  literal (tranger2 installs a topic only over a lower topic_version). A
 *  topic the literal did not raise goes on running from the file, and its
 *  draft in __system__ -- an operator's edit included -- is left alone,
 *  as a literal N+1 arriving the ordinary way leaves it (M-B of the
 *  2026-09-23 independent review: it was projected with the rule of
 *  `imposing`, every topic that differed).
 ***************************************************************************/
PRIVATE int upsert_treedb_schema(
    hgobj gobj,
    const char *treedb_name,
    json_t *kw,     // not owned
    json_t *current,// not owned, the projection already stored, or NULL
    BOOL imposing,  // the schema from C wins over every stored topic_version
    json_t *file_taken_over // not owned, the file in use a newer literal replaces, or NULL
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The literal's own number, as it is: the version is published by
     *  whoever changes the schema, never invented here. `c_schema_version`
     *  records which literal this projection came from, for diff-schema.
     */
    json_int_t c_schema_version = kw_get_int(gobj, kw, "schema_version", 1, KW_WILD_NUMBER);
    json_int_t schema_version = c_schema_version;

    json_t *kw_treedb = json_pack("{s:s, s:I, s:I, s:I}",
        "id", treedb_name,
        "schema_version", (json_int_t )schema_version,
        "c_schema_version", (json_int_t )c_schema_version,
        "system_schema_version", (json_int_t )priv->system_schema_version
    );

    json_t *treedb;
    if(current) {
        treedb = gobj_update_node(
            priv->gobj_node_system,
            "treedbs",
            kw_treedb,
            json_pack("{s:b}", "refs", 1),      // fkey,hook options
            gobj
        );
    } else {
        treedb = gobj_create_node(
            priv->gobj_node_system,
            "treedbs",
            kw_treedb,
            json_pack("{s:b}", "refs", 1),      // fkey,hook options
            gobj
        );
    }
    if(!treedb) {
        return -1;  // Error already logged
    }

    json_t *current_topics = current? kw_get_dict(gobj, current, "topics", 0, 0): NULL;

    /*
     *  What a column may declare, read once for the whole projection
     */
    json_t *cols_desc = _treedb_create_topic_cols_desc();

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
         *  new or moved, and their topic.
         */
        json_t *kw_cols = json_array();
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

            json_t *current_col = current_cols? json_object_get(current_cols, col_id): NULL;
            if(current_col &&
                !projection_changes_node(gobj, kw_col, current_col, schema_col_skip, cols_desc)
            ) {
                JSON_DECREF(kw_col)
                continue;
            }
            json_array_append_new(kw_cols, kw_col);
        }
        json_decref(jn_cols);

        /*
         *  A topic is published by raising ITS version, the same rule
         *  tranger2 applies to topic_cols.json, and the number is the
         *  literal's as it is. A topic the literal did not raise is left as
         *  it is stored, dynamic edits included — said when the literal
         *  declares something else, because a column changed in C without
         *  a higher topic_version is the classic change that reaches nothing.
         *  Imposing, that rule does not apply: the literal wins at both
         *  levels, here as on disk.
         */
        if(current_topic) {
            json_int_t stored_topic_version = kw_get_int(
                gobj, current_topic, "topic_version", 0, KW_WILD_NUMBER
            );
            BOOL topic_changes = (json_array_size(kw_cols) > 0 ||
                projection_changes_node(gobj, kw_topic, current_topic, schema_topic_skip, NULL)
            )? TRUE: FALSE;

            /*
             *  Imposing, a topic is written because it DIFFERS, not because
             *  its version is higher. Re-appending an identical topic would
             *  add a record per start saying nothing.
             */
            if(imposing && !topic_changes) {
                JSON_DECREF(kw_cols)
                json_decref(kw_topic);
                continue;
            }

            if(file_taken_over && !imposing) {
                json_int_t file_topic_version = schema_topic_version(
                    gobj, file_taken_over, topic_name
                );
                if(!topic_changes) {
                    JSON_DECREF(kw_cols)
                    json_decref(kw_topic);
                    continue;
                }
                if(topic_version <= file_topic_version) {
                    gobj_log_info(gobj, 0,
                        "function",         "%s", __FUNCTION__,
                        "msgset",           "%s", MSGSET_INFO,
                        "msg",              "%s", "Topic from C differs from its draft, but the schema from C does not raise it past the file in use: the draft is kept",
                        "treedb_name",      "%s", treedb_name,
                        "topic_name",       "%s", topic_name,
                        "topic_version",    "%d", (int)topic_version,
                        "file_version",     "%d", (int)file_topic_version,
                        "stored_version",   "%d", (int)stored_topic_version,
                        NULL
                    );
                    JSON_DECREF(kw_cols)
                    json_decref(kw_topic);
                    continue;
                }
                /*  raised past the file: projected, as imposing would  */

            } else if(topic_version <= stored_topic_version && !imposing) {
                if(topic_changes) {
                    gobj_log_info(gobj, 0,
                        "function",         "%s", __FUNCTION__,
                        "msgset",           "%s", MSGSET_INFO,
                        "msg",              "%s", "Topic from C differs from the one in use, but its topic_version is not higher: not applied",
                        "treedb_name",      "%s", treedb_name,
                        "topic_name",       "%s", topic_name,
                        "topic_version",    "%d", (int)topic_version,
                        "stored_version",   "%d", (int)stored_topic_version,
                        NULL
                    );
                }
                JSON_DECREF(kw_cols)
                json_decref(kw_topic);
                continue;
            }
        }

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
                JSON_DECREF(kw_cols)
                continue;   // Error already logged
            }
        } else {
            topic = gobj_create_node(
                priv->gobj_node_system,
                "topics",
                kw_topic,
                json_pack("{s:b}", "refs", 1),      // fkey,hook options
                gobj
            );
            if(!topic) {
                JSON_DECREF(kw_cols)
                continue;   // Error already logged
            }

            gobj_link_nodes(
                priv->gobj_node_system,
                "topics",               // hook
                "treedbs",              // parent_topic_name,
                json_incref(treedb),    // parent_record,owned
                "topics",               // child_topic_name,
                json_incref(topic),     // child_record,owned
                gobj
            );
        }

        json_t *kw_col;
        json_array_foreach(kw_cols, idx2, kw_col) {
            const char *col_id = kw_get_str(gobj, kw_col, "id", "", 0);
            json_t *current_col = current_cols? json_object_get(current_cols, col_id): NULL;

            json_t *col;
            if(current_col) {
                col = gobj_update_node(
                    priv->gobj_node_system,
                    "cols",
                    json_incref(kw_col),
                    json_pack("{s:b}", "refs", 1),  // fkey,hook options
                    gobj
                );
                if(!col) {
                    continue;   // Error already logged
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
                    continue;   // Error already logged
                }

                gobj_link_nodes(
                    priv->gobj_node_system,
                    "cols",                 // hook
                    "topics",               // parent_topic_name,
                    json_incref(topic),     // parent_record,owned
                    "cols",                 // child_topic_name,
                    json_incref(col),       // child_record,owned
                    gobj
                );
            }

            /*
             *  free
             */
            json_decref(col);
        }

        /*
         *  free
         */
        JSON_DECREF(kw_cols)
        json_decref(topic);
    }

    /*
     *  free
     */
    JSON_DECREF(cols_desc)
    json_decref(treedb);

    return 0;
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
 *  What a literal that is NOT newer than __system__ does to the treedb,
 *  asked when the treedb opens from its FILE (impose off).
 *
 *  __system__ and the file in use share one version number and two authors
 *  write it: the literal's author, and save-schema, which gives the draft
 *  the file's version + 1. The number of __system__ can therefore be a
 *  save's, never applied, while the literal moves on -- and the literal
 *  WINS over the file at open (treedb_open_db() installs a literal newer
 *  than the file). Judged by __system__'s number alone, that literal was
 *  "behind" and never projected: the treedb ran it, __system__ went on
 *  holding the draft it had replaced, and the next save published that
 *  draft over it, reverting the literal's change with no word (the second
 *  medium of M36, 2026-09-23 review).
 *
 *  So the question is the one treedb_open_db() asks, against the FILE:
 *
 *    LITERAL_TAKES_OVER_FILE     the literal is newer than the file, or
 *                                there is no file: it is what runs, and
 *                                it is projected -- the topics it raised
 *                                past the file, or every topic when there
 *                                is no file (see upsert_treedb_schema).
 *    LITERAL_SAME_VERSION_OTHER_SCHEMA
 *                                the literal carries the version of the file
 *                                in use, came after the last projection, and
 *                                is another schema: two schemas, one number.
 *                                The file wins (ties go to the file), and it
 *                                is said, loudly, instead of silently.
 *    LITERAL_NOT_APPLIED         behind the file, or the projection is
 *                                already of this literal: the ordinary rule.
 ***************************************************************************/
typedef enum {
    LITERAL_NOT_APPLIED = 0,
    LITERAL_TAKES_OVER_FILE,
    LITERAL_SAME_VERSION_OTHER_SCHEMA,
} literal_verdict_t;

PRIVATE literal_verdict_t literal_against_file_in_use(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_schema,          // not owned, the literal
    json_int_t new_version,
    json_int_t stored_version,
    json_int_t stored_c_version
)
{
    if(stored_c_version == new_version && stored_version == new_version) {
        return LITERAL_NOT_APPLIED;     /*  the projection is of this literal  */
    }

    json_t *in_use = load_schema_file_in_use(gobj, treedb_name);
    json_int_t in_use_version = schema_version_of(gobj, in_use);
    literal_verdict_t verdict = LITERAL_NOT_APPLIED;

    if(!in_use) {
        /*
         *  No file in use: the treedb opens from the literal, whatever
         *  version __system__ holds. Said as it is -- it used to be told as
         *  "a draft saved over it" (L-6 of the 2026-09-23 independent review).
         */
        verdict = LITERAL_TAKES_OVER_FILE;
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_TREEDB,
            "msg",              "%s", "No schema file in use: the treedb opens with the schema from C, projected whole over __system__",
            "treedb_name",      "%s", treedb_name,
            "schema_version",   "%d", (int)new_version,
            "system_version",   "%d", (int)stored_version,
            NULL
        );

    } else if(new_version > in_use_version) {
        verdict = LITERAL_TAKES_OVER_FILE;
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_TREEDB,
            "msg",              "%s", "Schema from C is newer than the file in use but not than __system__: it takes over the file, and replaces in __system__ the drafts of the topics it raises past the file",
            "treedb_name",      "%s", treedb_name,
            "schema_version",   "%d", (int)new_version,
            "in_use_version",   "%d", (int)in_use_version,
            "system_version",   "%d", (int)stored_version,
            NULL
        );

    } else if(new_version == in_use_version && stored_c_version != new_version && in_use) {
        json_t *flat_literal = schema_to_flat(jn_schema);
        json_t *flat_in_use = schema_to_flat(in_use);
        json_t *diff = flat_diff(flat_in_use, flat_literal);
        BOOL differs = json_object_size(json_object_get(diff, "added")) > 0 ||
            json_object_size(json_object_get(diff, "removed")) > 0 ||
            json_object_size(json_object_get(diff, "changed")) > 0;
        JSON_DECREF(flat_literal)
        JSON_DECREF(flat_in_use)
        if(differs) {
            verdict = LITERAL_SAME_VERSION_OTHER_SCHEMA;
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
    JSON_DECREF(in_use)

    return verdict;
}

/***************************************************************************
 *  Keep the __system__ projection in step with the schema compiled in C.
 *
 *  Same rule treedb_open_db applies between that schema and the persisted
 *  schema file: the stored one wins on ties, and the incoming one has to be
 *  strictly newer to take over. The version is published by whoever changes
 *  the schema — the author of the literal, or an editor working on
 *  __system__ — and nobody else invents one.
 *
 *  So a literal BEHIND the schema in use is not applied. That is the schema
 *  being changed dynamically, which is a decision, not an accident: a new
 *  installation that has to carry those changes takes them into the literal.
 *
 *  The rule is the same for a treedb opened with `impose`, which is why that
 *  path calls this one: what `imposing` changes is the TOPICS of a projection
 *  that is being re-made (see upsert_treedb_schema), never whether it is.
 *
 *  ONLY THE MASTER WRITES __system__. A replica reads the treedb from disk as
 *  it is at that moment and reconciles nothing: the master's appends reach it
 *  through the store, and a projection written by two owners is a projection
 *  nobody can read. This is the only place __system__ is written from, the
 *  migration of legacy ids included, so the guard belongs here.
 ***************************************************************************/
PRIVATE int reconcile_treedb_schema(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_schema,  // not owned
    BOOL imposing       // the treedb is being opened with the schema from C
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  The tranger's own flag, not the attribute: a master that could not
     *  take the store in exclusive opens as a replica (timeranger2.c), and
     *  what decides whether a write lands is what it ended up being.
     */
    if(!kw_get_bool(gobj, priv->tranger_system_, "master", 0, KW_REQUIRED)) {
        return 0;
    }

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
        return upsert_treedb_schema(gobj, treedb_name, jn_schema, NULL, imposing, NULL);
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

    json_int_t new_version = kw_get_int(gobj, jn_schema, "schema_version", 1, KW_WILD_NUMBER);
    BOOL takes_over = FALSE;
    if(new_version <= stored_version && !imposing) {
        switch(literal_against_file_in_use(
                gobj, treedb_name, jn_schema, new_version,
                stored_version, stored_c_version)) {
            case LITERAL_TAKES_OVER_FILE:
                /*
                 *  The treedb is about to run the literal, and __system__
                 *  must say so: see literal_against_file_in_use(). A topic
                 *  the literal raised past the FILE is written, whatever
                 *  version a save gave it in __system__; the others are
                 *  left as they are (see upsert_treedb_schema).
                 *
                 *  This assumes the CLIENT treedb opens as a master:
                 *  treedb_open_db() installs nothing on a replica. The
                 *  client tranger does not exist yet here, and it takes the
                 *  `master` of this service, as __system__ did; the two
                 *  differ only when the lock of the client store is held
                 *  by another process. Then the client opens as a replica
                 *  and runs its file while __system__ says the literal,
                 *  until the next open as master installs the literal --
                 *  whose projection is then already there.
                 */
                takes_over = TRUE;
                break;
            case LITERAL_SAME_VERSION_OTHER_SCHEMA:
            case LITERAL_NOT_APPLIED:
            default:
                break;
        }
    }
    if(new_version <= stored_version && !takes_over) {
        if(new_version < stored_version) {
            gobj_log_info(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INFO,
                "msg",              "%s", "TreeDB schema from C is behind the schema in use, not applied",
                "treedb_name",      "%s", treedb_name,
                "schema_version",   "%d", (int)new_version,
                "stored_version",   "%d", (int)stored_version,
                NULL
            );
        }
        return 0;
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

    gobj_log_info(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_INFO,
        "msg",              "%s", "Updating TreeDB schema in __system__",
        "treedb_name",      "%s", treedb_name,
        "schema_version",   "%d", (int)new_version,
        "stored_version",   "%d", (int)stored_version,
        NULL
    );

    /*
     *  Taken over with NO file in use, every topic opens from the literal,
     *  and every one is projected: the rule of `imposing`.
     */
    json_t *file_taken_over = takes_over? load_schema_file_in_use(gobj, treedb_name) : NULL;
    int ret = upsert_treedb_schema(
        gobj, treedb_name, jn_schema, current,
        imposing || (takes_over && !file_taken_over),
        file_taken_over
    );
    JSON_DECREF(file_taken_over)
    JSON_DECREF(current)

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
    json_t *kw  // owned
)
{
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
    json_t *jn_client_treedb_schema // not owned
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

        reconcile_treedb_schema(gobj, treedb_name, jn_client_treedb_schema, FALSE);
    }

    /*
     *  With impose_c_schema off, a treedb opens from its schema FILE (with
     *  it on, get_c_schema_to_impose() opens it from the literal, over the
     *  file). The literal is handed to treedb_open_db() without `impose`, so
     *  it is installed only when it is newer than the file: the file wins
     *  on ties and when it is ahead -- which is what apply-schema makes it.
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
    json_t *jn_c_schema // not owned
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

    reconcile_treedb_schema(gobj, treedb_name, jn_c_schema, TRUE);

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
 *  The projection is an upsert that never deletes, so what it holds and the
 *  schema does not is indistinguishable from an operator addition — until
 *  somebody compares the two. That is this function: it projects the schema
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
