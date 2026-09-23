/****************************************************************************
 *      TIMERANGER2.H
 *
 *      Time Ranger 2, a serie-time key-value database over flat files
 *
 *      Copyright (c) 2017-2024 Niyamaka.
 *      Copyright (c) 2024-2026, ArtGins.
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
 *                              "marks_tm_unordered"  true in a topic created
 *                                              after 7.25.4: its md2 files
 *                                              whose __tm__ goes back are
 *                                              marked, see below
 *
 *          topic_cols.json     Optional, defines fields of the topic.
 *
 *          topic_var.json      Variable topic metadata (Writable)
 *
 *                              'user_flag' data
 *                              Always written through topic_var.json.new and
 *                              a rename(), never in place.
 *
 *          /{topic}                        Topic directory
 *          /{topic}/keys/{key}             A directory for each key
 *          /{topic}/keys/{key}/fmt.json    Files containing the topic's records: {format-file}.json
 *          /{topic}/keys/{key}/fmt.md2     Files containing the topic's metadata: {format-file}.json
 *          /{topic}/keys/{key}/fmt.unordered     Marker: a __t__ went back in fmt.md2
 *          /{topic}/keys/{key}/fmt.tm_unordered  Marker: a __tm__ went back in fmt.md2
 *                                          (only a topic with marks_tm_unordered)
 *
 *          The first and the last row of an md2 file give its time range only
 *          while its rows are in order. The master leaves the marker when an
 *          append breaks that order, and a load reads a marked file whole.
 *          In a topic WITHOUT marks_tm_unordered (created by 7.25.4 or earlier) no
 *          file's tm range is trusted: a tm condition leaves no file out and
 *          ends no scan, it skips rows.
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
 *  A whole record (a primary key + every instance under it) can be
 *  deleted via `tranger2_delete_key()` — irrecoverable. There is no
 *  per-instance delete in v7 today (see TODO.md / `sf_deleted_instance`).
 *  Only the master can write or delete, non-master only can read.
 *  The metadata `md2_record_t` can be updated only in cases:
 *    - any bit in `user_flag`
 *
 *  Tranger is based in __t__ of the record (time when was saved)
 *  The __t__ time gets the filename where to save the record,
 *      according to the defined mask in the topic or in the tranger.
 *  Each record's file (.json) has his own metadata (.md) file
 *  In memory, a cache is loaded with the first and last record metadata of each file .md file.
 ****************************************************************************/

#pragma once

#include <limits.h>
#include <yev_loop.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
/*
 *  In timeranger2 the key is stored as a directory name,
 *  so the maximum key size is NAME_MAX (255 on Linux).
 *  (In old timeranger the key was stored inline in the metadata record,
 *  limited to 48 bytes.)
 */
#define RECORD_KEY_VALUE_MAX   (NAME_MAX+1)
#define tranger2_max_key_size() (RECORD_KEY_VALUE_MAX-1)

/***************************************************************
 *              Structures  timeranger2
 ***************************************************************/
#define KEY_TYPE_MASK2        0x000F
#define NOT_INHERITED_MASK    0xF000 /* HACK Remains 0x0FFF will be inherited by all records of topic */

typedef enum { /* WARNING table with name's strings in timeranger.c / sf_names */
    sf_string_key           = 0x0001,
    sf_rowid_key            = 0x0002,
    sf_int_key              = 0x0004,
    sf_zip_record           = 0x0010,
    sf_cipher_record        = 0x0020,
    sf_t_ms                 = 0x0100,   /* record time in milliseconds */
    sf_tm_ms                = 0x0200,   /* message time in milliseconds */
    sf_deleted_instance     = 0x0400,   /* per-instance tombstone, inherited by rt_by_disk followers */
    sf_immutable_record     = 0x0800,   /* record cannot be deleted; inherited band, set per-record */
    sf_loading_from_disk    = 0x1000,
} system_flag2_t;

typedef struct {
    uint64_t __t__;         // time when record is stored
    uint64_t __tm__;        // time when record was created

    uint64_t __offset__;    // offset where the record is stored
    uint64_t __size__;      // size of the record

    uint16_t system_flag;   // system flags managed internally by timeranger
    uint16_t user_flag;     // user flags managed by the user. Examples: tag in treedb, msg pending in queues
    uint64_t rowid;         // row id of the record in the flat file
    uint64_t g_rowid;       // GLOBAL row id of the key, files included: what __md_tranger__ stores as g_rowid
} md2_record_ex_t;

typedef struct {
    const char *topic_name;
    const char *pkey;   // Name of Primary key, usually "id"
    const system_flag2_t system_flag;
    const char *tkey;   // Name of Time key, usually "tm"
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
{0}
};

/*
   Startup a TimeRanger2 database and return its in-memory handle.
   Builds `directory` from `path` (+`database`), then loads (or, if master,
   creates) the on-disk `__timeranger2__.json` metadata.

   `jn_tranger` is owned (consumed here, even on error); see tranger2_json_desc
   for its fields (`path` required; if `database` is empty it is taken from the
   last path segment).

   HACK master fallback: if `master` is requested but the exclusive lock fails,
   the handle silently downgrades to non-master (`"master": false`).

   Return: the tranger handle — it is YOURS, release it with tranger2_shutdown().
   NULL on error (empty path/database, metadata file missing on a non-master, or
   the metadata file cannot be opened).
*/
PUBLIC json_t *tranger2_startup(
    hgobj gobj,
    json_t *jn_tranger, // owned, See tranger2_json_desc for parameters
    yev_loop_h yev_loop
);

