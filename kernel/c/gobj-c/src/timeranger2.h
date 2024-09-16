/****************************************************************************
 *      TIMERANGER2.H
 *
 *      Time Ranger 2, a serie-time key-value database over flat files
 *
 *      Copyright (c) 2017-2024 Niyamaka.
 *      All Rights Reserved.
 *
 *  Disk layout:
 *
 *  (store directory) -> (database):
 *
 *      __timeranger2__.json    TimerRanger metadata.
 *          Created when the database is created, never modifiable.
 *          File used as a semaphore for processes to use.
 *          For now, a single master process opens it with exclusive write permission.
 *          Non-masters processes open the database in read-only mode.
 *
 *
 *      /{topic}            Topic directory
 *
 *          topic_desc.json     Fixed topic metadata (Only read)
 *
 *                              "topic_name"    Topic name
 *                              "filename_mask"
 *                              "rpermission"
 *                              "xpermission"
 *                              "pkey"          Record's field with Key of message.
 *                              "tkey"          Record's field with Time of message.
 *                              "directory"
 *                              'system_flag'
 *
 *          topic_cols.json     Optional, defines fields of the topic.
 *
 *          topic_var.json      Variable topic metadata (Writable)
 *
 *                              'user_flag' data
 *
 *          /{topic}            Topic directory
 *          /{topic}/{key}          A directory for each key
 *          /{topic}/{key}/data     Directory containing the topic's records: {format-file}.json
 *          /{topic}/{key}/md2      Directory containing the topic's metadata: {format-file}.json
 *
 *          {format-file}.json  Data files
 *                              It should never be modified externally.
 *                              The data is immutable, once it is written, it can never be modified.
 *                              To change the data, add a new record with the desired changes.
 *
 *          metadata (32 bytes):
 *              __t__
 *              __tm__
 *              __offset__
 *              __size__
 *
 ****************************************************************************/

#pragma once

#include "gobj.h"
#include "helpers.h"
#include "kwid.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures  timeranger2
 ***************************************************************/
#define TIME_MASK   0x0000000FFFFFFFFF  /* Maximum date: UTC 4147-08-20T07:32:15+0000 */
#define MD2_MASK    0x0ffffff000000000

#define KEY_TYPE_MASK2        0x00000F
#define NOT_INHERITED_MASK2   0xFF0000 /* Remains will set to all records of topic */

typedef enum { // WARNING table with name's strings in timeranger.c / sf_names
    sf2_string_key          = 0x000001,
    sf2_int_key             = 0x000004,
    sf2_zip_record          = 0x000010,
    sf2_cipher_record       = 0x000020,
    sf2_save_md_in_record   = 0x000040,     // save md in record file too
    sf2_t_ms                = 0x000100,     // record time in miliseconds
    sf2_tm_ms               = 0x000200,     // message time in miliseconds
    sf2_no_record_disk      = 0x001000,
    sf2_loading_from_disk   = 0x010000,
    sf2_soft_deleted_record = 0x020000,     // old sf_mark1
    sf2_hard_deleted_record = 0x800000,
} system_flag2_t;

#define get_system_flag(md_record) (((md_record->__tm__) & 0x0ffffff000000000ULL) >> 36)
#define get_user_flag(md_record) (((md_record->__t__) & 0x0ffffff000000000ULL) >> 36)
#define get_time_t(md_record) ((md_record->__t__) & TIME_MASK)
#define get_time_tm(md_record) ((md_record->__tm__) & TIME_MASK)

#pragma pack(1)

typedef struct { // Size: 96 bytes
    uint64_t __t__;
    uint64_t __tm__;

    uint64_t __offset__;
    uint64_t __size__;
} md2_record_t;

#pragma pack()

/***************************************************************
 *              Prototypes
 ***************************************************************/

/**rst**
   Startup TimeRanger database
**rst**/
static const json_desc_t tranger2_json_desc[] = {
// Name                 Type    Default     Fillspace
{"path",                "str",  "",         ""}, // If database exists then only needs (path,[database]) params
{"database",            "str",  "",         ""}, // If null, path must contains the 'database'

// Default for topics
{"filename_mask",       "str",  "%Y-%m-%d", ""}, // Organization of tables (file name format, see strftime())
{"xpermission" ,        "int",  "02770",    ""}, // Use in creation, default 02770;
{"rpermission",         "int",  "0660",     ""}, // Use in creation, default 0660;

// Volatil fields
{"on_critical_error",   "int",  "2",        ""}, // Volatil, default LOG_OPT_EXIT_ZERO (Zero to avoid restart)
{"master",              "bool", "false",    ""}, // Volatil, the master is the only that can write.
{"gobj",                "int",  "",         ""}, // Volatil, gobj of tranger
{"trace_level",         "int",  "0",        ""}, // Volatil, trace level

{0}
};

