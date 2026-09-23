/***********************************************************************
 *          TIMERANGER2.C
 *
 *          Time Ranger 2, a series time-key-value database over flat files
 *
 *          Copyright (c) 2017-2018 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>
#include <fnmatch.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>

#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <testing.h>
#include <gobj.h>
#include "fs_watcher.h"
#include "timeranger2.h"

extern void jsonp_free(void *ptr); // json low level

/***************************************************************
 *              Constants
 ***************************************************************/
PRIVATE const char *topic_fields[] = {
    "topic_name",
    "pkey",
    "tkey",
    "system_flag",
    "cols",
    "directory",
    "wr_fd_files",
    "rd_fd_files",
    "lists",
    "filename_mask",
    "xpermission",
    "rpermission",
    "marks_tm_unordered",

    0
};

PRIVATE const char *sf_names[16+1] = {
    "sf_string_key",            // 0x0001
    "sf_rowid_key",             // 0x0002
    "sf_int_key",               // 0x0004
    "",                         // 0x0008
    "sf_zip_record",            // 0x0010
    "sf_cipher_record",         // 0x0020
    "",                         // 0x0040
    "",                         // 0x0080
    "sf_t_ms",                  // 0x0100
    "sf_tm_ms",                 // 0x0200
    "sf_deleted_instance",      // 0x0400
    "sf_immutable_record",      // 0x0800

    // Non-inherited
    "sf_loading_from_disk",     // 0x1000
    "",                         // 0x2000
    "",                         // 0x4000
    "",                         // 0x8000
    0
};

/***************************************************************
 *              Structures
 ***************************************************************/
#pragma pack(1)

typedef struct { // Size: 32 bytes — fields are big-endian on disk
    uint64_t __t__;     // HACK: 16 high bits for user_flag, remain for time
    uint64_t __tm__;    // HACK: 16 high bits for system_flag, remain for time

    uint64_t __offset__;
    uint64_t __size__;
} md2_record_t;

#pragma pack()

#define TIME_FLAG_MASK  0x00000FFFFFFFFFFFULL  /* Maximum date: UTC 559444-03-08T09:40:15+0000 */
#define USER_FLAG_MASK  0x0FFFF00000000000ULL

static inline uint16_t get_user_flag(const md2_record_t *md_record) {
    return (uint16_t )((md_record->__t__ & USER_FLAG_MASK) >> 44);
}
static inline uint16_t get_system_flag(const md2_record_t *md_record) {
    return (uint16_t)((md_record->__tm__ & USER_FLAG_MASK) >> 44);
}
static inline uint64_t get_time_t(const md2_record_t *md_record) {
    return md_record->__t__ & TIME_FLAG_MASK;
}
static inline uint64_t get_time_tm(const md2_record_t *md_record) {
    return md_record->__tm__ & TIME_FLAG_MASK;
}

static inline void set_user_flag(md2_record_t *md_record, uint16_t user_flag_) {
    // Clear the user flag bits (44-59) in md_record->__t__
    md_record->__t__ &= ~USER_FLAG_MASK;

    // Set the new user flag by shifting it to the correct position and OR-ing it into __t__
    uint64_t user_flag = user_flag_;
    md_record->__t__ |= (user_flag & 0xFFFF) << 44;
}

static inline void set_system_flag(md2_record_t *md_record, uint16_t system_flag_) {
    // Clear the user flag bits (44-59) in md_record->__t__
    md_record->__tm__ &= ~USER_FLAG_MASK;

    // Set the new user flag by shifting it to the correct position and OR-ing it into __t__
    uint64_t system_flag = system_flag_;
    md_record->__tm__ |= (system_flag & 0xFFFF) << 44;
}

static inline BOOL is_deleted_instance(const md2_record_ex_t *md_record_ex) {
    return (md_record_ex->system_flag & sf_deleted_instance) != 0;
}


/***************************************************************
 *              Prototypes
 ***************************************************************/
typedef gbuffer_t * (*filter_callback_t) (   // Remember to free returned gbuffer
    void *user_data,  // topic user_data
    json_t *tranger,
    json_t *topic,
    gbuffer_t * gbuf  // must be owned
);

PRIVATE BOOL topic_name_is_confined(
    hgobj gobj,
    json_t *tranger,
    const char *topic_name,
    const char *caller
);
PRIVATE void add_iterator_to_topic(hgobj gobj, json_t *topic, json_t *iterator);
PRIVATE void mark_file_unordered(
    hgobj gobj,
    json_t *topic,
    const char *key,
    const char *file_id,
    json_t *cache_cell,
    const char *mark
);
PRIVATE void mark_file_before_append(
    hgobj gobj,
    json_t *topic,
    const char *key,
    const char *file_id,
    md2_record_t *md_record
);
PRIVATE int widen_cell_from_rows(
    hgobj gobj,
    const char *topic_directory,
    const char *key,
    const char *file_id,
    json_t *cache_cell,
    json_int_t from_row
);
PRIVATE void join_cell_ranges(json_t *cell, json_t *other);
PRIVATE void merge_cache_cell(json_t *cur_cache_cell, json_t *new_cache_cell);
PRIVATE void remove_iterator_from_index(json_t *topic, json_t *iterator);
PRIVATE int close_fd_opened_files(
    hgobj gobj,
    json_t *topic,
    const char *key
);
PRIVATE int close_fd_wr_files(
    hgobj gobj,
    json_t *topic,
    const char *key
);
PRIVATE int close_fd_rd_files(
    hgobj gobj,
    json_t *topic,
    const char *key
);

PRIVATE int json_array_find_idx(
    json_t *jn_list,
    json_t *item
);

PRIVATE int build_topic_cache_from_disk(
    hgobj gobj,
    json_t *topic
);
PRIVATE json_t *get_key_cache(
    json_t *topic,
    const char *key
);
PRIVATE json_t *create_cache_key(void);
PRIVATE void set_cache_int(json_t *dict, const char *key, json_int_t value);
PRIVATE json_t *find_cache_cell(
    json_t *topic,
    const char *key,
    const char *file_id,
    json_int_t *pfile_base,
    int *pinsert_idx
);
PRIVATE json_t *load_key_cache_from_disk(
    hgobj gobj,
    const char *topic_directory,
    const char *key
);
PRIVATE json_t *load_cache_cell_from_disk(
    hgobj gobj,
    const char *topic_directory,
    const char *key,
    char *filename, // md2 filename with extension, WARNING modified, .md2 removed
    json_t *known_cell  // the cell this file already has in memory, or NULL
);
PRIVATE json_int_t load_first_and_last_record_md(
    hgobj gobj,
    const char *topic_directory,
    const char *key,
    const char *filename,
    md2_record_t *md_first_record,
    md2_record_t *md_last_record
);

PRIVATE json_int_t update_new_record_from_mem(
    hgobj gobj,
    json_t *topic,
    const char *key,
    const char *file_id,    // the file the record was written to
    md2_record_t *md_record
);
PRIVATE json_int_t update_totals_of_key_cache(
    hgobj gobj,
    json_t *topic,
    const char *key
);
PRIVATE json_int_t update_totals_of_key_cache2(
    hgobj gobj,
    json_t *topic,
    const char *key,
    json_t *cache_cell,
    json_int_t rows_added
);

PRIVATE json_int_t get_topic_key_rows(hgobj gobj, json_t *topic, const char *key);

PRIVATE void *rkey_compile(hgobj gobj, const char *rkey);
PRIVATE int rkey_arm(hgobj gobj, json_t *list, const char *key, json_t *match_cond);
PRIVATE void rkey_disarm(json_t *list);
PRIVATE BOOL list_wants_key(json_t *list, const char *key);

PRIVATE json_t *get_cache_files(json_t *topic, const char *key);
PRIVATE json_t *get_cache_total(json_t *topic, const char *key);

PRIVATE BOOL match_cond_selects_records(json_t *match_cond);
PRIVATE json_t *build_iterator_index(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *segments,
    json_t *match_cond
);
PRIVATE json_t *segment_of_rowid(json_t *segments, json_int_t rowid);

PRIVATE json_t *get_segments(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *match_cond, // NOT owned but can be modified
    BOOL *realtime
);
PRIVATE int get_md_by_rowid( // Get record metadata by rowid
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *segment,
    uint64_t rowid, // relative to 1
    md2_record_ex_t *md_record_ex
);
PRIVATE int read_md(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id,
    uint64_t rowid, // relative to 1
    md2_record_ex_t *md_record_ex
);
PRIVATE json_t *read_record_content(
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id,
    md2_record_ex_t *md_record_ex
);
PRIVATE json_int_t first_segment_row(
    json_t *segments,
    json_t *cache_total,
    json_t *match_cond,  // not owned
    BOOL tm_known,
    json_int_t *rowid
);
PRIVATE json_int_t next_segment_row(
    json_t *segments,
    json_t *match_cond,  // not owned
    json_int_t cur_segment,
    json_int_t *rowid
);
PRIVATE BOOL segment_t_ordered(json_t *segment);
PRIVATE BOOL topic_marks_tm(json_t *topic);
PRIVATE BOOL segment_tm_ordered(json_t *topic, json_t *segment);
PRIVATE json_int_t leave_segment_row(json_t *segment, BOOL backward);
PRIVATE json_t *key_cache_stamp(json_t *topic, const char *key);
PRIVATE void forget_segments_of_key(json_t *topic, const char *key);
PRIVATE BOOL tranger2_match_metadata(
    json_t *match_cond,
    json_int_t total_rows,
    json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    BOOL t_ordered,
    BOOL tm_ordered,
    BOOL *end,
    BOOL *end_segment
);
PRIVATE fs_event_t *monitor_disks_directory_by_master(
    hgobj gobj,
    yev_loop_h yev_loop,
    json_t *tranger,
    json_t *topic
);
PRIVATE int master_fs_callback(fs_event_t *fs_event);
PRIVATE fs_event_t *monitor_rt_disk_by_client(
    hgobj gobj,
    yev_loop_h yev_loop,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *id
);
PRIVATE int client_fs_callback(fs_event_t *fs_event);
PRIVATE int master_to_update_client_load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list, // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    json_t *record      // must be owned
);
PRIVATE json_int_t update_new_records_from_disk(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    char *filename,
    const char *rt_id
);
PRIVATE json_int_t publish_new_rt_disk_records(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *old_file_cache,
    json_t *new_file_cache,
    json_int_t file_base,
    const char *rt_id
);

PRIVATE int update_key_by_hard_link(
    hgobj gobj,
    json_t *tranger,
    char *path // WARNING path modified
);

PRIVATE int scan_disks_key_for_new_file(
    hgobj gobj,
    json_t *tranger,
    char *path
);

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *  Startup TimeRanger database
 ***************************************************************************/
PUBLIC json_t *tranger2_startup(
    hgobj gobj,
    json_t *jn_tranger, // owned, See tranger2_json_desc for parameters
    yev_loop_h yev_loop
)
{
    json_t *tranger = create_json_record(gobj, tranger2_json_desc); // no master by default
    json_object_update_existing(tranger, jn_tranger);
    json_object_set_new(tranger, "gobj", json_integer((json_int_t)(uintptr_t)gobj));
    JSON_DECREF(jn_tranger)

    char path[PATH_MAX];
    if(1) {
        const char *path_ = kw_get_str(gobj, tranger, "path", "", KW_REQUIRED);
        if(empty_string(path_)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Cannot startup TimeRanger. What path?",
                NULL
            );
            json_decref(tranger);
            return 0;
        }
        build_path(path, sizeof(path), path_, "", NULL); // I want to modify the path
    }

    const char *database = kw_get_str(gobj, tranger, "database", "", KW_REQUIRED);
    if(empty_string(database)) {
        database = pop_last_segment(path);
        if(empty_string(database)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Cannot startup TimeRanger. What database?",
                NULL
            );
            json_decref(tranger);
            return 0;
        }
        json_object_set_new(tranger, "path", json_string(path));
        json_object_set_new(tranger, "database", json_string(database));
    }

    char directory[PATH_MAX];
    build_path(directory, sizeof(directory), path, database, NULL);
    kw_set_dict_value(gobj, tranger, "directory", json_string(directory));

    /*-------------------------------------*
     *  Build database directory and
     *  __timeranger2__.json metadata file
     *-------------------------------------*/
    log_opt_t on_critical_error = kw_get_int(
        gobj,
        tranger,
        "on_critical_error",
        0,
        KW_REQUIRED
    );
    BOOL master = kw_get_bool(
        gobj,
        tranger,
        "master",
        0,
        KW_REQUIRED|KW_WILD_NUMBER
    );

    int fd = -1;
    if(file_exists(directory, "__timeranger2__.json")) {
        /*
         *  Pass on_critical_error here, NOT LOG_NONE — this is the single-master
         *  guard, and the level and the exit are the SAME knob. When a store is
         *  already locked, this exclusive probe fails and logs a CRITICAL; with
         *  the default on_critical_error == 2 (LOG_OPT_EXIT_ZERO) that CRITICAL
         *  calls exit(0) *inside the log*, before the non-master fallback below
         *  is even reached. exit(0) is deliberate: the watcher does not relaunch
         *  a clean exit, so a second instance that loses the lock stays down and
         *  exactly one master owns the store. Silencing this probe (LOG_NONE
         *  drops both the log and the EXIT_ZERO bit) would let that second
         *  instance fall through and run as a rogue non-master. The fallback is
         *  reached only for stores configured on_critical_error == 0 (read-only
         *  replicas that are meant to run non-master); test_tranger_startup
         *  drives it with LOG_OPT_TRACE_STACK so it can exercise that path
         *  without exiting.
         */
        json_t *jn_disk_tranger = load_persistent_json(
            gobj,
            directory,
            "__timeranger2__.json",
            on_critical_error,
            &fd,
            master? TRUE:FALSE, //exclusive
            TRUE // silence
        );
        if(!jn_disk_tranger) {
            // If can't open in exclusive mode then be a not master
            jn_disk_tranger = load_persistent_json(
                gobj,
                directory,
                "__timeranger2__.json",
                on_critical_error,
                &fd,
                FALSE, // exclusive
                TRUE // silence
            );
            if(jn_disk_tranger) {
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TRANGER,
                    "msg",          "%s", "Open as not master, __timeranger2__.json locked",
                    "path",         "%s", directory,
                    NULL
                );
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TRANGER,
                    "msg",          "%s", "Cannot open __timeranger__",
                    "path",         "%s", directory,
                    NULL
                );
                json_decref(tranger);
                return 0;
            }
            master = FALSE;
            json_object_set_new(tranger, "master", json_false());
        }
        json_object_update_existing(tranger, jn_disk_tranger);
        json_decref(jn_disk_tranger);

    } else { // __timeranger2__.json not exist
        if(!master) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Cannot startup TimeRanger. __timeranger2__.json not found and not master",
                "path",         "%s", directory,
                NULL
            );
            json_decref(tranger);
            return 0;
        }
        /*
         *  I'm MASTER and  __timeranger2__.json not exist, create it
         */
        const char *filename_mask = kw_get_str(gobj, tranger, "filename_mask", "%Y-%m-%d", KW_REQUIRED);
        int xpermission = (int)kw_get_int(gobj, tranger, "xpermission", 02770, KW_REQUIRED);
        int rpermission = (int)kw_get_int(gobj, tranger, "rpermission", 0660, KW_REQUIRED);

        json_t *jn_tranger_ = json_object();
        kw_get_str(gobj, jn_tranger_, "filename_mask", filename_mask, KW_CREATE);
        kw_get_int(gobj, jn_tranger_, "rpermission", rpermission, KW_CREATE);
        kw_get_int(gobj, jn_tranger_, "xpermission", xpermission, KW_CREATE);
        save_json_to_file(
            gobj,
            directory,
            "__timeranger2__.json",
            xpermission,
            rpermission,
            on_critical_error,
            TRUE,   //create
            TRUE,  //only_read
            jn_tranger_  // owned
        );
        // Re-open
        json_t *jn_disk_tranger = load_persistent_json(
            gobj,
            directory,
            "__timeranger2__.json",
            on_critical_error,
            &fd,
            TRUE, //exclusive
            TRUE // silence
        );
        if(!jn_disk_tranger) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TRANGER,
                "msg",          "%s", "Cannot create __timeranger2__.json",
                "path",         "%s", directory,
                NULL
            );
            json_decref(tranger);
            return 0;
        }

        json_object_update_existing(tranger, jn_disk_tranger);
        json_decref(jn_disk_tranger);

        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Creating __timeranger2__.json",
            "path",         "%s", directory,
            NULL
        );
    }

    /*
     *  Load Only read, volatil, defining in run-time
     */
    kw_set_dict_value(gobj, tranger, "fd_opened_files", json_object());
    kw_set_dict_value(gobj, tranger, "topics", json_object());
    kw_set_subdict_value(gobj, tranger, "fd_opened_files", "__timeranger2__.json", json_integer(fd));
    kw_set_dict_value(gobj, tranger, "yev_loop", json_integer((json_int_t)(uintptr_t)yev_loop));

    return tranger;
}

/***************************************************************************
 *  Close TimeRanger database
 *  Close topics without remove memory to give time to close yev_events
 ***************************************************************************/
PUBLIC int tranger2_stop(json_t *tranger)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    const char *key;
    json_t *jn_value;
    void *temp;
    json_t *jn_topics = kw_get_dict(gobj, tranger, "topics", 0, KW_REQUIRED);
    json_object_foreach_safe(jn_topics, temp, key, jn_value) {
        tranger2_close_topic(tranger, key);
    }

    /*
     *  A closed fd is marked -1: the number is free for anybody from now
     *  on, and a second stop closed whatever took it.
     */
    json_t *opened_files = kw_get_dict(gobj, tranger, "fd_opened_files", 0, KW_REQUIRED);
    json_object_foreach(opened_files, key, jn_value) {
        int fd = (int)kw_get_int(gobj, opened_files, key, 0, KW_REQUIRED);
        if(fd >= 0) {
            close(fd);
        }
        json_object_set_new(opened_files, key, json_integer(-1));
    }
    json_object_set_new(tranger, "__closed__", json_true());

    return 0;
}

/***************************************************************************
 *  A tranger used again after tranger2_stop() is alive again: its shutdown
 *  must close what it opens from now on. And a master gave its lock back
 *  at the stop, so it takes it again, or it is not the master any more --
 *  another process may have taken the store meanwhile.
 *
 *  That is the startup's single-master conflict, and it is answered the
 *  same way, with the same knob: a CRITICAL at `on_critical_error`. With a
 *  yuno's default (LOG_OPT_EXIT_ZERO) the process exits(0) inside the log
 *  and stays down, like a second instance at the startup; the other process
 *  owns the store. A tranger configured to survive a critical goes on as a
 *  replica: `master` false, and `master_lost` true to say why. It reads,
 *  and every write refuses (tranger_is_master).
 *
 *  A demoted master never takes the lock again, not even once it is free:
 *  what it holds in memory (the caches of its topics, the lists a treedb
 *  opened as master, rt_mem and not rt_disk) did not follow what the other
 *  master wrote meanwhile, and appending on top of it would number rows
 *  that exist. To be the master again it is shut down and started again
 *  (tranger2_shutdown() + tranger2_startup()), which reads the store anew.
 *
 *  Only the lock is taken: the tranger's settings were read at the startup.
 ***************************************************************************/
PRIVATE void revive_stopped_tranger(hgobj gobj, json_t *tranger)
{
    if(!json_is_true(json_object_get(tranger, "__closed__"))) {
        return;     // on the hot path of every append: no kw path lookup
    }
    json_object_del(tranger, "__closed__");

    if(!json_boolean_value(json_object_get(tranger, "master"))) {
        return;
    }

    const char *directory = kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED);
    char path[PATH_MAX];
    build_path(path, sizeof(path), directory, "__timeranger2__.json", NULL);

    log_opt_t on_critical_error = (log_opt_t)kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED);

    int fd = open(path, O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(fd < 0) {
        gobj_log_critical(gobj, on_critical_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TRANGER,
            "msg",          "%s", "Master lock NOT retaken after a stop: cannot open the lock file, go on as not master",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        json_object_set_new(tranger, "master", json_false());
        json_object_set_new(tranger, "master_lost", json_true());
        return;
    }
    if(flock(fd, LOCK_EX|LOCK_NB) < 0) {
        int err = errno;
        close(fd);
        gobj_log_critical(gobj, on_critical_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TRANGER,
            "msg",          "%s", (err == EWOULDBLOCK)?
                "Master lock NOT retaken after a stop: another process holds it, go on as not master" :
                "Master lock NOT retaken after a stop: flock() FAILED, go on as not master",
            "path",         "%s", path,
            "errno",        "%d", err,
            "serrno",       "%s", strerror(err),
            NULL
        );
        json_object_set_new(tranger, "master", json_false());
        json_object_set_new(tranger, "master_lost", json_true());
        return;
    }
    kw_set_subdict_value(gobj, tranger, "fd_opened_files", "__timeranger2__.json", json_integer(fd));
}

/***************************************************************************
 *  May this tranger write? Asked by every write path BEFORE it writes: a
 *  stopped master takes its lock again first (revive_stopped_tranger), and
 *  the answer is what it holds NOW. It was read from the `master` of the
 *  stop, and tranger2_create_topic() wrote the topic files into the store
 *  of the process that had taken it meanwhile (M3 of the 2026-09-23
 *  independent review).
 ***************************************************************************/
PRIVATE BOOL tranger_is_master(hgobj gobj, json_t *tranger)
{
    revive_stopped_tranger(gobj, tranger);
    return json_boolean_value(json_object_get(tranger, "master"))? TRUE: FALSE;
}

/***************************************************************************
 *  Shutdown TimeRanger database
 ***************************************************************************/
PUBLIC int tranger2_shutdown(json_t *tranger)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    BOOL __closed__ = kw_get_bool(gobj, tranger, "__closed__", 0, 0);
    if(!__closed__) {
        tranger2_stop(tranger);
    }
    JSON_DECREF(tranger)
    return 0;
}

/***************************************************************************
   Convert string (..|..|...) to system_flag_t integer
 ***************************************************************************/
PUBLIC system_flag2_t tranger2_str2system_flag(const char *system_flag)
{
    uint32_t bitmask = 0;

    int list_size;
    const char **names = split2(system_flag, "|, ", &list_size);

    for(int i=0; i<list_size; i++) {
        const char *name = *(names +i);
        int idx = idx_in_list(sf_names, name, TRUE);
        if(idx < 0) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Unknown system_flag name, ignored",
                "system_flag",  "%s", system_flag,
                "name",         "%s", name,
                NULL
            );
            continue;
        }
        bitmask |= 1 << idx;    // sf_names[idx] is bit (1 << idx)
    }

    split_free2(names);

    return bitmask;
}

/***************************************************************************
 *  The order of the columns is part of a schema: it is the order a table
 *  paints them in, and the order a form asks for them.
 *
 *  topic_cols.json is deliberately frozen until `topic_version` rises -- the
 *  persisted file is what a topic opens with, and a change to WHAT a column
 *  declares has to announce itself with a version, or the two schemas drift
 *  apart in silence. A change to the ORDER declares nothing new: same
 *  columns, same content, different sequence. That one is let through, so an
 *  order fixed at the source reaches a store that already exists without
 *  every topic in the tree having to bump its version.
 *
 *  Return TRUE when only the order moved.
 ***************************************************************************/
PRIVATE BOOL only_the_order_moved(
    json_t *stored_cols,    // not owned
    json_t *new_cols        // not owned
)
{
    if(!json_is_object(stored_cols) || !json_is_object(new_cols)) {
        return FALSE;
    }
    if(json_object_size(stored_cols) != json_object_size(new_cols)) {
        return FALSE;
    }

    const char *col_name; json_t *col;
    json_object_foreach(new_cols, col_name, col) {
        json_t *stored_col = json_object_get(stored_cols, col_name);
        if(!stored_col || !json_equal(stored_col, col)) {
            return FALSE;
        }
    }

    /*
     *  Same columns saying the same things: only the sequence can differ.
     */
    void *new_iter = json_object_iter(new_cols);
    void *stored_iter = json_object_iter(stored_cols);
    while(new_iter && stored_iter) {
        if(strcmp(
            json_object_iter_key(new_iter),
            json_object_iter_key(stored_iter)
        )!=0) {
            return TRUE;
        }
        new_iter = json_object_iter_next(new_cols, new_iter);
        stored_iter = json_object_iter_next(stored_cols, stored_iter);
    }

    return FALSE;
}

/***************************************************************************
 *  A topic name is ONE directory component under the database, and it is
 *  also a segment of the backtick kw paths (`topics`<name>`cols`). Refuse
 *  what could escape the database or split a path: empty, "." and "..",
 *  '/' and '`'. Unlike a key, a leading '.' is legit: MQTT queues are
 *  "<client_id>-IN/-OUT" and the broker accepts a client_id such as ".foo".
 ***************************************************************************/
PRIVATE BOOL name_escapes_its_directory(const char *name)
{
    if(empty_string(name) ||
       strcmp(name, ".")==0 ||
       strcmp(name, "..")==0 ||
       strchr(name, '/') != NULL) {
        return TRUE;
    }
    return FALSE;
}

PRIVATE BOOL topic_name_is_confined(
    hgobj gobj,
    json_t *tranger,
    const char *topic_name,
    const char *caller
)
{
    if(name_escapes_its_directory(topic_name) || strchr(topic_name, '`') != NULL) {
        gobj_log_error(gobj, 0,
            "function",     "%s", caller,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Invalid topic name (path metacharacters not allowed)",
            "database",     "%s", kw_get_str(gobj, tranger, "directory", "", 0),
            "topic_name",   "%s", topic_name?topic_name:"",
            NULL
        );
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************
 *  The id of a disk feed is a directory too, `<topic>/disks/<id>/`, and
 *  the follower rmrdir()s it before creating it: the directory half of the
 *  topic-name rule, or an id from the wire removes whatever it points at.
 *  And one component: longer than NAME_MAX, the mkdir fails and the feed
 *  answered "opened" while it could never receive anything.
 *  A backtick is fine here: an rt id is not a segment of any kw path, and
 *  treedb names its own feeds `<treedb>`<topic>`<id>`.
 *
 *  The id may come from a peer (open-rt / open-list of C_TRANGER): a
 *  refusal is a WARNING, with no stack -- nothing of ours is broken.
 ***************************************************************************/
PRIVATE BOOL rt_id_is_confined(
    hgobj gobj,
    json_t *topic,
    const char *id,
    const char *caller
)
{
    if(empty_string(id)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", caller,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Invalid rt id (empty)",
            "topic_name",   "%s", tranger2_topic_name(topic),
            NULL
        );
        gobj_log_set_last_message("Invalid rt id: empty");
        return FALSE;
    }
    if(name_escapes_its_directory(id)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", caller,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Invalid rt id (path metacharacters not allowed)",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "id",           "%s", id?id:"",
            NULL
        );
        gobj_log_set_last_message("Invalid rt id '%s'", id?id:"");
        return FALSE;
    }
    if(strlen(id) > NAME_MAX) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", caller,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Invalid rt id (longer than NAME_MAX)",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "length",       "%d", (int)strlen(id),
            "max",          "%d", NAME_MAX,
            NULL
        );
        gobj_log_set_last_message("Invalid rt id: longer than %d", NAME_MAX);
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************
 *  Replace a json file of a topic WHOLE, through a temporary file and a
 *  rename(): at every instant the file on disk is the old one or the new
 *  one, never a truncated one.
 *
 *  The temporary `<filename>.new` is unlinked first and created
 *  O_EXCL|O_NOFOLLOW with the tranger's rpermission (0440 when
 *  `only_read`): a `.new` left behind by a process that died is not
 *  reused, so it lends the file neither its mode nor its owner, and one
 *  that is a symlink is not followed (independent review of the second
 *  fix round, repro r_var: O_TRUNC did both).
 *
 *  `durable` also fsyncs the temporary file before the rename and the
 *  directory after it, so the new file survives a power cut too. It costs
 *  two disk flushes: it is asked where the file changes rarely, not on the
 *  hot path of treedb's rowid counter (every create of a rowid-key node),
 *  which is safe against the death of the process and not against a power
 *  cut.
 *
 *  Return 0, or -1 (logged) with the old file untouched.
 ***************************************************************************/
PRIVATE int replace_json_file(
    hgobj gobj,
    json_t *tranger,
    const char *directory,
    const char *filename,
    json_t *jn,             // NOT owned
    BOOL durable,
    BOOL only_read
)
{
    char name_new[NAME_MAX];
    if(snprintf(name_new, sizeof(name_new), "%s.new", filename) >= (int)sizeof(name_new)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Cannot replace a topic file, its name is too long",
            "directory",    "%s", directory,
            "filename",     "%s", filename,
            NULL
        );
        return -1;
    }
    char path_new[PATH_MAX];
    char path[PATH_MAX];
    if(!build_path(path_new, sizeof(path_new), directory, name_new, NULL) ||
            !build_path(path, sizeof(path), directory, filename, NULL)) {
        return -1;  // Error already logged
    }

    char msg[NAME_MAX + 64];
    if(unlink(path_new) < 0 && errno != ENOENT) {
        snprintf(msg, sizeof(msg), "Cannot replace %s, cannot remove a temporary file left behind", filename);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", msg,
            "path",         "%s", path_new,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    int mode = only_read? 0440 : (int)kw_get_int(gobj, tranger, "rpermission", 0, KW_REQUIRED);
    int fd = open(path_new, O_CREAT|O_EXCL|O_RDWR|O_NOFOLLOW|O_CLOEXEC, mode);
    if(fd < 0) {
        snprintf(msg, sizeof(msg), "Cannot replace %s, cannot create the temporary file", filename);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", msg,
            "path",         "%s", path_new,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    const char *failed = NULL;
    if(fchmod(fd, (mode_t)mode) < 0) {
        failed = "fchmod() FAILED";     // the umask of the process took bits off
    } else if(json_dumpfd(jn, fd, JSON_INDENT(4)) < 0) {
        failed = "write FAILED";
    } else if(durable && fsync(fd) < 0) {
        failed = "fsync() FAILED";
    }
    int err = errno;
    if(close(fd) < 0 && !failed) {
        failed = "close() FAILED";
        err = errno;
    }
    if(!failed && rename(path_new, path) < 0) {
        failed = "rename() FAILED";
        err = errno;
    }
    if(failed) {
        snprintf(msg, sizeof(msg), "Cannot replace %s, %s", filename, failed);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", msg,
            "path",         "%s", path,
            "errno",        "%d", err,
            "serrno",       "%s", strerror(err),
            NULL
        );
        unlink(path_new);
        return -1;
    }

    if(durable) {
        int dir_fd = open(directory, O_RDONLY|O_DIRECTORY|O_CLOEXEC);
        if(dir_fd < 0 || fsync(dir_fd) < 0) {
            snprintf(msg, sizeof(msg), "%s replaced, but its directory cannot be fsync'ed", filename);
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", msg,
                "path",         "%s", directory,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
        if(dir_fd >= 0) {
            close(dir_fd);
        }
    }
    return 0;
}

/***************************************************************************
 *  Replace a topic's topic_var.json WHOLE (no merge with what it held),
 *  see replace_json_file(): a process that dies half way loses neither the
 *  topic_version nor the `last_rowid_id` counter treedb keeps there.
 *
 *  The topic in memory takes the new values only when the file did.
 ***************************************************************************/
PRIVATE int replace_topic_var(
    hgobj gobj,
    json_t *tranger,
    const char *directory,
    const char *topic_name,
    json_t *jn_topic_var,  // owned
    BOOL durable
)
{
    if(replace_json_file(gobj, tranger, directory, "topic_var.json", jn_topic_var, durable, FALSE) < 0) {
        JSON_DECREF(jn_topic_var)
        return -1;  // Error already logged
    }

    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, 0);
    if(topic) {
        kw_update_except(gobj, topic, jn_topic_var, topic_fields); // data from topic disk are inmutable!
    }
    JSON_DECREF(jn_topic_var)
    return 0;
}

/***************************************************************************
   Create topic if not exist. Alias create table.
   HACK IDEMPOTENT function
 ***************************************************************************/
PUBLIC json_t *tranger2_create_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,    // If the topic exists then only needs (tranger, topic_name) parameters
    const char *topic_name,
    const char *pkey,
    const char *tkey,
    json_t *jn_topic_ext, // owned, See topic_json_desc for parameters, overwrite certain tranger params.
    system_flag2_t system_flag,
    json_t *jn_cols,    // owned
    json_t *jn_var      // owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    BOOL master = tranger_is_master(gobj, tranger);

    /*-------------------------------*
     *      Some checks
     *-------------------------------*/
    if(!jn_cols) {
        jn_cols = json_object();
    }
    if(!jn_var) {
        jn_var = json_object();
    }
    if(!pkey) {
        pkey = "";
    }
    if(!tkey) {
        tkey = "";
    }
    if(!topic_name_is_confined(gobj, tranger, topic_name, __FUNCTION__)) {
        JSON_DECREF(jn_cols)
        JSON_DECREF(jn_var)
        JSON_DECREF(jn_topic_ext)
        return 0;
    }
    json_int_t topic_new_version = kw_get_int(gobj, jn_var, "topic_version", 0, KW_WILD_NUMBER);
    json_int_t topic_old_version = 0;

    /*-------------------------------*
     *      Check directory
     *-------------------------------*/
    char directory[PATH_MAX-30];
    snprintf(
        directory,
        sizeof(directory),
        "%s/%s",
        kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
        topic_name
    );

    if(!is_directory(directory)) {
        /*-------------------------------*
         *  Create topic if master
         *-------------------------------*/
        if(!master) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "directory",    "%s", directory,
                "msg",          "%s", "Cannot open TimeRanger topic. Not found and no master",
                NULL
            );
            JSON_DECREF(jn_cols)
            JSON_DECREF(jn_var)
            JSON_DECREF(jn_topic_ext)
            return 0;
        }

        system_flag2_t system_flag_key_type = system_flag & KEY_TYPE_MASK2;
        if(system_flag_key_type & sf_rowid_key) {
            pkey = "__rowid__";
        }

        if(empty_string(pkey)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "database",     "%s", kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
                "msg",          "%s", "tranger_create_topic(): What pkey?",
                NULL
            );
            JSON_DECREF(jn_cols)
            JSON_DECREF(jn_var)
            JSON_DECREF(jn_topic_ext)
            return 0;
        }

        if(mkrdir(directory, (int)kw_get_int(gobj, tranger, "xpermission", 0, KW_REQUIRED))<0) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "path",         "%s", directory,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot create TimeRanger subdir. mkrdir() FAILED",
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }

        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Creating topic",
            "topic",        "%s", topic_name,
            "path",         "%s", directory,
            NULL
        );

        /*----------------------------------------*
         *      Create topic_desc.json
         *----------------------------------------*/
        json_t *jn_topic_desc = json_object();
        kw_get_str(gobj, jn_topic_desc, "topic_name", topic_name, KW_CREATE);
        kw_get_str(gobj, jn_topic_desc, "pkey", pkey, KW_CREATE);
        kw_get_str(gobj, jn_topic_desc, "tkey", tkey, KW_CREATE);

        if(!system_flag_key_type) {
            if(!empty_string(pkey)) {
                system_flag |= sf_string_key;   // set default
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "database",     "%s", kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
                    "msg",          "%s", "tranger_create_topic(): What key type?",
                    NULL
                );
                JSON_DECREF(jn_cols)
                JSON_DECREF(jn_var)
                JSON_DECREF(jn_topic_ext)
                return 0;
            }
        }
        kw_get_int(gobj, jn_topic_desc, "system_flag", system_flag, KW_CREATE);

        if(jn_topic_ext) {
            json_t *jn_topic_ext_ = create_json_record(gobj, topic_json_desc); // no master by default
            json_object_update_existing(jn_topic_ext_, jn_topic_ext);
            json_object_update(jn_topic_desc, jn_topic_ext_);
            JSON_DECREF(jn_topic_ext_)
        }

        /*
         *  A topic created from now on marks every md2 file whose tm goes
         *  back (`<file>.tm_unordered`): the tm range of each of its files
         *  can be trusted. A topic without this key was written before the
         *  marks, and no tm range of it is (see topic_marks_tm).
         */
        json_object_set_new(jn_topic_desc, "marks_tm_unordered", json_true());

        json_t *topic_desc = kw_clone_by_path(
            gobj,
            jn_topic_desc, // owned
            topic_fields
        );
        save_json_to_file(
            gobj,
            directory,
            "topic_desc.json",
            (int)kw_get_int(gobj, tranger, "xpermission", 0, KW_REQUIRED),
            (int)kw_get_int(gobj, tranger, "rpermission", 0, KW_REQUIRED),
            kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
            master? TRUE:FALSE, //create
            TRUE,  //only_read
            topic_desc  // owned
        );

        if(gobj_global_trace_level() & TRACE_FS) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Creating topic_desc.json",
                "path",         "%s", directory,
                "topic",        "%s", topic_name,
                NULL
            );
        }

        /*----------------------------------------*
         *      Create topic_cols.json
         *----------------------------------------*/
        JSON_INCREF(jn_cols)
        tranger2_write_topic_cols(
            tranger,
            topic_name,
            jn_cols
        );
        if(gobj_global_trace_level() & TRACE_FS) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Creating topic_cols.json",
                "path",         "%s", directory,
                "topic",        "%s", topic_name,
                NULL
            );
        }

        /*----------------------------------------*
         *      Create topic_var.json
         *----------------------------------------*/
        JSON_INCREF(jn_var)
        tranger2_write_topic_var(
            tranger,
            topic_name,
            jn_var
        );
        if(gobj_global_trace_level() & TRACE_FS) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Creating topic_var.json",
                "path",         "%s", directory,
                "topic",        "%s", topic_name,
                NULL
            );
        }

        /*----------------------------------------*
         *      Create data directory
         *----------------------------------------*/
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/keys",
            directory
        );
        if(mkrdir(full_path, (int)kw_get_int(gobj, tranger, "xpermission", 0, KW_REQUIRED))<0) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "path",         "%s", full_path,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot create TimeRanger subdir. mkrdir() FAILED",
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }

        /*----------------------------------------*
         *      Create data directory
         *----------------------------------------*/
        snprintf(full_path, sizeof(full_path), "%s/disks",
            directory
        );
        if(mkrdir(full_path, (int)kw_get_int(gobj, tranger, "xpermission", 0, KW_REQUIRED))<0) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "path",         "%s", full_path,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot create TimeRanger subdir. mkrdir() FAILED",
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }

    } else if (master) {
        /*---------------------------------------------*
         *  Exists the directory but check
         *      topic_var.json (USE TO CHANGE VERSION)
         *      topic_version
         *      topic_cols.json
         *---------------------------------------------*/
        snprintf(
            directory,
            sizeof(directory),
            "%s/%s",
            kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
            topic_name
        );

        BOOL version_changed = FALSE;
        json_int_t last_rowid_id = 0;
        if(topic_new_version) {
            /*----------------------------------------*
             *      Check topic_version
             *----------------------------------------*/
            json_t *topic_var = load_json_from_file(
                gobj,
                directory,
                "topic_var.json",
                0
            );
            topic_old_version = kw_get_int(gobj, topic_var, "topic_version", 0, KW_WILD_NUMBER);
            last_rowid_id = kw_get_int(gobj, topic_var, "last_rowid_id", 0, 0);
            JSON_DECREF(topic_var)

            /*
             *  A version that goes UP publishes a change. One that goes DOWN
             *  does too while a treedb imposes its schema from C (see
             *  treedb_open_db, option "impose"): the newer stored topic is
             *  then a change being reverted, not one to keep.
             */
            BOOL imposing = json_boolean_value(json_object_get(tranger, "__schema_imposing__"));
            if(topic_new_version > topic_old_version ||
                (imposing && topic_new_version < topic_old_version)
            ) {
                if(topic_new_version < topic_old_version) {
                    gobj_log_info(gobj, 0,
                        "function",         "%s", __FUNCTION__,
                        "msgset",           "%s", MSGSET_INFO,
                        "msg",              "%s", "Imposing topic_version from C over a newer one",
                        "database",         "%s", kw_get_str(gobj, tranger, "database", "", KW_REQUIRED),
                        "topic",            "%s", topic_name,
                        "topic_version",    "%d", (int)topic_new_version,
                        "stored_version",   "%d", (int)topic_old_version,
                        NULL
                    );
                }
                file_remove(directory, "topic_cols.json");
                version_changed = TRUE;
            }
        }

        if(version_changed) {
            /*----------------------------------------*
             *  Replace topic_var.json
             *
             *  The version change re-creates topic_var.json from the
             *  schema, so a key the schema no longer carries (pkey2s)
             *  goes away with it. `last_rowid_id` is not schema: it is the
             *  counter treedb keeps there so a rowid id is never handed out
             *  twice, and re-seeding it from the ids still alive would hand
             *  out the deleted highest one again. It survives the change.
             *  The file is REPLACED, never removed first: a process dying
             *  between a remove and the write-back lost the counter.
             *----------------------------------------*/
            json_t *new_var = json_deep_copy(jn_var);
            if(last_rowid_id > 0) {
                json_object_set_new(new_var, "last_rowid_id", json_integer(last_rowid_id));
            }
            if(replace_topic_var(gobj, tranger, directory, topic_name, new_var, TRUE)==0) {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "Re-Creating topic_var.json",
                    "database",     "%s", kw_get_str(gobj, tranger, "database", "", KW_REQUIRED),
                    "topic",        "%s", topic_name,
                    NULL
                );
            } else {
                // Error already logged: the old topic_var.json is still there
            }

        } else if(!file_exists(directory, "topic_var.json")) {
            /*----------------------------------------*
             *      Create topic_var.json
             *----------------------------------------*/
            JSON_INCREF(jn_var)
            if(tranger2_write_topic_var(
                tranger,
                topic_name,
                jn_var
            )==0) {
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "Re-Creating topic_var.json",
                    "database",     "%s", kw_get_str(gobj, tranger, "database", "", KW_REQUIRED),
                    "topic",        "%s", topic_name,
                    NULL
                );
            } else {
                // Error already logged
            }
        }


        if(!file_exists(directory, "topic_cols.json")) {
            /*----------------------------------------*
             *      Create topic_cols.json
             *----------------------------------------*/
            JSON_INCREF(jn_cols);
            tranger2_write_topic_cols(
                tranger,
                topic_name,
                jn_cols
            );
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Re-Creating topic_cols.json",
                "database",     "%s", kw_get_str(gobj, tranger, "database", "", KW_REQUIRED),
                "topic",        "%s", topic_name,
                NULL
            );
        } else {
            /*----------------------------------------*
             *      Re-order topic_cols.json
             *----------------------------------------*/
            json_t *stored_cols = load_json_from_file(
                gobj,
                directory,
                "topic_cols.json",
                0
            );
            if(only_the_order_moved(stored_cols, jn_cols)) {
                JSON_INCREF(jn_cols);
                tranger2_write_topic_cols(
                    tranger,
                    topic_name,
                    jn_cols
                );
                gobj_log_info(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INFO,
                    "msg",          "%s", "Re-ordering topic_cols.json",
                    "database",     "%s", kw_get_str(gobj, tranger, "database", "", KW_REQUIRED),
                    "topic",        "%s", topic_name,
                    NULL
                );
            }
            JSON_DECREF(stored_cols)
        }
    }

    JSON_DECREF(jn_cols)
    JSON_DECREF(jn_var)
    JSON_DECREF(jn_topic_ext)

    return tranger2_open_topic(tranger, topic_name, TRUE);
}

