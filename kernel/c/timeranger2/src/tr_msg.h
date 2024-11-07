/****************************************************************************
 *          TR_MSG.C
 *
 *          Messages (ordered by pkey: active and their instances) with TimeRanger
 *

 Example of `list`:

 {
    "topic_name": "gps_events",
    "match_cond": {
        "max_key_instances": 1
    },
    "load_record_callback": 94862617778903,
    "messages": {
        ----
    }
}

 Example of `messages`:

{
    "352093088196125": {
        "instances": [
            {
                "imei": "352093088196125",
                "msg": "event",
                "t_rx": 1555318428,
                ...
                "__md_tranger__": {
                    "__rowid__": 13607,
                    "__t__": 1555318428,
                    "__tm__": 1555318405000,
                    "__offset__": 3216,
                    "__size__": 402,
                    "__user_flag__": 0,
                    "__system_flag__": 16777729
                }
            },
            {
            }
        ],
        "active": {
            "imei": "352093088196125",
            "msg": "event",
            "t_rx": 1555318428,
            ...
            "__md_tranger__": {
                "__rowid__": 13607,
                "__t__": 1555318428,
                ...
            }
        }
    },
    "key2": {
        ...
    },
    ...
}

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

/***************************************************************
 *              Structures
 ***************************************************************/


/***************************************************************
 *              Desc
 ***************************************************************/

#ifdef __DB_TRANGER_C__     /* ONLY to show json structures */

// TOPIC
static const json_desc_t topic_json_desc[] = {
    // Name             Type        Default
    {"pkey",            "str",      ""},            // Primary key of topic
    {"tkey",            "str",      ""},            // Time key of topic
    {"desc",            "dict",     "{}"},          // Desc of topic
    {"lists",           "list",     "[LIST]"},      // Opened lists of topic
    {0}
};

// LIST
static const json_desc_t list_json_desc[] = {
    // Name             Type        Default
    {"topic_name",      "str",      ""},            // List's topic
    {"match_cond",      "dict",     "{}"},          // Filter of messages
    {"messages",        "dict",     "{MESSAGE}"},   // Messages
    {0}
};

// MESSAGE
static const json_desc_t message_json_desc[] = {
    // Name             Type       Default
    {"active",          "dict",    "{INSTANCE}"},  // Active Instance
    {"instances",       "list",    "[INSTANCE]"},  // Instances of Message
    {0}
};

// INSTANCE
static const json_desc_t instance_json_desc[] = {
    // Name             Type       Default
    {"__md_tranger__",  "dict",    "{}"},    // Instance Metadata
    {"content",         "dict",    "{}"},    // Instance Content
    {0}
};

static topic_desc_t db_tranger_desc[] = {
    // Topic Name,      Pkey        Key Type    Tkey        Topic Json Desc
    {"TOPIC",           "",         0,          "",         topic_json_desc},
    {"LIST",            "",         0,          "",         list_json_desc},
    {"MESSAGE",         "",         0,          "",         message_json_desc},
    {"INSTANCE",        "",         0,          "",         instance_json_desc},
    {0}
};

#endif /* __DB_TRANGER_C__ */

/***************************************************************
 *              Prototypes
 ***************************************************************/
/**rst**
    Open topics for messages (Remember previously open tranger_startup())
**rst**/
PUBLIC int trmsg_open_topics(
    json_t *tranger,
    const topic_desc_t *descs
);
PUBLIC int trmsg_close_topics(
    json_t *tranger,
    const topic_desc_t *descs
);

PUBLIC int trmsg_add_instance(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_msg,  // owned
    md2_record_t *md_record
);

/*
    match_cond of second level:

        key                 (str) key
        rkey                (str) regular expression of key
                                  WARNING: loading form disk keys matched in rkey)
                                           but loading realtime of all keys

        max_key_instances   (int) Maximum number of instances per key.

        order_by_tm         (bool) Not use with max_key_instances=1

        trmsg_instance_callback (int) callback function,
                                     inform of loaded instance, chance to change content

        match_fields        (dict or list of dicts) Load instances that matches fields

    for the first level see:

        `Iterator match_cond` in timeranger2.h

 */

