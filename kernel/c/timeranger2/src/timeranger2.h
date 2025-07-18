/****************************************************************************
 *      TIMERANGER2.H
 *
 *      Time Ranger 2, a serie-time key-value database over flat files
 *
 *      Copyright (c) 2017-2024 Niyamaka.
 *      Copyright (c) 2024, ArtGins.
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
 *          /{topic}                        Topic directory
 *          /{topic}/keys/{key}             A directory for each key
 *          /{topic}/keys/{key}/fmt.json    Files containing the topic's records: {format-file}.json
 *          /{topic}/keys/{key}/fmt.md2     Files containing the topic's metadata: {format-file}.json
 *
 *          {format-file}.json  Data files
 *                              It should never be modified externally.
 *                              The data is immutable, once it is written, it can never be modified.
 *                              To change the data, add a new record with the desired changes.
 *                              (There is an option to delete a record overwriting to 0 the message)
 *
 *          metadata (32 bytes):
 *              __t__
 *              __tm__
 *              __offset__
 *              __size__
 *
 *  Realtime by disk
 *  ----------------
 *
 *  C   create
 *  MI  Monitor Inotify
 *  D   Delete
 *
 *      /{topic}
 *          /keys
 *              /{key}
 *                  {format-file}.json
 *                  {format-file}.md2
 *
 *          /disks                                          C master, MI master
 *              /{rt_id}                                    C non-m, MI non-m, D non-m (all tree)
 *                  /{key}                                  C master, MI non-m
 *                      {format-file}.json  (hard link)     C master, D non-m (when read)
 *                      {format-file}.md2   (hard link)     C master, D non-m (when read)
 *
 *
 *  The non-master is the agent creating the /disks/{rt_id} that the master is monitoring
 *
 *  - (1) MONITOR (MI) /disks/
 *      Master will monitor the (topic) directory where clients will mark their rt disks.
 *  - (2) MONITOR (C) (MI)r /disks/{rt_id}/
 *      Client will create the {rt_id} directory (usually the yuno_role^yuno_name)
 *  - (3) MONITOR Master knows that a Client has opened a rt disk for the topic,
 *      Master to open a mem rt to update /disks/rt_id/
 *  - (4) MONITOR Master update directory /disks/rt_id/ on new records
 *      It will Create a hard link of md2 file
 *  - (5) MONITOR Client notified of update directory /disks/rt_id/ on new records
 *      It will delete hard link and then will read the original .md2 file
 *
 *  HACK tranger is only append. No update, no insert.
 *  A record can be deleted (it's unrecoverable).
 *  Only the master can write or delete, non-master only can read.
 *  The metadata `md2_record_t` can be updated only in two cases:
 *    - any bit in `user_flag`
 *    - `sf_deleted_record` bit in `system_flag` field when the record is deleted
 *
 *  Tranger is based in __t__ of the record (time when was saved)
 *  The __t__ time gets the filename where to save the record,
 *      according to the defined mask in the topic or in the tranger.
 *  Each record's file (.json) has his own metadata (.md) file
 *  In memory, a cache is loaded with the first and last record metadata of each file .md file.
 ****************************************************************************/

#pragma once

#include <yev_loop.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define RECORD_KEY_VALUE_MAX   (48)
#define tranger2_max_key_size() (RECORD_KEY_VALUE_MAX-1)

/***************************************************************
 *              Structures  timeranger2
 ***************************************************************/
#define KEY_TYPE_MASK2        0x000F
#define NOT_INHERITED_MASK    0xF000 /* Remains will set to all records of topic */

typedef enum { /* WARNING table with name's strings in timeranger.c / sf_names */
    sf_string_key           = 0x0001,
    sf_rowid_key            = 0x0002,
    sf_int_key              = 0x0004,
    sf_zip_record           = 0x0010,
    sf_cipher_record        = 0x0020,
    sf_t_ms                 = 0x0100,   /* record time in milliseconds */
    sf_tm_ms                = 0x0200,   /* message time in milliseconds */
    sf_deleted_record       = 0x0400,
    sf_loading_from_disk    = 0x1000,
} system_flag2_t;