/***************************************************************************
   Open topic
 ***************************************************************************/
PUBLIC json_t *tranger2_open_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,
    const char *topic_name,
    BOOL verbose
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*-------------------------------*
     *      Some checks
     *-------------------------------*/
    if(!topic_name_is_confined(gobj, tranger, topic_name, __FUNCTION__)) {
        return NULL;
    }

    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, 0);
    if(topic) {
        return topic;
    }

    /*-------------------------------*
     *      Check directory
     *-------------------------------*/
    char directory[PATH_MAX];
    snprintf(
        directory,
        sizeof(directory),
        "%s/%s",
        kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
        topic_name
    );

    if(!is_directory(directory)) {
        if(verbose) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "tranger_open_topic(): directory not found",
                "directory",    "%s", kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
                NULL
            );
        }
        return NULL;
    }

    /*
     *  A directory that is not a topic is not a critical: the name may come
     *  from a peer, and with on_critical_error=2 the critical is an exit(0).
     */
    if(!file_exists(directory, "topic_desc.json")) {
        if(verbose) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Not a topic: topic_desc.json not found",
                "directory",    "%s", directory,
                NULL
            );
        }
        return NULL;
    }

    /*-------------------------------*
     *      Load topic files
     *-------------------------------*/
    /*
     *  topic_desc
     */
    topic = load_persistent_json(
        gobj,
        directory,
        "topic_desc.json",
        0,      // a failed READ never exits: the name may come from a peer
        0,
        FALSE, // exclusive
        FALSE // silence
    );
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open topic: topic_desc.json does not load",
            "directory",    "%s", directory,
            NULL
        );
        return NULL;
    }

    /*
     *  topic_var
     */
    json_t *topic_var = load_json_from_file(
        gobj,
        directory,
        "topic_var.json",
        0
    );

    kw_update_except(gobj, topic, topic_var, topic_fields); // data from topic disk are inmutable!
    json_decref(topic_var);

    /*
     *  topic_cols
     */
    json_t *topic_cols = load_json_from_file(
        gobj,
        directory,
        "topic_cols.json",
        0
    );
    json_object_set_new(
        topic,
        "cols",
        topic_cols
    );

    /*
     *  Add the topic to topics
     */
    revive_stopped_tranger(gobj, tranger);
    kw_set_subdict_value(gobj, tranger, "topics", topic_name, topic);

    /*
     *  Load volatil, defining in run-time
     */
    kw_get_str(gobj, topic, "directory", directory, KW_CREATE);
    kw_get_dict(gobj, topic, "wr_fd_files", json_object(), KW_CREATE);
    kw_get_dict(gobj, topic, "rd_fd_files", json_object(), KW_CREATE);
    kw_get_dict(gobj, topic, "cache", json_object(), KW_CREATE);
    kw_get_dict(gobj, topic, "lists", json_array(), KW_CREATE);
    kw_get_dict(gobj, topic, "disks", json_array(), KW_CREATE);
    kw_get_dict(gobj, topic, "iterators", json_array(), KW_CREATE);
    kw_get_dict(gobj, topic, "iterators_by_id", json_object(), KW_CREATE);

    /*-------------------------------------*
     *  Load keys and metadata from disk
     *-------------------------------------*/
    build_topic_cache_from_disk(gobj, topic);

    /*
     *  Monitoring the disk to realtime disk lists
     */
    yev_loop_h yev_loop = (yev_loop_h)kw_get_int(gobj, tranger, "yev_loop", 0, KW_REQUIRED);
    if(yev_loop) {
        BOOL master = json_boolean_value(json_object_get(tranger, "master"));
        if(master) {
            // (1) MONITOR (MI) /disks/
            // Master will monitor the (topic) directory where clients will mark their rt disks.

            if(gobj_global_trace_level() & TRACE_FS) {
                gobj_log_debug(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_YEV_LOOP,
                    "msg",              "%s", "MASTER:  MONITOR INOTIFY (MI) /disks/",
                    "msg2",             "%s", "👓🔷 MASTER:  MONITOR INOTIFY (MI) /disks/",
                    "directory",        "%s", directory,
                    NULL
                );
            }

            /*
             *  HACK this function will open mem lists for directories found in /disk
             */
            fs_event_t *fs_event_master = monitor_disks_directory_by_master(
                gobj,
                yev_loop,
                tranger,
                topic
            );
            kw_set_dict_value(
                gobj,
                topic,
                "fs_event_master",
                json_integer((json_int_t)(uintptr_t)fs_event_master)
            );
        }
    }

    return topic;
}

/***************************************************************************
   Get topic by his topic_name.
   HACK topic can exist in disk, but it's not opened until tranger_open_topic()
 ***************************************************************************/
PUBLIC json_t *tranger2_topic( // WARNING returned JSON IS NOT YOURS
    json_t *tranger,
    const char *topic_name
)
{
    json_t *topic = json_object_get(json_object_get(tranger, "topics"), topic_name);
    if(!topic) {
        hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
        topic = tranger2_open_topic(tranger, topic_name, FALSE);
        if(!topic) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Cannot open topic",
                "topic",        "%s", topic_name,
                NULL
            );
            return 0;
        }
    }

    return topic;
}

/***************************************************************************
 *  Return in bf the path of topic
 ***************************************************************************/
PUBLIC int tranger2_topic_path(
    char *bf,
    size_t bfsize,
    json_t *tranger,
    const char *topic_name
) {
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    if(!topic_name_is_confined(gobj, tranger, topic_name, __FUNCTION__)) {
        if(bfsize > 0) {
            *bf = 0;
        }
        return -1;
    }
    snprintf(bf, bfsize, "%s/%s",
        kw_get_str(0, tranger, "directory", "", KW_REQUIRED),
        topic_name
    );
    return 0;
}

/***************************************************************************
 *  Return a list of topic names
 ***************************************************************************/
PUBLIC json_t *tranger2_list_topics( // return is yours
    json_t *tranger
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *jn_list = json_array();

    json_t *jn_topics = kw_get_dict(gobj, tranger, "topics", 0, KW_REQUIRED);
    const char *key; json_t *jn_value;
    json_object_foreach(jn_topics, key, jn_value) {
        json_array_append_new(jn_list, json_string(key));
    }

    return jn_list;
}

/***************************************************************************
 *  Return a list of topic names found in the tranger database directory
 *  on disk (subdirectories of the tranger directory)
 ***************************************************************************/
PUBLIC json_t *tranger2_list_topic_names( // return is yours, WARNING works in disk, not in memory
    json_t *tranger
)
{
    hgobj gobj = (hgobj)json_integer_value(
        json_object_get(tranger, "gobj")
    );

    const char *directory = kw_get_str(
        gobj, tranger, "directory", "", KW_REQUIRED
    );

    json_t *jn_list = json_array();

    DIR *dir = opendir(directory);
    if(dir) {
        struct dirent *entry;
        while((entry = readdir(dir)) != NULL) {
            if(entry->d_name[0] == '.') {
                continue;
            }
            char full_path[PATH_MAX];
            snprintf(
                full_path, sizeof(full_path),
                "%s/%s", directory, entry->d_name
            );
            if(is_directory(full_path)) {
                json_array_append_new(
                    jn_list, json_string(entry->d_name)
                );
            }
        }
        closedir(dir);
    }

    return jn_list;
}

/***************************************************************************
   Return list of keys of the topic
 ***************************************************************************/
PUBLIC json_t *tranger2_list_keys(// return is yours, WARNING fn slow for thousands of keys!
    json_t *tranger,
    const char *topic_name
) {
    json_t *topic = tranger2_topic( // WARNING returned json IS NOT YOURS
        tranger,
        topic_name
    );

    json_t *topic_cache = json_object_get(topic, "cache");
    json_t *jn_keys = json_array();

    void *iter = json_object_iter(topic_cache);
    while(iter) {
        const char *key = json_object_iter_key(iter);
        json_array_append_new(jn_keys, json_string(key));
        iter = json_object_iter_next(topic_cache, iter);
    }

    return jn_keys;
}

/***************************************************************************
   Get topic size (number of records of all keys)
 ***************************************************************************/
PUBLIC uint64_t tranger2_topic_size( // WARNING fn slow for thousands of keys!
    json_t *tranger,
    const char *topic_name
) {
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        return 0;
    }

    uint64_t total = 0;
    json_t *topic_cache = json_object_get(topic, "cache");
    void *iter = json_object_iter(topic_cache);
    while(iter) {
        const char *key = json_object_iter_key(iter);
        total += get_topic_key_rows(gobj, topic, key);
        iter = json_object_iter_next(topic_cache, iter);
    }

    return total;
}

/***************************************************************************
   Get key size (number of records of key)
 ***************************************************************************/
PUBLIC uint64_t tranger2_topic_key_size(
    json_t *tranger,
    const char *topic_name,
    const char *key
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        return 0;
    }
    if(empty_string(key)) {
        return tranger2_topic_size(tranger, topic_name);

    } else {
        return get_topic_key_rows(gobj, topic, key);
    }
}

/***************************************************************************
   Get the time span of a key: a copy of its cache totals
   {fr_t, to_t, fr_tm, to_tm, rows}, in the topic's own time unit.
 ***************************************************************************/
PUBLIC json_t *tranger2_topic_key_range( // return is yours
    json_t *tranger,
    const char *topic_name,
    const char *key
)
{
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        return NULL;    // Error already logged
    }
    json_t *cache_total = get_cache_total(topic, key);
    if(!cache_total) {
        return NULL;    // Unknown key: silent, it's a legitimate question
    }

    return json_deep_copy(cache_total);
}

/***************************************************************************
   TRUE if the topic is open in this tranger. Silent: a closed topic is a
   legitimate answer here, not an error.
 ***************************************************************************/
PUBLIC BOOL tranger2_topic_is_open(
    json_t *tranger,
    const char *topic_name
)
{
    if(!tranger || empty_string(topic_name)) {
        return FALSE;
    }
    json_t *topics = json_object_get(tranger, "topics");

    return json_object_get(topics, topic_name)? TRUE:FALSE;
}

/***************************************************************************
   Return topic name of topic.
 ***************************************************************************/
PUBLIC const char *tranger2_topic_name(
    json_t *topic
)
{
    return kw_get_str(0, topic, "topic_name", "", KW_REQUIRED);
}

/***************************************************************************
   Close record topic.
 ***************************************************************************/
PUBLIC int tranger2_close_topic(
    json_t *tranger,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, 0);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger_close_topic(): Topic not found",
            "database",     "%s", kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }

    close_fd_opened_files(gobj, topic, NULL);

    // MONITOR Master Unwatching (MI) topic /disks/
    yev_loop_h yev_loop = (yev_loop_h)kw_get_int(gobj, tranger, "yev_loop", 0, KW_REQUIRED);
    BOOL master = json_boolean_value(json_object_get(tranger, "master"));
    if(yev_loop && master) {
        if(gobj_global_trace_level() & TRACE_FS) {
            gobj_log_debug(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_YEV_LOOP,
                "msg",              "%s", "MASTER: Unwatching (MI) topic /disks/",
                "msg2",             "%s", "👓🔶 MASTER:  Unwatching (MI) topic /disks/",
                "action",           "%s", "fs_stop_watcher_event()",
                NULL
            );
        }

        fs_event_t *fs_event_master = (fs_event_t *)kw_get_int(
            gobj, topic, "fs_event_master", 0, KW_REQUIRED
        );
        if(fs_event_master) {
            fs_stop_watcher_event(fs_event_master);
        }
    }

    tranger2_close_all_lists(tranger, topic_name, "", "");

    json_t *jn_topics = kw_get_dict_value(gobj, tranger, "topics", 0, KW_REQUIRED);
    json_object_del(jn_topics, topic_name);

    return 0;
}

/***************************************************************************
   Delete topic. Alias delete table.
 ***************************************************************************/
PUBLIC int tranger2_delete_topic(
    json_t *tranger,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    BOOL master = tranger_is_master(gobj, tranger);

    if(!master) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Only master can delete",
            "topic_name",   "%s", topic_name?topic_name:"",
            NULL
        );
        return -1;
    }
    if(!topic_name_is_confined(gobj, tranger, topic_name, __FUNCTION__)) {
        return -1;
    }

    /*
     *  Build directory path first so we can check existence without
     *  the ERROR log that tranger2_topic() emits for missing topics.
     */
    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%s/%s",
        kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
        topic_name
    );

    if(access(directory, F_OK) != 0) {
        /*
         *  Topic directory does not exist on disk — nothing to delete.
         *  This is not an error: the caller may be doing a pre-creation
         *  cleanup and the topic was never created.
         */
        return 0;
    }

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Topic not found",
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }

    if(json_boolean_value(json_object_get(topic, "system_topic"))) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot delete topic: system topic",
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "Deleting topic",
        "topic",        "%s", topic_name,
        "directory",    "%s", directory,
        NULL
    );

    /*
     *  Close topic
     */
    tranger2_close_topic(tranger, topic_name);

    /*
     *  Check if the topic already exists
     */
    return rmrdir(directory);
}

/***************************************************************************
   Backup topic and re-create it.
   If ``backup_path`` is empty then it will be used the topic path
   If ``backup_name`` is empty then it will be used ``topic_name``.bak
   If overwrite_backup is TRUE and backup exists then it will be overwrited.
   Return the new topic
 ***************************************************************************/
PUBLIC json_t *tranger2_backup_topic(
    json_t *tranger,
    const char *topic_name,
    const char *backup_path,
    const char *backup_name,
    BOOL overwrite_backup,
    tranger_backup_deleting_callback_t tranger_backup_deleting_callback
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    BOOL master = tranger_is_master(gobj, tranger);

    if(!master) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Only master can back up",
            "topic_name",   "%s", topic_name?topic_name:"",
            NULL
        );
        return 0;
    }
    if(!topic_name_is_confined(gobj, tranger, topic_name, __FUNCTION__)) {
        return 0;
    }

    /*
     *  Close topic
     */
    tranger2_close_topic(tranger, topic_name);

    /*-------------------------------*
     *  Get original directory
     *-------------------------------*/
    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%s/%s",
        kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
        topic_name
    );

    /*
     *  Check if topic already exists
     */
    if(!is_directory(directory)) {
        return 0;
    }

    /*-------------------------------*
     *  Get backup directory
     *-------------------------------*/
    char backup_directory[PATH_MAX];
    if(empty_string(backup_path)) {
        snprintf(backup_directory, sizeof(backup_directory), "%s",
            kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED)
        );
    } else {
        snprintf(backup_directory, sizeof(backup_directory), "%s",
            backup_path
        );
    }
    if(backup_directory[strlen(backup_directory)-1]!='/') {
        snprintf(backup_directory + strlen(backup_directory),
            sizeof(backup_directory) - strlen(backup_directory), "%s",
            "/"
        );
    }
    if(empty_string(backup_name)) {
        snprintf(backup_directory + strlen(backup_directory),
            sizeof(backup_directory) - strlen(backup_directory), "%s.bak",
            topic_name
        );
    } else {
        snprintf(backup_directory + strlen(backup_directory),
            sizeof(backup_directory) - strlen(backup_directory), "%s",
            backup_name
        );
    }

    if(is_directory(backup_directory)) {
        if(overwrite_backup) {
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Backup timeranger topic, deleting",
                "database",     "%s", kw_get_str(gobj, tranger, "database", "", KW_REQUIRED),
                "topic",        "%s", topic_name,
                "path",         "%s", backup_directory,
                NULL
            );
            BOOL notmain = FALSE;
            if(tranger_backup_deleting_callback) {
                notmain = tranger_backup_deleting_callback(tranger, topic_name, backup_directory);
            }
            if(!notmain) {
                rmrdir(backup_directory);
            }
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "backup_directory EXISTS",
                "path",         "%s", backup_directory,
                NULL
            );
            return 0;
        }
    }

    /*-------------------------------*
     *      Load topic files
     *-------------------------------*/
    /*
     *  topic_desc
     */
    json_t *topic_desc = load_persistent_json(
        gobj,
        directory,
        "topic_desc.json",
        0,
        0,
        FALSE, // exclusive
        FALSE // silence
    );
    if(!topic_desc) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot load topic_desc.json",
            NULL
        );
        return 0;
    }

    /*
     *  topic_cols
     */
    json_t *topic_cols = load_json_from_file(
        gobj,
        directory,
        "topic_cols.json",
        0
    );

    /*
     *  topic_var
     */
    json_t *jn_topic_var = load_json_from_file(
        gobj,
        directory,
        "topic_var.json",
        0
    );

    /*-------------------------------*
     *      Move!
     *-------------------------------*/
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "Backup timeranger topic, moving",
        "database",     "%s", kw_get_str(gobj, tranger, "database", "", KW_REQUIRED),
        "topic",        "%s", topic_name,
        "src",          "%s", directory,
        "dst",          "%s", backup_directory,
        NULL
    );
    if(rename(directory, backup_directory)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "cannot backup topic",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            "src",          "%s", directory,
            "dst",          "%s", backup_directory,
            NULL
        );
        JSON_DECREF(topic_desc)
        JSON_DECREF(topic_cols)
        return 0;
    }

    if(!jn_topic_var) {
        jn_topic_var = json_object();
    }
    json_t *topic = tranger2_create_topic(
        tranger,
        topic_name,
        kw_get_str(gobj, topic_desc, "pkey", "", KW_REQUIRED),
        kw_get_str(gobj, topic_desc, "tkey", "", KW_REQUIRED),
        json_incref(topic_desc),
        (system_flag2_t)kw_get_int(gobj, topic_desc, "system_flag", 0, KW_REQUIRED),
        topic_cols,     // owned
        jn_topic_var    // owned
    );

    JSON_DECREF(topic_desc)
    return topic;
}

/***************************************************************************
   Write topic var
 ***************************************************************************/
PUBLIC int tranger2_write_topic_var(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_topic_var  // owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(!jn_topic_var) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "jn_topic_var EMPTY",
            NULL
        );
        return -1;
    }
    if(!json_is_object(jn_topic_var)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "jn_topic_var is NOT DICT",
            NULL
        );
        JSON_DECREF(jn_topic_var)
        return -1;
    }

    BOOL master = tranger_is_master(gobj, tranger);
    if(!master) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Only master can write",
            NULL
        );
        JSON_DECREF(jn_topic_var)
        return -1;
    }
    if(!topic_name_is_confined(gobj, tranger, topic_name, __FUNCTION__)) {
        JSON_DECREF(jn_topic_var)
        return -1;
    }
    char directory[PATH_MAX];
    snprintf(
        directory,
        sizeof(directory),
        "%s/%s",
        kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
        topic_name
    );

    json_t *topic_var = load_json_from_file(
        gobj,
        directory,
        "topic_var.json",
        0
    );
    if(!topic_var) {
        topic_var = json_object();
    }
    json_object_update(topic_var, jn_topic_var);
    json_decref(jn_topic_var);

    /*
     *  Through a temporary file and a rename, never in place: this runs on
     *  every create of a rowid-key node (treedb's `last_rowid_id`), and a
     *  process that died between the truncate and the write left the file
     *  empty (M4 of the 2026-09-23 independent review). Not fsync'ed: see
     *  replace_topic_var.
     */
    return replace_topic_var(gobj, tranger, directory, topic_name, topic_var, FALSE);
}

/***************************************************************************
   Write topic cols
 ***************************************************************************/
PUBLIC int tranger2_write_topic_cols(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_topic_cols  // owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(!jn_topic_cols) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "jn_topic_cols EMPTY",
            NULL
        );
        return -1;
    }
    if(!json_is_object(jn_topic_cols) && !json_is_array(jn_topic_cols)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "jn_topic_cols MUST BE dict or list",
            NULL
        );
        JSON_DECREF(jn_topic_cols)
        return -1;
    }

    BOOL master = tranger_is_master(gobj, tranger);
    if(!master) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Only master can write",
            NULL
        );
        JSON_DECREF(jn_topic_cols)
        return -1;
    }
    if(!topic_name_is_confined(gobj, tranger, topic_name, __FUNCTION__)) {
        JSON_DECREF(jn_topic_cols)
        return -1;
    }
    char directory[PATH_MAX];
    if(!build_path(directory, sizeof(directory),
            kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED), topic_name, NULL)) {
        JSON_DECREF(jn_topic_cols)
        return -1;  // Error already logged
    }

    /*
     *  Replaced, never written in place, and the memory takes the new cols
     *  only when the file did: it took them BEFORE the write, whose result
     *  was ignored, and answered 0 (independent review of the second fix
     *  round). Not fsync'ed: see replace_json_file().
     */
    if(replace_json_file(gobj, tranger, directory, "topic_cols.json", jn_topic_cols, FALSE, FALSE) < 0) {
        JSON_DECREF(jn_topic_cols)
        return -1;  // Error already logged
    }

    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, 0);
    if(topic) {
        json_object_set(topic, "cols", jn_topic_cols);
    }
    JSON_DECREF(jn_topic_cols)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *tranger2_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        // Error already logged
        return 0;
    }
    /*
     *  `pkey2s` travels with the descriptor: a topic whose pkey is
     *  synthetic (a `rowid`/`uuid` flagged id) is unreadable without it,
     *  because the name a human knows the record by lives in the
     *  secondary key. A viewer that only gets the pkey can do nothing
     *  but print the rowid. Absent keys are skipped by kw_clone_by_path,
     *  so a topic without secondary keys is unaffected.
     *
     *  So do the two marks a topic can carry beyond its columns:
     *  `system_topic` (it cannot be deleted) and `main_topic` (the tree
     *  of the treedb hangs from it, stamped by treedb_open_db from the
     *  schema). A viewer draws both, and neither is readable from `cols`.
     */
    const char *fields[] = {
        "topic_name",
        "pkey",
        "pkey2s",
        "tkey",
        "system_flag",
        "topic_version",
        "system_topic",
        "main_topic",
        0
    };

    json_t *desc = kw_clone_by_path(
        gobj,
        json_incref(topic),
        fields
    );

    json_t *cols = kwid_new_list(gobj, topic, KW_VERBOSE, "cols");
    json_object_set_new(desc, "cols", cols);

    return desc;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *tranger2_list_topic_desc_cols( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        // Error already logged
        return 0;
    }
    return kwid_new_list(gobj, topic, KW_VERBOSE, "cols");
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *tranger2_dict_topic_desc_cols( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        // Error already logged
        return 0;
    }
    return kwid_new_dict(gobj, topic, KW_VERBOSE, "cols");
}

/***************************************************************************
 *  Get fullpath of filename in content or md2 level
 *  The directory will be create if it's master
 ***************************************************************************/
PRIVATE char *get_file_id(
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    uint64_t __t__ // WARNING must be in seconds!
)
{
    hgobj gobj = 0;
    *bf = 0;

    struct tm *tm = gmtime((time_t *)&__t__);
    if(!tm) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "gmtime() FAILED",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return NULL;
    }

    const char *filename_mask = json_string_value(json_object_get(topic, "filename_mask"));
    if(empty_string(filename_mask)) {
        filename_mask = json_string_value(json_object_get(tranger, "filename_mask"));
    }

    if(empty_string(filename_mask) || strftime(bf, (size_t)bfsize, filename_mask, tm) == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "filename_mask gives no file id",
            "filename_mask","%s", filename_mask?filename_mask:"",
            "t",            "%lu", (unsigned long)__t__,
            NULL
        );
        *bf = 0;
        return NULL;
    }
    return bf;
}

/***************************************************************************
 *  Get fullpath of filename in content or md2 level
 *  The directory will be create if it's master
 ***************************************************************************/
PRIVATE char *get_t_filename(
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    BOOL for_data,  // TRUE for data, FALSE for md2
    uint64_t __t__ // WARNING must be in seconds!
)
{
    char filename[NAME_MAX];
    if(!get_file_id(filename, sizeof(filename), tranger, topic, __t__)) {
        // Error already logged
        if(bfsize > 0) {
            *bf = 0;
        }
        return NULL;
    }

    snprintf(bf, bfsize, "%s.%s",
        filename,
        for_data?"json":"md2"
    );

    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_file(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *full_path
)
{
    BOOL master = json_boolean_value(json_object_get(tranger, "master"));
    const char *topic_dir = json_string_value(json_object_get(topic, "directory"));

    /*----------------------------------------*
     *  Create (only)the new file if master
     *----------------------------------------*/
    if(!master) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "path",         "%s", full_path,
            "msg",          "%s", "Only master can write",
            NULL
        );
        return -1;
    }

    /*------------------------------*
     *      Check key directory
     *      create it not exist
     *------------------------------*/
    char path_key[PATH_MAX];
    snprintf(path_key, sizeof(path_key), "%s/keys/%s",
        topic_dir,
        key
    );
    if(!is_directory(path_key)) {
        if(master) {
            int xpermission = (int)kw_get_int(
                gobj,
                topic,
                "xpermission",
                (int)kw_get_int(gobj, tranger, "xpermission", 02770, KW_REQUIRED),
                0
            );
            if(mkrdir(path_key, xpermission)<0) {
                gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                    "function",     "%s", __FUNCTION__,
                    "path",         "%s", path_key,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "Cannot create subdir. mkrdir() FAILED",
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
            }
        } else {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "path",         "%s", path_key,
                "msg",          "%s", "key directory not found",
                NULL
            );
            return -1;
        }
    }

    /*-----------------------------------------*
     *  Check if there are many files opened
     *-----------------------------------------*/
    json_t *wr_fd = kw_get_subdict_value(gobj, topic, "wr_fd_files", key, 0, 0); // no required
    // it could be the first time
    if(json_object_size(wr_fd)>=2) {
        close_fd_wr_files(gobj, topic, key);
    }

    int fp = newfile(full_path, (int)kw_get_int(gobj, tranger, "rpermission", 0, KW_REQUIRED), FALSE);
    if(fp < 0) {
        if(errno == EMFILE) {
            struct rlimit rl = {0};
            getrlimit(RLIMIT_NOFILE, &rl);

            gobj_log_error(gobj, 0,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_INTERNAL,
                "path",                 "%s", full_path,
                "msg",                  "%s", "TOO MANY OPEN FILES 1",
                "current soft limit",   "%d", rl.rlim_cur,
                "current hard limit",   "%d", rl.rlim_max,
                NULL
            );
            close_fd_wr_files(gobj, topic, "");

            fp = newfile(full_path, (int)kw_get_int(gobj, tranger, "rpermission", 0, KW_REQUIRED), FALSE);
            if(fp < 0) {
                gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "Cannot create json file, after close files",
                    "path",         "%s", full_path,
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
                return -1;
            }
        } else {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot create json file",
                "path",         "%s", full_path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            return -1;
        }
    }

    return fp;
}

/***************************************************************************
 *  Called only from master
 ***************************************************************************/
