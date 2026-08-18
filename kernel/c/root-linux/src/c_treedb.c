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
 *
 *          Copyright (c) 2021 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <limits.h>

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

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *get_client_treedb_schema(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_client_treedb_schema // not owned
);
PRIVATE int delete_client_treedb_schema(
    hgobj gobj,
    const char *treedb_name
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
    helper_quote2doublequote(treedb_system_schema);
    json_t *jn_treedb_system_schema;
    jn_treedb_system_schema = legalstring2json(treedb_system_schema, TRUE);
    if(!jn_treedb_system_schema) {
        exit(-1);
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
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, json_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
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
     *      Get schema of __system__
     *-----------------------------------*/
    json_t *jn_client_treedb_schema = get_client_treedb_schema(
        gobj,
        treedb_name,
        _jn_treedb_schema // not owned
    );
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
    json_t *kw_resource = json_pack("{s:I, s:s, s:o, s:i, s:b}",
        "tranger", (json_int_t)(uintptr_t)tranger_client,
        "treedb_name", treedb_name,
        "treedb_schema", jn_client_treedb_schema,
        "exit_on_error", exit_on_error,
        "with_link_events", gobj_read_bool_attr(gobj, "with_link_events")
    );

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

    return msg_iev_build_response(gobj,
        gobj_client_node?0:-1,
        json_sprintf("%s", gobj_client_node?"Treedb opened!":gobj_log_last_message()),
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

    hgobj gobj_client_tranger = gobj_bottom_gobj(gobj_client_node);

    gobj_stop(gobj_client_node);
    gobj_stop(gobj_client_tranger);

    gobj_destroy(gobj_client_tranger);
    gobj_destroy(gobj_client_node);

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

    int ret = delete_client_treedb_schema(gobj, treedb_name);

    return msg_iev_build_response(gobj,
        ret,
        json_sprintf("%s", ret<0?gobj_log_last_message():"Treedb deleted!"),
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

    json_t *tranger = gobj_read_pointer_attr(gobj_client_node, "tranger");

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

    int ret = treedb_delete_topic(
        gobj_read_pointer_attr(gobj_client_node, "tranger"),
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




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Find a column node of a topic by the column name.
 *
 *  A column name is unique only inside its topic, so `cols` keys its nodes
 *  by rowid and holds the name in `value`: asking the topic-wide pkey2 index
 *  for `value` would answer with the columns of every other topic too. The
 *  lookup goes through the topic's own `cols` hook.
 *
 *  Return the column's rowid, or NULL if the topic has no such column.
 *  Return is NOT yours.
 ***************************************************************************/
PRIVATE const char *find_col_id(
    hgobj gobj,
    json_t *current_topic,  // topic node with its `cols` hook expanded, NOT owned
    const char *col_name
)
{
    json_t *cols = kw_get_dict(gobj, current_topic, "cols", 0, 0);
    const char *col_id; json_t *col;
    json_object_foreach(cols, col_id, col) {
        if(strcmp(kw_get_str(gobj, col, "value", "", 0), col_name)==0) {
            return col_id;
        }
    }

    return NULL;
}

/***************************************************************************
 *  Find a topic node of a treedb by the topic name.
 *
 *  Same reason as find_col_id: a topic name is unique only inside its
 *  treedb, so `topics` keys its nodes by rowid and holds the name in
 *  `value`. Keyed by the bare name, the second treedb of a store declaring
 *  `users` collided with the first and lost its schema.
 *
 *  Return the topic node, or NULL. Return is NOT yours.
 ***************************************************************************/
PRIVATE json_t *find_topic_node(
    hgobj gobj,
    json_t *current_topics, // the treedb's `topics` hook expanded, NOT owned
    const char *topic_name
)
{
    const char *topic_id; json_t *topic;
    json_object_foreach(current_topics, topic_id, topic) {
        if(strcmp(kw_get_str(gobj, topic, "value", "", 0), topic_name)==0) {
            return topic;
        }
    }

    return NULL;
}

/***************************************************************************
 *  Project a schema into the __system__ treedb: create what is missing,
 *  update what moved.
 *
 *  HACK Nothing is ever deleted here. A column's `id` is a rowid handed out
 *  from the topic size, so re-creating columns renumbers all of them and can
 *  hand a retired number to a different column; and a delete is the one
 *  destructive primitive of the store — it drops the schema's own history,
 *  which is the reason to keep a schema in a treedb at all, and it refuses a
 *  snapshot-tagged node. An update appends a new version instead, so what a
 *  column used to declare stays readable with `instances`.
 *
 *  What exists here and not in the incoming schema is left alone: it is
 *  indistinguishable from an operator addition, and removing a topic or a
 *  column is a deliberate action, never a side effect of an upgrade.
 ***************************************************************************/
PRIVATE int upsert_treedb_schema(
    hgobj gobj,
    const char *treedb_name,
    json_t *kw,     // not owned
    json_t *current // not owned, the projection already stored, or NULL
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t c_schema_version = kw_get_int(gobj, kw, "schema_version", 1, KW_WILD_NUMBER);

    /*
     *  `schema_version` says what this schema is worth to treedb_open_db, and
     *  whoever edits it here raises it. `c_schema_version` says which version
     *  of the C literal this projection came from, and only this function
     *  writes it: without that second number the two lines share one counter,
     *  an edit made here silently outranks every later release of the literal,
     *  and nothing says so.
     *
     *  A re-projection must also outrank whatever is already published, or the
     *  persisted schema file — which may sit at the edited version — keeps
     *  masking it.
     */
    json_int_t schema_version = c_schema_version;
    if(current) {
        json_int_t stored = kw_get_int(gobj, current, "schema_version", 0, KW_WILD_NUMBER);
        schema_version = (stored > c_schema_version? stored: c_schema_version) + 1;
    }

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

    /*
     *  A write to a schema publishes itself by raising the versions (see
     *  publish_schema_change in tr_treedb). The projector sets them itself,
     *  and every node it writes is a schema write, so it says so while it
     *  works — otherwise each column would move the versions again.
     */
    json_object_set_new(priv->tranger_system_, "__schema_publishing__", json_true());

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
        const char *pkey = kw_get_str(gobj, jn_topic, "pkey", "id", 0);
        const char *tkey = kw_get_str(gobj, jn_topic, "tkey", "", 0);
        const char *system_flag = kw_get_str(gobj, jn_topic, "system_flag", "sf_string_key", 0);
        json_int_t topic_version = kw_get_int(gobj, jn_topic, "topic_version", 1, KW_WILD_NUMBER);
        json_t *topic_pkey2s_ = kw_get_dict_value(gobj, jn_topic, "pkey2s", 0, 0);
        BOOL system_topic = kw_get_bool(gobj, jn_topic, "system_topic", 0, 0);

        json_t *current_topic = find_topic_node(gobj, current_topics, topic_name);

        /*
         *  A topic's version cannot go BACKWARDS on a re-projection, for the
         *  same reason `schema_version` cannot: the literal's number may be
         *  lower than what has already been published, and tranger2 keeps the
         *  persisted topic_cols.json whenever the incoming version is not
         *  higher. Writing the literal's number verbatim left a re-projection
         *  that fixed the columns in __system__ and never reached the topic.
         */
        if(current_topic) {
            json_int_t stored_topic_version = kw_get_int(
                gobj, current_topic, "topic_version", 0, KW_WILD_NUMBER
            );
            if(stored_topic_version > topic_version) {
                topic_version = stored_topic_version;
            }
            topic_version += 1;
        }

        json_t *kw_topic = json_pack("{s:s, s:s, s:s, s:s, s:I, s:b}",
            "value", topic_name,
            "pkey", pkey,
            "system_flag", system_flag,
            "tkey", tkey,
            "topic_version", (json_int_t )topic_version,
            "system_topic", system_topic
        );
        if(topic_pkey2s_) {
            json_object_set(kw_topic, "pkey2s", topic_pkey2s_);
        }
        if(current_topic) {
            /*  Keep the rowid: it is this topic's identity  */
            json_object_set(
                kw_topic,
                "id",
                json_object_get(current_topic, "id")
            );
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

        json_t *jn_cols = kwid_new_list(gobj, jn_topic, 0, "cols");
        if(!jn_cols) {
            json_decref(topic);
            continue;
        }

        int idx2; json_t *jn_col;
        json_array_foreach(jn_cols, idx2, jn_col) {
            const char *col_name = kw_get_str(gobj, jn_col, "id", "", KW_REQUIRED);
            if(empty_string(col_name)) {
                continue;
            }
            const char *header = kw_get_str(gobj, jn_col, "header", col_name, 0);
            const char *type = kw_get_str(gobj, jn_col, "type", "", KW_REQUIRED);
            if(empty_string(type)) {
                continue;
            }
            /*
             *  Copy every attribute the DESCRIPTOR declares, never a list
             *  written by hand here: that list is why `enum`, `template` and
             *  `pkey2s` never reached the projection. An attribute added to
             *  the descriptor did not add itself here, and the loss showed up
             *  only later, as a schema that had quietly changed — a column
             *  keeping its `enum` flag while its enumeration was gone.
             */
            json_t *kw_col = json_object();
            json_object_set_new(kw_col, "value", json_string(col_name));

            int idx3; json_t *desc_entry;
            json_array_foreach(cols_desc, idx3, desc_entry) {
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

            const char *col_id = current_topic?
                find_col_id(gobj, current_topic, col_name):
                NULL;

            json_t *col;
            if(col_id) {
                /*
                 *  Keep the rowid: it is this column's identity
                 */
                json_object_set_new(kw_col, "id", json_string(col_id));
                col = gobj_update_node(
                    priv->gobj_node_system,
                    "cols",
                    kw_col,
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
                    kw_col,
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
        json_decref(jn_cols);
        json_decref(topic);
    }

    /*
     *  free
     */
    json_object_del(priv->tranger_system_, "__schema_publishing__");
    JSON_DECREF(cols_desc)
    json_decref(treedb);

    return 0;
}

/***************************************************************************
 *  Keep the __system__ projection in step with the schema compiled in C.
 *
 *  Same rule treedb_open_db applies between that schema and the persisted
 *  schema file: the stored one wins on ties, and the incoming one has to be
 *  strictly newer to take over. So raising `schema_version` is what publishes
 *  a change, whichever side made it.
 ***************************************************************************/
PRIVATE int reconcile_treedb_schema(
    hgobj gobj,
    const char *treedb_name,
    json_t *jn_schema // not owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

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
        return upsert_treedb_schema(gobj, treedb_name, jn_schema, NULL);
    }

    /*
     *  Compare against the version of the LITERAL this projection came from,
     *  never against `schema_version` — that one belongs to whoever edits the
     *  schema here, and comparing against it would let one edit outrank every
     *  later release of the literal. Stores projected before `c_schema_version`
     *  existed fall back to it.
     */
    json_t *stored_treedb = json_array_get(stored, 0);
    json_int_t stored_version = kw_get_int(
        gobj,
        stored_treedb,
        "c_schema_version",
        0,
        KW_WILD_NUMBER
    );
    if(stored_version == 0) {
        stored_version = kw_get_int(gobj, stored_treedb, "schema_version", 0, KW_WILD_NUMBER);
    }

    /*
     *  A projection is a function of two things: the literal it came from and
     *  the meta-schema that says how a schema is stored. Comparing only the
     *  literal froze a projection made by an older SDK forever — and an older
     *  SDK is exactly the one whose projection may be missing what it did not
     *  know how to store yet (`enum` and `template` were). An absent field
     *  reads as 0, so a projection made before this existed re-projects on the
     *  next start, which is how those losses heal.
     */
    json_int_t stored_meta = kw_get_int(
        gobj,
        stored_treedb,
        "system_schema_version",
        0,
        KW_WILD_NUMBER
    );
    JSON_DECREF(stored)

    json_int_t new_version = kw_get_int(gobj, jn_schema, "schema_version", 1, KW_WILD_NUMBER);
    if(new_version <= stored_version && priv->system_schema_version <= stored_meta) {
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

    int ret = upsert_treedb_schema(gobj, treedb_name, jn_schema, current);
    JSON_DECREF(current)

    return ret;
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
     *  HACK Both `topics` and `cols` are keyed by rowid and carry their name
     *  in `value` (a name is unique only inside its parent), so the schema is
     *  re-keyed by name on the way out — that is the shape a schema has.
     */
    json_t *stored_topics = kw_get_dict(gobj, treedb, "topics", 0, KW_EXTRACT);
    json_t *topics = json_object();
    json_object_set_new(treedb, "topics", topics);

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
         *  HACK It's a topic with rowid and pkey2
         *  TODO en este tipo de tablas en el frontend
         *      tienen que salvar sin "id" (porque es rowid)
         *
         */
        json_t *new_cols = json_object();
        const char *col_name; json_t *col;
        json_object_foreach(cols, col_name, col) {
            /*
             *  TODO get pkey2 and interchange pkey by pkey2
             *
             *  HACK The id of a rowid record is his pkey2
             */
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
         *  Set new checked cols
         */
        json_object_set_new(topic, "cols", new_cols);

        json_decref(cols);
    }
    JSON_DECREF(stored_topics)

    return treedb;
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
     */
    if(input_schema_ok) {
        reconcile_treedb_schema(gobj, treedb_name, jn_client_treedb_schema);
    }

    /*
     *  A treedb opens from its projection, always. There used to be a flag
     *  (`use_internal_schema`) to open from the literal instead, and with it
     *  an edit made in __system__ reached nothing until every yuno's config
     *  was changed one by one. It distinguishes nothing now: the projection
     *  is seeded from the literal and re-made whenever the literal or the
     *  projector moves ahead, so opening from it IS opening from the literal
     *  until somebody edits it — which is the whole point.
     *
     *  The literal is still the fallback, for a projection that cannot be
     *  rebuilt into a valid schema.
     */
    json_t *client_treedb_schema = get_treedb_schema(gobj, treedb_name);
    if(client_treedb_schema) {
        if(parse_schema(client_treedb_schema)==0) {
            /*
             *  Use current last treedbs schema
             */
            return client_treedb_schema;
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB,
                "msg",          "%s", "Last treedb schema fails",
                NULL
            );
            gobj_trace_json(gobj, client_treedb_schema, "Last treedb schema fails");
            JSON_DECREF(client_treedb_schema);
            // continue below
        }
    }

    client_treedb_schema = json_incref(jn_client_treedb_schema);

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
        return -1;
    }

    int ret = 0;

// TODO falla, hay que revisar

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