/*
 *  HACK Return of callback: TODO review
 *      0 do nothing (callback will create their own list, or not),
 *      1 add record to returned list.data,
 *      -1 break the load
 */
typedef int (*trmsg_instance_callback_t)(
    json_t *tranger,    // not yours
    json_t *list,       // not yours
    BOOL is_active,
    json_t *instance    // not yours
);

/*
 *  trmsg_open_list() use tranger2_open_list(), see doc
 */
PUBLIC json_t *trmsg_open_list( // WARNING loading all records causes delay in starting applications
    json_t *tranger,
    const char *topic_name,
    json_t *match_cond,  // owned
    json_t *extra       // owned
);

PUBLIC int trmsg_close_list(
    json_t *tranger,
    json_t *list
);

/*
 *  Functions returning items of list (NOT YOURS).
 */
PUBLIC json_t *trmsg_get_messages( // Return (NOT yours) dict: messages`
    json_t *list
);
PUBLIC json_t *trmsg_get_message( // Return (NOT yours) dict: "active" (dict) and "instances" (list)
    json_t *list,
    const char *key
);
PUBLIC json_t *trmsg_get_active_message( // Return (NOT yours) dict: messages`message(key)`active
    json_t *list,
    const char *key
);
PUBLIC json_t *trmsg_get_active_md( // Return (NOT yours) dict: messages`message(key)`active`__md_tranger__
    json_t *list,
    const char *key
);
PUBLIC json_t *trmsg_get_instances( // Return (NOT yours) list: messages`message(key)`instances
    json_t *list,
    const char *key
);

/*
 *  Return a list of **cloned** records with instances in 'data' hook.
 *  Ready for webix use.
 *  WARNING Returned value is yours, must be decref.
 */
PUBLIC json_t *trmsg_data_tree(
    json_t *list,
    json_t *jn_filter  // owned
);

/*
 *  Return a list of **cloned** records (list of dicts).
 *  WARNING Returned value is yours, must be decref.
 */
PUBLIC json_t *trmsg_active_records(
    json_t *list,
    json_t *jn_filter  // owned
);

/*
 *  Return a list of **cloned** record's instances (list of dicts).
 *  WARNING Returned value is yours, must be decref.
 */
PUBLIC json_t *trmsg_record_instances(
    json_t *list,
    const char *key,
    json_t *jn_filter  // owned
);

/*
 *  Foreach ACTIVE **cloned** messages
 */
PUBLIC int trmsg_foreach_active_messages(
    json_t *list,
    int (*callback)( // Return < 0 break the foreach
        json_t *list,  // Not yours!
        const char *key,
        json_t *record ,// It's yours, Must be owned
        void *user_data1,
        void *user_data2
    ),
    void *user_data1,
    void *user_data2,
    json_t *jn_filter  // owned
);

/*
 *  Foreach INSTANCES **cloned** messages
 */
PUBLIC int trmsg_foreach_instances_messages(
    json_t *list,
    int (*callback)( // Return < 0 break the foreach
        json_t *list,  // Not yours!
        const char *key,
        json_t *instances, // It's yours, Must be owned
        void *user_data1,
        void *user_data2
    ),
    void *user_data1,
    void *user_data2,
    json_t *jn_filter  // owned
);

/*
 *  Foreach **duplicated** or **cloned** MESSAGES
 *  You select if the parameter 'record' in the callback is
 *  a duplicated or cloned (incref original) record.
 */
PUBLIC int trmsg_foreach_messages(
    json_t *list,
    BOOL duplicated, // False then cloned messages
    int (*callback)( // Return < 0 break the foreach
        json_t *list,  // Not yours!
        const char *key,
        json_t *instances, // It's yours, Must be owned
        void *user_data1,
        void *user_data2
    ),
    void *user_data1,
    void *user_data2,
    json_t *jn_filter  // owned
);

#ifdef __cplusplus
}
#endif