/*
   Close a TimeRanger database: close every opened topic and every opened file
   descriptor, and mark the handle `__closed__`. Memory is deliberately RETAINED
   (not freed) so pending yev_loop events can still drain; tranger2_shutdown()
   frees it. `tranger` is borrowed. Always returns 0.

   A closed fd is marked -1 in `fd_opened_files`, so no later stop closes its
   number again. A master gives its single-master lock back here.

   The first call after the stop that opens a topic OR writes (create_topic,
   delete_topic, backup_topic, write_topic_var/_cols, append_record,
   delete_key, delete_instance, write_user_flag, set_user_flag,
   set_system_flag) revives the handle (clears `__closed__`), and a
   master takes its lock again BEFORE anything is written. If another process
   holds it (flock() answers EWOULDBLOCK), that is the startup's single-master
   conflict and it is answered the same way: a CRITICAL at
   `on_critical_error`, so with a yuno's default (LOG_OPT_EXIT_ZERO) the
   process exits(0) and stays down. Any OTHER failure to take it (the lock
   file cannot be opened: EMFILE, ENOENT...; flock() fails with ENOLCK,
   EINTR...) is an ERROR, "Master lock NOT retaken after a stop: ...", and the
   process goes on. A handle that did not take its lock back (and did not
   exit) goes on as a replica: it reads, every write is refused, and its json
   says `"master": false, "master_lost": true`.
   A demoted handle never takes the lock again, not even once it is free (its
   memory did not follow what the other master wrote): shut it down and start
   it again. That holds for a TRANSIENT cause too (EMFILE, EINTR, ENOLCK): the
   process goes on, refuses every write for good, and only the ERROR and
   `master` false (`master_lost` true) say so -- monitor them, and restart. The `master` of the handle is what it holds NOW: read it again
   after the restart.
*/
PUBLIC int tranger2_stop(json_t *tranger);

/*
   Shutdown a TimeRanger database: run tranger2_stop() first if it was not already
   stopped (safe to call after an explicit stop), then FREE the handle.
   `tranger` is owned (consumed/decref'd here). Always returns 0.
*/
PUBLIC int tranger2_shutdown(json_t *tranger);

/*
   Convert string "s|s|s" or "s s s" or "s,s,s"
   or any combinations of them to system_flag_t integer
*/
PUBLIC system_flag2_t tranger2_str2system_flag(const char *system_flag);

/*
   Create a topic if it does not exist (alias: create table), then open and
   return it.

       HACK IDEMPOTENT: if the topic already exists it is just opened; the
       creation branch is skipped.

   Creation is MASTER-ONLY: on a non-master (or a master that lost its lock at a
   stop, see tranger2_stop), if the topic directory is absent the call fails,
   and an existing topic is opened read-only, nothing written. On the master it
   writes topic_desc.json (with "marks_tm_unordered": true) / topic_cols.json /
   topic_var.json plus the keys/ and disks/ subdirs. If `jn_var`'s topic_version
   is greater than the on-disk one, topic_cols.json is REPLACED and then
   topic_var.json (temporary file + rename each, both fsync'ed with their
   directory; the treedb counter `last_rowid_id` is carried over). When either cannot be
   written the call returns NULL (logged) and the topic is not opened: the
   version on disk has not moved, and the next create tries again.

   Key type: if `system_flag` carries no key-type bit it defaults to
        sf_string_key   if pkey is defined
        sf_int_key      otherwise

   `jn_topic_ext`, `jn_cols` and `jn_var` are all owned (consumed here, even on
   every error path).

   Return: the topic — NOT YOURS (it lives inside tranger["topics"]). NULL on
   error (empty topic_name, directory missing on a non-master, empty pkey, or no
   resolvable key type).
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
   Open a topic from disk into memory: load its desc/var/cols, build the in-memory
   key cache, register it under tranger["topics"], and (master + yev_loop) start
   the inotify watcher on its disks/ directory.
   HACK IDEMPOTENT: if already opened, the SAME json_t topic is returned without
   reloading.
   `verbose` TRUE logs an error when the topic directory does not exist.
   Return: the topic — NOT YOURS (owned by tranger["topics"]). NULL on empty name
   or a non-existent topic directory.
*/
PUBLIC json_t *tranger2_open_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,
    const char *topic_name,
    BOOL verbose
);

/*
   Get an opened topic by its topic_name, lazily opening it (with verbose=FALSE)
   if not yet in memory.
   HACK a topic can exist on disk but is not opened until tranger2_open_topic().
   Return: the topic — NOT YOURS. NULL if it cannot be opened, and in that case
   an ERROR is logged (callers that expect a possible miss, e.g. delete_topic,
   probe with access() first to avoid the log).
*/
PUBLIC json_t *tranger2_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,
    const char *topic_name
);

/*
   Write "<tranger directory>/<topic_name>" into the caller buffer `bf`.
   Pure string build: it does NOT validate that the topic exists on disk or in
   memory. Always returns 0.
*/
PUBLIC int tranger2_topic_path(
    char *bf,
    size_t bfsize,
    json_t *tranger,
    const char *topic_name
);

/*
   Return an array of the names (strings) of the topics currently OPENED in memory
   (from tranger["topics"]) — not a disk scan. Compare tranger2_list_topic_names(),
   which scans the database directory on disk.
   Return is yours; never NULL (empty array when no topic is opened).
*/
PUBLIC json_t *tranger2_list_topics( // return is yours
    json_t *tranger
);

