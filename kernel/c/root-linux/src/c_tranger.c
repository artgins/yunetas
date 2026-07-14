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

list-keys returns, per key, its record count AND its time span taken from the
topic's cache totals: {key, records, fr_t, to_t, fr_tm, to_tm}. A client can
therefore bound a time picker to what the key actually holds, without reading
one single record.

open-iterator also accepts metadata match conditions that pre-filter the index
(0/empty = unset): from_t/to_t, from_tm/to_tm, from_rowid/to_rowid and the
user_flag conditions (user_flag, not_user_flag, user_flag_mask_set,
user_flag_mask_notset). total_rows/pagination then reflect the filtered set:

command-yuno id=1911 service=tranger command=open-iterator iterator_id=it1 topic_name=pp key=1 from_rowid=100 to_rowid=200
command-yuno id=1911 service=tranger command=open-iterator iterator_id=it2 topic_name=pp key=1 from_t=1731601280 to_tm=1731698630

`t` is the PERSISTENCE time (when the record was appended) and `tm` the MESSAGE
time (when the event it carries happened): they are two independent axes, and
the two ranges combine. Both are expressed in the TOPIC's own unit: seconds, or
milliseconds when the topic sets sf_t_ms / sf_tm_ms. `topics expanded=1` returns
each topic's system_flag, pkey and tkey, so a client can tell which unit it must
use before asking for anything else.

Realtime feed (streams new appends as EV_TRANGER_RECORD_ADDED to subscribers):

command-yuno id=1911 service=tranger command=open-rt rt_id=rt1 topic_name=pp key=1
command-yuno id=1911 service=tranger command=close-rt rt_id=rt1


 *          Copyright (c) 2020 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdlib.h>