static const json_desc_t topic_json_desc[] = {
// Name                 Type    Default     Fillspace
{"on_critical_error",   "int",  "2",        ""}, // Volatil, default LOG_OPT_EXIT_ZERO (Zero to avoid restart)
{"filename_mask",       "str",  "%Y-%m-%d", ""}, // Organization of tables (file name format, see strftime())
{"xpermission" ,        "int",  "02770",    ""}, // Use in creation, default 02770;
{"rpermission",         "int",  "0660",     ""}, // Use in creation, default 0660;

// TODO add these
//"topic_name",
//"pkey",
//"tkey",
//"system_flag",
//"cols",
//"directory",
//"__last_rowid__",
//"topic_idx_fd",
//"fd_opened_files",
//"file_opened_files",
//"lists",
{0}
};

PUBLIC json_t *tranger2_startup(
    hgobj gobj,
    json_t *jn_tranger // owned, See tranger2_json_desc for parameters
);

/**rst**
   Shutdown TimeRanger database
**rst**/
PUBLIC int tranger2_shutdown(json_t *tranger);

/**rst**
   Convert string "s|s|s" or "s s s" or "s,s,s"
   or any combinations of them to system_flag_t integer
**rst**/
PUBLIC system_flag2_t tranger2_str2system_flag(const char *system_flag);

/**rst**
   Create topic if not exist. Alias create table.

       HACK IDEMPOTENT function

   if key type is not specified, then it will be:
        if pkey defined:
            sf2_string_key
        else
            sf2_int_key;

**rst**/
PUBLIC json_t *tranger2_create_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,    // If the topic exists then only needs (tranger, topic_name) parameters
    const char *topic_name,
    const char *pkey,
    const char *tkey,
    json_t *jn_topic_ext,   // owned, See topic_json_desc for parameters
    system_flag2_t system_flag,
    json_t *jn_cols,    // owned
    json_t *jn_var      // owned
);

/**rst**
   Open topic
   HACK IDEMPOTENT function, always return the same json_t topic
**rst**/
PUBLIC json_t *tranger2_open_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,
    const char *topic_name,
    BOOL verbose
);

/**rst**
   Get topic by his topic_name.
   Topic is opened if it's not opened.
   HACK topic can exists in disk, but it's not opened until tranger_open_topic()
**rst**/
PUBLIC json_t *tranger2_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,
    const char *topic_name
);

/**rst**
   Get key size (number of records)
**rst**/
PUBLIC uint64_t tranger2_topic_key_size(
    json_t *tranger,
    const char *topic_name,
    const char *key
);

/**rst**
   Return topic name of topic.
**rst**/
PUBLIC const char *tranger2_topic_name(
    json_t *topic
);

/**rst**
   Close record topic.
**rst**/
PUBLIC int tranger2_close_topic(
    json_t *tranger,
    const char *topic_name
);

/**rst**
   Delete topic. Alias delete table.
**rst**/
PUBLIC int tranger2_delete_topic(
    json_t *tranger,
    const char *topic_name
);

/**rst**
   Backup topic and re-create it.
   If ``backup_path`` is empty then it will be used the topic path
   If ``backup_name`` is empty then it will be used ``topic_name``.bak
   If overwrite_backup is true and backup exists then it will be overwrited
        but before tranger_backup_deleting_callback() will be called
            and if it returns TRUE then the existing backup will be not removed.
   Return the new topic
**rst**/

typedef BOOL (*tranger_backup_deleting_callback_t)( // Return TRUE if you control the backup
    json_t *tranger,
    const char *topic_name,
    const char *path
);
PUBLIC json_t *tranger2_backup_topic(
    json_t *tranger,
    const char *topic_name,
    const char *backup_path,
    const char *backup_name,
    BOOL overwrite_backup,
    tranger_backup_deleting_callback_t tranger_backup_deleting_callback
);

/**rst**
   Write topic var
**rst**/
PUBLIC int tranger2_write_topic_var(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_topic_var  // owned
);

/**rst**
   Write topic cols
**rst**/
PUBLIC int tranger2_write_topic_cols(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_cols  // owned
);

PUBLIC json_t *tranger2_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
);

/**rst**
    Return json object with record metadata
**rst**/
PUBLIC json_t *tranger2_md2json(md2_record_t *md2_record);

/**rst**
    Append a new item to record.
    Return the new record's metadata.
**rst**/
PUBLIC int tranger2_append_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t __t__,         // if 0 then the time will be set by TimeRanger with now time
    uint32_t user_flag,
    md2_record_t *md2_record, // required
    json_t *jn_record       // owned
);

/**rst**
    Delete record.
**rst**/
PUBLIC int tranger2_delete_hard_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid
);

/**rst**
    Write record mark1
**rst**/
PUBLIC int tranger2_delete_soft_record( // old tranger_write_mark1
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid,
    BOOL set
);

/**rst**
    Write record user flag
**rst**/
PUBLIC int tranger2_write_user_flag(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid,
    uint32_t user_flag
);