/*
   Return an array of names (strings) of every subdirectory found in the tranger
   database directory on disk — each subdir is treated as a topic. Disk scan on
   every call (no caching); any subdirectory counts (not validated as a real
   topic); order is filesystem-dependent.
   Return is yours; never NULL (empty array if the directory cannot be opened).
*/
PUBLIC json_t *tranger2_list_topic_names( // return is yours, WARNING works in disk, not in memory
    json_t *tranger
);

/*
   Return an array of the key names (strings) of a topic, read from its in-memory
   `cache`. Return is yours; never NULL — a missing topic yields an EMPTY array.
   Slow for thousands of keys (O(n), allocates a string per key).
*/
PUBLIC json_t *tranger2_list_keys(// return is yours, WARNING fn slow for thousands of keys!
    json_t *tranger,
    const char *topic_name
);

/*
   Get the topic size (total record count across all keys), summed from the
   in-memory cache counters (not a live disk count). Returns 0 if the topic is
   not found (ambiguous with a genuinely empty topic). Slow for thousands of keys
   (iterates every key).
*/
PUBLIC uint64_t tranger2_topic_size( // WARNING fn slow for thousands of keys!
    json_t *tranger,
    const char *topic_name
);

/*
   Get the record count of one `key` of a topic (from the in-memory cache
   counter). If `key` is empty it falls back to tranger2_topic_size() (the whole
   topic). Returns 0 if the topic is not found.
*/
PUBLIC uint64_t tranger2_topic_key_size(
    json_t *tranger,
    const char *topic_name,
    const char *key
);

/*
   Get the time span of one `key` of a topic, from the in-memory cache totals
   (maintained on load and on every append):

        {"fr_t": t, "to_t": t, "fr_tm": tm, "to_tm": tm, "rows": n}

   `t`/`tm` are in the topic's own unit: seconds, or milliseconds when the topic
   sets sf_t_ms / sf_tm_ms (see `system_flag` in the topic desc).
   Return is yours. NULL (silent) if the topic or the key is unknown.
*/
PUBLIC json_t *tranger2_topic_key_range( // return is yours
    json_t *tranger,
    const char *topic_name,
    const char *key
);

/*
   TRUE if the topic is currently OPEN in this tranger. Silent (a closed topic
   is a legitimate answer, not an error).

   WARNING a topic OWNS the iterators, rt_mem and rt_disk handles opened on it:
   tranger2_close_topic() closes them all (tranger2_close_all_lists) and frees
   the topic. Anyone caching such a handle must therefore check the topic is
   still open before touching it — the handle is dead memory once it is not.
*/
PUBLIC BOOL tranger2_topic_is_open(
    json_t *tranger,
    const char *topic_name
);

/*
   Return the topic_name string stored in a topic object. Takes the TOPIC object
   (not tranger + name). The returned pointer is borrowed (owned by the topic
   json; valid while the topic lives) — do not free it. "" if absent.
*/
PUBLIC const char *tranger2_topic_name(
    json_t *topic
);

/*
   Close an OPENED topic: close its fds, stop the master disk watcher, drop all
   its lists/iterators, and remove it from tranger["topics"] (frees the topic
   memory). Affects memory only — does NOT touch disk. Returns -1 (with an error
   log) if the topic was never opened, else 0.
*/
PUBLIC int tranger2_close_topic(
    json_t *tranger,
    const char *topic_name
);

/*
   Delete a topic (alias: delete table): close it in memory, then rmrdir its
   directory tree from disk — irrecoverable.
   Returns 0 as a no-op if the directory does not exist (supports pre-creation
   cleanup). Returns -1 if the topic cannot be opened or is a system_topic
   (system topics are refused). Otherwise returns the rmrdir() result.
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
   Write topic vars. MASTER-ONLY. MERGES `jn_topic_var` into the existing
   topic_var.json (update, not replace) and persists it through
   topic_var.json.new and a rename(): the file on disk is the old one or the new
   one, never a truncated one (safe against the death of the process; not
   fsync'ed, so not against a power cut). The in-memory topic takes the values,
   except the immutable desc fields, only when the file did. `jn_topic_var` is
   owned (consumed, even on error). Returns 0, or -1 if it is NULL/not a dict,
   the handle is not master, or the file cannot be written.
*/
PUBLIC int tranger2_write_topic_var(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_topic_var  // owned
);

/*
   Write topic cols. MASTER-ONLY. REPLACES the topic's cols WHOLESALE — both the
   in-memory topic["cols"] and topic_cols.json (not a merge). `jn_cols` (a dict or
   a list) is owned (consumed, even on error). Returns 0, or -1 if it is NULL/not
   dict|list, the handle is not master, or the file cannot be written: the file
   is replaced through `topic_cols.json.new` and a rename(), fsync'ed with its
   directory (safe against a power cut: a topic_version change writes these
   cols before the version), and the memory takes the new cols only when the
   file did.
   NOTE: a cols change must bump the topic's topic_version (and schema_version for
   structural changes) or the persisted topic_cols.json masks the new schema.
*/
PUBLIC int tranger2_write_topic_cols(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_cols  // owned
);

