/***********************************************************************
 *          C_TRANGER.C
 *          Tranger GClass.
 *
 *          Trangers: resources with tranger

To test with CLI:

command-yuno id=1911 service=tranger command=open-list list_id=pepe topic_name=pp fields=id,__md_tranger__

command-yuno id=1911 service=__yuno__ command=subscribe-event gobj_name=tranger event_name=EV_TRANGER_RECORD_ADDED rename_event_name=EV_MT_COMMAND_ANSWER first_shot=1 config={"__list_id__":"pepe"}

command-yuno id=1911 service=__yuno__ command=subscribe-event gobj_name=tranger event_name=EV_TRANGER_RECORD_ADDED rename_event_name=EV_MT_COMMAND_ANSWER  config={"__list_id__":"pepe"}

command-yuno id=1911 service=tranger command=add-record topic_name=pp record='{"id":"1","tm":0}'

command-yuno id=1911 service=tranger command=close-list list_id=pepe

command-yuno id=1911 service=tranger command=open-list list_id=pepe topic_name=pp only_md=1
command-yuno id=1911 service=tranger command=get-list-data list_id=pepe
command-yuno id=1911 service=tranger command=add-record topic_name=pp record='{"id":"1","tm":0}'
command-yuno id=1911 service=tranger command=close-list list_id=pepe

command-yuno id=1911 service=tranger command=open-list list_id=pepe topic_name=pp
command-yuno id=1911 service=tranger command=get-list-data list_id=pepe
command-yuno id=1911 service=tranger command=add-record topic_name=pp record='{"id":"1","tm":0}'
command-yuno id=1911 service=tranger command=close-list list_id=pepe


 *          Copyright (c) 2020 Niyamaka, 2024- ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <kwid.h>
#include <helpers.h>
#include <command_parser.h>
#include <timeranger2.h>

#include "msg_ievent.h"
#include "c_tranger.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md2_record_t *md_record,
    json_t *jn_record
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_print_tranger(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_check_json(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_topics(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_desc(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_create_topic(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_topic(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_open_list(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_close_list(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_add_record(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_list_data(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,  "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_print_tranger[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"), // TODO pon database
SDATAPM (DTP_BOOLEAN,   "expanded",     0,              0,          "No expanded (default) return [[size]]"),
SDATAPM (DTP_INTEGER,   "lists_limit",  0,              0,          "Expand lists only if size < limit. 0 no limit"),
SDATAPM (DTP_INTEGER,   "dicts_limit",  0,              0,          "Expand dicts only if size < limit. 0 no limit"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_check_json[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER,   "max_refcount",   0,              0,          "Maximum refcount"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_desc[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_create_topic[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_STRING,    "pkey",         0,              "id",       "Primary Key"),
SDATAPM (DTP_STRING,    "tkey",         0,              "tm",       "Time Key"),
SDATAPM (DTP_STRING,    "system_flag",  0,              "sf_string_key|sf_t_ms", "System flag: sf2_string_key|sf2_int_key|sf2_t_ms|sf2_tm_ms|sf2_no_record_disk, future: sf2_zip_record|sf2_cipher_record"),
SDATAPM (DTP_JSON,      "jn_cols",      0,              0,          "Cols"),
SDATAPM (DTP_JSON,      "jn_var",       0,              0,          "Var"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_delete_topic[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_BOOLEAN,   "force",        0,              0,          "Force delete"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_add_record[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_INTEGER,   "__t__",        0,              0,          "Time of record"),
SDATAPM (DTP_INTEGER,   "user_flag",    0,              0,          "User flag of record"),
SDATAPM (DTP_JSON,      "record",       0,              0,          "Record json"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_open_list[] = {
/*-PM----type-----------name--------------------flag--------default-description---------- */
SDATAPM (DTP_STRING,    "list_id",              0,          0,      "Id of list"),
SDATAPM (DTP_STRING,    "topic_name",           0,          0,      "Topic name"),
SDATAPM (DTP_STRING,    "key",                  0,          0,      "match_cond:"),
SDATAPM (DTP_STRING,    "notkey",               0,          0,      "match_cond:"),
SDATAPM (DTP_STRING,    "from_tm",              0,          0,      "match_cond:"),
SDATAPM (DTP_STRING,    "to_tm",                0,          0,      "match_cond:"),
SDATAPM (DTP_INTEGER,   "from_rowid",           0,          0,      "match_cond:"),
SDATAPM (DTP_INTEGER,   "to_rowid",             0,          0,      "match_cond:"),
SDATAPM (DTP_STRING,    "from_t",               0,          0,      "match_cond:"),
SDATAPM (DTP_STRING,    "to_t",                 0,          0,      "match_cond:"),
SDATAPM (DTP_STRING,    "fields",               0,          0,      "match_cond: Only this fields"),
SDATAPM (DTP_STRING,    "rkey",                 0,          0,      "match_cond: regular expression of key"),
SDATAPM (DTP_BOOLEAN,   "return_data",          0,          0,      "True for return list data"),
SDATAPM (DTP_BOOLEAN,   "backward",             0,          0,      "match_cond:"),
SDATAPM (DTP_BOOLEAN,   "only_md",              0,          0,      "match_cond: don't load jn_record on loading disk"),
SDATAPM (DTP_INTEGER,   "user_flag",            0,          0,      "match_cond:"),
SDATAPM (DTP_INTEGER,   "not_user_flag",        0,          0,      "match_cond:"),
SDATAPM (DTP_INTEGER,   "user_flag_mask_set",   0,          0,      "match_cond:"),
SDATAPM (DTP_INTEGER,   "user_flag_mask_notset",0,          0,      "match_cond:"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_close_list[] = {
/*-PM----type-----------name--------------------flag----default-description---------- */
SDATAPM (DTP_STRING,    "list_id",              0,      0,      "Id of list"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_get_list_data[] = {
/*-PM----type-----------name--------------------flag----default-description---------- */
SDATAPM (DTP_STRING,    "list_id",              0,      0,      "Id of list"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-------json_fn---------description--*/
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,    cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "authzs",           0,          pm_authzs,  cmd_authzs,     "Authorization's help"),

/*-CMD2---type----------name----------------flag------------alias---items---------------json_fn-------------description--*/
SDATACM2 (DTP_SCHEMA,   "print-tranger",    SDF_AUTHZ_X,    0,      pm_print_tranger,   cmd_print_tranger,  "Print tranger"),
SDATACM2 (DTP_SCHEMA,   "check-json",       0,              0,      pm_check_json,      cmd_check_json, "Check json refcounts"),
SDATACM2 (DTP_SCHEMA,   "topics",           SDF_AUTHZ_X,    0,      0,                  cmd_topics,         "List topics"),
SDATACM2 (DTP_SCHEMA,   "desc",             SDF_AUTHZ_X,    0,      pm_desc,            cmd_desc,           "Schema of topic or full"),
SDATACM2 (DTP_SCHEMA,   "create-topic",     SDF_AUTHZ_X,    0,      pm_create_topic,    cmd_create_topic,   "Create topic"),
SDATACM2 (DTP_SCHEMA,   "delete-topic",     SDF_AUTHZ_X,    0,      pm_delete_topic,    cmd_delete_topic,   "Delete topic"),
SDATACM2 (DTP_SCHEMA,   "open-list",        SDF_AUTHZ_X,    0,      pm_open_list,       cmd_open_list,      "Open list"),
SDATACM2 (DTP_SCHEMA,   "close-list",       SDF_AUTHZ_X,    0,      pm_close_list,      cmd_close_list,     "Close list"),
SDATACM2 (DTP_SCHEMA,   "add-record",       SDF_AUTHZ_X,    0,      pm_add_record,      cmd_add_record,     "Add record"),
SDATACM2 (DTP_SCHEMA,   "get-list-data",    SDF_AUTHZ_X,    0,      pm_get_list_data,   cmd_get_list_data,  "Get list data"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (DTP_POINTER,     "tranger",          0,                  0,              "Tranger handler"),
SDATA (DTP_STRING,      "path",             SDF_RD|SDF_REQUIRED,"", "Path of database"),
SDATA (DTP_STRING,      "database",         SDF_RD|SDF_REQUIRED,"", "Database name"),
SDATA (DTP_STRING,      "filename_mask",    SDF_RD|SDF_REQUIRED,"%Y-%m-%d",    "Organization of tables (file name format, see strftime())"),

SDATA (DTP_INTEGER,     "xpermission",      SDF_RD,             "02770",        "Use in creation, default 02770"),
SDATA (DTP_INTEGER,     "rpermission",      SDF_RD,             "0660",         "Use in creation, default 0660"),
SDATA (DTP_INTEGER,     "on_critical_error",SDF_RD,             "2",            "exit on error (Zero to avoid restart)"),
SDATA (DTP_BOOLEAN,     "master",           SDF_RD,             FALSE,          "the master is the only that can write"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,              "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendent value!
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
PRIVATE sdata_desc_t pm_authz_create[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATAPM0 (DTP_STRING,       "topic_name",       0,          "",             "Topic name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authz_write[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATAPM0 (DTP_STRING,       "topic_name",       0,          "",             "Topic name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authz_read[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATAPM0 (DTP_STRING,       "topic_name",       0,          "",             "Topic name"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authz_delete[] = {
/*-PM-----type--------------name----------------flag--------authpath--------description-- */
SDATAPM0 (DTP_STRING,       "topic_name",       0,          "",             "Topic name"),
SDATA_END()
};

PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name------------flag----alias---items---------------description--*/
SDATAAUTHZ (DTP_SCHEMA, "create",       0,      0,      pm_authz_create,    "Permission to create topics"),
SDATAAUTHZ (DTP_SCHEMA, "write",        0,      0,      pm_authz_write,     "Permission to write topics"),
SDATAAUTHZ (DTP_SCHEMA, "read",         0,      0,      pm_authz_read,      "Permission to read topics"),
SDATAAUTHZ (DTP_SCHEMA, "delete",       0,      0,      pm_authz_delete,    "Permission to delete topics"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    json_t *tranger;

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

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  HACK low level service: tranger must be here in create method instead of mt_start.
     */
    json_t *jn_tranger = json_pack("{s:s, s:s, s:s, s:i, s:i, s:i, s:b}",
        "path", gobj_read_str_attr(gobj, "path"),
        "database", gobj_read_str_attr(gobj, "database"),
        "filename_mask", gobj_read_str_attr(gobj, "filename_mask"),
        "xpermission", (int)gobj_read_integer_attr(gobj, "xpermission"),
        "rpermission", (int)gobj_read_integer_attr(gobj, "rpermission"),
        "on_critical_error", (int)gobj_read_integer_attr(gobj, "on_critical_error"),
        "master", gobj_read_bool_attr(gobj, "master")
    );

    priv->tranger = tranger2_startup(
        jn_tranger // owned
    );
    gobj_write_pointer_attr(gobj, "tranger", priv->tranger);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(tranger2_shutdown, priv->tranger);
    gobj_write_pointer_attr(gobj, "tranger", 0);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
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
 *      Framework Method subscription_added
 ***************************************************************************/
PRIVATE int mt_subscription_added(
    hgobj gobj,
    json_t *subs)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *__config__ = kw_get_dict(gobj, subs, "__config__", 0, 0);
    BOOL first_shot = kw_get_bool(gobj, __config__, "__first_shot__", FALSE, 0);
    if(!first_shot) {
        return 0;
    }
    hgobj subscriber = (hgobj)kw_get_int(gobj, subs, "subscriber", 0, KW_REQUIRED);
    const char *event = kw_get_str(gobj, subs, "event", "", KW_REQUIRED);

    if(empty_string(event)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "tranger subscription must be with explicit event",
            "subscriber",   "%s", gobj_full_name(subscriber),
            NULL
        );
        return 0; // return -1 unsubscribe this subs
    }

    json_t *__global__ = kw_get_dict(gobj, subs, "__global__", 0, 0);
    json_t *__filter__ = kw_get_dict(gobj, subs, "__filter__", 0, 0);

    const char *event_name = kw_get_str(gobj, subs, "renamed_event", 0, 0);

    const char *__list_id__ = kw_get_str(gobj, __config__, "__list_id__", "", 0);
    if(empty_string(__list_id__)) {
        return gobj_send_event(
            subscriber,
            (!empty_string(event_name))?event_name:event,
            msg_iev_build_response_without_reverse_dst(gobj,
                -1,
                json_sprintf("__list_id__ required"),
                0,
                0, // owned
                __global__?kw_duplicate(gobj, __global__):0  // owned
            ),
            gobj
        );
    }

    // TODO review, old tranger_get_list
    json_t *list = tranger2_get_iterator_by_id(priv->tranger, __list_id__);
    if(!list) {
        return gobj_send_event(
            subscriber,
            (!empty_string(event_name))?event_name:event,
            msg_iev_build_response_without_reverse_dst(gobj,
                -1,
                json_sprintf("tranger list not found: '%s'", __list_id__),
                0,
                0, // owned
                __global__?kw_duplicate(gobj, __global__):0  // owned
            ),
            gobj
        );
    }

    if(strcasecmp(event, "EV_TRANGER_RECORD_ADDED")==0) {
        json_t *jn_data = json_array();
        size_t idx;
        json_t *jn_record;
        json_array_foreach(kw_get_list(gobj, list, "data", 0, KW_REQUIRED), idx, jn_record) {
            JSON_INCREF(__filter__)
            if(!kw_match_simple(jn_record, __filter__)) {
                continue;
            }
            json_array_append(jn_data, jn_record);
        }

        /*
         *  Inform
         */
        return gobj_send_event(
            subscriber,
            (!empty_string(event_name))?event_name:event,
            msg_iev_build_response_without_reverse_dst(gobj,
                0,
                0,
                0,
                jn_data, // owned
                __global__?kw_duplicate(gobj, __global__):0  // owned
            ),
            gobj
        );

    }

    return 0; // return -1 unsubscribe this subs
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

    return tranger2_topic_key_size(priv->tranger, topic_name, key);
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
//    return gobj_build_authzs_doc(gobj, cmd, kw, src);
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
PRIVATE json_t *cmd_print_tranger(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        KW_DECREF(kw);
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    BOOL expanded = kw_get_bool(gobj, kw, "expanded", 0, KW_WILD_NUMBER);
    int lists_limit = (int)kw_get_int(gobj, kw, "lists_limit", 100, KW_WILD_NUMBER);
    int dicts_limit = (int)kw_get_int(gobj, kw, "dicts_limit", 100, KW_WILD_NUMBER);
    const char *path = kw_get_str(gobj, kw, "path", "", 0);

    json_t *value = priv->tranger;

    if(!empty_string(path)) {
        value = kw_find_path(gobj, value, path, FALSE);
        if(!value) {
            return msg_iev_build_response(gobj,
                -1,
                json_sprintf("Path not found: '%s'", path),
                0,
                0,
                kw  // owned
            );
        }
    }

    if(expanded) {
        if(!lists_limit && !dicts_limit) {
            kw_incref(value); // All
        } else {
            value = kw_collapse(gobj, value, lists_limit, dicts_limit);
        }
    } else {
        value = kw_collapse(gobj, value, 0, 0);
    }

    /*
     *  Inform
     */
    return msg_iev_build_response(gobj,
        0,
        0,
        0,
        value,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_check_json(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int max_refcount = kw_get_int(gobj, kw, "max_refcount", 1, KW_WILD_NUMBER);

    json_t *tranger = priv->tranger;
    int result = 0;
    json_check_refcounts(tranger, max_refcount, &result)?0:-1;
    return msg_iev_build_response(gobj,
        result,
        json_sprintf("check refcounts of tranger: %s", result==0?"Ok":"Bad"),
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "create";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        KW_DECREF(kw);
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

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

    const char *pkey = kw_get_str(gobj, kw, "pkey", "", 0);
    const char *tkey = kw_get_str(gobj, kw, "tkey", "", 0);
    const char *system_flag_ = kw_get_str(gobj, kw, "system_flag", "", 0);
    json_t *jn_cols = kw_get_dict(gobj, kw, "jn_cols", 0, 0);
    json_t *jn_var = kw_get_dict(gobj, kw, "jn_var", 0, 0);

    system_flag2_t system_flag = tranger2_str2system_flag(system_flag_);

    json_t *topic = tranger2_create_topic( // WARNING returned json IS NOT YOURS, HACK IDEMPOTENT function
        priv->tranger,      // If topic exists then only needs (tranger, topic_name) parameters
        topic_name,
        pkey,
        tkey,
        NULL,
        system_flag,
        json_incref(jn_cols),    // owned
        json_incref(jn_var)      // owned
    );

    return msg_iev_build_response(gobj,
        topic?0:-1,
        topic?json_sprintf("Topic created: '%s'", topic_name):json_string(gobj_log_last_message()),
        0,
        json_incref(topic),
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_delete_topic(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "delete";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        KW_DECREF(kw);
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

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

    BOOL force = kw_get_bool(gobj, kw, "force", 0, 0);
    json_t *topic = tranger2_topic(priv->tranger, topic_name);
    if(!topic) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Topic not found: '%s'", topic_name),
            0,
            0,
            kw  // owned
        );
    }

    json_int_t topic_size = tranger2_topic_size(topic);
    if(topic_size != 0) {
        if(!force) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("'%s' topic with records, you must force to delete", topic_name),
                0,
                0,
                kw  // owned
            );
        }
    }

    int ret = tranger2_delete_topic(priv->tranger, topic_name);

    return msg_iev_build_response(gobj,
        ret,
        ret>=0?json_sprintf("Topic deleted: '%s'", topic_name):json_string(gobj_log_last_message()),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_topics(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        KW_DECREF(kw);
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    json_t *topics = kw_get_dict(priv->tranger, "topics", 0, KW_REQUIRED);
    json_t *topic_list = json_array();

    const char *topic_name; json_t *topic;
    json_object_foreach(topics, topic_name, topic) {
        if(!json_is_object(topic)) {
            continue;
        }
        json_array_append_new(topic_list, json_string(topic_name));
    }

    return msg_iev_build_response(gobj,
        topics?0:-1,
        topics?0:json_string(gobj_log_last_message()),
        0,
        topic_list,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_desc(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        KW_DECREF(kw);
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

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

    json_t *topic = tranger2_topic(priv->tranger, topic_name);
    if(!topic) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Topic not found: '%s'", topic_name),
            0,
            0,
            kw  // owned
        );
    }
    json_t *desc = kwid_new_dict("", priv->tranger, "topics`%s`cols", topic_name);

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
PRIVATE json_t *cmd_open_list(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        KW_DECREF(kw);
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    const char *list_id = kw_get_str(gobj, kw, "list_id", "", 0);
    BOOL return_data = kw_get_bool(kw, "return_data", 0, 0);

    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);

    if(empty_string(list_id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What list_id?"),
            0,
            0,
            kw  // owned
        );
    }

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
    json_t *topic = tranger2_topic(priv->tranger, topic_name);
    if(!topic) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Topic not found: '%s'", topic_name),
            0,
            0,
            kw  // owned
        );
    }

    json_t *list = tranger_get_list(priv->tranger, list_id);
    if(list) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("List is already open: '%s'", list_id),
            0,
            0,
            kw  // owned
        );
    }

    BOOL  backward = kw_get_bool(kw, "backward", 0, 0);
    BOOL  only_md = kw_get_bool(kw, "only_md", 0, 0);
    int64_t from_rowid = (int64_t)kw_get_int(gobj, kw, "from_rowid", 0, 0);
    int64_t to_rowid = (int64_t)kw_get_int(gobj, kw, "to_rowid", 0, 0);
    uint32_t user_flag = (uint32_t)kw_get_int(gobj, kw, "user_flag", 0, 0);
    uint32_t not_user_flag = (uint32_t)kw_get_int(gobj, kw, "not_user_flag", 0, 0);
    uint32_t user_flag_mask_set = (uint32_t)kw_get_int(gobj, kw, "user_flag_mask_set", 0, 0);
    uint32_t user_flag_mask_notset = (uint32_t)kw_get_int(gobj, kw, "user_flag_mask_notset", 0, 0);
    const char *key = kw_get_str(gobj, kw, "key", 0, 0);
    const char *notkey = kw_get_str(gobj, kw, "notkey", 0, 0);
    const char *rkey = kw_get_str(gobj, kw, "rkey", 0, 0);
    const char *from_t = kw_get_str(gobj, kw, "from_t", 0, 0);
    const char *to_t = kw_get_str(gobj, kw, "to_t", 0, 0);
    const char *from_tm = kw_get_str(gobj, kw, "from_tm", 0, 0);
    const char *to_tm = kw_get_str(gobj, kw, "to_tm", 0, 0);
    const char *fields = kw_get_str(gobj, kw, "fields", 0, 0);

    json_t *match_cond = json_pack("{s:b, s:b}",
        "backward", backward,
        "only_md", only_md
    );
    if(from_rowid) {
        json_object_set_new(match_cond, "from_rowid", json_integer(from_rowid));
    }
    if(to_rowid) {
        json_object_set_new(match_cond, "to_rowid", json_integer(to_rowid));
    }
    if(user_flag) {
        json_object_set_new(match_cond, "user_flag", json_integer(user_flag));
    }
    if(not_user_flag) {
        json_object_set_new(match_cond, "not_user_flag", json_integer(not_user_flag));
    }
    if(user_flag_mask_set) {
        json_object_set_new(match_cond, "user_flag_mask_set", json_integer(user_flag_mask_set));
    }
    if(user_flag_mask_notset) {
        json_object_set_new(match_cond, "user_flag_mask_notset", json_integer(user_flag_mask_notset));
    }
    if(key) {
        json_object_set_new(match_cond, "key", json_string(key));
    }
    if(notkey) {
        json_object_set_new(match_cond, "notkey", json_string(notkey));
    }
    if(rkey) {
        json_object_set_new(match_cond, "rkey", json_string(rkey));
    }
    if(from_t) {
        timestamp_t timestamp;
        int offset;
        if(all_numbers(from_t)) {
            timestamp = atoll(from_t);
        } else {
            parse_date_basic(from_t, &timestamp, &offset);
        }
        json_object_set_new(match_cond, "from_t", json_integer(timestamp));
    }
    if(to_t) {
        timestamp_t timestamp;
        int offset;
        if(all_numbers(to_t)) {
            timestamp = atoll(to_t);
        } else {
            parse_date_basic(to_t, &timestamp, &offset);
        }
        json_object_set_new(match_cond, "to_t", json_integer(timestamp));
    }
    if(from_tm) {
        timestamp_t timestamp;
        int offset;
        if(all_numbers(from_tm)) {
            timestamp = atoll(from_tm);
        } else {
            parse_date_basic(from_tm, &timestamp, &offset);
        }
        json_object_set_new(match_cond, "from_tm", json_integer(timestamp));
    }
    if(to_tm) {
        timestamp_t timestamp;
        int offset;
        if(all_numbers(to_tm)) {
            timestamp = atoll(to_tm);
        } else {
            parse_date_basic(to_tm, &timestamp, &offset);
        }
        json_object_set_new(match_cond, "to_tm", json_integer(timestamp));
    }
    if(!empty_string(fields)) {
        json_object_set_new(
            match_cond,
            "fields",
            json_string(fields)
        );
    }

    json_t *jn_list = json_pack("{s:s, s:s, s:o, s:I, s:I}",
        "id", list_id,
        "topic_name", topic_name,
        "match_cond", match_cond,
        "load_record_callback", (json_int_t)(size_t)load_record_callback,
        "gobj", (json_int_t)(size_t)gobj
    );

    list = tranger_open_list(priv->tranger, jn_list);

    return msg_iev_build_response(
        gobj,
        list?0:-1,
        list?json_sprintf("List opened: '%s'", list_id):json_string(gobj_log_last_message()),
        0,
        return_data?json_incref(kw_get_list(list, "data", 0, KW_REQUIRED)):0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_close_list(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        KW_DECREF(kw);
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    const char *list_id = kw_get_str(gobj, kw, "list_id", "", 0);

    if(empty_string(list_id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What list_id?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *list = tranger_get_list(priv->tranger, list_id);
    if(!list) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("List not found: '%s'", list_id),
            0,
            0,
            kw  // owned
        );
    }

    int result = tranger_close_list(priv->tranger, list);

    return msg_iev_build_response(
        gobj,
        result,
        result>=0?json_sprintf("List closed: '%s'", list_id):json_string(gobj_log_last_message()),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_add_record(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "write";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        KW_DECREF(kw);
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    int result = 0;
    json_t *jn_comment = 0;

    do {
        /*
         *  Get parameters
         */
        const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
        uint64_t __t__ = kw_get_int(gobj, kw, "__t__", 0, 0);
        uint32_t user_flag = kw_get_int(gobj, kw, "user_flag", 0, 0);
        json_t *record = kw_get_dict(gobj, kw, "record", 0, 0);

        /*
         *  Check parameters
         */
        if(empty_string(topic_name)) {
           jn_comment = json_sprintf("What topic_name?");
           result = -1;
           break;
        }
        json_t *topic = tranger_topic(priv->tranger, topic_name);
        if(!topic) {
           jn_comment = json_sprintf("Topic not found: '%s'", topic_name);
           result = -1;
           break;
        }
        if(!record) {
           jn_comment = json_sprintf("What record?");
           result = -1;
           break;
        }

        /*
         *  Append record to tranger topic
         */
        md_record_t md_record;
        result = tranger_append_record(
            priv->tranger,
            topic_name,
            __t__,                  // if 0 then the time will be set by TimeRanger with now time
            user_flag,
            &md_record,             // required
            json_incref(record)     // owned
        );

        if(result<0) {
            jn_comment = json_string(gobj_log_last_message());
            break;
        } else {
           jn_comment = json_sprintf("Record added");
        }
    } while(0);

    /*
     *  Response
     */
    return msg_iev_build_response(
        gobj,
        result,
        jn_comment,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_get_list_data(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        KW_DECREF(kw);
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s'", permission),
            0,
            0,
            kw  // owned
        );
    }

    const char *list_id = kw_get_str(gobj, kw, "list_id", "", 0);

    if(empty_string(list_id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What list_id?"),
            0,
            0,
            kw  // owned
        );
    }

    json_t *list = tranger_get_list(priv->tranger, list_id);
    if(!list) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("List not found: '%s'", list_id),
            0,
            0,
            kw  // owned
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        json_incref(kw_get_list(list, "data", 0, KW_REQUIRED)),
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




            /***************************
             *      Private Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    json_t *list,
    md2_record_t *md_record,
    json_t *jn_record  // owned
)
{
    json_t *match_cond = kw_get_dict(list, "match_cond", 0, KW_REQUIRED);
    BOOL only_md = kw_get_bool(match_cond, "only_md", 0, 0);
    BOOL has_fields = kw_has_key(match_cond, "fields");
    if(has_fields) {
        only_md = FALSE;
    }

    if(jn_record) {
        json_object_set_new(jn_record, "__md_tranger__", tranger2_md2json(md_record));
    } else {
        if(only_md) {
            jn_record = json_object();
        } else {
            jn_record = tranger_read_record_content(tranger, topic, md_record);
        }
        json_object_set_new(jn_record, "__md_tranger__", tranger2_md2json(md_record));
    }

    json_t *list_data = kw_get_list(list, "data", 0, KW_REQUIRED);

    const char ** keys = 0;
    if(has_fields) {
        const char *fields = kw_get_str(match_cond, "fields", "", 0);
        keys = split2(fields, ", ", 0);
    }
    if(keys) {
        json_t *jn_record_with_fields = kw_clone_by_path(
            jn_record,   // owned
            keys
        );
        jn_record = jn_record_with_fields;
        json_array_append(
            list_data,
            jn_record
        );
        split_free2(keys);
    } else {
        json_array_append(
            list_data,
            jn_record
        );
    }

    if(!(md_record->__system_flag__ & sf_loading_from_disk)) {
        json_t *jn_data = json_array();

        if(only_md) {
            json_array_append(jn_data, kw_get_dict(jn_record, "__md_tranger__", 0, KW_REQUIRED));
        } else {
            json_array_append(jn_data, jn_record);
        }

        hgobj gobj = (hgobj)(size_t)kw_get_int(list, "gobj", 0, KW_REQUIRED);
        gobj_publish_event(gobj, "EV_TRANGER_RECORD_ADDED", jn_data);
    }

    JSON_DECREF(jn_record);

    return 0; // HACK lest timeranger to add record to list.data
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tranger_add_record(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Get parameters
     */
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    uint64_t __t__ = kw_get_int(gobj, kw, "__t__", 0, 0);
    uint32_t user_flag = kw_get_int(gobj, kw, "user_flag", 0, 0);
    json_t *record = kw_get_dict(gobj, kw, "record", 0, 0);

    json_t *__temp__ = kw_get_dict_value(gobj, kw, "__temp__", 0, KW_REQUIRED);
    JSON_INCREF(__temp__); // Save to __answer__

    int result = 0;
    json_t *jn_comment = 0;

    do {
        /*
         *  Check parameters
         */
        json_t *topic = tranger2_topic(priv->tranger, topic_name);
        if(!topic) {
           jn_comment = json_sprintf("Topic not found: '%s'", topic_name);
           result = -1;
           break;
        }
        if(!record) {
           jn_comment = json_sprintf("What record?");
           result = -1;
           break;
        }

        /*
         *  Append record to tranger topic
         */
        md2_record_t md_record;
        result = tranger2_append_record(
            priv->tranger,
            topic_name,
            __t__,                  // if 0 then the time will be set by TimeRanger with now time
            user_flag,
            &md_record,             // required
            json_incref(record)     // owned
        );

        if(result<0) {
            jn_comment = json_string(gobj_log_last_message());
            break;
        } else {
           jn_comment = json_sprintf("Record added");
        }
    } while(0);

    /*
     *  Response
     */
    json_t *iev = iev_create(
        event,
        msg_iev_build_webix2(gobj,
            result,
            jn_comment,
            0,
            0,  // owned
            kw,  // owned
            "__answer__"
        )
    );
    json_object_set_new(iev, "__temp__", __temp__);  // Set the channel

    /*
     *  Inform
     */
    return gobj_send_event(
        src,
        "EV_SEND_IEV",
        iev,
        gobj
    );
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_subscription_added = mt_subscription_added,
    .mt_topic_size = mt_topic_size,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TRANGER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TRANGER_ADD_RECORD);
GOBJ_DEFINE_EVENT(EV_TRANGER_RECORD_ADDED);

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
        {EV_TRANGER_ADD_RECORD,       ac_tranger_add_record,      0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TRANGER_RECORD_ADDED,       EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
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
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level
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
PUBLIC int register_c_tranger(void)
{
    return create_gclass(C_TRANGER);
}
