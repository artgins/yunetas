/***********************************************************************
 *          C_MQIOGATE.C
 *          Mqiogate GClass.
 *
 *          Multiple Persistent Queue IOGate
 *
 *   WARNING no puede tener hijos distintos a clase QIOGate
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <command_parser.h>
#include "msg_ievent.h"
#include "c_iogate.h"
#include "c_qiogate.h"
#include "c_mqiogate.h"

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
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_view_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t pm_channel[] = {
/*-PM----type-----------name------------flag----------------default-----description---------- */
SDATAPM (DTP_STRING,    "channel_name", 0,                  0,          "Channel name."),
SDATAPM (DTP_BOOLEAN,   "opened",       0,                  0,          "Channel opened"),
SDATA_END()
};


PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,        cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "view-channels",    0,          pm_channel,     cmd_view_channels,"View channels."),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag------------------------default---------description---------- */
SDATA (DTP_STRING,      "method",           SDF_RD,                     "lastdigits", "Method to select the child to send the message ('lastdigits', ). Default 'lastdigits', numeric value with the 'digits' last digits used to select the child. Digits can be decimal or hexadecimal ONLY, automatically detected."),
SDATA (DTP_INTEGER,     "digits",           SDF_RD|SDF_STATS,           "1",              "Digits to calculate output"),

SDATA (DTP_STRING,      "key",              SDF_RD,                     "id",           "field of kw to obtain the index to child to send message. It must be a numeric value, and the last digit is used to index the child, so you can have until 10 childs with the default method."),
SDATA (DTP_POINTER,     "user_data",        0,                          0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                          0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                          0,              "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",                "Trace messages"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    const char *method;
    const char *key;
    unsigned digits;
    int n_childs;
} PRIVATE_DATA;

PRIVATE hgclass __gclass__ = 0;




            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    // WARNING no puede tener hijos distintos a clase QIOGate

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
    SET_PRIV(key,           gobj_read_str_attr)
    SET_PRIV(method,        gobj_read_str_attr)
    SET_PRIV(digits,        gobj_read_integer_attr)
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

    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", "QIOGate"
    );
    json_t *dl_childs = gobj_match_childs(gobj, jn_filter);
    priv->n_childs = json_array_size(dl_childs);
    gobj_free_iter(dl_childs);

    if(!priv->n_childs) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONFIGURATION_ERROR,
            "msg",          "%s", "NO CHILDS of QIOGate gclass",
            NULL
        );
    }

    if(empty_string(priv->key)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONFIGURATION_ERROR,
            "msg",          "%s", "key EMPTY",
            "method",       "%s", priv->method,
            "key",          "%s", priv->key,
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
    return 0;
}

/***************************************************************************
 *      Framework Method stats
 ***************************************************************************/
PRIVATE json_t *mt_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    json_t *jn_stats = json_object();

    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", C_QIOGATE
    );
    json_t *dl_childs = gobj_match_childs(gobj, jn_filter);

    int idx; json_t *jn_child;
    json_array_foreach(dl_childs, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        KW_INCREF(kw)
        json_t *jn_stats_child = build_stats(
            child,
            stats,
            kw,     // owned
            src
        );
        json_object_set_new(jn_stats, gobj_name(child), jn_stats_child);
    }

    gobj_free_iter(dl_childs);

    KW_DECREF(kw)
    return jn_stats;
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
        "",
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_view_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_resp = json_array();
    /*
     *  Guarda lista de hijos con queue para las estadísticas
     */
    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", C_IOGATE
    );
    json_t *dl_childs = gobj_match_childs_tree(gobj, jn_filter);

    int idx; json_t *jn_child;
    json_array_foreach(dl_childs, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *r = gobj_command(child, "view-channels", json_incref(kw), gobj);
        json_t *data = kw_get_dict_value(gobj, r, "data", 0, 0);
        if(data) {
            json_array_append(jn_resp, data);
        }
        json_decref(r);
    }
    gobj_free_iter(dl_childs);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_resp,
        "",
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj gobj_dst=0;

    const char *id = "";
    int idx = 0;

    if(priv->n_childs <= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "QIOGate WITHOUT destine",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    SWITCHS(priv->method) {
        CASES("lastdigits")
        DEFAULTS
            int digits = priv->digits;
            id = kw_get_str(gobj, kw, priv->key, "", KW_REQUIRED);
            int len = strlen(id);
            if(len > 0) {
                if(digits > len) {
                    digits = len;
                }
                char *s = (char *)id + len - digits;
                if(all_numbers(s)) {
                    idx = atoi(s);
                } else {
                    idx = (int)strtol(s, NULL, 16);
                }
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_CONFIGURATION_ERROR,
                    "msg",          "%s", "key value WITHOUT LENGTH",
                    "method",       "%s", priv->method,
                    "key",          "%s", priv->key,
                    NULL
                );
            }
            idx = idx % priv->n_childs;
            if(idx < 0) {
                idx = 0;
            }
            break;
    } SWITCHS_END

    gobj_child_by_index(gobj, idx+1, &gobj_dst);

    if(!gobj_dst) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "QIOGate destine NOT FOUND",
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "MQIOGATE %s (idx %d, value '%s') ==> %s", priv->key, idx, id, gobj_short_name(gobj_dst));
    }

    return gobj_send_event(gobj_dst, event, kw, gobj);
}


/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    // top input
    {EV_SEND_MESSAGE, 0,   0,  ""},
    {NULL, 0, 0, 0}
};
PRIVATE const EVENT output_events[] = {
    {NULL, 0, 0, 0}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {EV_SEND_MESSAGE,         ac_send_message,        0},
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
    GCLASS_MQIOGATE_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        0, //mt_play,
        0, //mt_pause,
        mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        mt_stats,
        0, //mt_command_parser,
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
    command_table,  // command_table
    0,  // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_mqiogate(void)
{
    return &_gclass;
}