/*
   Mark the md2 files of a topic whose __t__ or __tm__ goes back, and make the
   topic one that MARKS: the operator's migration of a topic written before the
   markers existed. MASTER-ONLY, synchronous, on demand.

   Why: a topic created by 7.25.4 or earlier has no "marks_tm_unordered" in its
   topic_desc.json. Its files were never marked, so no file's tm range can be
   trusted, and a tm query (`from_tm` / `to_tm`) leaves out no file and ends no
   scan early: it reads every md2 row of the key, 32 bytes a row. The cost
   grows with the files of the key -- 13 ms in 7.25.4 against ~400 ms after
   b237e0af4 on one key of 30 files x 20000 rows (the review's repro r_perf).
   A marked topic reads only the files whose tm range meets the query.

   What it does, for every key of the topic (the keys of its cache), every md2
   file of the key on disk:
       - reads the file whole, once;
       - writes `<file>.tm_unordered` where a __tm__ goes back, and
         `<file>.unordered` where a __t__ does, unless it is there;
       - gives the cell in memory the file's whole ranges and those flags;
   then, if the topic did not mark yet, sets "marks_tm_unordered": true in
   topic_desc.json (a temporary file, fsync, rename, fsync of the directory)
   and in memory. The next page of an open iterator takes its segments again.

   Cost: a listing of each key's directory and ONE sequential read of every
   md2 file, 32 bytes a row -- linear in the rows and in the files. It is
   synchronous: the yuno's event loop is blocked while it runs. Measured with
   a warm page cache: 16 ms for 1 key of 30 files x 20000 rows (600000 rows);
   72-88 ms for 4 keys of 3650 daily files of one row each (14600 files).
   (Its first version, never released, found each file's cell by walking the
   cells from the first, quadratic in the files of a key: 2.5 s for those
   14600 files.)

   A file that needs a marker and whose name leaves no room for one
   (`<file_id>.tm_unordered` longer than NAME_MAX) is skipped and logged:
   every load reads such a file whole already, so it needs none.

   Run it again on a topic that marks to re-mark it: after a rollback to a
   binary that appends without markers (every release up to 7.25.4), or after
   a crash that lost a marker. It is idempotent: a file already marked is left
   as it is, and a marker is never removed (a marker on a file in order only
   costs a whole read of that file).

   Replicas: a replica that has the topic open keeps reading it as a legacy
   topic (no file left out) until it opens it again; the markers it meets on
   disk are right for either.

   Return a dict, YOURS:
       {
           "topic_name": "...",
           "was_marking": false,       // the topic marked before the call
           "keys": 1, "files": 30, "rows": 600000,
           "t_unordered_marked": 0,    // markers written by this call
           "tm_unordered_marked": 2,
           "marks_tm_unordered": true
       }
   NULL (logged, and in gobj_log_last_message()) when the handle is not the
   master ("Only master can write"), the topic does not exist, a md2 file
   cannot be read, a marker or topic_desc.json cannot be written. The keys
   are walked in the order of the topic's cache, and the call stops at the
   first failure. Then:
       - the topic is NOT marked: "marks_tm_unordered" is unchanged, in
         topic_desc.json and in memory, so no file's tm range is trusted,
         exactly as before the call;
       - the markers written before the failure stay on disk;
       - the cells of the files read before the failure keep, in memory,
         the flags and the whole-file ranges the call gave them, and the
         totals of their keys follow (the key that failed too). Those ranges
         are what the disk holds, so they only make the answers exact.
   Run it again once the cause is fixed.

   Example, the whole store of a yuno, key by key reported:
       json_t *names = tranger2_list_topic_names(tranger);
       size_t i; json_t *jn_name;
       json_array_foreach(names, i, jn_name) {
           json_t *report = tranger2_mark_tm_order(tranger, json_string_value(jn_name));
           if(!report) {
               break;  // logged; the topics already marked stay marked
           }
           JSON_DECREF(report)
       }
       JSON_DECREF(names)
*/
PUBLIC json_t *tranger2_mark_tm_order(
    json_t *tranger,
    const char *topic_name
);

/*
   Return a fresh descriptor dict of a topic: {topic_name, pkey, tkey,
   system_flag, topic_version, cols} where `cols` is the columns as a LIST.
   Return MUST be decref'd. NULL if the topic cannot be opened (error logged).
*/
PUBLIC json_t *tranger2_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
);

/*
   Return a topic's columns as a LIST (array). Return MUST be decref'd. NULL if
   the topic cannot be opened. (old tranger_list_topic_desc())
*/
PUBLIC json_t *tranger2_list_topic_desc_cols( // Return MUST be decref, old tranger_list_topic_desc()
    json_t *tranger,
    const char *topic_name
);

/*
   Return a topic's columns as a DICT keyed by column id. Return MUST be decref'd.
   NULL if the topic cannot be opened. (old tranger_dict_topic_desc())
*/
PUBLIC json_t *tranger2_dict_topic_desc_cols( // Return MUST be decref,old tranger_dict_topic_desc()
    json_t *tranger,
    const char *topic_name
);

