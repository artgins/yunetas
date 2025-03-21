/***********************************************************************
 *          C_MQIOGATE.C
 *          Mqiogate GClass.
 *
 *          Multiple Persistent Queue IOGate
 *
 *   WARNING no puede tener hijos distintos a clase C_QIOGATE
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <command_parser.h>
#include <stats_parser.h>
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
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in children"),
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
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag------------------------default---------description---------- */
SDATA (DTP_STRING,      "method",           SDF_RD,                     "lastdigits", "Method to select the child to send the message ('lastdigits', ). Default 'lastdigits', numeric value with the 'digits' last digits used to select the child. Digits can be decimal or hexadecimal ONLY, automatically detected."),
SDATA (DTP_INTEGER,     "digits",           SDF_RD|SDF_STATS,           "1",              "Digits to calculate output"),

SDATA (DTP_STRING,      "key",              SDF_RD,                     "id",           "field of kw to obtain the index to child to send message. It must be a numeric value, and the last digit is used to index the child, so you can have until 10 children with the default method."),
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
    int n_children;
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
    // WARNING no puede tener hijos distintos a clase C_QIOGATE

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
        "__gclass_name__", C_QIOGATE
    );
    json_t *dl_children = gobj_match_children(gobj, jn_filter);
    priv->n_children = json_array_size(dl_children);
    gobj_free_iter(dl_children);

    if(!priv->n_children) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONFIGURATION_ERROR,
            "msg",          "%s", "NO CHILDS of C_QIOGATE gclass",
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
    json_t *dl_children = gobj_match_children(gobj, jn_filter);

    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
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

    gobj_free_iter(dl_children);
    KW_DECREF(kw)

    return build_command_response(
        gobj,
        0,          // result
        0,          // jn_comment
        0,          // jn_schema
        jn_stats     // jn_data, owned
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
PRIVATE json_t *cmd_view_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_resp = json_array();
    /*
     *  Guarda lista de hijos con queue para las estadísticas
     */
    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", C_IOGATE
    );
    json_t *dl_children = gobj_match_children_tree(gobj, jn_filter);

    int idx; json_t *jn_child;
    json_array_foreach(dl_children, idx, jn_child) {
        hgobj child = (hgobj)(size_t)json_integer_value(jn_child);
        json_t *r = gobj_command(child, "view-channels", json_incref(kw), gobj);
        json_t *data = kw_get_dict_value(gobj, r, "data", 0, 0);
        if(data) {
            json_array_append(jn_resp, data);
        }
        json_decref(r);
    }
    gobj_free_iter(dl_children);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_resp,
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

    if(priv->n_children <= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "C_QIOGATE WITHOUT destine",
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
            idx = idx % priv->n_children;
            if(idx < 0) {
                idx = 0;
            }
            break;
    } SWITCHS_END

    gobj_dst = gobj_child_by_index(gobj, idx+1);

    if(!gobj_dst) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "C_QIOGATE destine NOT FOUND",
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
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_writing = mt_writing,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_stats = mt_stats,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_MQIOGATE);

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
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_SEND_MESSAGE,           0},
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
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        command_table,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
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
PUBLIC int register_c_mqiogate(void)
{
    return create_gclass(C_MQIOGATE);
}