PRIVATE int get_topic_wr_fd( // optimized
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    BOOL for_data,
    const char *file_id     // get_file_id() of the record's __t__
)
{
    char full_path[PATH_MAX];
    char relative_path[PATH_MAX];
    char filename[NAME_MAX*2];

    /*-----------------------------*
     *      Check file
     *-----------------------------*/
    snprintf(filename, sizeof(filename), "%s.%s", file_id, for_data?"json":"md2");

    /*------------------------------------------*
     *  Open content file fd,
     *  look if already opened else create it
     *------------------------------------------*/
    int fd = (int)json_integer_value(
        json_object_get(
            json_object_get(
                json_object_get(
                    topic,
                    "wr_fd_files"
                ),
                key
            ),
            filename
        )
    );

    if(fd<=0) {
        BOOL master = json_boolean_value(json_object_get(tranger, "master"));
        const char *topic_dir = json_string_value(json_object_get(topic, "directory"));
        snprintf(full_path, sizeof(full_path), "%s/keys/%s/%s", topic_dir, key, filename);

        if(master) {
            fd = open(full_path, O_RDWR|O_NOFOLLOW|O_CLOEXEC, 0);
            if(fd < 0) {
                if(errno == EMFILE) {
                    struct rlimit rl = {0};
                    getrlimit(RLIMIT_NOFILE, &rl);

                    gobj_log_error(gobj, 0,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_INTERNAL,
                        "path",                 "%s", full_path,
                        "msg",                  "%s", "TOO MANY OPEN FILES 2",
                        "current soft limit",   "%d", rl.rlim_cur,
                        "current hard limit",   "%d", rl.rlim_max,
                        NULL
                    );

                    close_fd_wr_files(gobj, topic, "");
                    fd = open(full_path, O_RDWR|O_NOFOLLOW|O_CLOEXEC, 0);
                }
                if(fd < 0) {
                    fd = create_file(gobj, tranger, topic, key, full_path);
                }
            }
        } else {
            fd = open(full_path, O_RDONLY|O_CLOEXEC, 0);
        }

        if(fd<0) {
            gobj_log_critical(gobj,
                master?kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED):0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot open file to write",
                "path",         "%s", full_path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        snprintf(relative_path, sizeof(relative_path), "%s`%s", key, filename);

        // change to native jansson functions
        // kw_set_dict_value(
        //     gobj,
        //     kw_get_dict(
        //         gobj,
        //         topic,
        //         "wr_fd_files",
        //         0,
        //         KW_REQUIRED
        //     ),
        //     relative_path,
        //     json_integer(fd)
        // );
        json_t *wr_fd_files = json_object_get(topic, "wr_fd_files");
        json_t *key_dict = json_object_get(wr_fd_files, key);
        if(!key_dict) {
            key_dict = json_object();
            json_object_set_new(wr_fd_files, key, key_dict);
        }
        json_object_set_new(key_dict, filename, json_integer(fd));
    }

    return fd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_topic_rd_fd(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id,
    BOOL for_data
)
{
    char full_path[PATH_MAX];
    char relative_path[NAME_MAX*2];
    char filename[NAME_MAX];

    /*-----------------------------*
     *      Check file
     *-----------------------------*/
    //const char *file_id = json_string_value(json_object_get(segment, "id"));
    snprintf(filename, sizeof(filename), "%s.%s", file_id, for_data?"json":"md2");

    int fd = (int)json_integer_value(
        json_object_get(
            json_object_get(
                json_object_get(
                    topic,
                    "rd_fd_files"
                ),
                key
            ),
            filename
        )
    );

    if(fd<=0) {
        const char *topic_dir = json_string_value(json_object_get(topic, "directory"));
        snprintf(full_path, sizeof(full_path), "%s/keys/%s/%s", topic_dir, key, filename);

        /*-----------------------------*
         *      Open content file
         *-----------------------------*/
        fd = open(full_path, O_RDONLY|O_CLOEXEC, 0);
        if(fd < 0) {
            if(errno == EMFILE) {
                struct rlimit rl = {0};
                getrlimit(RLIMIT_NOFILE, &rl);

                gobj_log_error(gobj, 0,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_INTERNAL,
                    "path",                 "%s", full_path,
                    "msg",                  "%s", "TOO MANY OPEN FILES 3",
                    "current soft limit",   "%d", rl.rlim_cur,
                    "current hard limit",   "%d", rl.rlim_max,
                    NULL
                );

                close_fd_rd_files(gobj, topic, "");
                fd = open(full_path, O_RDONLY|O_CLOEXEC, 0);
            }
        }
        if(fd<0) {
            /*
             *  A failed READ never leaves the process, on_critical_error or
             *  not: nothing was written, and the caller answers an error. It
             *  is reached by a key deleted under an open iterator, and with
             *  on_critical_error=2 it was an exit(0) nobody relaunches.
             */
            gobj_log_critical(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot open file to read",
                "path",         "%s", full_path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        snprintf(relative_path, sizeof(relative_path), "%s`%s", key, filename);

        // change to native jansson functions
        // kw_set_dict_value(
        //     gobj,
        //     kw_get_dict(
        //         gobj,
        //         topic,
        //         "rd_fd_files",
        //         0,
        //         KW_REQUIRED
        //     ),
        //     relative_path,
        //     json_integer(fd)
        // );
        json_t *rd_fd_files = json_object_get(topic, "rd_fd_files");
        json_t *key_dict = json_object_get(rd_fd_files, key);
        if(!key_dict) {
            key_dict = json_object();
            json_object_set_new(rd_fd_files, key, key_dict);
        }
        json_object_set_new(key_dict, filename, json_integer(fd));
    }

    return fd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_fd_files(
    hgobj gobj,
    json_t *fd_files,
    const char *key_
)
{
    json_t *jn_value;
    const char *key;
    void *tmp;

    json_object_foreach_safe(fd_files, tmp, key, jn_value) {
        if(json_is_object(jn_value)) {
            if(empty_string(key_) || strcmp(key, key_)==0) {
                json_t *jn_value2;
                const char *key2;
                void *tmp2;
                json_object_foreach_safe(jn_value, tmp2, key2, jn_value2) {
                    int fd = (int)kw_get_int(gobj, jn_value, key2, -1, KW_REQUIRED);
                    if(fd >= 0) {
                        close(fd);
                    }
                    json_object_del(jn_value, key2);
                }
                json_object_del(fd_files, key);
            }
        } else {
            int fd = (int)kw_get_int(gobj, fd_files, key, -1, KW_REQUIRED);
            if(fd >= 0) {
                close(fd);
            }
            json_object_del(fd_files, key);
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_fd_wr_files(
    hgobj gobj,
    json_t *topic,
    const char *key
)
{
    json_t *fd_files = kw_get_dict(gobj, topic, "wr_fd_files", 0, KW_REQUIRED);
    return close_fd_files(gobj, fd_files, key);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_fd_rd_files(
    hgobj gobj,
    json_t *topic,
    const char *key
)
{
    json_t *fd_files = kw_get_dict(gobj, topic, "rd_fd_files", 0, KW_REQUIRED);
    return close_fd_files(gobj, fd_files, key);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_fd_opened_files(
    hgobj gobj,
    json_t *topic,
    const char *key
)
{
    close_fd_wr_files(gobj, topic, key);
    close_fd_rd_files(gobj, topic, key);
    return 0;
}

/***************************************************************************
 *  Return json object with record metadata
 ***************************************************************************/
PRIVATE json_t *md2json(
    md2_record_ex_t *md_record_ex
)
{
    json_t *jn_md = json_object();
    json_object_set_new(jn_md, "g_rowid", json_integer((json_int_t)md_record_ex->g_rowid));
    json_object_set_new(jn_md, "i_rowid", json_integer((json_int_t)md_record_ex->rowid));
    json_object_set_new(jn_md, "t", json_integer((json_int_t)md_record_ex->__t__));
    json_object_set_new(jn_md, "tm", json_integer((json_int_t)md_record_ex->__tm__));
    json_object_set_new(jn_md, "offset", json_integer((json_int_t)md_record_ex->__offset__));
    json_object_set_new(jn_md, "size", json_integer((json_int_t)md_record_ex->__size__));
    json_object_set_new(jn_md, "system_flag", json_integer(md_record_ex->system_flag));
    json_object_set_new(jn_md, "user_flag", json_integer(md_record_ex->user_flag));

    return jn_md;
}

/***************************************************************************
 *  A feed (rt_mem list or rt_disk) opened with only_md wants a record's
 *  metadata but not its body — the realtime paths honor it like the
 *  historical iterator (tranger2_open_iterator) already does.
 ***************************************************************************/
PRIVATE BOOL feed_wants_only_md(json_t *feed)
{
    json_t *match_cond = json_object_get(feed, "match_cond");
    if(!match_cond) {
        return FALSE;
    }
    return json_boolean_value(json_object_get(match_cond, "only_md"))? TRUE : FALSE;
}

/***************************************************************************
    Append a new item to record.
    The 'pkey' and 'tkey' are getting according to the topic schema.
    Return the new record's metadata.
 ***************************************************************************/
PUBLIC int tranger2_append_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t __t__,         // if 0 then the time will be set by TimeRanger with now time
    uint16_t user_flag,
    md2_record_ex_t *md_record_ex, // required, to return the metadata
    json_t *record       // JSON owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(!record || record->refcount <= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Cannot append record, record NULL",
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }

    // TEST performance 800.000

    BOOL master = tranger_is_master(gobj, tranger);
    if(!master) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot append record, NO master",
            "topic",        "%s", topic_name,
            NULL
        );
        gobj_trace_json(gobj, record, "Cannot append record, NO master");
        JSON_DECREF(record)
        return -1;
    }

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Cannot append record, topic not found",
            "topic",        "%s", topic_name,
            NULL
        );
        gobj_trace_json(gobj, record, "Cannot append record, topic not found");
        JSON_DECREF(record)
        return -1;
    }

    /*--------------------------------------------*
     *  If time not specified, use the now time
     *--------------------------------------------*/
    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
    if(!__t__) {
        if(system_flag & (sf_t_ms)) {
            __t__ = time_in_milliseconds();
        } else {
            __t__ = time_in_seconds();
        }
    }

    /*--------------------------------------------*
     *  Prepare new record metadata
     *--------------------------------------------*/
    md2_record_t md_record;
    memset(&md_record, 0, sizeof(md2_record_t));
    md_record.__t__ = __t__;

    // TEST performance 700000

    /*-----------------------------------*
     *  Get the primary-key
     *-----------------------------------*/
    const char *pkey = json_string_value(json_object_get(topic, "pkey"));
    system_flag2_t system_flag_key_type = system_flag & KEY_TYPE_MASK2;

    const char *key_value = NULL;
    char key_int[NAME_MAX+1];

    switch(system_flag_key_type) {
        case sf_string_key:
            {
                key_value = json_string_value(json_object_get(record, pkey));
                if(empty_string(key_value)) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_JSON,
                        "msg",          "%s", "Cannot append record, no pkey",
                        "topic",        "%s", topic_name,
                        "pkey",         "%s", pkey,
                        NULL
                    );
                    gobj_trace_json(gobj, record, "Cannot append record, no pkey");
                    JSON_DECREF(record)
                    return -1;
                }
                if(strlen(key_value) > NAME_MAX) {
                    // Key will be a directory name, cannot be greater than NAME_MAX
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER,
                        "msg",          "%s", "Cannot append record, pkey too long",
                        "topic",        "%s", topic_name,
                        "key_value",    "%s", pkey,
                        "size",         "%d", strlen(key_value),
                        "maxsize",      "%d", NAME_MAX,
                        NULL
                    );
                    gobj_trace_json(gobj, record, "Cannot append record, pkey too long");
                    JSON_DECREF(record)
                    return -1;
                }
                /*
                 *  The key becomes a single directory component under
                 *  <topic_dir>/keys/. It must not contain a path separator
                 *  nor be a relative-path element ('.' or '..'), otherwise it
                 *  escapes the keys/ directory (path traversal). Reject any
                 *  value containing '/' or beginning with '.'.
                 */
                if(strchr(key_value, '/') != NULL || key_value[0] == '.') {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER,
                        "msg",          "%s", "Cannot append record, invalid pkey (path traversal)",
                        "topic",        "%s", topic_name,
                        "key_value",    "%s", key_value,
                        NULL
                    );
                    gobj_trace_json(gobj, record, "Cannot append record, invalid pkey (path traversal)");
                    JSON_DECREF(record)
                    return -1;
                }
            }
            break;

        case sf_int_key:
            {
                uint64_t i = json_integer_value(json_object_get(record, pkey));
                snprintf(key_int, sizeof(key_int), "%0*"PRIu64, 19, i);
                key_value = key_int;
            }
            break;

        case sf_rowid_key:
            {
                key_value = "__rowid__";
                json_object_set_new(record, "__rowid__", json_string("__rowid__"));
            }
            break;

        default:
            // No pkey
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_JSON,
                "msg",          "%s", "Cannot append record, no pkey type",
                "topic",        "%s", topic_name,
                "pkey",         "%s", pkey,
                NULL
            );
            gobj_trace_json(gobj, record, "Cannot append record, no pkey type");
            JSON_DECREF(record)
            return -1;
    }

    // TEST performance 630000

    /*--------------------------------------------*
     *  Get and save the t-key if exists
     *--------------------------------------------*/
    const char *tkey = json_string_value(json_object_get(topic, "tkey"));
    if(!empty_string(tkey)) {
        json_t *jn_tval = json_object_get(record, tkey);
        if(!jn_tval) {
            md_record.__tm__ = 0; // No tkey value, mark with 0
        } else {
            if(json_is_string(jn_tval)) {
                timestamp_t timestamp = approxidate(json_string_value(jn_tval));
                if(system_flag & sf_tm_ms) {
                    timestamp *= 1000;
                }
                md_record.__tm__ = timestamp;
            } else if(json_is_integer(jn_tval)) {
                md_record.__tm__ = json_integer_value(jn_tval);
            } else {
                md_record.__tm__ = 0; // No tkey value, mark with 0
            }
        }
    } else {
        md_record.__tm__ = 0;  // No tkey value, mark with 0
    }

    // TEST performance 600000

    /*------------------------------------------------------*
     *  The file of the record, named by its __t__:
     *  the data and md2 files and the cache cell share it
     *------------------------------------------------------*/
    char file_id[NAME_MAX];
    if(!get_file_id(
        file_id,
        sizeof(file_id),
        tranger,
        topic,
        (system_flag & sf_t_ms)? __t__/1000:__t__
    )) {
        // Error already logged
        gobj_trace_json(gobj, record, "Cannot append record, cannot name its file");
        JSON_DECREF(record)
        return -1;
    }

    /*------------------------------------------------------*
     *  Save content, to file
     *------------------------------------------------------*/
    int content_fp = get_topic_wr_fd(gobj, tranger, topic, key_value, TRUE, file_id);

    // TEST performance 475000

    /*--------------------------------------------*
     *  New record always at the end
     *--------------------------------------------*/
    off_t __offset__ = 0;
    if(content_fp >= 0) {
        __offset__ = lseek(content_fp, 0, SEEK_END);
        if(__offset__ < 0) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot append record, lseek() FAILED",
                "topic",        "%s", topic_name,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            gobj_trace_json(gobj, record, "Cannot append record, lseek() FAILED");
            JSON_DECREF(record)
            return -1;
        }
        md_record.__offset__ = __offset__;

        /*--------------------------------------------*
         *  Get the record's content, always json.
         *  A __md_tranger__ the record carries (from an earlier append, or
         *  from a load) is the metadata of THAT record, not content.
         *--------------------------------------------*/
        json_object_del(record, "__md_tranger__");
        char *srecord = json_dumps(record, JSON_COMPACT|JSON_ENCODE_ANY);
        if(!srecord) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_JSON,
                "msg",          "%s", "Cannot append record, json_dumps() FAILED",
                "topic",        "%s", topic_name,
                NULL
            );
            gobj_trace_json(gobj, record, "Cannot append record, json_dumps() FAILED");
            JSON_DECREF(record)
            // JSON_DECREF(kw_)
            return -1;
        }
        // JSON_DECREF(kw_)

        size_t size = strlen(srecord);
        char *p = srecord;

        /*
         *  Saving: first compress, second encrypt (sure this order?)
         */
// TODO       if(system_flag & sf_zip_record) {
//            // if(topic->compress_callback) {
//            //     gbuf = topic->compress_callback(
//            //         topic,
//            //         srecord
//            //     );
//            // }
//        }
//        if(system_flag & sf_cipher_record) {
//            // if(topic->encrypt_callback) {
//            //     gbuf = topic->encrypt_callback(
//            //         topic,
//            //         srecord
//            //     );
//            // }
//        }

        md_record.__size__ = size + 1; // put the final null

        /*-------------------------*
         *  Write record content
         *-------------------------*/
        size_t ln = write( // write new (record content)
            content_fp,
            p,
            md_record.__size__
        );
        if(ln != md_record.__size__) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot append record, write FAILED",
                "topic",        "%s", topic_name,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            gobj_trace_json(gobj, record, "Cannot append record, write FAILED");
            JSON_DECREF(record)
            jsonp_free(srecord);
            return -1;
        }

        jsonp_free(srecord);
    } else {
        // Error already logged by get_topic_wr_fd
        JSON_DECREF(record)
        return -1;
    }

    // TEST performance 170000

    /*--------------------------------------------*
     *  Write record metadata
     *--------------------------------------------*/
    set_user_flag(&md_record, user_flag);
    set_system_flag(&md_record, system_flag & ~NOT_INHERITED_MASK);

    json_int_t g_rowid = 0;
    json_int_t i_rowid = 0;
    int md2_fd = get_topic_wr_fd(gobj, tranger, topic, key_value, FALSE, file_id);

    if(md2_fd >= 0) {
        off_t offset = lseek(md2_fd, 0, SEEK_END);
        if(offset < 0) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot append record, lseek() FAILED",
                "topic",        "%s", topic_name,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            gobj_trace_json(gobj, record, "Cannot append record, lseek() FAILED");
            JSON_DECREF(record)
            return -1;
        }

        i_rowid = (json_int_t)(offset/sizeof(md2_record_t)) + 1;

        /*--------------------------------------------*
         *  The marker of an out-of-order record goes
         *  down BEFORE its row (see the function)
         *--------------------------------------------*/
        mark_file_before_append(gobj, topic, key_value, file_id, &md_record);

        /*--------------------------------------------*
         *  write md2 in big endian
         *--------------------------------------------*/
        md2_record_t big_endian;
        big_endian.__t__ = htonll(md_record.__t__);
        big_endian.__tm__ = htonll(md_record.__tm__);
        big_endian.__offset__ = htonll(md_record.__offset__);
        big_endian.__size__ = htonll(md_record.__size__);

        size_t ln = write( // write md
            md2_fd,
            &big_endian,
            sizeof(md2_record_t)
        );
        if(ln != sizeof(md2_record_t)) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot save record metadata, write FAILED",
                "topic",        "%s", tranger2_topic_name(topic),
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            JSON_DECREF(record)
            return -1;
        }

        /*
         *  Update cache
         */
        g_rowid = update_new_record_from_mem(gobj, topic, key_value, file_id, &md_record);
        if(system_flag_key_type & sf_rowid_key) {
            if(g_rowid != i_rowid) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_JSON,
                    "msg",          "%s", "g_rowid != i_rowid",
                    "topic",        "%s", topic_name,
                    "g_rowid",      "%lu", (unsigned long) g_rowid,
                    "i_rowid",      "%lu", (unsigned long) i_rowid,
                    NULL
                );
            }
        }
    } else {
        // Error already logged by get_topic_wr_fd
        JSON_DECREF(record)
        return -1;
    }

    // TEST performance with sf_save_md_in_record 98000
    // TEST performance sin sf_save_md_in_record 124000

    /*-----------------------------------------------------*
     *  Add the message metadata to the record
     *  Could be useful for records with the same __t__
     *  for example, to distinguish them by the readers.
     *-----------------------------------------------------*/
    md_record_ex->__t__ = get_time_t(&md_record);
    md_record_ex->__tm__ = get_time_tm(&md_record);
    md_record_ex->__offset__ = md_record.__offset__;
    md_record_ex->__size__ = md_record.__size__;
    md_record_ex->system_flag = get_system_flag(&md_record);
    md_record_ex->user_flag = get_user_flag(&md_record);
    md_record_ex->rowid = i_rowid;
    md_record_ex->g_rowid = g_rowid;

    /*
     *  The caller has the metadata in md_record_ex. The record gets a
     *  __md_tranger__ only when a realtime list below takes it (it wants
     *  the key and not only its metadata), built once, before the first.
     */
    BOOL md_attached = FALSE;

    /*--------------------------------------------*
     *      FEED the lists
     *      Call callbacks of realtime lists
     *--------------------------------------------*/
    json_t *lists = json_object_get(topic, "lists");
    int idx;
    json_t *list;
    json_array_foreach(lists, idx, list) {
        if(list_wants_key(list, key_value)) {
            tranger2_load_record_callback_t load_record_callback =
                (tranger2_load_record_callback_t)(size_t)json_integer_value(
                    json_object_get(list, "load_record_callback")
                );

            if(load_record_callback) {
                BOOL only_md = feed_wants_only_md(list);
                if(!only_md && !md_attached) {
                    json_object_set_new(record, "__md_tranger__", md2json(md_record_ex));
                    md_attached = TRUE;
                }
                // Inform to the user list: record real time from memory
                load_record_callback(
                    tranger,
                    topic,
                    key_value,
                    list,
                    g_rowid,
                    md_record_ex,
                    only_md? NULL : json_incref(record)
                );
            }
        }
    }

    JSON_DECREF(record)
    return 0;
}

/***************************************************************************
 *  Fire registered key-delete callbacks for every in-memory subscriber
 *  whose filter matches the deleted key, on the master's delete path.
 *
 *  Subscribers live in three arrays on the topic:
 *      topic.lists[]       — rt_mem
 *      topic.iterators[]   — open_iterator
 *      topic.disks[]       — rt_disk
 *
 *  Each entry's `key` field is the filter ("" = match all).
 *
 *  A feed with an fs_watcher (an rt_disk with a loop) is NOT fired here: it
 *  hears of the delete through its own directory (client_fs_callback ->
 *  fire_key_deleted_to_feed()), exactly once.
 ***************************************************************************/
PRIVATE void fire_key_deleted_locally(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *deleted_key
)
{
    /*
     *  The key is gone: drop its watermark from every disk feed, whatever the
     *  band this call is fanning out to. Two reasons, and the second is a bug:
     *
     *   - `published` holds one mark PER KEY, and a keyless feed takes every
     *     key of the topic. On a topic that cycles its keys (an hourly bucket)
     *     the mark set would grow for as long as the feed lives, with a mark
     *     for every key that ever existed.
     *   - a key RE-CREATED in the same file would inherit the dead key's mark,
     *     and a mark above the new key's rowids is a ceiling, not a watermark:
     *     the records of the reborn key would be served to nobody.
     */
    json_t *disks_ = json_object_get(topic, "disks");
    int idx_; json_t *disk_;
    json_array_foreach(disks_, idx_, disk_) {
        json_t *published = json_object_get(disk_, "published");
        if(published) {
            json_object_del(published, deleted_key);
        }
    }

    const char *bands[] = { "lists", "iterators", "disks", NULL };
    for(int b = 0; bands[b]; b++) {
        json_t *band = json_object_get(topic, bands[b]);
        if(!json_is_array(band)) {
            continue;
        }
        int idx;
        json_t *entry;
        json_array_foreach(band, idx, entry) {
            if(json_integer_value(json_object_get(entry, "fs_event_client"))) {
                continue;   // Its directory tells it
            }
            const char *filter_key = json_string_value(json_object_get(entry, "key"));
            if(filter_key && filter_key[0] != '\0'
                    && strcmp(filter_key, deleted_key) != 0) {
                continue;
            }
            tranger2_key_deleted_callback_t cb =
                (tranger2_key_deleted_callback_t)(uintptr_t)json_integer_value(
                    json_object_get(entry, "key_deleted_callback"));
            if(!cb) {
                continue;
            }
            void *user_data = (void *)(uintptr_t)json_integer_value(
                json_object_get(entry, "key_deleted_user_data"));
            cb(tranger, topic, deleted_key, entry, user_data);
        }
    }
}

/***************************************************************************
 *  Tell ONE rt_disk feed that a key was deleted: drop its watermark of the
 *  key and, when the feed wants the key, call its key_deleted callback.
 *  The callback may close the feed: `disk` is not touched after it.
 ***************************************************************************/
PRIVATE void fire_key_deleted_to_feed(
    json_t *tranger,
    json_t *topic,
    json_t *disk,
    const char *deleted_key
)
{
    json_t *published = json_object_get(disk, "published");
    if(published) {
        json_object_del(published, deleted_key);
    }

    const char *filter_key = json_string_value(json_object_get(disk, "key"));
    if(filter_key && filter_key[0] != '\0' && strcmp(filter_key, deleted_key) != 0) {
        return;
    }
    tranger2_key_deleted_callback_t cb =
        (tranger2_key_deleted_callback_t)(uintptr_t)json_integer_value(
            json_object_get(disk, "key_deleted_callback"));
    if(!cb) {
        return;
    }
    void *user_data = (void *)(uintptr_t)json_integer_value(
        json_object_get(disk, "key_deleted_user_data"));
    cb(tranger, topic, deleted_key, disk, user_data);
}

/***************************************************************************
 *  Mirror the key deletion into the directory of EVERY feed:
 *  `topic/disks/<rt_id>/<key>/` is removed, and followers watching their
 *  `disks/<rt_id>/` pick it up as FS_SUBDIR_DELETED_TYPE.
 *
 *  A feed that received no record of the key since it opened has no
 *  `<key>/` to remove, so it used to hear nothing, and its cache kept the
 *  dead key (every later read of it failed). There the directory is created
 *  and removed at once: a key directory that appears and vanishes says the
 *  same thing.
 ***************************************************************************/
PRIVATE void mirror_key_delete_to_disks(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key
)
{
    const char *topic_dir = json_string_value(json_object_get(topic, "directory"));
    if(!topic_dir) {
        return;
    }

    char disks_root[PATH_MAX];
    snprintf(disks_root, sizeof(disks_root), "%s/disks", topic_dir);
    if(!is_directory(disks_root)) {
        return;
    }

    DIR *dir = opendir(disks_root);
    if(!dir) {
        return;
    }
    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.' &&
          (entry->d_name[1] == '\0' ||
           (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }
        char rt_path[PATH_MAX];
        build_path(rt_path, sizeof(rt_path), disks_root, entry->d_name, NULL);
        if(!is_directory(rt_path)) {
            continue;
        }
        char key_path[PATH_MAX];
        build_path(key_path, sizeof(key_path), disks_root, entry->d_name, key, NULL);
        if(is_directory(key_path)) {
            if(rmrdir(key_path) < 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "rmrdir() on disks/<rt_id>/<key>/ FAILED",
                    "path",         "%s", key_path,
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
            }
        } else {
            if(mkdir(key_path, json_integer_value(json_object_get(tranger, "xpermission")))<0 ||
               rmdir(key_path)<0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "cannot signal the key delete in disks/<rt_id>/<key>/",
                    "path",         "%s", key_path,
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
            }
        }
    }
    closedir(dir);
}

/***************************************************************************
    Delete a whole record (= primary key) from a topic.
    Removes the `keys/<key>/` directory and every instance it holds.
    Irrecoverable. Only the master can delete.

    Propagates the deletion: first mirrors the rmrdir into
    `topic/disks/<rt_id>/<key>/` so followers pick it up via inotify,
    then fires in-process `key_deleted_callback`s on rt_mem /
    iterators / rt_disk subscribers, then removes the live key dir.
    Order matters — followers may read while reacting.
 ***************************************************************************/
PUBLIC int tranger2_delete_key(
    json_t *tranger,
    const char *topic_name,
    const char *key
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    BOOL master = tranger_is_master(gobj, tranger);

    /*----------------------------------------*
     *  Delete key only if master
     *----------------------------------------*/
    if(!master) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Only master can delete",
            "topic_name",   "%s", topic_name,
            "key",          "%s", key,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *  Validate key BEFORE it reaches any
     *  filesystem path: a legitimate key is a
     *  single directory component. Reject any
     *  embedded '/' (path traversal / absolute
     *  path), "." / ".." and any leading '.'
     *  (dot-relative or hidden component) so a
     *  caller-supplied id cannot escape the
     *  topic's keys/ dir and drive rmrdir()
     *  against an out-of-tree directory.
     *----------------------------------------*/
    if(empty_string(key) ||
       strchr(key, '/') != NULL ||
       key[0] == '.') {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Invalid key (path metacharacters not allowed)",
            "topic_name",   "%s", topic_name,
            "key",          "%s", key,
            NULL
        );
        return -1;
    }

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_topic() failed",
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }

    /*
     *  Close opened files
     */
    close_fd_opened_files(gobj, topic, key);

    /*
     *  Remove directory of topic's key FIRST: the delete is announced only
     *  once it is done. It was announced before the rmrdir(), so a key whose
     *  directory could not be removed was heard deleted by every follower
     *  and subscriber while it was still on disk, and still in this cache
     *  (independent review of the second fix round).
     */
    const char *topic_dir = json_string_value(json_object_get(topic, "directory"));
    json_t *topic_cache = json_object_get(topic, "cache");

    char path_key[PATH_MAX];
    if(!build_path(path_key, sizeof(path_key), topic_dir, "keys", key, NULL)) {
        return -1;  // Error already logged
    }
    if(is_directory(path_key)) {
        if(rmrdir(path_key)<0) {
            gobj_log_critical(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "path",         "%s", path_key,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot delete subdir key. rmrdir() FAILED",
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            /*
             *  Some of its files may be gone: the cache of the key is read
             *  again from what is left, and the iterators take their
             *  segments again from it. Nothing is announced.
             */
            json_t *key_cache = load_key_cache_from_disk(gobj, topic_dir, key);
            if(json_array_size(json_object_get(key_cache, "files")) > 0 ||
                    json_array_size(json_object_get(key_cache, "unreadable")) > 0) {
                json_object_set_new(topic_cache, key, key_cache);
                update_totals_of_key_cache(gobj, topic, key);   // Errors already logged
            } else {
                JSON_DECREF(key_cache)
                json_object_del(topic_cache, key);
            }
            forget_segments_of_key(topic, key);
            return -1;
        }
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "path",         "%s", path_key,
            "msg",          "%s", "key directory not found",
            NULL
        );
        // Don't return -1, if key not exist let remove from memory
    }

    /*
     *  Remove key from topic_cache
     */
    json_object_del(topic_cache, key);
    forget_segments_of_key(topic, key);

    /*
     *  Propagate the delete to external followers first (rmrdir of
     *  topic/disks/<rt_id>/<key>/ — inotify fan-out on their side),
     *  then to local in-process subscribers.
     */
    mirror_key_delete_to_disks(gobj, tranger, topic, key);
    fire_key_deleted_locally(gobj, tranger, topic, key);  // in-process non-watcher subs

    return 0;
}

/***************************************************************************
    Get md record from disk to re-rewrite or re-read
    Return fd and offset
 ***************************************************************************/
PRIVATE int get_md_record_for_wr(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,      // In old tranger with 'rowid' was enough to get a record md
    const char *key,    // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t i_rowid,
    md2_record_t *md_record,
    off_t *p_offset
)
{
    memset(md_record, 0, sizeof(md2_record_t));
    *p_offset = 0;

    if(i_rowid == 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "rowid 0",
            "topic",        "%s", tranger2_topic_name(topic),
            "i_rowid",      "%lu", (unsigned long)i_rowid,
            NULL
        );
        return -1;
    }

    /*
     *  This is a READ before a modify, and the whole of the pure read
     *  tranger2_read_user_flag(): a (key, __t__) with no md2 file is the
     *  caller's, and get_topic_wr_fd() would CREATE the file on a master.
     *  None of the three criticals below exits: they return before writing.
     */
    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
    char file_id[NAME_MAX];
    if(!get_file_id(
        file_id,
        sizeof(file_id),
        tranger,
        topic,
        (system_flag & sf_t_ms)? __t__/1000:__t__
    )) {
        // Error already logged
        return -1;
    }
    char filename[NAME_MAX*2];
    snprintf(filename, sizeof(filename), "%s.md2", file_id);
    char full_path[PATH_MAX];
    build_path(full_path, sizeof(full_path),
        json_string_value(json_object_get(topic, "directory")), "keys", key, filename, NULL
    );
    if(!is_regular_file(full_path)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Record metadata file not found",
            "topic",        "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            "__t__",        "%lu", (unsigned long)__t__,
            "path",         "%s", full_path,
            NULL
        );
        return -1;
    }

    int md2_fd = get_topic_wr_fd(gobj, tranger, topic, key, FALSE, file_id);
    if(md2_fd < 0) {
        // Error already logged
        return -1;
    }

    off_t offset = (off_t) ((i_rowid-1) * sizeof(md2_record_t));
    off_t offset_ = lseek(md2_fd, offset, SEEK_SET);
    if(offset != offset_) {
        gobj_log_critical(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "lseek() failed, topic_idx.md corrupted",
            "topic",        "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "offset",       "%lu", (unsigned long)offset,
            "offset_",      "%lu", (unsigned long)offset_,
            NULL
        );
        return -1;
    }

    *p_offset = offset;

    size_t ln = read( // read direct md
        md2_fd,
        md_record,
        sizeof(md2_record_t)
    );
    if(ln != sizeof(md2_record_t)) {
        gobj_log_critical(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read record metadata, read FAILED",
            "topic",        "%s", tranger2_topic_name(topic),
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            "offset",       "%lu", (unsigned long)offset,
            "md2_fd",       "%d", (int)md2_fd,
            "ln",           "%d", (int)ln,
            NULL
        );
        return -1;
    }

    md_record->__t__ = ntohll(md_record->__t__);
    md_record->__tm__ = ntohll(md_record->__tm__);
    md_record->__offset__ = ntohll(md_record->__offset__);
    md_record->__size__ = ntohll(md_record->__size__);

    if(get_time_t(md_record) != __t__) {
        gobj_log_critical(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "__t__ not match, topic_idx.md corrupted",
            "topic",        "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "t1",           "%lu", (unsigned long)get_time_t(md_record),
            "t2",           "%lu", (unsigned long)__t__,
            NULL
        );
        return -1;
    }

    return md2_fd;
}

/***************************************************************************
    Re-Write new record metadata to file
    This function works directly in disk,
        WARNING cache segments in memory are not used or updated
 ***************************************************************************/
PRIVATE int rewrite_md_to_file(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    int md2_fd,
    off_t offset_,
    md2_record_t *md_record
)
{
    off_t offset = lseek(md2_fd, offset_, SEEK_SET);
    if(offset != offset_) {
        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED) | LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "lseek() failed, topic_idx.md corrupted",
            "topic",        "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "offset",       "%lu", (unsigned long)offset,
            "offset_",      "%lu", (unsigned long)md_record->__offset__,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------*
     *  re-write md2 in big endian
     *--------------------------------------------*/
    md2_record_t big_endian;
    big_endian.__t__ = htonll(md_record->__t__);
    big_endian.__tm__ = htonll(md_record->__tm__);
    big_endian.__offset__ = htonll(md_record->__offset__);
    big_endian.__size__ = htonll(md_record->__size__);

    size_t ln = write( // write md
        md2_fd,
        &big_endian,
        sizeof(md2_record_t)
    );
    if(ln != sizeof(md2_record_t)) {
        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot re-write record metadata, write FAILED",
            "topic",        "%s", tranger2_topic_name(topic),
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    /*
     *  Update cache
     */
    // NO! This function is to low-level management of records as queues

    return 0;
}

/***************************************************************************
    Write record user flag
    This function works directly in disk, segments in memory not used or updated
 ***************************************************************************/
PUBLIC int tranger2_write_user_flag(
    json_t *tranger,
    const char *topic_name, // In old tranger with 'rowid' was enough to get a record md
    const char *key,        // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t rowid,         // Must be real rowid in the file, not in topic global rowid
    uint16_t user_flag
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*
     *  A rewrite of a md2 row in place is a write: on a replica (or a
     *  master that lost its lock) the fd is read-only, and the failed
     *  write was a CRITICAL -- an exit(0) with the default
     *  on_critical_error (M-B of the independent review of the second
     *  fix round).
     */
    if(!tranger_is_master(gobj, tranger)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Only master can write",
            "topic_name",   "%s", topic_name,
            "key",          "%s", key,
            NULL
        );
        return -1;
    }
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot open topic",
            "topic",        "%s", topic_name,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
    if(system_flag & sf_rowid_key) {
        key = "__rowid__";
    }

    off_t offset;
    md2_record_t md_record;
    int md_fd = get_md_record_for_wr(
        gobj,
        tranger,
        topic,
        key,
        __t__,
        rowid,
        &md_record,
        &offset
    );
    if(md_fd < 0) {
        // Error already logged
        return -1;
    }

    set_user_flag(&md_record, user_flag);

    if(rewrite_md_to_file(
        gobj,
        tranger,
        topic,
        key,
        md_fd,
        offset,
        &md_record
    )<0) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
    Write record user flag using mask
    This function works directly in disk, segments in memory not used or updated
 ***************************************************************************/
PUBLIC int tranger2_set_user_flag(
    json_t *tranger,
    const char *topic_name, // In old tranger with 'rowid' was enough to get a record md
    const char *key,        // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t i_rowid,         // Must be real rowid in the file, not in topic global rowid
    uint16_t mask,
    BOOL set
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*
     *  A rewrite of a md2 row in place is a write: on a replica (or a
     *  master that lost its lock) the fd is read-only, and the failed
     *  write was a CRITICAL -- an exit(0) with the default
     *  on_critical_error (M-B of the independent review of the second
     *  fix round).
     */
    if(!tranger_is_master(gobj, tranger)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Only master can write",
            "topic_name",   "%s", topic_name,
            "key",          "%s", key,
            NULL
        );
        return -1;
    }
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot open topic",
            "topic",        "%s", topic_name,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
    if(system_flag & sf_rowid_key) {
        key = "__rowid__";
    }

    off_t offset;
    md2_record_t md_record;
    int md_fd = get_md_record_for_wr(
        gobj,
        tranger,
        topic,
        key,
        __t__,
        i_rowid,
        &md_record,
        &offset
    );
    if(md_fd < 0) {
        // Error already logged
        return -1;
    }

    uint16_t user_flag = get_user_flag(&md_record);
    if(set) {
        /*
         *  Set
         */
        user_flag |= mask;

    } else {
        /*
         *  Reset
         */
        user_flag  &= ~mask;
    }
    set_user_flag(&md_record, user_flag);

    if(rewrite_md_to_file(
        gobj,
        tranger,
        topic,
        key,
        md_fd,
        offset,
        &md_record
    )<0) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
    Set/reset writable system flag bits using a mask.
    Only sf_immutable_record is writable through this API; any other bit
    is refused so a caller cannot flip key-type / ms / tombstone / loading
    bits. This function works directly in disk, segments in memory not used
    or updated.
 ***************************************************************************/
PUBLIC int tranger2_set_system_flag(
    json_t *tranger,
    const char *topic_name, // In old tranger with 'rowid' was enough to get a record md
    const char *key,        // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t i_rowid,       // Must be real rowid in the file, not in topic global rowid
    uint16_t mask,
    BOOL set
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*
     *  A rewrite of a md2 row in place is a write: on a replica (or a
     *  master that lost its lock) the fd is read-only, and the failed
     *  write was a CRITICAL -- an exit(0) with the default
     *  on_critical_error (M-B of the independent review of the second
     *  fix round).
     */
    if(!tranger_is_master(gobj, tranger)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Only master can write",
            "topic_name",   "%s", topic_name,
            "key",          "%s", key,
            NULL
        );
        return -1;
    }

    /*
     *  Gate: only the immutable-record bit may be written through this API.
     */
    if(mask & ~(uint16_t)sf_immutable_record) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot set system_flag: bit not writable",
            "topic_name",   "%s", topic_name,
            "mask",         "%d", (int)mask,
            NULL
        );
        return -1;
    }

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot open topic",
            "topic",        "%s", topic_name,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    system_flag2_t topic_flag = json_integer_value(json_object_get(topic, "system_flag"));
    if(topic_flag & sf_rowid_key) {
        key = "__rowid__";
    }

    off_t offset;
    md2_record_t md_record;
    int md_fd = get_md_record_for_wr(
        gobj,
        tranger,
        topic,
        key,
        __t__,
        i_rowid,
        &md_record,
        &offset
    );
    if(md_fd < 0) {
        // Error already logged
        return -1;
    }

    uint16_t system_flag = get_system_flag(&md_record);
    if(set) {
        system_flag |= mask;
    } else {
        system_flag &= ~mask;
    }
    set_system_flag(&md_record, system_flag);

    if(rewrite_md_to_file(
        gobj,
        tranger,
        topic,
        key,
        md_fd,
        offset,
        &md_record
    )<0) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
    Delete a single instance (one row of the per-key .md2 index).
    Mutates the .md2 row in place: OR `sf_deleted_instance` into
    the system_flag bits. Read paths (iterator history, iterator
    pages, rt_by_disk follower) skip rows with the bit set.

    If zero_payload, also overwrites the matching __size__ bytes
    at __offset__ in `data/<mask>.json` with zeros (sensitive-data
    wipe). Opt-in: after the wipe the .json file is no longer a
    parseable concatenation — only the .md2 index makes sense of it.

    Irrecoverable. Master-only. Segments in memory and topic_cache
    are not updated (see header doc).
 ***************************************************************************/
PUBLIC int tranger2_delete_instance(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    uint64_t __t__,
    uint64_t rowid,
    BOOL zero_payload
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    BOOL master = tranger_is_master(gobj, tranger);

    if(!master) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Only master can delete",
            "topic_name",   "%s", topic_name,
            "key",          "%s", key,
            NULL
        );
        return -1;
    }

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot open topic",
            "topic",        "%s", topic_name,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    system_flag2_t topic_flag = json_integer_value(json_object_get(topic, "system_flag"));
    if(topic_flag & sf_rowid_key) {
        key = "__rowid__";
    }

    /*----------------------------------------*
     *  Defense-in-depth: `key` is forwarded as
     *  a single keys/<key>/ path component to
     *  get_md_record_for_wr / get_topic_wr_fd
     *  (which mkrdir on master). Reject path
     *  metacharacters here too, with the SAME
     *  predicate as the append/delete boundary,
     *  so a future direct caller passing an
     *  unvalidated key cannot escape keys/. The
     *  sf_rowid_key value ("__rowid__") passes.
     *----------------------------------------*/
    if(empty_string(key) ||
       strchr(key, '/') != NULL ||
       key[0] == '.') {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Invalid key (path metacharacters not allowed)",
            "topic_name",   "%s", topic_name,
            "key",          "%s", key,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *  Load the md row, OR the tombstone bit
     *----------------------------------------*/
    off_t offset;
    md2_record_t md_record;
    int md_fd = get_md_record_for_wr(
        gobj,
        tranger,
        topic,
        key,
        __t__,
        rowid,
        &md_record,
        &offset
    );
    if(md_fd < 0) {
        // Error already logged
        return -1;
    }

    /*
     *  Capture payload locator BEFORE we touch the md row (we need
     *  __offset__/__size__ for the optional wipe below).
     */
    uint64_t payload_offset = md_record.__offset__;
    uint64_t payload_size   = md_record.__size__;

    uint16_t system_flag = get_system_flag(&md_record);
    if(system_flag & sf_immutable_record) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot delete instance: immutable record",
            "topic_name",   "%s", topic_name,
            "key",          "%s", key,
            NULL
        );
        return -1;
    }
    if(system_flag & sf_deleted_instance) {
        // Already dead. Nothing to do, but not an error.
        return 0;
    }
    system_flag |= sf_deleted_instance;
    set_system_flag(&md_record, system_flag);

    if(rewrite_md_to_file(
        gobj,
        tranger,
        topic,
        key,
        md_fd,
        offset,
        &md_record
    )<0) {
        // Error already logged
        return -1;
    }

    /*----------------------------------------*
     *  Optional payload wipe
     *----------------------------------------*/
    if(zero_payload && payload_size > 0) {
        char file_id[NAME_MAX];
        if(!get_file_id(
            file_id,
            sizeof(file_id),
            tranger,
            topic,
            (topic_flag & sf_t_ms)? __t__/1000:__t__
        )) {
            // Error already logged
            return -1;
        }
        int data_fd = get_topic_wr_fd(gobj, tranger, topic, key, TRUE, file_id);
        if(data_fd < 0) {
            // Error already logged
            return -1;
        }

        /*
         *  Bound the wipe to within the data file before zeroing. payload_offset/
         *  payload_size come straight off the on-disk md row; a forged/corrupt
         *  header would otherwise drive a zeroing write across an arbitrary span
         *  of the data file (cross-record overwrite / data-destruction). Same
         *  offset+size-vs-filesize validation as read_record_content. Order
         *  matters: check offset <= filesize first so the subtraction can't wrap.
         */
        struct stat data_st;
        if(fstat(data_fd, &data_st) < 0) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED) | LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "fstat() on data file FAILED",
                "topic",        "%s", topic_name,
                "key",          "%s", key,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            return -1;
        }
        if(payload_offset > (uint64_t)data_st.st_size ||
                payload_size > (uint64_t)data_st.st_size - payload_offset) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED) | LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Bad on-disk record: __offset__/__size__ out of range; refusing payload wipe",
                "topic",        "%s", topic_name,
                "key",          "%s", key,
                "offset",       "%lu", (unsigned long)payload_offset,
                "size",         "%lu", (unsigned long)payload_size,
                "filesize",     "%lu", (unsigned long)data_st.st_size,
                NULL
            );
            return -1;
        }

        off_t off_ = lseek(data_fd, (off_t)payload_offset, SEEK_SET);
        if(off_ != (off_t)payload_offset) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED) | LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "lseek() on data file FAILED",
                "topic",        "%s", topic_name,
                "key",          "%s", key,
                "offset",       "%lu", (unsigned long)payload_offset,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        char buf[4096];
        memset(buf, 0, sizeof(buf));
        uint64_t remaining = payload_size;
        while(remaining > 0) {
            size_t chunk = (remaining > sizeof(buf))? sizeof(buf) : (size_t)remaining;
            ssize_t wrote = write(data_fd, buf, chunk);
            if(wrote != (ssize_t)chunk) {
                gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED) | LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "write() zero payload FAILED",
                    "topic",        "%s", topic_name,
                    "key",          "%s", key,
                    "remaining",    "%lu", (unsigned long)remaining,
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
                return -1;
            }
            remaining -= chunk;
        }
    }

    return 0;
}

/***************************************************************************
    Read record user flag
    This function works directly in disk, segments in memory not used or updated
 ***************************************************************************/