/*
    Append a new record to a topic. The primary key (`pkey`) and time key (`tkey`)
    are extracted from `jn_record` per the topic schema. MASTER-ONLY.
    If `__t__` is 0 the time is set to now (milliseconds if the topic is sf_t_ms,
    else seconds). The new record's metadata is returned through the required
    `md_record_ex` out-param (NOT the return value), its global rowid included
    (`md_record_ex->g_rowid`). `jn_record` is owned (consumed, even on error).
    The record is stored WITHOUT a `__md_tranger__` (one it carries from an
    earlier append or a load is dropped: it is metadata, not content), and the
    function adds one only to the record it hands to the realtime lists that
    take it (a list on the key that is not `only_md`). It is not added FOR the
    caller: read the out-param. The function owns the record and changes it IN
    PLACE, so a caller that keeps a reference of its own (json_incref before
    the call) sees both: the carried `__md_tranger__` gone, and a fresh one
    when a list took the record (plus `__rowid__` on an sf_rowid_key topic).
    The content is written before the md2 row. When the md2 cannot be opened,
    created, sought or written, the append is not acknowledged (-1) and the
    content is cut back to where the record began (a partial md2 row too).
    An append into a file flagged unreadable at the cache build counts the
    file again first: readable now, the file gets its cell and loses its
    flag; still unreadable, the append is refused (-1, nothing written) --
    its row would follow rows no cell counts.
    Return: 0 on success, -1 on error (record NULL, not master, topic not found,
    missing/oversized pkey, an unsafe key that would escape keys/, a file of
    the key flagged unreadable, or a write that failed).
*/
PUBLIC int tranger2_append_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t __t__,         // if 0 then the time will be set by TimeRanger with now time
    uint16_t user_flag,
    md2_record_ex_t *md_record_ex, // required, to return the metadata
    json_t *jn_record       // JSON owned
);

/*
    Delete a whole record (= primary key) from a topic.
    Removes the `keys/<key>/` directory and every instance it
    holds. Irrecoverable. Only the master can delete.

    Propagates the deletion to:
      - in-memory subscribers (rt_mem, iterators, rt_disk in this
        process) that registered a key-delete callback via
        `tranger2_set_rt_key_deleted_callback()`;
      - external `rt_by_disk` followers, by removing the matching
        `topic/disks/<rt_id>/<key>/` subdirectories. Followers
        watching their `disks/<rt_id>/` recursively pick it up via
        the standard inotify channel and run the same callback
        fan-out on their side.

    See `tranger2_delete_instance()` for per-instance delete (the
    row in the .md2 index is marked dead and the payload bytes are
    optionally zeroed — irrecoverable, no resurrection).
*/
PUBLIC int tranger2_delete_key(
    json_t *tranger,
    const char *topic_name,
    const char *key
);

/* Legacy name kept as a source-level alias so existing callers
   keep compiling. New code should use `tranger2_delete_key`.
   Will be removed in a future major bump. */
#define tranger2_delete_record tranger2_delete_key

/*
    Delete a single instance (one row of the per-key .md2 index).

    The .md2 row is mutated in place: `sf_deleted_instance` is OR'd
    into the system_flag bits. The on-disk `.md2`/`.json` files
    stay append-only — the row's slot is kept, only its tombstone
    bit is set. Read paths (iterator history, iterator pages,
    rt_by_disk follower) skip rows with the bit set; downstream
    consumers like treedb inherit the skip transparently.

    Side effects:
      - `rowid`s do NOT renumber. Slot N stays slot N (dark).
      - `iterator_size()`, `total_rows`, `pages` keep counting the
        slot. Cache cells (fr_t/to_t/fr_tm/to_tm) are not refreshed.
      - `tranger2_read_record_content()` and
        `tranger2_read_user_flag()` still return the row when the
        caller supplies (key, __t__, rowid) directly. The skip only
        applies to iteration. This is intentional, for audit /
        wipe-verification tooling.

    If `zero_payload` is TRUE, the matching `__size__` bytes at
    `__offset__` in the corresponding `data/<mask>.json` are
    overwritten with zeros (sensitive-data wipe). Opt-in because
    the JSON file is no longer a parseable concatenation after the
    hole — only the .md2 index makes sense of it.

    Irrecoverable. Master-only.
*/
PUBLIC int tranger2_delete_instance(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    uint64_t __t__,
    uint64_t rowid,         // per-key i_rowid (slot in .md2), based 1
    BOOL zero_payload
);

/*
    Write record user flag
    This function works directly in disk, segments in memory not used or updated
    Master only: a replica (or a master that lost its lock) logs "Only master
    can write" and gets -1.
*/
PUBLIC int tranger2_write_user_flag(
    json_t *tranger,
    const char *topic_name, // In old tranger with 'rowid' was enough to get a record md
    const char *key,        // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t rowid,         // Must be real rowid in the file, not in topic global rowid
    uint16_t user_flag
);

/*
    Write record user flag using mask
    This function works directly in disk, segments in memory not used or updated
    Master only: a replica (or a master that lost its lock) logs "Only master
    can write" and gets -1.
*/
PUBLIC int tranger2_set_user_flag(
    json_t *tranger,
    const char *topic_name, // In old tranger with 'rowid' was enough to get a record md
    const char *key,        // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t rowid,         // Must be real rowid in the file, not in topic global
    uint16_t mask,
    BOOL set
);

/*
    Set/reset writable system-flag bits using a mask.
    Only sf_immutable_record may be written; any other bit is refused so a
    caller cannot flip key-type / ms / tombstone / loading bits.
    This function works directly in disk, segments in memory not used or updated
    Master only: a replica (or a master that lost its lock) logs "Only master
    can write" and gets -1.
*/
PUBLIC int tranger2_set_system_flag(
    json_t *tranger,
    const char *topic_name, // In old tranger with 'rowid' was enough to get a record md
    const char *key,        // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t rowid,         // Must be real rowid in the file, not in topic global
    uint16_t mask,
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
                        // WARNING absolute = the key's GLOBAL rowid, files included: a topic
                        // rotates its md2 (filename_mask) and the position INSIDE the new file
                        // restarts at 1, so a consumer deduping/paging by this must be given
                        // the global one (md_record_ex->g_rowid, what __md_tranger__ stores as g_rowid)
    md2_record_ex_t *md_record_ex,
    json_t *jn_record  // must be JSON owned
);