#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

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
PRIVATE int publish_rt_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,       // rt_mem/rt_disk, don't own
    json_int_t rowid,
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
PRIVATE json_t *cmd_open_rt(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_close_rt(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE int mt_subscription_deleted(hgobj gobj, json_t *subs);

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

PRIVATE sdata_desc_t pm_topics[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_BOOLEAN,   "expanded",     0,              0,          "Return a dict per topic (topic_name, system_flag, pkey, tkey) instead of just its name"),
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
SDATAPM (DTP_STRING,    "rkey",                 0,          0,      "match_cond: regex (PCRE2) over the keys; only for a KEYLESS list. Governs both the disk load AND the realtime feed"),
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
SDATAPM (DTP_STRING,    "rkey",                 0,          0,      "Regex (PCRE2) the key must match (empty = every key)"),
SDATAPM (DTP_STRING,    "order",                0,          0,      "Sort the matching keys: 'key' (default) or 'records'"),
SDATAPM (DTP_BOOLEAN,   "desc",                 0,          0,      "Sort descending"),
SDATAPM (DTP_INTEGER,   "from",                 0,          0,      "First matching key of the page (1-based; needs limit)"),
SDATAPM (DTP_INTEGER,   "limit",                0,          0,      "Page size. 0 (default) = every matching key, answered as a plain list (as before); >0 answers a page: {total_rows, pages, data}"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_open_iterator[] = {
/*-PM----type-----------name--------------------flag----default-description---------- */
SDATAPM (DTP_STRING,    "iterator_id",          0,          0,      "Id of iterator (optional, defaults to key)"),
SDATAPM (DTP_STRING,    "topic_name",           0,          0,      "Topic name"),
SDATAPM (DTP_STRING,    "key",                  0,          0,      "Key to iterate (required)"),
SDATAPM (DTP_BOOLEAN,   "backward",             0,          0,      "Iterate backward"),
SDATAPM (DTP_INTEGER,   "from_t",               0,          0,      "match_cond: from persistence time (t, topic unit; 0=unbounded)"),
SDATAPM (DTP_INTEGER,   "to_t",                 0,          0,      "match_cond: to persistence time (t, topic unit; 0=unbounded)"),
SDATAPM (DTP_INTEGER,   "from_tm",              0,          0,      "match_cond: from message time (tm, topic unit; 0=unbounded)"),
SDATAPM (DTP_INTEGER,   "to_tm",                0,          0,      "match_cond: to message time (tm, topic unit; 0=unbounded)"),
SDATAPM (DTP_INTEGER,   "from_rowid",           0,          0,      "match_cond: first rowid (1-based; negative=from end; 0=unbounded)"),
SDATAPM (DTP_INTEGER,   "to_rowid",             0,          0,      "match_cond: last rowid (negative=from end; 0=unbounded)"),
SDATAPM (DTP_INTEGER,   "user_flag",            0,          0,      "match_cond: exact user_flag"),
SDATAPM (DTP_INTEGER,   "not_user_flag",        0,          0,      "match_cond: exclude records with this user_flag"),
SDATAPM (DTP_INTEGER,   "user_flag_mask_set",   0,          0,      "match_cond: user_flag bits that must be set"),
SDATAPM (DTP_INTEGER,   "user_flag_mask_notset",0,          0,      "match_cond: user_flag bits that must be clear"),
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

PRIVATE sdata_desc_t pm_open_rt[] = {
/*-PM----type-----------name--------------------flag----default-description---------- */
SDATAPM (DTP_STRING,    "rt_id",                0,          0,      "Id of the realtime feed"),
SDATAPM (DTP_STRING,    "topic_name",           0,          0,      "Topic name"),
SDATAPM (DTP_STRING,    "key",                  0,          0,      "Key to follow (empty = all keys)"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_close_rt[] = {
/*-PM----type-----------name--------------------flag----default-description---------- */
SDATAPM (DTP_STRING,    "rt_id",                0,          0,      "Id of the realtime feed"),
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
SDATACM2 (DTP_SCHEMA,   "topics",           SDF_AUTHZ_X,    0,      pm_topics,          cmd_topics,         "List topics: names, or with expanded=1 a dict per topic (system_flag tells the time unit of t/tm)"),
SDATACM2 (DTP_SCHEMA,   "desc",             SDF_AUTHZ_X,    0,      pm_desc,            cmd_desc,           "Schema of topic or full"),
SDATACM2 (DTP_SCHEMA,   "create-topic",     SDF_AUTHZ_X,    0,      pm_create_topic,    cmd_create_topic,   "Create topic"),
SDATACM2 (DTP_SCHEMA,   "open-topic",       SDF_AUTHZ_X,    0,      pm_open_topic,    cmd_open_topic,   "Open topic"),
SDATACM2 (DTP_SCHEMA,   "delete-topic",     SDF_AUTHZ_X,    0,      pm_delete_topic,    cmd_delete_topic,   "Delete topic"),

SDATACM2 (DTP_SCHEMA,   "open-list",        SDF_AUTHZ_X,    0,      pm_open_list,       cmd_open_list,      "Open list. With return_data=1 loads and returns the matching records, auto-closing (one-shot read); else the list stays open collecting appends until close-list"),
SDATACM2 (DTP_SCHEMA,   "close-list",       SDF_AUTHZ_X,    0,      pm_close_list,      cmd_close_list,     "Close list"),
// TODO add-record (write path) is not implemented
SDATACM2 (DTP_SCHEMA,   "add-record",       SDF_AUTHZ_X,    0,      pm_add_record,      cmd_add_record,     "Add record"),
SDATACM2 (DTP_SCHEMA,   "get-list-data",    SDF_AUTHZ_X,    0,      pm_get_list_data,   cmd_get_list_data,  "Get list data"),

SDATACM2 (DTP_SCHEMA,   "list-keys",        SDF_AUTHZ_X,    0,      pm_list_keys,       cmd_list_keys,      "List the keys of a topic with their record counts and their time span (fr_t/to_t, fr_tm/to_tm)"),
SDATACM2 (DTP_SCHEMA,   "open-iterator",    SDF_AUTHZ_X,    0,      pm_open_iterator,   cmd_open_iterator,  "Open a stateful per-key iterator (index only, no upfront load) for cursor pagination; close with close-iterator"),
SDATACM2 (DTP_SCHEMA,   "get-page",         SDF_AUTHZ_X,    0,      pm_get_page,        cmd_get_page,       "Get a page of records from an open iterator: data is {total_rows, pages, data}"),
SDATACM2 (DTP_SCHEMA,   "close-iterator",   SDF_AUTHZ_X,    0,      pm_close_iterator,  cmd_close_iterator, "Close an iterator opened with open-iterator"),

SDATACM2 (DTP_SCHEMA,   "open-rt",          SDF_AUTHZ_X,    0,      pm_open_rt,         cmd_open_rt,        "Open a realtime feed on a topic key (no history load); new appends are published as EV_TRANGER_RECORD_ADDED. Close with close-rt"),
SDATACM2 (DTP_SCHEMA,   "close-rt",         SDF_AUTHZ_X,    0,      pm_close_rt,        cmd_close_rt,       "Close a realtime feed opened with open-rt"),
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
    json_t *rts;        // open realtime feeds registry: rt_id -> integer pointer of the rt handle

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
    priv->rts = json_object();

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
}

/***************************************************************************
 *  Register a handle opened for a remote client (list / iterator / rt).
 *
 *  We keep the POINTER, but never the pointer alone: a topic OWNS the
 *  iterators, rt_mem and rt_disk handles opened on it, and
 *  tranger2_close_topic() closes them all and frees the topic — behind our
 *  back, since anyone can close a topic (the app closing a treedb at
 *  shutdown, a delete-topic at runtime). The topic_name stored beside the
 *  pointer is what lets live_handle() tell a live handle from freed memory.
 ***************************************************************************/
PRIVATE void register_handle(
    json_t *registry,
    const char *id,
    const char *topic_name,
    json_t *handle
)
{
    json_object_set_new(registry, id, json_pack("{s:s, s:I}",
        "topic_name", topic_name?topic_name:"",
        "ptr", (json_int_t)(uintptr_t)handle
    ));
}

/***************************************************************************
 *  The handle registered under `id`, or NULL when the tranger already freed
 *  it (its topic was closed) — in which case there is nothing left to close
 *  and the caller must simply drop the entry. Dereferencing the cached
 *  pointer in that state is a use-after-free: it crashed a yuno on shutdown
 *  (close-treedb frees the topics, then C_TRANGER's mt_destroy walked its
 *  registry closing iterators that no longer existed).
 ***************************************************************************/
PRIVATE json_t *live_handle(hgobj gobj, json_t *registry, const char *id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *entry = json_object_get(registry, id);
    if(!entry || !priv->tranger) {
        return NULL;
    }
    const char *topic_name = kw_get_str(gobj, entry, "topic_name", "", 0);
    if(!tranger2_topic_is_open(priv->tranger, topic_name)) {
        return NULL;
    }

    return (json_t *)(uintptr_t)kw_get_int(gobj, entry, "ptr", 0, 0);
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Close what a remote client left open (it may never send close-list /
     *  close-iterator / close-rt) BEFORE shutting down the tranger they
     *  belong to. A handle whose topic is already closed was freed with it:
     *  live_handle() returns NULL and the entry is just dropped.
     */
    if(priv->lists) {
        const char *list_id; json_t *jn_entry; void *tmp;
        json_object_foreach_safe(priv->lists, tmp, list_id, jn_entry) {
            json_t *list = live_handle(gobj, priv->lists, list_id);
            if(list) {
                tranger2_close_list(priv->tranger, list);
            }
            json_object_del(priv->lists, list_id);
        }
        JSON_DECREF(priv->lists)
    }

    if(priv->iterators) {
        const char *iterator_id; json_t *jn_entry; void *tmp;
        json_object_foreach_safe(priv->iterators, tmp, iterator_id, jn_entry) {
            json_t *iterator = live_handle(gobj, priv->iterators, iterator_id);
            if(iterator) {
                tranger2_close_iterator(priv->tranger, iterator);
            }
            json_object_del(priv->iterators, iterator_id);
        }
        JSON_DECREF(priv->iterators)
    }

    if(priv->rts) {
        const char *rt_id; json_t *jn_entry; void *tmp;
        json_object_foreach_safe(priv->rts, tmp, rt_id, jn_entry) {
            json_t *rt = live_handle(gobj, priv->rts, rt_id);
            if(rt) {
                tranger2_close_list(priv->tranger, rt);
            }
            json_object_del(priv->rts, rt_id);
        }
        JSON_DECREF(priv->rts)
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

    BOOL expanded = kw_get_bool(gobj, kw, "expanded", 0, KW_WILD_NUMBER);

    json_t *topics = kw_get_dict(gobj, priv->tranger, "topics", 0, KW_REQUIRED);
    json_t *topic_list = json_array();

    const char *topic_name; json_t *topic;
    json_object_foreach(topics, topic_name, topic) {
        if(!json_is_object(topic)) {
            continue;
        }
        if(!expanded) {
            json_array_append_new(topic_list, json_string(topic_name));
            continue;
        }

        /*
         *  Expanded: the topic desc (topic_name, pkey, tkey, system_flag,
         *  topic_version). Above all system_flag, which says whether t/tm
         *  are seconds or milliseconds (sf_t_ms / sf_tm_ms).
         */
        json_t *desc = tranger2_topic_desc(priv->tranger, topic_name);
        if(!desc) {
            continue;   // Error already logged
        }
        json_array_append_new(topic_list, desc);
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
                /*
                 *  PCRE2, the same engine timeranger2 now matches rkey with
                 *  (and list-keys above): one flavour of regex per parameter,
                 *  or the same rkey would mean two different things depending
                 *  on which half of open-list ran it.
                 */
                int errornumber = 0;
                PCRE2_SIZE erroroffset = 0;
                pcre2_code *re = pcre2_compile(
                    (PCRE2_SPTR)rkey, PCRE2_ZERO_TERMINATED, 0,
                    &errornumber, &erroroffset, NULL
                );
                if(!re) {
                    PCRE2_UCHAR err[256];
                    pcre2_get_error_message(errornumber, err, sizeof(err));
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER,
                        "msg",          "%s", "Bad rkey regex",
                        "rkey",         "%s", rkey,
                        "offset",       "%d", (int)erroroffset,
                        "error",        "%s", (char *)err,
                        NULL
                    );
                    JSON_DECREF(jn_keys)
                    JSON_DECREF(jn_data)
                    JSON_DECREF(match_cond)
                    return msg_iev_build_response(
                        gobj,
                        -1,
                        json_sprintf("Bad rkey regex '%s' at %d: %s",
                            rkey, (int)erroroffset, (char *)err),
                        0,
                        0,
                        kw  // owned
                    );
                }
                pcre2_jit_compile(re, PCRE2_JIT_COMPLETE);

                pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
                if(!md) {
                    pcre2_code_free(re);
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_MEMORY,
                        "msg",          "%s", "pcre2_match_data_create_from_pattern() FAILED",
                        NULL
                    );
                    JSON_DECREF(jn_keys)
                    JSON_DECREF(jn_data)
                    JSON_DECREF(match_cond)
                    return msg_iev_build_response(
                        gobj,
                        -1,
                        json_sprintf("No memory for the rkey matcher"),
                        0,
                        0,
                        kw  // owned
                    );
                }

                int idx = (int)json_array_size(jn_keys);
                while(--idx >= 0) {
                    const char *key_ = json_string_value(json_array_get(jn_keys, idx));
                    int m = pcre2_match(
                        re, (PCRE2_SPTR)(key_?key_:""), PCRE2_ZERO_TERMINATED,
                        0, 0, md, NULL
                    );
                    if(m < 0) {
                        json_array_remove(jn_keys, idx);
                    }
                }
                pcre2_match_data_free(md);
                pcre2_code_free(re);
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
    /*
     *  `rkey` used to be refused here: the realtime paths did not honour it, so
     *  a live list would have loaded the matching keys from disk and then fed
     *  the caller the appends of EVERY key. timeranger2 applies it to both
     *  halves now (the disk load and rt_mem/rt_disk alike), so a live list can
     *  finally be opened on a SUBSET of the topic's keys.
     */
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
    register_handle(priv->lists, list_id, topic_name, list);

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
    json_t *list = live_handle(gobj, priv->lists, list_id);
    json_object_del(priv->lists, list_id);
    if(!list) {
        /*  Its topic was closed: the tranger closed the list with it.  */
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("%s: list was already closed with its topic: '%s'",
                gobj_yuno_role_plus_name(), list_id),
            0,
            0,
            kw  // owned
        );
    }

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
    json_t *list = live_handle(gobj, priv->lists, list_id);
    if(!list) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: list was already closed with its topic: '%s'",
                gobj_yuno_role_plus_name(), list_id),
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
        json_incref(kw_get_list(gobj, list, "data", 0, KW_REQUIRED)),
        kw  // owned
    );
}

/***************************************************************************
 *  List the keys of a topic with their record counts.
 ***************************************************************************/
/***************************************************************************
 *  Sort a [{key, records}] list in place, by key or by record count.
 *
 *  Sorting HERE is what lets the page be the right page: a client that only
 *  receives 50 of 100.000 keys cannot sort what it was not given. And that
 *  same cardinality is why this is qsort over a plain pointer array, not an
 *  insertion sort over the json array: the sort runs INSIDE the event loop,
 *  and O(n²) with a jansson refcount round-trip per swap stalls the yuno
 *  for the very topics this command was paginated for. Ties on the record
 *  count break by key, so equal counts page deterministically.
 *
 *  Returns -1 if it could NOT sort. The caller must then refuse the command:
 *  an unsorted list paged as if it were sorted is a WRONG page (`from`/`limit`
 *  cut it at positions that mean nothing), and a client has no way to tell —
 *  it asked for order, and the answer says it got it.
 ***************************************************************************/
PRIVATE int cmp_rows_by_key(const void *a, const void *b)
{
    const char *ka = json_string_value(json_object_get(*(json_t * const *)a, "key"));
    const char *kb = json_string_value(json_object_get(*(json_t * const *)b, "key"));
    return strcmp(ka?ka:"", kb?kb:"");
}

PRIVATE int cmp_rows_by_records(const void *a, const void *b)
{
    json_int_t ra = json_integer_value(json_object_get(*(json_t * const *)a, "records"));
    json_int_t rb = json_integer_value(json_object_get(*(json_t * const *)b, "records"));
    if(ra != rb) {
        return (ra < rb)? -1 : 1;
    }
    return cmp_rows_by_key(a, b);
}

PRIVATE int sort_keys(json_t *jn_list, BOOL by_records, BOOL desc)
{
    size_t size = json_array_size(jn_list);
    if(size < 2) {
        return 0;
    }

    json_t **rows = gbmem_malloc(size * sizeof(json_t *));
    if(!rows) {
        return -1;     // Error already logged
    }
    for(size_t i = 0; i < size; i++) {
        rows[i] = json_incref(json_array_get(jn_list, i));
    }

    qsort(rows, size, sizeof(json_t *), by_records? cmp_rows_by_records : cmp_rows_by_key);

    json_array_clear(jn_list);
    for(size_t i = 0; i < size; i++) {
        json_t *row = desc? rows[size - 1 - i] : rows[i];
        json_array_append_new(jn_list, row);    // transfers the incref above
    }
    gbmem_free(rows);

    return 0;
}

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

    const char *rkey = kw_get_str(gobj, kw, "rkey", "", 0);
    const char *order = kw_get_str(gobj, kw, "order", "", 0);
    BOOL desc = kw_get_bool(gobj, kw, "desc", 0, KW_WILD_NUMBER);
    json_int_t from = (json_int_t)kw_get_int(gobj, kw, "from", 0, KW_WILD_NUMBER);
    json_int_t limit = (json_int_t)kw_get_int(gobj, kw, "limit", 0, KW_WILD_NUMBER);

    if(!empty_string(order) && strcmp(order, "key")!=0 && strcmp(order, "records")!=0) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Unknown order: '%s' (key|records)", order),
            0,
            0,
            kw  // owned
        );
    }
    if(from < 1) {
        from = 1;
    }
    if(limit < 0) {
        limit = 0;
    }

    /*
     *  A topic can hold a hundred thousand keys, and until now the answer was
     *  ALWAYS every one of them: a client that only wanted the keys of one
     *  device had to be handed the lot and filter them itself. `rkey` filters
     *  HERE.
     *
     *  PCRE2 and not POSIX regcomp(): every yuno already links libpcre2-8
     *  (tools/cmake/project.cmake) and gobj-c already speaks it
     *  (json_replace_vars.c), and this is the one place in the read path that
     *  runs the SAME pattern against up to a hundred thousand subjects — the
     *  case its JIT exists for. The pattern is compiled and JIT-compiled ONCE,
     *  outside the loop. (A JIT that is not there is not an error: pcre2_match
     *  falls back to the interpreter on its own.)
     */
    pcre2_code *re = NULL;
    pcre2_match_data *match_data = NULL;
    if(!empty_string(rkey)) {
        int errornumber = 0;
        PCRE2_SIZE erroroffset = 0;
        re = pcre2_compile(
            (PCRE2_SPTR)rkey, PCRE2_ZERO_TERMINATED, 0,
            &errornumber, &erroroffset, NULL
        );
        if(!re) {
            PCRE2_UCHAR err[256];
            pcre2_get_error_message(errornumber, err, sizeof(err));
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Bad rkey regex",
                "rkey",         "%s", rkey,
                "offset",       "%d", (int)erroroffset,
                "error",        "%s", (char *)err,
                NULL
            );
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Bad rkey regex '%s' at %d: %s",
                    rkey, (int)erroroffset, (char *)err),
                0,
                0,
                kw  // owned
            );
        }
        pcre2_jit_compile(re, PCRE2_JIT_COMPLETE);

        match_data = pcre2_match_data_create_from_pattern(re, NULL);
        if(!match_data) {
            pcre2_code_free(re);
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "pcre2_match_data_create_from_pattern() FAILED",
                NULL
            );
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("No memory for the rkey matcher"),
                0,
                0,
                kw  // owned
            );
        }
    }

    /*
     *  Every MATCHING key with its record count. The count is a cache lookup,
     *  so it is cheap enough to do for the whole matching set — which is what
     *  lets `order=records` sort by it and `total_rows` be exact. The key's
     *  time span is NOT: it copies the cache totals into a fresh dict, so it
     *  is built only for the keys that actually travel (the page).
     */
    json_t *jn_keys = tranger2_list_keys(priv->tranger, topic_name);
    json_t *jn_matched = json_array();
    int idx; json_t *jn_key;
    json_array_foreach(jn_keys, idx, jn_key) {
        const char *key = json_string_value(jn_key);
        if(!key) {
            key = "";
        }
        if(re) {
            int m = pcre2_match(
                re, (PCRE2_SPTR)key, PCRE2_ZERO_TERMINATED, 0, 0, match_data, NULL
            );
            if(m < 0) {
                continue;   // PCRE2_ERROR_NOMATCH (and any match error): not this key
            }
        }
        json_array_append_new(jn_matched, json_pack("{s:s, s:I}",
            "key", key,
            "records", (json_int_t)tranger2_topic_key_size(priv->tranger, topic_name, key)
        ));
    }
    JSON_DECREF(jn_keys)
    if(re) {
        pcre2_match_data_free(match_data);
        pcre2_code_free(re);
    }

    int sorted = 0;
    if(!empty_string(order)) {
        sorted = sort_keys(jn_matched, strcmp(order, "records")==0, desc);
    } else if(desc) {
        sorted = sort_keys(jn_matched, FALSE, TRUE);
    }
    if(sorted < 0) {
        JSON_DECREF(jn_matched)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: cannot sort the keys of topic '%s', no memory",
                gobj_yuno_role_plus_name(),
                topic_name
            ),
            0,
            0,
            kw  // owned
        );
    }

    json_int_t total_rows = (json_int_t)json_array_size(jn_matched);
    json_int_t first = limit > 0 ? from : 1;
    json_int_t last = limit > 0 ? (from + limit - 1) : total_rows;

    json_t *jn_data = json_array();
    for(json_int_t i = first; i <= last && i <= total_rows; i++) {
        json_t *jn_row = json_incref(json_array_get(jn_matched, (size_t)(i-1)));
        if(!jn_row) {
            continue;   // Cannot happen (i is bounded by total_rows), be safe
        }

        /*
         *  The key's time span, straight from the cache totals: it lets a
         *  client bound a time picker to what the key really holds without
         *  reading a single record. Both axes (t: persistence, tm: message)
         *  in the topic's own unit.
         */
        const char *key = kw_get_str(gobj, jn_row, "key", "", 0);
        json_t *range = tranger2_topic_key_range(priv->tranger, topic_name, key);
        static const char *range_keys[] = {"fr_t", "to_t", "fr_tm", "to_tm", NULL};
        for(int i2 = 0; range_keys[i2] != NULL; i2++) {
            json_int_t v = 0;   // 0 = span unknown (key with no cache totals yet)
            if(range) {
                v = (json_int_t)kw_get_int(gobj, range, range_keys[i2], 0, 0);
            }
            json_object_set_new(jn_row, range_keys[i2], json_integer(v));
        }
        JSON_DECREF(range)

        json_array_append_new(jn_data, jn_row);
    }
    JSON_DECREF(jn_matched)

    /*
     *  With no `limit` the answer is the plain list it has always been — every
     *  client written before this keeps working. Asking for a PAGE gets the
     *  same envelope get-page uses, so a client pages keys exactly as it pages
     *  records.
     */
    json_t *jn_result = jn_data;
    if(limit > 0) {
        json_int_t pages = (total_rows + limit - 1) / limit;
        if(pages < 1) {
            pages = 1;
        }
        jn_result = json_pack("{s:I, s:I, s:o}",
            "total_rows", total_rows,
            "pages", pages,
            "data", jn_data
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_result,
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

    /*
     *  Base match_cond + the optional metadata conditions actually
     *  supplied (0/empty = unset). Every key here is honored per RECORD:
     *  a filtered iterator builds its row index at open, so total_rows,
     *  `pages` and the pages themselves count only matching records, and
     *  get-page's from_rowid is a position among them (not a global rowid).
     *  (Record-FIELD filters are not indexable this way — those stay
     *  client-side in the SPA.)
     */
    json_t *match_cond = json_pack("{s:b}", "backward", backward);
    static const char *cond_keys[] = {
        "from_t", "to_t", "from_tm", "to_tm",
        "from_rowid", "to_rowid",
        "user_flag", "not_user_flag",
        "user_flag_mask_set", "user_flag_mask_notset",
        NULL
    };
    for(int i = 0; cond_keys[i] != NULL; i++) {
        json_int_t v = (json_int_t)kw_get_int(gobj, kw, cond_keys[i], 0, KW_WILD_NUMBER);
        if(v != 0) {
            json_object_set_new(match_cond, cond_keys[i], json_integer(v));
        }
    }

    json_t *iterator = tranger2_open_iterator(
        priv->tranger,
        topic_name,
        key,
        match_cond,         // owned
        NULL,               // no load_record_callback: index only, get-page reads lazily
        iterator_id,
        gobj_name(gobj),    // creator
        NULL,               // data
        json_pack("{s:I}",  // extra, owned: the OWNER, see cmd_open_rt
            "src_gobj", (json_int_t)(uintptr_t)src
        )
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
    register_handle(priv->iterators, iterator_id, topic_name, iterator);

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
    json_t *iterator = live_handle(gobj, priv->iterators, iterator_id);
    if(!iterator) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: iterator was already closed with its topic: '%s'",
                gobj_yuno_role_plus_name(), iterator_id),
            0,
            0,
            kw  // owned
        );
    }

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
    json_t *iterator = live_handle(gobj, priv->iterators, iterator_id);
    json_object_del(priv->iterators, iterator_id);
    if(!iterator) {
        /*  Its topic was closed: the tranger closed the iterator with it.  */
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("%s: iterator was already closed with its topic: '%s'",
                gobj_yuno_role_plus_name(), iterator_id),
            0,
            0,
            kw  // owned
        );
    }

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

