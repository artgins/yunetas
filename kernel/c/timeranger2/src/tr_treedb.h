/****************************************************************************
 *          TR_TREEDB.C
 *
 *          Tree Database over TimeRanger
 *          Hierarchical tree of objects (nodes, records, messages)
 *          linked by child-parent relation.
 *
 *          Copyright (c) 2019 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "timeranger2.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_TREEDB_UPDATE_NODE);
GOBJ_DECLARE_EVENT(EV_TREEDB_NODE_CREATED);
GOBJ_DECLARE_EVENT(EV_TREEDB_NODE_UPDATED);
GOBJ_DECLARE_EVENT(EV_TREEDB_NODE_DELETED);

/***************************************************************
 *              Prototypes
 ***************************************************************/
/**rst**

    Rules:
        - In links only fkeys are saved in tranger (child node info).
        - Keys (id's) can't contain ` nor ^ characters.
        - A fkey field only can be fkey once, i.e, only can have one parent (???)

**rst**/

/**rst**
    Open tree db (Remember previously open tranger_startup())

    Tree of nodes of data. Node's data contains attributes, in json.

    The nodes are linked with relation parent->child
    in pure-child or multi-parent mode.
    The tree in json too.

    HACK Conventions:
        1) the pkey of all topics must be "id".
        2) the "id" field (primary key) MUST be a string.

    Option "persistent"
        Try to load the schema from file
        File has precedence.
        Once saved,
            if you want to change the schema
            then you must change the schema version and topic_version

    Return a dict inside of tranger with path "treedbs`{treedb_name}" DO NOT use it directly

    System topics:
        __snaps__
        __graphs__

**rst**/
PUBLIC json_t *treedb_open_db( // WARNING Return IS NOT YOURS!
    json_t *tranger,
    const char *treedb_name,
    json_t *jn_schema,  // owned
    const char *options // "persistent"
);
PUBLIC int treedb_close_db(
    json_t *tranger,
    const char *treedb_name
);

typedef int (*treedb_callback_t)(
    void *user_data,
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *operation,  // "EV_TREEDB_NODE_UPDATED",
                            // "EV_TREEDB_NODE_UPDATED",
                            // "EV_TREEDB_NODE_DELETED"
    json_t *node            // owned
);

PUBLIC int treedb_set_callback(
    json_t *tranger,
    const char *treedb_name,
    treedb_callback_t treedb_callback,
    void *user_data
);

/***************************************************************************
    Return is NOT YOURS, pkey MUST be "id"
    WARNING This function don't load hook links.
    HACK IDEMPOTENT function

    Schema (jn_cols)
    ================

    Dictionary of
        $key: {
            "header":       string
            "fillspace":    integer
            "type":         enum
            "flag":         enum
            "default":      blob
            "placeholder":  string
        }

    "type" enum
    -----------

        // Real types

        "string"
        "integer"
        "object" or "dict"
        "array" or "list"
        "real"
        "boolean"
        "blob"
        "number" ("integer" or "real")

    "flag" enum
    -----------

        // Field attributes

        "persistent"    // implicit "readable"
        "required"
        "notnull"
        "wild"
        "inherit"
                        // For use in gobj attributes
        "readable"      // Field readable by user
        "writable"      // Field writable by user implicit "readable"
        "stats"         // field with stats implicit "readable"
        "rstats"        // field with resettable stats implicit "stats"
        "pstats"        // field with persistent stats implicit "stats"

        // Field types

        "fkey"
        "hook"
        "enum"
        "uuid"
        "rowid"
        "password"
        "email"
        "url"
        "time"
        "now"
        "date"
        "color"
        "image"
        "tel"
        "template"
        "table"
        "id"
        "currency"
        "hex"
        "binary"
        "percent"
        "base64"

 ***************************************************************************/
PUBLIC json_t *treedb_create_topic( // WARNING Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    int topic_version,
    const char *topic_tkey,
    json_t *pkey2s, // owned, string or dict of string | [strings]
    json_t *jn_cols, // owned
    uint32_t snap_tag,
    BOOL create_schema
);

PUBLIC int treedb_close_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);

PUBLIC int treedb_delete_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);

PUBLIC json_t *treedb_list_treedb( // Return a list with treedb names
    json_t *tranger,
    json_t *kw
);

PUBLIC json_t *treedb_topics( //Return a list with topic names of the treedb
    json_t *tranger,
    const char *treedb_name,
    json_t *jn_options // "dict" return list of dicts, otherwise return list of strings
);

PUBLIC size_t treedb_topic_size(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);

