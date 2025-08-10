/***********************************************************************
 *          C_COUNTER.C
 *          Counter GClass.
 *          Copyright (c) 2014 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <regex.h>
#include "c_counter.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
SDATA (ASN_OCTET_STR,   "info",                 SDF_RD,  "", "Info to put in output event"),
SDATA (ASN_JSON,        "jn_info",              SDF_RD,  0, "Json Info to put in output event"),
SDATA (ASN_UNSIGNED,    "max_count",            SDF_RD,  0, "Count to reach."),
SDATA (ASN_INTEGER,     "expiration_timeout",   SDF_RD,  0, "Timeout to finish the counter"),

SDATA (ASN_JSON,        "input_schema",         SDF_RD,  0, "json input schema. See counter.h"),

SDATA (ASN_OCTET_STR,   "final_event_name",     SDF_RD,  "EV_FINAL_COUNT", "final output event"),

SDATA (ASN_POINTER,     "user_data",            0,  0, "user data"),
SDATA (ASN_POINTER,     "user_data2",           0,  0, "more user data"),
SDATA (ASN_POINTER,     "subscriber",           0,  0, "subscriber of output-events. If it's null then subscriber is the parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_DEBUG = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"debug",        "Trace to debug"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;

    uint32_t max_count;
    json_t *jn_input_schema;

    const char *final_event_name;

    uint32_t cur_count;

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

    priv->timer = gobj_create("", GCLASS_TIMER, 0, gobj);

    SET_PRIV(final_event_name,          gobj_read_str_attr)

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Set timer if configured it.
     */
    int expiration_timeout = gobj_read_int32_attr(gobj, "expiration_timeout");
    gobj_start(priv->timer);
    if(expiration_timeout > 0) {
        set_timeout(priv->timer, expiration_timeout);
    }

    priv->max_count = gobj_read_uint32_attr(gobj, "max_count");
    if(!priv->max_count) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "max_count ZERO",
            NULL
        );
    }
    priv->jn_input_schema = gobj_read_json_attr(gobj, "input_schema");
    if(!json_is_array(priv->jn_input_schema)) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "input_schema MUST BE a json array",
            NULL
        );
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_unsubscribe_list(gobj_find_subscriptions(gobj, 0, 0, 0), TRUE, FALSE);

    clear_timeout(priv->timer);
    if(gobj_is_running(priv->timer))
        gobj_stop(priv->timer);
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
BOOL match_kw(
    hgobj gobj,
    const char *event,
    json_t * jn_filters,
    json_t *event_kw)
{
    const char *key;
    json_t *value;
    json_object_foreach(jn_filters, key, value) {
        const char *field = key;
        json_t *jn_field = kw_get_dict_value(event_kw, field, 0, 0);
        if(!jn_field) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "filter field NOT in event kw",
                "field",        "%s", field,
                NULL
            );
            log_debug_json(0, event_kw, "filter field NOT in event kw");
            return FALSE;
        }
        if(json_is_string(value)) {
            const char *regex = json_string_value(value);
            if(!empty_string(regex)) {
                /*
                 * Must be a string field
                 */
                if(!json_is_string(jn_field)) {
                    log_error(0,
                        "gobj",         "%s", gobj_full_name(gobj),
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "json filter is str but event kw NOT",
                        "field",        "%s", field,
                        NULL
                    );
                    log_debug_json(0, jn_filters, "jn_filters");
                    log_debug_json(0, event_kw, "event_kw");
                    return FALSE;
                }
                const char *value = json_string_value(jn_field);
                if(!empty_string(value)) {
                    regex_t re_filter;
                    if(regcomp(&re_filter, regex, REG_EXTENDED | REG_NOSUB)==0) {
                        int res = regexec(&re_filter, value, 0, 0, 0);
                        regfree(&re_filter);
                        if(res != 0) {
                            return FALSE;
                        }
                    }
                }
            }
        } else if(json_is_integer(value)) {
            json_int_t iv = json_integer_value(value);
            /*
             * Must be a integer field
             */
            if(!json_is_integer(jn_field)) {
                log_error(0,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "json filter is integer but event kw NOT",
                    "field",        "%s", field,
                    NULL
                );
                log_debug_json(0, jn_filters, "jn_filters");
                log_debug_json(0, event_kw, "event_kw");
                return FALSE;
            }
            json_int_t ik = json_integer_value(jn_field);
            if(iv != ik) {
                return FALSE;
            }
        } else {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "json filter type NOT IMPLEMENTED",
                NULL
            );
            return FALSE;
        }
    }

    return TRUE;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void publish_finalcount(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = json_pack("{s:s, s:i, s:i}",
        "info", gobj_read_str_attr(gobj, "info"),
        "max_count", priv->max_count,
        "cur_count", priv->cur_count
    );

    gobj_publish_event(gobj, priv->final_event_name, kw);
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Count an event
 ***************************************************************************/
PRIVATE int ac_count(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *event2count = kw_get_str(kw, "__original_event_name__", "", 0);
    BOOL trace = gobj_trace_level(gobj) & TRACE_DEBUG;

    if(json_array_size(priv->jn_input_schema)==0) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "filter list EMPTY",
            NULL
        );
    }
    size_t index;
    json_t *value;
    json_array_foreach(priv->jn_input_schema, index, value) {
        json_t *jn_EvChkItem =  value;
        if(!json_is_object(jn_EvChkItem)) {
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "list item (EvChkItem) MUST BE a json object",
                NULL
            );
            continue;
        }
        const char *EvChkItem_event = kw_get_str(jn_EvChkItem, "event", 0, 0);
        if(strcmp(EvChkItem_event, event2count)==0) {
            if(trace) {
                log_debug_printf("YES match event", "search:%s, recvd:%s ", EvChkItem_event, event2count);
            }
            // event matchs
            json_t *jn_filters = kw_get_dict(
                jn_EvChkItem,
                "filters",
                0,
                0
            );
            if(!jn_filters) {
                /*
                 *  No filter: accept TRUE.
                 */
                priv->cur_count++;
                if(priv->cur_count >= priv->max_count) {
                    // publish the output event and die!
                    publish_finalcount(gobj);
                    clear_timeout(priv->timer);
                    gobj_stop(priv->timer);
                    JSON_DECREF(kw);
                    return 0;
                }
                JSON_DECREF(kw);
                return 0;
            }
            if(trace) {
                log_debug_json(0, jn_filters, "jn_filters");
                log_debug_json(0, kw, "kw");
            }

            //if(match_kw(gobj, event2count, jn_filters, kw)) {
            // TODO quito match_kw, al incorporar realm_id que tiene ^ el match falla
            // por usar expresión regular. Pongo kw_match_simple V4.0.0
            JSON_INCREF(jn_filters);
            if(kw_match_simple(kw, jn_filters)) {
                priv->cur_count++;
                if(trace) {
                    log_debug_printf("match kw!", "count %d", priv->cur_count);
                }
                if(priv->cur_count >= priv->max_count) {
                    // publish the output event and die!
                    publish_finalcount(gobj);
                    clear_timeout(priv->timer);
                    gobj_stop(priv->timer);
                    JSON_DECREF(kw);
                    return 0;
                }
            }
        } else {
            if(trace) {
                log_debug_printf("NO match event", "search:%s, recvd:%s ", EvChkItem_event, event2count);
            }
        }
    }
    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Arriving here is because the final count has not been reached.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);


    // publish the output event and die!
    publish_finalcount(gobj);
    clear_timeout(priv->timer);
    gobj_stop(priv->timer);

    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  If die timer, die self
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_stop(gobj);
    gobj_destroy(gobj);
    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    {"EV_COUNT",        0},
    {"EV_TIMEOUT",      0},
    {"EV_STOPPED",      0},
    {NULL, 0}
};
PRIVATE const EVENT output_events[] = {
    {"EV_FINAL_COUNT",  0},
    {NULL, 0}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {"EV_COUNT",        ac_count,       0},
    {"EV_TIMEOUT",      ac_timeout,     0},
    {"EV_STOPPED",      ac_stopped,     0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_IDLE,
    NULL
};

PRIVATE FSM fsm = {
    input_events,
    output_events,
    state_names,
    states,
};

/***************************************************************************
 *              GClass
 ***************************************************************************/
/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_COUNTER_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        0, //mt_destroy,
        mt_start,
        mt_stop,
        0, //mt_play,
        0, //mt_pause,
        0, //mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        0, //mt_stats,
        0, //mt_command,
        0, //mt_inject_event,
        0, //mt_create_resource,
        0, //mt_list_resource,
        0, //mt_save_resource,
        0, //mt_delete_resource,
        0, //mt_future21
        0, //mt_future22
        0, //mt_get_resource
        0, //mt_state_changed,
        0, //mt_authenticate,
        0, //mt_list_childs,
        0, //mt_stats_updated,
        0, //mt_disable,
        0, //mt_enable,
        0, //mt_trace_on,
        0, //mt_trace_off,
        0, //mt_gobj_created,
        0, //mt_future33,
        0, //mt_future34,
        0, //mt_publish_event,
        0, //mt_publication_pre_filter,
        0, //mt_publication_filter,
        0, //mt_authz_checker,
        0, //mt_future39,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_nodes,
        0, //mt_future44,
        0, //mt_unlink_nodes,
        0, //mt_topic_jtree,
        0, //mt_get_node,
        0, //mt_list_nodes,
        0, //mt_shoot_snap,
        0, //mt_activate_snap,
        0, //mt_list_snaps,
        0, //mt_treedbs,
        0, //mt_treedb_topics,
        0, //mt_topic_desc,
        0, //mt_topic_links,
        0, //mt_topic_hooks,
        0, //mt_node_parents,
        0, //mt_node_childs,
        0, //mt_list_instances,
        0, //mt_node_tree,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    0,  // acl
    s_user_trace_level,
    0, // cmds
    0, // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_counter(void)
{
    return &_gclass;
}