PUBLIC uint16_t tranger2_read_user_flag(
    json_t *tranger,
    const char *topic_name, // In old tranger with 'rowid' was enough to get a record md
    const char *key,        // In tranger2 ('key', '__t__', 'rowid') is required
    uint64_t __t__,
    uint64_t rowid          // Must be real rowid in the file, not in topic global rowid
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot open topic",
            "topic",        "%s", topic_name,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return 0;
    }

    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
    if(system_flag & sf_rowid_key) {
        key = "__rowid__";
    }

    off_t offset;
    md2_record_t md_record;
    int md_fd = get_md_record_for_wr(
        gobj,
        tranger,
        topic,
        key,
        __t__,
        rowid,
        &md_record,
        &offset
    );
    if(md_fd < 0) {
        // Error already logged
        return -1;
    }
    uint16_t user_flag = get_user_flag(&md_record);
    return user_flag;
}

/***************************************************************************
 *  Open realtime by mem
 ***************************************************************************/
PUBLIC json_t *tranger2_open_rt_mem(
    json_t *tranger,
    const char *topic_name,
    const char *key,        // if empty receives all keys, else only this key
    json_t *match_cond,     // owned
    tranger2_load_record_callback_t load_record_callback,   // called on append new record on mem
    const char *id,         // list id, optional
    const char *creator,
    json_t *extra           // owned, user data, this json will be added to the return iterator
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    if(!match_cond) {
        match_cond = json_object();
    }
    if(!key) {
        key="";
    }
    if(!creator) {
        creator="";
    }

    /*
     *  Here the topic is opened if it's not opened
     */
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_open_rt_mem: what topic?",
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return NULL;
    }

    if(!load_record_callback) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_open_rt_mem: what load_record_callback?",
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return NULL;
    }

    json_t *mem = json_object();
    char id_[64];
    if(empty_string(id)) {
        snprintf(id_, sizeof(id_), "%"PRIu64, (uint64_t )(size_t)mem);
        id = id_;
    }

    if(tranger2_get_rt_mem_by_id(tranger, topic_name, id, creator)) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_open_rt_mem(): Mem already exists",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(mem)
        JSON_DECREF(extra)
        return NULL;
    }

    json_object_update_new(mem, json_pack("{s:s, s:s, s:s, s:s, s:o, s:I}",
        "id", id,
        "creator", creator,
        "topic_name", topic_name,
        "key", key,
        "match_cond", match_cond,
        "load_record_callback", (json_int_t)(uintptr_t)load_record_callback
    ));
    json_object_set_new(mem, "list_type", json_string("rt_mem"));
    json_object_update_missing_new(mem, extra);

    /*
     *  A KEYLESS feed receives every key of the topic — unless its match_cond
     *  carries an `rkey`, which narrows it to the keys that match. A malformed
     *  rkey refuses the open: degrading it to "every key" would push the client
     *  exactly the records it asked not to receive.
     */
    if(rkey_arm(gobj, mem, key, json_object_get(mem, "match_cond"))<0) {
        JSON_DECREF(mem)    // Error already logged
        return NULL;
    }

    /*
     *  Add the mem to the topic
     */
    json_array_append_new(
        kw_get_dict_value(gobj, topic, "lists", 0, KW_REQUIRED),
        mem
    );

    if(gobj_global_trace_level() & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_YEV_LOOP,
            "msg",              "%s", "open rt mem",
            "msg2",             "%s", "📝🔷 open rt mem",
            "topic_name",       "%s", topic_name,
            "key",              "%s", key,
            "rt_id",            "%s", id?id:"",
            "creator",          "%s", creator?creator:"",
            "p",                "%p", mem,
            NULL
        );
    }

    return mem;
}

/***************************************************************************
 *  Close realtime list
 ***************************************************************************/
PUBLIC int tranger2_close_rt_mem(
    json_t *tranger,
    json_t *mem
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    if(!mem) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_close_rt_mem(): mem NULL",
            NULL
        );
        return -1;
    }

    const char *id = kw_get_str(gobj, mem, "id", "", 0);
    const char *topic_name = kw_get_str(gobj, mem, "topic_name", "", KW_REQUIRED);

    if(gobj_global_trace_level() & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_YEV_LOOP,
            "msg",              "%s", "open rt mem",
            "msg2",             "%s", "📝🔶 close rt mem",
            "topic_name",       "%s", topic_name,
            "id",               "%s", id,
            "p",                "%p", mem,
            NULL
        );
    }

    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, KW_REQUIRED);

    json_t *lists = kw_get_dict_value(gobj, topic, "lists", 0, KW_REQUIRED);
    if(!lists) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_close_rt_mem(): lists not found",
            NULL
        );
        return -1;
    }

    rkey_disarm(mem);   /*  its compiled rkey dies with it  */

    int idx = json_array_find_idx(lists, mem);
    if(idx >=0 && idx < json_array_size(lists)) {
        json_array_remove(
            lists,
            idx
        );
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_close_rt_mem(): list not found",
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  Get realtime list by his id
 ***************************************************************************/
PUBLIC json_t *tranger2_get_rt_mem_by_id(
    json_t *tranger,
    const char *topic_name,
    const char *id,  // rt_id
    const char *creator
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(empty_string(id)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_get_rt_mem_by_id: what id?",
            NULL
        );
        return 0;
    }
    if(!creator) {
        creator = "";
    }

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(topic) {
        json_t *lists = kw_get_list(gobj, topic, "lists", 0, KW_REQUIRED);
        int idx; json_t *list;
        json_array_foreach(lists, idx, list) {
            const char *rt_id = kw_get_str(gobj, list, "id", "", 0);
            if(strcmp(id, rt_id)==0) {
                const char *creator_ = json_string_value(
                    json_object_get(list, "creator")
                );
                if(empty_string(creator) && empty_string(creator_)) {
                    return list;
                }
                if(strcmp(creator, creator_)==0) {
                    return list;
                } else {
                    continue;
                }
            }
        }
    }

    // Be silence, check at top.
    return 0;
}

/***************************************************************************
 *  Open realtime by disk,
 *  valid when the yuno is the master writing or not-master reading,
 *  realtime messages from events of disk
 *  WARNING could arrives the last message
 *      or an middle message (by example, a deleted message or md changed)
 ***************************************************************************/
PUBLIC json_t *tranger2_open_rt_disk(
    json_t *tranger,
    const char *topic_name,
    const char *key,        // if empty receives all keys, else only this key
    json_t *match_cond,     // owned
    tranger2_load_record_callback_t load_record_callback,   // called on append new record on disk
    const char *id,         // disk id, REQUIRED: the name of disks/<id>/ (empty is refused)
    const char *creator,
    json_t *extra           // owned, user data, this json will be added to the return iterator
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    if(!match_cond) {
        match_cond = json_object();
    }
    if(!key) {
        key="";
    }
    if(!creator) {
        creator="";
    }

    /*
     *  Here the topic is opened if it's not opened
     */
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "what topic?",
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return NULL;
    }

    if(!load_record_callback) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "what load_record_callback?",
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return NULL;
    }

    /*
     *  The id may come from a peer (C_TRANGER's open-rt / open-list): an
     *  empty one is refused like any other bad id, with a warning.
     */
    if(!rt_id_is_confined(gobj, topic, id, __FUNCTION__)) {
        // Error already logged
        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return NULL;
    }

    json_t *disk = json_object();

    /*
     *  The directory `disks/<id>/` is keyed by the id ALONE, and an open
     *  rmrdir()s it before creating it: a second feed with the id of a live
     *  one, under another creator, took its directory over, and its close
     *  removed it -- the first feed (a treedb's, on a replica) stopped
     *  receiving for good, without a log. One id, one feed, whoever opens it.
     *  The same creator opening it twice is the same refusal, and the same
     *  warning: the id may come from a peer in both cases.
     *
     *  Per PROCESS: the list of feeds is this tranger's. Two processes that
     *  follow one store with the same id still take each other's directory.
     */
    json_t *disks_ = kw_get_list(gobj, topic, "disks", 0, KW_REQUIRED);
    int idx_; json_t *disk_;
    json_array_foreach(disks_, idx_, disk_) {
        if(strcmp(kw_get_str(gobj, disk_, "id", "", 0), id)==0) {
            BOOL same_creator = strcmp(kw_get_str(gobj, disk_, "creator", "", 0), creator)==0;
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", same_creator?
                    "rt disk id already in use by the same creator, refused" :
                    "rt disk id already in use by another creator, refused",
                "topic_name",   "%s", tranger2_topic_name(topic),
                "key",          "%s", key,
                "id",           "%s", id,
                "creator",      "%s", creator,
                "owner",        "%s", kw_get_str(gobj, disk_, "creator", "", 0),
                NULL
            );
            gobj_log_set_last_message("rt disk id '%s' already in use", id);
            JSON_DECREF(match_cond)
            JSON_DECREF(disk)
            JSON_DECREF(extra)
            return NULL;
        }
    }

    json_object_update_new(disk, json_pack("{s:s, s:s, s:s, s:s, s:o, s:I}",
        "id", id,
        "creator", creator,
        "topic_name", topic_name,
        "key", key,
        "match_cond", match_cond,
        "load_record_callback", (json_int_t)(uintptr_t)load_record_callback
    ));
    json_object_update_missing_new(disk, extra);
    json_object_set_new(disk, "list_type", json_string("rt_disk"));

    /*
     *  Same contract as rt_mem: keyless + rkey = only the keys that match it.
     */
    if(rkey_arm(gobj, disk, key, json_object_get(disk, "match_cond"))<0) {
        JSON_DECREF(disk)   // Error already logged
        return NULL;
    }

    /*
     *  Add the list to the topic
     */
    json_array_append_new(
        kw_get_dict_value(gobj, topic, "disks", 0, KW_REQUIRED),
        disk
    );

    if(gobj_global_trace_level() & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_YEV_LOOP,
            "msg",              "%s", "open rt disk",
            "msg2",             "%s", "💽🔷 open rt disk",
            "topic_name",       "%s", topic_name,
            "key",              "%s", key,
            "rt_id",            "%s", id?id:"",
            "creator",          "%s", creator?creator:"",
            "p",                "%p", disk,
            NULL
        );
    }

    /*
     *  Create in disk the realtime disk directory and monitor
     */
    yev_loop_h yev_loop = (yev_loop_h)kw_get_int(gobj, tranger, "yev_loop", 0, KW_REQUIRED);
    if(yev_loop) {
        // Master can operate as a non-master and operate through the disk (NOT SENSE, only to test)
        // BOOL master = json_boolean_value(json_object_get(tranger, "master"));
        if(1) {
            // (2) MONITOR (C) (MI)r /disks/rt_id/
            // The directory is created inside this function
            // log FS inside this function

            fs_event_t *fs_event_client = monitor_rt_disk_by_client(
                gobj,
                yev_loop,
                tranger,
                topic,
                key,
                id
            );
            kw_set_dict_value(
                gobj,
                disk,
                "fs_event_client",
                json_integer((json_int_t)(uintptr_t)fs_event_client)
            );
        }
    }

    return disk;
}

/***************************************************************************
 *  Close realtime disk
 ***************************************************************************/
PUBLIC int tranger2_close_rt_disk(
    json_t *tranger,
    json_t *disk
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    if(!disk) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_close_rt_disk(): disk NULL",
            NULL
        );
        return -1;
    }

    const char *id = kw_get_str(gobj, disk, "id", "", 0);
    const char *topic_name = kw_get_str(gobj, disk, "topic_name", "", KW_REQUIRED);

    if(gobj_global_trace_level() & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_YEV_LOOP,
            "msg",              "%s", "open rt mem",
            "msg2",             "%s", "💽🔶 close rt disk",
            "topic_name",       "%s", topic_name,
            "id",               "%s", id,
            "p",                "%p", disk,
            NULL
        );
    }

    rkey_disarm(disk);  /*  its compiled rkey dies with it  */

    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, KW_REQUIRED);

    yev_loop_h yev_loop = (yev_loop_h)kw_get_int(gobj, tranger, "yev_loop", 0, KW_REQUIRED);
    if(yev_loop) {
        // MONITOR Client Unwatching (MI) topic /disks/rt_id/
        if(gobj_global_trace_level() & TRACE_FS) {
            gobj_log_debug(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_YEV_LOOP,
                "msg",              "%s", "CLIENT: Unwatching (MI) topic /disks/rt_id/",
                "msg2",             "%s", "👓🔶 CLIENT: Unwatching (MI) topic /disks/rt_id/",
                "action",           "%s", "fs_stop_watcher_event()",
                NULL
            );
        }

        fs_event_t *fs_event_client = (fs_event_t *)kw_get_int(
            gobj, disk, "fs_event_client", 0, KW_REQUIRED
        );
        if(fs_event_client) {
            fs_stop_watcher_event(fs_event_client);
        }

        // MONITOR (D) /disks/rt_id/
        char full_path[PATH_MAX];
        const char *directory = kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED);
        snprintf(full_path, sizeof(full_path), "%s/disks/%s",
            directory,
            kw_get_str(gobj, disk, "id", "", KW_REQUIRED)
        );

        if(gobj_global_trace_level() & TRACE_FS) {
            gobj_log_debug(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_YEV_LOOP,
                "msg",              "%s", "CLIENT: (D) /disks/rt_id/",
                "msg2",             "%s", "👓🔶 CLIENT: (D) /disks/rt_id/",
                "action",           "%s", "rmrdir()",
                "full_path",        "%s", full_path,
                NULL
            );
        }
        rmrdir(full_path);
    }

    json_t *disks = kw_get_dict_value(gobj, topic, "disks", 0, KW_REQUIRED);
    if(!disks) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_close_rt_disk(): disks not found",
            NULL
        );
        return -1;
    }

    int idx = json_array_find_idx(disks, disk);
    if(idx >=0 && idx < json_array_size(disks)) {
        json_array_remove(
            disks,
            idx
        );
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_close_rt_disk(): disk not found",
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  Get realtime disk by his id
 ***************************************************************************/
PUBLIC json_t *tranger2_get_rt_disk_by_id(
    json_t *tranger,
    const char *topic_name,
    const char *id,  // rt_id
    const char *creator
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(empty_string(id)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_get_rt_disk_by_id: what id?",
            NULL
        );
        return 0;
    }
    if(!creator) {
        creator = "";
    }
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(topic) {
        json_t *disks = kw_get_list(gobj, topic, "disks", 0, KW_REQUIRED);
        int idx; json_t *disk;
        json_array_foreach(disks, idx, disk) {
            const char *rt_id = kw_get_str(gobj, disk, "id", "", 0);
            if(strcmp(id, rt_id)==0) {
                const char *creator_ = json_string_value(
                    json_object_get(disk, "creator")
                );
                if(empty_string(creator) && empty_string(creator_)) {
                    return disk;
                }
                if(strcmp(creator, creator_)==0) {
                    return disk;
                } else {
                    continue;
                }
            }
        }
    }

    // Be silence, check at top.
    return 0;
}

/***************************************************************************
 *  Register a key-delete callback on a rt_mem / rt_disk / iterator
 *  handle. See typedef tranger2_key_deleted_callback_t in the header.
 *
 *  Stored on the handle as two int slots so it survives across
 *  master/follower processes when the handle is serialized.
 ***************************************************************************/
PUBLIC int tranger2_set_rt_key_deleted_callback(
    json_t *list,
    tranger2_key_deleted_callback_t cb,
    void *user_data
)
{
    if(!list) {
        return -1;
    }
    json_object_set_new(list,
        "key_deleted_callback",
        json_integer((json_int_t)(uintptr_t)cb));
    json_object_set_new(list,
        "key_deleted_user_data",
        json_integer((json_int_t)(uintptr_t)user_data));
    return 0;
}

/***************************************************************************
 *  MASTER: find realtime disks of clients
 ***************************************************************************/
PRIVATE BOOL find_rt_disk_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,     // type found
    char *full_path,        // directory+filename found
    const char *directory,  // directory of found filename
    char *filename,         // dname[255]
    int level,              // level of tree where file found
    wd_option opt           // option parameter
)
{
    char full_path2[PATH_MAX];
    // Copy the full path to use later, this will be destroyed
    snprintf(full_path2, sizeof(full_path2), "%s", full_path);

    json_t *tranger = user_data;
    char *rt_id = pop_last_segment(full_path);
    char *disks = pop_last_segment(full_path);
    char *topic_name = pop_last_segment(full_path);

    if(strcmp(disks, "disks")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Bad path master 0 /disks/rt_id/",
            "directory",    "%s", directory,
            "filename",     "%s", filename,
            NULL
        );
        return TRUE; // continue
    }

    json_t *rt = tranger2_open_rt_mem(
        tranger,
        topic_name,
        "",         // key, if empty receives all keys, else only this key
        json_pack("{s:b}",  // match_cond, all records, only_md
            "only_md", 1
        ),
        master_to_update_client_load_record_callback,   // called on append new record
        rt_id,
        "", // creator
        NULL
    );

    json_object_set_new(rt, "disk_path", json_string(full_path2));
    json_object_set_new(rt, "master_to_update_client", json_true());

    return TRUE; // to continue
}
PRIVATE void find_rt_disk(json_t *tranger, const char *path)
{
    walk_dir_tree(
        0,
        path,
        0,
        WD_MATCH_DIRECTORY,
        find_rt_disk_cb,
        tranger
    );
}

/***************************************************************************
 *  MASTER Watch create/delete subdirectories of disk realtime id's
 *      that creates/deletes non-master
 ***************************************************************************/
PRIVATE fs_event_t *monitor_disks_directory_by_master(
    hgobj gobj,
    yev_loop_h yev_loop,
    json_t *tranger,
    json_t *topic
)
{
    char full_path[PATH_MAX];
    const char *directory = kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED);
    snprintf(full_path, sizeof(full_path), "%s/disks",
        directory
    );

    /*
     *  Find current rt_id's of clients, open a tranger2_open_rt_mem for each
     */
    find_rt_disk(tranger, full_path);

    /*
     *  Monitor future rt_id's of clients, will open a tranger2_open_rt_mem for each
     */
    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop,
        full_path,
        0,      // fs_flag,
        master_fs_callback,
        gobj,
        tranger,    // user_data
        (void *)1           // user_data2
    );
    if(!fs_event) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "fs_create_watcher_event FAILED",
            NULL
        );
        return NULL;
    }
    fs_start_watcher_event(fs_event);
    return fs_event;
}

/***************************************************************************
 *  MASTER
 *  Clients will create a rt disk directory in /disks when open a rt disk
 ***************************************************************************/
PRIVATE int master_fs_callback(fs_event_t *fs_event)
{
    hgobj gobj = fs_event->gobj;
    json_t *tranger = fs_event->user_data;

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path),
        "%s/%s",
        (char *)fs_event->directory,
        (char *)fs_event->filename
    );

    switch(fs_event->fs_type) {
        case FS_SUBDIR_CREATED_TYPE:
            {
                // (3) MONITOR Client has opened a rt disk for the topic,
                // Master to open a mem rt to update /disks/rt_id/

                char full_path2[PATH_MAX];
                snprintf(full_path2, sizeof(full_path2),
                    "%s/%s",
                    (char *)fs_event->directory,
                    (char *)fs_event->filename
                );

                char *rt_id = pop_last_segment(full_path);
                char *disks = pop_last_segment(full_path);
                char *topic_name = pop_last_segment(full_path);

                if(gobj_global_trace_level() & TRACE_FS) {
                    gobj_log_debug(gobj, 0,
                        "function",         "%s", __FUNCTION__,
                        "msgset",           "%s", MSGSET_YEV_LOOP,
                        "msg",              "%s", "MASTER: Directory created by client",
                        "msg2",             "%s", "💾🔷 MASTER: Directory created by client",
                        "full_path",        "%s", full_path2,
                        "action",           "%s", "tranger2_open_rt_mem()",
                        "topic_name",       "%s", topic_name,
                        "disks",            "%s", disks,
                        "rt_id",            "%s", rt_id,
                        NULL
                    );
                }

                if(strcmp(disks, "disks")!=0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "Bad path master 1 /disks/rt_id/",
                        "directory",    "%s", fs_event->directory,
                        "filename",     "%s", fs_event->filename,
                        NULL
                    );
                    break;
                }

                json_t *rt = tranger2_open_rt_mem(
                    tranger,
                    topic_name,
                    "",         // key, if empty receives all keys, else only this key
                    json_pack("{s:b}",  // match_cond, all records, only_md
                        "only_md", 1
                    ),
                    master_to_update_client_load_record_callback, // called on append new record
                    rt_id,
                    "", // creator
                    NULL
                );

                json_object_set_new(rt, "disk_path", json_string(full_path2));
                json_object_set_new(rt, "master_to_update_client", json_true());
            }
            break;
        case FS_SUBDIR_DELETED_TYPE:
            {
                // MONITOR Client has closed a rt disk for the topic,
                // Master to close the mem rt
                char full_path2[PATH_MAX];
                snprintf(full_path2, sizeof(full_path2),
                    "%s/%s",
                    (char *)fs_event->directory,
                    (char *)fs_event->filename
                );

                char *rt_id = pop_last_segment(full_path);
                char *disks = pop_last_segment(full_path);
                char *topic_name = pop_last_segment(full_path);

                if(gobj_global_trace_level() & TRACE_FS) {
                    gobj_log_debug(gobj, 0,
                        "function",         "%s", __FUNCTION__,
                        "msgset",           "%s", MSGSET_YEV_LOOP,
                        "msg",              "%s", "MASTER: Directory deleted by client",
                        "msg2",             "%s", "💾🔶 MASTER: Directory deleted by client",
                        "full_path",        "%s", full_path2,
                        "action",           "%s", "tranger2_close_rt_mem()",
                        "topic_name",       "%s", topic_name,
                        "disks",            "%s", disks,
                        "rt_id",            "%s", rt_id,
                        NULL
                    );
                }

                if(strcmp(disks, "disks")!=0) {
                    /*
                     *  Ignore, must be a key, i.e. /disks/rt_id/key
                     */
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "Bad path master 2 /disks/rt_id/",
                        "directory",    "%s", fs_event->directory,
                        "filename",     "%s", fs_event->filename,
                        NULL
                    );
                    break;
                }

                json_t *rt = tranger2_get_rt_mem_by_id(
                    tranger,
                    topic_name,
                    rt_id,
                    ""
                );
                tranger2_close_rt_mem(tranger, rt);
            }
            break;
        case FS_FILE_CREATED_TYPE:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "FS_FILE_CREATED_TYPE master fs_event NOT processed",
                NULL
            );
            break;
        case FS_FILE_DELETED_TYPE:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "FS_FILE_DELETED_TYPE master fs_event NOT processed",
                NULL
            );
            break;
        case FS_FILE_MODIFIED_TYPE:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "FS_FILE_MODIFIED_TYPE master fs_event NOT processed",
                NULL
            );
            break;
        case FS_FILE_RENAME_TYPE:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "FS_FILE_RENAME_TYPE master fs_event NOT processed",
                NULL
            );
            break;
    }

    return 0;
}

/***************************************************************************
 *  MASTER mem rt callback to update /disks/rt_id/
 ***************************************************************************/
PRIVATE int master_to_update_client_load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list, // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    json_t *record      // must be owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    char full_path_dest[PATH_MAX];
    char full_path_orig[PATH_MAX];

    // (4) MONITOR update directory /disks/rt_id/ on new records
    // Create a hard link of md2 file
    // Log is below

    json_t *rt = list;
    const char *disk_path = json_string_value(json_object_get(rt, "disk_path"));

    /*
     *  Defense-in-depth (behind the append-boundary key validation):
     *  `key` is the record pkey and is used unsanitized to build the
     *  /disks/<rt_id>/<key> mkdir target and the link() destination below.
     *  A traversing key would let mkdir create a directory and link create a
     *  hard link outside disks/<rt_id>/. Use the SAME predicate as the append
     *  boundary ('/' or a leading '.'): a single path component without '/'
     *  that does not start with '.' (so neither '.' nor '..') cannot escape.
     *  Matching the append rule keeps this mirror from silently skipping a key
     *  the master legitimately accepted (e.g. an embedded ".." like "a..b").
     */
    if(empty_string(key) || strchr(key, '/') || key[0] == '.') {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "rt mem callback: invalid key, possible path traversal",
            "key",          "%s", key?key:"",
            "disk_path",    "%s", disk_path?disk_path:"",
            NULL
        );
        JSON_DECREF(record)
        return 0;
    }

    /*
     *  Create the directory for the key
     */
    snprintf(full_path_dest, sizeof(full_path_dest), "%s/%s", disk_path, key);
    if(!is_directory(full_path_dest)) {
        if(gobj_global_trace_level() & TRACE_FS) {
            gobj_log_debug(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_YEV_LOOP,
                "msg",              "%s", "MASTER: rt mem callback",
                "action",           "%s", "create directory of key",
                "path",              "%s", full_path_dest,
                NULL
            );
        }
        if(mkdir(full_path_dest, json_integer_value(json_object_get(tranger, "xpermission")))<0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "mkdir() FAILED",
                "path",         "%s", full_path_dest,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
    }

    /*
     *  Create the hard link for the md2 file
     */
    char filename[NAME_MAX*2];
    system_flag2_t system_flag = md_record_ex->system_flag;

    if(!get_t_filename(
        filename,
        sizeof(filename),
        tranger,
        topic,
        FALSE,
        (system_flag & sf_t_ms)? md_record_ex->__t__/1000 : md_record_ex->__t__
    )) {
        // Error already logged: no file to link into the feed's directory
        JSON_DECREF(record)
        return 0;
    }
    snprintf(full_path_dest, sizeof(full_path_dest), "%s/%s/%s", disk_path, key, filename);

    const char *topic_dir = json_string_value(json_object_get(topic, "directory"));
    snprintf(full_path_orig, sizeof(full_path_orig), "%s/keys/%s/%s", topic_dir, key, filename);

    if(!is_regular_file(full_path_dest)) {
        if(gobj_global_trace_level() & TRACE_FS) {
            gobj_log_debug(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_YEV_LOOP,
                "msg",              "%s", "MASTER: rt mem callback",
                "action",           "%s", "update directory /disks/rt_id/, create hard link",
                "src",              "%s", full_path_orig,
                "dst",              "%s", full_path_dest,
                NULL
            );
        }

        if(link(full_path_orig, full_path_dest)<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "link() FAILED",
                "src",          "%s", full_path_orig,
                "dst",          "%s", full_path_dest,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
    }

    JSON_DECREF(record)
    return 0;
}

/***************************************************************************
 *  CLIENT (or MASTER) Watch create/delete subdirectories
 *  and files of disk realtime id's that creates/deletes master
 ***************************************************************************/
PRIVATE fs_event_t *monitor_rt_disk_by_client(
    hgobj gobj,
    yev_loop_h yev_loop,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *id
)
{
    if(empty_string(id)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "rt_id cannot be empty",
            NULL
        );
        return NULL;
    }

    char full_path[PATH_MAX];
    const char *directory = kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED);
    snprintf(full_path, sizeof(full_path), "%s/disks/%s",
        directory,
        id
    );

    if(gobj_global_trace_level() & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_YEV_LOOP,
            "msg",              "%s", "MONITOR INOTIFY (MI)r /disks/{rt_id}/",
            "msg2",             "%s", "👓🔷 MONITOR INOTIFY (MI)r /disks/{rt_id}/",
            "action",           "%s", "fs_create_watcher_event recursive",
            "path",             "%s", full_path,
            "rt_id",            "%s", id,
            NULL
        );
    }

    /*
     *  the function must delete his directory and re-create,
     *  this is done after reading all records and now to signalize to master to update after now
     */
    if(is_directory(full_path)) {
        rmrdir(full_path);
    }
    if(mkdir(full_path, json_integer_value(json_object_get(tranger, "xpermission")))<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "mkdir() FAILED",
            "path",         "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }

    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop,
        full_path,
        FS_FLAG_RECURSIVE_PATHS,      // fs_flag,
        client_fs_callback,
        gobj,
        tranger,  // user_data
        topic     // user_data2 — needed to resolve key-delete fan-out
    );
    if(!fs_event) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "fs_create_watcher_event FAILED",
            NULL
        );
        return NULL;
    }
    fs_start_watcher_event(fs_event);
    return fs_event;
}

/***************************************************************************
 *  CLIENT: The MASTER signalize a new record appended
 *
 *  Path schema:
 *      /yuneta/store/{SERVICE}/{OWNER}/{REALM_ID}/{TIMERANGER}/{TOPIC}/disks/{CLIENT}/{KEY}
 *  Example:
 *      .../disks/C_ENERGY_HISTORY^energy_history/DVES_000000
 *
 ***************************************************************************/
PRIVATE int client_fs_callback(fs_event_t *fs_event)
{
    hgobj gobj = fs_event->gobj;
    json_t *tranger = fs_event->user_data;
    json_t *watched_topic = fs_event->user_data2;   // set by monitor_rt_disk_by_client

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s",
        (char *)fs_event->directory,
        (char *)fs_event->filename
    );

    switch(fs_event->fs_type) {
        case FS_SUBDIR_CREATED_TYPE: /*  */
            /*
             *  - Key directory created, ignore
             */

            //(5) MONITOR notify of update directory /disks/rt_id/ on new records
            /*
                inotify(7) — Linux manual page

                If monitoring an entire directory subtree, and a new subdirectory
                is created in that tree or an existing directory is renamed into
                that tree, be aware that by the time you create a watch for the
                new subdirectory, new files (and subdirectories) may already
                exist inside the subdirectory.  Therefore, you may want to scan
                the contents of the subdirectory immediately after adding the
                watch (and, if desired, recursively add watches for any
                subdirectories that it contains).
             */
            {
                if(gobj_global_trace_level() & TRACE_FS) {
                    gobj_log_debug(gobj, 0,
                        "function",         "%s", __FUNCTION__,
                        "msgset",           "%s", MSGSET_YEV_LOOP,
                        "msg",              "%s", "CLIENT: Directory created",
                        "msg2",             "%s", "💾🔷 CLIENT: Directory created",
                        "action",           "%s", "ignored",
                        "full_path",        "%s", full_path,
                        NULL
                    );
                }
                if(is_directory(full_path)) {
                    scan_disks_key_for_new_file(gobj, tranger, full_path);
                }
                // else: created and removed at once, the key-delete signal
            }
            break;

        case FS_SUBDIR_DELETED_TYPE:
            /*
             *  Master mirrored a tranger2_delete_key() into our
             *  disks/<rt_id>/<key>/: clear the cache rollup and tell the ONE
             *  feed whose directory fired (the master mirrors the delete into
             *  the directory of every feed, so each feed hears it once, from
             *  its own watcher). Only a directory right under this watcher's
             *  root is a key: the root itself going is the feed closing.
             */
            if(watched_topic &&
               strcmp((const char *)fs_event->directory, fs_event->path)==0) {
                const char *deleted_key = (const char *)fs_event->filename;
                if(gobj_global_trace_level() & TRACE_FS) {
                    gobj_log_debug(gobj, 0,
                        "function",         "%s", __FUNCTION__,
                        "msgset",           "%s", MSGSET_YEV_LOOP,
                        "msg",              "%s", "CLIENT: Key directory deleted",
                        "msg2",             "%s", "💾🔶 CLIENT: Key directory deleted",
                        "action",           "%s", "propagate key-delete",
                        "deleted_key",      "%s", deleted_key,
                        "full_path",        "%s", full_path,
                        NULL
                    );
                }
                json_t *cache = json_object_get(watched_topic, "cache");
                if(cache) {
                    json_object_del(cache, deleted_key);
                }
                forget_segments_of_key(watched_topic, deleted_key);

                json_t *disk = NULL;
                int idx; json_t *disk_;
                json_array_foreach(json_object_get(watched_topic, "disks"), idx, disk_) {
                    fs_event_t *fs = (fs_event_t *)(uintptr_t)json_integer_value(
                        json_object_get(disk_, "fs_event_client")
                    );
                    if(fs == fs_event) {
                        disk = disk_;
                        break;
                    }
                }
                if(disk) {
                    fire_key_deleted_to_feed(tranger, watched_topic, disk, deleted_key);
                } else {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "no rt_disk feed owns this watcher",
                        "deleted_key",  "%s", deleted_key,
                        "path",         "%s", fs_event->path,
                        NULL
                    );
                }
            }
            break;

        case FS_FILE_CREATED_TYPE:
            /*
             *  - Record to key added, read
             *  - Delete the hard link of md2 file when read
             */
            // (5) MONITOR notify of update directory /disks/rt_id/ on new records
            {
                if(gobj_global_trace_level() & TRACE_FS) {
                    gobj_log_debug(gobj, 0,
                        "function",         "%s", __FUNCTION__,
                        "msgset",           "%s", MSGSET_YEV_LOOP,
                        "msg",              "%s", "CLIENT: File created",
                        "msg2",             "%s", "💾🔷 CLIENT: File created",
                        "path",             "%s", full_path,
                        "action",           "%s", "update_key_by_hard_link()",
                        NULL
                    );
                }
                update_key_by_hard_link(gobj, tranger, full_path); // full_path modified */
            }
            break;

        case FS_FILE_DELETED_TYPE:
            /*
             *  - Key file deleted, ignore, it's me
             */
            if(gobj_global_trace_level() & TRACE_FS) {
                gobj_log_debug(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_YEV_LOOP,
                    "msg",              "%s", "CLIENT: File delete",
                    "msg2",             "%s", "💾🔶 CLIENT: File delete",
                    "action",           "%s", "ignore",
                    "full_path",        "%s", full_path,
                    NULL
                );
            }
            break;

        case FS_FILE_MODIFIED_TYPE:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "FS_FILE_MODIFIED_TYPE client fs_event NOT processed",
                NULL
            );
            break;

        case FS_FILE_RENAME_TYPE:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "FS_FILE_RENAME_TYPE client fs_event NOT processed",
                NULL
            );
            break;
    }

    return 0;
}

/***************************************************************************
 *  CLIENT:
 ***************************************************************************/
PRIVATE int scan_disks_key_for_new_file(
    hgobj gobj,
    json_t *tranger,
    char *path
)
{
    dir_array_t da;

    find_files_with_suffix_array(
        gobj,
        path,
        ".md2",
        &da
    );

    dir_array_sort(&da);

    for(int i=0; i<da.count; i++) {
        char *filename = da.items[i];
        char full_path[PATH_MAX];
        build_path(full_path, sizeof(full_path), path, filename, NULL);
        update_key_by_hard_link(gobj, tranger, full_path); // full_path modified */
    }

    dir_array_free(&da);

    return 0;
}

/***************************************************************************
 *  CLIENT:
 ***************************************************************************/
PRIVATE int update_key_by_hard_link(
    hgobj gobj,
    json_t *tranger,
    char *path // WARNING path modified
)
{
    if(gobj_global_trace_level() & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_YEV_LOOP,
            "msg",              "%s", "CLIENT: unlink hard key and update_new_records_from_disk",
            "path",             "%s", path,
            NULL
        );
    }
    if(unlink(path)<0) { // WARNING it's a bit slow!
        if(errno == ENOENT) {
            /*
             *  Not an error: the notification was ALREADY consumed by the other
             *  path. A brand-new key reaches us twice, by construction:
             *
             *    - fs_watcher, on the IN_CREATE of the key directory, adds the
             *      watch on it FIRST and calls us AFTER (fs_watcher.c). In the
             *      window between the two, the master (another process) hard-
             *      links the .md2 inside — so the file gets its OWN IN_CREATE
             *      (FS_FILE_CREATED_TYPE -> here) *and* is found by the
             *      directory scan (scan_disks_key_for_new_file -> here).
             *
             *  Whichever runs second finds the file gone. That is the hazard
             *  the inotify(7) note quoted at FS_SUBDIR_CREATED_TYPE warns about,
             *  and the scan exists precisely to cover it — it just cannot know
             *  which files the watch will also report. Logging it as an ERROR
             *  put one in the log of every follower on every new key (an hourly
             *  bucket = an hourly error) for something that is working exactly
             *  as designed.
             *
             *  The read below still runs: it is idempotent (it publishes only
             *  the rows the cache does not have yet), so consuming twice costs
             *  a re-read and nothing else — while SKIPPING it would lose the
             *  records if the other path had unlinked and then failed.
             */
            if(gobj_global_trace_level() & TRACE_FS) {
                gobj_log_debug(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_YEV_LOOP,
                    "msg",              "%s", "CLIENT: hard link already consumed",
                    "msg2",             "%s", "💾🔶 CLIENT: hard link already consumed",
                    "path",             "%s", path,
                    NULL
                );
            }
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "unlink() FAILED",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
    }

    char *md2 = pop_last_segment(path);
    char *key = pop_last_segment(path);
    char *rt_id = pop_last_segment(path);
    char *disks = pop_last_segment(path);
    char *topic_name = pop_last_segment(path);
    if(strcmp(disks, "disks")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Bad path client 2 /disks/rt_id/key/md2",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            "disks",        "%s", disks,
            "rt_id",        "%s", rt_id,
            "key",          "%s", key,
            "md2",          "%s", md2,
            NULL
        );
        return -1;
    }

    json_t *topic = tranger2_topic(tranger, topic_name);

    /*
     *  The `rt_id` in the path is not decoration: the master hard-links the
     *  md2 into the directory of EVERY feed that wants the key, so this
     *  notification belongs to ONE feed — the one whose directory fired.
     */
    update_new_records_from_disk(
        gobj,
        tranger,
        topic,
        key,
        md2,
        rt_id
    );

    return 0;
}

/***************************************************************************
 *  CLIENT
 *  Call from a client realtime disk

    The last record of files  - {topic}/cache/{key}/files/[{r}] -
    has the last record appended.
        {
            "id": "2000-01-03",
            "fr_t": 946857600,
            "to_t": 946864799,
            "fr_tm": 946857600,
            "to_tm": 946864799,
            "rows": 226651,
        }

    Here, with the information of master in the file, we know:
        - {topic} (topic_name)
        - {key}
        - md2 (filename .md2) without his extension is the id of the [{r}]

    Find the segment, normally will be the last segment:
        - If the id of the last segment matchs with the md2,
            see the rows of the new md2,
            see the difference and load the new records and publish.
        - If the id of the last segment doesn't match with md2,
            do a full reload of the cache segments

            IT'S necessary to load and publish only the new records!
            !!! How are you going to repeats records to the client? You fool? !!!

 *  Return -1 if error and if successful return total rows ( > 0)
 ***************************************************************************/
