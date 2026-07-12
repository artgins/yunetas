/***********************************************************************
 *          C_TRANGER.C
 *          Tranger GClass.
 *
 *          Trangers: resources with tranger

To test with CLI:

command-yuno id=1911 service=tranger command=open-list list_id=pepe topic_name=pp fields=id,__md_tranger__

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

Cursor pagination (open-iterator/get-page/close-iterator) and key listing:

command-yuno id=1911 service=tranger command=list-keys topic_name=pp
command-yuno id=1911 service=tranger command=open-iterator iterator_id=it1 topic_name=pp key=1
command-yuno id=1911 service=tranger command=get-page iterator_id=it1 from_rowid=1 limit=100
command-yuno id=1911 service=tranger command=close-iterator iterator_id=it1


 *          Copyright (c) 2020 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <regex.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <command_parser.h>
#include <timeranger2.h>

#include "msg_ievent.h"
#include "c_yuno.h"
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
    const char *key,
    json_t *list,       // iterator or rt_mem/rt_disk, don't own
    json_int_t rowid,   // global rowid of key
    md2_record_ex_t *md_record_ex,
    json_t *jn_record   // must be owned
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
PRIVATE json_t *cmd_open_topic(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_delete_topic(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_open_list(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_close_list(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_add_record(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_list_data(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_keys(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_open_iterator(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_get_page(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_close_iterator(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING, "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,  "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
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

PRIVATE sdata_desc_t pm_open_topic[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_create_topic[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic_name",   0,              0,          "Topic name"),
SDATAPM (DTP_STRING,    "pkey",         0,              "id",       "Primary Key"),
SDATAPM (DTP_STRING,    "tkey",         0,              "tm",       "Time Key"),
SDATAPM (DTP_STRING,    "system_flag",  0,              "sf_string_key", "System flag: sf_string_key|sf_rowid_key|sf_int_key|sf_t_ms|sf_tm_ms, future: sf_zip_record|sf_cipher_record"),
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
SDATAPM (DTP_STRING,    "list_id",              0,          0,      "Id of list"),
SDATAPM (DTP_STRING,    "topic_name",           0,          0,      "Topic name"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_get_list_data[] = {
/*-PM----type-----------name--------------------flag----default-description---------- */
SDATAPM (DTP_STRING,    "list_id",              0,          0,      "Id of list"),
SDATAPM (DTP_STRING,    "topic_name",           0,          0,      "Topic name"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_list_keys[] = {
/*-PM----type-----------name--------------------flag----default-description---------- */
SDATAPM (DTP_STRING,    "topic_name",           0,          0,      "Topic name"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_open_iterator[] = {
/*-PM----type-----------name--------------------flag----default-description---------- */
SDATAPM (DTP_STRING,    "iterator_id",          0,          0,      "Id of iterator (optional, defaults to key)"),
SDATAPM (DTP_STRING,    "topic_name",           0,          0,      "Topic name"),
SDATAPM (DTP_STRING,    "key",                  0,          0,      "Key to iterate (required)"),
SDATAPM (DTP_BOOLEAN,   "backward",             0,          0,      "Iterate backward"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_get_page[] = {
/*-PM----type-----------name--------------------flag----default-description---------- */
SDATAPM (DTP_STRING,    "iterator_id",          0,          0,      "Id of iterator"),
SDATAPM (DTP_INTEGER,   "from_rowid",           0,          0,      "First rowid of the page (based 1)"),
SDATAPM (DTP_INTEGER,   "limit",                0,          0,      "Page size (nº of records)"),
SDATAPM (DTP_BOOLEAN,   "backward",             0,          0,      "Page backward"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_close_iterator[] = {
/*-PM----type-----------name--------------------flag----default-description---------- */
SDATAPM (DTP_STRING,    "iterator_id",          0,          0,      "Id of iterator"),
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
SDATACM2 (DTP_SCHEMA,   "open-topic",       SDF_AUTHZ_X,    0,      pm_open_topic,    cmd_open_topic,   "Open topic"),
SDATACM2 (DTP_SCHEMA,   "delete-topic",     SDF_AUTHZ_X,    0,      pm_delete_topic,    cmd_delete_topic,   "Delete topic"),

SDATACM2 (DTP_SCHEMA,   "open-list",        SDF_AUTHZ_X,    0,      pm_open_list,       cmd_open_list,      "Open list. With return_data=1 loads and returns the matching records, auto-closing (one-shot read); else the list stays open collecting appends until close-list"),
SDATACM2 (DTP_SCHEMA,   "close-list",       SDF_AUTHZ_X,    0,      pm_close_list,      cmd_close_list,     "Close list"),
// TODO add-record (write path) is not implemented
SDATACM2 (DTP_SCHEMA,   "add-record",       SDF_AUTHZ_X,    0,      pm_add_record,      cmd_add_record,     "Add record"),
SDATACM2 (DTP_SCHEMA,   "get-list-data",    SDF_AUTHZ_X,    0,      pm_get_list_data,   cmd_get_list_data,  "Get list data"),

SDATACM2 (DTP_SCHEMA,   "list-keys",        SDF_AUTHZ_X,    0,      pm_list_keys,       cmd_list_keys,      "List the keys of a topic with their record counts"),
SDATACM2 (DTP_SCHEMA,   "open-iterator",    SDF_AUTHZ_X,    0,      pm_open_iterator,   cmd_open_iterator,  "Open a stateful per-key iterator (index only, no upfront load) for cursor pagination; close with close-iterator"),
SDATACM2 (DTP_SCHEMA,   "get-page",         SDF_AUTHZ_X,    0,      pm_get_page,        cmd_get_page,       "Get a page of records from an open iterator: data is {total_rows, pages, data}"),
SDATACM2 (DTP_SCHEMA,   "close-iterator",   SDF_AUTHZ_X,    0,      pm_close_iterator,  cmd_close_iterator, "Close an iterator opened with open-iterator"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (DTP_POINTER,     "tranger",          0,                  0,              "Tranger handler"),
SDATA (DTP_STRING,      "path",             SDF_RD|SDF_REQUIRED,"",             "Path of database"),
SDATA (DTP_STRING,      "database",         SDF_RD,             "",             "Database name"),
SDATA (DTP_STRING,      "filename_mask",    SDF_RD|SDF_REQUIRED,"%Y-%m-%d",    "Organization of tables (file name format, see strftime())"),

SDATA (DTP_INTEGER,     "xpermission",      SDF_RD,             "02770",        "Use in creation, default 02770"),
SDATA (DTP_INTEGER,     "rpermission",      SDF_RD,             "0660",         "Use in creation, default 0660"),
SDATA (DTP_INTEGER,     "on_critical_error",SDF_RD,             "2",            "exit on error (Zero to avoid restart)"),
SDATA (DTP_BOOLEAN,     "master",           SDF_RD,             "0",            "the master is the only that can write"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,              "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
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
PRIVATE sdata_desc_t pm_authz_list[] = {
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
SDATAAUTHZ (DTP_SCHEMA, "list",         0,      0,      pm_authz_list,      "Permission to list topics"),
SDATAAUTHZ (DTP_SCHEMA, "read",         0,      0,      pm_authz_read,      "Permission to read topics"),
SDATAAUTHZ (DTP_SCHEMA, "delete",       0,      0,      pm_authz_delete,    "Permission to delete topics"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    json_t *tranger;
    json_t *lists;      // open lists registry: list_id -> integer pointer of the list handle
    json_t *iterators;  // open iterators registry: iterator_id -> integer pointer of the iterator handle

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
    } else if(gobj_is_pure_child(gobj)) {
        subscriber = gobj_parent(gobj);
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
        gobj,
        jn_tranger, // owned
        yuno_event_loop()
    );
    gobj_write_pointer_attr(gobj, "tranger", priv->tranger);

    priv->lists = json_object();
    priv->iterators = json_object();

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Close the lists still open (a remote client may never send
     *  close-list) BEFORE shutting down the tranger they belong to.
     */
    if(priv->lists) {
        const char *list_id; json_t *jn_ptr; void *tmp;
        json_object_foreach_safe(priv->lists, tmp, list_id, jn_ptr) {
            json_t *list = (json_t *)(uintptr_t)json_integer_value(jn_ptr);
            if(list && priv->tranger) {
                tranger2_close_list(priv->tranger, list);
            }
            json_object_del(priv->lists, list_id);
        }
        JSON_DECREF(priv->lists)
    }

    /*
     *  Close the iterators still open (a remote client may never send
     *  close-iterator) BEFORE shutting down the tranger they belong to.
     */
    if(priv->iterators) {
        const char *iterator_id; json_t *jn_ptr; void *tmp;
        json_object_foreach_safe(priv->iterators, tmp, iterator_id, jn_ptr) {
            json_t *iterator = (json_t *)(uintptr_t)json_integer_value(jn_ptr);
            if(iterator && priv->tranger) {
                tranger2_close_iterator(priv->tranger, iterator);
            }
            json_object_del(priv->iterators, iterator_id);
        }
        JSON_DECREF(priv->iterators)
    }

    EXEC_AND_RESET(tranger2_shutdown, priv->tranger)
    priv->tranger = 0;
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    tranger2_stop(priv->tranger);
    return 0;
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
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
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

    int max_refcount = (int)kw_get_int(gobj, kw, "max_refcount", 1, KW_WILD_NUMBER);

    json_t *tranger = priv->tranger;
    int result = 0;
    json_check_refcounts(tranger, max_refcount, &result);
    return msg_iev_build_response(gobj,
        result,
        json_sprintf("check refcounts of tranger: %s. If it was error then check the log files", result==0?"Ok":"Bad"),
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
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
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
PRIVATE json_t *cmd_open_topic(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
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

    json_t *topic = tranger2_open_topic( // WARNING returned json IS NOT YOURS
        priv->tranger,      // If topic exists then only needs (tranger, topic_name) parameters
        topic_name,
        FALSE
    );

    return msg_iev_build_response(gobj,
        topic?0:-1,
        topic?json_sprintf("Topic opened: '%s'", topic_name):json_string(gobj_log_last_message()),
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
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
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

    uint64_t topic_size = tranger2_topic_size(topic, NULL);
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
    const char *permission = "list";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    json_t *topics = kw_get_dict(gobj, priv->tranger, "topics", 0, KW_REQUIRED);
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
 *  Return description (schema) of the topic
 ***************************************************************************/
PRIVATE json_t *cmd_desc(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
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
    json_t *desc = kwid_new_dict(gobj, priv->tranger, 0, "topics`%s`cols", topic_name);

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
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    const char *list_id = kw_get_str(gobj, kw, "list_id", "", 0);
    /*  KW_WILD_NUMBER: booleans/integers arrive as strings when the command
     *  is forwarded (command-yuno/command-agent skip type coercion).  */
    BOOL return_data = kw_get_bool(gobj, kw, "return_data", 0, KW_WILD_NUMBER);

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

    if(kw_has_key(priv->lists, list_id)) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("List is already open: '%s'", list_id),
            0,
            0,
            kw  // owned
        );
    }

    BOOL  backward = kw_get_bool(gobj, kw, "backward", 0, KW_WILD_NUMBER);
    BOOL  only_md = kw_get_bool(gobj, kw, "only_md", 0, KW_WILD_NUMBER);
    int64_t from_rowid = (int64_t)kw_get_int(gobj, kw, "from_rowid", 0, KW_WILD_NUMBER);
    int64_t to_rowid = (int64_t)kw_get_int(gobj, kw, "to_rowid", 0, KW_WILD_NUMBER);
    uint32_t user_flag = (uint32_t)kw_get_int(gobj, kw, "user_flag", 0, KW_WILD_NUMBER);
    uint32_t not_user_flag = (uint32_t)kw_get_int(gobj, kw, "not_user_flag", 0, KW_WILD_NUMBER);
    uint32_t user_flag_mask_set = (uint32_t)kw_get_int(gobj, kw, "user_flag_mask_set", 0, KW_WILD_NUMBER);
    uint32_t user_flag_mask_notset = (uint32_t)kw_get_int(gobj, kw, "user_flag_mask_notset", 0, KW_WILD_NUMBER);
    const char *key = kw_get_str(gobj, kw, "key", 0, 0);
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
    if(rkey) {
        json_object_set_new(match_cond, "rkey", json_string(rkey));
    }
    if(from_t) {
        json_object_set_new(match_cond, "from_t", json_string(from_t));
    }
    if(to_t) {
        json_object_set_new(match_cond, "to_t", json_string(to_t));
    }
    if(from_tm) {
        json_object_set_new(match_cond, "from_tm", json_string(from_tm));
    }
    if(to_tm) {
        json_object_set_new(match_cond, "to_tm", json_string(to_tm));
    }
    if(!empty_string(fields)) {
        json_object_set_new(
            match_cond,
            "fields",
            json_string(fields)
        );
    }

    if(return_data) {
        /*
         *  One-shot snapshot read: load the matching records per key with
         *  short-lived iterators, closed before returning — a remote client
         *  (e.g. a SPA) may never send close-list.
         */
        json_t *jn_data = json_array();
        json_t *jn_keys;
        if(!empty_string(key)) {
            jn_keys = json_pack("[s]", key);
        } else {
            jn_keys = tranger2_list_keys(priv->tranger, topic_name);
            if(!empty_string(rkey)) {
                regex_t re;
                if(regcomp(&re, rkey, REG_EXTENDED|REG_NOSUB)!=0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER,
                        "msg",          "%s", "regcomp() FAILED",
                        "rkey",         "%s", rkey,
                        NULL
                    );
                    JSON_DECREF(jn_keys)
                    JSON_DECREF(jn_data)
                    JSON_DECREF(match_cond)
                    return msg_iev_build_response(
                        gobj,
                        -1,
                        json_sprintf("Bad regular expression in rkey: '%s'", rkey),
                        0,
                        0,
                        kw  // owned
                    );
                }
                int idx = (int)json_array_size(jn_keys);
                while(--idx >= 0) {
                    const char *key_ = json_string_value(json_array_get(jn_keys, idx));
                    if(regexec(&re, key_?key_:"", 0, NULL, 0)!=0) {
                        json_array_remove(jn_keys, idx);
                    }
                }
                regfree(&re);
            }
        }

        int idx; json_t *jn_key;
        json_array_foreach(jn_keys, idx, jn_key) {
            const char *key_ = json_string_value(jn_key);
            json_t *iterator = tranger2_open_iterator(
                priv->tranger,
                topic_name,
                key_,
                json_incref(match_cond),    // owned
                load_record_callback,       // collects into extra's `data`
                "",         // iterator id (defaults to the key)
                list_id,    // creator
                NULL,       // data (the callback collects, applying fields/only_md)
                json_pack("{s:O}", "data", jn_data) // extra, owned
            );
            if(!iterator) {
                // Error already logged
                continue;
            }
            tranger2_close_iterator(priv->tranger, iterator);
        }
        JSON_DECREF(jn_keys)
        JSON_DECREF(match_cond)

        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("List loaded: '%s', %d records", list_id, (int)json_array_size(jn_data)),
            0,
            jn_data,
            kw  // owned
        );
    }

    /*
     *  Stateful list: collects the loaded records and the realtime appends
     *  in its `data` until close-list; appends are also published as
     *  EV_TRANGER_RECORD_ADDED (see load_record_callback).
     */
    if(!empty_string(rkey)) {
        /*  The realtime paths (rt_mem/rt_disk) do not honour rkey.  */
        JSON_DECREF(match_cond)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("rkey is only supported with return_data=1"),
            0,
            0,
            kw  // owned
        );
    }

    json_object_set_new(
        match_cond,
        "load_record_callback",
        json_integer((json_int_t)(uintptr_t)load_record_callback)
    );
    json_t *extra = json_pack("{s:s, s:o, s:I}",
        "id", list_id,
        "data", json_array(),
        "gobj", (json_int_t)(uintptr_t)gobj
    );
    json_t *list = tranger2_open_list(
        priv->tranger,
        topic_name,
        match_cond,     // owned
        extra,          // owned
        list_id,        // rt_id
        !gobj_read_bool_attr(gobj, "master"),   // non-master follows appends via disk
        gobj_name(gobj) // creator
    );
    if(!list) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_string(gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }
    json_object_set_new(priv->lists, list_id, json_integer((json_int_t)(uintptr_t)list));

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("List opened: '%s'", list_id),
        0,
        0,
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
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
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

    json_t *jn_ptr = json_object_get(priv->lists, list_id);
    if(!jn_ptr) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("List not found: '%s'", list_id),
            0,
            0,
            kw  // owned
        );
    }
    json_t *list = (json_t *)(uintptr_t)json_integer_value(jn_ptr);
    json_object_del(priv->lists, list_id);

    int result = tranger2_close_list(priv->tranger, list);

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
    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TODO pending to review",
        NULL
    );
    return msg_iev_build_response(
        gobj,
        -1,
        json_sprintf("Pending to review"),
        0,
        0,
        kw  // owned
    );

//    PRIVATE_DATA *priv = gobj_priv_data(gobj);
//
//    /*----------------------------------------*
//     *  Check AUTHZS
//     *----------------------------------------*/
//    const char *permission = "write";
//    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
//        return msg_iev_build_response(
//            gobj,
//            -403,
//            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
//            0,
//            0,
//            kw  // owned
//        );
//    }
//
//    int result = 0;
//    json_t *jn_comment = 0;
//
//    do {
//        /*
//         *  Get parameters
//         */
//        const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
//        uint64_t __t__ = kw_get_int(gobj, kw, "__t__", 0, 0);
//        uint32_t user_flag = kw_get_int(gobj, kw, "user_flag", 0, 0);
//        json_t *record = kw_get_dict(gobj, kw, "record", 0, 0);
//
//        /*
//         *  Check parameters
//         */
//        if(empty_string(topic_name)) {
//           jn_comment = json_sprintf("What topic_name?");
//           result = -1;
//           break;
//        }
//        json_t *topic = tranger2_topic(priv->tranger, topic_name);
//        if(!topic) {
//           jn_comment = json_sprintf("Topic not found: '%s'", topic_name);
//           result = -1;
//           break;
//        }
//        if(!record) {
//           jn_comment = json_sprintf("What record?");
//           result = -1;
//           break;
//        }
//
//        /*
//         *  Append record to tranger topic
//         */
//        md2_record_t md_record;
//        result = tranger2_append_record(
//            priv->tranger,
//            topic_name,
//            __t__,                  // if 0 then the time will be set by TimeRanger with now time
//            user_flag,
//            &md_record,             // required
//            json_incref(record)     // owned
//        );
//
//        if(result<0) {
//            jn_comment = json_string(gobj_log_last_message());
//            break;
//        } else {
//           jn_comment = json_sprintf("Record added");
//        }
//    } while(0);
//
//    /*
//     *  Response
//     */
//    return msg_iev_build_response(
//        gobj,
//        result,
//        jn_comment,
//        0,
//        0,
//        kw  // owned
//    );
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
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
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

    json_t *jn_ptr = json_object_get(priv->lists, list_id);
    if(!jn_ptr) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("List not found: '%s'", list_id),
            0,
            0,
            kw  // owned
        );
    }
    json_t *list = (json_t *)(uintptr_t)json_integer_value(jn_ptr);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        json_incref(kw_get_list(gobj, list, "data", 0, KW_REQUIRED)),
        kw  // owned
    );
}

/***************************************************************************
 *  List the keys of a topic with their record counts.
 ***************************************************************************/
PRIVATE json_t *cmd_list_keys(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
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

    json_t *jn_keys = tranger2_list_keys(priv->tranger, topic_name);
    json_t *jn_data = json_array();
    int idx; json_t *jn_key;
    json_array_foreach(jn_keys, idx, jn_key) {
        const char *key = json_string_value(jn_key);
        uint64_t size = tranger2_topic_key_size(priv->tranger, topic_name, key);
        json_array_append_new(jn_data, json_pack("{s:s, s:I}",
            "key", key?key:"",
            "records", (json_int_t)size
        ));
    }
    JSON_DECREF(jn_keys)

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *  Open a stateful per-key iterator for cursor pagination. It builds only
 *  the key's row index (no upfront record load, no realtime feed); the
 *  records are read lazily by get-page. Registered until close-iterator or
 *  mt_destroy.
 ***************************************************************************/
PRIVATE json_t *cmd_open_iterator(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    const char *key = kw_get_str(gobj, kw, "key", "", 0);
    const char *iterator_id = kw_get_str(gobj, kw, "iterator_id", "", 0);
    /*  KW_WILD_NUMBER: booleans arrive as strings when forwarded.  */
    BOOL backward = kw_get_bool(gobj, kw, "backward", 0, KW_WILD_NUMBER);

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
    if(empty_string(key)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What key?"),
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
    if(empty_string(iterator_id)) {
        iterator_id = key;
    }
    if(kw_has_key(priv->iterators, iterator_id)) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("Iterator is already open: '%s'", iterator_id),
            0,
            0,
            kw  // owned
        );
    }

    json_t *match_cond = json_pack("{s:b}", "backward", backward);
    json_t *iterator = tranger2_open_iterator(
        priv->tranger,
        topic_name,
        key,
        match_cond,         // owned
        NULL,               // no load_record_callback: index only, get-page reads lazily
        iterator_id,
        gobj_name(gobj),    // creator
        NULL,               // data
        NULL                // extra
    );
    if(!iterator) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_string(gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }
    json_object_set_new(priv->iterators, iterator_id, json_integer((json_int_t)(uintptr_t)iterator));

    json_int_t total_rows = (json_int_t)tranger2_iterator_size(iterator);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Iterator opened: '%s'", iterator_id),
        0,
        json_pack("{s:s, s:I}",
            "iterator_id", iterator_id,
            "total_rows", total_rows
        ),
        kw  // owned
    );
}

/***************************************************************************
 *  Get a page of records from an open iterator. Returns
 *  {total_rows, pages, data} as computed by tranger2_iterator_get_page().
 ***************************************************************************/
PRIVATE json_t *cmd_get_page(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    const char *iterator_id = kw_get_str(gobj, kw, "iterator_id", "", 0);
    if(empty_string(iterator_id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What iterator_id?"),
            0,
            0,
            kw  // owned
        );
    }
    json_t *jn_ptr = json_object_get(priv->iterators, iterator_id);
    if(!jn_ptr) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Iterator not found: '%s'", iterator_id),
            0,
            0,
            kw  // owned
        );
    }
    json_t *iterator = (json_t *)(uintptr_t)json_integer_value(jn_ptr);

    json_int_t from_rowid = (json_int_t)kw_get_int(gobj, kw, "from_rowid", 1, KW_WILD_NUMBER);
    json_int_t limit = (json_int_t)kw_get_int(gobj, kw, "limit", 100, KW_WILD_NUMBER);
    BOOL backward = kw_get_bool(gobj, kw, "backward", 0, KW_WILD_NUMBER);

    json_t *page = tranger2_iterator_get_page(
        priv->tranger,
        iterator,
        from_rowid,
        (size_t)(limit>0?limit:0),
        backward
    );
    if(!page) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_string(gobj_log_last_message()),
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
        page,
        kw  // owned
    );
}