typedef struct {
    uint64_t __t__;
    uint64_t __tm__;

    uint64_t __offset__;
    uint64_t __size__;

    uint16_t system_flag;
    uint16_t user_flag;
    uint64_t rowid;
} md2_record_ex_t;

typedef struct {
    const char *topic_name;
    const char *pkey;   // Primary key
    const system_flag2_t system_flag;
    const char *tkey;   // Time key
    const json_desc_t *jn_cols;
    const json_desc_t *jn_topic_ext;
} topic_desc_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/

/*
   Startup TimeRanger database
*/
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
    json_t *jn_tranger, // owned, See tranger2_json_desc for parameters
    yev_loop_h yev_loop
);

/*
   Close TimeRanger database
   Close topics and file's fd
*/
PUBLIC int tranger2_stop(json_t *tranger);

/*
   Shutdown TimeRanger database
   Free memory
*/
PUBLIC int tranger2_shutdown(json_t *tranger);

/*
   Convert string "s|s|s" or "s s s" or "s,s,s"
   or any combinations of them to system_flag_t integer
*/
PUBLIC system_flag2_t tranger2_str2system_flag(const char *system_flag);

/*
   Create topic if not exist. Alias create table.

       HACK IDEMPOTENT function

   if key type is not specified, then it will be:
        if pkey defined:
            sf_string_key
        else
            sf_int_key;

*/
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

/*
   Open topic
   HACK IDEMPOTENT function, always return the same json_t topic
*/
PUBLIC json_t *tranger2_open_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,
    const char *topic_name,
    BOOL verbose
);

/*
   Get topic by his topic_name.
   Topic is opened if it's not opened.
   HACK topic can exists in disk, but it's not opened until tranger_open_topic()
*/
PUBLIC json_t *tranger2_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,
    const char *topic_name
);

/*
   Return a list of topic names
*/
PUBLIC json_t *tranger2_list_topics( // return is yours
    json_t *tranger
);

/*
   Return list of keys of the topic
*/
PUBLIC json_t *tranger2_list_keys(// return is yours, WARNING fn slow for thousands of keys!
    json_t *tranger,
    const char *topic_name
);

/*
   Get topic size (number of records of all keys)
*/
PUBLIC uint64_t tranger2_topic_size( // WARNING fn slow for thousands of keys!
    json_t *tranger,
    const char *topic_name
);

/*
   Get key size (number of records of key)
*/
PUBLIC uint64_t tranger2_topic_key_size(
    json_t *tranger,
    const char *topic_name,
    const char *key
);

/*
   Return topic name of topic.
*/
PUBLIC const char *tranger2_topic_name(
    json_t *topic
);

/*
   Close record topic.
*/
PUBLIC int tranger2_close_topic(
    json_t *tranger,
    const char *topic_name
);

/*
   Delete topic. Alias delete table.
*/
PUBLIC int tranger2_delete_topic(
    json_t *tranger,
    const char *topic_name
);

/*
   Backup topic and re-create it.
   If ``backup_path`` is empty then it will be used the topic path
   If ``backup_name`` is empty then it will be used ``topic_name``.bak
   If overwrite_backup is true and backup exists then it will be overwrite
        but before tranger_backup_deleting_callback() will be called
            and if it returns TRUE then the existing backup will be not removed.
   Return the new topic
*/

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

/*
   Write topic var
*/
PUBLIC int tranger2_write_topic_var(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_topic_var  // owned
);

/*
   Write topic cols
*/
PUBLIC int tranger2_write_topic_cols(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_cols  // owned
);

PUBLIC json_t *tranger2_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
);

PUBLIC json_t *tranger2_list_topic_desc_cols( // Return MUST be decref, old tranger_list_topic_desc()
    json_t *tranger,
    const char *topic_name
);

PUBLIC json_t *tranger2_dict_topic_desc_cols( // Return MUST be decref,old tranger_dict_topic_desc()
    json_t *tranger,
    const char *topic_name
);