PRIVATE json_int_t update_new_records_from_disk(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    char *filename,
    const char *rt_id   // the feed whose /disks/<rt_id>/ directory fired
)
{
    const char *topic_directory = json_string_value(json_object_get(topic, "directory"));

    /*
     *  The cell this file already has, found BEFORE the load: a marked
     *  file is then read from the row after the ones the cell counted.
     */
    char file_id_[NAME_MAX];
    if(snprintf(file_id_, sizeof(file_id_), "%s", filename) >= (int)sizeof(file_id_)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "md2 filename too long",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            "filename",     "%s", filename,
            NULL
        );
        return -1;
    }
    char *ext = strrchr(file_id_, '.');
    if(ext) {
        *ext = 0;
    }
    json_int_t file_base = 0;
    int insert_idx = 0;
    json_t *cur_cache_cell = find_cache_cell(
        topic,
        key,
        file_id_,
        &file_base,
        &insert_idx
    );

    json_t *new_cache_cell = load_cache_cell_from_disk( // A bit slow, open/read/close file
        gobj,
        topic_directory,
        key,
        filename,   // warning .md2 removed
        cur_cache_cell
    );
    if(!new_cache_cell) {
        // Error already logged
        return -1;
    }

    // Publish new data to iterator
    // TODO WARNING here publishing records without totals updated!!
    json_int_t rows_added = publish_new_rt_disk_records(
        gobj,
        tranger,
        topic,
        key,
        cur_cache_cell,
        new_cache_cell,
        file_base,
        rt_id
    );

    /*
     *  UPDATE CACHE from disk
     */
    if(!cur_cache_cell) {
        json_t *key_cache = get_key_cache(topic, key);
        json_t *cache_files = json_object_get(key_cache, "files");
        json_array_insert_new(cache_files, (size_t)insert_idx, new_cache_cell);
    } else {
        merge_cache_cell(cur_cache_cell, new_cache_cell);
    }

    json_int_t totals = update_totals_of_key_cache2(
        gobj,
        topic,
        key,
        cur_cache_cell?cur_cache_cell:new_cache_cell,
        rows_added
    );

    return totals;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_int_t publish_new_rt_disk_records( // return # of new records
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *old_cache_cell,
    json_t *new_cache_cell,
    json_int_t file_base, // rows of the files before this one
    const char *rt_id   // the feed whose /disks/<rt_id>/ directory fired
)
{
    BOOL master = json_boolean_value(json_object_get(tranger, "master"));
    json_t *disks = json_object_get(topic, "disks");

    json_int_t from_rowid = json_integer_value(json_object_get(old_cache_cell, "rows"));
    if(from_rowid == 0) {
        from_rowid = 1;
    } else {
        from_rowid++;
    }
    json_int_t to_rowid = json_integer_value(json_object_get(new_cache_cell, "rows"));

    const char *file_id = json_string_value(json_object_get(new_cache_cell, "id"));

    /*
     *  Rowids here are RELATIVE TO THE FILE (read_md seeks (rowid-1) records
     *  into <file_id>.md2), and a topic rotates its file (filename_mask, by
     *  default one a day). The key's GLOBAL rowid — what the callback's
     *  contract promises and what the master's rt_mem path already delivers
     *  (update_new_record_from_mem returns g_rowid) — is that position plus
     *  the rows of every file BEFORE this one: `file_base`, counted by
     *  find_cache_cell() in the order of the files, not "every cell before
     *  the last", which an out-of-order file is not.
     */

    /*
     *  ONE feed owns this notification.
     *
     *  The master hard-links the new md2 into the directory of EVERY feed that
     *  wants the key (master_to_update_client_load_record_callback), so an
     *  append on a key watched by N feeds wakes us N times — once per feed.
     *  Publishing to every feed on each of those wake-ups gave each feed N
     *  copies of every record (N x N publishes): with a per-key Live card and a
     *  whole-topic Live card open on the same key, the rows came out DOUBLED in
     *  BOTH cards. Serve the feed whose directory fired, and nobody else.
     *
     *  Its watermark is its own, too: the shared key cache advances with the
     *  FIRST wake-up, so a feed served by a later one would find "nothing new"
     *  and lose the record. `published` remembers, per key, the last rowid this
     *  feed was given.
     */
    json_t *fired_disk = NULL;
    if(!empty_string(rt_id)) {
        int idx; json_t *disk;
        json_array_foreach(disks, idx, disk) {
            const char *disk_id = json_string_value(json_object_get(disk, "id"));
            if(disk_id && strcmp(disk_id, rt_id)==0) {
                fired_disk = disk;
                break;
            }
        }
        if(!fired_disk) {
            /*  Its feed is gone (closed while its hard link was in flight):
             *  the records are still new to the tranger, so the cache and the
             *  in-process mem lists below must still see them.  */
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "rt disk feed of the fired directory is gone",
                "topic_name",   "%s", tranger2_topic_name(topic),
                "key",          "%s", key,
                "rt_id",        "%s", rt_id,
                NULL
            );
        }
    }

    /*
     *  Seed the watermark of EVERY feed that wants this key and has none yet —
     *  at from_rowid-1, the batch start, while it is still known. The shared
     *  cache advances when THIS (first) wake-up returns, so a feed whose
     *  directory fires later in the same batch cannot recover the start from
     *  the cache: unseeded, it computed "nothing new" and permanently lost the
     *  first records after it opened (they reached the sibling card and never
     *  this one).
     *
     *  A mark counts in ONE FILE: its rowid is relative to that file, and the
     *  topic rotates (a new md2 restarts at rowid 1). So there is one mark
     *  per (feed, key, FILE): published[key] = {file_id: rowid, ...}.
     *  It was one per (feed, key), reseeded whenever another file came, and
     *  a batch that touched two files of a key -- a late record and a
     *  current one, or a plain rotation while the follower was busy --
     *  reseeded a feed's mark on the second file before that feed had read
     *  the first: it lost the records of BOTH (M18 of the 2026-09-21
     *  review). Only a MISSING mark is seeded, and never over another
     *  file's. Presence is what says "seeded" -- a rowid of 0 is legit (a
     *  batch starting at rowid 1: a brand-new key, or a fresh file). The
     *  marks of a key are no more than its cells, and go with the key.
     */
    if(1) {
        int idx; json_t *disk;
        json_array_foreach(disks, idx, disk) {
            if(!list_wants_key(disk, key)) {
                continue;
            }
            json_t *published = json_object_get(disk, "published");
            if(!published) {
                published = json_object();
                json_object_set_new(disk, "published", published);
            }
            json_t *marks = json_object_get(published, key);
            if(!json_is_object(marks)) {
                marks = json_object();
                json_object_set_new(published, key, marks);
            }
            if(!json_object_get(marks, file_id?file_id:"")) {
                json_object_set_new(marks, file_id?file_id:"",
                    json_integer((json_int_t)(from_rowid - 1)));
            }
        }
    }

    json_int_t from_disk_rowid = from_rowid;
    if(fired_disk) {
        json_t *published = json_object_get(fired_disk, "published");
        json_t *marks = published? json_object_get(published, key) : NULL;
        json_t *mark = marks? json_object_get(marks, file_id?file_id:"") : NULL;
        if(mark) {
            from_disk_rowid = json_integer_value(mark) + 1;
            json_object_set_new(marks, file_id?file_id:"", json_integer(to_rowid));
        }
    }

    /*
     *  Read once, serve two audiences: the feed that fired (from ITS watermark)
     *  and, on a follower, the in-process rt_mem lists (from the shared cache,
     *  so they see each record once no matter how many disk feeds are open).
     */
    json_int_t first_rowid = (from_disk_rowid < from_rowid)? from_disk_rowid : from_rowid;

    /*
     *  A feed opened with only_md wants the metadata but not the body: skip the
     *  per-record disk read when no audience of this key needs the content, and
     *  hand NULL to the only_md feeds below. The historical iterator already
     *  honors only_md; the realtime feed must too.
     */
    BOOL fired_only_md = fired_disk? feed_wants_only_md(fired_disk) : FALSE;
    BOOL need_body = (fired_disk && !fired_only_md)? TRUE : FALSE;
    if(!master && !need_body) {
        json_t *scan_lists = json_object_get(topic, "lists");
        int i; json_t *l;
        json_array_foreach(scan_lists, i, l) {
            if(list_wants_key(l, key) && !feed_wants_only_md(l)) {
                need_body = TRUE;
                break;
            }
        }
    }

    for(json_int_t rowid=first_rowid; rowid<=to_rowid; rowid++) {
        md2_record_ex_t md_record_ex;
        if(read_md(
            gobj,
            tranger,
            topic,
            key,
            file_id,
            rowid,
            &md_record_ex
        )<0) {
            // Error already logged; md_record_ex is uninitialized on failure, skip it
            continue;
        }
        if(is_deleted_instance(&md_record_ex)) {
            continue;
        }

        /*
         *  What the consumer is given is the GLOBAL rowid of the key (the
         *  master's rt_mem path hands it g_rowid): a file-relative one
         *  restarts at 1 with every rotation, and a consumer that dedupes or
         *  pages by it (the SPA's Live cards do) sees the new file's records
         *  as records it already had.
         */
        json_int_t g_rowid = file_base + rowid;
        md_record_ex.g_rowid = g_rowid;

        json_t *record = NULL;
        if(need_body) {
            record = read_record_content(
                tranger,
                topic,
                key,
                file_id,
                &md_record_ex
            );
            if(record) {
                json_object_set_new(record, "__md_tranger__", md2json(&md_record_ex));
            }
        }

        /*----------------------------*
         *      FEED the feed that fired
         *----------------------------*/
        int idx;
        if(fired_disk && rowid >= from_disk_rowid && list_wants_key(fired_disk, key)) {
            tranger2_load_record_callback_t load_record_callback =
                (tranger2_load_record_callback_t)(size_t)json_integer_value(
                    json_object_get(fired_disk, "load_record_callback")
                );

            if(load_record_callback) {
                // Inform to the user list: record realtime from disk
                load_record_callback(
                    tranger,
                    topic,
                    key,
                    fired_disk,
                    g_rowid,
                    &md_record_ex,
                    fired_only_md? NULL : json_incref(record)
                );
            }
        }

        /*----------------------------*
         *      FEED the in-process mem lists (a follower reads from disk):
         *      once per record, whatever the number of disk feeds open.
         *----------------------------*/
        if(!master && rowid >= from_rowid) {
            json_t *lists = json_object_get(topic, "lists");
            json_t *list;
            json_array_foreach(lists, idx, list) {
                if(list_wants_key(list, key)) {
                    tranger2_load_record_callback_t load_record_callback =
                        (tranger2_load_record_callback_t)(size_t)json_integer_value(
                            json_object_get(list, "load_record_callback")
                        );

                    if(load_record_callback) {
                        // Inform to the user list: record real time from memory
                        load_record_callback(
                            tranger,
                            topic,
                            key,
                            list,
                            g_rowid,
                            &md_record_ex,
                            feed_wants_only_md(list)? NULL : json_incref(record)
                        );
                    }
                }
            }
        }

        JSON_DECREF(record)
    }

    return to_rowid - from_rowid + 1;
}

/***************************************************************************
 ***************************************************************************/
// PRIVATE json_t *find_keys_in_disk(
//     hgobj gobj,
//     const char *directory,
//     const char *rkey
// )
// {
//     json_t *jn_keys = json_array();
//
//     const char *pattern;
//     if(!empty_string(rkey)) {
//         pattern = rkey;
//     } else {
//         pattern = ".*";
//     }
//
//     int dirs_size;
//     char **dirs = get_ordered_filename_array(
//         gobj,
//         directory,
//         pattern,
//         WD_MATCH_DIRECTORY|WD_ONLY_NAMES,
//         &dirs_size
//     );
//
//     for(int i=0; i<dirs_size; i++) {
//         json_array_append_new(jn_keys, json_string(dirs[i]));
//     }
//     free_ordered_filename_array(dirs, dirs_size);
//
//     return jn_keys;
// }

/***************************************************************************
 *  Returns list of keys that exist on disk
 *  directory: path to search
 *  cb: callback called for each match
 *  Return: number of directories found
 ***************************************************************************/
struct find_keys_s {
    hgobj gobj;
    json_t *topic;
    const char *directory;
    const char *key;
};

typedef int (*find_keys_cb_fn)(struct find_keys_s *find_keys);

/***************************************************************************
 *  Compare two key names, for qsort()
 ***************************************************************************/
PRIVATE int cmp_key_names(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

PRIVATE int find_keys_in_disk(
    hgobj gobj,
    json_t *topic,
    find_keys_cb_fn cb
)
{
    const char *directory = json_string_value(json_object_get(topic, "directory"));
    struct dirent *entry;
    int match_count = 0;
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/keys",
        directory
    );

    DIR *dir = opendir(full_path);
    if(!dir) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "find tranger2 keys: directory not found",
            "path",         "%s", full_path,
           NULL
        );
        return 0;
    }

    struct find_keys_s find_keys = {
        .gobj = gobj,
        .topic = topic,
        .directory = directory,
        .key = 0
    };

    /*
     *  HACK Collect the names first, and read them back SORTED.
     *
     *  readdir() hands the keys back in whatever order the filesystem keeps
     *  them -- hash order on ext4 -- and that order is not an internal
     *  detail: it is the order the keys enter the topic cache, therefore the
     *  order a keyless list loads its records in, therefore the order the
     *  nodes of a treedb sit in memory, and therefore the order of the
     *  columns of a schema rebuilt from its __system__ projection. Sorted,
     *  a store reads back the same way twice, and two replicas of it read
     *  back the same way as each other.
     */
    json_t *jn_keys = json_array();
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.' &&
          (entry->d_name[1] == '\0' ||
           (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        int is_dir = 0;

        /*
         *  No d_type (XFS with ftype=0, NFS, FUSE, overlay): ask the inode.
         *  The entry lives in keys/, so that is the directory to join, not
         *  the topic's: stat'ing <topic>/<key> found nothing, and the topic
         *  opened with an empty cache over files that were all there.
         */
        #ifdef DT_DIR
        if(entry->d_type == DT_DIR) {
            is_dir = 1;
        } else if(entry->d_type == DT_UNKNOWN) {
            struct stat st;
            char path[PATH_MAX];
            build_path(path, sizeof(path), full_path, entry->d_name, NULL);
            if(stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                is_dir = 1;
            }
        }
        #else
        {
            struct stat st;
            char path[PATH_MAX];
            build_path(path, sizeof(path), full_path, entry->d_name, NULL);
            if(stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                is_dir = 1;
            }
        }
        #endif

        if(!is_dir) {
            continue;
        }

        json_array_append_new(jn_keys, json_string(entry->d_name));
    }

    closedir(dir);

    /*
     *  The array owns the names; this one only points at them, to be sorted.
     */
    size_t keys_size = json_array_size(jn_keys);
    char **names = keys_size? gbmem_malloc(keys_size * sizeof(char *)): NULL;
    if(names) {
        for(size_t i = 0; i < keys_size; i++) {
            names[i] = (char *)json_string_value(json_array_get(jn_keys, i));
        }
        qsort(names, keys_size, sizeof(char *), cmp_key_names);
    }

    for(size_t i = 0; i < keys_size; i++) {
        /*
         *  With no room for the index the keys come back in the filesystem's
         *  order: worse than sorted, but still every one of them.
         *  Error already logged by gbmem_malloc().
         */
        find_keys.key = names?
            names[i]:
            json_string_value(json_array_get(jn_keys, i));

        // Call user callback
        if(cb(&find_keys) < 0) {
            break; // user requested stop
        }

        match_count++;
    }

    GBMEM_FREE(names)
    json_decref(jn_keys);

    return match_count;
}

/***************************************************************************
 *  Load metadata of topic in cache:
 *      keys with its range of time available
 ***************************************************************************/
PRIVATE int find_keys_cb(struct find_keys_s *find_keys)
{
    json_t *topic_cache = json_object_get(find_keys->topic, "cache");
    json_t *key_cache = load_key_cache_from_disk(
        find_keys->gobj,
        find_keys->directory,
        find_keys->key
    );
    json_object_set_new(topic_cache, find_keys->key, key_cache);
    update_totals_of_key_cache(find_keys->gobj, find_keys->topic, find_keys->key);
    return 0; // don't break
}

PRIVATE int build_topic_cache_from_disk(
    hgobj gobj,
    json_t *topic
) {
    return find_keys_in_disk(gobj, topic, find_keys_cb);
}

/***************************************************************************
 *  Create the tree ("cache`%s`files", key) if not exist
    "cache": {
        "{key}": {key_cache},
        "{key2}": {key_cache2}
        ...
     }

    {key_cache} = {
        "files": [
            {
                "id": "tracks-2024-11-14",
                "fr_t": 1731601280,
                "to_t": 1731606678,
                "fr_tm": 1731601189,
                "to_tm": 1731606678,
                "rows": 184,
            },
            ...
        ],
        "total": {
            "fr_t": 1731601280,
            "to_t": 1731698630,
            "fr_tm": 1731601189,
            "to_tm": 1731698630,
            "rows": 1791
        }
    }
 ***************************************************************************/
PRIVATE json_t *create_cache_key(void)
{
    json_t *key_cache = json_object();
    json_object_set_new(key_cache, "files", json_array());
    json_object_set_new(key_cache, "total", json_object());

    return key_cache;
}

/***************************************************************************
 *  Return key_cache, create ("cache`%s`files", key) if not exist
 ***************************************************************************/
PRIVATE json_t *get_key_cache(
    json_t *topic,
    const char *key
)
{
    // "cache`%s`files", key
    json_t *topic_cache = json_object_get(topic, "cache");
    json_t *key_cache = json_object_get(topic_cache, key);

    if(!key_cache) {
        // NEW KEY in CACHE
        key_cache = create_cache_key();
        json_object_set_new(topic_cache, key, key_cache);
    }
    return key_cache;
}

/***************************************************************************
 *  Order of two file ids, as the load orders the md2 files: by their NAME
 *  (dir_array_sort), suffix included.
 ***************************************************************************/
PRIVATE int cmp_file_ids(const char *a, const char *b)
{
    char a_[NAME_MAX];
    char b_[NAME_MAX];
    snprintf(a_, sizeof(a_), "%s.md2", a);
    snprintf(b_, sizeof(b_), "%s.md2", b);
    return strcmp(a_, b_);
}

/***************************************************************************
 *  Return the cache cell of `file_id` in the key's cells, or NULL if the
 *  file has none yet. Create the tree ("cache`%s`files", key) if not exist.
 *
 *  The cells are in the order the load gives them (one per md2 file, by
 *  name), and a global rowid is a position in that order. A record whose
 *  __t__ belongs to an EARLIER file goes to that file's cell: looking only
 *  at the LAST cell, it got a second cell of the same file at the end, and
 *  the segments served the FIRST record of that file in its place.
 *
 *  *pfile_base: the rows of every file before this one.
 *  *pinsert_idx: where a new cell of this file must go.
 ***************************************************************************/
PRIVATE json_t *find_cache_cell(
    json_t *topic,
    const char *key,
    const char *file_id,
    json_int_t *pfile_base,
    int *pinsert_idx
)
{
    json_t *key_cache = get_key_cache(topic, key);
    json_t *cache_files = json_object_get(key_cache, "files");

    /*
     *  The LAST cell first: it is where almost every record goes (the
     *  current file) or after which it goes (a new one). Its base is the
     *  key's total minus its own rows, so neither case walks the cells.
     *  The walk below, from the first cell, cost O(files of the key) on
     *  every append -- 3.7 us at one file, 318 us at 3650 (M17 of the
     *  2026-09-21 review).
     */
    size_t n_cells = json_array_size(cache_files);
    if(n_cells > 0) {
        json_t *last_cell = json_array_get(cache_files, n_cells - 1);
        const char *last_id = json_string_value(json_object_get(last_cell, "id"));
        int cmp = cmp_file_ids(last_id?last_id:"", file_id);
        if(cmp <= 0) {
            json_int_t total_rows = json_integer_value(
                json_object_get(json_object_get(key_cache, "total"), "rows")
            );
            if(cmp == 0) {
                *pfile_base = total_rows - json_integer_value(json_object_get(last_cell, "rows"));
                *pinsert_idx = (int)(n_cells - 1);
                return last_cell;
            }
            *pfile_base = total_rows;
            *pinsert_idx = (int)n_cells;
            return NULL;
        }
    }

    json_int_t file_base = 0;
    int insert_idx = (int)json_array_size(cache_files);
    json_t *found = NULL;

    int idx; json_t *cache_cell;
    json_array_foreach(cache_files, idx, cache_cell) {
        const char *file_id_ = json_string_value(json_object_get(cache_cell, "id"));
        int cmp = cmp_file_ids(file_id_?file_id_:"", file_id);
        if(cmp == 0) {
            found = cache_cell;
            insert_idx = idx;
            break;
        }
        if(cmp > 0) {
            insert_idx = idx;
            break;
        }
        file_base += json_integer_value(json_object_get(cache_cell, "rows"));
    }

    *pfile_base = file_base;
    *pinsert_idx = insert_idx;
    return found;
}

/***************************************************************************
 *  A md2 file of the key that the cache build cannot count: the key is
 *  flagged, `"unreadable": [file_id, ...]` in its cache, sorted like the
 *  cells. Its rows are in no cell, and every load of the key says
 *  `load_failed` (tranger2_open_iterator) and stops where the file is.
 ***************************************************************************/
PRIVATE void flag_key_unreadable(
    hgobj gobj,
    json_t *key_cache,
    const char *topic_directory,
    const char *key,
    const char *file_id,
    const char *cause
)
{
    gobj_log_error(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_TRANGER,
        "msg",              "%s", "md2 file of the key unreadable when its cache was built: every load of the key says load_failed",
        "cause",            "%s", cause,
        "topic_directory",  "%s", topic_directory,
        "key",              "%s", key,
        "file_id",          "%s", file_id,
        NULL
    );
    json_t *unreadable = json_object_get(key_cache, "unreadable");
    if(!unreadable) {
        unreadable = json_array();
        json_object_set_new(key_cache, "unreadable", unreadable);
    }
    json_array_append_new(unreadable, json_string(file_id));
}

/***************************************************************************
 *  Get range time of a key
 *
 *  A md2 file that cannot be counted is not a file of 0 rows: its rows
 *  exist, and a key read without them is a SHORTER key that nothing
 *  flagged -- a treedb came up after a restart with the node absent, and
 *  a create of its id wrote over the records nobody read (independent
 *  review of the third fix round). The key is flagged instead
 *  (flag_key_unreadable), for:
 *      - a md2 that cannot be opened or read, or whose size is not a
 *        whole number of rows;
 *      - a md2 of 0 rows whose content file is NOT empty: the rows of that
 *        content are gone. (It is also what a first append whose md2
 *        write failed leaves; the flag is on the safe side there.)
 *  A md2 of 0 rows with an empty content file loses nothing: it gets no
 *  cell (a cell of 0 rows has the range of a zeroed row, 1970).
 ***************************************************************************/
PRIVATE json_t *load_key_cache_from_disk(
    hgobj gobj,
    const char *topic_directory,
    const char *key
) {
    char full_path[PATH_MAX];
    build_path(full_path, sizeof(full_path), topic_directory, "keys", key, NULL);

    // NEW KEY in CACHE
    json_t *key_cache = create_cache_key();
    json_t *cache_files = json_object_get(key_cache, "files");

    dir_array_t da;

    find_files_with_suffix_array(
        gobj,
        full_path,
        ".md2",
        &da
    );

    dir_array_sort(&da);

    for(int i=0; i<da.count; i++) {
        char *filename = da.items[i];
        char file_id[NAME_MAX];
        snprintf(file_id, sizeof(file_id), "%s", filename);
        char *dot = strrchr(file_id, '.');
        if(dot) {
            *dot = 0;
        }

        json_t *cache_cell = load_cache_cell_from_disk(
            gobj,
            topic_directory,
            key,
            filename,   // warning .md2 removed
            NULL        // no cell yet: the cache is being built
        );
        if(!cache_cell) {
            // Error already logged, the cause
            flag_key_unreadable(gobj, key_cache, topic_directory, key, file_id,
                "the md2 file cannot be read (see the log before)"
            );
            continue;
        }
        if(json_integer_value(json_object_get(cache_cell, "rows")) == 0) {
            JSON_DECREF(cache_cell)
            char content_name[NAME_MAX + 8];
            char content_path[PATH_MAX];
            snprintf(content_name, sizeof(content_name), "%s.json", file_id);
            if(!build_path(content_path, sizeof(content_path), full_path, content_name, NULL)) {
                // Error already logged: a name no file can have, no content lost
                continue;
            }
            if(filesize(content_path) > 0) {
                flag_key_unreadable(gobj, key_cache, topic_directory, key, file_id,
                    "the md2 file has no rows and its content file is not empty"
                );
            }
            continue;
        }
        json_array_append_new(cache_files, cache_cell);
    }

    dir_array_free(&da);

    return key_cache;
}

/***************************************************************************
 *  Write an integer field of a cache cell or of a key's totals.
 *  The cache is rewritten on every append: reuse the integer already there
 *  instead of allocating a new one and freeing the old (10 of each per
 *  append). Safe because nothing holds a reference to these integers:
 *  every reader copies the value, or deep-copies the cell (get_segments,
 *  tranger2_topic_key_range).
 ***************************************************************************/
PRIVATE void set_cache_int(json_t *dict, const char *key, json_int_t value)
{
    json_t *jn_value = json_object_get(dict, key);
    if(json_is_integer(jn_value)) {
        json_integer_set(jn_value, value);
    } else {
        json_object_set_new(dict, key, json_integer(value));
    }
}

/***************************************************************************
 *  Update a cache cell with a new record metadata
 *  HACK tranger is only append. No update, no insert.
 *  The record can be deleted (it's unrecoverable).
 *  Only the master can write or delete, non-master only can read.
 *  The metadata `md2_record_t` can be updated only in two cases:
 *    - any bit in `user_flag`
 ***************************************************************************/
PRIVATE json_t *update_cache_cell(
    json_t *file_cache,
    const char *file_id,
    md2_record_t *md_record,
    int operation,  // -1 to subtract (NOT USED), 0 to set, +1 to add
    uint64_t rows_
)
{
    uint64_t file_from_t = (uint64_t)(-1);
    uint64_t file_to_t = 0;
    uint64_t file_from_tm = (uint64_t)(-1);
    uint64_t file_to_tm = 0;
    uint64_t rows = 0;

    if(!file_cache) {
        file_cache = json_object();
        json_object_set_new(file_cache, "id", json_string(file_id));
    } else {
        file_from_t = json_integer_value(json_object_get(file_cache, "fr_t"));
        file_to_t = json_integer_value(json_object_get(file_cache, "to_t"));
        file_from_tm = json_integer_value(json_object_get(file_cache, "fr_tm"));
        file_to_tm = json_integer_value(json_object_get(file_cache, "to_tm"));
        rows = json_integer_value(json_object_get(file_cache, "rows"));
    }

    if(operation > 0) {
        rows += rows_;
    } else if(operation == 0) {
        rows = rows_;

    } else { // < 0
        // NOT USED
        rows -= rows_;
    }

    /*
     *  WARNING the times MUST be read through the accessors: in a
     *  md2_record_t the 16 high bits of __t__ carry the user_flag and those
     *  of __tm__ the system_flag (see USER_FLAG_MASK). The disk path arrives
     *  here already masked (load_first_and_last_record_md), the append path
     *  does NOT — taking the raw field stored a time with the flags baked in
     *  (a record with any user_flag or system_flag bit set poisoned the
     *  cache range, and every range check against it — get_segments' file
     *  selection included — went wrong).
     */
    uint64_t record_t = get_time_t(md_record);
    uint64_t record_tm = get_time_tm(md_record);

    if(record_t < file_from_t) {
        file_from_t = record_t;
    }
    if(record_t > file_to_t) {
        file_to_t = record_t;
    }

    if(record_tm < file_from_tm) {
        file_from_tm = record_tm;
    }
    if(record_tm > file_to_tm) {
        file_to_tm = record_tm;
    }

    set_cache_int(file_cache, "fr_t", (json_int_t)file_from_t);
    set_cache_int(file_cache, "to_t", (json_int_t)file_to_t);
    set_cache_int(file_cache, "fr_tm", (json_int_t)file_from_tm);
    set_cache_int(file_cache, "to_tm", (json_int_t)file_to_tm);
    set_cache_int(file_cache, "rows", (json_int_t)rows);

    return file_cache;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *load_cache_cell_from_disk(
    hgobj gobj,
    const char *topic_directory,
    const char *key,
    char *filename, // md2 filename with extension, WARNING modified, .md2 removed
    json_t *known_cell  // the cell this file already has in memory, or NULL
)
{
    /*----------------------------------*
     *  Get the first and last record
     *----------------------------------*/
    md2_record_t md_first_record = {0};
    md2_record_t md_last_record = {0};

    json_int_t file_rows = load_first_and_last_record_md(
        gobj,
        topic_directory,
        key,
        filename,
        &md_first_record,
        &md_last_record
    );
    if(file_rows < 0) {
        // Error already logged
        return NULL;
    }

    /*---------------------------*
     *      Create the cell
     *---------------------------*/
    char *p = strrchr(filename, '.');
    if(p) {
        *p = 0;
        // Now filename is file_id
    }
    json_t *file_cache = update_cache_cell(0, filename, &md_first_record, 0, 1);
    update_cache_cell(file_cache, filename, &md_last_record, 0, file_rows);

    /*
     *  The first and the last row give the file's range only while its rows
     *  are in time order. A late record (a __t__ below the file's to_t) is
     *  the LAST row with a lower time, and the range read from it hid the
     *  file from time-range queries (M16 of the 2026-09-21 review); a __tm__
     *  that goes back hid it from tm queries (M2 of the 2026-09-23
     *  independent review). The master marks such a file
     *  (mark_file_unordered); a marked one is read whole -- 32 bytes a row,
     *  sequentially -- and only that one.
     */
    char marker[NAME_MAX];
    char tm_marker[NAME_MAX];
    BOOL t_marked;
    BOOL tm_marked;
    if(snprintf(marker, sizeof(marker), "%s.unordered", filename) >= (int)sizeof(marker) ||
            snprintf(tm_marker, sizeof(tm_marker), "%s.tm_unordered", filename) >= (int)sizeof(tm_marker)) {
        /*
         *  A marker of this file cannot exist (the master could not write
         *  it either: "file_id too long"), and a truncated name was looked
         *  for: the file is taken as marked, read whole, never trusted.
         */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Cannot look for the markers of a md2 file, its name is too long: read whole",
            "topic_directory", "%s", topic_directory,
            "key",          "%s", key,
            "file_id",      "%s", filename,
            NULL
        );
        t_marked = TRUE;
        tm_marked = TRUE;
    } else {
        char key_directory[PATH_MAX];
        build_path(key_directory, sizeof(key_directory), topic_directory, "keys", key, NULL);
        t_marked = file_exists(key_directory, marker);
        tm_marked = file_exists(key_directory, tm_marker);
    }
    if(t_marked || tm_marked) {
        /*
         *  The rows already read need no second reading: a follower wakes
         *  up on every append of the master, and read the marked file WHOLE
         *  each time (N12 of the 2026-09-22 review). A cell flagged in memory
         *  holds the range of every row it counted (it was read whole); only
         *  the rows after them are read, and the two ranges are joined.
         */
        json_int_t from_row = 1;
        if(known_cell && (json_is_true(json_object_get(known_cell, "unordered")) ||
                json_is_true(json_object_get(known_cell, "tm_unordered")))) {
            join_cell_ranges(file_cache, known_cell);
            from_row = json_integer_value(json_object_get(known_cell, "rows")) + 1;
        }
        if(widen_cell_from_rows(gobj, topic_directory, key, filename, file_cache, from_row) < 0) {
            // Error already logged: the cell keeps the first/last range
        }
        if(t_marked) {
            json_object_set_new(file_cache, "unordered", json_true());
        }
        if(tm_marked) {
            json_object_set_new(file_cache, "tm_unordered", json_true());
        }
    }

    return file_cache;
}

/***************************************************************************
 *  Widen `cell`'s ranges (t and tm) to hold `other`'s too.
 ***************************************************************************/
PRIVATE void join_cell_ranges(json_t *cell, json_t *other)
{
    const char *lows[] = {"fr_t", "fr_tm", NULL};
    const char *highs[] = {"to_t", "to_tm", NULL};
    for(int i = 0; lows[i]; i++) {
        json_int_t a = json_integer_value(json_object_get(cell, lows[i]));
        json_int_t b = json_integer_value(json_object_get(other, lows[i]));
        if(b < a) {
            set_cache_int(cell, lows[i], b);
        }
    }
    for(int i = 0; highs[i]; i++) {
        json_int_t a = json_integer_value(json_object_get(cell, highs[i]));
        json_int_t b = json_integer_value(json_object_get(other, highs[i]));
        if(b > a) {
            set_cache_int(cell, highs[i], b);
        }
    }
}

/***************************************************************************
 *  Read the rows of a md2 file from `from_row` (1-based) on and widen the
 *  cell's ranges (t and tm) to what they really hold. Only for a file the
 *  master marked unordered; from row 1 it is the whole file.
 ***************************************************************************/
PRIVATE int widen_cell_from_rows(
    hgobj gobj,
    const char *topic_directory,
    const char *key,
    const char *file_id,
    json_t *cache_cell,
    json_int_t from_row
)
{
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.md2", file_id);
    char full_path[PATH_MAX];
    build_path(full_path, sizeof(full_path), topic_directory, "keys", key, filename, NULL);

    int fd = open(full_path, O_RDONLY|O_CLOEXEC, 0);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open md2 file to widen its range",
            "path",         "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    if(from_row > 1) {
        off_t offset = (off_t)(from_row - 1) * (off_t)sizeof(md2_record_t);
        if(lseek(fd, offset, SEEK_SET) != offset) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot seek md2 file to widen its range",
                "path",         "%s", full_path,
                "from_row",     "%ld", (long)from_row,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            close(fd);
            return -1;
        }
    }

    uint64_t fr_t = (uint64_t)json_integer_value(json_object_get(cache_cell, "fr_t"));
    uint64_t to_t = (uint64_t)json_integer_value(json_object_get(cache_cell, "to_t"));
    uint64_t fr_tm = (uint64_t)json_integer_value(json_object_get(cache_cell, "fr_tm"));
    uint64_t to_tm = (uint64_t)json_integer_value(json_object_get(cache_cell, "to_tm"));

    md2_record_t rows[1024];
    ssize_t ln;
    while((ln = read(fd, rows, sizeof(rows))) > 0) {
        size_t n = (size_t)ln / sizeof(md2_record_t);
        for(size_t i = 0; i < n; i++) {
            uint64_t t = (ntohll(rows[i].__t__)) & TIME_FLAG_MASK;
            uint64_t tm = (ntohll(rows[i].__tm__)) & TIME_FLAG_MASK;
            if(t < fr_t) {
                fr_t = t;
            }
            if(t > to_t) {
                to_t = t;
            }
            if(tm < fr_tm) {
                fr_tm = tm;
            }
            if(tm > to_tm) {
                to_tm = tm;
            }
        }
    }
    int ret = 0;
    if(ln < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read md2 file to widen its range",
            "path",         "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        ret = -1;
    }
    close(fd);

    set_cache_int(cache_cell, "fr_t", (json_int_t)fr_t);
    set_cache_int(cache_cell, "to_t", (json_int_t)to_t);
    set_cache_int(cache_cell, "fr_tm", (json_int_t)fr_tm);
    set_cache_int(cache_cell, "to_tm", (json_int_t)to_tm);
    return ret;
}

/***************************************************************************
 *  A record arrived that the first and the last row of its file do not
 *  bound any more. Leave a marker beside the md2 so a load reads the file
 *  whole (see load_cache_cell_from_disk), and flag the cell. `mark`:
 *      "unordered"     a __t__ below the file's to_t (a late record)
 *      "tm_unordered"  a __tm__ below the file's to_tm (only in a topic that
 *                      marks tm, see topic_marks_tm)
 *  The marker is `<file>.<mark>`. Once per file: the cell remembers.
 *
 *  The cell is flagged WHATEVER the disk says: this process then reads the
 *  file whole. A marker that cannot be written is logged, and the cell says
 *  so (`<mark>_not_on_disk`): every later append to the file tries it again
 *  (mark_file_before_append), because until it is there a reload -- or a
 *  replica -- misreads the file's range. It used to return with the cell
 *  unflagged, and the master itself took the file for one in order: the
 *  early end of a tm scan hid rows 7.25.4 served (M-A of the independent
 *  review of the second fix round).
 *
 *  No fsync: an append is not fsync'ed either. Written before the md2 row
 *  (mark_file_before_append), the marker is never behind the row it
 *  describes for a process that dies; after a power cut the order holds on
 *  a journaled filesystem that commits its metadata in order (ext4), and
 *  a lost marker is what tranger2_mark_tm_order() writes again.
 ***************************************************************************/
PRIVATE void mark_file_unordered(
    hgobj gobj,
    json_t *topic,
    const char *key,
    const char *file_id,
    json_t *cache_cell,
    const char *mark
)
{
    char not_on_disk[NAME_MAX];
    snprintf(not_on_disk, sizeof(not_on_disk), "%s_not_on_disk", mark);
    BOOL retry = json_is_true(json_object_get(cache_cell, not_on_disk));
    if(json_is_true(json_object_get(cache_cell, mark)) && !retry) {
        return;
    }
    json_object_set_new(cache_cell, mark, json_true());

    char marker[NAME_MAX];
    if(snprintf(marker, sizeof(marker), "%s.%s", file_id, mark) >= (int)sizeof(marker)) {
        if(!retry) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Cannot mark md2 file, file_id too long",
                "topic",        "%s", tranger2_topic_name(topic),
                "key",          "%s", key,
                "file_id",      "%s", file_id,
                "mark",         "%s", mark,
                NULL
            );
        }
        json_object_set_new(cache_cell, not_on_disk, json_true());
        return;
    }
    char path[PATH_MAX];
    build_path(path, sizeof(path),
        json_string_value(json_object_get(topic, "directory")), "keys", key, marker, NULL
    );
    int fd = newfile(path, (int)json_integer_value(json_object_get(topic, "rpermission")), FALSE);
    if(fd < 0 && !is_regular_file(path)) {
        if(!retry) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot mark md2 file, a reload will misread its time range",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
        // else: Error already logged when the marker was first missed
        json_object_set_new(cache_cell, not_on_disk, json_true());
        return;
    }
    if(fd >= 0) {
        close(fd);
    }
    if(retry) {
        json_object_del(cache_cell, not_on_disk);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "md2 file marked, the marker missed earlier is written",
            "path",         "%s", path,
            NULL
        );
    }
}

/***************************************************************************
 *  Mark the record's file BEFORE its md2 row is written, when the record
 *  is one the first and the last row of the file do not bound (see
 *  mark_file_unordered). A process that dies between the two leaves a
 *  marker with no row behind it -- which costs a whole read of the file
 *  at the next load -- and never a row with no marker, which hid rows.
 *  A marker still missing is tried again here, on any append to the file.
 ***************************************************************************/
