/***********************************************************************
 *          C_NODE.C
 *          Node GClass.
 *
 *          Nodes: resources with treedb
 *
 *          Copyright (c) 2020 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>

#include <gobj.h>
#include <g_events.h>
#include <g_states.h>
#include <helpers.h>
#include <command_parser.h>
#include <tr_treedb.h>
#include <timeranger2.h>

#include "msg_ievent.h"
#include "yunetas_environment.h"
#include "c_node.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int treedb_callback(
    void *user_data,
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *operation,  // EV_TREEDB_NODE_UPDATED,
                            // EV_TREEDB_NODE_UPDATED,
                            // EV_TREEDB_NODE_DELETED
    json_t *node            // owned
);

PRIVATE json_t *fetch_node(  // WARNING Return is NOT YOURS, pure node
    hgobj gobj,
    const char *topic_name,
    json_t *kw  // NOT owned, 'id' and pkey2s fields are used to find the node
);

PRIVATE int export_treedb(
    hgobj gobj,
    const char *path,
    BOOL with_metadata,
    BOOL without_rowid,
    hgobj src
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_create_node(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_update_node(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_node(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_link_nodes(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_unlink_nodes(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_treedbs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_topics(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_jtree(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_desc(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_hooks(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_links(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_parents(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_children(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_nodes(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_node(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_node_instances(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_node_pkey2s(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_snaps(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_snap_content(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_shoot_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_activate_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_deactivate_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_import_db(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_export_db(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t* cmd_system_topic_schema(hgobj gobj, const char* cmd, json_t* kw, hgobj src);

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

PRIVATE sdata_desc_t pm_jtree[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_STRING,    "node_id",      0,              0,          "Node id"),
SDATAPM (DTP_STRING,    "hook",         0,              0,          "Hook to build the tree"),
SDATAPM (DTP_STRING,    "rename_hook",  0,              0,          "Rename the hook field in the response"),
SDATAPM (DTP_JSON,      "filter",       0,              0,          "Filter to children"),
SDATAPM (DTP_JSON,      "options",      0,              0,          "Options: refs, hook_refs, fkey_refs, only_id, hook_only_id, fkey_only_id, list_dict, hook_list_dict, fkey_list_dict, size, hook_size"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_create_node[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_STRING,    "content64",    0,              0,          "Node content in base64"),
SDATAPM (DTP_JSON,      "record",       0,              0,          "Node content in json"),
SDATAPM (DTP_JSON,      "options",      0,              0,          "Options: refs, hook_refs, fkey_refs, only_id, hook_only_id, fkey_only_id, list_dict, hook_list_dict, fkey_list_dict, size, hook_size"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_update_node[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_STRING,    "content64",    0,              0,          "Node content in base64"),
SDATAPM (DTP_JSON,      "record",       0,              0,          "Node content in json"),
SDATAPM (DTP_JSON,      "options",      0,              0,          "Options: create, autolink, volatil, refs, hook_refs, fkey_refs, only_id, hook_only_id, fkey_only_id, list_dict, hook_list_dict, fkey_list_dict, size, hook_size"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_delete_node[] = {
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_JSON,      "record",       0,              0,          "Node content in json"),
SDATAPM (DTP_JSON,      "options",      0,              0,          "Options: 'force'"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_link_nodes[] = {
SDATAPM (DTP_STRING,    "parent_ref",   0,              0,          "Parent node ref (parent_topic_name^parent_id^hook_name)"),
SDATAPM (DTP_STRING,    "child_ref",    0,              0,          "Child node ref (child_topic_name^child_id)"),
SDATAPM (DTP_JSON,      "options",      0,              0,          "Options: create, autolink, volatil, refs, hook_refs, fkey_refs, only_id, hook_only_id, fkey_only_id, list_dict, hook_list_dict, fkey_list_dict, size, hook_size"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_trace[] = {
SDATAPM (DTP_BOOLEAN,   "set",          0,              0,          "Trace: set 1 o 0"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_topics[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "treedb_name",  0,              0,          "Treedb name"),
SDATAPM (DTP_JSON,      "options",      0,              0,          "Options: 'dict'"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_desc[] = {
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_hooks[] = {
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_links[] = {
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_parents[] = {
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_STRING,    "node_id",      0,              0,          "Node id"),
SDATAPM (DTP_STRING,    "link",         0,              0,          "Link port (fkey) to parents"),
SDATAPM (DTP_JSON,      "options",      0,              0,          "Options: refs, fkey_refs, only_id, fkey_only_id, list_dict, fkey_list_dict"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_children[] = {
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_STRING,    "node_id",      0,              0,          "Node id"),
SDATAPM (DTP_STRING,    "hook",         0,              0,          "Hook port to children"),
SDATAPM (DTP_JSON,      "filter",       0,              0,          "Filter to children"),
SDATAPM (DTP_JSON,      "options",      0,              0,          "Options: recursive, refs, hook_refs, only_id, hook_only_id, list_dict, fkey_list_dict, size, hook_size"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_list_nodes[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_JSON,      "filter",       0,              0,          "Search filter"),
SDATAPM (DTP_JSON,      "options",      0,              0,          "Options: refs, hook_refs, fkey_refs, only_id, hook_only_id, fkey_only_id, list_dict, hook_list_dict, fkey_list_dict, size, hook_size"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_get_node[] = {
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_STRING,    "node_id",      0,              0,          "Node id"),
SDATAPM (DTP_JSON,      "options",      0,              0,          "Options: refs, hook_refs, fkey_refs, only_id, hook_only_id, fkey_only_id, list_dict, hook_list_dict, fkey_list_dict, size, hook_size"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_node_instances[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_STRING,    "node_id",      0,              0,          "Node id"),
SDATAPM (DTP_STRING,    "pkey2",        0,              0,          "PKey2 field"),
SDATAPM (DTP_JSON,      "filter",       0,              0,          "Search filter"),
SDATAPM (DTP_JSON,      "options",      0,              0,          "Options: only_id, list_dict, size"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_node_pkey2s[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_snap_content[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "snap_id",      0,              0,          "Snap id"),
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_shoot_snap[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              0,          "Snap name"),
SDATAPM (DTP_STRING,    "description",  0,              0,          "Snap description"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_activate_snap[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,              0,          "Snap name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_import_db[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "content64",    0,              0,          "Content in base64"),
SDATAPM (DTP_STRING,    "if-resource-exists", 0,        0,          "abort, skip, overwrite"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_export_db[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "filename",     0,              0,          "Filename to save db"),
SDATAPM (DTP_BOOLEAN,   "overwrite",    0,              0,          "Overwrite the file if it exits"),
SDATAPM (DTP_BOOLEAN,   "with_metadata",0,              0,          "Write metadata"),
SDATAPM (DTP_BOOLEAN,   "without_rowid",0,              "0",        "Without id in records with rowid id"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};
PRIVATE const char *a_nodes[] = {"list-nodes", "list-records", 0};
PRIVATE const char *a_node[] = {"get-node", "get-record", 0};
PRIVATE const char *a_create[] = {"create-record", 0};
PRIVATE const char *a_update[] = {"update-record", 0};
PRIVATE const char *a_delete[] = {"delete-record", 0};
PRIVATE const char *a_schemas[] = {"schemas","list-schemas", 0};
PRIVATE const char *a_schema[] = {"schema", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-------json_fn---------description--*/
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,    cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "authzs",           0,          pm_authzs,  cmd_authzs,     "Authorization's help"),

