/***********************************************************************
 *          C_COUNTER.C
 *          Counter GClass.
 *          Copyright (c) 2014 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>

#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include "c_timer.h"
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
SDATA (DTP_STRING,      "info",                 SDF_RD,  "", "Info to put in output event"),
SDATA (DTP_JSON,        "jn_info",              SDF_RD,  0, "Json Info to put in output event"),
SDATA (DTP_INTEGER,     "max_count",            SDF_RD,  0, "Count to reach."),
SDATA (DTP_INTEGER,     "expiration_timeout",   SDF_RD,  0, "Timeout to finish the counter"),

SDATA (DTP_JSON,        "input_schema",         SDF_RD,  0, "json input schema. See counter.h"),

SDATA (DTP_STRING,      "final_event_name",     SDF_RD,  "EV_FINAL_COUNT", "final output event"),

SDATA (DTP_POINTER,     "user_data",            0,  0, "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,  0, "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0,  0, "subscriber of output-events. If it's null then subscriber is the parent."),
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    SET_PRIV(final_event_name,          gobj_read_str_attr)

    event_type_t *event_type = gobj_event_type_by_name(gobj, priv->final_event_name);
    if(event_type) {
        priv->final_event_name = event_type->event_name;
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "event_type not found",
            "event",        "%s", priv->final_event_name,
            NULL
        );
    }

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
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Set timer if configured it.
     */
    int expiration_timeout = (int)gobj_read_integer_attr(gobj, "expiration_timeout");
    if(expiration_timeout > 0) {
        set_timeout(priv->timer, expiration_timeout);
    } else {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "C_COUNTER without expiration_timeout",
            NULL
        );
    }

    priv->max_count = gobj_read_integer_attr(gobj, "max_count");
    if(!priv->max_count) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "max_count ZERO",
            NULL
        );
    }
    priv->jn_input_schema = gobj_read_json_attr(gobj, "input_schema");
    if(!json_is_array(priv->jn_input_schema)) {
        gobj_log_error(gobj, 0,
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

    gobj_unsubscribe_list(
        gobj,
        gobj_find_subscriptions(gobj, 0, 0, 0),
        TRUE
    );

    clear_timeout(priv->timer);
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




// /***************************************************************************
//  *
//  ***************************************************************************/
// PRIVATE BOOL match_kw(
//     hgobj gobj,
//     const char *event,
//     json_t * jn_filters,
//     json_t *event_kw)
// {
//     const char *key;
//     json_t *value;
//     json_object_foreach(jn_filters, key, value) {
//         const char *field = key;
//         json_t *jn_field = kw_get_dict_value(gobj, event_kw, field, 0, 0);
//         if(!jn_field) {
//             gobj_log_error(gobj, 0,
//                 "function",     "%s", __FUNCTION__,
//                 "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//                 "msg",          "%s", "filter field NOT in event kw",
//                 "field",        "%s", field,
//                 NULL
//             );
//             gobj_trace_json(gobj, event_kw, "filter field NOT in event kw");
//             return FALSE;
//         }
//         if(json_is_string(value)) {
//             const char *regex = json_string_value(value);
//             if(!empty_string(regex)) {
//                 /*
//                  * Must be a string field
//                  */
//                 if(!json_is_string(jn_field)) {
//                     gobj_log_error(gobj, 0,
//                         "function",     "%s", __FUNCTION__,
//                         "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//                         "msg",          "%s", "json filter is str but event kw NOT",
//                         "field",        "%s", field,
//                         NULL
//                     );
//                     gobj_trace_json(gobj, jn_filters, "jn_filters");
//                     gobj_trace_json(gobj, event_kw, "event_kw");
//                     return FALSE;
//                 }
//                 const char *value = json_string_value(jn_field);
//                 if(!empty_string(value)) {
//                     regex_t re_filter;
//                     if(regcomp(&re_filter, regex, REG_EXTENDED | REG_NOSUB)==0) {
//                         int res = regexec(&re_filter, value, 0, 0, 0);
//                         regfree(&re_filter);
//                         if(res != 0) {
//                             return FALSE;
//                         }
//                     }
//                 }
//             }
//         } else if(json_is_integer(value)) {
//             json_int_t iv = json_integer_value(value);
//             /*
//              * Must be a integer field
//              */
//             if(!json_is_integer(jn_field)) {
//                 gobj_log_error(gobj, 0,
//                     "function",     "%s", __FUNCTION__,
//                     "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//                     "msg",          "%s", "json filter is integer but event kw NOT",
//                     "field",        "%s", field,
//                     NULL
//                 );
//                 gobj_trace_json(gobj, jn_filters, "jn_filters");
//                 gobj_trace_json(gobj, event_kw, "event_kw");
//                 return FALSE;
//             }
//             json_int_t ik = json_integer_value(jn_field);
//             if(iv != ik) {
//                 return FALSE;
//             }
//         } else {
//             gobj_log_error(gobj, 0,
//                 "function",     "%s", __FUNCTION__,
//                 "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//                 "msg",          "%s", "json filter type NOT IMPLEMENTED",
//                 NULL
//             );
//             return FALSE;
//         }
//     }
//
//     return TRUE;
// }

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void publish_finalcount(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    json_t *kw = json_pack("{s:s, s:i, s:i}",
        "info", gobj_read_str_attr(gobj, "info"),
        "max_count", priv->max_count,
        "cur_count", priv->cur_count
    );

    gobj_publish_event(gobj, priv->final_event_name, kw);

    gobj_stop(gobj);
    if(gobj_is_volatil(gobj)) {
        gobj_destroy(gobj);
    }
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
    const char *event2count = kw_get_str(gobj, kw, "__original_event_name__", "", 0);
    BOOL trace = (int)gobj_trace_level(gobj) & TRACE_DEBUG;

    if(json_array_size(priv->jn_input_schema)==0) {
        gobj_log_error(gobj, 0,
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
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "list item (EvChkItem) MUST BE a json object",
                NULL
            );
            continue;
        }
        const char *EvChkItem_event = kw_get_str(gobj, jn_EvChkItem, "event", 0, 0);
        if(strcmp(EvChkItem_event, event2count)==0) {
            if(trace) {
                trace_msg0("YES match event: search:%s, received:%s ", EvChkItem_event, event2count);
            }
            // event matchs
            json_t *jn_filters = kw_get_dict(gobj,
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
                    KW_DECREF(kw);
                    // publish the output event and die!
                    publish_finalcount(gobj);
                    return 0;
                }
                KW_DECREF(kw);
                return 0;
            }
            if(trace) {
                gobj_trace_json(gobj, jn_filters, "jn_filters");
                gobj_trace_json(gobj, kw, "kw");
            }

            JSON_INCREF(jn_filters);
            if(kw_match_simple(kw, jn_filters)) {
                priv->cur_count++;
                if(trace) {
                    trace_msg0("match kw: YES count %d", priv->cur_count);
                }
                if(priv->cur_count >= priv->max_count) {
                    KW_DECREF(kw);
                    // publish the output event and die!
                    publish_finalcount(gobj);
                    return 0;
                }
            } else {
                if(trace) {
                    trace_msg0("match kw: NOT count %d", priv->cur_count);
                }
            }
        } else {
            if(trace) {
                trace_msg0("NO match event: search:%s, received:%s ", EvChkItem_event, event2count);
            }
        }
    }
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Arriving here is because the final count has not been reached.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    KW_DECREF(kw);

    // publish the output event and die!
    publish_finalcount(gobj);
    return 0;
}

/***********************************************************************
 *          FSM
 ***********************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_COUNTER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_COUNT);
GOBJ_DEFINE_EVENT(EV_FINAL_COUNT);

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
        {EV_COUNT,      ac_count,   0},
        {EV_TIMEOUT,    ac_timeout, 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_FINAL_COUNT,    EVF_OUTPUT_EVENT},
        {EV_COUNT,          0},
        {EV_TIMEOUT,        0},
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
        0,                  // lmt (no local methods table)
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,                  // authz_table
        0,                  // command_table
        s_user_trace_level,
        0                   // gcflag
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_counter(void)
{
    return create_gclass(C_COUNTER);
}