PRIVATE void mark_file_before_append(
    hgobj gobj,
    json_t *topic,
    const char *key,
    const char *file_id,
    md2_record_t *md_record
)
{
    json_int_t file_base = 0;
    int insert_idx = 0;
    json_t *cell = find_cache_cell(topic, key, file_id, &file_base, &insert_idx);
    if(!cell) {
        return;     // The first row of the file: nothing to be out of order with
    }
    if(get_time_t(md_record) < (uint64_t)json_integer_value(json_object_get(cell, "to_t")) ||
            json_is_true(json_object_get(cell, "unordered_not_on_disk"))) {
        mark_file_unordered(gobj, topic, key, file_id, cell, "unordered");
    }
    if(topic_marks_tm(topic) && (
            get_time_tm(md_record) < (uint64_t)json_integer_value(json_object_get(cell, "to_tm")) ||
            json_is_true(json_object_get(cell, "tm_unordered_not_on_disk")))) {
        mark_file_unordered(gobj, topic, key, file_id, cell, "tm_unordered");
    }
}

/***************************************************************************
 *  A cell read again from disk (a follower's update): its RANGE is the
 *  union of what was known and what was read, never what was read alone.
 *  The read gives the first/last rows, and after a late record the last
 *  row is not the file's maximum.
 ***************************************************************************/
PRIVATE void merge_cache_cell(json_t *cur_cache_cell, json_t *new_cache_cell)
{
    const char *mins[] = {"fr_t", "fr_tm", NULL};
    const char *maxs[] = {"to_t", "to_tm", NULL};
    for(int i = 0; mins[i]; i++) {
        json_int_t a = json_integer_value(json_object_get(cur_cache_cell, mins[i]));
        json_int_t b = json_integer_value(json_object_get(new_cache_cell, mins[i]));
        if(a < b) {
            json_object_set_new(new_cache_cell, mins[i], json_integer(a));
        }
    }
    for(int i = 0; maxs[i]; i++) {
        json_int_t a = json_integer_value(json_object_get(cur_cache_cell, maxs[i]));
        json_int_t b = json_integer_value(json_object_get(new_cache_cell, maxs[i]));
        if(a > b) {
            json_object_set_new(new_cache_cell, maxs[i], json_integer(a));
        }
    }
    json_object_update_new(cur_cache_cell, new_cache_cell);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_int_t load_first_and_last_record_md(
    hgobj gobj,
    const char *topic_directory,
    const char *key,
    const char *filename,
    md2_record_t *md_first_record,
    md2_record_t *md_last_record
)
{
    json_int_t file_rows = 0;

    /*----------------------------------*
     *  Open the .md2 file of the key
     *  (name relative to time __t__)
     *----------------------------------*/
    char full_path[PATH_MAX];
    build_path(full_path, sizeof(full_path), topic_directory, "keys", key, filename, NULL);
    int fd = open(full_path, O_RDONLY|O_CLOEXEC, 0);
    if(fd<0) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open md2 file",
            "path",         "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    /*---------------------------*
     *      Read first record
     *---------------------------*/
    ssize_t ln = read( // read first md
        fd,
        md_first_record,
        sizeof(md2_record_t)
    );
    if(ln == sizeof(md2_record_t)) {
        md_first_record->__t__ = (ntohll(md_first_record->__t__)) & TIME_FLAG_MASK;
        md_first_record->__tm__ = (ntohll(md_first_record->__tm__)) & TIME_FLAG_MASK;
        md_first_record->__offset__ = ntohll(md_first_record->__offset__);
        md_first_record->__size__ = ntohll(md_first_record->__size__);
        file_rows = 1;
    } else {
        if(ln<0) {
            gobj_log_critical(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot read first record of md2 file",
                "path",         "%s", full_path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        } else if(ln==0) {
            // No data
        }
    }

    /*---------------------------*
     *      Read last record
     *---------------------------*/
    /*
     *  Seek the last record
     */
    off_t offset = lseek(fd, 0, SEEK_END);
    if(offset < 0 || (offset % sizeof(md2_record_t)!=0)) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Cannot read last record, md2 file corrupted",
            "path",         "%s", full_path,
            "offset",       "%ld", (long)offset,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        close(fd);
        return -1;
    }

    if(offset >= sizeof(md2_record_t)) {
        /*
         *  Save file rows
         */
        file_rows = offset/sizeof(md2_record_t);

        /*
         *  Read last record (firstly to back the size of md2_record_t)
         */
        offset -= sizeof(md2_record_t);
        off_t offset2 = lseek(fd, offset, SEEK_SET);
        if(offset2 < 0 || offset2 != offset) {
            gobj_log_critical(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot read last record, lseek() FAILED",
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }

        ln = read( // read last md
            fd,
            md_last_record,
            sizeof(md2_record_t)
        );
        if(ln == sizeof(md2_record_t)) {
            md_last_record->__t__ = (ntohll(md_last_record->__t__)) & TIME_FLAG_MASK;
            md_last_record->__tm__ = (ntohll(md_last_record->__tm__)) & TIME_FLAG_MASK;
            md_last_record->__offset__ = ntohll(md_last_record->__offset__);
            md_last_record->__size__ = ntohll(md_last_record->__size__);
        } else {
            gobj_log_critical(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot read last record of md2 file",
                "path",         "%s", full_path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
    }

    close(fd);

    return file_rows;
}

/***************************************************************************
 *  Update or create the files cache of a key,
 *  call from tranger2_append_record()
 *
 *  Return -1 if error and if successful return total rows ( > 0)
 ***************************************************************************/
PRIVATE json_int_t update_new_record_from_mem(
    hgobj gobj,
    json_t *topic,
    const char *key,
    const char *file_id,    // the file the record was written to
    md2_record_t *md_record
)
{
    json_t *topic_cache = kw_get_dict(gobj, topic, "cache", 0, 0);
    if(!topic_cache) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "no cache files",
            "topic",        "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            NULL
        );
        return -1;
    }

    /*
     *  The cell of the record's file, wherever it is: a __t__ of an earlier
     *  file writes into that file (the caller opened it by this file_id).
     *  Create the key cache if not exist.
     */
    json_int_t file_base = 0;
    int insert_idx = 0;
    json_t *cur_cache_cell = find_cache_cell(
        topic,
        key,
        file_id,
        &file_base,
        &insert_idx
    );

    /*
     *  UPDATE CACHE from mem
     */
    if(!cur_cache_cell) {
        json_t *key_cache = get_key_cache(topic, key);
        json_t *cache_files = json_object_get(key_cache, "files");
        cur_cache_cell = update_cache_cell(0, file_id, md_record, 1, 1);
        json_array_insert_new(cache_files, (size_t)insert_idx, cur_cache_cell);

    } else {
        // The file was marked before its row was written (mark_file_before_append)
        update_cache_cell(cur_cache_cell, file_id, md_record, 1, 1);
    }

    if(update_totals_of_key_cache2(gobj, topic, key, cur_cache_cell, 1)<0) {
        // Error already logged
        return -1;
    }

    /*
     *  The record's GLOBAL rowid is its place in the order of the files, as
     *  a reload numbers it: the last row of its file plus the rows of the
     *  files before. For an append to the last file that is the total, as
     *  it always was. For an earlier file, every record of the later files
     *  moves one place up -- which a reload did anyway, unannounced.
     */
    return file_base + json_integer_value(json_object_get(cur_cache_cell, "rows"));
}

/***************************************************************************
 *  Update totals of a key, WARNING to use ONLY in initial load
 *  Return -1 if error and if successful return total rows ( > 0)
 ***************************************************************************/
PRIVATE json_int_t update_totals_of_key_cache(
    hgobj gobj,
    json_t *topic,
    const char *key
) {
    // "cache`%s`files", key
    json_t *cache_files = json_object_get(
        json_object_get(
            json_object_get(
                topic,
                "cache"
            ),
            key
        ),
        "files"
    );
    if(!cache_files) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "no cache files",
            "topic",        "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            NULL
        );
        return -1;
    }

    json_int_t total_rows = 0;
    uint64_t global_from_t = (uint64_t)(-1);
    uint64_t global_to_t = 0;
    uint64_t global_from_tm = (uint64_t)(-1);
    uint64_t global_to_tm = 0;

    int idx; json_t *cache_file;
    json_array_foreach(cache_files, idx, cache_file) {
        json_int_t fr_t = json_integer_value(json_object_get(cache_file, "fr_t"));
        json_int_t to_t = json_integer_value(json_object_get(cache_file, "to_t"));
        json_int_t fr_tm = json_integer_value(json_object_get(cache_file, "fr_tm"));
        json_int_t to_tm = json_integer_value(json_object_get(cache_file, "to_tm"));
        json_int_t rows = json_integer_value(json_object_get(cache_file, "rows"));

        if(fr_t < global_from_t) {
            global_from_t = fr_t;
        }
        if(fr_t > global_to_t) {
            global_to_t = fr_t;
        }
        if(fr_tm < global_from_tm) {
            global_from_tm = fr_tm;
        }
        if(fr_tm > global_to_tm) {
            global_to_tm = fr_tm;
        }
        if(to_t < global_from_t) {
            global_from_t = to_t;
        }
        if(to_t > global_to_t) {
            global_to_t = to_t;
        }
        if(to_tm < global_from_tm) {
            global_from_tm = to_tm;
        }
        if(to_tm > global_to_tm) {
            global_to_tm = to_tm;
        }

        total_rows += rows;
    }

    json_t *total_range = json_object_get(
        json_object_get(
            json_object_get(
                topic,
                "cache"
            ),
            key
        ),
        "total"
    );

    set_cache_int(total_range, "fr_t", (json_int_t)global_from_t);
    set_cache_int(total_range, "to_t", (json_int_t)global_to_t);
    set_cache_int(total_range, "fr_tm", (json_int_t)global_from_tm);
    set_cache_int(total_range, "to_tm", (json_int_t)global_to_tm);
    set_cache_int(total_range, "rows", (json_int_t)total_rows);

    return total_rows;
}

/***************************************************************************
 *  Update totals of a key
 *
 *  Return -1 if error and if successful return total rows ( > 0)
 ***************************************************************************/
PRIVATE json_int_t update_totals_of_key_cache2(
    hgobj gobj,
    json_t *topic,
    const char *key,
    json_t *cache_file,
    json_int_t rows_added
) {
    json_t *total_range = json_object_get(
        json_object_get(
            json_object_get(
                topic,
                "cache"
            ),
            key
        ),
        "total"
    );

    json_int_t total_rows = json_integer_value(json_object_get(total_range, "rows"));
    uint64_t global_from_t = json_integer_value(json_object_get(total_range, "fr_t"));
    uint64_t global_to_t = json_integer_value(json_object_get(total_range, "to_t"));
    uint64_t global_from_tm = json_integer_value(json_object_get(total_range, "fr_tm"));
    uint64_t global_to_tm = json_integer_value(json_object_get(total_range, "to_tm"));

    if(global_from_t == 0) {
        global_from_t = (uint64_t)(-1);
    }
    if(global_from_tm == 0) {
        global_from_tm = (uint64_t)(-1);
    }

    json_int_t fr_t = json_integer_value(json_object_get(cache_file, "fr_t"));
    json_int_t to_t = json_integer_value(json_object_get(cache_file, "to_t"));
    json_int_t fr_tm = json_integer_value(json_object_get(cache_file, "fr_tm"));
    json_int_t to_tm = json_integer_value(json_object_get(cache_file, "to_tm"));

    if(fr_t < global_from_t) {
        global_from_t = fr_t;
    }
    if(fr_t > global_to_t) {
        global_to_t = fr_t;
    }
    if(fr_tm < global_from_tm) {
        global_from_tm = fr_tm;
    }
    if(fr_tm > global_to_tm) {
        global_to_tm = fr_tm;
    }
    if(to_t < global_from_t) {
        global_from_t = to_t;
    }
    if(to_t > global_to_t) {
        global_to_t = to_t;
    }
    if(to_tm < global_from_tm) {
        global_from_tm = to_tm;
    }
    if(to_tm > global_to_tm) {
        global_to_tm = to_tm;
    }

    total_rows += rows_added;

    set_cache_int(total_range, "fr_t", (json_int_t)global_from_t);
    set_cache_int(total_range, "to_t", (json_int_t)global_to_t);
    set_cache_int(total_range, "fr_tm", (json_int_t)global_from_tm);
    set_cache_int(total_range, "to_tm", (json_int_t)global_to_tm);
    set_cache_int(total_range, "rows", (json_int_t)total_rows);

    return total_rows;
}

/***************************************************************************
 *  Read one md2 file whole: its t and tm ranges, and whether its t or its
 *  tm goes back somewhere. Return the rows read, -1 on error (logged).
 ***************************************************************************/
PRIVATE json_int_t scan_md2_order(
    hgobj gobj,
    const char *full_path,
    uint64_t *fr_t, uint64_t *to_t,
    uint64_t *fr_tm, uint64_t *to_tm,
    BOOL *t_back,
    BOOL *tm_back
)
{
    *fr_t = (uint64_t)-1;
    *to_t = 0;
    *fr_tm = (uint64_t)-1;
    *to_tm = 0;
    *t_back = FALSE;
    *tm_back = FALSE;

    int fd = open(full_path, O_RDONLY|O_NOFOLLOW|O_CLOEXEC, 0);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open md2 file to read its order",
            "path",         "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    json_int_t n_rows = 0;
    md2_record_t rows[1024];
    ssize_t ln;
    while((ln = read(fd, rows, sizeof(rows))) > 0) {
        size_t n = (size_t)ln / sizeof(md2_record_t);
        for(size_t i = 0; i < n; i++) {
            uint64_t t = (ntohll(rows[i].__t__)) & TIME_FLAG_MASK;
            uint64_t tm = (ntohll(rows[i].__tm__)) & TIME_FLAG_MASK;
            if(n_rows > 0 && t < *to_t) {
                *t_back = TRUE;
            }
            if(n_rows > 0 && tm < *to_tm) {
                *tm_back = TRUE;
            }
            if(t < *fr_t) {
                *fr_t = t;
            }
            if(t > *to_t) {
                *to_t = t;
            }
            if(tm < *fr_tm) {
                *fr_tm = tm;
            }
            if(tm > *to_tm) {
                *to_tm = tm;
            }
            n_rows++;
        }
    }
    int err = errno;
    close(fd);
    if(ln < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read md2 file to read its order",
            "path",         "%s", full_path,
            "errno",        "%d", err,
            "serrno",       "%s", strerror(err),
            NULL
        );
        return -1;
    }
    return n_rows;
}

/***************************************************************************
 *  Write the marker `<file_id>.<mark>` of a md2 file, if it is not there.
 *  Return 1 written, 0 already there, -1 error (logged).
 ***************************************************************************/
PRIVATE int write_order_marker(
    hgobj gobj,
    const char *key_directory,
    const char *file_id,
    const char *mark,
    int rpermission
)
{
    char marker[NAME_MAX];
    if(snprintf(marker, sizeof(marker), "%s.%s", file_id, mark) >= (int)sizeof(marker)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Cannot mark md2 file, file_id too long",
            "directory",    "%s", key_directory,
            "file_id",      "%s", file_id,
            "mark",         "%s", mark,
            NULL
        );
        return -1;
    }
    char path[PATH_MAX];
    if(!build_path(path, sizeof(path), key_directory, marker, NULL)) {
        return -1;  // Error already logged
    }
    if(is_regular_file(path)) {
        return 0;
    }
    int fd = newfile(path, rpermission, FALSE);
    if(fd < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot mark md2 file",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    close(fd);
    return 1;
}

/***************************************************************************
 *  See timeranger2.h
 ***************************************************************************/
PUBLIC json_t *tranger2_mark_tm_order(
    json_t *tranger,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(!tranger_is_master(gobj, tranger)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Only master can write",
            "topic_name",   "%s", topic_name,
            NULL
        );
        gobj_log_set_last_message("Only master can write");
        return NULL;
    }

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot mark a topic, topic not found",
            "topic_name",   "%s", topic_name,
            NULL
        );
        gobj_log_set_last_message("Topic not found: '%s'", topic_name);
        return NULL;
    }

    const char *topic_directory = json_string_value(json_object_get(topic, "directory"));
    int rpermission = (int)json_integer_value(json_object_get(topic, "rpermission"));
    BOOL was_marking = topic_marks_tm(topic);

    json_int_t n_keys = 0;
    json_int_t n_files = 0;
    json_int_t n_rows = 0;
    json_int_t t_marked = 0;
    json_int_t tm_marked = 0;
    BOOL failed = FALSE;

    const char *key; json_t *key_cache;
    json_object_foreach(json_object_get(topic, "cache"), key, key_cache) {
        n_keys++;
        char key_directory[PATH_MAX];
        if(!build_path(key_directory, sizeof(key_directory), topic_directory, "keys", key, NULL)) {
            failed = TRUE;  // Error already logged
            break;
        }

        /*
         *  The files on disk and the cells in memory are both in the order
         *  of the load (by name, see cmp_file_ids): they are walked
         *  TOGETHER, one cursor each. find_cache_cell() per file walked the
         *  cells from the first, O(files^2) per key: 2.5 s for 4 keys of
         *  3650 daily files, the yuno blocked all along (independent review
         *  of the third fix round).
         */
        json_t *cache_files = json_object_get(key_cache, "files");
        size_t n_cells = json_array_size(cache_files);
        size_t cell_idx = 0;

        dir_array_t da;
        find_files_with_suffix_array(gobj, key_directory, ".md2", &da);
        dir_array_sort(&da);
        for(int i = 0; i < da.count && !failed; i++) {
            char file_id[NAME_MAX];
            snprintf(file_id, sizeof(file_id), "%s", da.items[i]);
            char *dot = strrchr(file_id, '.');
            if(dot) {
                *dot = 0;
            }
            char full_path[PATH_MAX];
            build_path(full_path, sizeof(full_path), key_directory, da.items[i], NULL);

            uint64_t fr_t, to_t, fr_tm, to_tm;
            BOOL t_back, tm_back;
            json_int_t rows = scan_md2_order(
                gobj, full_path, &fr_t, &to_t, &fr_tm, &to_tm, &t_back, &tm_back
            );
            if(rows < 0) {
                failed = TRUE;  // Error already logged
                break;
            }
            n_files++;
            n_rows += rows;
            if(rows == 0) {
                continue;
            }

            /*
             *  A file that needs a marker and whose name leaves no room for
             *  one cannot have it: every load reads it whole already
             *  (load_cache_cell_from_disk), and its cell is flagged. It is
             *  skipped, not the topic: it aborted the whole migration.
             */
            if((t_back || tm_back) && strlen(file_id) + sizeof(".tm_unordered") > NAME_MAX) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "Cannot mark a md2 file, its name leaves no room for a marker: skipped, every load reads it whole",
                    "topic_name",   "%s", topic_name,
                    "key",          "%s", key,
                    "file_id",      "%s", file_id,
                    NULL
                );
                continue;
            }

            int w_t = t_back? write_order_marker(gobj, key_directory, file_id, "unordered", rpermission): 0;
            int w_tm = tm_back? write_order_marker(gobj, key_directory, file_id, "tm_unordered", rpermission): 0;
            if(w_t < 0 || w_tm < 0) {
                failed = TRUE;  // Error already logged
                break;
            }
            t_marked += w_t;
            tm_marked += w_tm;

            /*
             *  The cell in memory: its range is the whole file's, and its
             *  flags what the disk says now
             */
            json_t *cell = NULL;
            while(cell_idx < n_cells) {
                json_t *candidate = json_array_get(cache_files, cell_idx);
                const char *cell_id = json_string_value(json_object_get(candidate, "id"));
                int cmp = cmp_file_ids(cell_id? cell_id: "", file_id);
                if(cmp < 0) {
                    cell_idx++;
                    continue;
                }
                if(cmp == 0) {
                    cell = candidate;
                }
                break;
            }
            if(cell) {
                if(t_back) {
                    json_object_set_new(cell, "unordered", json_true());
                    json_object_del(cell, "unordered_not_on_disk");
                }
                if(tm_back) {
                    json_object_set_new(cell, "tm_unordered", json_true());
                    json_object_del(cell, "tm_unordered_not_on_disk");
                }
                if((json_int_t)fr_t < json_integer_value(json_object_get(cell, "fr_t"))) {
                    set_cache_int(cell, "fr_t", (json_int_t)fr_t);
                }
                if((json_int_t)to_t > json_integer_value(json_object_get(cell, "to_t"))) {
                    set_cache_int(cell, "to_t", (json_int_t)to_t);
                }
                if((json_int_t)fr_tm < json_integer_value(json_object_get(cell, "fr_tm"))) {
                    set_cache_int(cell, "fr_tm", (json_int_t)fr_tm);
                }
                if((json_int_t)to_tm > json_integer_value(json_object_get(cell, "to_tm"))) {
                    set_cache_int(cell, "to_tm", (json_int_t)to_tm);
                }
            }
        }
        dir_array_free(&da);

        /*
         *  Also after a failure half way through the key: the cells already
         *  widened are the key's cells, and its totals follow them.
         */
        update_totals_of_key_cache(gobj, topic, key);   // Errors already logged
        if(failed) {
            break;
        }
    }

    /*
     *  The segments an iterator took carry the flags of their moment: its
     *  next page takes them again (the stamp alone does not move, no row
     *  was added).
     */
    int idx; json_t *iterator;
    json_array_foreach(json_object_get(topic, "iterators"), idx, iterator) {
        json_object_set_new(iterator, "segments_stamp", json_null());
    }

    if(failed) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TRANGER,
            "msg",          "%s", "Cannot mark the topic: it is not marked; the markers written stay, and the cells read keep their whole ranges",
            "topic_name",   "%s", topic_name,
            NULL
        );
        gobj_log_set_last_message("Cannot mark topic '%s' (see the log)", topic_name);
        return NULL;
    }

    if(!was_marking) {
        char topic_dir[PATH_MAX];
        snprintf(topic_dir, sizeof(topic_dir), "%s", topic_directory);
        json_t *topic_desc = load_json_from_file(gobj, topic_dir, "topic_desc.json", 0);
        if(!topic_desc) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TRANGER,
                "msg",          "%s", "Cannot mark the topic, cannot read topic_desc.json",
                "topic_name",   "%s", topic_name,
                NULL
            );
            gobj_log_set_last_message("Cannot mark topic '%s': cannot read topic_desc.json", topic_name);
            return NULL;
        }
        json_object_set_new(topic_desc, "marks_tm_unordered", json_true());
        int ret = replace_json_file(gobj, tranger, topic_dir, "topic_desc.json", topic_desc, TRUE, TRUE);
        JSON_DECREF(topic_desc)
        if(ret < 0) {
            gobj_log_set_last_message("Cannot mark topic '%s': cannot write topic_desc.json", topic_name);
            return NULL;    // Error already logged
        }
        json_object_set_new(topic, "marks_tm_unordered", json_true());
    }

    json_t *report = json_pack("{s:s, s:b, s:I, s:I, s:I, s:I, s:I, s:b}",
        "topic_name", topic_name,
        "was_marking", was_marking,
        "keys", n_keys,
        "files", n_files,
        "rows", n_rows,
        "t_unordered_marked", t_marked,
        "tm_unordered_marked", tm_marked,
        "marks_tm_unordered", 1
    );
    gobj_log_info(gobj, 0,
        "function",             "%s", __FUNCTION__,
        "msgset",               "%s", MSGSET_INFO,
        "msg",                  "%s", "Topic marked: its md2 files out of order have their markers",
        "topic_name",           "%s", topic_name,
        "was_marking",          "%d", (int)was_marking,
        "keys",                 "%ld", (long)n_keys,
        "files",                "%ld", (long)n_files,
        "rows",                 "%ld", (long)n_rows,
        "t_unordered_marked",   "%ld", (long)t_marked,
        "tm_unordered_marked",  "%ld", (long)tm_marked,
        NULL
    );
    return report;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *tranger2_open_iterator( // LOADING: load data from disk, APPENDING: add real time data
    json_t *tranger,
    const char *topic_name,
    const char *key,    // required
    json_t *match_cond, // owned
    tranger2_load_record_callback_t load_record_callback, // called on loading and appending new record, optional
    const char *iterator_id,     // iterator id, optional, if empty will be the key
    const char *creator,
    json_t *data,       // JSON array, if not empty, fills it with the LOADING data, not owned
    json_t *extra       // owned, user data, this json will be added to the return iterator
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        JSON_DECREF(match_cond)     // Error already logged
        JSON_DECREF(extra)
        return NULL;
    }

    /*----------------------*
     *  Check parameters
     *----------------------*/
    if(!match_cond) {
        match_cond = json_object();
    }
    if(empty_string(key)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "What key?",
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return NULL;
    }
    if(empty_string(iterator_id)) {
        iterator_id = key;
    }
    if(!creator) {
        creator = "";
    }

    if(tranger2_get_iterator_by_id(tranger, topic_name, iterator_id, creator)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Iterator already exists",
            "topic_name",   "%s", topic_name,
            "key",          "%s", key,
            "id",           "%s", iterator_id,
            "creator",      "%s", creator,
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return NULL;
    }

    BOOL realtime;
    json_t *segments = get_segments(
        gobj,
        tranger,
        topic,
        key,
        match_cond, // NOT owned but can be modified
        &realtime
    );
    if(!load_record_callback) {
        realtime = FALSE;
    }

    json_t *iterator = json_object();
    json_object_set_new(iterator, "id", json_string(iterator_id));
    json_object_set_new(iterator, "creator", json_string(creator));
    json_object_set_new(iterator, "key", json_string(key));
    json_object_set_new(iterator, "topic_name", json_string(topic_name));
    json_object_set_new(iterator, "match_cond", match_cond);    // owned
    json_object_set_new(iterator, "segments", segments);        // owned
    json_object_set_new(iterator, "segments_stamp", key_cache_stamp(topic, key));

    json_object_set_new(iterator, "cur_segment", json_integer(0));
    json_object_set_new(iterator, "cur_rowid", json_integer(0));
    json_object_set_new(iterator, "list_type", json_string("iterator"));

    json_object_set_new(
        iterator,
        "load_record_callback",
        json_integer((json_int_t)(uintptr_t)load_record_callback)
    );
    json_object_update_missing_new(iterator, extra);

    /*
     *  A key with md2 files the cache build could not count (see
     *  load_key_cache_from_disk) has rows in no cell: its history is not
     *  whole, whatever the load finds, and the iterator says so. A load
     *  stops where the first of those files is in its direction -- where a
     *  running tranger stops when the damage happens behind its back.
     */
    json_t *unreadable = json_object_get(
        json_object_get(json_object_get(topic, "cache"), key), "unreadable"
    );
    if(json_array_size(unreadable) > 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TRANGER,
            "msg",          "%s", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
            "topic_name",   "%s", topic_name,
            "key",          "%s", key,
            "files",        "%j", unreadable,
            NULL
        );
        json_object_set_new(iterator, "load_failed", json_true());
    } else {
        unreadable = NULL;
    }

    /*-------------------------------------------------------------------------*
     *  WITH HISTORY:
     *      If there is "load_record_callback" then
     *          - callback all records in disk
     *  WITH REALTIME
     *      If there is "load_record_callback" and NO "to_rowid" defined then
     *          - get records in realtime, listening to changes in disk
     *-------------------------------------------------------------------------*/
    if(load_record_callback || data) {
        BOOL only_md = json_boolean_value(json_object_get(match_cond, "only_md"));

        /*---------------------------*
         *      History
         *---------------------------*/
        json_int_t rowid = 0;
        md2_record_ex_t md_record_ex;

        json_t *cache_total = get_cache_total(topic, key);
        BOOL backward = json_boolean_value(json_object_get(match_cond, "backward"));
        json_int_t cur_segment = first_segment_row(
            segments,
            cache_total,
            match_cond,
            topic_marks_tm(topic),
            &rowid
        );

        /*
         *  Save the pointer
         */
        if(cur_segment >= 0) {
            json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
            json_object_set_new(iterator, "cur_rowid", json_integer(rowid));
        }

        json_int_t total_rows = get_topic_key_rows(gobj, topic, key);

        /*
         *  A row whose metadata or whose content cannot be read ends the
         *  load, and the iterator says so in `load_failed` (the cause is
         *  logged where it failed): a caller that ASKS the history a
         *  question -- is any record of the key frozen by a snapshot? --
         *  must not read "no" in a load that stopped half way. The rows
         *  before it, in the load's direction, were handed to the callback.
         *
         *  A content that cannot be read was handed to the callback as
         *  NULL and the load went on: treedb made a node of it, with id ""
         *  (independent review of the third fix round). An only_md load
         *  reads no content, and no content fails it.
         */
        const char *first_unreadable = json_string_value(json_array_get(unreadable, 0));
        const char *last_unreadable = json_string_value(
            json_array_get(unreadable, json_array_size(unreadable) - 1)
        );
        BOOL end = FALSE;
        while(!end && cur_segment >= 0) {
            json_t *segment = json_array_get(segments, cur_segment);
            if(unreadable) {
                const char *seg_file_id = json_string_value(json_object_get(segment, "id"));
                if(!seg_file_id ||
                        (!backward && cmp_file_ids(seg_file_id, first_unreadable) > 0) ||
                        (backward && cmp_file_ids(seg_file_id, last_unreadable) < 0)) {
                    break;  // Error already logged, load_failed already set
                }
            }
            /*
             *  Get the metadata
             */
            if(get_md_by_rowid(
                gobj,
                tranger,
                topic,
                key,
                segment,
                rowid,
                &md_record_ex
            )<0) {
                json_object_set_new(iterator, "load_failed", json_true());
                break;  // Error already logged
            }
            if(is_deleted_instance(&md_record_ex)) {
                cur_segment = next_segment_row(
                    segments,
                    match_cond,
                    cur_segment,
                    &rowid
                );
                if(cur_segment >= 0) {
                    json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
                    json_object_set_new(iterator, "cur_rowid", json_integer(rowid));
                }
                continue;
            }
            BOOL end_segment = FALSE;
            if(tranger2_match_metadata(
                match_cond, total_rows, rowid, &md_record_ex,
                segment_t_ordered(segment), segment_tm_ordered(topic, segment),
                &end, &end_segment
            )) {
                const char *file_id = json_string_value(json_object_get(segment, "id"));
                json_t *record = NULL;

                md_record_ex.system_flag |= sf_loading_from_disk;

                if(!only_md) {
                    record = read_record_content(
                        tranger,
                        topic,
                        key,
                        file_id,
                        &md_record_ex
                    );
                    if(!record) {
                        json_object_set_new(iterator, "load_failed", json_true());
                        break;  // Error already logged
                    }
                    json_object_set_new(record, "__md_tranger__", md2json(&md_record_ex));
                }

                // Inform to the user list, historic
                if(load_record_callback) {
                    int ret = load_record_callback(
                        tranger,
                        topic,
                        key,    // key
                        iterator,
                        rowid,  // rowid
                        &md_record_ex,
                        json_incref(record) // must be owned
                    );
                    /*
                     *  Return:
                     *      0 do nothing (callback will create their own list, or not),
                     *      -1 break the load
                     */
                    if(ret < 0) {
                        JSON_DECREF(record)
                        break;
                    }
                }
                if(data) {
                    json_array_append(data, record);
                }
                JSON_DECREF(record)
            }
            if(end_segment) {
                rowid = leave_segment_row(segment, backward);
            }

            cur_segment = next_segment_row(
                segments,
                match_cond,
                cur_segment,
                &rowid
            );
            if(cur_segment >= 0) {
                json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
                json_object_set_new(iterator, "cur_rowid", json_integer(rowid));
            }
        }
    } else if(match_cond_selects_records(match_cond)) {
        /*-------------------------------------------------------------------*
         *  PAGING, FILTERED (no callback, no data: the caller will pull
         *  pages with tranger2_iterator_get_page()).
         *
         *  The loading path above matches every record against match_cond as
         *  it walks; the paging path cannot, so it needs the row index built
         *  upfront — otherwise its conditions would only be honored at file
         *  granularity and total_rows/pages would count records the pages
         *  never return.
         *-------------------------------------------------------------------*/
        json_t *index = build_iterator_index(
            gobj, tranger, topic, key, segments, match_cond
        );
        if(!index) {
            // Error already logged
            add_iterator_to_topic(gobj, topic, iterator);
            tranger2_close_iterator(tranger, iterator);
            return NULL;
        }
        json_object_set_new(iterator, "index", index);
    }

    add_iterator_to_topic(gobj, topic, iterator);

    return iterator;
}

/***************************************************************************
 *  An open iterator lives in the topic's "iterators" array, the band every
 *  walk reads (close_all_lists, the key_deleted fan-out), and in
 *  "iterators_by_id" {creator: {id: iterator}}, what tranger2_get_iterator_
 *  by_id() reads. The lookup was a linear walk of the array, and it runs at
 *  every open to refuse a duplicate: a multi-key iterator of N keys opens N
 *  iterators, which made its open O(N^2). The index holds a second reference;
 *  tranger2_close_iterator() removes both.
 ***************************************************************************/
PRIVATE void add_iterator_to_topic(hgobj gobj, json_t *topic, json_t *iterator)
{
    json_t *index = kw_get_dict(gobj, topic, "iterators_by_id", 0, KW_REQUIRED);
    const char *creator = kw_get_str(gobj, iterator, "creator", "", 0);
    const char *id = kw_get_str(gobj, iterator, "id", "", 0);
    json_t *by_creator = json_object_get(index, creator);
    if(!by_creator) {
        by_creator = json_object();
        json_object_set_new(index, creator, by_creator);
    }
    json_object_set(by_creator, id, iterator);

    json_array_append_new(
        kw_get_list(gobj, topic, "iterators", 0, KW_REQUIRED),
        iterator
    );
}

/***************************************************************************
 *  Drop the index entry of `iterator`, only if it is THAT iterator.
 ***************************************************************************/
PRIVATE void remove_iterator_from_index(json_t *topic, json_t *iterator)
{
    json_t *index = json_object_get(topic, "iterators_by_id");
    const char *creator = json_string_value(json_object_get(iterator, "creator"));
    const char *id = json_string_value(json_object_get(iterator, "id"));
    json_t *by_creator = json_object_get(index, creator?creator:"");
    if(by_creator && json_object_get(by_creator, id?id:"") == iterator) {
        json_object_del(by_creator, id?id:"");
        if(json_object_size(by_creator) == 0) {
            json_object_del(index, creator?creator:"");
        }
    }
}

/***************************************************************************
 *  Close iterator
 ***************************************************************************/
PUBLIC int tranger2_close_iterator(
    json_t *tranger,
    json_t *iterator
)
{
    // TODO cierra los file handlers usados !!!

    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    if(!iterator) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_close_iterator(): iterator NULL",
            NULL
        );
        return -1;
    }

    json_t *rt_mem = json_object_get(iterator, "rt_mem");
    if(rt_mem) {
        tranger2_close_rt_mem(tranger, rt_mem);
    }
    json_t *rt_disk = json_object_get(iterator, "rt_disk");
    if(rt_disk) {
        tranger2_close_rt_disk(tranger, rt_disk);
    }

    json_t *topic = tranger2_topic(tranger, kw_get_str(gobj, iterator, "topic_name", "", KW_REQUIRED));

    json_t *iterators = kw_get_list(gobj, topic, "iterators", 0, KW_REQUIRED);
    if(!iterators) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_close_iterator(): iterators not found",
            NULL
        );
        return -1;
    }

    int idx = json_array_find_idx(iterators, iterator);
    if(idx >=0 && idx < json_array_size(iterators)) {
        remove_iterator_from_index(topic, iterator);
        json_array_remove(
            iterators,
            idx
        );
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_close_iterator(): iterator not found",
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  Get realtime list by his id
 ***************************************************************************/
PUBLIC json_t *tranger2_get_iterator_by_id(
    json_t *tranger,
    const char *topic_name,
    const char *id,
    const char *creator
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(empty_string(id)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_get_iterator_by_id: What id?",
            NULL
        );
        return 0;
    }
    if(!creator) {
        creator = "";
    }

    json_t *topic = tranger2_topic(tranger, topic_name);
    json_t *by_creator = json_object_get(
        json_object_get(topic, "iterators_by_id"),
        creator
    );

    // Be silence, check at top.
    return json_object_get(by_creator, id);
}

/***************************************************************************
 *  TRUE if match_cond carries a condition that selects RECORDS, not just
 *  files. get_segments() can only reason about whole files (it compares the
 *  cache ranges of each .md2 file), so any of these needs a per-record pass
 *  to be honored exactly.
 ***************************************************************************/