/***************************************************************************
 *  Close an iterator opened with open-iterator.
 ***************************************************************************/
PRIVATE json_t *cmd_close_iterator(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------------*
     *  Check AUTHZS
     *----------------------------------------*/
    const char *permission = "read";
    if(!gobj_user_has_authz(gobj, permission, kw_incref(kw), src)) {
        return msg_iev_build_response(
            gobj,
            -403,
            json_sprintf("No permission to '%s' in service '%s'", permission, gobj_name(gobj)),
            0,
            0,
            kw  // owned
        );
    }

    const char *iterator_id = kw_get_str(gobj, kw, "iterator_id", "", 0);
    if(empty_string(iterator_id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What iterator_id?"),
            0,
            0,
            kw  // owned
        );
    }
    json_t *jn_ptr = json_object_get(priv->iterators, iterator_id);
    if(!jn_ptr) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("Iterator not found: '%s'", iterator_id),
            0,
            0,
            kw  // owned
        );
    }
    json_t *iterator = (json_t *)(uintptr_t)json_integer_value(jn_ptr);
    json_object_del(priv->iterators, iterator_id);

    int result = tranger2_close_iterator(priv->tranger, iterator);

    return msg_iev_build_response(
        gobj,
        result,
        result>=0?json_sprintf("Iterator closed: '%s'", iterator_id):json_string(gobj_log_last_message()),
        0,
        0,
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_topic(hgobj gobj, const char *lmethod, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", KW_REQUIRED);

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

    KW_DECREF(kw)
    return topic;
}

/***************************************************************************
 *  Collect each record into the list's shared `data` array (set in the
 *  `extra` of cmd_open_list, merged by timeranger2 into the iterators and
 *  the rt list). Records appended in realtime (not loaded from disk) are
 *  also published as EV_TRANGER_RECORD_ADDED.
 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,       // iterator or rt_mem/rt_disk, don't own
    json_int_t rowid,   // global rowid of key
    md2_record_ex_t *md_record_ex,
    json_t *jn_record   // must be owned
)
{
    hgobj gobj = (hgobj)(uintptr_t)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
    json_t *match_cond = kw_get_dict(gobj, list, "match_cond", 0, KW_REQUIRED);
    json_t *list_data = kw_get_list(gobj, list, "data", 0, KW_REQUIRED);
    if(!list_data) {
        // Error already logged
        JSON_DECREF(jn_record)
        return -1;
    }

    if(!jn_record) {
        /*  only_md load: the content is not read from disk — synthesize a
         *  md-only record so the list still reflects the matched rows.  */
        jn_record = json_pack("{s:s, s:{s:I, s:I, s:I, s:i, s:i}}",
            "key", key?key:"",
            "__md_tranger__",
                "rowid", (json_int_t)md_record_ex->rowid,
                "t", (json_int_t)md_record_ex->__t__,
                "tm", (json_int_t)md_record_ex->__tm__,
                "system_flag", (int)md_record_ex->system_flag,
                "user_flag", (int)md_record_ex->user_flag
        );
    }

    const char *fields = kw_get_str(gobj, match_cond, "fields", "", 0);
    if(!empty_string(fields)) {
        const char **field_keys = split2(fields, ", ", 0);
        jn_record = kw_clone_by_path(
            gobj,
            jn_record,   // owned
            field_keys
        );
        split_free2(field_keys);
    }
    json_array_append(list_data, jn_record);

    if(!(md_record_ex->system_flag & sf_loading_from_disk)) {
        json_t *jn_data = json_pack("{s:s, s:s, s:I, s:O}",
            "topic_name", tranger2_topic_name(topic),
            "key", key?key:"",
            "rowid", rowid,
            "record", jn_record
        );
        gobj_publish_event(gobj, EV_TRANGER_RECORD_ADDED, jn_data);
    }

    JSON_DECREF(jn_record)

    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tranger_add_record(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->tranger) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "tranger NULL",
            NULL
        );
        return -1;
    }

    /*
     *  Get parameters
     */
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    uint64_t __t__ = kw_get_int(gobj, kw, "__t__", 0, 0);
    uint32_t user_flag = kw_get_int(gobj, kw, "user_flag", 0, 0);
    json_t *record = kw_get_dict(gobj, kw, "record", 0, 0);

    json_t *__temp__ = kw_get_dict_value(gobj, kw, "__temp__", 0, KW_REQUIRED);
    JSON_INCREF(__temp__) // Save to __answer__

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
        md2_record_ex_t md_record;
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
    json_t *kw_response = msg_iev_build_response(
        gobj,
        result,
        jn_comment,
        0,
        0,
        kw
    );

    json_object_set_new(kw_response, "__temp__", __temp__);  // Set the channel
    gbuffer_t *gbuf = iev_create_to_gbuffer(
        gobj,
        event,
        kw_response  // owned and serialized
    );
    if(!gbuf) {
        // error already logged
        return -1;
    }
    json_t *kw_send = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    return gobj_send_event(src,
        EV_SEND_MESSAGE, // original is "EV_SEND_IEV" TODO check
        kw_send,
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
    .mt_topic_size = mt_topic_size,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
};

/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {"get_topic",         get_topic,   "read"},
    {0, 0, 0}
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
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
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
        {EV_TRANGER_ADD_RECORD,         0},
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
        lmt,
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
PUBLIC int register_c_tranger(void)
{
    return create_gclass(C_TRANGER);
}
