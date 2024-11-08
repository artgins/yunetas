/***********************************************************************
 *          TR_MSG.C
 *
 *          Messages (ordered by pkey: active and their instances) with TimeRanger
 *
 *          Copyright (c) 2019 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 *
    Load in memory a iter of topic's **active** messages, with n 'instances' of each key.
    If topic tag is 0:
        the active message of each key series is the **last** key instance.
    else:
        the active message is the **last** key instance with tag equal to topic tag.

***********************************************************************/
#include <string.h>

#include <kwid.h>
#include "tr_msg.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
    Open topics for messages (Remember previously open tranger_startup())
 ***************************************************************************/
PUBLIC int trmsg_open_topics(
    json_t *tranger,
    const topic_desc_t *descs
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    int ret = 0;
    for(int i=0; descs[i].topic_name!=0; i++) {
        const topic_desc_t *topic_desc = descs + i;

        json_t *topic = tranger2_create_topic(
            tranger,    // If the topic exists then only needs (tranger,name) parameters
            topic_desc->topic_name,
            topic_desc->pkey,
            topic_desc->tkey,
            NULL,
            topic_desc->system_flag,
            topic_desc->json_desc?create_json_record(gobj, topic_desc->json_desc):0, // owned
            0
        );
        if(!topic) {
            ret += -1;
        }
    }

    return ret;
}

/***************************************************************************
    Close topics for messages
 ***************************************************************************/