PRIVATE BOOL match_cond_selects_records(json_t *match_cond)
{
    static const char *cond_keys[] = {
        "from_rowid", "to_rowid",
        "from_t", "to_t",
        "from_tm", "to_tm",
        "user_flag", "not_user_flag",
        "user_flag_mask_set", "user_flag_mask_notset",
        NULL
    };
    for(int i = 0; cond_keys[i] != NULL; i++) {
        if(json_integer_value(json_object_get(match_cond, cond_keys[i])) != 0) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Build the row INDEX of a paging iterator: the ordered list of the global
 *  rowids that actually match match_cond.
 *
 *  Without it a paged iterator can only filter by FILE: get_segments() drops
 *  the .md2 files whose cached range cannot intersect the conditions, but
 *  every record of a surviving file is still "in", and the row count is the
 *  file's. The index is what makes total_rows, the page count and the page
 *  contents exact — it applies tranger2_match_metadata() to each record's
 *  metadata (32 bytes, no record content is read).
 *
 *  Only built for iterators that filter (see match_cond_selects_records): an
 *  unfiltered iterator indexes nothing, keeps its zero-cost open, and its
 *  page positions ARE the global rowids.
 *
 *  Return is owned by the caller. NULL (error already logged) if the metadata
 *  cannot be read.
 ***************************************************************************/
PRIVATE json_t *build_iterator_index(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *segments,
    json_t *match_cond
)
{
    /*
     *  Walk FORWARD, always: the segments are in ascending order in both
     *  directions of get_segments(), and `backward` in match_cond would
     *  invert the meaning of match_metadata's early-exit flag. Direction is
     *  a property of the PAGE READ, not of the index.
     */
    json_t *forward_cond = json_deep_copy(match_cond);
    json_object_set_new(forward_cond, "backward", json_false());

    json_int_t total_rows = get_topic_key_rows(gobj, topic, key);
    json_t *index = json_array();
    md2_record_ex_t md_record_ex;
    BOOL end = FALSE;

    int idx; json_t *segment;
    json_array_foreach(segments, idx, segment) {
        json_int_t first_row = json_integer_value(json_object_get(segment, "first_row"));
        json_int_t last_row = json_integer_value(json_object_get(segment, "last_row"));

        for(json_int_t rowid = first_row; rowid <= last_row; rowid++) {
            if(get_md_by_rowid(gobj, tranger, topic, key, segment, rowid, &md_record_ex) < 0) {
                // Error already logged
                JSON_DECREF(forward_cond)
                JSON_DECREF(index)
                return NULL;
            }
            if(is_deleted_instance(&md_record_ex)) {
                continue;
            }
            BOOL end_segment = FALSE;
            if(tranger2_match_metadata(
                forward_cond, total_rows, rowid, &md_record_ex,
                segment_t_ordered(segment), segment_tm_ordered(topic, segment),
                &end, &end_segment
            )) {
                json_array_append_new(index, json_integer(rowid));
            }
            if(end || end_segment) {
                break;
            }
        }
        if(end) {
            break;
        }
    }

    JSON_DECREF(forward_cond)
    return index;
}

/***************************************************************************
 *  The segment holding a global rowid. NULL (silent) if no segment does —
 *  the caller decides how loud that is.
 ***************************************************************************/
PRIVATE json_t *segment_of_rowid(json_t *segments, json_int_t rowid)
{
    int idx; json_t *segment;
    json_array_foreach(segments, idx, segment) {
        json_int_t first_row = json_integer_value(json_object_get(segment, "first_row"));
        json_int_t last_row = json_integer_value(json_object_get(segment, "last_row"));
        if(rowid >= first_row && rowid <= last_row) {
            return segment;
        }
    }
    return NULL;
}

/***************************************************************************
 *  Get Iterator size (nº of rows)
 *
 *  A FILTERED iterator answers with its index: the exact number of records
 *  matching match_cond. An unfiltered one sums its segments, which is the
 *  same thing when nothing is filtered out.
 ***************************************************************************/
PUBLIC size_t tranger2_iterator_size(
    json_t *iterator
)
{
    json_t *index = json_object_get(iterator, "index");
    if(index) {
        return json_array_size(index);
    }

    size_t rows = 0;
    json_t *segments = json_object_get(iterator, "segments");
    if (json_array_size(segments) == 0) {
        return 0;
    }

    int idx;
    json_t *segment;
    json_array_foreach(segments, idx, segment) {
        json_int_t rows_ = json_integer_value(json_object_get(segment, "rows"));
        rows += rows_;
    }

    return rows;
}

/***************************************************************************
 *  A page that cannot read a row of its key: is the key still on disk? The
 *  delete of a key is seen (key_deleted) only in the process that deleted
 *  it; a replica's iterator just meets the files gone. Say so, beside the
 *  read error, so the cause is not guessed from an errno.
 ***************************************************************************/
PRIVATE BOOL log_if_key_gone(hgobj gobj, json_t *topic, const char *key)
{
    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir),
        json_string_value(json_object_get(topic, "directory")), "keys", key, NULL
    );
    if(!is_directory(key_dir)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TRANGER,
            "msg",          "%s", "Key gone from disk while its iterator was open: deleted (by the master?)",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            NULL
        );
        gobj_log_set_last_message("key '%s' was deleted", key);
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *      Get a page of records from iterator
 *      Return
 *          total_rows:     iterator size (nº of rows)
 *          pages:          number of pages with the required limit
 *          data:           list of required records found
 ***************************************************************************/
PUBLIC json_t *tranger2_iterator_get_page( // return must be owned
    json_t *tranger,
    json_t *iterator,
    json_int_t from_rowid,    // based 1
    size_t limit,
    BOOL backward
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    if(!iterator) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "tranger2_iterator_get_page(): iterator NULL",
            NULL
        );
        return NULL;
    }
    json_t *topic = tranger2_topic(
        tranger,
        kw_get_str(gobj, iterator, "topic_name", "", KW_REQUIRED)
    );
    const char *key = json_string_value(json_object_get(iterator, "key"));
    json_t *segments = json_object_get(iterator, "segments");

    /*
     *  A FILTERED iterator pages over its INDEX: `from_rowid` is then a
     *  position among the MATCHING records (1-based), not a global rowid, and
     *  total_rows/pages count only those. An unfiltered iterator has no index
     *  and keeps the original meaning (positions ARE global rowids) — nothing
     *  is filtered out, so the two coincide.
     */
    json_t *index = json_object_get(iterator, "index");
    if(index) {
        json_int_t indexed_rows = (json_int_t)json_array_size(index);
        json_int_t indexed_pages = (limit > 0)? (indexed_rows / (json_int_t)limit) : 0;
        if(limit > 0 && (indexed_rows % (json_int_t)limit) != 0) {
            indexed_pages++;
        }

        json_t *data = json_array();
        if(from_rowid > 0 && from_rowid <= indexed_rows && limit > 0) {
            md2_record_ex_t md_record_ex;
            for(size_t i = 0; i < limit; i++) {
                json_int_t pos = backward
                    ? (indexed_rows - (from_rowid - 1) - 1 - (json_int_t)i)
                    : (from_rowid - 1 + (json_int_t)i);
                if(pos < 0 || pos >= indexed_rows) {
                    break;
                }
                json_int_t rowid = json_integer_value(json_array_get(index, pos));
                json_t *segment = segment_of_rowid(segments, rowid);
                if(!segment) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "indexed rowid without segment",
                        "key",          "%s", key,
                        "rowid",        "%ld", (long)rowid,
                        NULL
                    );
                    break;
                }
                if(get_md_by_rowid(
                    gobj, tranger, topic, key, segment, rowid, &md_record_ex
                )<0) {
                    log_if_key_gone(gobj, topic, key);
                    break;      // Error already logged
                }
                const char *file_id = json_string_value(json_object_get(segment, "id"));
                json_t *record = read_record_content(
                    tranger, topic, key, file_id, &md_record_ex
                );
                if(!record) {
                    /*
                     *  The metadata can come from an fd opened before the
                     *  key went (the index was built with it): the content
                     *  file is the one that says so.
                     */
                    if(log_if_key_gone(gobj, topic, key)) {
                        break;  // Error already logged
                    }
                    continue;   // Error already logged
                }
                json_object_set_new(record, "__md_tranger__", md2json(&md_record_ex));
                json_array_append_new(data, record);
            }
        }

        return json_pack("{s:I, s:I, s:o}",
            "total_rows", indexed_rows,
            "pages", indexed_pages,
            "data", data
        );
    }

    /*
     *  An unfiltered iterator pages the key as it IS. Its segments were
     *  taken at the open and the appends since then are in none of them,
     *  while total_rows below is the live count: a page past the count of
     *  the open came back empty, and a client reading newest first missed
     *  the newest rows (N4 of the 2026-09-22 review). Taken again from the
     *  cache, which every append keeps current -- but only when the key's
     *  cache moved since they were taken: get_segments() deep-copies every
     *  cell of the key, and a client paging an idle key paid it per page.
     */
    json_t *stamp = key_cache_stamp(topic, key);
    if(!json_equal(stamp, json_object_get(iterator, "segments_stamp"))) {
        BOOL realtime;
        segments = get_segments(
            gobj,
            tranger,
            topic,
            key,
            json_object_get(iterator, "match_cond"),
            &realtime
        );
        json_object_set_new(iterator, "segments", segments);
        json_object_set_new(iterator, "segments_stamp", stamp);
    } else {
        JSON_DECREF(stamp)
    }

    json_int_t total_rows = get_topic_key_rows(gobj, topic, key);

    /*
     *  `pages` is a property of the KEY and the requested page size, not of
     *  this particular answer: an out-of-range page has no data, but the key
     *  still has its pages. Reporting 0 here told the client "there is
     *  nothing at all" and collapsed its pager to a single page.
     */
    json_int_t pages = (limit > 0)? (total_rows / (json_int_t)limit) : 0;
    if(limit > 0 && (total_rows % (json_int_t)limit) != 0) {
        pages++;
    }

    if(from_rowid <= 0 || from_rowid > total_rows || limit <= 0) {
        return json_pack("{s:I, s:I, s:[]}",
            "total_rows", total_rows,
            "pages", pages,
            "data"
        );
    }

    json_t *match_cond = json_object();
    json_object_set_new(match_cond, "from_rowid", json_integer(from_rowid));
    json_int_t to_rowid = from_rowid + (json_int_t)limit - 1;
    json_object_set_new(match_cond, "to_rowid", json_integer(to_rowid));
    json_object_set_new(match_cond, "backward", json_boolean(backward));

    json_int_t rowid = 0;
    json_t *cache_total = get_cache_total(topic, key);
    json_int_t cur_segment = first_segment_row(
        segments,
        cache_total,
        match_cond,
        topic_marks_tm(topic),
        &rowid
    );

    if(cur_segment < 0) {
        JSON_DECREF(match_cond)
        return json_pack("{s:I, s:I, s:[]}",
            "total_rows", total_rows,
            "pages", pages,
            "data"
        );
    }

    /*
     *  Save the pointer
     */
    json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
    json_object_set_new(iterator, "cur_rowid", json_integer(rowid));

    json_t *data = json_array();
    md2_record_ex_t md_record_ex;

    BOOL end = FALSE;
    while(!end && cur_segment >= 0) {
        json_t *segment = json_array_get(segments, cur_segment);
        /*
         *  Get the metadata
         */
        if(get_md_by_rowid(
            gobj,
            tranger,
            topic,
            key,
            segment,
            rowid,
            &md_record_ex
        )<0) {
            log_if_key_gone(gobj, topic, key);
            break;      // Error already logged
        }

        if(is_deleted_instance(&md_record_ex)) {
            cur_segment = next_segment_row(
                segments,
                match_cond,
                cur_segment,
                &rowid
            );
            if(cur_segment >= 0) {
                json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
                json_object_set_new(iterator, "cur_rowid", json_integer(rowid));
            }
            continue;
        }

        BOOL end_segment = FALSE;
        if(tranger2_match_metadata(
            match_cond, total_rows, rowid, &md_record_ex,
            segment_t_ordered(segment), segment_tm_ordered(topic, segment),
            &end, &end_segment
        )) {
            const char *file_id = json_string_value(json_object_get(segment, "id"));
            json_t *record = read_record_content(
                tranger,
                topic,
                key,
                file_id,
                &md_record_ex
            );
            if(record) {
                json_object_set_new(record, "__md_tranger__", md2json(&md_record_ex));
                json_array_append_new(data, record);
            } else if(log_if_key_gone(gobj, topic, key)) {
                break;  // Error already logged
            }
        }
        if(end_segment) {
            rowid = leave_segment_row(segment, backward);
        }

        cur_segment = next_segment_row(
            segments,
            match_cond,
            cur_segment,
            &rowid
        );
        if(cur_segment >= 0) {
            json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
            json_object_set_new(iterator, "cur_rowid", json_integer(rowid));
        }
    }

    JSON_DECREF(match_cond)
    return json_pack("{s:I, s:I, s:o}",
        "total_rows", total_rows,
        "pages", pages,
        "data", data
    );
}

/***************************************************************************
 *  What the segments of a key are taken from, in three numbers: the rows
 *  of the key, its files, and its last file. An append moves the rows, a
 *  new file the count and the last id. While the key lives its cells only
 *  grow, so equal stamps are equal segments. A key deleted and written
 *  again can come back with the same three numbers: its delete drops the
 *  stamp of every iterator of the key (forget_segments_of_key).
 ***************************************************************************/
PRIVATE json_t *key_cache_stamp(json_t *topic, const char *key)
{
    json_t *cache_files = get_cache_files(topic, key);
    json_t *cache_total = get_cache_total(topic, key);
    size_t n_files = json_array_size(cache_files);
    json_t *last_file = n_files > 0? json_array_get(cache_files, n_files - 1): NULL;
    return json_pack("{s:I, s:I, s:s}",
        "rows", (json_int_t)json_integer_value(json_object_get(cache_total, "rows")),
        "files", (json_int_t)n_files,
        "last_file", last_file? json_string_value(json_object_get(last_file, "id")): ""
    );
}

/***************************************************************************
 *  The key was deleted: what its iterators took from its cache names rows
 *  that are gone. The stamp above does not see it when the key is written
 *  again with the same numbers and its rows spread another way over its
 *  files, and a page read the new files with the old segments (L1 of the
 *  2026-09-23 independent review). So an unfiltered iterator loses its
 *  segments and its stamp, and takes them again at its next page; a
 *  filtered one loses its index -- the rows it indexed do not exist any
 *  more, and an index is built only at the open.
 ***************************************************************************/
PRIVATE void forget_segments_of_key(json_t *topic, const char *key)
{
    json_t *iterators = json_object_get(topic, "iterators");
    int idx; json_t *iterator;
    json_array_foreach(iterators, idx, iterator) {
        const char *key_ = json_string_value(json_object_get(iterator, "key"));
        if(!key_ || strcmp(key_, key) != 0) {
            continue;
        }
        json_object_set_new(iterator, "segments", json_array());
        json_object_set_new(iterator, "segments_stamp", json_null());
        if(json_object_get(iterator, "index")) {
            json_object_set_new(iterator, "index", json_array());
        }
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_cache_files(json_t *topic, const char *key)
{
    json_t *cache = json_object_get(topic, "cache");
    if(!cache) {
        return NULL;
    }
    json_t *key_cache = json_object_get(cache, key);
    if(!key_cache) {
        return NULL;
    }
    json_t *cache_files = json_object_get(key_cache, "files");
    return cache_files;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_cache_total(json_t *topic, const char *key)
{
    json_t *cache = json_object_get(topic, "cache");
    if(!cache) {
        return NULL;
    }
    json_t *key_cache = json_object_get(cache, key);
    if(!key_cache) {
        return NULL;
    }
    json_t *cache_total = json_object_get(key_cache, "total");
    return cache_total;
}

/***************************************************************************
 *  Return a list of segments that match conditions
 *  match_cond can be modified in (times in string)
 ***************************************************************************/
PRIVATE json_t *get_segments(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *match_cond, // NOT owned but can be modified
    BOOL *prealtime
)
{
    BOOL backward = json_boolean_value(json_object_get(match_cond, "backward"));
    BOOL realtime = FALSE;
    *prealtime = realtime;

    json_t *jn_segments = json_array();

    /*-------------------------------------*
     *      Recover cache data
     *-------------------------------------*/
    json_t *cache_files = get_cache_files(topic, key);
    if(!cache_files) {
        /*
         *  Key not exits
         */
        return jn_segments;
    }

    json_t *cache_total = get_cache_total(topic, key);
    if(!cache_total) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "no cache",
            "topic",        "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            NULL
        );
        return jn_segments;
    }

    json_int_t total_from_t = kw_get_int(gobj, cache_total, "fr_t", 0, KW_REQUIRED);
    json_int_t total_to_t = kw_get_int(gobj, cache_total, "to_t", 0, KW_REQUIRED);
    json_int_t total_from_tm = kw_get_int(gobj, cache_total, "fr_tm", 0, KW_REQUIRED);
    json_int_t total_to_tm = kw_get_int(gobj, cache_total, "to_tm", 0, KW_REQUIRED);
    json_int_t total_rows = kw_get_int(gobj, cache_total, "rows", 0, KW_REQUIRED);

    if(total_rows == 0) {
        // NO rows
        return jn_segments;
    }

    /*-------------------------------------*
     *      Check range rows in totals
     *-------------------------------------*/
    json_int_t from_rowid = 0;
    json_t *jn_from_rowid = json_object_get(match_cond, "from_rowid");
    if(json_is_string(jn_from_rowid)) {
        from_rowid = kw_get_int(gobj, match_cond, "from_rowid", 0, KW_WILD_NUMBER);
        json_object_set_new(match_cond, "from_rowid", json_integer(from_rowid));
    } else {
        from_rowid = json_integer_value(json_object_get(match_cond, "from_rowid"));
    }

    json_int_t to_rowid = 0;
    json_t *jn_to_rowid = json_object_get(match_cond, "to_rowid");
    if(json_is_string(jn_to_rowid)) {
        to_rowid = kw_get_int(gobj, match_cond, "to_rowid", 0, KW_WILD_NUMBER);
        json_object_set_new(match_cond, "to_rowid", json_integer(to_rowid));
    } else {
        to_rowid = json_integer_value(json_object_get(match_cond, "to_rowid"));
    }

    // WARNING adjust REPEATED
    if(from_rowid == 0) {
        from_rowid = 1;
    } else if(from_rowid > 0) {
        // positive offset
        if(from_rowid > total_rows) {
            // not exist
            return jn_segments;
        }
    } else {
        // negative offset
        if(from_rowid < -total_rows) {
            // out of range, begin at 0
            from_rowid = 1;
        } else {
            from_rowid = total_rows + from_rowid + 1;
        }
    }

    if(to_rowid == 0) {
        realtime = TRUE;
        to_rowid = total_rows;
    } else if(to_rowid > 0) {
        // positive offset
        if(to_rowid > total_rows) {
            // out of range, begin at 0
            to_rowid = total_rows;
        }
    } else {
        // negative offset
        if(to_rowid < -total_rows) {
            // not exist
            return jn_segments;
        } else {
            to_rowid = total_rows + to_rowid + 1;
        }
    }

    if(to_rowid < from_rowid) {
        // Bad range
        return jn_segments;
    }

    /*-------------------------------------*
     *      Check range t in totals
     *-------------------------------------*/
    json_int_t from_t = 0;
    json_t *jn_from_t = json_object_get(match_cond, "from_t");
    if(json_is_string(jn_from_t)) {
        if(strchr(json_string_value(jn_from_t), 'T')!=0) {
            timestamp_t timestamp;
            timestamp = approxidate(json_string_value(jn_from_t));
            system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
            if(system_flag & sf_tm_ms) {
                timestamp *= 1000;
            }
            from_t = (json_int_t)timestamp;
        } else {
            from_t = kw_get_int(gobj, match_cond, "from_t", 0, KW_WILD_NUMBER);
        }
        json_object_set_new(match_cond, "from_t", json_integer(from_t));
    } else {
        from_t = json_integer_value(json_object_get(match_cond, "from_t"));
    }

    // WARNING adjust
    if(from_t == 0) {
        from_t = total_from_t;
    } else {
        if(from_t > total_to_t) {
            // not exist
            return jn_segments;
        } else if(from_t < total_from_t) {
            // out of range, begin at start
            from_t = total_from_t;
        }
    }

    json_int_t to_t = 0;
    json_t *jn_to_t = json_object_get(match_cond, "to_t");
    if(json_is_string(jn_to_t)) {
        if(strchr(json_string_value(jn_to_t), 'T')!=0) {
            timestamp_t timestamp;
            timestamp = approxidate(json_string_value(jn_to_t));
            system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
            if(system_flag & sf_tm_ms) {
                timestamp *= 1000;
            }
            to_t = (json_int_t)timestamp;
        } else {
            to_t = kw_get_int(gobj, match_cond, "to_t", 0, KW_WILD_NUMBER);
        }
        json_object_set_new(match_cond, "to_t", json_integer(to_t));
    } else {
        to_t = json_integer_value(json_object_get(match_cond, "to_t"));
    }

    // WARNING adjust
    if(to_t == 0) {
        to_t = total_to_t;
    } else {
        if(realtime) {
            realtime = FALSE;
        }
        if(to_t > total_to_t) {
            // out of range, begin at the end
            to_t = total_to_t;
        } else if(to_t < total_from_t) {
            // not exist
            return jn_segments;
        }
    }

    /*-------------------------------------*
     *      Check range tm in totals
     *-------------------------------------*/
    json_int_t from_tm = 0;
    json_t *jn_from_tm = json_object_get(match_cond, "from_tm");
    if(json_is_string(jn_from_tm)) {
        if(strchr(json_string_value(jn_from_tm), 'T')!=0) {
            timestamp_t timestamp;
            timestamp = approxidate(json_string_value(jn_from_tm));
            system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
            if(system_flag & sf_tm_ms) {
                timestamp *= 1000;
            }
            from_tm = (json_int_t)timestamp;
        } else {
            from_tm = kw_get_int(gobj, match_cond, "from_tm", 0, KW_WILD_NUMBER);
        }
        json_object_set_new(match_cond, "from_tm", json_integer(from_tm));
    } else {
        from_tm = json_integer_value(json_object_get(match_cond, "from_tm"));
    }

    /*
     *  The tm ranges of the cache are trusted only in a topic that marks
     *  the files whose tm goes back (topic_marks_tm): in one written before
     *  the marks, a file's tm range came from its first and last rows, and
     *  a file holding a matching row could be left out. There no file is
     *  left out by tm; the rows are.
     */
    BOOL tm_known = topic_marks_tm(topic);

    // WARNING adjust
    if(from_tm == 0) {
        from_tm = total_from_tm;
    } else if(tm_known) {
        if (from_tm > total_to_tm) {
            // not exist
            return jn_segments;
        } else if (from_tm < total_from_tm) {
            // out of range, begin at start
            from_tm = total_from_tm;
        }
    }

    json_int_t to_tm = 0;
    json_t *jn_to_tm = json_object_get(match_cond, "to_tm");
    if(json_is_string(jn_to_tm)) {
        if(strchr(json_string_value(jn_to_tm), 'T')!=0) {
            timestamp_t timestamp;
            timestamp = approxidate(json_string_value(jn_to_tm));
            system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
            if(system_flag & sf_tm_ms) {
                timestamp *= 1000;
            }
            to_tm = (json_int_t)timestamp;
        } else {
            to_tm = kw_get_int(gobj, match_cond, "to_tm", 0, KW_WILD_NUMBER);
        }
        json_object_set_new(match_cond, "to_tm", json_integer(to_tm));
    } else {
        to_tm = json_integer_value(json_object_get(match_cond, "to_tm"));
    }

    // WARNING adjust
    if(to_tm == 0) {
        to_tm = total_to_tm;
    } else {
        if(realtime) {
            realtime = FALSE;
        }
        if(tm_known) {
            if (to_tm > total_to_tm) {
                // out of range, begin at the end
                to_tm = total_to_tm;
            } else if (to_tm < total_from_tm) {
                // not exist
                return jn_segments;
            }
        }
    }

    *prealtime = realtime;

    /*------------------------------------------*
     *  Search the first file in cache files
     *  to begin the loop
     *  MATCHING RANGE
     *------------------------------------------*/
    if(!backward) {
        int idx; json_t *cache_file;
        json_int_t partial_rows2 = 1;

        json_array_foreach(cache_files, idx, cache_file) {
            json_int_t rows2 = kw_get_int(gobj, cache_file, "rows", 0, KW_REQUIRED);
            json_int_t rangeStart = partial_rows2; // first row of this segment
            json_int_t rangeEnd = partial_rows2 + rows2 - 1; // last row of this segment
            // If the current range starts after the input range ends, stop further checks
            if (rangeStart > to_rowid) {
                break;
            }

            json_int_t rangeT_start = kw_get_int(gobj, cache_file, "fr_t", 0, KW_REQUIRED);
            json_int_t rangeT_end = kw_get_int(gobj, cache_file, "to_t", 0, KW_REQUIRED);
            if(rangeT_start > to_t) {
                break;
            }

            /*
             *  No early break on tm: the files are cut by __t__, and the tm
             *  written by a producer need not grow with it. A file out of
             *  the tm range is skipped by the test below, not the end, and
             *  the segments then have a HOLE of rowids: the scans step over
             *  it (see next_segment_row).
             */
            json_int_t rangeTM_start = kw_get_int(gobj, cache_file, "fr_tm", 0, KW_REQUIRED);
            json_int_t rangeTM_end = kw_get_int(gobj, cache_file, "to_tm", 0, KW_REQUIRED);

            // Print only the valid ranges
            if (rangeStart <= to_rowid && rangeEnd >= from_rowid &&
                rangeT_start <= to_t && rangeT_end >= from_t &&
                (!tm_known || (rangeTM_start <= to_tm && rangeTM_end >= from_tm))
            ) {
                json_t *jn_segment = json_deep_copy(cache_file);
                json_object_set_new(jn_segment, "first_row", json_integer(rangeStart));
                json_object_set_new(jn_segment, "last_row", json_integer(rangeEnd));
                json_object_set_new(jn_segment, "key", json_string(key));
                json_array_append_new(jn_segments, jn_segment);
            }

            partial_rows2 += rows2;
        }
    } else {
        int idx; json_t *cache_file;
        json_int_t partial_rows2 = total_rows;

        json_array_backward(cache_files, idx, cache_file) {
            json_int_t rows2 = kw_get_int(gobj, cache_file, "rows", 0, KW_REQUIRED);
            json_int_t rangeStart = partial_rows2 - rows2 + 1; // first row of this segment
            json_int_t rangeEnd = partial_rows2;  // last row of this segment
            // If the current range ends before the input range starts, stop further checks
            if (rangeEnd < from_rowid) {
                break;
            }

            json_int_t rangeT_start = kw_get_int(gobj, cache_file, "fr_t", 0, KW_REQUIRED);
            json_int_t rangeT_end = kw_get_int(gobj, cache_file, "to_t", 0, KW_REQUIRED);
            if (rangeT_end < from_t) {
                break;
            }

            /*  No early break on tm: see the forward loop  */
            json_int_t rangeTM_start = kw_get_int(gobj, cache_file, "fr_tm", 0, KW_REQUIRED);
            json_int_t rangeTM_end = kw_get_int(gobj, cache_file, "to_tm", 0, KW_REQUIRED);

            // Print only the valid ranges
            if (rangeStart <= to_rowid && rangeEnd >= from_rowid &&
                rangeT_start <= to_t && rangeT_end >= from_t &&
                (!tm_known || (rangeTM_start <= to_tm && rangeTM_end >= from_tm))
            ) {

                json_t *jn_segment = json_deep_copy(cache_file);
                json_object_set_new(jn_segment, "first_row", json_integer(rangeStart));
                json_object_set_new(jn_segment, "last_row", json_integer(rangeEnd));
                json_object_set_new(jn_segment, "key", json_string(key));
                json_array_insert_new(jn_segments, 0, jn_segment);
            }
            partial_rows2 -= rows2;
        }
    }

    return jn_segments;
}

/***************************************************************************
 *  Are the rows of a segment in __t__ order? Not when the master marked
 *  its md2 file `unordered` (a late record); the flag travels in the
 *  segment, a deep copy of the cache cell.
 ***************************************************************************/
PRIVATE BOOL segment_t_ordered(json_t *segment)
{
    return json_is_true(json_object_get(segment, "unordered"))? FALSE: TRUE;
}

/***************************************************************************
 *  Does the topic mark its md2 files whose __tm__ goes back? Every topic
 *  created since the marks exist says so in its topic_desc.json
 *  (`marks_tm_unordered`). One written before them cannot tell which of
 *  its files are in tm order, nor trust the tm range of any: its first and
 *  last rows gave it.
 ***************************************************************************/
PRIVATE BOOL topic_marks_tm(json_t *topic)
{
    return json_is_true(json_object_get(topic, "marks_tm_unordered"))? TRUE: FALSE;
}

/***************************************************************************
 *  Are the rows of a segment in __tm__ order? Only in a topic that marks
 *  its files (topic_marks_tm) and a file that is not marked.
 ***************************************************************************/
PRIVATE BOOL segment_tm_ordered(json_t *topic, json_t *segment)
{
    if(!topic_marks_tm(topic)) {
        return FALSE;
    }
    return json_is_true(json_object_get(segment, "tm_unordered"))? FALSE: TRUE;
}

/***************************************************************************
 *  The row a scan leaves `segment` from: its last in a forward scan, its
 *  first in a backward one. A scan told that no later row of the segment
 *  matches (`end_segment`) goes on from there, into the next segment.
 ***************************************************************************/
PRIVATE json_int_t leave_segment_row(json_t *segment, BOOL backward)
{
    return json_integer_value(json_object_get(segment, backward? "first_row": "last_row"));
}

/***************************************************************************
 *  Used by tranger2_iterator_get_page() where rowid/limit is set
 *      as from_rowid/to_rowid in a self create match_cond
 *  and by tranger2_open_iterator()
 *
 *  `end` tells the scan that no later row (in the scan's direction) can
 *  match, so it may stop. That is only true of a field whose rows are in
 *  order:
 *      - rowid: always.
 *      - __t__: only in a segment whose md2 file is not marked `unordered`
 *        (`t_ordered`). A late record sits AFTER rows with a higher t, and
 *        a scan that stopped at the first row past the range lost it, in
 *        both directions. The files are cut by t, so the end of the segment
 *        is the end of the scan.
 *
 *  `end_segment` tells it that no later row OF THIS SEGMENT can match: the
 *  scan goes on in the next segment. That is what __tm__ gives, and only in
 *  a segment in tm order (`tm_ordered`, see segment_tm_ordered): tm is the
 *  time the record was CREATED, written by the producer, and the files are
 *  cut by t, so a later file may hold a lower tm; and inside a file marked
 *  `tm_unordered` a tm condition skips a row and ends nothing.
 ***************************************************************************/
PRIVATE BOOL tranger2_match_metadata(
    json_t *match_cond,
    json_int_t total_rows,
    json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    BOOL t_ordered,
    BOOL tm_ordered,
    BOOL *end,
    BOOL *end_segment
)
{
    BOOL backward = json_boolean_value(json_object_get(match_cond, "backward"));
    *end = FALSE;
    *end_segment = FALSE;

    /*--------------------------*
     *      Rowid
     *--------------------------*/
    json_int_t from_rowid = json_integer_value(json_object_get(match_cond, "from_rowid"));
    json_int_t to_rowid = json_integer_value(json_object_get(match_cond, "to_rowid"));

    // WARNING adjust REPEATED
    if(from_rowid == 0) {
        from_rowid = 1;
    } else if(from_rowid > 0) {
        // positive offset
        if(from_rowid > total_rows) {
            // not exist
            *end = TRUE;
            return FALSE;
        }
    } else {
        // negative offset
        if(from_rowid < -total_rows) {
            // out of range, begin at 0
            from_rowid = 1;
        } else {
            from_rowid = total_rows + from_rowid + 1;
        }
    }

    if(to_rowid == 0) {
        to_rowid = total_rows;
    } else if(to_rowid > 0) {
        // positive offset
        if(to_rowid > total_rows) {
            // out of range, begin at 0
            to_rowid = total_rows;
        }
    } else {
        // negative offset
        if(to_rowid < -total_rows) {
            // not exist
            *end = TRUE;
            return FALSE;
        } else {
            to_rowid = total_rows + to_rowid + 1;
        }
    }

    if(from_rowid != 0) {
        if(rowid < from_rowid) {
            if(backward) {
                *end = TRUE;
            }
            return FALSE;
        }
    }

    if(to_rowid != 0) {
        if(rowid > to_rowid) {
            if(!backward) {
                *end = TRUE;
            }
            return FALSE;
        }
    }

    /*--------------------------*
     *      t
     *--------------------------*/
    json_int_t from_t = json_integer_value(json_object_get(match_cond, "from_t"));
    json_int_t to_t = json_integer_value(json_object_get(match_cond, "to_t"));

    if(from_t != 0) {
        if(md_record_ex->__t__ < from_t) {
            if(backward && t_ordered) {
                *end = TRUE;
            }
            return FALSE;
        }
    }

    if(to_t != 0) {
        if(md_record_ex->__t__ > to_t) {
            if(!backward && t_ordered) {
                *end = TRUE;
            }
            return FALSE;
        }
    }

    /*--------------------------*
     *      tm
     *--------------------------*/
    json_int_t from_tm = json_integer_value(json_object_get(match_cond, "from_tm"));
    json_int_t to_tm = json_integer_value(json_object_get(match_cond, "to_tm"));

    if(from_tm != 0) {
        if(md_record_ex->__tm__ < from_tm) {
            if(backward && tm_ordered) {
                *end_segment = TRUE;
            }
            return FALSE;
        }
    }

    if(to_tm != 0) {
        if(md_record_ex->__tm__ > to_tm) {
            if(!backward && tm_ordered) {
                *end_segment = TRUE;
            }
            return FALSE;
        }
    }

    /*--------------------------*
     *  user_flag
     *  not_user_flag
     *  user_flag_mask_set
     *  user_flag_mask_notset
     *--------------------------*/
    uint16_t user_flag = (uint16_t)json_integer_value(
        json_object_get(match_cond, "user_flag")
    );
    if(user_flag) {
        if((md_record_ex->user_flag != user_flag)) {
            return FALSE;
        }
    }

    uint16_t not_user_flag = (uint16_t)json_integer_value(
        json_object_get(match_cond, "not_user_flag")
    );
    if(not_user_flag) {
        if(md_record_ex->user_flag == not_user_flag) {
            return FALSE;
        }
    }

    uint16_t user_flag_mask_set = (uint16_t)json_integer_value(
        json_object_get(match_cond, "user_flag_mask_set")
    );
    if(user_flag_mask_set) {
        if((md_record_ex->user_flag & user_flag_mask_set) != user_flag_mask_set) {
            return FALSE;
        }
    }

    uint16_t user_flag_mask_notset = (uint16_t)json_integer_value(
        json_object_get(match_cond, "user_flag_mask_notset")
    );
    if(user_flag_mask_notset) {
        if((md_record_ex->user_flag | ~user_flag_mask_notset) != ~user_flag_mask_notset) {
            return FALSE;
        }
    }

    return TRUE;
}

/***************************************************************************
 *  Here searching only by rowid between segments previously matched
 *  by all others like t, tm
 *  Used by tranger2_iterator_get_page() where rowid/limit is set
 *      as from_rowid/to_rowid in a self created match_cond
 *  and by tranger2_open_iterator()
 *  In this point all segments are matched, this function is only
 *  to search the first segment to begin.
 *
 *  The segments can have holes (a file left out by tm): a rowid bound that
 *  falls in one begins at the first row of the next segment in the scan's
 *  direction, never at a row the segment does not hold.
 *  `tm_known`: the tm ranges of the segments can be trusted (topic_marks_tm).
 ***************************************************************************/
PRIVATE json_int_t first_segment_row(
    json_t *segments,
    json_t *cache_total,
    json_t *match_cond,  // not owned
    BOOL tm_known,
    json_int_t *prowid
)
{
    BOOL backward = json_boolean_value(json_object_get(match_cond, "backward"));

//    json_int_t total_from_t = json_integer_value(json_object_get(cache_total, "fr_t"));
//    json_int_t total_to_t = json_integer_value(json_object_get(cache_total, "to_t"));
//    json_int_t total_from_tm = json_integer_value(json_object_get(cache_total, "fr_tm"));
//    json_int_t total_to_tm = json_integer_value(json_object_get(cache_total, "to_tm"));
    json_int_t total_rows = json_integer_value(json_object_get(cache_total, "rows"));

    *prowid = -1;
    size_t segments_size = json_array_size(segments);
    if(segments_size == 0) {
        // Here silence, avoid multiple logs, only logs in first/last
        return -1;
    }

    json_int_t rowid;
    int idx; json_t *segment;
    if(!backward) {
        json_int_t from_rowid = json_integer_value(json_object_get(match_cond, "from_rowid"));
        json_int_t from_tm = json_integer_value(json_object_get(match_cond, "from_tm"));
        json_int_t from_t = json_integer_value(json_object_get(match_cond, "from_t"));

        json_array_foreach(segments, idx, segment) {
            json_int_t seg_first_rowid = json_integer_value(json_object_get(segment, "first_row"));
            json_int_t seg_last_rowid = json_integer_value(json_object_get(segment, "last_row"));

            json_int_t seg_last_t = json_integer_value(json_object_get(segment, "to_t"));
            json_int_t seg_last_tm = json_integer_value(json_object_get(segment, "to_tm"));

            // WARNING adjust REPEATED
            if(from_rowid == 0) {
                rowid = seg_first_rowid;
            } else if(from_rowid > 0) {
                // positive offset
                if(from_rowid > total_rows) {
                    // not exist
                    return -1;
                }
                rowid = from_rowid;
            } else {
                // negative offset
                if(from_rowid < -total_rows) {
                    // out of range, begin at 0
                    rowid = seg_first_rowid;
                } else {
                    rowid = total_rows + from_rowid + 1;
                }
            }

            do {
                if(rowid > seg_last_rowid) {
                    // no match, break and continue
                    break;
                }

                if(from_t != 0) {
                    if(from_t > seg_last_t) {
                        // no match, break and continue
                        break;
                    }
                }

                if(from_tm != 0 && tm_known) {
                    if(from_tm > seg_last_tm) {
                        // no match, break and continue
                        break;
                    }
                }

                // Match
                if(rowid < seg_first_rowid) {
                    rowid = seg_first_rowid;    // the bound fell in a hole
                }
                *prowid = rowid;
                return idx;
            } while(0);
        }

    } else {
        json_int_t to_rowid = json_integer_value(json_object_get(match_cond, "to_rowid"));
        json_int_t to_tm = json_integer_value(json_object_get(match_cond, "to_tm"));
        json_int_t to_t = json_integer_value(json_object_get(match_cond, "to_t"));

        json_array_backward(segments, idx, segment) {
            json_int_t seg_first_rowid = json_integer_value(json_object_get(segment, "first_row"));
            json_int_t seg_last_rowid = json_integer_value(json_object_get(segment, "last_row"));

            json_int_t seg_first_t = json_integer_value(json_object_get(segment, "fr_t"));
            json_int_t seg_first_tm = json_integer_value(json_object_get(segment, "fr_tm"));

            // WARNING adjust REPEATED
            if(to_rowid == 0) {
                rowid = seg_last_rowid;
            } else if(to_rowid > 0) {
                // positive offset
                if(to_rowid > total_rows) {
                    // out of range, begin at 0
                    rowid = seg_last_rowid;
                } else {
                    rowid = to_rowid;
                }
            } else {
                // negative offset
                if(to_rowid < -total_rows) {
                    // not exist
                    return -1;
                } else {
                    rowid = total_rows + to_rowid + 1;
                }
            }

            do {
                if(rowid < seg_first_rowid) {
                    // no match, break and continue
                    break;
                }

                if(to_t != 0) {
                    if(to_t < seg_first_t) {
                        break;
                    }
                }

                if(to_tm != 0 && tm_known) {
                    if(to_tm < seg_first_tm) {
                        break;
                    }
                }

                // Match
                if(rowid > seg_last_rowid) {
                    rowid = seg_last_rowid;     // the bound fell in a hole
                }
                *prowid = rowid;
                return idx;
            } while(0);
        }
    }

    return -1;
}

/***************************************************************************
 *  The next row of a scan, in its direction, and the segment that holds
 *  it. The segments are in rowid order but need not be CONSECUTIVE: a file
 *  left out by a tm condition is a hole (get_segments), and the scan goes
 *  on at the first row of the next segment. Only rows going BACK in the
 *  scan's direction would be a broken invariant.
 ***************************************************************************/
PRIVATE json_int_t next_segment_row(
    json_t *segments,
    json_t *match_cond,  // not owned
    json_int_t cur_segment,
    json_int_t *rowid
)
{
    hgobj gobj = 0;
    BOOL backward = json_boolean_value(json_object_get(match_cond, "backward"));
    json_int_t cur_rowid = *rowid;
    *rowid = -1;

    size_t segments_size = json_array_size(segments);
    if(segments_size == 0) {
        // Here silence, avoid multiple logs, only logs in first/last
        return -1;
    }

    json_t *segment = json_array_get(segments, cur_segment);

    if(!backward) {
        /*
         *  Increment rowid
         */
        cur_rowid++;

        /*
         *  Check if is in the same segment, if not then go to the next segment
         */
        json_int_t segment_last_row = json_integer_value(json_object_get(segment, "last_row"));

        if(cur_rowid > segment_last_row) {
            // Go to the next segment
            cur_segment++;
            if(cur_segment >= segments_size) {
                // No more
                return -1;
            }
            segment = json_array_get(segments, cur_segment);
            json_int_t segment_first_row = json_integer_value(json_object_get(segment, "first_row"));
            if(cur_rowid > segment_first_row) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "next segment begins before the row just read",
                    "cur_rowid",    "%ld", (long)cur_rowid,
                    "segment_first","%ld", (long)segment_first_row,
                    NULL
                );
                return -1;
            }
            cur_rowid = segment_first_row;  // over a hole, if any
        }

        *rowid = cur_rowid;

    } else {
        /*
         *  Decrement rowid
         */
        cur_rowid--;
        if(cur_rowid <= 0) {
            // No more
            return -1;
        }

        /*
         *  Check if is in the same segment, if not then go to the previous segment
         */
        json_int_t segment_first_row = json_integer_value(json_object_get(segment, "first_row"));
        if(cur_rowid < segment_first_row) {
            // Go to the previous segment
            cur_segment--;
            if(cur_segment < 0) {
                // No more
                return -1;
            }
            segment = json_array_get(segments, cur_segment);

            json_int_t segment_last_row = json_integer_value(json_object_get(segment, "last_row"));
            if(cur_rowid < segment_last_row) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "previous segment ends after the row just read",
                    "cur_rowid",    "%ld", (long)cur_rowid,
                    "segment_last", "%ld", (long)segment_last_row,
                    NULL
                );
                return -1;
            }
            cur_rowid = segment_last_row;   // over a hole, if any
        }

        *rowid = cur_rowid;
    }

    return cur_segment;
}