/*
    Key-delete callback for rt_mem / rt_disk / iterators.

    Fires when `tranger2_delete_key()` runs against a key this
    subscriber tracks:
      - on the master: directly from `tranger2_delete_key()`.
      - on `rt_by_disk` followers: from the inotify watcher on
        `disks/<rt_id>/`, when the master mirrors the deletion.

    The subscriber's `key` filter is honoured: an empty `key` ("")
    matches every deletion; a specific key only matches that one.

    Returning non-zero is allowed but currently ignored — there is
    nothing to break out of.

    Register with `tranger2_set_rt_key_deleted_callback()`.
*/
typedef int (*tranger2_key_deleted_callback_t)(
    json_t *tranger,
    json_t *topic,
    const char *key,        // the deleted key
    json_t *list,           // iterator / rt_mem / rt_disk entry, don't own
    void *user_data
);

/*
    Register a key-delete callback on a list/iterator/rt_disk
    handle returned by `tranger2_open_rt_mem()`,
    `tranger2_open_rt_disk()` or `tranger2_open_iterator()`.

    Additive: existing handles default to "no callback", behaviour
    unchanged. `user_data` is opaque; it is passed back verbatim.
    Passing `cb=NULL` clears any previously registered callback.
*/
PUBLIC int tranger2_set_rt_key_deleted_callback(
    json_t *list,
    tranger2_key_deleted_callback_t cb,
    void *user_data
);

/*
    Iterator match_cond:

        backward
        only_md     (don't load jn_record on calling callbacks; honored by the
                     historical load AND the realtime feeds — rt_mem and rt_disk
                     hand the callback NULL jn_record, saving the disk read)

        from_rowid  // if to_rowid && to_t && to_tm is 0 then there is realtime
        to_rowid
        from_t      // t:  PERSISTENCE time (when the record was appended)
        to_t
        from_tm     // tm: MESSAGE time (the record's tkey field)
        to_tm

        user_flag
        not_user_flag
        user_flag_mask_set
        user_flag_mask_notset

    `t` and `tm` are two INDEPENDENT axes and their ranges combine (AND). Both
    are expressed in the TOPIC's own unit: seconds, or milliseconds when the
    topic sets sf_t_ms / sf_tm_ms.

    Every condition above is honored per RECORD, in both modes of the iterator:
      - LOADING (a load_record_callback and/or `data`): each record is matched
        as the history is walked.
      - PAGING (neither): a filtered iterator builds its row INDEX at open, so
        tranger2_iterator_size() and tranger2_iterator_get_page() count and
        return exactly the matching records. In that case get_page's
        `from_rowid` is a position among the MATCHING records (1-based), not a
        global rowid; an UNFILTERED iterator builds no index (open stays
        O(1) on the key size) and its positions are the global rowids.

    WARNING an unfiltered iterator counts its rows from the segment totals, so
    a deleted instance (tranger2_delete_instance) still adds to its total_rows
    while its pages skip it. A filtered one never indexes a dead row, so its
    count and its pages agree.

    WARNING the index is a SNAPSHOT taken at open: records appended after a
    FILTERED iterator opens never enter its total_rows or its pages, while an
    unfiltered iterator recounts the key on every call and keeps growing.
    Reopen the iterator (or pair it with a realtime feed) to see new appends.

    A LOADING that meets a row whose metadata or whose CONTENT cannot be read
    stops there, logs it, and leaves `"load_failed": true` in the returned
    iterator: the history the callback saw is incomplete. The rows before it,
    in the load's direction, WERE handed to the callback: forward the oldest
    ones, backward the newest. (Up to 7.25.4 a content that could not be read
    was handed as NULL and the load went on; an `only_md` load reads no
    content, and no content fails it.)
    The same holds after a RESTART: a md2 file the topic's cache could not
    count when it was built from disk -- one that cannot be opened or read,
    or whose size is not a whole number of rows -- flags its key. Every
    iterator of that key says `"load_failed": true` (paging ones too, and
    logs it), and a loading stops where the first flagged file is in its
    direction. E.g. key A with files d1 d2 d3 and garbage after d2's md2
    while the yuno was down:
        forward load  -> the rows of d1, then load_failed
        backward load -> the rows of d3, then load_failed
    A md2 of 0 rows gets no cell and flags nothing. With a content file that
    is not empty it is what an append never acknowledged leaves (content
    written, md2 row not): a WARNING names the file and the key loads from
    its other files ("md2 file of the key with no rows and a content file
    that is not empty: an append that was never acknowledged, the file is
    ignored"). The flag of a file goes when a cell counts it again (an append
    into it that finds it readable, or a follower that reads it whole);
    tranger2_delete_key() of the key clears every flag with the key.
    tranger2_open_list() answers NULL for such a load of its one key; a
    keyless list goes on with the other keys and names the failed ones in the
    handle it returns (see tranger2_open_list).

    `tm` is written by the producer and the md2 files are cut by `t`, so the
    segments of a tm condition can leave out a file in the middle: the scan
    steps over the hole. A row past the tm range ends the scan of its FILE
    (not of the key) only when the file is known to be in tm order: a topic
    with "marks_tm_unordered" and a file without `.tm_unordered`.

    A delete of the key (tranger2_delete_key(), or a replica's key-deleted
    notice) drops the segments of every iterator of the key: an unfiltered one
    takes them again at its next page, a filtered one keeps an EMPTY index.

    Return: the iterator, NOT YOURS (the topic owns it). NULL (error logged,
    match_cond and extra consumed) if the topic cannot be opened, `key` is
    empty, an iterator with this id and creator already exists, or a filtered
    paging index cannot be built.
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
    Close an iterator: close any rt_mem/rt_disk feed attached to it, then remove
    it from the topic's iterators list. Returns -1 (error logged) if `iterator`
    is NULL or not found in the topic, else 0.
*/
PUBLIC int tranger2_close_iterator(
    json_t *tranger,
    json_t *iterator
);