/*------------------------------------*
 *      Utils
 *------------------------------------*/
PUBLIC json_t *_treedb_create_topic_cols_desc(void);

PUBLIC int parse_schema(
    json_t *schema  // not owned
);
PUBLIC int parse_schema_cols( // Return 0 if ok or # of errors in negative
    json_t *cols_desc,  // NOT owned
    json_t *data        // owned
);
PUBLIC int parse_hooks(
    json_t *schema  // not owned
);

PUBLIC int current_snap_tag(
    json_t *tranger,
    const char *treedb_name
);

PUBLIC BOOL treedb_is_treedbs_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);
PUBLIC json_t *treedb_get_id_index( // WARNING Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);
PUBLIC json_t *treedb_topic_pkey2s( // Return list with pkey2s
    json_t *tranger,
    const char *topic_name
);
PUBLIC json_t *treedb_topic_pkey2s_filter(
    json_t *tranger,
    const char *topic_name,
    json_t *node, // NO owned
    const char *id
);

PUBLIC int treedb_set_trace(BOOL set);

PUBLIC BOOL decode_parent_ref( // parent_topic_name^parent_id^hook_name
    const char *pref,
    char *topic_name, int topic_name_size,
    char *id, int id_size,
    char *hook_name, int hook_name_size
);

PUBLIC BOOL decode_child_ref( // child_topic_name^child_id
    const char *pref,
    char *topic_name, int topic_name_size,
    char *id, int id_size
);


/*------------------------------------*
 *      Manage the tree's nodes
 *------------------------------------*/

/**rst**
    Create a new node
**rst**/
PUBLIC json_t *treedb_create_node( // WARNING Return is NOT YOURS, pure node
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw // owned
);

/**rst**
    Direct saving to tranger.
    Tag __tag__ (user_flag) is inherited.
**rst**/
PUBLIC int treedb_save_node(
    json_t *tranger,
    json_t *node    // NOT owned, pure node.
);

/**rst**
    Update the existing current node with fields of kw
    HACK fkeys and hook fields are not updated!
**rst**/
PUBLIC json_t *treedb_update_node( // WARNING Return is NOT YOURS, pure node
    json_t *tranger,
    json_t *node,   // NOT owned, pure node.
    json_t *kw,     // owned
    BOOL save
);

/**rst**
    Set volatil values (not fkey, not hook, not persistent)
    (using default if not available)
**rst**/
PUBLIC int set_volatil_values(
    json_t *tranger,
    const char *topic_name,
    json_t *record,  // NOT owned
    json_t *kw // NOT owned
);

/**rst**
    "force" delete links. If there are links are not force then delete_node will fail
**rst**/
PUBLIC int treedb_delete_node(
    json_t *tranger,
    json_t *node,       // owned, pure node
    json_t *jn_options  // bool "force"
);

/**rst**
    "force" delete links. If there are links are not force then delete_node will fail
**rst**/
PUBLIC int treedb_delete_instance(
    json_t *tranger,
    json_t *node,       // owned, pure node
    const char *pkey2_name,
    json_t *jn_options  // bool "force"
);

PUBLIC int treedb_clean_node( // remove all links (fkeys)
    json_t *tranger,
    json_t *node,           // NOT owned, pure node
    BOOL save
);
PUBLIC int treedb_autolink( // use fkeys fields of kw to auto-link
    json_t *tranger,
    json_t *node,           // NOT owned, pure node
    json_t *kw, // owned
    BOOL save
);

PUBLIC int treedb_link_nodes(
    json_t *tranger,
    const char *hook,
    json_t *parent_node,    // NOT owned, pure node
    json_t *child_node      // NOT owned, pure node
);

PUBLIC int treedb_unlink_nodes(
    json_t *tranger,
    const char *hook,
    json_t *parent_node,    // NOT owned, pure node
    json_t *child_node      // NOT owned, pure node
);

/**rst**
    Meaning of parent and child 'references' (fkeys, hooks)
    -----------------------------------------------------
    'fkey ref'
        The parent's references (link to up) have 3 ^ fields:

            "topic_name^id^hook_name"

    'hook ref'
        The child's references (link to down) have 2 ^ fields:

            "topic_name^id"

    fkey options
    -------------
    "refs"
    "fkey_refs"
        Return 'fkey ref'
            ["topic_name^id^hook_name", ...]


    "only_id"
    "fkey_only_id"
        Return the 'fkey ref' with only the 'id' field
            ["$id",...]


    "list_dict" (default)
    "fkey_list_dict"
        Return the kwid style:
            [{"id": "$id", "topic_name":"$topic_name", "hook_name":"$hook_name"}, ...]


    hook options
    ------------
    "refs"
    "hook_refs"
        Return 'hook ref'
            ["topic_name^id", ...]

    "only_id"
    "hook_only_id"
        Return the 'hook ref' with only the 'id' field
            ["$id",...]

    "list_dict" (default)
    "hook_list_dict"
        Return the kwid style:
            [{"id": "$id", "topic_name":"$topic_name"}, ...]

    "size"
    "hook_size"
        Return the kwid style:
            [{"size": size}]


    Other options
    -------------

    "with_metadata"
        Return with metadata

    "without_rowid"
        Don't "id" when is "rowid", by default it's returned

    HACK id is converted in ids (using kwid_get_ids())
    HACK if __filter__ exists in jn_filter it will be used as filter

**rst**/

PUBLIC json_t *treedb_get_node( // WARNING Return is NOT YOURS, pure node
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *id  // using the primary key
);

PUBLIC json_t *treedb_get_instance( // WARNING Return is NOT YOURS, pure node
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *pkey2_name, // required
    const char *id,     // primary key
    const char *key2    // secondary key
);

PUBLIC json_t *node_collapsed_view( // Return MUST be decref
    json_t *tranger, // NOT owned
    json_t *node, // NOT owned
    json_t *jn_options // owned fkey,hook options, see Other options
);

PUBLIC json_t *treedb_list_nodes( // Return MUST be decref
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *topic_desc, // NOT owned
        json_t *node,       // NOT owned
        json_t *jn_filter   // NOT owned
    )
);
PUBLIC json_t *treedb_list_instances( // Return MUST be decref
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *pkey2_name,
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *topic_desc, // NOT owned
        json_t *node,       // NOT owned
        json_t *jn_filter   // NOT owned
    )
);