PUBLIC int trmsg_close_topics(
    json_t *tranger,
    const topic_desc_t *descs
)
{
    int ret = 0;
    for(int i=0; descs[i].topic_name!=0; i++) {
        const topic_desc_t *topic_desc = descs + i;

        ret += tranger2_close_topic(
            tranger,
            topic_desc->topic_name
        );
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int trmsg_add_instance(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_msg,  // owned
    md2_record_t *md_record
)
{
    md2_record_t md_record_;
    if(!md_record) {
        md_record = &md_record_;
    }

    if(tranger2_append_record(
        tranger,
        topic_name,
        0, // __t__,         // if 0 then the time will be set by TimeRanger with now time
        0, // user_flag,
        md_record,
        jn_msg // owned
    )<0) {
        // Error already logged
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Load messages in a dict() of keys,
 *  and each key with a instances of value
 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list, // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,   // in a rt_mem will be the relative rowid, in rt_disk the absolute rowid
    md2_record_t *md_record,
    json_t *jn_record // must be owned, can be null if sf_loading_from_disk
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    json_t *jn_messages = kw_get_dict(gobj, list, "messages", 0, KW_REQUIRED);
    json_t *jn_filter2 = kw_get_dict(gobj, list, "match_cond", 0, KW_REQUIRED);

    /*
     *  Search the message for this key
     */
    json_t *message = kw_get_dict_value(gobj, jn_messages, key, json_object(), KW_CREATE);
    json_t *instances = kw_get_list(gobj, message, "instances", json_array(), KW_CREATE);
    json_t *active = kw_get_dict(gobj, message, "active", json_object(), KW_CREATE);

    if(!jn_record) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "jn_record NULL",
            NULL
        );
        return -1;
    }

    /*---------------------------------*
     *  Apply filter of second level
     *---------------------------------*/
    /*
     *  Match fields
     */
    json_t *match_fields = kw_get_dict_value(
        gobj,
        jn_filter2,
        "match_fields",
        0,
        0
    );
    if(match_fields) {
        JSON_INCREF(match_fields)
        if(!kw_match_simple(jn_record, match_fields)) {
            JSON_DECREF(jn_record)
            return 0;  // Timeranger does not load the record, it's me.
        }
    }

    /*
     *  Create instance
     */
    json_t *instance = jn_record;

    /*
     *  Check active
     *  The last-loaded msg will be the active msg
     */
    BOOL is_active = TRUE;

    /*
     *  Filter by callback
     */
    trmsg_instance_callback_t trmsg_instance_callback =
        (trmsg_instance_callback_t)(size_t)kw_get_int(
        gobj,
        list,
        "trmsg_instance_callback",
        0,
        0
    );
    if(trmsg_instance_callback) {
        int ret = trmsg_instance_callback(
            tranger,
            list,
            is_active,
            instance
        );

        if(ret < 0) {
            JSON_DECREF(instance)
            return -1;  // break the load
        } else if(ret>0) {
            // continue below, add the instance
        } else { // == 0
            JSON_DECREF(instance)
            return 0;  // Timeranger does not load the record, it's me.
        }
    }

    /*
     *  max_key_instances
     */
    unsigned max_key_instances = kw_get_int(
        gobj,
        jn_filter2,
        "max_key_instances",
        0,
        KW_WILD_NUMBER
    );
    if(max_key_instances > 0) {
        if(json_array_size(instances) >= max_key_instances) {
            json_t *instance2remove = json_array_get(instances, 0);
            if(instance2remove != instance) {
                json_array_remove(instances, 0);
            } else {
                instance2remove = json_array_get(instances, 1);
                json_array_remove(instances, 1);
            }
            if(instance2remove == active) {
                json_object_set_new(message, "active", json_object());
            }
        }
    }

    /*
     *  Inserta
     */
    if(kw_get_bool(gobj, jn_filter2, "order_by_tm", 0, 0)) {
        /*
         *  Order by tm
         */
        json_int_t tm = kw_get_int(gobj, instance, "__md_tranger__`tm", 0, KW_REQUIRED);
        json_int_t last_instance = json_array_size(instances);
        json_int_t idx = last_instance;
        while(idx > 0) {
            json_t *instance_ = json_array_get(instances, idx-1);
            json_int_t tm_ = kw_get_int(gobj, instance_, "__md_tranger__`tm", 0, KW_REQUIRED);
            if(tm >= tm_) {
                break;
            }
            idx--;
        }
        json_array_insert_new(instances, idx, instance);

    } else {
        /*
         *  Order by rowid
         */
        json_array_append_new(instances, instance);
    }

    /*
     *  Set active
     */
    if(is_active) {
        json_object_set(message, "active", instance);
    }

    return 0;  // Timeranger does not load the record, it's me.
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *build_msg_index_path(
    char *bf,
    int bfsize,
    const char *topic_name,
    const char *key
)
{
    snprintf(bf, bfsize, "msgs`%s`%s", topic_name, key);
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *trmsg_open_list( // WARNING loading all records causes delay in starting applications
    json_t *tranger,
    const char *topic_name,
    json_t *match_cond, // owned
    json_t *extra       // owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "topic not found",
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return NULL;
    }
    if(!match_cond) {
        match_cond = json_object();
    }

    json_t *jn_extra = json_pack("{s:o}",
        "messages", json_object()
    );
    json_object_update_missing_new(jn_extra, extra);

    json_object_set_new(
        match_cond,
        "load_record_callback",
        json_integer((json_int_t)(size_t)load_record_callback)
    );

    /*
     *  id is required to close the list
     */
    if(!kw_has_key(match_cond, "id")) {
        const char *key = kw_get_str(gobj, match_cond, "key", "", 0);
        if(!empty_string(key)) {
            json_object_set_new(
                match_cond,
                "id",
                json_string(key)
            );
        }
    }

    json_t *rt;
    if(tranger2_open_list(
        tranger,
        topic_name,
        match_cond,  // owned
        jn_extra,    // owned
        &rt
    )<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "tranger2_open_list() failed",
            "topic_name",   "%s", topic_name,
            NULL
        );
    }

    return rt;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int trmsg_close_list(
    json_t *tranger,
    json_t *list
)
{
    return tranger2_close_list(tranger, list);
}

/***************************************************************************
 *  Return (NOT yours) dict: messages`
 ***************************************************************************/
PUBLIC json_t *trmsg_get_messages(
    json_t *list
)
{
    hgobj gobj = 0;
    return kw_get_dict(gobj, list, "messages", 0, KW_REQUIRED);
}

/***************************************************************************
 *  Return (NOT yours) dict: "active" (dict) and "instances" (list)
 ***************************************************************************/
PUBLIC json_t *trmsg_get_message(
    json_t *list,
    const char *key
)
{
    hgobj gobj = 0;
    json_t *messages = kw_get_dict(gobj, list, "messages", 0, KW_REQUIRED);
    json_t *message = kw_get_dict(gobj, messages, key, 0, 0);

    return message;
}

/***************************************************************************
 *  Return (NOT yours) dict: messages`message(key)`active
 ***************************************************************************/
PUBLIC json_t *trmsg_get_active_message(
    json_t *list,
    const char *key
)
{
    hgobj gobj = 0;
    json_t *messages = kw_get_dict(gobj, list, "messages", 0, KW_REQUIRED);
    json_t *message = kw_get_dict(gobj, messages, key, 0, 0);
    if(!message) {
        return 0;
    }
    json_t *active = kw_get_dict_value(gobj, message, "active", 0, KW_REQUIRED);
    return active;
}

/***************************************************************************
 *  Return (NOT yours) dict: messages`message(key)`active`__md_tranger__
 ***************************************************************************/
PUBLIC json_t *trmsg_get_active_md(
    json_t *list,
    const char *key
)
{
    hgobj gobj = 0;
    json_t *messages = kw_get_dict(gobj, list, "messages", 0, KW_REQUIRED);
    json_t *message = kw_get_dict(gobj, messages, key, 0, 0);
    if(!message) {
        return 0;
    }
    json_t *active = kw_get_dict_value(gobj, message, "active", 0, KW_REQUIRED);
    json_t *md = kw_get_dict(gobj, active, "__md_tranger__", 0, KW_REQUIRED);
    return md;
}

/***************************************************************************
 *  Return (NOT yours) list: messages`message(key)`instances
 ***************************************************************************/
PUBLIC json_t *trmsg_get_instances(
    json_t *list,
    const char *key
)
{
    hgobj gobj = 0;
    json_t *messages = kw_get_dict(gobj, list, "messages", 0, KW_REQUIRED);
    json_t *message = kw_get_dict(gobj, messages, key, 0, 0);
    if(!message) {
        return 0;
    }
    json_t *instances= kw_get_dict_value(gobj, message, "instances", 0, KW_REQUIRED);
    return instances;
}

/***************************************************************************
 *  Return a list of **cloned** records with instances in 'data' hook.
 *  Ready for webix use.
 *  WARNING Returned value is yours, must be decref.
 ***************************************************************************/
PUBLIC json_t *trmsg_data_tree(
    json_t *list,
    json_t *jn_filter  // owned
)
{
    hgobj gobj = 0;
    json_t *jn_records = json_array();
    json_t *messages = trmsg_get_messages(list);

    const char *key;
    json_t *message;
    json_object_foreach(messages, key, message) {
        json_t *active = kw_get_dict_value(gobj, message, "active", 0, KW_REQUIRED);
        JSON_INCREF(jn_filter);
        if(kw_match_simple(active, jn_filter)) {
            json_t *jn_active = json_incref(active);
            json_array_append_new(jn_records, jn_active);
            json_t *jn_data = kw_get_list(gobj, jn_active, "data", json_array(), KW_CREATE);
            json_t *instances = kw_get_dict_value(gobj, message, "instances", 0, KW_REQUIRED);
            json_int_t active_rowid = kw_get_int(
                gobj,
                jn_active, "__md_tranger__`rowid", 0, KW_REQUIRED
            );
            BOOL active_found = FALSE;
            int idx; json_t *instance;
            json_array_foreach(instances, idx, instance) {
                if(!active_found) {
                    json_int_t instance_rowid = kw_get_int(
                        gobj,
                        instance, "__md_tranger__`rowid", 0, KW_REQUIRED
                    );
                    if(instance_rowid == active_rowid) {
                        // Active record is already added and it's the root (with 'data' hook)
                        active_found = TRUE;
                        continue;
                    }
                }

                JSON_INCREF(jn_filter);
                if(kw_match_simple(instance, jn_filter)) {
                    json_t *jn_instance = json_incref(instance);
                    json_array_append_new(jn_data, jn_instance);
                }
            }
        }
    }

    JSON_DECREF(jn_filter)
    return jn_records;
}

/***************************************************************************
 *  Return a list of **cloned** records (list of dicts).
 *  WARNING Returned value is yours, must be decref.
 ***************************************************************************/
PUBLIC json_t *trmsg_active_records(
    json_t *list,
    json_t *jn_filter  // owned
)
{
    json_t *jn_records = json_array();
    json_t *messages = trmsg_get_messages(list);

    const char *key;
    json_t *message;
    void *n;
    json_object_foreach_safe(messages, n, key, message) {
        json_t *active = json_object_get(message, "active");
        JSON_INCREF(jn_filter);
        if(kw_match_simple(active, jn_filter)) {
            json_t *jn_active = json_incref(active);
            json_array_append_new(jn_records, jn_active);
        }
    }

    JSON_DECREF(jn_filter)
    return jn_records;
}

/***************************************************************************
 *  Return a list of **cloned** record's instances (list of dicts).
 *  WARNING Returned value is yours, must be decref.
 ***************************************************************************/
PUBLIC json_t *trmsg_record_instances(
    json_t *list,
    const char *key,
    json_t *jn_filter  // owned
)
{
    json_t *jn_records = json_array();

    json_t *instances = trmsg_get_instances(list, key);

    int idx;
    json_t *jn_value;
    json_array_foreach(instances, idx, jn_value) {
        JSON_INCREF(jn_filter);
        if(kw_match_simple(jn_value, jn_filter)) {
            json_t *jn_record = json_incref(jn_value); // Your copy
            json_array_append_new(jn_records, jn_record);
        }
    }

    JSON_DECREF(jn_filter)
    return jn_records;
}

/***************************************************************************
 *  Foreach ACTIVE **cloned** messages
 ***************************************************************************/
PUBLIC int trmsg_foreach_active_messages(
    json_t *list,
    int (*callback)( // Return < 0 break the foreach
        json_t *list,  // Not yours!
        const char *key,
        json_t *record, // It's yours, Must be owned
        void *user_data1,
        void *user_data2
    ),
    void *user_data1,
    void *user_data2,
    json_t *jn_filter  // owned
)
{
    json_t *messages = trmsg_get_messages(list);

    const char *key;
    json_t *message;
    void *n;
    json_object_foreach_safe(messages, n, key, message) {
        json_t *active = json_object_get(message, "active");
        JSON_INCREF(jn_filter);
        if(kw_match_simple(active, jn_filter)) {
            json_t *jn_active = json_incref(active); // Your copy

            if(callback(list, key, jn_active, user_data1, user_data2)<0) {
                JSON_DECREF(jn_filter)
                return -1;
            }
        }
    }

    JSON_DECREF(jn_filter)
    return 0;
}

/***************************************************************************
 *  Foreach INSTANCES **cloned** messages
 ***************************************************************************/
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
)
{
    json_t *messages = trmsg_get_messages(list);

    const char *key;
    json_t *message;
    void *n;
    json_object_foreach_safe(messages, n, key, message) {
        json_t *instances = json_object_get(message, "instances");
        json_t *jn_instances = json_array(); // Your copy

        int idx; json_t *instance;
        json_array_foreach(instances, idx, instance) {
            JSON_INCREF(jn_filter);
            if(kw_match_simple(instance, jn_filter)) {
                json_t *jn_instance = json_incref(instance);
                json_array_append_new(jn_instances, jn_instance);
            }
        }
        if(callback(list, key, jn_instances, user_data1, user_data2)<0) {
            JSON_DECREF(jn_filter)
            return -1;
        }
    }

    JSON_DECREF(jn_filter)
    return 0;
}

/***************************************************************************
 *  Foreach **duplicated** or **cloned** MESSAGES
 *  You select if the parameter 'record' in the callback is
 *  a duplicated or cloned (incref original) record.
 ***************************************************************************/
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
)
{
    json_t *messages = trmsg_get_messages(list);

    const char *key;
    json_t *message;
    void *n;
    json_object_foreach_safe(messages, n, key, message) {
        json_t *jn_message;
        JSON_INCREF(jn_filter);
        if(kw_match_simple(message, jn_filter)) {
            if(duplicated) {
                jn_message = json_deep_copy(message);
            } else {
                jn_message = json_incref(message);
            }

            if(callback(list, key, jn_message, user_data1, user_data2)<0) {
                JSON_DECREF(jn_filter)
                return -1;
            }
        }
    }

    JSON_DECREF(jn_filter)
    return 0;
}