/*
    Append a new item to record.
    The 'pkey' and 'tkey' are getting according to the topic schema.
    Return the new record's metadata.
*/
PUBLIC int tranger2_append_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t __t__,         // if 0 then the time will be set by TimeRanger with now time
    uint16_t user_flag,
    md2_record_ex_t *md_record_ex, // required, to return the metadata
    json_t *jn_record       // owned
);

/*
    Delete record.
*/
PUBLIC int tranger2_delete_record(
    json_t *tranger,
    const char *topic_name,
    const char *key
);

/*
    Write record user flag
    This function works directly in disk, segments in memory not used or updated
*/
PUBLIC int tranger2_write_user_flag(
    json_t *tranger,
    const char *topic_name, // In old tranger with 'rowid' was enough to get a record md
    const char *key,        // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t rowid,         // Must be real rowid in the file, not in topic global rowid
    uint32_t user_flag
);

/*
    Write record user flag using mask
    This function works directly in disk, segments in memory not used or updated
*/
PUBLIC int tranger2_set_user_flag(
    json_t *tranger,
    const char *topic_name, // In old tranger with 'rowid' was enough to get a record md
    const char *key,        // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t rowid,         // Must be real rowid in the file, not in topic global
    uint32_t mask,
    BOOL set
);

/*
    Read record user flag
    This function works directly in disk, segments in memory not used or updated
*/
PUBLIC uint16_t tranger2_read_user_flag(
    json_t *tranger,
    const char *topic_name, // In old tranger with 'rowid' was enough to get a record md
    const char *key,        // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t rowid          // Must be real rowid in the file, not in topic global
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
    json_t *list,       // iterator or rt_mem/rt_disk, don't own
    json_int_t rowid,   // in a rt_mem will be the relative rowid, in rt_disk the absolute rowid
    md2_record_ex_t *md_record_ex,
    json_t *jn_record  // must be owned
);

/*
    Iterator match_cond:

        backward
        only_md     (don't load jn_record on calling callbacks)

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

*/
/*
 *  LOADING: load data from disk, APPENDING: add real time data
 *      rt_by_disk  if TRUE  => rt by disk
 *                  if FALSE => realtime by memory
 */
PUBLIC json_t *tranger2_open_iterator(
    json_t *tranger,
    const char *topic_name,
    const char *key,    // required
    json_t *match_cond, // owned
    tranger2_load_record_callback_t load_record_callback, // called on LOADING and APPENDING, optional
    const char *iterator_id,     // iterator id, optional, if empty will be the key
    const char *creator,     // creator
    json_t *data,       // JSON array, if not empty, fills it with the LOADING data, not owned
    json_t *extra       // owned, user data, this json will be added to the return iterator
);

/*
    Close iterator
*/
PUBLIC int tranger2_close_iterator(
    json_t *tranger,
    json_t *iterator
);

/*
    Get iterator by his id
*/
PUBLIC json_t *tranger2_get_iterator_by_id( // Silence inside. Check out.
    json_t *tranger,
    const char *topic_name,
    const char *iterator_id,
    const char *creator
);

/*
    Get Iterator size (nº of rows)
*/
PUBLIC size_t tranger2_iterator_size(
    json_t *iterator
);

/*
    Get a page of records from iterator
    Return
        total_rows:     iterator size (nº of rows)
        pages:          number of pages with the required limit
        data:           list of required records found
*/
PUBLIC json_t *tranger2_iterator_get_page( // return must be owned
    json_t *tranger,
    json_t *iterator,
    json_int_t from_rowid,    // based 1
    size_t limit,
    BOOL backward
);


/*
    Open realtime by mem, valid when the yuno is the master writing,
    realtime messages from append_message()
*/
PUBLIC json_t *tranger2_open_rt_mem(
    json_t *tranger,
    const char *topic_name,
    const char *key,        // if empty receives all keys, else only this key
    json_t *match_cond,     // owned
    tranger2_load_record_callback_t load_record_callback,   // called on append new record on mem
    const char *list_id,    // rt list id, optional (internally will use the pointer of rt)
    const char *creator,
    json_t *extra           // owned, user data, this json will be added to the return iterator
);