/*
 *  Return a list of parent **references** pointed by the link (fkey)
 */
PUBLIC json_t *treedb_parent_refs( // Return MUST be decref
    json_t *tranger,
    const char *fkey,
    json_t *node,       // NOT owned, pure node
    json_t *jn_options  // owned, fkey options
);

/*
 *  Return a list of parent collapsed view nodes pointed by the link (fkey)
 */
PUBLIC json_t *treedb_list_parents( // Return MUST be decref
    json_t *tranger,
    const char *fkey,   // must be a fkey field
    json_t *node,       // NOT owned, pure node
    BOOL collapsed_view, // TRUE return collapsed views
    json_t *jn_options // owned, fkey,hook options when collapsed_view is true
);

/*
 *  Return a list of childs through the hook
 */
PUBLIC json_t *treedb_node_childs(
    json_t *tranger,
    const char *hook,
    json_t *node,       // NOT owned, pure node
    json_t *jn_filter,  // filter to childs tree
    json_t *jn_options  // fkey,hook options, "recursive"
);

PUBLIC int add_jtree_path( // Add path to child
    json_t *parent,  // not owned
    json_t *child  // not owned
);

/*
 *  Return a tree of childs through the hook
 */
PUBLIC json_t *treedb_node_jtree(
    json_t *tranger,
    const char *hook,   // hook to build the hierarchical tree
    const char *rename_hook, // change the hook name in the tree response
    json_t *node,       // NOT owned, pure node
    json_t *jn_filter,  // filter to childs tree
    json_t *jn_options  // fkey,hook options
);

/*----------------------------*
 *          Schema
 *----------------------------*/
/*
 *  Return a list with the cols that are links (fkeys) of the topic
 */
PUBLIC json_t *treedb_get_topic_links(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);

/*
 *  Return a list with the cols that are hooks of the topic
 */
PUBLIC json_t *treedb_get_topic_hooks(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
);

/*----------------------------*
 *          Snaps
 *----------------------------*/
PUBLIC int treedb_shoot_snap( // tag the current tree db
    json_t *tranger,
    const char *treedb_name,
    const char *snap_name,
    const char *description
);
PUBLIC int treedb_activate_snap( // Activate tag, return the snap tag
    json_t *tranger,
    const char *treedb_name,
    const char *snap_name
);
PUBLIC json_t *treedb_list_snaps( // Return MUST be decref, list of snaps
    json_t *tranger,
    const char *treedb_name,
    json_t *filter
);

/*----------------------------*
 *          Template
 *----------------------------*/
PUBLIC json_t *create_template_record(
    const char *template_name,
    json_t *cols,       // NOT owned
    json_t *kw          // Owned
);

#ifdef __cplusplus
}
#endif