/*
    Get an open iterator by its id (optionally filtered by creator). Returns the
    iterator — NOT YOURS (borrowed, owned by the topic). NULL if none matches or
    `id` is empty. Stays silent on a miss (only the empty-id case is logged).
    A NULL or empty `creator` matches only creatorless iterators; to reach one
    registered with a creator, pass that same creator.
*/
PUBLIC json_t *tranger2_get_iterator_by_id( // Silence inside. Check out.
    json_t *tranger,
    const char *topic_name,
    const char *iterator_id,
    const char *creator
);

/*
    Get the iterator size: the number of rows it will return.
    A FILTERED iterator answers with its index (the records matching
    match_cond); an unfiltered one sums its segments. 0 if it has neither.
*/
PUBLIC size_t tranger2_iterator_size(
    json_t *iterator
);

/*
    Get a page of records from iterator.
    `from_rowid` (1-based) is a position among the rows the iterator RETURNS:
    a global rowid when it is unfiltered, a position among the matching
    records when it filters (see `Iterator match_cond` above).
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
    Open a realtime-by-memory feed — valid on the MASTER that writes the topic;
    `load_record_callback` fires on each append (from tranger2_append_record()).
    `key` empty means all keys, else only that key. `load_record_callback` is
    REQUIRED. `list_id` is optional (defaults to the handle pointer). `match_cond`
    and `extra` are owned (consumed, even on error); `extra`'s fields are merged
    into the handle without overwriting the reserved ones.
    Return: the rt_mem handle — NOT YOURS (owned by the topic; close with
    tranger2_close_rt_mem()). NULL if the topic is missing, the callback is NULL,
    or a feed with the same id already exists.
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
    Close a realtime-by-memory feed and remove it from the topic. Returns -1 if
    `mem` is NULL, else 0.
*/
PUBLIC int tranger2_close_rt_mem(
    json_t *tranger,
    json_t *mem
);

/*
    Get an open rt_mem feed by its id (optionally filtered by creator). Returns the
    handle — NOT YOURS (borrowed). NULL if none matches or `id` is empty (only the
    empty-id case is logged). A NULL or empty `creator` matches only creatorless
    feeds; to reach one registered with a creator, pass that same creator.
*/
PUBLIC json_t *tranger2_get_rt_mem_by_id( // Silence inside. Check out.
    json_t *tranger,
    const char *topic_name,
    const char *rt_id,
    const char *creator
);

/*
    Open a realtime-by-disk feed — valid on the master writing OR a non-master
    reading; `load_record_callback` fires on disk-change events. `key` empty means
    all keys, else only that key. `load_record_callback` is REQUIRED and `rt_id`
    is REQUIRED (unlike rt_mem). `match_cond` and `extra` are owned (consumed, even
    on error). Creates the realtime disk directory and its inotify monitor.
    Return: the rt_disk handle — NOT YOURS (owned by the topic; close with
    tranger2_close_rt_disk()). NULL if the topic is missing, the callback is NULL,
    `rt_id` is refused (empty, a path metacharacter, longer than NAME_MAX: a
    warning), or a feed of this tranger already uses the id, whatever its
    creator (a warning). The guard is per process: two processes that follow
    one store with the same id still take each other's disks/<id>/ directory.
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
    Close a realtime-by-disk feed: stop its inotify monitor and remove it from the
    topic. Returns -1 if `disk` is NULL, else 0.
*/
PUBLIC int tranger2_close_rt_disk(
    json_t *tranger,
    json_t *disk
);

/*
    Get an open rt_disk feed by its id (optionally filtered by creator). Returns
    the handle — NOT YOURS (borrowed). NULL if none matches or `id` is empty (only
    the empty-id case is logged). A NULL or empty `creator` matches only creatorless
    feeds; to reach one registered with a creator, pass that same creator.
*/
PUBLIC json_t *tranger2_get_rt_disk_by_id( // Silence inside. Check out.
    json_t *tranger,
    const char *topic_name,
    const char *rt_id,
    const char *creator
);