/*
    Close realtime mem
*/
PUBLIC int tranger2_close_rt_mem(
    json_t *tranger,
    json_t *mem
);

/*
    Get mem by his id
*/
PUBLIC json_t *tranger2_get_rt_mem_by_id( // Silence inside. Check out.
    json_t *tranger,
    const char *topic_name,
    const char *rt_id,
    const char *creator
);

/*
    Open realtime by disk, valid when the yuno is the master writing or not-master reading,
    realtime messages from events of disk
*/
PUBLIC json_t *tranger2_open_rt_disk(
    json_t *tranger,
    const char *topic_name,
    const char *key,        // if empty receives all keys, else only this key
    json_t *match_cond,     // owned
    tranger2_load_record_callback_t load_record_callback,   // called on append new record on disk
    const char *rt_id,      // rt disk id, REQUIRED
    const char *creator,
    json_t *extra           // owned, user data, this json will be added to the return iterator
);

/*
    Close realtime disk
*/
PUBLIC int tranger2_close_rt_disk(
    json_t *tranger,
    json_t *disk
);

/*
    Get disk by his id
*/
PUBLIC json_t *tranger2_get_rt_disk_by_id( // Silence inside. Check out.
    json_t *tranger,
    const char *topic_name,
    const char *rt_id,
    const char *creator
);

/*
    Open list, load records in memory

    match_cond of second level:
        key                 (str) key (if not exists then rkey is used)
        rkey                (str) regular expression of key (empty "" is equivalent to ".*"
                            WARNING: loading form disk keys matched in rkey)
                                   but loading realtime of all keys

        load_record_callback (tranger2_load_record_callback_t) // called on LOADING and APPENDING

    For the first level see:

        `Iterator match_cond` in timeranger2.h

    HACK Return of callback:
        0 do nothing (callback will create their own list, or not),
        -1 break the load

    Return realtime (rt_mem or rt_disk)  or no_rt

*/
PUBLIC json_t *tranger2_open_list( // WARNING loading all records causes delay in starting applications
    json_t *tranger,
    const char *topic_name,
    json_t *match_cond, // owned
    json_t *extra,      // owned, will be added to the returned rt
    const char *rt_id,
    BOOL rt_by_disk,
    const char *creator
);

/*
    Close list (rt_mem or rt_disk or no_rt)
*/
PUBLIC int tranger2_close_list(
    json_t *tranger,
    json_t *list
);

/*
    Close all, iterators, disk or mem lists belongs to creator
*/
PUBLIC int tranger2_close_all_lists(
    json_t *tranger,
    const char *topic_name,
    const char *rt_id,      // if empty, remove all lists of creator
    const char *creator     // if empty, remove all
);

/*
 *  Read content, useful when you load only md and want recover the content
 *  Load the (JSON) message pointed by metadata (md_record_ex)
 */
PUBLIC json_t *tranger2_read_record_content( // return is yours
    json_t *tranger,
    json_t *topic,
    const char *key,
    md2_record_ex_t *md_record_ex
);

/*
 *  print_md0_record: Print rowid, t, tm, key
 *  print_md1_record: Print rowid, uflag, sflag, t, tm, key
 *  print_md2_record: print rowid, offset, size, t, path
 *  print_record_filename: Print path
 */
PUBLIC void tranger2_print_md0_record(
    char *bf,
    int bfsize,
    const char *key,
    json_int_t rowid,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
);
PUBLIC void tranger2_print_md1_record(
    char *bf,
    int bfsize,
    const char *key,
    json_int_t rowid,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
);
PUBLIC void tranger2_print_md2_record(
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_int_t rowid,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
);

PUBLIC void tranger2_print_record_filename(
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
);

PUBLIC void tranger2_set_trace_level(
    json_t *tranger,
    int trace_level
);

#ifdef __cplusplus
}
#endif