/*
 *
 *  TODO To review below functions, perhaps not needed
 *
 */

/***************************************************************************
 *
 ***************************************************************************/
//PRIVATE json_int_t segments_first_row(json_t *segments)
//{
//    if (json_array_size(segments) == 0) {
//        return 0;
//    }
//    json_int_t cur_segment = 0;
//    json_t *segment = json_array_get(segments, cur_segment);
//    return json_integer_value(json_object_get(segment, "first_row"));
//}

/***************************************************************************
 *
 ***************************************************************************/
//PRIVATE json_int_t segments_last_row(json_t *segments)
//{
//    if (json_array_size(segments) == 0) {
//        return 0;
//    }
//    json_int_t cur_segment = (json_int_t)json_array_size(segments) - 1;
//    json_t *segment = json_array_get(segments, cur_segment);
//    return json_integer_value(json_object_get(segment, "last_row"));
//}

/***************************************************************************
 *
 ***************************************************************************/
//PRIVATE json_int_t segments_first_t(json_t *segments)
//{
//    if (json_array_size(segments) == 0) {
//        return 0;
//    }
//
//    json_int_t cur_segment = 0;
//    json_t *segment = json_array_get(segments, cur_segment);
//    return json_integer_value(json_object_get(segment, "fr_t"));
//}

/***************************************************************************
 *
 ***************************************************************************/
//PRIVATE json_int_t segments_last_t(json_t *segments)
//{
//    if (json_array_size(segments) == 0) {
//        return 0;
//    }
//
//    json_int_t cur_segment = (json_int_t)json_array_size(segments) - 1;
//    json_t *segment = json_array_get(segments, cur_segment);
//    return json_integer_value(json_object_get(segment, "to_t"));
//}

/***************************************************************************
 *  Get first_t
 ***************************************************************************/
//PRIVATE json_int_t segments_first_tm(json_t *segments)
//{
//    if (json_array_size(segments) == 0) {
//        return 0;
//    }
//
//    json_int_t cur_segment = 0;
//    json_t *segment = json_array_get(segments, cur_segment);
//    return json_integer_value(json_object_get(segment, "fr_tm"));
//}

/***************************************************************************
 *
 ***************************************************************************/
//PRIVATE json_int_t segments_last_tm(json_t *segments)
//{
//    if (json_array_size(segments) == 0) {
//        return 0;
//    }
//
//    json_int_t cur_segment = (json_int_t)json_array_size(segments) - 1;
//    json_t *segment = json_array_get(segments, cur_segment);
//    return json_integer_value(json_object_get(segment, "to_tm"));
//}

/***************************************************************************
 *  rkey: the regex a KEYLESS list matches the topic's keys against.
 *
 *  A list opened on one `key` receives that key and nothing else. A list
 *  opened with NO key receives every key of the topic — and `rkey` is what
 *  narrows that: "every key that matches this regex".
 *
 *  The compiled pattern lives IN the list (as a pointer stored in its json,
 *  the same way its load_record_callback does), because it is consulted once
 *  per appended record, on the hot path: compiling it per record would make
 *  a filter cost more than the load it saves. PCRE2 with JIT, like the rest
 *  of the framework (gobj-c's json_replace_vars, c_tranger's list-keys).
 ***************************************************************************/
PRIVATE void *rkey_compile(hgobj gobj, const char *rkey)
{
    int errornumber = 0;
    PCRE2_SIZE erroroffset = 0;

    pcre2_code *re = pcre2_compile(
        (PCRE2_SPTR)rkey, PCRE2_ZERO_TERMINATED, 0, &errornumber, &erroroffset, NULL
    );
    if(!re) {
        /*
         *  Warning, not error: the pattern is CALLER input (it reaches here
         *  straight from a remote client via c_tranger's open-list), and a
         *  malformed peer parameter is not a broken internal invariant.
         *  gobj_log_set_last_message() feeds the refusal the caller answers
         *  with (see cmd_open_list).
         */
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
        gobj_log_set_last_message("Bad rkey regex '%s' at %d: %s",
            rkey, (int)erroroffset, (char *)err
        );
        return NULL;
    }
    pcre2_jit_compile(re, PCRE2_JIT_COMPLETE);
    return re;
}

/***************************************************************************
 *  Arm a list with its rkey (from its match_cond), or leave it unarmed.
 *
 *  Returns 0 when the list is usable: no rkey, or an rkey that COMPILED.
 *  A malformed rkey returns -1 and the caller must refuse to open the list:
 *  degrading it to "every key" would hand the client records it explicitly
 *  asked not to receive, which is worse than failing.
 ***************************************************************************/
PRIVATE int rkey_arm(hgobj gobj, json_t *list, const char *key, json_t *match_cond)
{
    const char *rkey = kw_get_str(gobj, match_cond, "rkey", "", 0);
    if(!empty_string(key) || empty_string(rkey)) {
        return 0;   // a keyed list ignores rkey; no rkey is "every key"
    }

    pcre2_code *re = rkey_compile(gobj, rkey);
    if(!re) {
        return -1;      // Error already logged
    }
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
    if(!md) {
        pcre2_code_free(re);
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "pcre2_match_data_create_from_pattern() FAILED",
            NULL
        );
        return -1;
    }

    json_object_set_new(list, "rkey_re", json_integer((json_int_t)(uintptr_t)re));
    json_object_set_new(list, "rkey_md", json_integer((json_int_t)(uintptr_t)md));
    return 0;
}

PRIVATE void rkey_disarm(json_t *list)
{
    pcre2_code *re = (pcre2_code *)(uintptr_t)json_integer_value(
        json_object_get(list, "rkey_re")
    );
    pcre2_match_data *md = (pcre2_match_data *)(uintptr_t)json_integer_value(
        json_object_get(list, "rkey_md")
    );
    if(md) {
        pcre2_match_data_free(md);
        json_object_del(list, "rkey_md");
    }
    if(re) {
        pcre2_code_free(re);
        json_object_del(list, "rkey_re");
    }
}

/***************************************************************************
 *  Does this list want the records of `key`?
 *
 *  A keyed list: only its key. A keyless list: every key, unless it armed an
 *  rkey, in which case only the keys that match it. The ONE place that
 *  decides, so the disk load, the memory feed and the disk feed cannot drift
 *  apart — a list whose initial load was filtered and whose realtime was not
 *  is the worst of both worlds.
 ***************************************************************************/
PRIVATE BOOL list_wants_key(json_t *list, const char *key)
{
    const char *key_ = json_string_value(json_object_get(list, "key"));
    if(!empty_string(key_)) {
        return strcmp(key_, key?key:"")==0;
    }

    pcre2_code *re = (pcre2_code *)(uintptr_t)json_integer_value(
        json_object_get(list, "rkey_re")
    );
    if(!re) {
        return TRUE;    // keyless and no rkey: every key of the topic
    }
    pcre2_match_data *md = (pcre2_match_data *)(uintptr_t)json_integer_value(
        json_object_get(list, "rkey_md")
    );
    if(!md) {
        return TRUE;    // armed without its match data: cannot happen (rkey_arm)
    }

    int ret = pcre2_match(
        re, (PCRE2_SPTR)(key?key:""), PCRE2_ZERO_TERMINATED, 0, 0, md, NULL
    );
    return ret >= 0;
}

/***************************************************************************
 *  Get key rows (topic key size)
 ***************************************************************************/
PRIVATE json_int_t get_topic_key_rows(hgobj gobj, json_t *topic, const char *key)
{
    // char path[PATH_MAX];
    //
    // // Silence, please
    //
    // if(empty_string(key)) {
    //     return 0;
    // }
    // snprintf(path, sizeof(path), "cache`%s`total`rows", key);
    // return kw_get_int(gobj, topic, path, 0, 0);

    // No big difference using this
    json_t *jn_rows = json_object_get(
        json_object_get(
            json_object_get(
                json_object_get(
                    topic,
                    "cache"
                ),
                key
            ),
            "total"
        ),
        "rows"
    );

    return json_integer_value(jn_rows);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int json_array_find_idx(json_t *jn_list, json_t *item)
{
    int idx;
    json_t *jn_value;
    json_array_foreach(jn_list, idx, jn_value) {
        if(jn_value == item) {
            return idx;
        }
    }
    return -1;
}

/***************************************************************************
 *  Get md record by rowid
 *  A segment represents a file .json / .md2
 *
 *  Requirements:
 *      rowid belongs to the segment
 *  Internally
 *
 ***************************************************************************/
PRIVATE int get_md_by_rowid(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *segment,
    uint64_t rowid, // relative to 1
    md2_record_ex_t *md_record_ex
)
{
    /*
     *  TODO Find in the cache the range md
     */

    /*
     *  Check rowid is in range of segment
     */
    json_int_t first_rowid = json_integer_value(json_object_get(segment, "first_row"));
    json_int_t last_rowid = json_integer_value(json_object_get(segment, "last_row"));
    if(!(rowid >= first_rowid && rowid <= last_rowid)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Bad rowid range in segment",
            "topic",        "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            "rowid",        "%ld", (long)rowid,
            "first",        "%ld", (long)first_rowid,
            "last",         "%ld", (long)last_rowid,
            NULL
        );
        return -1;
    }

    json_int_t relative_rowid = (json_int_t)rowid - first_rowid + 1;
    if(relative_rowid <= 0) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read record metadata, relative_rowid negative",
            "topic",        "%s", tranger2_topic_name(topic),
            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "key",          "%s", key,
            "rowid",        "%ld", (long)rowid,
            "relative_rowid","%ld", (long)relative_rowid,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        gobj_trace_json(gobj, segment,  "Cannot read record metadata, relative_rowid negative");
        return -1;
    }

    /*
     *  Get file handler
     */
    const char *file_id = json_string_value(json_object_get(segment, "id"));
    if(read_md(
        gobj,
        tranger,
        topic,
        key,
        file_id,
        relative_rowid, // relative to 1
        md_record_ex
    )<0) {
        // Error already logged
        return -1;
    }
    md_record_ex->g_rowid = rowid;
    return 0;
}

/***************************************************************************
 *  Get md record by rowid
 *  A segment represents a file .json / .md2
 *
 *  Requirements:
 *      rowid belongs to the segment
 *  Internally
 ***************************************************************************/
PRIVATE int read_md(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id,
    uint64_t rowid, // relative to 1
    md2_record_ex_t *md_record_ex
)
{
    /*
     *  TODO Find in the cache the range md
     */

    /*
     *  Check parameters
     */
    if(rowid <= 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "rowid must be relative to 1",
            "topic",        "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            "file_id",      "%s", file_id,
            "rowid",        "%d", (int)rowid,
            NULL
        );
        return -1;
    }

    md2_record_t md_record;

    /*
     *  Get file handler
     */
    int fd = get_topic_rd_fd(
        gobj,
        tranger,
        topic,
        key,
        file_id,
        FALSE
    );
    if(fd<0) {
        return -1;
    }

    off_t offset = (off_t) ((rowid - 1) * sizeof(md2_record_t));
    off_t offset_ = lseek(fd, offset, SEEK_SET);
    if(offset != offset_) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read record metadata, lseek FAILED",
            "topic",        "%s", tranger2_topic_name(topic),
            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "key",          "%s", key,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    size_t ln = read( // read direct md for segment
        fd,
        &md_record,
        sizeof(md2_record_t)
    );
    if(ln != sizeof(md2_record_t)) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read record metadata, read FAILED",
            "topic",        "%s", tranger2_topic_name(topic),
            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "key",          "%s", key,
            "rowid",        "%ld", (long)rowid,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    md_record.__t__ = ntohll(md_record.__t__);
    md_record.__tm__ = ntohll(md_record.__tm__);
    md_record.__offset__ = ntohll(md_record.__offset__);
    md_record.__size__ = ntohll(md_record.__size__);

    md_record_ex->__t__ = get_time_t(&md_record);
    md_record_ex->__tm__ = get_time_tm(&md_record);
    md_record_ex->__offset__ = md_record.__offset__;
    md_record_ex->__size__ = md_record.__size__;
    md_record_ex->system_flag = get_system_flag(&md_record);
    md_record_ex->user_flag = get_user_flag(&md_record);
    md_record_ex->rowid = rowid;
    return 0;
}

/***************************************************************************
 *  Read record data
 *
 *  Read content, useful when you load only md and want recover the content
 *  Load the (JSON) message pointed by metadata (md_record_ex)
 ***************************************************************************/
PUBLIC json_t *tranger2_read_record_content( // return is yours
    json_t *tranger,
    json_t *topic,
    const char *key,
    md2_record_ex_t *md_record_ex
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "What topic?",
            NULL
        );
        return NULL;
    }

    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
    if(system_flag & sf_rowid_key) {
        key = "__rowid__";
    }

    if(empty_string(key)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "What key?",
            NULL
        );
        return NULL;
    }
    if(!md_record_ex) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "md_record_ex required",
            NULL
        );
        return NULL;
    }

    uint64_t t = md_record_ex->__t__;
    if(md_record_ex->system_flag & sf_t_ms) {
        t /= 1000;
    }

    char file_id[NAME_MAX];
    if(!get_file_id(
        file_id,
        sizeof(file_id),
        tranger,
        topic,
        t
    )) {
        // Error already logged
        return NULL;
    }

    json_t *record = read_record_content(
        tranger,
        topic,
        key,
        file_id,
        md_record_ex
    );
    if(!record) {
        // Error already logged
        return NULL;
    }
    json_object_set_new(record, "__md_tranger__", md2json(md_record_ex));

    return record;
}

/***************************************************************************
 *   Read record data
 ***************************************************************************/
PRIVATE json_t *read_record_content(
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id,
    md2_record_ex_t *md_record_ex
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*
     *  Get file handler
     */
    //const char *file_id = json_string_value(json_object_get(segment, "id"));
    int fd = get_topic_rd_fd(
        gobj,
        tranger,
        topic,
        key,
        file_id,
        TRUE
    );
    if(fd<0) {
        return NULL;
    }

    /*
     *  Harden against corrupt/forged on-disk md2 records: __offset__ and
     *  __size__ come straight off disk (big-endian, byte-swapped) and below
     *  drive lseek/gbmem_malloc/read with no other bound. Validate them
     *  against the actual data-file size before use, so a bad header cannot
     *  oversize the allocation/read or seek out of range. Defense-in-depth
     *  for the case where topic files are authored by a less-trusted writer
     *  (e.g. an rt_by_disk follower); harmless for trusted single-writer use.
     */
    struct stat content_st;
    if(fstat(fd, &content_st) < 0) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read record data, fstat FAILED",
            "topic",        "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return NULL;
    }
    if(md_record_ex->__size__ == 0 ||
            md_record_ex->__offset__ > (uint64_t)content_st.st_size ||
            md_record_ex->__size__ > (uint64_t)content_st.st_size - md_record_ex->__offset__) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Bad on-disk record: __offset__/__size__ out of range",
            "topic",        "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            "__offset__",   "%lu", (unsigned long)md_record_ex->__offset__,
            "__size__",     "%lu", (unsigned long)md_record_ex->__size__,
            "filesize",     "%lu", (unsigned long)content_st.st_size,
            NULL
        );
        return NULL;
    }

    off_t offset = (off_t)md_record_ex->__offset__;
    off_t offset_ = lseek(fd, offset, SEEK_SET);
    if(offset != offset_) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read record data, lseek FAILED",
            "topic",        "%s", tranger2_topic_name(topic),
            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "key",          "%s", key,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return NULL;
    }

    char *p = gbmem_malloc(md_record_ex->__size__);
    if(!p) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "Cannot read record data. NO Memory",
            "topic",        "%s", tranger2_topic_name(topic),
            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "size",         "%d", (int)md_record_ex->__size__,
            NULL
        );
        return NULL;
    }
    size_t ln = read( // read content
        fd,
        p,
        md_record_ex->__size__
    );

    if(ln != md_record_ex->__size__) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read record data, read FAILED",
            "topic",        "%s", tranger2_topic_name(topic),
            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "key",          "%s", key,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        gbmem_free(p);
        return NULL;
    }

    /*
     *  Restoring: first decrypt, second decompress
     */

// TODO   if(system_flag & sf_cipher_record) {
//        // if(topic->decrypt_callback) {
//        //     gbuf = topic->decrypt_callback(
//        //         topic->user_data,
//        //         topic,
//        //         gbuf    // must be owned
//        //     );
//        // }
//    }
//    if(system_flag & sf_zip_record) {
//        // if(topic->decompress_callback) {
//        //     gbuf = topic->decompress_callback(
//        //         topic->user_data,
//        //         topic,
//        //         gbuf    // must be owned
//        //     );
//        // }
//    }


    json_t *record;
    if(empty_string(p)) {
        record = json_object();
    } else {
        // strnlen, not strlen: p is exactly __size__ bytes and a forged/corrupt
        // record need not be NUL-terminated, so bound the scan to the buffer.
        record = anystring2json(p, strnlen(p, md_record_ex->__size__), FALSE);
    }

    gbmem_free(p);

    if(!record) {
        gobj_log_critical(gobj, 0, // Let continue, will be a message lost
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Bad data, anystring2json() FAILED.",
            "topic",        "%s", tranger2_topic_name(topic),
            "__t__",        "%lu", (unsigned long)md_record_ex->__t__,
            "__size__",     "%lu", (unsigned long)md_record_ex->__size__,
            "__offset__",   "%lu", (unsigned long)md_record_ex->__offset__,
            NULL
        );
        return NULL;
    }

    return record;
}

/***************************************************************************
 *
    Open a list: load the matching records from disk (calling load_record_callback
    on each), and, when the requested range is open-ended, keep the list live by
    also opening a realtime feed on top. High-level wrapper over
    tranger2_open_iterator() + tranger2_open_rt_mem()/tranger2_open_rt_disk().

    WARNING: loads every matching record into memory up front (delays start-up on
    large topics). Close the returned handle with tranger2_close_list().

    match_cond (second level, consumed here):
        key                 (str) exact key to load; if absent, rkey is used
        rkey                (str) regex (PCRE2) over the keys ("" == every key).
                            It governs BOTH halves of the list — the disk load
                            and the realtime feed — because a list whose initial
                            load is filtered and whose live records are not is
                            worse than no filter at all: the caller cannot even
                            tell that it is being lied to.
                            A malformed rkey REFUSES the list (returns NULL);
                            degrading it to "every key" would push the caller
                            exactly the records it asked not to receive.
        to_rowid            (int) upper bound of the range; see "realtime" below
        load_record_callback (tranger2_load_record_callback_t) REQUIRED
                            passed inside match_cond, not as a C argument.
                            Called on both LOADING (disk) and APPENDING (realtime).

    For the first-level match_cond see `Iterator match_cond` in timeranger2.h

    Realtime vs no_rt:
        - to_rowid == 0 (open-ended): open a realtime feed and return its handle.
            rt_by_disk TRUE  -> rt_disk
            rt_by_disk FALSE -> rt_mem (master only)
        - to_rowid != 0: one-shot load, no realtime; return `extra` tagged
          {"list_type": "no_rt"} (extra is REQUIRED in this case).

    Return of load_record_callback:
        0 do nothing (the callback may build its own list, or not),
        -1 break the load

    A key whose history cannot be loaded whole: the list of ONE key is
    refused (NULL). A keyless list logs the key, goes on with the others and
    opens its feed; the handle it returns says `"load_failed": true` and
    names the keys in `"load_failed_keys"`. The records of a failed key
    read BEFORE the failure WERE handed to the callback: in a forward load
    its oldest rows, in a backward one its newest. A key fails when a row's
    metadata or content cannot be read, and when the cache build flagged
    it (a md2 file it could not count, see load_key_cache_from_disk) --
    the same after a restart as behind a running tranger's back.

    Return: realtime handle (rt_mem / rt_disk) or the no_rt `extra`, NULL on error.
    Both match_cond and extra are owned (consumed).

 ***************************************************************************/
PUBLIC json_t *tranger2_open_list( // WARNING loading all records causes delay in starting applications
    json_t *tranger,
    const char *topic_name,
    json_t *match_cond, // owned
    json_t *extra,      // owned
    const char *rt_id,
    BOOL rt_by_disk,
    const char *creator
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *topic = tranger2_topic(tranger, topic_name); // HACK will open the topic if not yet.
    if(!topic) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "topic not found",
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return NULL;
    }

    tranger2_load_record_callback_t load_record_callback =
        (tranger2_load_record_callback_t)(size_t)json_integer_value(
            json_object_get(match_cond, "load_record_callback")
        );
    if(!load_record_callback) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "load_record_callback is required to tranger2_open_list",
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return NULL;
    }

    BOOL realtime = FALSE;
    json_int_t to_rowid = kw_get_int(gobj, match_cond, "to_rowid", 0, KW_WILD_NUMBER);
    if(to_rowid == 0) {
        realtime = TRUE;
    }

    /*
     *  The history is loaded with one-shot iterators, closed at once. Their
     *  creator is reserved: with the caller's default ("" and the key as the
     *  id) an iterator the caller keeps open on the key made this one
     *  "already exist", and the load did not happen.
     */
    const char *load_creator = "__tranger2_open_list__";

    json_t *load_failed_keys = NULL;    // keys of a keyless list whose history did not load

    const char *key = kw_get_str(gobj, match_cond, "key", "", 0);
    if(!empty_string(key)) {
        json_t *ll = tranger2_open_iterator(
            tranger,
            topic_name,
            key,
            json_incref(match_cond),  // match_cond, owned
            load_record_callback, // called on LOADING and APPENDING
            "",     // iterator id: the key
            load_creator,
            NULL,   // to store LOADING data, not owned
            json_incref(extra) // extra, owned
        );
        /*
         *  The list IS the history of its key: one that could not be
         *  loaded is not a list. It was closed with the error swallowed,
         *  and the caller took the half it got for the whole.
         */
        BOOL load_failed = (!ll || json_is_true(json_object_get(ll, "load_failed")))? TRUE: FALSE;
        if(ll) {
            tranger2_close_iterator(tranger, ll);
        }
        if(load_failed) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TRANGER,
                "msg",          "%s", "Cannot load the history of the list's key",
                "topic_name",   "%s", topic_name,
                "key",          "%s", key,
                NULL
            );
            JSON_DECREF(match_cond)
            JSON_DECREF(extra)
            return NULL;
        }

    } else {
        /*
         *  No key: the list is the WHOLE topic — every key of it, unless `rkey`
         *  narrows it to the keys that match. This is where rkey was documented
         *  and never applied: the load visited every key and the caller was
         *  handed records of keys it had explicitly filtered out.
         *
         *  A malformed rkey refuses the list. Loading everything instead would
         *  be the same lie in a louder voice.
         */
        const char *rkey = kw_get_str(gobj, match_cond, "rkey", "", 0);
        pcre2_code *re = NULL;
        pcre2_match_data *md = NULL;
        if(!empty_string(rkey)) {
            re = rkey_compile(gobj, rkey);
            if(!re) {
                JSON_DECREF(match_cond)     // Error already logged
                JSON_DECREF(extra)
                return NULL;
            }
            md = pcre2_match_data_create_from_pattern(re, NULL);
            if(!md) {
                pcre2_code_free(re);
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MEMORY,
                    "msg",          "%s", "pcre2_match_data_create_from_pattern() FAILED",
                    NULL
                );
                JSON_DECREF(match_cond)
                JSON_DECREF(extra)
                return NULL;
            }
        }

        /*
         *  Every key's history, as much of it as can be read. A key that
         *  cannot be loaded is logged and NAMED in the list
         *  (`load_failed_keys`), and the load goes on with the next key:
         *  the list is handed over with every readable key loaded and its
         *  realtime feed open.
         *
         *  It used to be closed with its load_failed unread, and the caller
         *  took what it got for the whole topic (M2 of the 2026-09-23
         *  independent review: the snapshot guard of the assets read "held
         *  by nothing"). Refusing the whole list instead (6d5760377) was
         *  worse: the keys after the bad one were not loaded and no feed was
         *  opened, so a treedb topic came up with 1 node of 6 and a replica
         *  stopped following it. A caller that must not act on a partial
         *  history reads `load_failed`.
         */
        json_t *failed_keys = json_array();
        json_t *jn_keys = tranger2_list_keys(tranger, topic_name);
        if(json_array_size(jn_keys)>0) {
            /*
             *  Load from disk
             */
            int idx; json_t *jn_key;
            json_array_foreach(jn_keys, idx, jn_key) {
                const char *key_ = json_string_value(jn_key);
                if(!key_) {
                    key_ = "";
                }
                if(re) {
                    int m = pcre2_match(
                        re, (PCRE2_SPTR)key_, PCRE2_ZERO_TERMINATED, 0, 0, md, NULL
                    );
                    if(m < 0) {
                        continue;   // this key is not in the list
                    }
                }

                json_t *ll = tranger2_open_iterator(
                    tranger,
                    topic_name,
                    key_,
                    json_incref(match_cond),  // match_cond, owned
                    load_record_callback, // called on LOADING and APPENDING
                    "",     // iterator id: the key
                    load_creator,
                    NULL,   // to store LOADING data, not owned
                    json_incref(extra) // extra, owned
                );
                BOOL load_failed = (!ll || json_is_true(json_object_get(ll, "load_failed")))?
                    TRUE: FALSE;
                if(ll) {
                    tranger2_close_iterator(tranger, ll);
                }
                if(load_failed) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TRANGER,
                        "msg",          "%s", "Cannot load the whole history of a key of the list: the records read before the failure were handed, the list goes on with the next key",
                        "topic_name",   "%s", topic_name,
                        "key",          "%s", key_,
                        NULL
                    );
                    json_array_append_new(failed_keys, json_string(key_));
                }
            }
        }
        json_decref(jn_keys);

        if(re) {
            pcre2_match_data_free(md);
            pcre2_code_free(re);
        }
        if(json_array_size(failed_keys) > 0) {
            load_failed_keys = failed_keys;
        } else {
            JSON_DECREF(failed_keys)
        }
    }

    /*-------------------------------*
     *  Open realtime for list
     *-------------------------------*/
    if(realtime) {
        json_t *rt = NULL;
        if(rt_by_disk) {
            rt = tranger2_open_rt_disk(
                tranger,
                topic_name,
                key,                    // if empty receives all keys, else only this key
                json_incref(match_cond),
                load_record_callback,   // called on append new record
                rt_id,
                creator,
                json_incref(extra)    // extra, owned
            );
        } else {
            rt = tranger2_open_rt_mem(
                tranger,
                topic_name,
                key,                    // if empty receives all keys, else only this key
                json_incref(match_cond),
                load_record_callback,   // called on append new record
                rt_id,
                creator,
                json_incref(extra)    // extra, owned
            );
        }

        if(!rt) {
            /*
             *  Error already logged, with its cause: an error with a stack
             *  here said "internal" of what is often a peer's bad rt id.
             */
            JSON_DECREF(load_failed_keys)
            JSON_DECREF(match_cond)
            JSON_DECREF(extra)
            return NULL;
        }
        if(load_failed_keys) {
            json_object_set_new(rt, "load_failed", json_true());
            json_object_set_new(rt, "load_failed_keys", load_failed_keys);
        }

        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return rt;

    } else {
        if(!extra) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "list with no-realtime require extra",
                "topic_name",   "%s", topic_name,
                NULL
            );
            JSON_DECREF(load_failed_keys)
            JSON_DECREF(match_cond)
            JSON_DECREF(extra)
            return NULL;
        }
        json_object_set_new(extra, "list_type", json_string("no_rt"));
        if(load_failed_keys) {
            json_object_set_new(extra, "load_failed", json_true());
            json_object_set_new(extra, "load_failed_keys", load_failed_keys);
        }

        JSON_DECREF(match_cond)

        return extra;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int tranger2_close_list(
    json_t *tranger,
    json_t *list
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    const char *list_type = kw_get_str(gobj, list, "list_type", "", KW_REQUIRED);

    if(strcmp(list_type, "rt_mem")==0) {
        return tranger2_close_rt_mem(tranger, list);
    } else if(strcmp(list_type, "rt_disk")==0) {
        return tranger2_close_rt_disk(tranger, list);
    } else if(strcmp(list_type, "no_rt")==0) {
        JSON_DECREF(list)
        return 0;
    }
    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "list not found",
        NULL
    );
    return -1;
}

/***************************************************************************
 *  Close all or rt_id lists belongs to creator (rt_mem or rt_disk)
 ***************************************************************************/
PUBLIC int tranger2_close_all_lists(
    json_t *tranger,
    const char *topic_name,
    const char *creator,    // if empty, remove all
    const char *rt_id       // if empty, remove all lists of creator
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(empty_string(topic_name)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "What topic name?",
            NULL
        );
        return -1;
    }

    json_t *topic = json_object_get(json_object_get(tranger, "topics"), topic_name);
    if(!topic) {
        return 0;
    }

    if(!creator) {
        creator = "";
    }
    if(!rt_id) {
        rt_id = "";
    }

    json_t *list_to_remove = json_array();
    int idx; json_t *rt;

    /*-------------------------------------*
     *      Get iterators to remove
     *-------------------------------------*/
    json_t *iterators = kw_get_list(gobj, topic, "iterators", 0, KW_REQUIRED);

    json_array_foreach(iterators, idx, rt) {
        const char *creator_ = kw_get_str(gobj, rt, "creator", "", KW_REQUIRED);
        if(empty_string(creator)) {
            json_array_append(list_to_remove, rt);
        } else {
            if(strcmp(creator, creator_)==0) {
                if(empty_string(rt_id)) {
                    json_array_append(list_to_remove, rt);
                } else {
                    const char *rt_id_ = kw_get_str(gobj, rt, "id", "", KW_REQUIRED);
                    if(strcmp(rt_id, rt_id_)==0) {
                        json_array_append(list_to_remove, rt);
                    }
                }
            }
        }
    }
    /*
     *  Remove iterators
     */
    json_array_foreach(list_to_remove, idx, rt) {
        tranger2_close_iterator(tranger, rt);
    }
    /*
     *  Clear list, prepare to the next
     */
    json_array_clear(list_to_remove);

    /*-------------------------------------*
     *      Get mem lists
     *-------------------------------------*/
    json_t *lists = kw_get_list(gobj, topic, "lists", 0, KW_REQUIRED);

    json_array_foreach(lists, idx, rt) {
        const char *creator_ = kw_get_str(gobj, rt, "creator", "", KW_REQUIRED);
        if(empty_string(creator)) {
            json_array_append(list_to_remove, rt);
        } else {
            if(strcmp(creator, creator_)==0) {
                if(empty_string(rt_id)) {
                    json_array_append(list_to_remove, rt);
                } else {
                    const char *rt_id_ = kw_get_str(gobj, rt, "id", "", KW_REQUIRED);
                    if(strcmp(rt_id, rt_id_)==0) {
                        json_array_append(list_to_remove, rt);
                    }
                }
            }
        }
    }
    /*
     *  Remove mem lists
     */
    json_array_foreach(list_to_remove, idx, rt) {
        tranger2_close_rt_mem(tranger, rt);
    }
    /*
     *  Clear list, prepare to the next
     */
    json_array_clear(list_to_remove);

    /*-------------------------------------*
     *      Get mem lists
     *-------------------------------------*/
    json_t *disks = kw_get_list(gobj, topic, "disks", 0, KW_REQUIRED);

    json_array_foreach(disks, idx, rt) {
        const char *creator_ = kw_get_str(gobj, rt, "creator", "", KW_REQUIRED);
        if(empty_string(creator)) {
            json_array_append(list_to_remove, rt);
        } else {
            if(strcmp(creator, creator_)==0) {
                if(empty_string(rt_id)) {
                    json_array_append(list_to_remove, rt);
                } else {
                    const char *rt_id_ = kw_get_str(gobj, rt, "id", "", KW_REQUIRED);
                    if(strcmp(rt_id, rt_id_)==0) {
                        json_array_append(list_to_remove, rt);
                    }
                }
            }
        }
    }
    /*
     *  Remove mem lists
     */
    json_array_foreach(list_to_remove, idx, rt) {
        tranger2_close_rt_disk(tranger, rt);
    }
    /*
     *  Clear list, prepare to the next
     */
    json_array_clear(list_to_remove);

    json_decref(list_to_remove);
    return 0;
}

/***************************************************************************
 *  Print rowid, t, tm, key
 ***************************************************************************/
PUBLIC void tranger2_print_md0_record(
    char *bf,
    int bfsize,
    const char *key,
    json_int_t rowid,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
)
{
    char fecha[90];
    char fecha_tm[90];

    system_flag2_t system_flag = md_record_ex->system_flag;

    time_t t = (time_t)md_record_ex->__t__;
    if(system_flag & sf_t_ms) {
        t /= 1000;
    }
    if(print_local_time) {
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%S%z", localtime(&t));
    } else {
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%S%z", gmtime(&t));
    }

    time_t t_m = (time_t)md_record_ex->__tm__;
    if(system_flag & sf_tm_ms) {
        t_m /= 1000;
    }
    if(print_local_time) {
        strftime(fecha_tm, sizeof(fecha_tm), "%Y-%m-%dT%H:%M:%S%z", localtime(&t_m));
    } else {
        strftime(fecha_tm, sizeof(fecha_tm), "%Y-%m-%dT%H:%M:%S%z", gmtime(&t_m));
    }

    system_flag2_t key_type = system_flag & KEY_TYPE_MASK2;

    if(key_type & (sf_int_key|sf_string_key|sf_rowid_key)) {
        snprintf(bf, bfsize,
            "rowid(g,i):%"JSON_INTEGER_FORMAT", %"PRIu64", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key: %s",
            rowid,
            (uint64_t)md_record_ex->rowid,
            (uint64_t)md_record_ex->__t__,
            fecha,
            (uint64_t)md_record_ex->__tm__,
            fecha_tm,
            key
        );
    } else {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "BAD metadata, without key type",
            "key",          "%s", key,
            NULL
        );
    }
}

/***************************************************************************
 *  Print rowid, uflag, sflag, t, tm, key
 ***************************************************************************/
PUBLIC void tranger2_print_md1_record(
    char *bf,
    int bfsize,
    const char *key,
    json_int_t rowid,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
)
{
    char fecha[90];
    char fecha_tm[90];

    system_flag2_t system_flag = md_record_ex->system_flag;
    unsigned user_flag = md_record_ex->user_flag;

    time_t t = (time_t)md_record_ex->__t__;
    if(system_flag & sf_t_ms) {
        t /= 1000;
    }
    if(print_local_time) {
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%S%z", localtime(&t));
    } else {
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%S%z", gmtime(&t));
    }

    time_t t_m = (time_t)md_record_ex->__tm__;
    if(system_flag & sf_tm_ms) {
        t_m /= 1000;
    }
    if(print_local_time) {
        strftime(fecha_tm, sizeof(fecha_tm), "%Y-%m-%dT%H:%M:%S%z", localtime(&t_m));
    } else {
        strftime(fecha_tm, sizeof(fecha_tm), "%Y-%m-%dT%H:%M:%S%z", gmtime(&t_m));
    }

    system_flag2_t key_type = system_flag & KEY_TYPE_MASK2;

    if(key_type & (sf_int_key|sf_string_key|sf_rowid_key)) {
        snprintf(bf, bfsize,
            "rowid(g,i):%"JSON_INTEGER_FORMAT", %"PRIu64", "
            "uflag:0x%"PRIX32", sflag:0x%"PRIX32", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key: %s",
            rowid,
            (uint64_t)md_record_ex->rowid,
            user_flag,
            system_flag,
            (uint64_t)md_record_ex->__t__,
            fecha,
            (uint64_t)md_record_ex->__tm__,
            fecha_tm,
            key
        );
    } else {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "BAD metadata, without key type",
            "key",          "%s", key,
            NULL
        );
    }
}

/***************************************************************************
 *  print rowid, offset, size, t, path
 ***************************************************************************/
PUBLIC void tranger2_print_md2_record(
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_int_t rowid,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
)
{
    system_flag2_t system_flag = md_record_ex->system_flag;

    time_t t = (time_t)md_record_ex->__t__;
    uint64_t offset = md_record_ex->__offset__;
    uint64_t size = md_record_ex->__size__;

    char filename[NAME_MAX*2];
    if(!get_t_filename(
        filename,
        sizeof(filename),
        tranger,
        topic,
        TRUE,   // TRUE for data, FALSE for md2
        (system_flag & sf_t_ms)? t/1000:t  // WARNING must be in seconds!
    )) {
        // Error already logged: the line says so instead of naming no file
        snprintf(filename, sizeof(filename), "%s", "?");
    }

    const char *topic_dir = kw_get_str(0, topic, "directory", "", KW_REQUIRED);

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/keys/%s/%s", topic_dir, key, filename);

    snprintf(bf, bfsize,
        "rowid(g,i):%"JSON_INTEGER_FORMAT", %"PRIu64", "
        "ofs:%7"PRIu64", sz:%7"PRIu64", "
        "t:%"PRIu64", "
        "f:%s",
        rowid,
        (uint64_t)md_record_ex->rowid,
        offset,
        size,
        (uint64_t)t,
        path
    );
}

/***************************************************************************
 *  Print path
 ***************************************************************************/
PUBLIC void tranger2_print_record_filename(
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
)
{
    system_flag2_t system_flag = md_record_ex->system_flag;

    time_t t = (time_t)md_record_ex->__t__;
    /*  A failure is logged there, and leaves bf empty  */
    get_t_filename(
        bf,
        bfsize,
        tranger,
        topic,
        TRUE,   // TRUE for data, FALSE for md2
        (system_flag & sf_t_ms)? t/1000:t  // WARNING must be in seconds!
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void tranger2_set_trace_level(
    json_t *tranger,
    int trace_level
)
{
    json_object_set_new(tranger, "trace_level", json_integer(trace_level));
}