/*-CMD2---type----------name------------flag------------ali-items-----------json_fn-------------description--*/
SDATACM2 (DTP_SCHEMA,   "treedbs",      SDF_AUTHZ_X,    0,  0,              cmd_treedbs,        "List treedb's"),
SDATACM2 (DTP_SCHEMA,   "topics",       SDF_AUTHZ_X,    0,  pm_topics,      cmd_topics,         "List topics"),
SDATACM2 (DTP_SCHEMA,   "jtree",        SDF_AUTHZ_X,    0,  pm_jtree,       cmd_jtree,          "List hierarchical tree (topic with self-link). Webix option return dict-list else return list of dicts. Always with __path__ "),
SDATACM2 (DTP_SCHEMA,   "create-node",  SDF_AUTHZ_X,    a_create, pm_create_node, cmd_create_node, "Create node"),
SDATACM2 (DTP_SCHEMA,   "update-node",  SDF_AUTHZ_X,    a_update, pm_update_node, cmd_update_node, "Update node"),
SDATACM2 (DTP_SCHEMA,   "delete-node",  SDF_AUTHZ_X,    a_delete, pm_delete_node, cmd_delete_node, "Delete node"),
SDATACM2 (DTP_SCHEMA,   "nodes",        SDF_AUTHZ_X,    a_nodes, pm_list_nodes, cmd_list_nodes,  "List nodes"),
SDATACM2 (DTP_SCHEMA,   "node",         SDF_AUTHZ_X,    a_node, pm_get_node, cmd_get_node,       "Get node by id"),
SDATACM2 (DTP_SCHEMA,   "instances",    SDF_AUTHZ_X,    0,  pm_node_instances,cmd_node_instances,"List node's instances"),
SDATACM2 (DTP_SCHEMA,   "link-nodes",   SDF_AUTHZ_X,    0,  pm_link_nodes,  cmd_link_nodes,     "Link nodes"),
SDATACM2 (DTP_SCHEMA,   "unlink-nodes", SDF_AUTHZ_X,    0,  pm_link_nodes,  cmd_unlink_nodes,   "Unlink nodes"),
SDATACM2 (DTP_SCHEMA,   "hooks",        SDF_AUTHZ_X,    0,  pm_hooks,       cmd_hooks,          "Hooks of node"),
SDATACM2 (DTP_SCHEMA,   "links",        SDF_AUTHZ_X,    0,  pm_links,       cmd_links,          "Links of node"),
SDATACM2 (DTP_SCHEMA,   "parents",      SDF_AUTHZ_X,    0,  pm_parents,     cmd_parents,        "Parent Refs of node"),
SDATACM2 (DTP_SCHEMA,   "children",       SDF_AUTHZ_X,    0,  pm_children,      cmd_children,         "Childs of node"),
SDATACM2 (DTP_SCHEMA,   "snaps",        SDF_AUTHZ_X,    0,  0,              cmd_list_snaps,     "List snaps"),
SDATACM2 (DTP_SCHEMA,   "snap-content", SDF_AUTHZ_X,    0,  pm_snap_content,cmd_snap_content,   "Show snap content"),
SDATACM2 (DTP_SCHEMA,   "shoot-snap",   SDF_AUTHZ_X,    0,  pm_shoot_snap,  cmd_shoot_snap,     "Shoot snap"),
SDATACM2 (DTP_SCHEMA,   "activate-snap",SDF_AUTHZ_X,    0,  pm_activate_snap,cmd_activate_snap, "Activate snap"),
SDATACM2 (DTP_SCHEMA,   "deactivate-snap",SDF_AUTHZ_X,  0,  0,              cmd_deactivate_snap,"De-Activate snap"),
SDATACM2 (DTP_SCHEMA,   "import-db",    SDF_AUTHZ_X,    0,  pm_import_db,   cmd_import_db, "Import db"),
SDATACM2 (DTP_SCHEMA,   "export-db",    SDF_AUTHZ_X,    0,  pm_export_db,   cmd_export_db, "Export db"),
SDATACM2 (DTP_SCHEMA,   "pkey2s",       SDF_AUTHZ_X,    0,  pm_node_pkey2s, cmd_node_pkey2s,    "List node's pkey2"),
SDATACM2 (DTP_SCHEMA,   "desc",         SDF_AUTHZ_X,    a_schema, pm_desc,  cmd_desc,           "Schema of topic"),
SDATACM2 (DTP_SCHEMA,   "descs",        SDF_AUTHZ_X,    a_schemas, 0,       cmd_desc,           "Schema of topics"),
SDATACM (DTP_SCHEMA,    "system-schema",0,              0,  cmd_system_topic_schema, "Get system schema"),
SDATACM2 (DTP_SCHEMA,   "trace",        SDF_AUTHZ_X,    0,  pm_trace,       cmd_trace,          "Set trace"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (DTP_POINTER,     "tranger",          0,                  0,              "Tranger handler, EXTERNALLY set, IMPORTANT!"),
SDATA (DTP_STRING,      "treedb_name",      SDF_RD|SDF_REQUIRED,"",             "Treedb name"),
SDATA (DTP_JSON,        "treedb_schema",    SDF_RD|SDF_REQUIRED,0,              "Treedb schema"),
SDATA (DTP_INTEGER,     "exit_on_error",    0,                  "2",            "exit on error, 2=LOG_OPT_EXIT_ZERO"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,              "subscriber of output-events. Not a child gobj."),
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

PRIVATE sdata_desc_t pm_authz_create[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATAPM0 (DTP_STRING,    "treedb_name",      0,          "",             "Treedb name"),
SDATAPM0 (DTP_STRING,    "topic_name",       0,          "",             "Topic name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authz_write[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATAPM0 (DTP_STRING,    "treedb_name",      0,          "",             "Treedb name"),
SDATAPM0 (DTP_STRING,    "topic_name",       0,          "",             "Topic name"),
SDATAPM0 (DTP_STRING,    "id",               0,          "record`id",    "Node Id"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authz_read[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATAPM0 (DTP_STRING,    "treedb_name",      0,          "__md_treedb__`treedb_name",             "Treedb name"),
SDATAPM0 (DTP_STRING,    "topic_name",       0,          "__md_treedb__`topic_name", "Topic name"),
SDATAPM0 (DTP_STRING,    "id",               0,          "id",           "Node Id"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authz_delete[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATAPM0 (DTP_STRING,    "treedb_name",      0,          "",             "Treedb name"),
SDATAPM0 (DTP_STRING,    "topic_name",       0,          "",             "Topic name"),
SDATAPM0 (DTP_STRING,    "id",               0,          "record`id",    "Node Id"),
SDATA_END()
};

PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name------------flag----alias---items---------------description--*/
SDATAAUTHZ (DTP_SCHEMA, "create",       0,      0,      pm_authz_create,    "Permission to create nodes"),
SDATAAUTHZ (DTP_SCHEMA, "update",       0,      0,      pm_authz_write,     "Permission to update nodes"),
SDATAAUTHZ (DTP_SCHEMA, "read",         0,      0,      pm_authz_read,      "Permission to read nodes"),
SDATAAUTHZ (DTP_SCHEMA, "delete",       0,      0,      pm_authz_delete,    "Permission to delete nodes"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    json_t *tranger;
    const char *treedb_name;
    json_t *treedb_schema;
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
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(tranger,                   gobj_read_pointer_attr)
    SET_PRIV(treedb_name,               gobj_read_str_attr)
    SET_PRIV(treedb_schema,             gobj_read_json_attr)
    SET_PRIV(exit_on_error,             gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    IF_EQ_SET_PRIV(tranger,     gobj_read_pointer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->tranger) {
        gobj_log_critical(gobj, priv->exit_on_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger NULL",
            NULL
        );
    }

    BOOL master = kw_get_bool(gobj, priv->tranger, "master", 0, KW_REQUIRED);

    if(!priv->treedb_schema) {
        if(master) {
            gobj_log_critical(gobj, priv->exit_on_error,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "treedb_schema NULL",
                NULL
            );
        }
    }
    if(empty_string(priv->treedb_name)) {
        gobj_log_critical(gobj, priv->exit_on_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "treedb_name name EMPTY",
            NULL
        );
        return -1;
    }

    json_t *treedb = treedb_open_db( // Return IS NOT YOURS!
        priv->tranger,
        priv->treedb_name,
        json_incref(priv->treedb_schema),  // owned
        "persistent"
    );

    treedb_set_callback(
        priv->tranger,
        priv->treedb_name,
        treedb_callback,
        gobj
    );

    return treedb?0:-1;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    treedb_close_db(priv->tranger, priv->treedb_name);

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_trace_on(hgobj gobj, const char *level, json_t *kw)
{
    treedb_set_trace(TRUE);
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_trace_off(hgobj gobj, const char *level, json_t *kw)
{
    treedb_set_trace(FALSE);
    KW_DECREF(kw)
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

    return treedb_list_treedb(
        priv->tranger,
        kw
    );
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_treedb_topics(
    hgobj gobj,
    const char *treedb_name,
    json_t *options, // "dict" return list of dicts, otherwise return list of strings
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return treedb_topics(
        priv->tranger,
        empty_string(treedb_name)?priv->treedb_name:treedb_name,
        options
    );
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_topic_desc(hgobj gobj, const char *topic_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!empty_string(topic_name)) {
        /*-----------------------------------*
         *      Check appropriate topic
         *-----------------------------------*/
        if(!treedb_is_treedbs_topic(
            priv->tranger,
            priv->treedb_name,
            topic_name
        )) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Topic name not found in treedbs",
                "treedb_name",  "%s", priv->treedb_name,
                "topic_name",   "%s", topic_name,
                NULL
            );
            return 0;
        }
        return tranger2_topic_desc(priv->tranger, topic_name);

    } else {
        json_t *topics_list = treedb_topics( //Return a list with topic names of the treedb
            priv->tranger,
            priv->treedb_name,
            0
        );
        json_t *jn_topics_desc = json_object();
        int idx; json_t *jn_topic_name;
        json_array_foreach(topics_list, idx, jn_topic_name) {
            topic_name = json_string_value(jn_topic_name);
            json_object_set_new(
                jn_topics_desc,
                topic_name,
                tranger2_topic_desc(priv->tranger, topic_name)
            );
        }
        json_decref(topics_list);
        return jn_topics_desc;
    }
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_topic_links(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!empty_string(topic_name)) {
        /*-----------------------------------*
         *      Check appropriate topic
         *-----------------------------------*/
        if(!treedb_is_treedbs_topic(
            priv->tranger,
            priv->treedb_name,
            topic_name
        )) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Topic name not found in treedbs",
                "treedb_name",  "%s", priv->treedb_name,
                "topic_name",   "%s", topic_name,
                NULL
            );
            KW_DECREF(kw)
            return 0;
        }

        KW_DECREF(kw)
        return treedb_get_topic_links(priv->tranger, treedb_name, topic_name);
    }

    // All topics
    json_t *topics = treedb_topics(
        priv->tranger,
        priv->treedb_name,
        0
    );

    json_t *links = json_object();
    int idx; json_t *jn_topic_name;
    json_array_foreach(topics, idx, jn_topic_name) {
        const char *topic_name_ = json_string_value(jn_topic_name);
        json_object_set_new(
            links,
            topic_name,
            treedb_get_topic_links(priv->tranger, treedb_name, topic_name_)
        );
    }
    JSON_DECREF(topics)
    KW_DECREF(kw)
    return links;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_topic_hooks(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!empty_string(topic_name)) {
        /*-----------------------------------*
         *      Check appropriate topic
         *-----------------------------------*/
        if(!treedb_is_treedbs_topic(
            priv->tranger,
            priv->treedb_name,
            topic_name
        )) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Topic name not found in treedbs",
                "treedb_name",  "%s", priv->treedb_name,
                "topic_name",   "%s", topic_name,
                NULL
            );
            KW_DECREF(kw)
            return 0;
        }

        KW_DECREF(kw)
        return treedb_get_topic_hooks(priv->tranger, treedb_name, topic_name);
    }

    // All topics
    json_t *topics = treedb_topics(
        priv->tranger,
        priv->treedb_name,
        0
    );

    json_t *hooks = json_object();
    int idx; json_t *jn_topic_name;
    json_array_foreach(topics, idx, jn_topic_name) {
        const char *topic_name_ = json_string_value(jn_topic_name);
        json_object_set_new(
            hooks,
            topic_name,
            treedb_get_topic_hooks(priv->tranger, treedb_name, topic_name_)
        );
    }

    JSON_DECREF(topics)
    KW_DECREF(kw)
    return hooks;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_create_node( // Return is YOURS
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options, // fkey,hook options
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        topic_name
    )) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_options)
        KW_DECREF(kw)
        return 0;
    }

    json_t *node = treedb_create_node( // Return is NOT YOURS
        priv->tranger,
        priv->treedb_name,
        topic_name,
        kw // owned
    );
    if(!node) {
        JSON_DECREF(jn_options)
        return 0;
    }

    return node_collapsed_view( // Return MUST be decref
        priv->tranger,
        node, // not owned
        jn_options // owned fkey,hook options
    );
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE size_t mt_topic_size(
    hgobj gobj,
    const char *topic_name,
    const char *key
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return treedb_topic_size(priv->tranger, priv->treedb_name, topic_name);
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_update_node( // Return is YOURS
    hgobj gobj,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    json_t *jn_options, // "create" "autolink" "volatil" fkey,hook options
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!jn_options) {
        jn_options = json_object();
    }

    BOOL volatil = kw_get_bool(gobj, jn_options, "volatil", 0, KW_WILD_NUMBER);
    BOOL create = kw_get_bool(gobj, jn_options, "create", 0, KW_WILD_NUMBER);
    BOOL autolink = kw_get_bool(gobj, jn_options, "autolink", 0, KW_WILD_NUMBER);

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        topic_name
    )) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_options)
        KW_DECREF(kw)
        return 0;
    }

    json_t *node = fetch_node(gobj, topic_name, kw);
    if(!node) {
        if(create) {
            node = treedb_create_node( // Return is NOT YOURS
                priv->tranger,
                priv->treedb_name,
                topic_name,
                kw_incref(kw)
            );
        }
        if(!node) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "node not found",
                "treedb_name",  "%s", priv->treedb_name,
                "topic_name",   "%s", topic_name,
                NULL
            );
            gobj_trace_json(gobj, kw, "node not found");
            JSON_DECREF(jn_options)
            KW_DECREF(kw)
            return 0;
        }
    } else {
        /*
         *  If node exists then it's an update
         */
        create = 0;
    }

    if(volatil) {
        set_volatil_values(
            priv->tranger,
            topic_name,
            node,  // NOT owned
            kw // NOT owned
        );

    } else {
        if(!create) {
            treedb_update_node( // Return is NOT YOURS
                priv->tranger,
                node,
                json_incref(kw),
                autolink?FALSE:TRUE
            );
        }
        if(autolink) {
            treedb_clean_node(priv->tranger, node, FALSE);  // remove current links
            treedb_autolink(priv->tranger, node, json_incref(kw), FALSE);
            treedb_save_node(priv->tranger, node);
        }
    }

    json_decref(kw);

    return node_collapsed_view( // Return MUST be decref
        priv->tranger,
        node, // not owned
        jn_options // owned fkey,hook options
    );
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_delete_node(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    json_t *jn_options, // "force"
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        topic_name
    )) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_options)
        KW_DECREF(kw)
        return 0;
    }

    const char *id = kw_get_str(gobj, kw, "id", 0, 0);
    if(empty_string(id)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "id required to delete node",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(kw)
        JSON_DECREF(jn_options)
        return -1;
    }

    json_t *main_node = treedb_get_node( // WARNING Return is NOT YOURS, pure node
        priv->tranger,
        priv->treedb_name,
        topic_name,
        id
    );
    if(!main_node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "node not found",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(kw)
        JSON_DECREF(jn_options)
        return -1;
    }

    json_t *node = 0;
    int ret = 0;
    json_t *jn_filter = json_object();

    /*
     *  Check if it has secondary keys
     */
    json_t *pkey2s_list = treedb_topic_pkey2s(priv->tranger, topic_name);
    int idx; json_t *jn_pkey2_name;
    json_array_foreach(pkey2s_list, idx, jn_pkey2_name) {
        const char *pkey2_name = json_string_value(jn_pkey2_name);
        const char *key2 = kw_get_str(gobj, kw, pkey2_name, 0, 0);
        node = treedb_get_instance( // WARNING Return is NOT YOURS, pure node
            priv->tranger,
            priv->treedb_name,
            topic_name,
            pkey2_name,
            id,     // primary key
            key2    // secondary key
        );
        if(node && node != main_node) {
            json_object_set_new(jn_filter, pkey2_name, json_string(key2));
            ret += treedb_delete_instance(
                priv->tranger,
                node,
                pkey2_name,
                json_incref(jn_options)
            );
        }
    }
    json_decref(pkey2s_list);

    if(kw_match_simple(main_node, jn_filter)) {
        int r = treedb_delete_node(
            priv->tranger,
            main_node,
            json_incref(jn_options)
        );
        ret += r;
    }

    JSON_DECREF(kw)
    JSON_DECREF(jn_options)
    return ret;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_link_nodes(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent_record,  // owned
    const char *child_topic_name,
    json_t *child_record,  // owned
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        parent_topic_name
    )) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", parent_topic_name,
            NULL
        );
        JSON_DECREF(parent_record)
        JSON_DECREF(child_record)
        return -1;
    }

    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        child_topic_name
    )) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", child_topic_name,
            NULL
        );
        JSON_DECREF(parent_record)
        JSON_DECREF(child_record)
        return -1;
    }

    /*-------------------------------*
     *      Recover nodes
     *-------------------------------*/
    //const char *parent_topic_name = kw_get_str(parent_record, "__md_treedb__`topic_name", 0, 0);
    //const char *child_topic_name = kw_get_str(child_record, "__md_treedb__`topic_name", 0, 0);

    json_t *parent_node = fetch_node( // WARNING Return is NOT YOURS, pure node
        gobj,
        parent_topic_name,
        parent_record
    );
    if(!parent_node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "parent node not found",
            NULL
        );
        gobj_trace_json(gobj, parent_record, "node not found");
        JSON_DECREF(parent_record)
        JSON_DECREF(child_record)
        return -1;
    }

    json_t *child_node = fetch_node( // WARNING Return is NOT YOURS, pure node
        gobj,
        child_topic_name,
        child_record
    );
    if(!child_node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "child node not found",
            NULL
        );
        gobj_trace_json(gobj, child_record, "node not found");
        JSON_DECREF(parent_record)
        JSON_DECREF(child_record)
        return -1;
    }

    JSON_DECREF(parent_record)
    JSON_DECREF(child_record)
    return treedb_link_nodes(
        priv->tranger,
        hook,
        parent_node,
        child_node
    );
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_unlink_nodes(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent_record,  // owned
    const char *child_topic_name,
    json_t *child_record,  // owned
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        parent_topic_name
    )) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", parent_topic_name,
            NULL
        );
        JSON_DECREF(parent_record)
        JSON_DECREF(child_record)
        return -1;
    }

    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        child_topic_name
    )) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", child_topic_name,
            NULL
        );
        JSON_DECREF(parent_record)
        JSON_DECREF(child_record)
        return -1;
    }

    /*-------------------------------*
     *      Recover nodes
     *-------------------------------*/
    //const char *parent_topic_name = kw_get_str(parent_record, "__md_treedb__`topic_name", 0, 0);
    //const char *child_topic_name = kw_get_str(child_record, "__md_treedb__`topic_name", 0, 0);

    json_t *parent_node = fetch_node( // WARNING Return is NOT YOURS, pure node
        gobj,
        parent_topic_name,
        parent_record
    );
    if(!parent_node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "parent node not found",
            NULL
        );
        gobj_trace_json(gobj, parent_record, "node not found");
        JSON_DECREF(parent_record)
        JSON_DECREF(child_record)
        return -1;
    }

    json_t *child_node = fetch_node( // WARNING Return is NOT YOURS, pure node
        gobj,
        child_topic_name,
        child_record
    );
    if(!child_node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "child node not found",
            NULL
        );
        gobj_trace_json(gobj, child_record, "node not found");
        JSON_DECREF(parent_record)
        JSON_DECREF(child_record)
        return -1;
    }

    JSON_DECREF(parent_record)
    JSON_DECREF(child_record)
    return treedb_unlink_nodes(
        priv->tranger,
        hook,
        parent_node,
        child_node
    );
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_get_node(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    json_t *jn_options, // fkey,hook options
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        topic_name
    )) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_options)
        KW_DECREF(kw)
        return 0;
    }

    json_t *node = fetch_node(gobj, topic_name, kw);
    if(!node) {
        // Silence
        JSON_DECREF(jn_options)
        KW_DECREF(kw)
        return 0;
    }

    KW_DECREF(kw)
    return node_collapsed_view( // Return MUST be decref
        priv->tranger, // not owned
        node, // not owned
        jn_options // owned
    );
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_list_nodes(
    hgobj gobj,
    const char *topic_name,
    json_t *jn_filter,
    json_t *jn_options, // "include-instances" fkey,hook options
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!jn_options) {
        jn_options = json_object();
    }

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        topic_name
    )) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        return 0;
    }

    BOOL include_instances = kw_get_bool(gobj, jn_options, "include-instances", 0, KW_WILD_NUMBER);

    /*
     *  Search in main list
     */
    json_t *iter = treedb_list_nodes(
        priv->tranger,
        priv->treedb_name,
        topic_name,
        json_incref(jn_filter),
        0
    );

    if(json_array_size(iter)==0 && include_instances) {
        /*
         *  Search in instances
         */
        json_decref(iter);
        iter = treedb_list_instances(
            priv->tranger,
            priv->treedb_name,
            topic_name,
            "",
            json_incref(jn_filter),
            0
        );
    }

    json_t *list = json_array();
    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_array_append_new(
            list,
            node_collapsed_view( // TODO añade opción "expand"
                priv->tranger,
                node,
                json_incref(jn_options)
            )
        );
    }
    json_decref(iter);

    JSON_DECREF(jn_filter)
    JSON_DECREF(jn_options)

    return list;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_list_instances(
    hgobj gobj,
    const char *topic_name,
    const char *pkey2,
    json_t *jn_filter,
    json_t *jn_options, // fkey,hook options
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        topic_name
    )) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        return 0;
    }

    // TODO Filtra la lista con los nodos con permiso para leer

    json_t *iter = treedb_list_instances( // Return MUST be decref
        priv->tranger,
        priv->treedb_name,
        topic_name,
        pkey2,
        json_incref(jn_filter),
        0
    );

    json_t *list = json_array();
    int idx; json_t *node;
    json_array_foreach(iter, idx, node) {
        json_array_append_new(
            list,
            node_collapsed_view(
                priv->tranger,
                node,
                json_incref(jn_options)
            )
        );
    }
    json_decref(iter);

    JSON_DECREF(jn_filter)
    JSON_DECREF(jn_options)
    return list;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_node_parents(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    const char *link,   // fkey
    json_t *jn_options, // owned , fkey options
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        topic_name
    )) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }

    json_t *node = fetch_node(gobj, topic_name, kw);
    if(!node) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Node not found",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            "kw",           "%j", kw,
            NULL
        );
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }

    /*
     *  Return a list of parent nodes pointed by the link (fkey)
     */
    if(!empty_string(link)) {
        JSON_DECREF(kw)
        return treedb_parent_refs( // Return MUST be decref
            priv->tranger,
            link, // must be a fkey field
            node, // not owned
            jn_options
        );
    }

    /*
     *  If no link return all links
     */
    json_t *parents = json_array();
    json_t *links = treedb_get_topic_links(priv->tranger, priv->treedb_name, topic_name);
    int idx; json_t *jn_link;
    json_array_foreach(links, idx, jn_link) {
        json_t *parents_ = treedb_parent_refs( // Return MUST be decref
            priv->tranger,
            json_string_value(jn_link), // must be a fkey field
            node, // not owned
            json_incref(jn_options)
        );
        json_array_extend(parents, parents_);
        JSON_DECREF(parents_);
    }
    JSON_DECREF(links)

    JSON_DECREF(jn_options)
    JSON_DECREF(kw)
    return parents;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_node_children(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    const char *hook,
    json_t *jn_filter,  // filter to children tree
    json_t *jn_options, // fkey,hook options, "recursive"
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        topic_name
    )) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }

    json_t *node = fetch_node(gobj, topic_name, kw);
    if(!node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Node not found",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            "kw",           "%j", kw,
            NULL
        );
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }

    /*
     *  Return a list of child nodes of the hook
     */
    json_t *iter = treedb_node_children( // Return MUST be decref
        priv->tranger,
        hook, // must be a hook field
        node, // not owned
        json_incref(jn_filter),
        json_incref(jn_options)
    );

    if(!iter) {
        // Error already logged
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }

    json_t *children = json_array();

    int idx; json_t *child;
    json_array_foreach(iter, idx, child) {
        json_array_append_new(
            children,
            node_collapsed_view(
                priv->tranger,
                child,
                json_incref(jn_options)
            )
        );
    }
    json_decref(iter);

    JSON_DECREF(jn_filter);
    JSON_DECREF(jn_options)
    JSON_DECREF(kw)
    return children;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_topic_jtree(
    hgobj gobj,
    const char *topic_name,
    const char *hook,   // hook to build the hierarchical tree
    const char *rename_hook, // change the hook name in the tree response
    json_t *kw,         // 'id' and pkey2s fields are used to find the root node
    json_t *jn_filter,  // filter to match records
    json_t *jn_options, // fkey,hook options
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        topic_name
    )) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }

    /*
     *  If root node is not specified then the first with no parent is used
     */
    json_t *node = fetch_node(gobj, topic_name, kw);
    if(!node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Node not found",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            "kw",           "%j", kw,
            NULL
        );
        JSON_DECREF(jn_filter);
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }

    /*
     *  Return a tree of child nodes of the hook
     */
    JSON_DECREF(kw)
    return treedb_node_jtree( // Return MUST be decref
        priv->tranger,
        hook, // must be a hook field
        rename_hook,
        node, // not owned
        jn_filter,
        jn_options
    );
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_node_tree(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the root node
    json_t *jn_options, // "with_metatada"
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!jn_options) {
        jn_options = json_object();
    }

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(
        priv->tranger,
        priv->treedb_name,
        topic_name
    )) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }

    /*
     *  If root node is not specified then the first with no parent is used
     */
    json_t *node = fetch_node(gobj, topic_name, kw);
    if(!node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Node not found",
            "treedb_name",  "%s", priv->treedb_name,
            "topic_name",   "%s", topic_name,
            "kw",           "%j", kw,
            NULL
        );
        JSON_DECREF(jn_options)
        JSON_DECREF(kw)
        return 0;
    }

    /*
     *  Return the duplicated full node
     */
    JSON_DECREF(jn_options)
    JSON_DECREF(kw)

    BOOL with_metadata = kw_get_bool(gobj, jn_options, "with_metadata", 0, KW_WILD_NUMBER);

    if(with_metadata) {
        return json_deep_copy(node);
    } else {
        return kw_filter_metadata(gobj, json_incref(node));
    }
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_shoot_snap(
    hgobj gobj,
    const char *name,
    json_t *kw,
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *description = kw_get_str(gobj, kw, "description", "", 0);
    int ret = treedb_shoot_snap(
        priv->tranger,
        priv->treedb_name,
        name,
        description
    );
    KW_DECREF(kw)
    return ret;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_activate_snap(
    hgobj gobj,
    const char *tag,
    json_t *kw,
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    KW_DECREF(kw)

    return treedb_activate_snap(
        priv->tranger,
        priv->treedb_name,
        tag
    );
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE json_t *mt_list_snaps(
    hgobj gobj,
    json_t *filter,
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return treedb_list_snaps(
        priv->tranger,
        priv->treedb_name,
        filter
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
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_create_node(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    const char *content64 = kw_get_str(gobj, kw, "content64", "", 0);
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0);

    if(empty_string(topic_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What topic_name?"),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Get content
     *  Priority: content64, record
     *----------------------------------------*/
    json_t *jn_content = 0;
    if(!empty_string(content64)) {
        /*
         *  Get content in base64 and decode
         */
        gbuffer_t *gbuf_content = gbuffer_base64_to_string(content64, strlen(content64));
        jn_content = legalstring2json(gbuffer_cur_rd_pointer(gbuf_content), TRUE);
        GBUFFER_DECREF(gbuf_content);
        if(!jn_content) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Can't decode json content64"),
                0,
                0,
                kw  // owned
            );
        }
    }

    if(!jn_content) {
        jn_content = kw_incref(kw_get_dict(gobj, kw, "record", 0, 0));
    } else {
        // To authz
        json_object_set(kw, "record", jn_content);
    }

    if(!jn_content) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What record?"),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "create";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        json_decref(jn_content);
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    json_t *node = gobj_create_node(
        gobj,
        topic_name,
        jn_content, // owned
        json_incref(_jn_options),
        src
    );
    return msg_iev_build_response(gobj,
        node?0:-1,
        json_sprintf("%s", node?"Node created!":gobj_log_last_message()),
        gobj_topic_desc(gobj, topic_name),
        node,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_update_node(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    const char *content64 = kw_get_str(gobj, kw, "content64", "", 0);
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0);

    if(empty_string(topic_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What topic_name?"),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Get content
     *  Priority: content64, record
     *----------------------------------------*/
    json_t *jn_content = 0;
    if(!empty_string(content64)) {
        /*
         *  Get content in base64 and decode
         */
        gbuffer_t *gbuf_content = gbuffer_base64_to_string(content64, strlen(content64));
        jn_content = legalstring2json(gbuffer_cur_rd_pointer(gbuf_content), TRUE);
        GBUFFER_DECREF(gbuf_content)
        if(!jn_content) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Can't decode json content64"),
                0,
                0,
                kw  // owned
            );
        }
    }

    if(!jn_content) {
        jn_content = json_incref(kw_get_dict(gobj, kw, "record", 0, 0));
    } else {
        // To authz
        json_object_set(kw, "record", jn_content);
    }

    if(!jn_content) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What record?"),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "update";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        json_decref(jn_content);
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    json_t *node = gobj_update_node(
        gobj,
        topic_name,
        jn_content, // owned
        json_incref(_jn_options),
        src
    );

    return msg_iev_build_response(gobj,
        node?0:-1,
        json_sprintf("%s", node?"Node update!":gobj_log_last_message()),
        gobj_topic_desc(gobj, topic_name),
        node,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_node(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0);
    json_t *_jn_record = kw_get_dict_value(gobj, kw, "record", 0, 0);

    if(empty_string(topic_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What topic_name?"),
            0,
            0,
            kw  // owned
        );
    }

    if(!kw_has_key(_jn_record, "id")) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("field 'id' is required to delete nodes"),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "delete";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Get a iter of matched resources.
     */
    json_t *node = gobj_get_node(
        gobj,
        topic_name,
        json_incref(_jn_record),
        0,
        src
    );
    if(!node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Node not found"),
            0,
            0,
            kw  // owned
        );
    }

    JSON_INCREF(node);
    if(gobj_delete_node(
            gobj,
            topic_name,
            node,
            json_incref(_jn_options),
            src
    )<0) {
        JSON_DECREF(node);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s", gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Node deleted"),
        gobj_topic_desc(gobj, topic_name),
        node,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_link_nodes(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *parent_ref = kw_get_str(gobj, kw, "parent_ref", "", 0);
    const char *child_ref = kw_get_str(gobj, kw, "child_ref", "", 0);
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0);

    if(empty_string(parent_ref)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What parent ref?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(child_ref)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What child ref?"),
            0,
            0,
            kw  // owned
        );
    }

    char parent_topic_name[NAME_MAX];
    char parent_id[NAME_MAX];
    char hook_name[NAME_MAX];
    if(!decode_parent_ref(
        parent_ref,
        parent_topic_name, sizeof(parent_topic_name),
        parent_id, sizeof(parent_id),
        hook_name, sizeof(hook_name)
    )) {
        /*
         *  It's not a fkey.
         *  It's not an error, it happens when it's a hook and fkey field.
         */
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Wrong parent ref"),
            0,
            0,
            kw  // owned
        );
    }

    char child_topic_name[NAME_MAX];
    char child_id[NAME_MAX];

    if(!decode_child_ref(
        child_ref,
        child_topic_name, sizeof(child_topic_name),
        child_id, sizeof(child_id)
    )) {
        // It's not a child ref
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Wrong child ref"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *parent_node = gobj_get_node(
        gobj,
        parent_topic_name,
        json_pack("{s:s}", "id", parent_id),
        0,
        src
    );
    if(!parent_node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Parent not found"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *child_node = gobj_get_node(
        gobj,
        child_topic_name,
        json_pack("{s:s}", "id", child_id),
        0,
        src
    );
    if(!child_node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Parent not found"),
            0,
            0,
            kw  // owned
        );
    }

    int result = gobj_link_nodes(
        gobj,
        hook_name,
        parent_topic_name,
        parent_node,
        child_topic_name,
        child_node,
        src
    );

    child_node = gobj_get_node(
        gobj,
        child_topic_name,
        json_pack("{s:s}", "id", child_id),
        json_incref(_jn_options),
        src
    );

    return msg_iev_build_response(gobj,
        result,
        result<0?json_sprintf("%s", gobj_log_last_message()):json_sprintf("Nodes linked!"),
        gobj_topic_desc(gobj, child_topic_name),
        child_node,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_unlink_nodes(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *parent_ref = kw_get_str(gobj, kw, "parent_ref", "", 0);
    const char *child_ref = kw_get_str(gobj, kw, "child_ref", "", 0);
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0);

    if(empty_string(parent_ref)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What parent ref?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(child_ref)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What child ref?"),
            0,
            0,
            kw  // owned
        );
    }

    char parent_topic_name[NAME_MAX];
    char parent_id[NAME_MAX];
    char hook_name[NAME_MAX];
    if(!decode_parent_ref(
        parent_ref,
        parent_topic_name, sizeof(parent_topic_name),
        parent_id, sizeof(parent_id),
        hook_name, sizeof(hook_name)
    )) {
        /*
         *  It's not a fkey.
         *  It's not an error, it happens when it's a hook and fkey field.
         */
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Wrong parent ref"),
            0,
            0,
            kw  // owned
        );
    }

    char child_topic_name[NAME_MAX];
    char child_id[NAME_MAX];

    if(!decode_child_ref(
        child_ref,
        child_topic_name, sizeof(child_topic_name),
        child_id, sizeof(child_id)
    )) {
        // It's not a child ref
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Wrong child ref"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *parent_node = gobj_get_node(
        gobj,
        parent_topic_name,
        json_pack("{s:s}", "id", parent_id),
        0,
        src
    );
    if(!parent_node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Parent not found"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *child_node = gobj_get_node(
        gobj,
        child_topic_name,
        json_pack("{s:s}", "id", child_id),
        0,
        src
    );
    if(!child_node) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Parent not found"),
            0,
            0,
            kw  // owned
        );
    }

    int result = gobj_unlink_nodes(
        gobj,
        hook_name,
        parent_topic_name,
        parent_node,
        child_topic_name,
        child_node,
        src
    );

    child_node = gobj_get_node(
        gobj,
        child_topic_name,
        json_pack("{s:s}", "id", child_id),
        json_incref(_jn_options),
        src
    );

    return msg_iev_build_response(gobj,
        result,
        result<0?json_sprintf("%s", gobj_log_last_message()):json_sprintf("Nodes unlinked!"),
        gobj_topic_desc(gobj, child_topic_name),
        child_node,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_treedbs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_incref(kw);
    json_t *treedbs = gobj_treedbs(gobj, kw, src);

    return msg_iev_build_response(gobj,
        treedbs?0:-1,
        0,
        0,
        treedbs,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_topics(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0);

    if(empty_string(treedb_name)) {
        treedb_name = priv->treedb_name;
    }
    json_t *topics = gobj_treedb_topics(gobj, treedb_name, json_incref(_jn_options), src);

    return msg_iev_build_response(gobj,
        topics?0:-1,
        topics?0:json_string(gobj_log_last_message()),
        0,
        topics,
        kw  // owned
    );
}

/***************************************************************************
 *  Return a hierarchical tree of the self-link topic
 *  If "webix" option is TRUE return webix style (dict-list),
 *      else list of dict's
 *  __path__ field in all records (id`id`... style)
 *  If root node is not specified then the first with no parent is used
 ***************************************************************************/
PRIVATE json_t *cmd_jtree(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    const char *node_id = kw_get_str(gobj, kw, "node_id", "", 0);
    const char *hook = kw_get_str(gobj, kw, "hook", "", 0);
    const char *rename_hook = kw_get_str(gobj, kw, "rename_hook", "", 0);
    json_t *_jn_filter = kw_get_dict(gobj, kw, "filter", 0, 0);
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0);

    if(empty_string(topic_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What topic_name?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(hook)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What hook?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(node_id)) {
        do {
            // Search the first "fkey" field
            json_t *links = gobj_topic_links(
                gobj,
                priv->treedb_name,
                topic_name,
                0,
                src
            );
            if(json_array_size(links)==0) {
                json_decref(links);
                break;
            }

            const char *link = json_string_value(json_array_get(links, 0));
            // Search the first "root" node (without parents);
            json_t *nodes = gobj_list_nodes(
                gobj,
                topic_name,
                0, // filter
                0, // options
                src
            );
            int idx; json_t *node;
            json_array_foreach(nodes, idx, node) {
                json_t *jn_hook = kw_get_list(gobj, node, link, 0, KW_REQUIRED);
                if(json_array_size(jn_hook)==0) {
                    node_id = kw_get_str(gobj, node, "id", "", KW_REQUIRED);
                    break;
                }
            }
            json_decref(nodes);
            json_decref(links);
        } while(0);

        if(empty_string(node_id)) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("What node_id?"),
                0,
                0,
                kw  // owned
            );
        }
    }

    json_t *jtree = gobj_topic_jtree(
        gobj,
        topic_name,
        hook,                       // hook to build the hierarchical tree
        rename_hook,                // change the hook name in the tree response
        json_pack("{s:s}", "id", node_id),
        json_incref(_jn_filter),    // filter to match records
        json_incref(_jn_options),   // "webix", fkey,hook options
        src
    );

    return msg_iev_build_response(gobj,
        jtree?0:-1,
        jtree?0:json_string(gobj_log_last_message()),
        0,
        jtree,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_desc(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    if(strcmp(cmd, "desc")==0) {
        if(empty_string(topic_name)) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("What topic_name?"),
                0,
                0,
                kw  // owned
            );
        }
    }

    json_t *desc = gobj_topic_desc(gobj, topic_name); // Empty topic_name -> return all

    return msg_iev_build_response(gobj,
        desc?0:-1,
        desc?0:json_string(gobj_log_last_message()),
        0,
        desc,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_system_topic_schema(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    /*
     *  Inform
     */
    return msg_iev_build_response(gobj,
        0,
        0,
        0,
        _treedb_create_topic_cols_desc(),
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_trace(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    BOOL set = kw_get_bool(gobj, kw, "set", 0, KW_WILD_NUMBER);

    if(set) {
        treedb_set_trace(TRUE);
    } else {
        treedb_set_trace(FALSE);
    }
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Set trace %s", set?"on":"FALSE"),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_links(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);

    json_incref(kw);
    json_t *links = gobj_topic_links(gobj, priv->treedb_name, topic_name, kw, src);

    return msg_iev_build_response(gobj,
        links?0:-1,
        links?0:json_string(gobj_log_last_message()),
        0,
        links,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_hooks(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);

    json_incref(kw);
    json_t *hooks = gobj_topic_hooks(gobj, priv->treedb_name, topic_name, kw, src);

    return msg_iev_build_response(gobj,
        hooks?0:-1,
        hooks?0:json_string(gobj_log_last_message()),
        0,
        hooks,
        kw  // owned
    );

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_parents(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    const char *node_id = kw_get_str(gobj, kw, "node_id", "", 0);
    const char *link = kw_get_str(gobj, kw, "link", "", 0);
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0);

    if(empty_string(topic_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What topic_name?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(node_id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What node id?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *parents = gobj_node_parents( // Return MUST be decref
        gobj,
        topic_name,
        json_pack("{s:s}", "id", node_id),
        link,
        json_incref(_jn_options),
        src
    );

    return msg_iev_build_response(
        gobj,
        parents?0:-1,
        parents?0:json_string(gobj_log_last_message()),
        0,
        parents,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_children(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    const char *node_id = kw_get_str(gobj, kw, "node_id", "", 0);
    const char *hook = kw_get_str(gobj, kw, "hook", "", 0);
    json_t *_jn_filter = kw_get_dict(gobj, kw, "filter", 0, 0);
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0);

    if(empty_string(topic_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What topic_name?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(hook)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What hook?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(node_id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What node id?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *children = gobj_node_children( // Return MUST be decref
        gobj,
        topic_name,
        json_pack("{s:s}", "id", node_id),
        hook,
        json_incref(_jn_filter),
        json_incref(_jn_options),
        src
    );

    return msg_iev_build_response(
        gobj,
        children?0:-1,
        children?0:json_string(gobj_log_last_message()),
        0,
        children,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_nodes(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    json_t *_jn_filter = kw_get_dict(gobj, kw, "filter", 0, 0);
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0);

    if(empty_string(topic_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What topic_name?"),
            0,
            0,
            kw  // owned
        );
    }

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    json_t *nodes = gobj_list_nodes(
        gobj,
        topic_name,
        json_incref(_jn_filter),
        json_incref(_jn_options),
        src
    );

    return msg_iev_build_response(
        gobj,
        nodes?0:-1,
        nodes?
            json_sprintf("%d nodes", (int)json_array_size(nodes)):
            json_string(gobj_log_last_message()),
        nodes?tranger2_list_topic_desc_cols(priv->tranger, topic_name):0,
        nodes,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_get_node(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    const char *id = kw_get_str(gobj, kw, "node_id", "", 0);
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0);

    if(empty_string(topic_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What topic_name?"),
            0,
            0,
            kw  // owned
        );
    }
    if(empty_string(id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What node id?"),
            0,
            0,
            kw  // owned
        );
    }
    json_object_set_new(kw, "id", json_string(id)); // HACK remove 'id' field of cli

    json_t *node = gobj_get_node(
        gobj,
        topic_name,
        json_incref(kw),
        json_incref(_jn_options),
        src
    );

    return msg_iev_build_response(gobj,
        node?0:-1,
        node?0:json_sprintf("Node not found"),
        node?tranger2_list_topic_desc_cols(priv->tranger, topic_name):0,
        node,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_node_instances(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    const char *node_id = kw_get_str(gobj, kw, "node_id", "", 0);
    const char *pkey2 = kw_get_str(gobj, kw, "pkey2", "", 0);
    json_t *jn_filter = json_incref(kw_get_dict(gobj, kw, "filter", 0, 0));
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0);

    if(empty_string(topic_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What topic_name?"),
            0,
            0,
            kw  // owned
        );
    }
    if(!empty_string(node_id)) {
        if(!jn_filter) {
            jn_filter = json_pack("{s:s}", "id", node_id);
        } else {
            json_object_set_new(jn_filter, "id", json_string(node_id));
        }
    }

    json_t *instances = gobj_list_instances(
        gobj,
        topic_name,
        pkey2,
        jn_filter,
        json_incref(_jn_options),
        src
    );

    return msg_iev_build_response(
        gobj,
        0,
        instances?
            json_sprintf("%d instances", (int)json_array_size(instances)):
            json_string(gobj_log_last_message()),
        instances?tranger2_list_topic_desc_cols(priv->tranger, topic_name):0,
        instances,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_node_pkey2s(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    if(empty_string(topic_name)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What topic_name?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *pkey2s = treedb_topic_pkey2s(
        priv->tranger,
        topic_name
    );

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%d pkey2s", (int)json_array_size(pkey2s)),
        0,
        pkey2s,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_snap_content(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    return msg_iev_build_response(
        gobj,
        -1,
        json_sprintf("TODO"),
        0,
        0,
        kw  // owned
    );
//    PRIVATE_DATA *priv = gobj_priv_data(gobj);
//
//    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
//    if(empty_string(topic_name)) {
//        return msg_iev_build_response(
//            gobj,
//            -1,
//            json_sprintf("What topic_name?"),
//            0,
//            0,
//            kw  // owned
//        );
//    }
//
//    int snap_id = kw_get_int(gobj, kw, "snap_id", 0, KW_WILD_NUMBER);
//    if(!snap_id) {
//        return msg_iev_build_response(
//            gobj,
//            -1,
//            json_sprintf("What snap_id?"),
//            0,
//            0,
//            kw  // owned
//        );
//    }
//
//    json_t *topic = tranger2_topic(priv->tranger, topic_name);
//    if(!topic) {
//        return msg_iev_build_response(
//            gobj,
//            -1,
//            json_sprintf("Topic not found: '%s'", topic_name),
//            0,
//            0,
//            kw  // owned
//        );
//    }
//
//    json_t *jn_data = json_array();
//
//    json_t *jn_filter = json_pack("{s:b, s:i}",
//        "backward", 1,
//        "user_flag", snap_id
//    );
//
//
//    json_t *list = tranger2_open_iterator( TODO no será open_list ?
//        topic_name,
//        "key", // TODO
//        jn_filter,  //match_cond,  // owned
//        NULL,       //load_record_callback, // called on LOADING and APPENDING
//        "",         // iterator_id,     // iterator id, optional, if empty will be the key
//        NULL, // creator TODO
//        jn_data,    // JSON array, if not empty, fills it with the LOADING data, not owned
//        NULL
//    );
//    tranger2_close_iterator(priv->tranger, list);
//
//    return msg_iev_build_response(
//        gobj,
//        0,
//        0,
//        tranger2_list_topic_desc_cols(priv->tranger, topic_name),
//        jn_data,
//        kw  // owned
//    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_snaps(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_data = gobj_list_snaps(
        gobj,
        kw_incref(kw),
        src
    );

    return msg_iev_build_response(gobj,
        0,
        0,
        tranger2_list_topic_desc_cols(priv->tranger, "__snaps__"),
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_shoot_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *name = kw_get_str(gobj, kw, "name", 0, 0);
    if(empty_string(name)) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "What snap name?"
            ),
            0,
            0,
            kw  // owned
        );
    }
    int ret = gobj_shoot_snap(
        gobj,
        name,
        kw_incref(kw),
        src
    );
    json_t *jn_data = 0;
    if(ret == 0) {
        jn_data = gobj_list_snaps(
            gobj,
            json_pack("{s:s}", "name", name),
            src
        );
    }

    return msg_iev_build_response(gobj,
        ret,
        ret==0?json_sprintf("Snap '%s' shot", name):json_string(gobj_log_last_message()),
        ret==0?tranger2_list_topic_desc_cols(priv->tranger, "__snaps__"):0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_activate_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *name = kw_get_str(gobj, kw, "name", 0, 0);
    if(empty_string(name)) {
        return msg_iev_build_response(gobj,
            -1,
            json_sprintf(
                "What snap name?"
            ),
            0,
            0,
            kw  // owned
        );
    }
    int ret = gobj_activate_snap(
        gobj,
        name,
        kw_incref(kw),
        src
    );
    return msg_iev_build_response(gobj,
        ret,
        ret>=0?json_sprintf("Snap activated: '%s'", name):json_string(gobj_log_last_message()),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_deactivate_snap(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    int ret = gobj_activate_snap(
        gobj,
        "__clear__",
        kw_incref(kw),
        src
    );
    return msg_iev_build_response(gobj,
        ret,
        ret==0?json_sprintf("Snap deactivated"):json_string(gobj_log_last_message()),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_export_db(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *filename = kw_get_str(gobj, kw, "filename", "", 0);
    BOOL overwrite = kw_get_bool(gobj, kw, "overwrite", 0, KW_WILD_NUMBER);
    BOOL with_metadata = kw_get_bool(gobj, kw, "with_metadata", 0, KW_WILD_NUMBER);
    BOOL without_rowid = kw_get_bool(gobj, kw, "without_rowid", 0, KW_WILD_NUMBER);

    char path[PATH_MAX];
    char name[NAME_MAX];
    char date[100];

    if(empty_string(filename)) {
        current_timestamp(date, sizeof(date));
        time_t t;
        struct tm *tm;
        time(&t);
        tm = localtime(&t);
        strftime(date, sizeof(date), "%Y-%m-%d", tm);
        snprintf(name, sizeof(name), "%s-%s-%s.trdb.json",
            priv->treedb_name,
            kw_get_str(gobj, priv->treedb_schema, "schema_version", "", KW_REQUIRED),
            date
        );
    } else {
        snprintf(name, sizeof(name), "%s.trdb.json", filename);
    }

    yuneta_realm_file(path, sizeof(path), "temp", "", TRUE);

    json_t *jn_data = json_pack("{s:s, s:s}",
        "path", path,
        "filename", name
    );

    yuneta_realm_file(path, sizeof(path), "temp", name, TRUE);

    if(access(path, 0)==0) {
        if(!overwrite) {
            json_decref(jn_data);
            return msg_iev_build_response(gobj,
                -1,
                json_sprintf("File '%s' already exists. Use overwrite option", name),
                0,
                0,
                kw  // owned
            );
        }
    }

    int ret = export_treedb(gobj, path, with_metadata, without_rowid, src);

    /*
     *  Inform
     */
    return msg_iev_build_response(gobj,
        ret,
        json_sprintf("Treedb exported %s", ret==0?"ok":"failed"),
        0,
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_import_db(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *content64 = kw_get_str(gobj, kw, "content64", "", 0);

    int if_resource_exists = 0; // abort by default
    const char *if_resource_exists_ = kw_get_str(gobj, kw, "if-resource-exists", "", 0);

    if(strcasecmp(if_resource_exists_, "skip")==0) {
        if_resource_exists = 1;
    } else if(strcasecmp(if_resource_exists_, "overwrite")==0) {
        if_resource_exists = 2;
    }

    /*------------------------------------------------*
     *  Firstly get content in base64 and decode
     *------------------------------------------------*/
    json_t *jn_db;
    if(empty_string(content64)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What content64?"),
            0,
            0,
            kw  // owned
        );
    }

    gbuffer_t *gbuf_content = gbuffer_base64_to_string(content64, strlen(content64));
    jn_db = gbuf2json(
        gbuf_content,  // owned
        2
    );
    if(!jn_db) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Bad json in content64"),
            0,
            0,
            kw  // owned
        );
    }

    /*-----------------------------*
     *  Load records
     *  If a resource exists
     *      0 abort (default)
     *      1 skip
     *      2 overwrite
     *-----------------------------*/
    int ret = 0;
    int new = 0;
    int overwrite = 0;
    int ignored = 0;
    int failure = 0;
    int abort = 0;
    int link_failure = 0;

    json_t *jn_loaded = json_object();
    json_t *jn_errores = json_object();

    // TODO Chequear permisos

    BOOL fin = FALSE;
    const char *topic_name;
    json_t *topic_records;
    json_object_foreach(jn_db, topic_name, topic_records) {
        if(fin) {
            break;
        }
        json_t *list_records = kw_get_dict_value(
            gobj,
            jn_loaded,
            topic_name,
            json_array(),
            KW_CREATE
        );

        int idx; json_t *record;
        json_array_foreach(topic_records, idx, record) {
            if(fin) {
                break;
            }
            json_t *node = gobj_create_node( // Return is NOT YOURS
                gobj,
                topic_name,
                json_incref(record),
                0,
                src
            );
            if(node) {
                json_decref(node);
                json_array_append(
                    list_records,
                    record
                );
                new++;
            } else {
                int n = kw_get_int(gobj, jn_errores, gobj_log_last_message(), 0, KW_CREATE);
                n++;
                json_object_set_new(jn_errores, gobj_log_last_message(), json_integer(n));

                if(if_resource_exists==1) {
                    // Skip
                    ignored++;
                } else if(if_resource_exists==2) {
                    // Overwrite
                    node = gobj_get_node(
                        gobj,
                        topic_name,
                        json_incref(record),
                        0,
                        src
                    );
                    if(node) {
                        json_decref(node);
                        json_array_append(
                            list_records,
                            record
                        );
                        overwrite++;
                    } else {
                        int n = (int)kw_get_int(gobj, jn_errores, gobj_log_last_message(), 0, KW_CREATE);
                        n++;
                        json_object_set_new(jn_errores, gobj_log_last_message(), json_integer(n));

                        failure++;
                    }
                } else {
                    // abort
                    abort++;
                    fin = TRUE;
                    gobj_trace_json(gobj, record, "%s", gobj_log_last_message());
                    break;
                }
            }
        }
    }

    /*
     *  Create links
     */
    json_object_foreach(jn_loaded, topic_name, topic_records) {
        int idx; json_t *record;
        json_array_foreach(topic_records, idx, record) {
            json_t *node = gobj_update_node(
                gobj,
                topic_name,
                json_incref(record),
                json_pack("{s:b}", "autolink", 1),
                src
            );
            if(node) {
                //gobj_trace_json(gobj, node, "node added");
                json_decref(node);
            } else {
                link_failure++;
                gobj_trace_json(gobj, record, "link_failure");
            }
        }
    }

    json_decref(jn_db);
    json_decref(jn_loaded);

    /*
     *  Inform
     */
    json_t *jn_data = json_pack("{s:i, s:i, s:i, s:i, s:i, s:i, s:o}",
        "abort", abort,
        "added", new,
        "overwrite", overwrite,
        "ignored", ignored,
        "failure", failure,
        "link failure", link_failure,
        "errores", jn_errores
    );

    return msg_iev_build_response(gobj,
        ret,
        0,
        0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
// PRIVATE json_t *cmd_tree(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
// {
//     do {
//         if(!fichajes_roles ||
//                 !(json_str_in_list(fichajes_roles, "admin", FALSE) ||
//                 json_str_in_list(fichajes_roles, "gestor", FALSE))) {
//             jn_comment = json_string("User has not 'admin' or 'gestor' role");
//             result = -1;
//             break;
//         }
//
//         /*
//          *  Get lista departments
//          */
//         const char *department = kw_get_str(gobj, kw, "department", 0, 0);
//         if(!department) {
//             department = "direccion";
//         }
//         json_t *node = gobj_get_node( // Return is NOT YOURS
//             priv->treedb_gest,
//             "gest_departments",
//             json_pack("{s:s}", "id", department),
//             0,
//             src
//         );
//         if(node) {
//             json_t *filter = json_pack("{s:s, s:s, s:s}", // Fields to include
//                 "name", "value", // HACK rename field name->value
//                 "users", "",
//                 "managers", ""
//             );
//             jn_data = webix_new_list_tree(node, "departments", "data", filter, "");
//         } else {
//             result = -1;
//             jn_comment = json_string(gobj_log_last_message());
//         }
//     } while(0);
/*
}*/




            /***************************
             *      Local Methods
             ***************************/




            /***************************
             *      Private Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int treedb_callback(
    void *user_data,
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *operation,  // EV_TREEDB_NODE_UPDATED,
                            // EV_TREEDB_NODE_UPDATED,
                            // EV_TREEDB_NODE_DELETED
    json_t *node            // owned
)
{
    json_t *collapse_node = node_collapsed_view( // Return MUST be decref
        tranger,
        node, // not owned
        json_pack("{s:b}",
            "list_dict", 1  // HACK always list_dict
        )
    );

    json_t *kw = json_pack("{s:s, s:s, s:o}",
        "treedb_name", treedb_name,
        "topic_name", topic_name,
        "node", collapse_node
    );
    json_decref(node);

    gobj_publish_event(user_data, operation, kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *fetch_node(
    hgobj gobj,
    const char *topic_name,
    json_t *kw  // NOT owned, 'id' and pkey2s fields are used to find the node
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *id = kw_get_str(gobj, kw, "id", 0, 0);
    if(!id) {
        // Silence
        return 0;
    }

    json_t *jn_filter = treedb_topic_pkey2s_filter(
        priv->tranger,
        topic_name,
        kw, // NO owned
        id
    );

    /*
     *  Busca en indice principal
     */
    json_t *node = treedb_get_node(
        priv->tranger,
        priv->treedb_name,
        topic_name,
        id
    );
    if(!node) {
        // Error already logged
        JSON_DECREF(jn_filter);
        return 0;
    }
    if(node && kw_match_simple(node, json_incref(jn_filter))) {
        JSON_DECREF(jn_filter);
        return node;
    }

    json_t *iter_pkey2s = treedb_topic_pkey2s(priv->tranger, topic_name);
    int idx; json_t *jn_pkey2_name;
    json_array_foreach(iter_pkey2s, idx, jn_pkey2_name) {
        const char *pkey2_name = json_string_value(jn_pkey2_name);
        if(empty_string(pkey2_name)) {
            continue;
        }

        const char *pkey2_value = kw_get_str(gobj, kw, pkey2_name, "", 0);

        node = treedb_get_instance( // WARNING Return is NOT YOURS, pure node
            priv->tranger,
            priv->treedb_name,
            topic_name,
            pkey2_name, // required
            id,         // primary key
            pkey2_value // secondary key
        );

        if(node && kw_match_simple(node, json_incref(jn_filter))) {
            break;
        } else {
            node = 0;
        }
    }
    json_decref(iter_pkey2s);

    JSON_DECREF(jn_filter);
    return node;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int export_treedb(
    hgobj gobj,
    const char *path,
    BOOL with_metadata,
    BOOL without_rowid,
    hgobj src
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    FILE *file = fopen(path, "w");
    if(!file) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot create file",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }

    json_t *jn_db = json_object();

    json_t *topics_list = gobj_treedb_topics(
        gobj,
        priv->treedb_name,
        0,
        src
    );
    int idx; json_t *jn_topic_name;
    json_array_foreach(topics_list, idx, jn_topic_name) {
        const char *topic_name = json_string_value(jn_topic_name);
        if(strcmp(topic_name, "__snaps__")==0) {
            continue;
        }
        json_t *nodes = gobj_list_nodes( // Return MUST be decref
            gobj,
            topic_name,
            0,
            json_pack("{s:b, s:b}",
                "with_metadata", with_metadata,
                "without_rowid", without_rowid
            ),
            src
        );
        json_object_set_new(jn_db, topic_name, nodes);
    }

    json_decref(topics_list);

    json_dump_file(jn_db, path, JSON_INDENT(4));
    json_decref(jn_db);

    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  HACK bypass authz control, only internal use
 ***************************************************************************/
PRIVATE int ac_treedb_update_node(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    /*
     *  Get parameters
     */
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    json_t *record = kw_get_dict(gobj, kw, "record", 0, 0);
    json_t *_jn_options = kw_get_dict(gobj, kw, "options", 0, 0); // "create", "auto-link"

    json_t *node = gobj_update_node( // Return is YOURS
        gobj,
        topic_name,
        json_incref(record),
        json_incref(_jn_options),
        src
    );
    json_decref(node); // return something? de momento no, uso interno.

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
    .mt_create=        mt_create,
    .mt_destroy=       mt_destroy,
    .mt_start=         mt_start,
    .mt_stop=          mt_stop,
    .mt_writing=       mt_writing,
    .mt_trace_on=      mt_trace_on,
    .mt_trace_off=     mt_trace_off,
    .mt_create_node=   mt_create_node,
    .mt_update_node=   mt_update_node,
    .mt_delete_node=   mt_delete_node,
    .mt_link_nodes=    mt_link_nodes,
    .mt_unlink_nodes=  mt_unlink_nodes,
    .mt_topic_jtree=   mt_topic_jtree,
    .mt_get_node=      mt_get_node,
    .mt_list_nodes=    mt_list_nodes,
    .mt_shoot_snap=    mt_shoot_snap,
    .mt_activate_snap= mt_activate_snap,
    .mt_list_snaps=    mt_list_snaps,
    .mt_treedbs=       mt_treedbs,
    .mt_treedb_topics= mt_treedb_topics,
    .mt_topic_desc=    mt_topic_desc,
    .mt_topic_links=   mt_topic_links,
    .mt_topic_hooks=   mt_topic_hooks,
    .mt_node_parents=  mt_node_parents,
    .mt_node_children=   mt_node_children,
    .mt_list_instances=mt_list_instances,
    .mt_node_tree=     mt_node_tree,
    .mt_topic_size=    mt_topic_size,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_NODE);

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
        {EV_TREEDB_UPDATE_NODE,   ac_treedb_update_node,      0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TREEDB_UPDATE_NODE,     EVF_PUBLIC_EVENT},
        {EV_TREEDB_NODE_CREATED,    EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_UPDATED,    EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_DELETED,    EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
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
PUBLIC int register_c_node(void)
{
    return create_gclass(C_NODE);
}