/*
    Open a list: load the matching records from disk (calling load_record_callback
    on each), and, when the requested range is open-ended, keep the list live by
    also opening a realtime feed on top. It is the high-level wrapper over
    tranger2_open_iterator() + tranger2_open_rt_mem()/tranger2_open_rt_disk().

    WARNING: this loads every matching record into memory up front, which can delay
    application start-up on large topics. Close the returned handle with
    tranger2_close_list() (routes to the right closer by "list_type").

    match_cond (second level, consumed by this function):
        key                 (str) exact key to load; if absent, rkey is used
        rkey                (str) PCRE2 regular expression over keys ("" == ".*")
                            Honoured by BOTH halves of the list: the DISK load
                            only visits keys matching rkey, and the REALTIME feed
                            only dispatches appends of keys matching it. The
                            compiled pattern lives in the list and dies with it.
        to_rowid            (int) upper bound of the range; see "realtime" below
        load_record_callback (tranger2_load_record_callback_t) REQUIRED
                            passed inside match_cond, not as a C argument.
                            Called on both LOADING (disk) and APPENDING (realtime).

    For the first-level match_cond (backward, from/to_rowid|t|tm, user_flag, ...) see:

        `Iterator match_cond` in this header.

    Realtime vs no_rt:
        - If to_rowid == 0 (open-ended range) a realtime feed is opened:
            rt_by_disk == TRUE  -> realtime by disk (tranger2_open_rt_disk)
            rt_by_disk == FALSE -> realtime by memory (tranger2_open_rt_mem,
                                   only valid on the master that writes the topic)
          Returns the realtime handle (list_type "rt_mem" or "rt_disk").
        - Otherwise (to_rowid != 0) no realtime is opened: the load is one-shot and
          the function returns `extra` tagged {"list_type": "no_rt"}. In this case
          `extra` is REQUIRED (a NULL extra is an error).

    Return of load_record_callback:
        0   do nothing (the callback may build its own list, or not)
        -1  break the load

    The history is loaded with one-shot iterators of a reserved creator
    ("__tranger2_open_list__"), so an iterator the caller keeps open on a key
    does not get in the way.

    A key whose history cannot be loaded whole (see `load_failed` of
    tranger2_open_iterator()):
        - a list of ONE key (`key` set) is refused: NULL, error logged.
        - a KEYLESS list logs the key ("Cannot load the whole history of a
          key of the list: the records read before the failure were handed,
          the list goes on with the next key"), loads every other key,
          opens its realtime feed, and says what it lacks in the handle:
              "load_failed": true,
              "load_failed_keys": ["B", ...]
          A caller that must not act on a partial history reads it, e.g.:
              json_t *list = tranger2_open_list(tranger, "devices", match_cond, extra, "", FALSE, "");
              if(json_is_true(json_object_get(list, "load_failed"))) {
                  // do not take "not found in the list" as "not on disk"
              }
    The records already handed to the callback stay handed in both cases: of
    a failed key, the ones read before the failure (forward: its oldest,
    backward: its newest).

    Return: the realtime handle (rt_mem / rt_disk) or the no_rt `extra`, NULL on error.
    Both `match_cond` and `extra` are owned (consumed) by this call.
*/
PUBLIC json_t *tranger2_open_list( // WARNING loading all records causes delay in starting applications
    json_t *tranger,
    const char *topic_name,
    json_t *match_cond, // owned
    json_t *extra,      // owned; added to the returned rt, or IS the returned handle when no_rt
    const char *rt_id,  // rt_by_disk: REQUIRED (disks/<rt_id>/, empty is refused); rt_mem: optional
    BOOL rt_by_disk,    // TRUE: realtime by disk; FALSE: realtime by memory (master only)
    const char *creator
);

/*
    Close a list opened with tranger2_open_list(), dispatching by its "list_type":
    rt_mem -> tranger2_close_rt_mem, rt_disk -> tranger2_close_rt_disk, no_rt ->
    just decref the handle. Returns -1 (error logged) on an unknown list_type.
*/
PUBLIC int tranger2_close_list(
    json_t *tranger,
    json_t *list
);

/*
    Close the iterators, rt_mem and rt_disk lists of a topic that belong to
    `creator`. An empty `creator` removes ALL of them; a non-empty `creator` with
    an empty `rt_id` removes all of that creator's; both set narrows to one id.
    No-op (returns 0) if the topic is not opened.
    (Parameter order is (creator, rt_id) — matches the implementation and all
    callers; the previous header listed them swapped.)
*/
PUBLIC int tranger2_close_all_lists(
    json_t *tranger,
    const char *topic_name,
    const char *creator,    // if empty, remove all
    const char *rt_id       // if empty, remove all lists of creator
);

/*
 *  Read a record's content: load the JSON message pointed to by `md_record_ex`
 *  (offset/size/__t__), for the case where only the metadata was loaded. Takes
 *  the TOPIC object (not tranger + name). The returned record has its
 *  "__md_tranger__" metadata attached; return is yours (decref it). NULL if
 *  topic/key/md_record_ex are missing or the payload cannot be read.
 *  NOTE: this returns the row even if its sf_deleted_instance tombstone is set —
 *  the skip only applies to iteration (see tranger2_delete_instance).
 */
PUBLIC json_t *tranger2_read_record_content( // return is yours
    json_t *tranger,
    json_t *topic,
    const char *key,
    md2_record_ex_t *md_record_ex
);

/*
 *  Format record metadata into caller buffer `bf` (bfsize):
 *    print_md0_record:      rowid, t, tm, key
 *    print_md1_record:      rowid, uflag, sflag, t, tm, key
 *    print_md2_record:      rowid, offset, size, t, path
 *    print_record_filename: the data-file path
 *  `print_local_time` selects local vs UTC for the t/tm timestamps
 *  (md0/md1 only; md2 and print_record_filename ignore it).
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

/*
   Set the tranger trace_level (stored on the handle; controls internal FS/dump
   tracing).
*/
PUBLIC void tranger2_set_trace_level(
    json_t *tranger,
    int trace_level
);

#ifdef __cplusplus
}
#endif