/**rst**
    Write record user flag using mask
**rst**/
PUBLIC int tranger2_set_user_flag(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid,
    uint32_t mask,
    BOOL set
);

/**rst**
    Read record user flag (for writing mode)
**rst**/
PUBLIC uint32_t tranger2_read_user_flag(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid
);




        /*********************************************
         *          Read mode functions
         *********************************************/




/*
 *  HACK Return of callback:
 *      0 do nothing (callback will create their own list, or not),
 *      1 add record to returned list.data,
 *      -1 break the load
 */
typedef int (*tranger2_load_record_callback_t)(
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *rt_id, // iterator or rt_list/rt_disk id
    json_int_t rowid,
    md2_record_t *md2_record,
    json_t *jn_record  // must be owned
);

/**rst**
    Open iterator

    match_cond:

        backward
        only_md     (don't load jn_record on calling callbacks)

        key     // TODO fuera de match_cond???
        notkey
        rkey    regular expression of key
        filter  dict with fields to match

        rt_by_mem   if TRUE  => realtime by memory (list, master);
                    if FALSE => rt by disk (force to false if not master)
                    default: FALSE


        from_rowid  // if to_rowid && to_t && to_tm is 0 then there is realtime
        to_rowid
        from_t
        to_t
        from_tm
        to_tm

        user_flag
        not_user_flag
        user_flag_mask_set
        user_flag_mask_notset

**rst**/
PUBLIC json_t *tranger2_open_iterator( // LOADING: load data from disk, APPENDING: add real time data
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *match_cond,  // owned
    tranger2_load_record_callback_t load_record_callback, // called on LOADING and APPENDING new record
    const char *iterator_id,     // iterator id, optional, if empty will be the key
    json_t *data    // JSON array, if not empty, fills it with the LOADING data, not owned
);

/**rst**
    Close iterator
**rst**/
PUBLIC int tranger2_close_iterator(
    json_t *tranger,
    json_t *iterator
);

/**rst**
    Get iterator by his id
**rst**/
PUBLIC json_t *tranger2_get_iterator_by_id(
    json_t *tranger,
    const char *iterator_id
);

/**rst**
    Get Iterator size (nº of rows)
**rst**/
PUBLIC size_t tranger2_iterator_size(
    json_t *iterator
);

/**rst**
    Get a list of records from iterator, to work with pages
**rst**/
PUBLIC json_t *tranger2_iterator_get_page( // return must be owned
    json_t *tranger,
    json_t *iterator,
    uint64_t from_rowid,
    uint64_t to_rowid
);

/**rst**
    Open realtime list, valid when the yuno is the master writing,
    realtime messages from append_message()
**rst**/
PUBLIC json_t *tranger2_open_rt_list(
    json_t *tranger,
    const char *topic_name,
    const char *key,        // if empty receives all keys, else only this key
    json_t *match_cond,     // owned
    tranger2_load_record_callback_t load_record_callback,   // called on append new record on mem
    const char *list_id     // list id, optional
);

/**rst**
    Close realtime list
**rst**/
PUBLIC int tranger2_close_rt_list(
    json_t *tranger,
    json_t *list
);

/**rst**
    Get list by his id
**rst**/
PUBLIC json_t *tranger2_get_rt_list_by_id(
    json_t *tranger,
    const char *list_id
);

/**rst**
    Open realtime disk, valid when the yuno is the master writing or not-master reading,
    realtime messages from events of disk
**rst**/
PUBLIC json_t *tranger2_open_rt_disk(
    json_t *tranger,
    const char *topic_name,
    const char *key,        // if empty receives all keys, else only this key
    json_t *match_cond,     // owned
    tranger2_load_record_callback_t load_record_callback,   // called on append new record on disk
    const char *disk_id     // disk id, optional
);

/**rst**
    Close realtime disk
**rst**/
PUBLIC int tranger2_close_rt_disk(
    json_t *tranger,
    json_t *disk
);

/**rst**
    Get disk by his id
**rst**/
PUBLIC json_t *tranger2_get_rt_disk_by_id(
    json_t *tranger,
    const char *disk_id
);

/*
 *  print_md0_record: Print rowid, t, tm, key
 *  print_md1_record: Print rowid, uflag, sflag, t, tm, key
 *  print_md2_record: print rowid, offset, size, t, path
 *  print_record_filename: Print path
 */
PUBLIC void print_md0_record(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    const char *key,
    json_int_t rowid,
    char *bf,
    int bfsize
);
PUBLIC void print_md1_record(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    const char *key,
    json_int_t rowid,
    char *bf,
    int bfsize
);
PUBLIC void print_md2_record(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    const char *key,
    json_int_t rowid,
    char *bf,
    int bfsize
);

PUBLIC void print_record_filename(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    char *bf,
    int bfsize
);

PUBLIC void tranger2_set_trace_level(
    json_t *tranger,
    int trace_level
);

#ifdef __cplusplus
}
#endif