/***************************************************************************
 *  Open a realtime feed on a topic key (no history load): each NEW append
 *  is streamed to subscribers as EV_TRANGER_RECORD_ADDED. rt-by-memory on
 *  the master (fires on tranger2_append_record), rt-by-disk on a reader
 *  (fires on the inotify watcher). Registered until close-rt or mt_destroy.
 ***************************************************************************/
PRIVATE json_t *cmd_open_rt(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
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

    const char *rt_id = kw_get_str(gobj, kw, "rt_id", "", 0);
    const char *topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    const char *key = kw_get_str(gobj, kw, "key", "", 0);

    if(empty_string(rt_id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What rt_id?"),
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
    if(kw_has_key(priv->rts, rt_id)) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("Realtime feed is already open: '%s'", rt_id),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  master writes -> rt by memory (fires on append);
     *  non-master reads -> rt by disk (fires on the master's disk mirror).
     */
    /*
     *  Stamp the OWNER (the gobj that asked for the feed) into the rt.
     *  A remote client that dies without close-rt (browser reload, dropped
     *  websocket) would otherwise leave its feed open forever, and each
     *  surviving feed re-publishes every append: N zombie feeds = N copies
     *  of the same record for everybody. mt_subscription_deleted() closes
     *  the feeds of a subscriber that is gone. The pointer is only ever
     *  COMPARED, never dereferenced.
     */
    json_t *rt;
    if(gobj_read_bool_attr(gobj, "master")) {
        rt = tranger2_open_rt_mem(
            priv->tranger,
            topic_name,
            key,                // empty = all keys, else only this key
            json_object(),      // match_cond, owned
            publish_rt_callback,
            rt_id,
            gobj_name(gobj),    // creator
            json_pack("{s:I}",  // extra, owned
                "src_gobj", (json_int_t)(uintptr_t)src
            )
        );
    } else {
        rt = tranger2_open_rt_disk(
            priv->tranger,
            topic_name,
            key,                // empty = all keys, else only this key
            json_object(),      // match_cond, owned
            publish_rt_callback,
            rt_id,              // rt_id REQUIRED for rt_disk
            gobj_name(gobj),    // creator
            json_pack("{s:I}",  // extra, owned
                "src_gobj", (json_int_t)(uintptr_t)src
            )
        );
    }
    if(!rt) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_string(gobj_log_last_message()),
            0,
            0,
            kw  // owned
        );
    }
    register_handle(priv->rts, rt_id, topic_name, rt);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Realtime feed opened: '%s'", rt_id),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *  Close a realtime feed opened with open-rt.
 ***************************************************************************/
PRIVATE json_t *cmd_close_rt(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
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

    const char *rt_id = kw_get_str(gobj, kw, "rt_id", "", 0);
    if(empty_string(rt_id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What rt_id?"),
            0,
            0,
            kw  // owned
        );
    }
    json_t *jn_ptr = json_object_get(priv->rts, rt_id);
    if(!jn_ptr) {
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("Realtime feed not found: '%s'", rt_id),
            0,
            0,
            kw  // owned
        );
    }
    json_t *rt = live_handle(gobj, priv->rts, rt_id);
    json_object_del(priv->rts, rt_id);
    if(!rt) {
        /*  Its topic was closed: the tranger closed the feed with it.  */
        return msg_iev_build_response(
            gobj,
            0,
            json_sprintf("%s: realtime feed was already closed with its topic: '%s'",
                gobj_yuno_role_plus_name(), rt_id),
            0,
            0,
            kw  // owned
        );
    }

    /*  tranger2_close_list dispatches by list_type (rt_mem / rt_disk).  */
    int result = tranger2_close_list(priv->tranger, rt);

    return msg_iev_build_response(
        gobj,
        result,
        result>=0?json_sprintf("Realtime feed closed: '%s'", rt_id):json_string(gobj_log_last_message()),
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

/***************************************************************************
 *  Realtime-only callback (open-rt): stream each NEW append as
 *  EV_TRANGER_RECORD_ADDED, WITHOUT retaining it anywhere (the feed loads
 *  no history and keeps no data list — the subscriber gets the pushes).
 *  Stamps a __md_tranger__ with the same field names get-page emits so the
 *  consumer renders live and paged records identically.
 ***************************************************************************/
PRIVATE int publish_rt_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,       // rt_mem/rt_disk, don't own
    json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    json_t *jn_record   // must be owned
)
{
    hgobj gobj = (hgobj)(uintptr_t)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

    if(!jn_record) {
        /*  content not available for this append — nothing to stream  */
        return 0;
    }

    if(json_is_object(jn_record)) {
        json_t *__md_tranger__ = json_pack("{s:I, s:I, s:I, s:I, s:i, s:i}",
            "g_rowid", (json_int_t)rowid,
            "i_rowid", (json_int_t)md_record_ex->rowid,
            "t", (json_int_t)md_record_ex->__t__,
            "tm", (json_int_t)md_record_ex->__tm__,
            "system_flag", (int)md_record_ex->system_flag,
            "user_flag", (int)md_record_ex->user_flag
        );
        json_object_set_new(jn_record, "__md_tranger__", __md_tranger__);
    }

    /*
     *  The `rt_id` ADDRESSES the record: this callback runs once per OPEN
     *  FEED, so without it two feeds on the same key publish the same append
     *  twice and every subscriber sees a duplicate. A subscriber filters on
     *  its own rt_id (__filter__), so it only ever gets the records of the
     *  feed IT opened.
     */
    const char *rt_id = json_string_value(json_object_get(list, "id"));

    json_t *jn_data = json_pack("{s:s, s:s, s:s, s:I, s:O}",
        "topic_name", tranger2_topic_name(topic),
        "key", key?key:"",
        "rt_id", rt_id?rt_id:"",
        "rowid", rowid,
        "record", jn_record
    );
    gobj_publish_event(gobj, EV_TRANGER_RECORD_ADDED, jn_data);

    JSON_DECREF(jn_record)

    return 0;
}

/***************************************************************************
 *  A subscriber is gone (its session/channel died, or it unsubscribed):
 *  close the realtime feeds and iterators IT opened. A remote client that
 *  dies without close-rt / close-iterator (browser reload, dropped
 *  websocket) used to leave them open forever — and a surviving feed keeps
 *  re-publishing every append, so N zombie feeds meant N copies of the same
 *  record for EVERY subscriber, not just for the one that leaked them.
 *
 *  The owner was stamped into the rt/iterator as `src_gobj` (see
 *  cmd_open_rt / cmd_open_iterator). The pointer is only COMPARED here.
 ***************************************************************************/
PRIVATE int mt_subscription_deleted(
    hgobj gobj,
    json_t *subs    // NOT owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj subscriber = (hgobj)(uintptr_t)kw_get_int(gobj, subs, "subscriber", 0, KW_REQUIRED);
    if(!subscriber || !priv->tranger) {
        return 0;
    }

    /*  The subscriber may still hold other subscriptions to this service
     *  (a tab with several live views closing one of them): only reap when
     *  its LAST subscription goes. gobj calls us BEFORE removing `subs`,
     *  so the one being deleted is still in the list — hence > 1.  */
    json_t *subscribings = gobj_find_subscribings(subscriber, 0, 0, gobj);
    size_t remaining = json_array_size(subscribings);
    JSON_DECREF(subscribings)
    if(remaining > 1) {
        return 0;
    }

    if(priv->rts) {
        const char *rt_id; json_t *jn_entry; void *tmp;
        json_object_foreach_safe(priv->rts, tmp, rt_id, jn_entry) {
            json_t *rt = live_handle(gobj, priv->rts, rt_id);
            if(!rt) {
                /*  Closed with its topic: nothing to close, drop the entry.  */
                json_object_del(priv->rts, rt_id);
                continue;
            }
            hgobj owner = (hgobj)(uintptr_t)kw_get_int(gobj, rt, "src_gobj", 0, 0);
            if(owner != subscriber) {
                continue;
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Closing realtime feed of a gone subscriber",
                "rt_id",        "%s", rt_id,
                "subscriber",   "%s", gobj_short_name(subscriber),
                NULL
            );
            tranger2_close_list(priv->tranger, rt);
            json_object_del(priv->rts, rt_id);
        }
    }

    if(priv->iterators) {
        const char *iterator_id; json_t *jn_entry; void *tmp;
        json_object_foreach_safe(priv->iterators, tmp, iterator_id, jn_entry) {
            json_t *iterator = live_handle(gobj, priv->iterators, iterator_id);
            if(!iterator) {
                /*  Closed with its topic: nothing to close, drop the entry.  */
                json_object_del(priv->iterators, iterator_id);
                continue;
            }
            hgobj owner = (hgobj)(uintptr_t)kw_get_int(gobj, iterator, "src_gobj", 0, 0);
            if(owner != subscriber) {
                continue;
            }
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Closing iterator of a gone subscriber",
                "iterator_id",  "%s", iterator_id,
                "subscriber",   "%s", gobj_short_name(subscriber),
                NULL
            );
            tranger2_close_iterator(priv->tranger, iterator);
            json_object_del(priv->iterators, iterator_id);
        }
    }

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
    .mt_subscription_deleted = mt_subscription_deleted,
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
        {EV_TRANGER_RECORD_ADDED,       EVF_OUTPUT_EVENT|EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
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
