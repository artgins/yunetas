/****************************************************************************
 *          gobj.h
 *
 *          Objects G for Yuneta Simplified
 *
 *          Objects G with this api:
 *              - List of events supported, they can be of output or input or both.
 *              - List of states. Each state can support different list of events.
 *              - List of attributes (json types).
 *              - List of commands.
 *
 *          Events and states are managed by a simple finite state machine.
 *
 *          Communication between gobjs happen through events.
 *
 *          The events are messages with key/value model,
 *          being the key a string and the value a json object.
 *
 *          The output events a gobj can be subscribed by others gobjs.
 *
 *          GObj's are  organized internally as a hierarchical tree forming a Yuno.
 *          They collaborate to reach to goal of the yuno.
 *
 *          This software is battery included with internal:
 *              - log system
 *              - buffer system
 *              - json system based in jansson library (https://github.com/akheron/jansson)
 *
 *          Copyright (c) 1996-2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 *
 *
 *  Version 7.0.0
 *  -------------
 *      Initial version is a refactoring of Yuneta for Linux,
 *      simplified to work with ESP32 microcontrollers
 *
 *
 ****************************************************************************/
#pragma once

#include "kwid.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *   Macros to assign attr data to priv data.
 ***************************************************************/
#define IF_EQ_SET_PRIV(__name__, __func__) \
    if(strcmp(path, #__name__)==0) {\
        priv->__name__ = __func__(gobj, #__name__);
#define ELIF_EQ_SET_PRIV(__name__, __func__) \
    } else if(strcmp(path, #__name__)==0) {\
        priv->__name__ = __func__(gobj, #__name__);
#define END_EQ_SET_PRIV() \
    }

#define SET_PRIV(__name__, __func__) \
    priv->__name__ = __func__(gobj, #__name__);

/***************************************************************
 *              SData - Structured Data
 ***************************************************************/
typedef enum {
    DTP_STRING = 1,
    DTP_BOOLEAN,
    DTP_INTEGER,
    DTP_REAL,
    DTP_LIST,
    DTP_DICT,
    DTP_JSON,
    DTP_POINTER
} data_type_t;


#define DTP_IS_STRING(type)     ((type) == DTP_STRING)
#define DTP_IS_BOOLEAN(type)    ((type) == DTP_BOOLEAN)
#define DTP_IS_INTEGER(type)    ((type) == DTP_INTEGER)
#define DTP_IS_REAL(type)       ((type) == DTP_REAL)
#define DTP_IS_LIST(type)       ((type) == DTP_LIST)
#define DTP_IS_DICT(type)       ((type) == DTP_DICT)
#define DTP_IS_JSON(type)       ((type) == DTP_JSON)
#define DTP_IS_POINTER(type)    ((type) == DTP_POINTER)
#define DTP_IS_SCHEMA(type)     ((type) == DTP_SCHEMA)
#define DTP_SCHEMA              DTP_POINTER

#define DTP_IS_NUMBER(type)     \
    (DTP_IS_INTEGER(type) ||    \
     DTP_IS_REAL(type) ||       \
     DTP_IS_BOOLEAN(type))

/*
 *  SData field flags.
 */
typedef enum {   // HACK strict ascendant value!, strings in sdata_flag_names[]
    SDF_NOTACCESS   = 0x00000001,   /* snmp/db exposed as not accessible */
    SDF_RD          = 0x00000002,   /* Field only readable by user */
    SDF_WR          = 0x00000004,   /* Field writable (an readable) by user */
    SDF_REQUIRED    = 0x00000008,   /* Required attribute. Must not be null */
    SDF_PERSIST     = 0x00000010,   /* (implicit SDF_WR) Field must be loaded/saved HACK 7-Feb-2020 */
    SDF_VOLATIL     = 0x00000020,   /* (implicit SDF_RD) Field must not be loaded/saved HACK 7-Feb-2020 */
    SDF_RESOURCE    = 0x00000040,   /* Mark as resource.  Use `schema` to specify the sdata schema */
    SDF_PKEY        = 0x00000080,   /* field used as primary key */
    SDF_FUTURE1     = 0x00000100,   /* old SDF_PURECHILD */
    SDF_FUTURE2     = 0x00000200,   /* old SDF_PARENTID */
    SDF_WILD_CMD    = 0x00000400,   /* Command with wild options (no checked) */
    SDF_STATS       = 0x00000800,   /* (implicit SDF_RD) Field with stats (METADATA)*/
    SDF_FKEY        = 0x00001000,   /* Foreign key (no pure child) */
    SDF_RSTATS      = 0x00002000,   /* Field with resettable stats, implicitly SDF_STATS */
    SDF_PSTATS      = 0x00004000,   /* Field with persistent stats, implicitly SDF_STATS */
    SDF_AUTHZ_R     = 0x00008000,   /* Need Attribute '__read_attribute__' authorization */
    SDF_AUTHZ_W     = 0x00010000,   /* Need Attribute '__write_attribute__' authorization */
    SDF_AUTHZ_X     = 0x00020000,   /* Need Command '__execute_command__' authorization */
    SDF_AUTHZ_P     = 0x00040000,   /* authorization constraint parameter */
    SDF_AUTHZ_S     = 0x00080000,   /* Need Stats '__read_stats__' authorization */
    SDF_AUTHZ_RS    = 0x00100000,   /* Need Stats '__reset_stats__' authorization */
} sdata_flag_t;

#define SDF_PUBLIC_ATTR (SDF_RD|SDF_WR|SDF_STATS|SDF_PERSIST|SDF_VOLATIL|SDF_RSTATS|SDF_PSTATS)
#define ATTR_WRITABLE   (SDF_WR|SDF_PERSIST)
#define ATTR_READABLE   SDF_PUBLIC_ATTR

/*
 *  Generic json function
 */
typedef json_t *(*json_function_fn)(
    void *param1,
    const char *something,
    json_t *kw, // Owned
    void *param2
);

typedef struct sdata_desc_s {
    const data_type_t type;
    const char *name;
    const char **alias;
    const sdata_flag_t flag;
    const char *default_value;
    const char *header;
    const int fillspace;
    const char *description;
    const json_function_fn json_fn;
    const struct sdata_desc_s *schema;
    const char *authpth;
} sdata_desc_t;


#define SDATA_END()                                     \
{                                                       \
    .type=0,                                            \
    .name=0,                                            \
    .alias=0,                                           \
    .flag=0,                                            \
    .default_value=0,                                   \
    .header=0,                                          \
    .fillspace=0,                                       \
    .description=0,                                     \
    .json_fn=0,                                         \
    .schema=0,                                          \
    .authpth=0                                          \
}

/*
 *  GObj Attribute
 */
/*-ATTR-type------------name----------------flag------------------------default---------description----------*/
#define SDATA(type_, name_, flag_, default_value_, description_) \
{                                                       \
    .type=(type_),                                      \
    .name=(name_),                                      \
    .alias=0,                                           \
    .flag=(flag_),                                      \
    .default_value=(default_value_),                    \
    .header=0,                                          \
    .fillspace=0,                                       \
    .description=(description_),                        \
    .json_fn=0,                                         \
    .schema=0,                                          \
    .authpth=0                                          \
}

/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
#define SDATACM(type_, name_, alias_, items_, json_fn_, description_) \
{                                                       \
    .type=(type_),                                      \
    .name=(name_),                                      \
    .alias=(alias_),                                    \
    .flag=0,                                            \
    .default_value=0,                                   \
    .header=0,                                          \
    .fillspace=0,                                       \
    .description=(description_),                        \
    .json_fn=(json_fn_),                                \
    .schema=(items_),                                   \
    .authpth=0                                          \
}

/*-CMD2--type-----------name----------------flag----------------alias---------------items-----------json_fn---------description---------- */
#define SDATACM2(type_, name_, flag_, alias_, items_, json_fn_, description_) \
{                                                       \
    .type=(type_),                                      \
    .name=(name_),                                      \
    .alias=(alias_),                                    \
    .flag=flag_,                                        \
    .default_value=0,                                   \
    .header=0,                                          \
    .fillspace=0,                                       \
    .description=(description_),                        \
    .json_fn=(json_fn_),                                \
    .schema=(items_),                                   \
    .authpth=0                                          \
}

/*-PM----type-----------name------------flag------------default-----description---------- */
#define SDATAPM(type_, name_, flag_, default_value_, description_) \
{                                                       \
    .type=(type_),                                      \
    .name=(name_),                                      \
    .alias=0,                                           \
    .flag=(flag_),                                      \
    .default_value=(default_value_),                    \
    .header=0,                                          \
    .fillspace=0,                                       \
    .description=(description_),                        \
    .json_fn=0,                                         \
    .schema=0,                                          \
    .authpth=0                                          \
}

/*-AUTHZ--type----------name------------flag----alias---items---------------description--*/
#define SDATAAUTHZ(type_, name_, flag_, alias_, items_, description_) \
{                                                       \
    .type=(type_),                                      \
    .name=(name_),                                      \
    .alias=(alias_),                                    \
    .flag=(flag_),                                      \
    .default_value=0,                                   \
    .header=0,                                          \
    .fillspace=0,                                       \
    .description=(description_),                        \
    .json_fn=0,                                         \
    .schema=(items_),                                   \
    .authpth=0                                          \
}

/*-PM-----type--------------name----------------flag--------authpath--------description-- */
#define SDATAPM0(type_, name_, flag_, authpth_, description_)     \
{                                                       \
    .type=(type_),                                      \
    .name=(name_),                                      \
    .alias=0,                                           \
    .flag=(flag_),                                      \
    .default_value=0,                                   \
    .header=0,                                          \
    .fillspace=0,                                       \
    .description=(description_),                        \
    .json_fn=0,                                         \
    .schema=0,                                          \
    .authpth=(authpth_)                                 \
}

/*
 *  Database Field
 */
/*-FIELD-type-----------name----------------flag------------------------resource--------header----------fillsp--description---------*/
#define SDATADF(type_, name_, flag_, header_, fillspace_, description_)  \
{                                                       \
    .type=(type_),                                      \
    .name=(name_),                                      \
    .alias=0,                                           \
    .flag=(flag_),                                      \
    .default_value=0,                                   \
    .header=(header_),                                  \
    .fillspace=(fillspace_),                            \
    .description=(description_),                        \
    .json_fn=0,                                         \
    .schema=0,                                          \
    .authpth=0                                          \
}

/***************************************************************
 *  Prototypes of functions to manage persistent attributes
 ***************************************************************/
typedef int (*startup_persistent_attrs_fn)(void);    // Initialize the database
typedef void (*end_persistent_attrs_fn)(void);       // End the database
typedef int (*load_persistent_attrs_fn)(             // Load the persistent attrs of gobj
    hgobj gobj,
    json_t *keys  // owned, if null load all persistent attrs, else, load
);
typedef int (*save_persistent_attrs_fn)(
    hgobj gobj,
    json_t *keys  // owned
);
typedef int (*remove_persistent_attrs_fn)(
    hgobj gobj,
    json_t *keys  // owned
);
typedef json_t * (*list_persistent_attrs_fn)(
    hgobj gobj,
    json_t *keys  // owned
);

/***************************************************************
 *              GClass/GObj Structures
 ***************************************************************/
typedef int (*gobj_action_fn)(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    hgobj src
);

typedef struct {
    gobj_event_t event;
    gobj_action_fn action;
    gobj_state_t next_state;
} ev_action_t;

typedef struct states_s {
    gobj_state_t state_name;
    ev_action_t *ev_action_list;
} states_t;

typedef enum { // HACK strict ascendant value!, strings in event_flag_names[]
    EVF_NO_WARN_SUBS    = 0x0001,   // Don't warn of "Publish event WITHOUT subscribers"
    EVF_OUTPUT_EVENT    = 0x0002,   // Output Event
    EVF_SYSTEM_EVENT    = 0x0004,   // System Event
    EVF_PUBLIC_EVENT    = 0x0008,   // You should document a public event, it's the API
    EVF_AUTHZ_INJECT    = 0x0010,   // Event needs '__inject_event__' authorization to be injected
    EVF_AUTHZ_SUBSCRIBE = 0x0020,   // Event needs '__subscribe_event__' authorization to be subscribed
} event_flag_t;

typedef struct event_type_s {
    gobj_event_t event_name;
    event_flag_t event_flag;
} event_type_t;

/*
 *  Cell functions
 */
typedef struct {
    char found;
    union {
        char *s;          // string
        BOOL b;           // bool
        json_int_t i;     // integer
        double f;         // float
        json_t *j;        // json_t
        void *p;          // pointer
    } v;
} SData_Value_t;

/*-------------------------------------------------------*
 *                  GClass Methods
 *-------------------------------------------------------*/
typedef int (*mt_start_fn)(hgobj gobj);
typedef int (*mt_stop_fn)(hgobj gobj);
typedef int (*mt_play_fn)(hgobj gobj);
typedef int (*mt_pause_fn)(hgobj gobj);
typedef void (*mt_create_fn)(hgobj gobj);
typedef void (*mt_create2_fn)(
    hgobj gobj,
    json_t *kw // owned
);
typedef void (*mt_destroy_fn)(hgobj gobj);
typedef void (*mt_writing_fn)(
    hgobj gobj,
    const char *name
);
typedef SData_Value_t (*mt_reading_fn)(
    hgobj gobj,
    const char *name
);
typedef int (*mt_subscription_added_fn)( // Negative return -> subscription deleted ???
    hgobj gobj,
    json_t * subs // NOT owned
);
typedef int (*mt_subscription_deleted_fn)(
    hgobj gobj,
    json_t * subs // NOT owned
);

/*
 *  These three functions return -1 (broke), 0 continue without publish, 1 continue and publish
 */
typedef BOOL  (*mt_publish_event_fn) ( // Return -1,0,1
    hgobj gobj,
    gobj_event_t event,
    json_t *kw  // NOT owned! you can modify this publishing kw
);
typedef BOOL  (*mt_publication_pre_filter_fn) ( // Return -1,0,1
    hgobj gobj,
    json_t *subs,
    gobj_event_t event,
    json_t *kw  // NOT owned! you can modify this publishing kw
);
typedef BOOL  (*mt_publication_filter_fn) ( // Return -1,0,1
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,  // NOT owned! you can modify this publishing kw
    hgobj subscriber
);
typedef int (*mt_child_added_fn)(hgobj gobj, hgobj child);
typedef int (*mt_child_removed_fn)(hgobj gobj, hgobj child);

typedef json_t *(*mt_create_resource_fn)(
    hgobj gobj,
    const char *resource,
    json_t *kw,
    json_t *jn_options
);
typedef void *(*mt_list_resource_fn)(
    hgobj gobj,
    const char *resource,
    json_t *jn_filter,
    json_t *jn_options
);
typedef int (*mt_save_resource_fn)(
    hgobj gobj,
    const char *resource,
    json_t *record,
    json_t *jn_options
);
typedef int (*mt_delete_resource_fn)(
    hgobj gobj,
    const char *resource,
    json_t *record,
    json_t *jn_options
);
typedef json_t *(*mt_get_resource_fn)(
    hgobj gobj,
    const char *resource,
    json_t *jn_filter,
    json_t *jn_options
);

typedef json_t *(*mt_create_node_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
typedef json_t *(*mt_update_node_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
typedef int (*mt_delete_node_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);
typedef int (*mt_link_nodes_fn)(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent,
    const char *child_topic_name,
    json_t *child,
    hgobj src
);
typedef int (*mt_unlink_nodes_fn)(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent,
    const char *child_topic_name,
    json_t *child,
    hgobj src
);
typedef json_t *(*mt_get_node_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *options,
    hgobj src
);

typedef json_t *(*mt_topic_jtree_fn)(
    hgobj gobj,
    const char *topic_name,
    const char *hook,
    const char *rename_hook,
    json_t *kw,
    json_t *jn_filter,
    json_t *jn_options,
    hgobj src
);

typedef json_t *(*mt_node_tree_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options,
    hgobj src
);

typedef json_t *(*mt_list_nodes_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *jn_filter,
    json_t *options, hgobj src
);
typedef int (*mt_shoot_snap_fn)(
    hgobj gobj,
    const char *tag,
    json_t *kw,
    hgobj src
);
typedef int (*mt_activate_snap_fn)(
    hgobj gobj,
    const char *tag,
    json_t *kw,
    hgobj src
);
typedef json_t *(*mt_list_snaps_fn)(
    hgobj gobj,
    json_t *filter,
    hgobj src
);

typedef json_t *(*mt_treedbs_fn)(
    hgobj gobj,
    json_t *kw,
    hgobj src
);
typedef json_t *(*mt_treedb_topics_fn)(
    hgobj gobj,
    const char *treedb_name,
    json_t *options,
    hgobj src
);
typedef json_t *(*mt_topic_desc_fn)(
    hgobj gobj,
    const char *topic_name
);
typedef json_t *(*mt_topic_links_fn)(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
    hgobj src
);
typedef json_t *(*mt_topic_hooks_fn)(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw, hgobj src
);
typedef json_t *(*mt_node_parents_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    const char *link,
    json_t *options,
    hgobj src
);
typedef json_t *(*mt_node_children_fn)(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    const char *hook,
    json_t *filter,
    json_t *options,
    hgobj src
);
typedef json_t *(*mt_list_instances_fn)(
    hgobj gobj,
    const char *topic_name,
    const char *pkey2_field,
    json_t *jn_filter,
    json_t *jn_options,
    hgobj src
);
typedef size_t (*mt_topic_size_fn)(
    hgobj gobj,
    const char *topic_name,
    const char *key
);

typedef json_t *(*local_method_fn)(
    hgobj gobj,
    gobj_lmethod_t lmethod,
    json_t *kw,
    hgobj src
);
typedef json_t *(*mt_authenticate_fn)(
    hgobj gobj,
    json_t *kw,
    hgobj src
);
typedef json_t *(*mt_list_children_fn)(
    hgobj gobj,
    const char *child_gclass,
    const char **attributes
);
typedef int (*mt_stats_updated_fn)(
    hgobj gobj,
    hgobj stats_gobj,
    const char *attr_name,
    int type, // my ASN1 types
    json_t * old_v,
    json_t * new_v
);
typedef int (*mt_disable_fn)(hgobj gobj);
typedef int (*mt_enable_fn)(hgobj gobj);
typedef int (*mt_trace_on_fn)(
    hgobj gobj,
    const char *level,
    json_t *kw
);
typedef int (*mt_trace_off_fn)(
    hgobj gobj,
    const char *level,
    json_t *kw
);

typedef void (*mt_gobj_created_fn)(
    hgobj gobj,
    hgobj gobj_created
);

typedef void (*mt_set_bottom_gobj_fn)(
    hgobj gobj,
    hgobj new_bottom_gobj,
    hgobj prev_bottom_gobj
);

typedef int (*mt_state_changed_fn)(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw
);

typedef BOOL (*authorization_checker_fn)(
    hgobj gobj,
    const char *authz,
    json_t *kw,
    hgobj src
);
typedef json_t *(*authentication_parser_fn)(
    hgobj gobj,
    json_t *kw,
    hgobj src
);

typedef int (*future_method_fn)(
    hgobj gobj,
    void *data,
    int c,
    char x
);

/*
 *  mt_command:
 *  First find command in gclass.cmds and execute it if found.
 *  If it doesn't exist then execute mt_command if it exists.
 *  Must return a json dict with "result", and "data" keys.
 *  Use the build_webix() helper to build the json.
 *  On error "result" will be a negative value, and "data" a string with the error description
 *  On successful "data" must return an any type of json data.
 *
 *  mt_stats or mt_command return 0 if there is a asynchronous response.
 *  For examples these will occur when the command is redirect to a event.
 */
typedef struct { // GClass methods (Yuneta framework methods)
    mt_create_fn mt_create;
    mt_create2_fn mt_create2;
    mt_destroy_fn mt_destroy;
    mt_start_fn mt_start;   // inside mt_start the gobj is already running
    mt_stop_fn mt_stop;    // inside mt_stop the gobj is already stopped
    mt_play_fn mt_play;
    mt_pause_fn mt_pause;
    mt_writing_fn mt_writing; // someone has written in the `name` attr. When path is 0, the gobj has been created with default values
    mt_reading_fn mt_reading; // opportunity to change the attr value before return attr
    mt_subscription_added_fn mt_subscription_added;      // refcount: 1 on the first subscription of this event. HACK return negative the subscription is rejected (unsubscribe).
    mt_subscription_deleted_fn mt_subscription_deleted;    // refcount: 1 on the last subscription of this event.
    mt_child_added_fn mt_child_added;     // called when the child is built, just after mt_create call.
    mt_child_removed_fn mt_child_removed;   // called when the child is almost live
    json_function_fn mt_stats;           // must return a webix json object 0 if asynchronous response, like all below. HACK match the stats's prefix only.
    json_function_fn mt_command_parser;  // User command parser. Preference over gclass.command_table. Return webix. HACK must implement AUTHZ
    gobj_action_fn mt_inject_event;     // Won't use the static built-in gclass machine? process yourself your events.
    mt_create_resource_fn mt_create_resource;
    mt_list_resource_fn mt_list_resource; // Can return an iter or a json, depends of gclass
    mt_save_resource_fn mt_save_resource;
    mt_delete_resource_fn mt_delete_resource;
    future_method_fn mt_future21; // OLD mt_add_child_resource_link;
    future_method_fn mt_future22; // OLD mt_delete_child_resource_link;
    mt_get_resource_fn mt_get_resource;
    mt_state_changed_fn mt_state_changed; // If this method is defined then the EV_STATE_CHANGED will not published
    mt_authenticate_fn mt_authenticate; // Return webix
    mt_list_children_fn mt_list_children;
    mt_stats_updated_fn mt_stats_updated;       // Return 0 if own the stats, or -1 if not.
    mt_disable_fn mt_disable;
    mt_enable_fn mt_enable;
    mt_trace_on_fn mt_trace_on;                 // Return webix
    mt_trace_off_fn mt_trace_off;               // Return webix
    mt_gobj_created_fn mt_gobj_created;         // ONLY for __yuno__.
    mt_set_bottom_gobj_fn mt_set_bottom_gobj;
    future_method_fn mt_future34;
    mt_publish_event_fn mt_publish_event;  // Return -1 (broke), 0 continue without publish, 1 continue and publish
    mt_publication_pre_filter_fn mt_publication_pre_filter; // Return -1,0,1
    mt_publication_filter_fn mt_publication_filter; // Return -1,0,1
    authorization_checker_fn mt_authz_checker;
    future_method_fn mt_future39;
    mt_create_node_fn mt_create_node;
    mt_update_node_fn mt_update_node;
    mt_delete_node_fn mt_delete_node;
    mt_link_nodes_fn mt_link_nodes;
    future_method_fn mt_future44;
    mt_unlink_nodes_fn mt_unlink_nodes;
    mt_topic_jtree_fn mt_topic_jtree;
    mt_get_node_fn mt_get_node;
    mt_list_nodes_fn mt_list_nodes;
    mt_shoot_snap_fn mt_shoot_snap;
    mt_activate_snap_fn mt_activate_snap;
    mt_list_snaps_fn mt_list_snaps;
    mt_treedbs_fn mt_treedbs;
    mt_treedb_topics_fn mt_treedb_topics;
    mt_topic_desc_fn mt_topic_desc;
    mt_topic_links_fn mt_topic_links;
    mt_topic_hooks_fn mt_topic_hooks;
    mt_node_parents_fn mt_node_parents;
    mt_node_children_fn mt_node_children;
    mt_list_instances_fn mt_list_instances;
    mt_node_tree_fn mt_node_tree;
    mt_topic_size_fn mt_topic_size;
    future_method_fn mt_future62;
    future_method_fn mt_future63;
    future_method_fn mt_future64;
} GMETHODS;

typedef json_t *(*internal_method_fn)(hgobj gobj, const char *lmethod, json_t *kw, hgobj src);

typedef struct { // Internal methods
    const char *lname;
    const internal_method_fn lm;
    const char *authz;
} LMETHOD;

/*
 *  trace_level_t is used to describe trace levels
 */
typedef struct {
    const char *name;
    const char *description;
} trace_level_t;

typedef enum { // HACK strict ascendant value!, strings in gclass_flag_names
    gcflag_manual_start             = 0x0001,   // gobj_start_tree() don't start. TODO do same with stop
    gcflag_no_check_output_events   = 0x0002,   // When publishing don't check events in output_event_list.
    gcflag_ignore_unknown_attrs     = 0x0004,   // When creating a gobj, ignore not existing attrs
    gcflag_required_start_to_play   = 0x0008,   // Don't to play if no start done.
    gcflag_singleton                = 0x0010,   // Can only have one instance
} gclass_flag_t;

typedef enum { // HACK strict ascendant value!, strings in gobj_flag_names
    gobj_flag_yuno              = 0x0001,
    gobj_flag_default_service   = 0x0002,
    gobj_flag_service           = 0x0004,   // Interface (events, attrs, commands, stats) available to external access
    gobj_flag_volatil           = 0x0008,
    gobj_flag_pure_child        = 0x0010,   // Pure child sends events directly to parent, others publish them
    gobj_flag_autostart         = 0x0020,   // Set by gobj_create_tree/gobj_service_factory too
    gobj_flag_autoplay          = 0x0040,   // Set by gobj_create_tree/gobj_service_factory too
} gobj_flag_t;


/***************************************************************
 *      Macros to define and declare the
 *          - GClases names
 *          - States
 *          - Events and some System Events
 *
 ***************************************************************/
// Macro GClasses
#define GOBJ_DEFINE_GCLASS(id) gclass_name_t id = #id
#define GOBJ_DECLARE_GCLASS(id) extern gclass_name_t id

// Macro States
#define GOBJ_DEFINE_STATE(id) gobj_state_t id = #id
#define GOBJ_DECLARE_STATE(id) extern gobj_state_t id

// Macro Events
#define GOBJ_DEFINE_EVENT(id) gobj_event_t id = #id
#define GOBJ_DECLARE_EVENT(id) extern gobj_event_t id

/***************************************************************
 *              Prototypes
 ***************************************************************/
/*---------------------------------*
 *      Start up functions
 *---------------------------------*/
typedef struct {
    startup_persistent_attrs_fn  startup;
    end_persistent_attrs_fn      end;
    load_persistent_attrs_fn     load;
    save_persistent_attrs_fn     save;
    remove_persistent_attrs_fn   remove;
    list_persistent_attrs_fn     list;
} persistent_attrs_t;

PUBLIC int gobj_start_up(       /* Initialize the gobj's system */
    int                         argc,                   /* pass main() arguments */
    char                        *argv[],                /* pass main() arguments */
    const json_t                *jn_global_settings,    /* NOT owned */
    const persistent_attrs_t    *persistent_attrs,
    json_function_fn            global_command_parser,  /* if NULL, use internal command parser */
    json_function_fn            global_statistics_parser,    /* if NULL, use internal stats parser */
    authorization_checker_fn    global_authorization_checker,   /* authorization checker function */
    authentication_parser_fn    global_authentication_parser  /* authentication parser function */
);

PUBLIC void gobj_shutdown(void); /* Shutdown the yuno, pausing and stopping the default service, the service's gobj and finally the __root__ */
PUBLIC BOOL gobj_is_shutdowning(void);  /* Check if yuno is shutdowning */
PUBLIC void gobj_set_exit_code(int exit_code); // set return code to exit when running as daemon
PUBLIC int gobj_get_exit_code(void);
PUBLIC void gobj_end(void);     /* De-initialize the gobj's system, free resources */

/*---------------------------------*
 *      GClass functions
 *---------------------------------*/
PUBLIC hgclass gclass_create( // create and register gclass
    gclass_name_t gclass_name,
    event_type_t *event_types,
    states_t *states,
    const GMETHODS *gmt,
    const LMETHOD *lmt,
    const sdata_desc_t *attrs_table,
    size_t priv_size,
    const sdata_desc_t *authz_table,
    const sdata_desc_t *command_table,
    const trace_level_t *s_user_trace_level,
    gclass_flag_t gclass_flag
);

PUBLIC int gclass_add_state(
    hgclass gclass,
    gobj_state_t state_name
);
PUBLIC int gclass_add_ev_action(
    hgclass gclass,
    gobj_state_t state_name,
    gobj_event_t event,
    gobj_action_fn action,
    gobj_state_t next_state
);
PUBLIC int gclass_add_event_type(
    hgclass gclass,
    gobj_event_t event_name,
    event_flag_t event_flag
);

PUBLIC event_type_t *gclass_find_event(const char *event, event_flag_t event_flag, BOOL verbose); // Find a HACK string event in any gclass

PUBLIC gobj_event_t gclass_find_public_event(const char *event, BOOL verbose); // Find a public HACK string event in any gclass

PUBLIC void gclass_unregister(hgclass hgclass);
PUBLIC gclass_name_t gclass_gclass_name(hgclass gclass);
PUBLIC BOOL gclass_has_attr(hgclass gclass, const char* name);

PUBLIC json_t *gclass_gclass_register(void); /* Get registered gclasses: Return [gclass:s}] */
PUBLIC hgclass gclass_find_by_name(gclass_name_t gclass_name); // gclass_name can be 'char *' or gclass_name_t
PUBLIC event_type_t *gclass_find_event_type(hgclass gclass, gobj_event_t event);

PUBLIC int gclass_check_fsm(hgclass gclass);
PUBLIC json_t *gclass2json(hgclass gclass); // Return a dict with gclass's description.
//  Return the data description of the command `command`
//  If `command` is null returns full command's table
PUBLIC const sdata_desc_t *gclass_command_desc(hgclass gclass, const char *name, BOOL verbose);


/*---------------------------------*
 *      GObj creation functions
 *---------------------------------*/
PUBLIC json_t * gobj_service_register(void); /* Get registered services: Return [{gclass:s, service:s}] */

PUBLIC hgobj gobj_create(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent
);

PUBLIC hgobj gobj_create2(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent,
    gobj_flag_t gobj_flag
);

PUBLIC hgobj gobj_create_yuno(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw // owned
);
PUBLIC hgobj gobj_create_service(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent
);

/*
 *  Default service has autostart but no autoplay: it will be played by play method of yuno
 */
PUBLIC hgobj gobj_create_default_service(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent
);

PUBLIC hgobj gobj_create_volatil(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent
);

PUBLIC hgobj gobj_create_pure_child(
    const char *gobj_name,
    gclass_name_t gclass_name,
    json_t *kw, // owned
    hgobj parent
);

/*
 *  OPTIONS 'service tree'
 *
    {
        'gclass': 'X',
        'name': 'x',
        'default_service': false,
        'as_service': false, || 'service': false,
        'autostart': true,
        'autoplay': true,
        'disabled': false,
        'pure_child': false

        // Attributes of the gobj

        'kw': {
            'subscriber': 'service' // It 'subscriber' is not set, the subscriber will be the parent
        }

        // Children of the gobj

        'children': [
            {
                'gclass': 'Xx',
                'name': 'xx',
                'autostart': true,
                ...

                'kw': {
                }
                'children': [
                ]
            }
        ]
    }

    HACK: It 'subscriber' is not set, the subscriber will be the parent

    HACK: If there is only one child in children, this will be set as gobj_set_bottom_gobj

    HACK: Rules for proper object usage
        - Inherently CHILD objects will ask if they are services to use gobj_publish_event()
            instead of gobj_send_event() to parent.
        - Inherently SERVICE objects will ask if they are pure_child to use gobj_send_event()
            instead of gobj_publish_event()
*/

/*
 *  Factory to create service gobj
 *  Used in entry_point, to run services
 *  SEE OPTIONS 'service tree'
 *  The first gobj is marked as service
 */
PUBLIC hgobj gobj_service_factory(
    const char *name,
    json_t * jn_service_config // owned, binary json of 'service tree'
    // get json_config_variables from global
);

/*
 *  Parse tree_config and build a service tree
 *  SEE OPTIONS 'service tree'
 *  Used by ycommand,ybatch,ystats,ytests,cli,...
 */
PUBLIC hgobj gobj_create_tree(
    hgobj parent,
    const char *tree_config,        // Can have comments #^^, string json of 'service tree'
    json_t *json_config_variables   // owned, values to apply to attributes variables
);


PUBLIC void gobj_destroy(hgobj gobj);

PUBLIC void gobj_destroy_children(hgobj gobj);
PUBLIC int gobj_destroy_named_children(hgobj gobj, const char *name); // with auto pause/stop


/*---------------------------------*
 *  Attribute functions
 *---------------------------------*/
/*
 *  gobj_load_persistent_attrs() is automatically executed on creation of gobj.
 *  The persistent attributes saved have the more prevalence over other json configuration
 *  ONLY unique gobjs can load/save persistent attributes.
 *  gobj_save_persistent_attrs() must be manually executed.
 *
 *  Persistent attrs now can be saved/removed individually,
 *  attrs can be a string, a list of keys, or a dict with the keys to saved/deleted
 *  if attrs is empty list/save/remove all attrs
 */
PUBLIC int gobj_load_persistent_attrs(  // str, list or dict. Only gobj services can load/save writable-persistent
    hgobj gobj,
    json_t *jn_attrs  // owned, WARNING: persistent attrs are loaded automatically in the creation of gobjs
);
PUBLIC int gobj_save_persistent_attrs(  // str, list or dict. Only gobj services can load/save writable-persistent
    hgobj gobj,
    json_t *jn_attrs  // owned
);
PUBLIC int gobj_remove_persistent_attrs( // str, list or dict
    hgobj gobj,
    json_t *jn_attrs  // owned, Only gobj services can load/save writable-persistent
);
PUBLIC json_t *gobj_list_persistent_attrs( // str, list or dict
    hgobj gobj,
    json_t *jn_attrs  // owned
);

/*
 *  Attribute functions WITHOUT bottom inheritance
 */
PUBLIC json_t *gobj_sdata_create(hgobj gobj, const sdata_desc_t* schema);

// Return the data description of the attribute `attr`
// If `attr` is null returns full attr's table
PUBLIC const sdata_desc_t *gclass_attr_desc(hgclass gclass, const char *attr, BOOL verbose);
PUBLIC const sdata_desc_t *gobj_attr_desc(hgobj gobj, const char *attr, BOOL verbose);
PUBLIC data_type_t gobj_attr_type(hgobj gobj, const char *name);
PUBLIC json_t *gobj_hsdata(hgobj gobj); // Return is NOT YOURS

PUBLIC const sdata_desc_t *gclass_authz_desc(hgclass gclass);

PUBLIC BOOL gobj_has_attr(hgobj hgobj, const char *name);
PUBLIC BOOL gobj_is_readable_attr(hgobj gobj, const char *name); // True is attr is SDF_RD (public readable)
PUBLIC BOOL gobj_is_writable_attr(hgobj gobj, const char *name);
PUBLIC int gobj_reset_volatil_attrs(hgobj gobj);
PUBLIC int gobj_reset_rstats_attrs(hgobj gobj);

PUBLIC json_t *gobj_read_attr( // Return is NOT yours!
    hgobj gobj,
    const char *path, // If it has ` then segments are gobj and leaf is the attribute (+bottom) TODO
    hgobj src
);

PUBLIC json_t *gobj_read_attrs( // Return is yours!
    hgobj gobj,
    sdata_flag_t include_flag,
    hgobj src
);

PUBLIC json_t *gobj_read_user_data(
    hgobj gobj,
    const char *name // If empty name return all user record
);

PUBLIC int gobj_write_attr(
    hgobj gobj,
    const char *path, // If it has ` then segments are gobj and leaf is the attribute (+bottom) TODO
    json_t *value,  // owned
    hgobj src
);
PUBLIC int gobj_write_attrs(
    hgobj gobj,
    json_t *kw,  // owned
    sdata_flag_t include_flag,
    hgobj src
);
PUBLIC int gobj_write_user_data(
    hgobj gobj,
    const char *name,
    json_t *value // owned
);

/*
 *  Attribute functions WITH bottom inheritance
 */

/*
 *  Attributes low level functions
 *  Acceso directo a la variable (con herencia de bottoms): puntero y descripción del atributo.
 *  ATTR: read the attr pointer, traversing inherited gobjs if need it.
 */
PUBLIC json_t *gobj_hsdata2(hgobj gobj, const char *name, BOOL verbose); // Return is NOT YOURS
PUBLIC BOOL gobj_has_bottom_attr(hgobj gobj_, const char *name);

PUBLIC const char *gobj_read_str_attr(hgobj gobj, const char *name);
PUBLIC BOOL gobj_read_bool_attr(hgobj gobj, const char *name);
PUBLIC json_int_t gobj_read_integer_attr(hgobj gobj, const char *name);
PUBLIC double gobj_read_real_attr(hgobj gobj, const char *name);
PUBLIC json_t *gobj_read_json_attr(hgobj gobj, const char *name); // WARNING return its NOT YOURS
PUBLIC void *gobj_read_pointer_attr(hgobj gobj, const char *name);

/*
 *  WARNING these gobj_write_*() functions DO NOT check the type of attr,
 *  it's your responsibility to use the correct json type
 */
PUBLIC int gobj_write_str_attr(hgobj gobj, const char *name, const char *value); // // WARNING value == 0  -> json_null()
PUBLIC int gobj_write_strn_attr(hgobj gobj_, const char *name, const char *s, size_t len);
PUBLIC int gobj_write_bool_attr(hgobj gobj, const char *name, BOOL value);
PUBLIC int gobj_write_integer_attr(hgobj gobj, const char *name, json_int_t value);
PUBLIC int gobj_write_real_attr(hgobj gobj, const char *name, double value);
PUBLIC int gobj_write_json_attr(hgobj gobj, const char *name, json_t *value);
PUBLIC int gobj_write_new_json_attr(hgobj gobj, const char *name, json_t *value);
PUBLIC int gobj_write_pointer_attr(hgobj gobj, const char *name, void *value);

/*--------------------------------------------*
 *  Operational functions
 *--------------------------------------------*/
PUBLIC json_t *gobj_local_method(
    hgobj gobj,
    const char *lmethod,
    json_t *kw,
    hgobj src
);

PUBLIC int gobj_start(hgobj gobj);
PUBLIC int gobj_start_children(hgobj gobj);   // only direct children
PUBLIC int gobj_start_tree(hgobj gobj);     // children with gcflag_manual_start flag are not started.
PUBLIC int gobj_stop(hgobj gobj);
PUBLIC int gobj_stop_children(hgobj gobj);    // only direct children
PUBLIC int gobj_stop_tree(hgobj gobj);      // all tree of children
PUBLIC int gobj_play(hgobj gobj);
PUBLIC int gobj_pause(hgobj gobj);
PUBLIC int gobj_enable(hgobj gobj); // exec own mt_enable() or gobj_start_tree()
PUBLIC int gobj_disable(hgobj gobj); // exec own mt_disable() or gobj_stop_tree()

PUBLIC int gobj_change_parent(hgobj gobj, hgobj gobj_new_parent); // TODO already implemented in js

PUBLIC int gobj_autostart_services(void);
PUBLIC int gobj_autoplay_services(void);
PUBLIC int gobj_stop_autostart_services(void);
PUBLIC int gobj_pause_autoplay_services(void);

/*
 *  mt_command must return a webix json or 0 on asynchronous responses.
 */
PUBLIC json_t *gobj_command( // With AUTHZ
    hgobj gobj,
    const char *command,
    json_t *kw,
    hgobj src
);

/*
 *  Return a dict with attrs marked with SDF_STATS and stats_metadata
 */
PUBLIC json_t * gobj_stats( // Call mt_stats() or __global_stats_parser_fn__ (build_stats)
    hgobj gobj,
    const char* stats,
    json_t* kw,
    hgobj src
);

PUBLIC hgobj gobj_set_bottom_gobj(hgobj gobj, hgobj bottom_gobj); // return previous bottom_gobj (MT)
PUBLIC hgobj gobj_last_bottom_gobj(hgobj gobj); // inherit attributes
PUBLIC hgobj gobj_bottom_gobj(hgobj gobj);

PUBLIC json_t *gobj_services(void); // return list of strings, must be owned
PUBLIC hgobj gobj_default_service(void);
PUBLIC hgobj gobj_find_service(const char *service, BOOL verbose);
PUBLIC hgobj gobj_find_service_by_gclass(const char *gclass_name, BOOL verbose); // OLD gobj_find_gclass_service
PUBLIC hgobj gobj_find_gobj(const char *gobj_path); // find gobj by path (full path)

PUBLIC hgobj gobj_first_child(hgobj gobj);
PUBLIC hgobj gobj_last_child(hgobj gobj);
PUBLIC hgobj gobj_next_child(hgobj child);
PUBLIC hgobj gobj_prev_child(hgobj child);
PUBLIC hgobj gobj_child_by_name(hgobj gobj, const char *name);
PUBLIC hgobj gobj_child_by_index(hgobj gobj, size_t index); // relative to 1
PUBLIC size_t gobj_child_size(hgobj gobj);
PUBLIC size_t gobj_child_size2(
    hgobj gobj_,
    json_t *jn_filter // owned
);
PUBLIC hgobj gobj_search_path(hgobj gobj, const char *path);

/*
    jn_filter: a dict with names of attributes with the value to mach,
        or a system name like:

        '__inherited_gclass_name__'
        '__gclass_name__'
        '__gobj_name__'
        '__prefix_gobj_name__'
        '__state__'
        '__disabled__'


 */

PUBLIC BOOL gobj_match_gobj( // Check if gobj match jn_filter conditions
    hgobj gobj,
    json_t *jn_filter // owned
);
PUBLIC hgobj gobj_find_child( // returns the first matched child
    hgobj gobj,
    json_t *jn_filter // owned
);

PUBLIC hgobj gobj_find_child_by_tree( // TODO already implemented in js
    hgobj gobj,
    json_t *jn_filter // owned
);

PUBLIC json_t *gobj_match_children( // return an iter of first level matching jn_filter
    hgobj gobj,
    json_t *jn_filter   // owned
);

PUBLIC json_t *gobj_match_children_tree( // return an iter of any level matching jn_filter
    hgobj gobj,
    json_t *jn_filter   // owned
);

PUBLIC int gobj_free_iter(json_t *iter); // Free the iter (list of hgobj)

typedef enum {
    /*
     * One must be selected
     */
    WALK_TOP2BOTTOM = 0x0001,
    WALK_BOTTOM2TOP = 0x0002,
    WALK_BYLEVEL    = 0x0004,

    /*
     * One must be selected when WALK_BYLEVEL is selected.
     */
    WALK_FIRST2LAST = 0x0010,
    WALK_LAST2FIRST = 0x0020,
} walk_type_t;

typedef int (*cb_walking_t)(
    hgobj gobj,
    void *user_data,
    void *user_data2,
    void *user_data3
);

PUBLIC int gobj_walk_gobj_children(
    hgobj gobj,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3
);
PUBLIC int gobj_walk_gobj_children_tree(
    hgobj gobj,
    walk_type_t walk_type,
    cb_walking_t cb_walking,
    void *user_data,
    void *user_data2,
    void *user_data3
);

/*---------------------------------*
 *      Info functions
 *---------------------------------*/
PUBLIC hgobj gobj_yuno(void); // Return yuno, the grandfather (Only one yuno per process, single thread)

PUBLIC const char *gobj_yuno_name(void);
PUBLIC const char *gobj_yuno_role(void);
PUBLIC const char *gobj_yuno_id(void);
PUBLIC const char *gobj_yuno_tag(void);
PUBLIC const char *gobj_yuno_role_plus_name(void);

PUBLIC const char *gobj_yuno_realm_id(void);
PUBLIC const char *gobj_yuno_realm_owner(void);
PUBLIC const char *gobj_yuno_realm_role(void);
PUBLIC const char *gobj_yuno_realm_name(void);
PUBLIC const char *gobj_yuno_realm_env(void);
PUBLIC const char *gobj_yuno_node_owner(void);

PUBLIC const char * gobj_name(hgobj gobj);
PUBLIC gclass_name_t gobj_gclass_name(hgobj gobj);
PUBLIC hgclass gobj_gclass(hgobj gobj);
PUBLIC const char * gobj_full_name(hgobj gobj);
PUBLIC const char * gobj_short_name(hgobj gobj);
PUBLIC json_t * gobj_global_variables(void);
PUBLIC void * gobj_priv_data(hgobj gobj);
PUBLIC hgobj gobj_parent(hgobj gobj);
PUBLIC BOOL gobj_is_destroying(hgobj gobj);
PUBLIC BOOL gobj_is_running(hgobj gobj);
PUBLIC BOOL gobj_is_playing(hgobj gobj);
PUBLIC BOOL gobj_is_service(hgobj gobj);
PUBLIC BOOL gobj_is_disabled(hgobj gobj);
PUBLIC BOOL gobj_is_volatil(hgobj gobj);
PUBLIC int gobj_set_volatil(hgobj gobj, BOOL set);
PUBLIC BOOL gobj_is_pure_child(hgobj gobj);
PUBLIC BOOL gobj_is_bottom_gobj(hgobj gobj);

PUBLIC BOOL gobj_typeof_gclass(hgobj gobj, const char *gclass_name);            /* strict same gclass */
PUBLIC BOOL gobj_typeof_inherited_gclass(hgobj gobj, const char *gclass_name);  /* check inherited (bottom) gclass */
PUBLIC size_t get_max_system_memory(void);
PUBLIC size_t get_cur_system_memory(void);

//  Return the data description of the command `command`
//  If `command` is null returns full command's table
PUBLIC const sdata_desc_t *gobj_command_desc(hgobj gobj, const char *name, BOOL verbose);

PUBLIC const char **get_sdata_flag_table(void); // Table of sdata (attr) flag names

PUBLIC json_t *get_attrs_schema(hgobj gobj);   // List with description (schema) of gobj's attributes.

/*
 *  jn_filter of gobj2json and gobj_view_tree: list of strings
            "fullname",
            "shortname",
            "gclass",
            "name",
            "parent",
            "attrs",
            "user_data",
            "gobj_flags",
            "state",
            "running",
            "playing",
            "service",
            "bottom_gobj",
            "disabled",
            "volatil",
            "commands", // true if it has commands
            "gobj_trace_level",
            "gobj_trace_no_level",
 */
PUBLIC json_t *gobj2json( // Return a dict with gobj's description.
    hgobj gobj,
    json_t *jn_filter // owned
);
PUBLIC json_t *gobj_view_tree(  // Return tree with gobj's tree.
    hgobj gobj,
    json_t *jn_filter // owned
);

/*--------------------------------------------*
 *          Events and States
 *--------------------------------------------*/
PUBLIC int gobj_send_event(
    hgobj dst,
    gobj_event_t event,
    json_t *kw,  // owned
    hgobj src
);

PUBLIC int gobj_send_event_to_children(  // Send the event to all children of first level supporting the event
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    hgobj src
);
PUBLIC int gobj_send_event_to_children_tree( // same as gobj_send_event_to_children but recursive
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,
    hgobj src
);


PUBLIC BOOL gobj_change_state(
    hgobj gobj,
    gobj_state_t state_name
);
PUBLIC gobj_state_t gobj_current_state(hgobj gobj);
PUBLIC BOOL gobj_in_this_state(hgobj gobj, gobj_state_t state);
PUBLIC BOOL gobj_has_state(hgobj gobj, gobj_state_t state);
PUBLIC hgclass gobj_state_find_by_name(gclass_name_t gclass_name); // gclass_name can be 'char *' or gclass_name_t


PUBLIC BOOL gobj_has_event(hgobj gobj, gobj_event_t event, event_flag_t event_flag); // old gobj_event_in_input_event_list
PUBLIC BOOL gobj_has_output_event(hgobj gobj, gobj_event_t event, event_flag_t event_flag); // old gobj_event_in_output_event_list

PUBLIC event_type_t *gobj_event_type( // silent function
    hgobj gobj,
    gobj_event_t event,
    BOOL include_system_events
);
PUBLIC event_type_t *gobj_event_type_by_name(hgobj gobj, const char *event_name); // Get event_type by HACK string event, silent function

/*--------------------------------------------*
 *          Publication/Subscriptions
 *--------------------------------------------*/
PUBLIC const sdata_desc_t *gobj_subs_desc(void);

/*
Sub-dictionaries to use in gobj_subscribe_event() kw
====================================================
HACK Only these keys are let, remain of keywords will be ignored!

    __config__   Dict. Parameters to custom gobj_subscribe_event() behaviour.
            System Parameters:
            - "__hard_subscription__":  bool
                True for permanent subscription.
                This subscription cannot be remove,
                (Well, with force you can)

            - "__own_event__": bool
                If __own_event__ defined and gobj_send_event inside of gobj_publish_event
                returns -1 then don't continue publishing

            - "__rename_event_name__": "new event name" TODO review
                You can rename the output original event name.
                The :attr:`original_event_name` attribute is added to
                the sent event with the value of the original event name.

            - "__first_shot__": bool TODO review
                If True then subscriber wants receive the event on subscription.
                Subscribing gobj mt_subscription_added() will be called
                and it's his responsibility to check the flag and to emit the event.

            - "__trans_filter__": string or string's list TODO review
                Transform kw to publish with transformation filters


    __global__  Dict. Base kw to be merged with the publishing kw
                (base kw will be updated by publishing kw).
                It's the common data received in each publication. Constants, global variables!

    __local__   Dict. Dictionary or list with Keys (path) to be deleted from kw before to publish.

    __filter__  Selection Filter: Enable to publish only messages matching the filter.
                Used by gobj_publish_event().

*/

PUBLIC json_t *gobj_subscribe_event( // return not yours
    hgobj publisher,
    gobj_event_t event,
    json_t *kw, // kw (__config__, __global__, __local__)
    hgobj subscriber
);
PUBLIC int gobj_unsubscribe_event(
    hgobj publisher,
    gobj_event_t event,
    json_t *kw, // kw (__config__, __global__, __local__)
    hgobj subscriber
);
PUBLIC int gobj_unsubscribe_list(
    hgobj gobj,
    json_t *dl_subs, // owned
    BOOL force  // delete hard_subscription subs too
);

PUBLIC json_t *gobj_find_subscriptions( // Return is YOURS
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,             // kw (__config__, __global__, __local__)
    hgobj subscriber
);

PUBLIC json_t *gobj_find_subscribings( // Return is YOURS
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,             // kw (__config__, __global__, __local__)
    hgobj publisher
);
PUBLIC json_t *gobj_list_subscriptions(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,             // kw (__config__, __global__, __local__)
    hgobj subscriber
);
PUBLIC json_t *gobj_list_subscribings(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,             // kw (__config__, __global__, __local__)
    hgobj subscriber
);

/*
 *  In general the meaning of returns are:
 *     -1  broke (could be interpreted like 'own the event and don't publish to others)
 *      0  continue without publish
 *      1  publish and continue
 *
 *  The process of publishing is:
 *  1) Own mt_publish_event method: If publisher gclass has mt_publish_event() call it.
 *      if return <= 0 return (all publishing process done by gclass)
 *      if return >0 continue with publishing process of gobj.c
 *
 *  2) LOOP over subscriptions:
 *      1) Pre-filter: If publisher gclass has mt_publication_pre_filter call it (before KW filling)
 *          kw NOT owned! you can modify the publishing kw
 *          Return:
 *              -1  broke
 *              0  continue without publish
 *              1  continue to publish
 *
 *      2) Check if System event: don't send if subscriber has not it
 *
 *      3) KW filling
 *          - Remove local keys (defined in __local__)
 *          - Add global keys (defined in __global__)
 *
 *      4) Publish (gobj_send_event to subscriber)
 *          If __own_event__ defined and return -1 don't continue publishing
 *
 */
PUBLIC int gobj_publish_event( //  Return the sum of returns of gobj_send_event
    hgobj publisher,
    gobj_event_t event,
    json_t *kw  // this kw extends kw_request.
);

/*--------------------------------------------*
 *      AUTHZ Authorization functions
 *--------------------------------------------*/
/*
 *  Global authorization levels
 *
    "__read_attribute__",       "Authorization to read gobj's attributes"
        params: "path"

    "__write_attribute__",      "Authorization to write gobj's attributes"
        params: "path"

    "__execute_command__",      "Authorization to execute gobj's commands"
        params: "command", "kw"

    "__inject_event__",         "Authorization to inject events to gobj"

    "__subscribe_event__",      "Authorization to subscribe events of gobj"
        params: "event", "kw"

    "__read_stats__"            "Authorization to read gobj's stats"
        params: "stats", "kw"

    "__reset_stats__"           "Authorization to reset gobj's stats"
        params: "stats", "kw"

 */

/*
 *  Authenticate
 *  Return json response
 *
        {
            "result": 0,        // 0 successful authentication, -1 error
            "comment": "",
            "username": ""      // username authenticated
        }

 *  HACK if there is no authentication parser the authentication is TRUE
    and the username is the current system user
 *  WARNING Becare and use no parser only in local services!
 */
PUBLIC json_t *gobj_authenticate(
    hgobj gobj,
    json_t *kw,
    hgobj src
);

PUBLIC json_t *gobj_authzs( // list authzs of gobj
    hgobj gobj  // If null return global authzs
);
PUBLIC json_t *gobj_authz( // return authz of gobj
    hgobj gobj,
    const char *authz // return all list if empty string, else return authz desc
);

/*
 *  Return if user has authz in gobj in context
 *  HACK if there is no authz checker the authz is TRUE
 */
PUBLIC BOOL gobj_user_has_authz(
    hgobj gobj,
    const char *authz,
    json_t *kw,
    hgobj src
);

PUBLIC const sdata_desc_t *gobj_get_global_authz_table(void);

PUBLIC json_t *authzs_list(
    hgobj gobj,
    const char *authz
);

PUBLIC const sdata_desc_t *authz_get_level_desc(
    const sdata_desc_t *authz_table,
    const char *authz
);

PUBLIC json_t *gobj_build_authzs_doc(
    hgobj gobj,
    const char *cmd,
    json_t *kw
);

/*--------------------------------------------*
 *  Stats functions
 *--------------------------------------------*/
PUBLIC json_int_t gobj_set_stat(hgobj gobj, const char *path, json_int_t value); // return old value
PUBLIC json_int_t gobj_incr_stat(hgobj gobj, const char *path, json_int_t value); // return new value
PUBLIC json_int_t gobj_decr_stat(hgobj gobj, const char *path, json_int_t value); // return new value
PUBLIC json_int_t gobj_get_stat(hgobj gobj, const char *path);
PUBLIC json_t *gobj_jn_stats(hgobj gobj);  // WARNING the json return is NOT YOURS!


/*-----------------------------------------------------*
 *  Resource functions
 *  Here the 'resource' is the 'key' of value (record)
 *-----------------------------------------------------*/
PUBLIC json_t *gobj_create_resource( // Return is NOT YOURS
    hgobj gobj,
    const char *resource,
    json_t *kw,  // owned
    json_t *jn_options // owned
);
PUBLIC int gobj_save_resource(
    hgobj gobj,
    const char *resource,
    json_t *record,     // WARNING NOT owned
    json_t *jn_options // owned
);

PUBLIC int gobj_delete_resource(
    hgobj gobj,
    const char *resource,
    json_t *record,     // owned
    json_t *jn_options // owned
);

PUBLIC json_t *gobj_list_resource( // // Return is YOURS, remember free it
    hgobj gobj,
    const char *resource,
    json_t *jn_filter,  // owned
    json_t *jn_options // owned
);
PUBLIC json_t *gobj_get_resource( // WARNING return is NOT yours!
    hgobj gobj,
    const char *resource,
    json_t *jn_filter,  // owned
    json_t *jn_options // owned
);


/*--------------------------------------------------*
 *  Node functions.
 *  node functions only in __linux__
 *--------------------------------------------------*/
#ifdef __linux__
PUBLIC json_t *gobj_treedbs( // Return a list with treedb names
    hgobj gobj,
    json_t *kw,
    hgobj src
);

PUBLIC json_t *gobj_treedb_topics(
    hgobj gobj,
    const char *treedb_name,
    json_t *options, // "dict" return list of dicts, otherwise return list of strings
    hgobj src
);

PUBLIC json_t *gobj_topic_desc(
    hgobj gobj,
    const char *topic_name
);

PUBLIC json_t *gobj_topic_links(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
    hgobj src
);

PUBLIC json_t *gobj_topic_hooks(
    hgobj gobj,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw,
    hgobj src
);

PUBLIC size_t gobj_topic_size(
    hgobj gobj,
    const char *topic_name,
    const char *key
);

PUBLIC json_t *gobj_create_node( // Return is YOURS
    hgobj gobj,
    const char *topic_name,
    json_t *kw,
    json_t *jn_options, // fkey,hook options
    hgobj src
);

PUBLIC json_t *gobj_update_node( // Return is YOURS
    hgobj gobj,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    json_t *jn_options, // "create" "autolink" "volatil" fkey,hook options
    hgobj src
);

PUBLIC int gobj_delete_node(
    hgobj gobj,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    json_t *jn_options, // "force"
    hgobj src
);

PUBLIC int gobj_link_nodes(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent_record,  // owned
    const char *child_topic_name,
    json_t *child_record,  // owned
    hgobj src
);

PUBLIC int gobj_unlink_nodes(
    hgobj gobj,
    const char *hook,
    const char *parent_topic_name,
    json_t *parent_record,  // owned
    const char *child_topic_name,
    json_t *child_record,  // owned
    hgobj src
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

PUBLIC json_t *gobj_get_node( // Return is YOURS
    hgobj gobj,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    json_t *jn_options, // fkey,hook options
    hgobj src
);

PUBLIC json_t *gobj_list_nodes( // Return MUST be decref
    hgobj gobj,
    const char *topic_name,
    json_t *jn_filter,
    json_t *jn_options, // "include-instances" fkey,hook options
    hgobj src
);

PUBLIC json_t *gobj_list_instances(
    hgobj gobj,
    const char *topic_name,
    const char *pkey2_field,
    json_t *jn_filter,
    json_t *jn_options, // owned, fkey,hook options
    hgobj src
);

/*
 *  Return a list of parent **references** pointed by the link (fkey)
 *  If no link return all links
 */
PUBLIC json_t *gobj_node_parents( // Return MUST be decref
    hgobj gobj,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    const char *link,
    json_t *jn_options, // fkey options
    hgobj src
);

/*
 *  Return a list of child nodes of the hook
 *  If no hook return all hooks
 */
PUBLIC json_t *gobj_node_children( // Return MUST be decref
    hgobj gobj,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the node
    const char *hook,
    json_t *jn_filter,  // filter to children
    json_t *jn_options, // fkey,hook options, "recursive"
    hgobj src
);

/*
 *  Return a hierarchical tree of the self-link topic
 *  If "webix" option is true return webix style (dict-list),
 *      else list of dict's
 *  __path__ field in all records (id`id`... style)
 *  If root node is not specified then the first with no parent is used
 */
PUBLIC json_t *gobj_topic_jtree( // Return MUST be decref
    hgobj gobj,
    const char *topic_name,
    const char *hook,   // hook to build the hierarchical tree
    const char *rename_hook, // change the hook name in the tree response
    json_t *kw,         // 'id' and pkey2s fields are used to find the root node
    json_t *jn_filter,  // filter to match records
    json_t *jn_options, // fkey,hook options
    hgobj src
);

/*
 *  Return the full tree of a node (duplicated)
 */
PUBLIC json_t *gobj_node_tree( // Return MUST be decref
    hgobj gobj,
    const char *topic_name,
    json_t *kw,         // 'id' and pkey2s fields are used to find the root node
    json_t *jn_options, // "with_metatada"
    hgobj src
);

PUBLIC int gobj_shoot_snap(  // tag the current tree db
    hgobj gobj,
    const char *tag,
    json_t *kw,
    hgobj src
);
PUBLIC int gobj_activate_snap( // Activate tag (stop/start the gobj)
    hgobj gobj,
    const char *tag,
    json_t *kw,
    hgobj src
);
PUBLIC json_t *gobj_list_snaps( // Return MUST be decref, list of snaps
    hgobj gobj,
    json_t *filter,
    hgobj src
);

#endif /* node functions only in __linux__ */


/*--------------------------------------------*
 *          Trace functions
 *--------------------------------------------*/
/*
 *  Global trace levels
 *  16 higher bits for global use.
 *  16 lower bits for user use.
 */
enum { /* String table in s_global_trace_level */
    TRACE_MACHINE           = 0x00010000,
    TRACE_CREATE_DELETE     = 0x00020000,
    TRACE_CREATE_DELETE2    = 0x00040000,
    TRACE_SUBSCRIPTIONS     = 0x00080000,
    TRACE_START_STOP        = 0x00100000,
    TRACE_URING             = 0x00200000,
    TRACE_EV_KW             = 0x00400000,
    TRACE_AUTHZS            = 0x00800000,
    TRACE_STATES            = 0x01000000,
    TRACE_GBUFFERS          = 0x02000000,
    TRACE_TIMER_PERIODIC    = 0x04000000,
    TRACE_TIMER             = 0x08000000,
    TRACE_URING_TIME        = 0x10000000,
    TRACE_FS                = 0x20000000,
};
#define TRACE_USER_LEVEL    0x0000FFFF
#define TRACE_GLOBAL_LEVEL  0xFFFF0000

/*
 *  Global trace level names:
 *
 *      "machine"
 *      "create_delete"
 *      "create_delete2"
 *      "subscriptions"
 *      "start_stop"
 *      "monitor"
 *      "event_monitor"
 *      "liburing"
 *      "ev_kw"
 *      "authzs"
 *      "states"
 *      "gbuffers"
 *      "timer_periodic"
 *      "timer"
 *      "liburing_timer"
 */

/*
 *  gobj_repr_gclass_trace_levels():
 *      Return [{gclass:null, trace_levels:[s]}]
 */

PUBLIC json_t * gobj_repr_global_trace_levels(void);
/*
 *  gobj_repr_gclass_trace_levels():
 *      Return [{gclass:s, trace_levels:[s]}]
 */
PUBLIC json_t * gobj_repr_gclass_trace_levels(const char *gclass_name);

/*
 *  Return trace level list (user defined)
 */
PUBLIC json_t *gobj_trace_level_list(hgclass gclass);

/*
 *  Get traces set in gclass and gobj (return list of strings)
 */
PUBLIC json_t *gobj_get_global_trace_level(void);
PUBLIC json_t *gobj_get_gclass_trace_level(hgclass gclass);
PUBLIC json_t *gobj_get_gclass_trace_no_level(hgclass gclass);
PUBLIC json_t *gobj_get_gobj_trace_level(hgobj gobj);
PUBLIC json_t *gobj_get_gobj_trace_no_level(hgobj gobj);

/*
 *  Get traces set in tree of gclass or gobj
 */
PUBLIC json_t *gobj_get_gclass_trace_level_list(hgclass gclass);
PUBLIC json_t *gobj_get_gclass_trace_no_level_list(hgclass gclass);
PUBLIC json_t *gobj_get_gobj_trace_level_tree(hgobj gobj);
PUBLIC json_t *gobj_get_gobj_trace_no_level_tree(hgobj gobj);

PUBLIC uint32_t gobj_global_trace_level(void);
PUBLIC uint32_t gobj_trace_level(hgobj gobj);
PUBLIC uint32_t gobj_trace_no_level(hgobj gobj);
PUBLIC BOOL gobj_is_level_tracing(hgobj gobj, uint32_t level);
PUBLIC BOOL gobj_is_level_not_tracing(hgobj gobj, uint32_t level);

/*
 *  Set trace levels and no-set trace levels, in gclass and gobj
 *      - if gobj is null then the trace level is global.
 *      - if level is NULL, all global and gclass levels are set/reset.
 *      - if level is "", all gclass levels are set/reset.
 *      - if gobj is not null then call mt_trace_on/mt_trace_off
 */
PUBLIC int gobj_set_gobj_trace(hgobj gobj, const char* level, BOOL set, json_t* kw);
PUBLIC int gobj_set_gclass_trace(hgclass gclass, const char *level, BOOL set);
PUBLIC int gobj_set_deep_tracing(int level); /* 1 all but considering __gobj_trace_no_level__, > 1 all */
PUBLIC int gobj_get_deep_tracing(void);
PUBLIC int gobj_set_global_trace(const char *level, BOOL set); // If level is empty, set all global traces
PUBLIC int gobj_set_global_no_trace(const char *level, BOOL set); // If level is empty, set all global traces

/*
 *  With these trace filter functions you can trace the levels of a gclass
 *  but only those gobjs that match the attributes being filtering.
 *  WARNING: by now only string type attributes can be filtering.
 */
PUBLIC int gobj_load_trace_filter(hgclass gclass, json_t *jn_trace_filter); // owned
PUBLIC int gobj_add_trace_filter(hgclass gclass, const char *attr, const char *value);
PUBLIC int gobj_remove_trace_filter(hgclass gclass, const char *attr, const char *value);  // If attr is empty then remove all filters, if value is empty then remove all values of attr
PUBLIC json_t *gobj_get_trace_filter(hgclass gclass); // Return is not YOURS

/*
 *  Set no-trace-level
 */
PUBLIC int gobj_set_gclass_no_trace(hgclass gclass, const char *level, BOOL set);
PUBLIC int gobj_set_gobj_no_trace(hgobj gobj, const char *level, BOOL set);

/*
 *  Machine trace
 */
PUBLIC void trace_machine(const char *fmt, ...) JANSSON_ATTRS((format(printf, 1, 2)));
PUBLIC void trace_machine2(const char *fmt, ...) JANSSON_ATTRS((format(printf, 1, 2)));
PUBLIC void gobj_set_trace_machine_format(int format); /* 0 legacy default, */
PUBLIC char *tab(char *bf, int bflen);


#ifdef __cplusplus
}
#endif
