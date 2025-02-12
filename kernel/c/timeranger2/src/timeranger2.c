/***********************************************************************
 *          TIMERANGER2.C
 *
 *          Time Ranger 2, a series time-key-value database over flat files
 *
 *          Copyright (c) 2017-2018 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>

#include <ansi_escape_codes.h>
#include <helpers.h>
#include <kwid.h>
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

    0
};

PRIVATE const char *sf_names[16+1] = {
    "sf_string_key",            // 0x0001
    "",                         // 0x0002
    "sf_int_key",               // 0x0004
    "",                         // 0x0008
    "sf2_zip_record",           // 0x0010
    "sf2_cipher_record",        // 0x0020
    "",                         // 0x0040
    "",                         // 0x0080
    "sf_t_ms",                  // 0x0100
    "sf_tm_ms",                 // 0x0200
    "sf_deleted_record",        // 0x0400
    "",                         // 0x0800

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

typedef struct { // Size: 96 bytes
    uint64_t __t__;
    uint64_t __tm__;

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


/***************************************************************
 *              Prototypes
 ***************************************************************/
typedef gbuffer_t * (*filter_callback_t) (   // Remember to free returned gbuffer
    void *user_data,  // topic user_data
    json_t *tranger,
    json_t *topic,
    gbuffer_t * gbuf  // must be owned
);

PRIVATE int get_topic_rd_fd(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id,
    BOOL for_data
);
PRIVATE int get_topic_wr_fd( // optimized
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    BOOL for_data,
    uint64_t __t__
);
PRIVATE char *get_t_filename(
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    BOOL for_data,  // TRUE for data, FALSE for md2
    uint64_t __t__ // WARNING must be in seconds!
);
PRIVATE int get_md_record_for_wr(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    uint64_t rowid,
    md2_record_t *md_record,
    BOOL verbose
);

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

PRIVATE int json_array_find_idx(
    json_t *jn_list,
    json_t *item
);

PRIVATE json_t *find_keys_in_disk(
    hgobj gobj,
    const char *directory,
    const char *rkey
);
PRIVATE int build_topic_cache_from_disk(
    hgobj gobj,
    json_t *tranger,
    json_t *topic
);
PRIVATE json_t *get_key_cache(
    json_t *topic,
    const char *key
);
PRIVATE json_t *create_cache_key(void);
PRIVATE json_t *get_last_cache_cell(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id
);
PRIVATE json_t *load_key_cache_from_disk(
    hgobj gobj,
    const char *directory,
    const char *key
);
PRIVATE json_t *load_cache_cell_from_disk(
    hgobj gobj,
    const char *directory,
    const char *key,
    char *filename  // md2 filename with extension, WARNING modified, .md2 removed
);
PRIVATE uint64_t load_first_and_last_record_md(
    hgobj gobj,
    const char *directory,
    const char *key,
    const char *filename,
    md2_record_t *md_first_record,
    md2_record_t *md_last_record
);

PRIVATE int update_new_record_from_mem(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key_value,
    md2_record_t *md_record
);
PRIVATE int update_totals_of_key_cache(
    hgobj gobj,
    json_t *topic,
    const char *key
);

PRIVATE json_int_t get_topic_key_rows(hgobj gobj, json_t *topic, const char *key);

PRIVATE json_t *get_cache_files(json_t *topic, const char *key);
PRIVATE json_t *get_cache_total(json_t *topic, const char *key);

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
    uint64_t rowid,
    md2_record_ex_t *md_record_ex
);
PRIVATE int read_md(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id,
    uint64_t rowid,
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
    json_int_t *rowid
);
PRIVATE json_int_t next_segment_row(
    json_t *segments,
    json_t *match_cond,  // not owned
    json_int_t cur_segment,
    json_int_t *rowid
);
PRIVATE BOOL tranger2_match_metadata(
    json_t *match_cond,
    json_int_t total_rows,
    json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    BOOL *end
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
PRIVATE int update_new_records_from_disk(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    char *filename
);
PRIVATE int publish_new_rt_disk_records(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *old_file_cache,
    json_t *new_file_cache
);

PRIVATE int update_key_by_hard_link(
    hgobj gobj,
    json_t *tranger,
    char *path
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
    json_object_set_new(tranger, "gobj", json_integer((json_int_t)(size_t)gobj));
    JSON_DECREF(jn_tranger)

    char path[PATH_MAX];
    if(1) {
        const char *path_ = kw_get_str(gobj, tranger, "path", "", KW_REQUIRED);
        if(empty_string(path_)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                    "msgset",       "%s", MSGSET_TRANGER_ERROR,
                    "msg",          "%s", "Open as not master, __timeranger2__.json locked",
                    "path",         "%s", directory,
                    NULL
                );
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TRANGER_ERROR,
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                "msgset",       "%s", MSGSET_TRANGER_ERROR,
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
    kw_set_dict_value(gobj, tranger, "yev_loop", json_integer((json_int_t)(size_t)yev_loop));

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

    json_t *opened_files = kw_get_dict(gobj, tranger, "fd_opened_files", 0, KW_REQUIRED);
    json_object_foreach(opened_files, key, jn_value) {
        int fd = (int)kw_get_int(gobj, opened_files, key, 0, KW_REQUIRED);
        if(fd >= 0) {
            close(fd);
        }
    }
    json_object_set_new(tranger, "__closed__", json_true());

    return 0;
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
        int idx = idx_in_list(sf_names, *(names +i), TRUE);
        if(idx > 0) {
            bitmask |= 1 << (idx-1);
        }
    }

    split_free2(names);

    return bitmask;
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
    BOOL master = json_boolean_value(json_object_get(tranger, "master"));

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
    if(empty_string(topic_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "database",     "%s", kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
            "msg",          "%s", "tranger_create_topic(): What topic name?",
            NULL
        );
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "directory",    "%s", directory,
                "msg",          "%s", "Cannot open TimeRanger topic. Not found and no master",
                NULL
            );
            JSON_DECREF(jn_cols)
            JSON_DECREF(jn_var)
            JSON_DECREF(jn_topic_ext)
            return 0;
        }

        if(empty_string(pkey)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create TimeRanger subdir. mkrdir() FAILED",
                "errno",        "%s", strerror(errno),
                NULL
            );
        }

        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Creating topic",
            "path",         "%s", directory,
            "topic",        "%s", topic_name,
            NULL
        );

        /*----------------------------------------*
         *      Create topic_desc.json
         *----------------------------------------*/
        json_t *jn_topic_desc = json_object();
        kw_get_str(gobj, jn_topic_desc, "topic_name", topic_name, KW_CREATE);
        kw_get_str(gobj, jn_topic_desc, "pkey", pkey, KW_CREATE);
        kw_get_str(gobj, jn_topic_desc, "tkey", tkey, KW_CREATE);

        system_flag2_t system_flag_key_type = system_flag & KEY_TYPE_MASK2;
        if(!system_flag_key_type) {
            if(!empty_string(pkey)) {
                system_flag |= sf_string_key;
            } else {
                system_flag |= sf_int_key;
            }
        }
        kw_get_int(gobj, jn_topic_desc, "system_flag", system_flag, KW_CREATE);

        if(jn_topic_ext) {
            json_t *jn_topic_ext_ = create_json_record(gobj, topic_json_desc); // no master by default
            json_object_update_existing(jn_topic_ext_, jn_topic_ext);
            json_object_update(jn_topic_desc, jn_topic_ext_);
            JSON_DECREF(jn_topic_ext_)
        }

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

        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Creating topic_desc.json",
            "path",         "%s", directory,
            "topic",        "%s", topic_name,
            NULL
        );

        /*----------------------------------------*
         *      Create topic_cols.json
         *----------------------------------------*/
        JSON_INCREF(jn_cols)
        tranger2_write_topic_cols(
            tranger,
            topic_name,
            jn_cols
        );
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Creating topic_cols.json",
            "path",         "%s", directory,
            "topic",        "%s", topic_name,
            NULL
        );

        /*----------------------------------------*
         *      Create topic_var.json
         *----------------------------------------*/
        JSON_INCREF(jn_var)
        tranger2_write_topic_var(
            tranger,
            topic_name,
            jn_var
        );
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Creating topic_var.json",
            "path",         "%s", directory,
            "topic",        "%s", topic_name,
            NULL
        );

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
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create TimeRanger subdir. mkrdir() FAILED",
                "errno",        "%s", strerror(errno),
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
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create TimeRanger subdir. mkrdir() FAILED",
                "errno",        "%s", strerror(errno),
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
            JSON_DECREF(topic_var)
            if(topic_new_version > topic_old_version) {
                file_remove(directory, "topic_cols.json");
                file_remove(directory, "topic_var.json");
            }
        }

        if(!file_exists(directory, "topic_var.json")) {
            /*----------------------------------------*
             *      Create topic_var.json
             *----------------------------------------*/
            JSON_INCREF(jn_var)
            tranger2_write_topic_var(
                tranger,
                topic_name,
                jn_var
            );
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Re-Creating topic_var.json",
                "database",     "%s", kw_get_str(gobj, tranger, "database", "", KW_REQUIRED),
                "topic",        "%s", topic_name,
                NULL
            );
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
    if(empty_string(topic_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "database",     "%s", kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
            "msg",          "%s", "tranger_open_topic(): What topic name?",
            NULL
        );
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "tranger_open_topic(): directory not found",
                "directory",    "%s", kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
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
        kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
        0,
        FALSE, // exclusive
        FALSE // silence
    );

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

    /*-------------------------------------*
     *  Load keys and metadata from disk
     *-------------------------------------*/
    build_topic_cache_from_disk(gobj, tranger, topic);

    /*
     *  Monitoring the disk to realtime disk lists
     */
    yev_loop_h yev_loop = (yev_loop_h)kw_get_int(gobj, tranger, "yev_loop", 0, KW_REQUIRED);
    if(yev_loop) {
        BOOL master = json_boolean_value(json_object_get(tranger, "master"));
        if(master) {
            // (1) MONITOR (MI) /disks/
            // Master to monitor the (topic) directory where clients will mark their rt disks.

            if(gobj_trace_level(gobj) & TRACE_FS) {
                gobj_log_debug(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_YEV_LOOP,
                    "msg",              "%s", "MASTER:  MONITOR INOTIFY (MI) /disks/",
                    "msg2",             "%s", "👓🔷 MASTER:  MONITOR INOTIFY (MI) /disks/",
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
                json_integer((json_int_t)(size_t)fs_event_master)
            );
        }
    }

    return topic;
}

/***************************************************************************
   Get topic by his topic_name.
   Topic is opened if it's not opened.
   HACK topic can exists in disk, but it's not opened until tranger_open_topic()
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
   Return list of keys of the topic
    match_cond:
        key
        rkey    regular expression of key
 ***************************************************************************/
PUBLIC json_t *tranger2_list_keys( // return is yours
    json_t *tranger,
    const char *topic_name,
    const char *rkey
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    json_t *topic = tranger2_topic( // WARNING returned json IS NOT YOURS
        tranger,
        topic_name
    );

    const char *directory = kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED);

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/keys",
        directory
    );

    json_t *jn_keys = find_keys_in_disk(gobj, full_path, rkey);
    return jn_keys;
}

/***************************************************************************
   Get topic size (number of records of all keys)
 ***************************************************************************/
PUBLIC uint64_t tranger2_topic_size(
    json_t *tranger,
    const char *topic_name
) {
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        return 0;
    }

    json_t *jn_keys = tranger2_list_keys( // return is yours
        tranger,
        topic_name,
        NULL
    );

    uint64_t total = 0;
    int idx; json_t *jn_key;
    json_array_foreach(jn_keys, idx, jn_key) {
        const char *key = json_string_value(jn_key);
        total += get_topic_key_rows(gobj, topic, key);
    }
    json_decref(jn_keys);

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
        return tranger2_topic_size(gobj, topic_name);

    } else {
        return get_topic_key_rows(gobj, topic, key);
    }
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
        if(gobj_trace_level(gobj) & TRACE_FS) {
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

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Topic not found",
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }

    /*
     *  Get directory
     */
    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%s/%s",
        kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
        topic_name
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
   If overwrite_backup is true and backup exists then it will be overwrited.
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
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "cannot backup topic",
            "errno",        "%s", strerror(errno),
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "jn_topic_var EMPTY",
            NULL
        );
        return -1;
    }
    if(!json_is_object(jn_topic_var)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "jn_topic_var is NOT DICT",
            NULL
        );
        JSON_DECREF(jn_topic_var)
        return -1;
    }

    BOOL master = json_boolean_value(json_object_get(tranger, "master"));
    if(!master) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Only master can write",
            NULL
        );
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

    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, 0);
    if(topic) {
        kw_update_except(gobj, topic, topic_var, topic_fields); // data from topic disk are inmutable!
    }

    save_json_to_file(
        gobj,
        directory,
        "topic_var.json",
        (int)kw_get_int(gobj, tranger, "xpermission", 0, KW_REQUIRED),
        (int)kw_get_int(gobj, tranger, "rpermission", 0, KW_REQUIRED),
        0,
        master? TRUE:FALSE, //create
        FALSE,  //only_read
        topic_var  // owned
    );

    return 0;
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "jn_topic_cols EMPTY",
            NULL
        );
        return -1;
    }
    if(!json_is_object(jn_topic_cols) && !json_is_array(jn_topic_cols)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "jn_topic_cols MUST BE dict or list",
            NULL
        );
        JSON_DECREF(jn_topic_cols)
        return -1;
    }

    BOOL master = json_boolean_value(json_object_get(tranger, "master"));
    if(!master) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Only master can write",
            NULL
        );
        JSON_DECREF(jn_topic_cols)
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

    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, 0);
    if(topic) {
        json_object_set(
            topic,
            "cols",
            jn_topic_cols
        );
    }

    save_json_to_file(
        gobj,
        directory,
        "topic_cols.json",
        (int)kw_get_int(gobj, tranger, "xpermission", 0, KW_REQUIRED),
        (int)kw_get_int(gobj, tranger, "rpermission", 0, KW_REQUIRED),
        0,
        master? TRUE:FALSE, //create
        FALSE,  //only_read
        jn_topic_cols  // owned
    );

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
    const char *fields[] = {
        "topic_name",
        "pkey",
        "tkey",
        "system_flag",
        "topic_version",
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gmtime() FAILED",
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return NULL;
    }

    const char *filename_mask = json_string_value(json_object_get(topic, "filename_mask"));
    if(empty_string(filename_mask)) {
        filename_mask = json_string_value(json_object_get(tranger, "filename_mask"));
    }

    strftime(bf, bfsize, filename_mask, tm);
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
    get_file_id(filename, sizeof(filename), tranger, topic, __t__);

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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "Cannot create subdir. mkrdir() FAILED",
                    "errno",        "%s", strerror(errno),
                    NULL
                );
            }
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
                "msgset",               "%s", MSGSET_INTERNAL_ERROR,
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
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
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
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
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
    uint64_t __t__
)
{
    char full_path[PATH_MAX];
    char relative_path[NAME_MAX*2];
    char filename[NAME_MAX];

    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));

    /*-----------------------------*
     *      Check file
     *-----------------------------*/
    get_t_filename(
        filename,
        sizeof(filename),
        tranger,
        topic,
        for_data,
        (system_flag & sf_t_ms)? __t__/1000:__t__
    );

    /*-----------------------------*
     *      Open content file
     *-----------------------------*/
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
            fd = open(full_path, O_RDWR|O_LARGEFILE|O_NOFOLLOW, 0);
            if(fd < 0) {
                if(errno == EMFILE) {
                    struct rlimit rl = {0};
                    getrlimit(RLIMIT_NOFILE, &rl);

                    gobj_log_error(gobj, 0,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_INTERNAL_ERROR,
                        "path",                 "%s", full_path,
                        "msg",                  "%s", "TOO MANY OPEN FILES 2",
                        "current soft limit",   "%d", rl.rlim_cur,
                        "current hard limit",   "%d", rl.rlim_max,
                        NULL
                    );

                    close_fd_wr_files(gobj, topic, "");
                    fd = open(full_path, O_RDWR|O_LARGEFILE|O_NOFOLLOW, 0);
                }
                if(fd < 0) {
                    fd = create_file(gobj, tranger, topic, key, full_path);
                }
            }
        } else {
            fd = open(full_path, O_RDONLY|O_LARGEFILE, 0);
        }

        if(fd<0) {
            gobj_log_critical(gobj,
                master?kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED):0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot open file to write",
                "path",         "%s", full_path,
                "errno",        "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        snprintf(relative_path, sizeof(relative_path), "%s`%s", key, filename);
        kw_set_dict_value(
            gobj,
            kw_get_dict(
                gobj,
                topic,
                "wr_fd_files",
                0,
                KW_REQUIRED
            ),
            relative_path,
            json_integer(fd)
        );
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
        fd = open(full_path, O_RDONLY|O_LARGEFILE, 0);
        if(fd<0) {
            gobj_log_critical(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot open file to read",
                "path",         "%s", full_path,
                "errno",        "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        snprintf(relative_path, sizeof(relative_path), "%s`%s", key, filename);
        kw_set_dict_value(
            gobj,
            kw_get_dict(
                gobj,
                topic,
                "rd_fd_files",
                0,
                KW_REQUIRED
            ),
            relative_path,
            json_integer(fd)
        );
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
    json_t *fd_files = kw_get_dict(gobj, topic, "wr_fd_files", 0, KW_REQUIRED);
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
    md2_record_ex_t *md_record_ex,
    json_int_t rowid
)
{
    json_t *jn_md = json_object();
    json_object_set_new(jn_md, "rowid", json_integer(rowid));
    json_object_set_new(jn_md, "t", json_integer((json_int_t)md_record_ex->__t__));
    json_object_set_new(jn_md, "tm", json_integer((json_int_t)md_record_ex->__tm__));
    json_object_set_new(jn_md, "offset", json_integer((json_int_t)md_record_ex->__offset__));
    json_object_set_new(jn_md, "size", json_integer((json_int_t)md_record_ex->__size__));
    json_object_set_new(jn_md, "system_flag", json_integer(md_record_ex->system_flag));
    json_object_set_new(jn_md, "user_flag", json_integer(md_record_ex->user_flag));

    return jn_md;
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
    md2_record_ex_t *md_record_ex, // required
    json_t *jn_record       // owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(!jn_record || jn_record->refcount <= 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "jn_record NULL",
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }

    // TEST performance 800.000

    BOOL master = json_boolean_value(json_object_get(tranger, "master"));
    if(!master) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot append record, NO master",
            "topic",        "%s", topic_name,
            NULL
        );
        gobj_trace_json(gobj, jn_record, "Cannot append record, NO master");
        JSON_DECREF(jn_record)
        return -1;
    }

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Cannot append record, topic not found",
            "topic",        "%s", topic_name,
            NULL
        );
        gobj_trace_json(gobj, jn_record, "Cannot append record, topic not found");
        JSON_DECREF(jn_record)
        return -1;
    }

    /*--------------------------------------------*
     *  If time not specified, use the now time
     *--------------------------------------------*/
    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
    if(!__t__) {
        if(system_flag & (sf_t_ms)) {
            __t__ = time_in_miliseconds();
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
    //const char *pkey = kw_get_str(gobj, topic, "pkey", "", KW_REQUIRED);
    const char *pkey = json_string_value(json_object_get(topic, "pkey"));
    system_flag2_t system_flag_key_type = system_flag & KEY_TYPE_MASK2;

    const char *key_value = NULL;
    char key_int[NAME_MAX+1];

    switch(system_flag_key_type) {
        case sf_string_key:
            {
                //key_value = kw_get_str(gobj, jn_record, pkey, 0, 0);
                key_value = json_string_value(json_object_get(jn_record, pkey));
                if(empty_string(key_value)) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_JSON_ERROR,
                        "msg",          "%s", "Cannot append record, no pkey",
                        "topic",        "%s", topic_name,
                        "pkey",         "%s", pkey,
                        NULL
                    );
                    gobj_trace_json(gobj, jn_record, "Cannot append record, no pkey");
                    JSON_DECREF(jn_record)
                    return -1;
                }
                if(strlen(key_value) > NAME_MAX) {
                    // Key will be a directory name, cannot be greater than NAME_MAX
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "Cannot append record, pkey too long",
                        "topic",        "%s", topic_name,
                        "key_value",    "%s", pkey,
                        "size",         "%d", strlen(key_value),
                        "maxsize",      "%d", NAME_MAX,
                        NULL
                    );
                    gobj_trace_json(gobj, jn_record, "Cannot append record, pkey too long");
                    JSON_DECREF(jn_record)
                    return -1;
                }
            }
            break;

        case sf_int_key:
            {
                //uint64_t i = (uint64_t)kw_get_int(
                //    gobj,
                //    jn_record,
                //    pkey,
                //    0,
                //    KW_REQUIRED|KW_WILD_NUMBER
                //);
                uint64_t i = json_integer_value(json_object_get(jn_record, pkey));
                snprintf(key_int, sizeof(key_int), "%0*"PRIu64, 19, i);
                key_value = key_int;
            }
            break;

        default:
            // No pkey
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_JSON_ERROR,
                "msg",          "%s", "Cannot append record, no pkey type",
                "topic",        "%s", topic_name,
                "pkey",         "%s", pkey,
                NULL
            );
            gobj_trace_json(gobj, jn_record, "Cannot append record, no pkey type");
            JSON_DECREF(jn_record)
            return -1;
    }

    // TEST performance 630000

    /*--------------------------------------------*
     *  Get and save the t-key if exists
     *--------------------------------------------*/
    //const char *tkey = kw_get_str(gobj, topic, "tkey", "", KW_REQUIRED);
    const char *tkey = json_string_value(json_object_get(topic, "tkey"));
    if(!empty_string(tkey)) {
        //json_t *jn_tval = kw_get_dict_value(gobj, jn_record, tkey, 0, 0);
        json_t *jn_tval = json_object_get(jn_record, tkey);
        if(!jn_tval) {
            md_record.__tm__ = 0; // No tkey value, mark with 0
        } else {
            if(json_is_string(jn_tval)) {
                timestamp_t timestamp;
                timestamp = approxidate(json_string_value(jn_tval));
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
     *  Save content, to file
     *------------------------------------------------------*/
    int content_fp = get_topic_wr_fd(gobj, tranger, topic, key_value, TRUE, __t__);

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
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot append record, lseek() FAILED",
                "topic",        "%s", topic_name,
                "errno",        "%s", strerror(errno),
                NULL
            );
            gobj_trace_json(gobj, jn_record, "Cannot append record, lseek() FAILED");
            JSON_DECREF(jn_record)
            return -1;
        }
        md_record.__offset__ = __offset__;

        /*--------------------------------------------*
         *  Get the record's content, always json
         *--------------------------------------------*/
        char *srecord = json_dumps(jn_record, JSON_COMPACT|JSON_ENCODE_ANY);
        if(!srecord) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_JSON_ERROR,
                "msg",          "%s", "Cannot append record, json_dumps() FAILED",
                "topic",        "%s", topic_name,
                NULL
            );
            gobj_trace_json(gobj, jn_record, "Cannot append record, json_dumps() FAILED");
            JSON_DECREF(jn_record)
            return -1;
        }
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
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot append record, write FAILED",
                "topic",        "%s", topic_name,
                "errno",        "%s", strerror(errno),
                NULL
            );
            gobj_trace_json(gobj, jn_record, "Cannot append record, write FAILED");
            JSON_DECREF(jn_record)
            jsonp_free(srecord);
            return -1;
        }

        jsonp_free(srecord);
    }

    // TEST performance 170000

    /*--------------------------------------------*
     *  Write record metadata
     *--------------------------------------------*/
    set_user_flag(&md_record, user_flag);
    set_system_flag(&md_record, system_flag & ~NOT_INHERITED_MASK);

    json_int_t relative_rowid = 0;
    int md2_fd = get_topic_wr_fd(gobj, tranger, topic, key_value, FALSE, __t__);
    if(md2_fd >= 0) {
        off_t offset = lseek(md2_fd, 0, SEEK_END);
        if(offset < 0) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot append record, lseek() FAILED",
                "topic",        "%s", topic_name,
                "errno",        "%s", strerror(errno),
                NULL
            );
            gobj_trace_json(gobj, jn_record, "Cannot append record, lseek() FAILED");
            JSON_DECREF(jn_record)
            return -1;
        }

        relative_rowid = (json_int_t)(offset/sizeof(md2_record_t)) + 1;

        /*--------------------------------------------*
         *  write md2 in big endian
         *--------------------------------------------*/
        md2_record_t big_endian;
        big_endian.__t__ = htonll(md_record.__t__);
        big_endian.__tm__ = htonll(md_record.__tm__);
        big_endian.__offset__ = htonll(md_record.__offset__);
        big_endian.__size__ = htonll(md_record.__size__);

        size_t ln = write(
            md2_fd,
            &big_endian,
            sizeof(md2_record_t)
        );
        if(ln != sizeof(md2_record_t)) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot save record metadata, write FAILED",
                "topic",        "%s", tranger2_topic_name(topic),
                "errno",        "%s", strerror(errno),
                NULL
            );
            JSON_DECREF(jn_record)
            return -1;
        }

        /*
         *  Update cache
         */
        update_new_record_from_mem(gobj, tranger, topic, key_value, &md_record);
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
    md_record_ex->rowid = relative_rowid;

    json_t *__md_tranger__ = md2json(md_record_ex, relative_rowid);
    json_object_set_new(
        jn_record,
        "__md_tranger__",
        __md_tranger__  // owned
    );

    /*--------------------------------------------*
     *      FEED the lists
     *      Call callbacks of realtime lists
     *--------------------------------------------*/
    json_t *lists = json_object_get(topic, "lists");
    int idx;
    json_t *list;
    json_array_foreach(lists, idx, list) {
        const char *key_ = json_string_value(json_object_get(list, "key"));
        if(empty_string(key_) || strcmp(key_, key_value)==0) {
            tranger2_load_record_callback_t load_record_callback =
                (tranger2_load_record_callback_t)(size_t)json_integer_value(
                    json_object_get(list, "load_record_callback")
                );

            if(load_record_callback) {
                // Inform to the user list: record real time from memory
                load_record_callback(
                    tranger,
                    topic,
                    key_value,
                    list,
                    relative_rowid,
                    md_record_ex,
                    json_incref(jn_record)
                );
            }
        }
    }

    JSON_DECREF(jn_record)
    return 0;
}

/***************************************************************************
    Get md record by rowid (by fd, for write)
 ***************************************************************************/
PRIVATE int get_md_record_for_wr(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    uint64_t rowid,
    md2_record_t *md_record,
    BOOL verbose
)
{
// TODO
//    memset(md_record, 0, sizeof(md2_record_t));
//
//    if(rowid == 0) {
//        if(verbose) {
//            gobj_log_error(gobj, 0,
//                "function",     "%s", __FUNCTION__,
//                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//                "msg",          "%s", "rowid 0",
//                "topic",        "%s", tranger2_topic_name(topic),
//                "rowid",        "%lu", (unsigned long)rowid,
//                NULL
//            );
//        }
//        return -1;
//    }
//
//    json_int_t __last_rowid__ = get_topic_key_rows(gobj, topic, key);   es last_rowid
//    json_int_t __last_rowid__ = kw_get_int(gobj, topic, "__last_rowid__", 0, KW_REQUIRED);
//    if(__last_rowid__ <= 0) {
//        return -1;
//    }
//
//    if(rowid > __last_rowid__) {
//        if(verbose) {
//            gobj_log_error(gobj, 0,
//                "function",     "%s", __FUNCTION__,
//                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//                "msg",          "%s", "rowid greater than last_rowid",
//                "topic",        "%s", tranger2_topic_name(topic),
//                "rowid",        "%lu", (unsigned long)rowid,
//                "last_rowid",   "%lu", (unsigned long)__last_rowid__,
//                NULL
//            );
//        }
//        return -1;
//    }
//
//    int fd = get_idx_fd(tranger, topic);
//    if(fd < 0) {
//        // Error already logged
//        return -1;
//    }
//
//    off_t offset = (off_t) ((rowid-1) * sizeof(md2_record_t));
//    off_t offset_ = lseek(fd, offset, SEEK_SET);
//    if(offset != offset_) {
//        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//            "msg",          "%s", "topic_idx.md corrupted",
//            "topic",        "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
//            "offset",       "%lu", (unsigned long)offset,
//            "offset_",      "%lu", (unsigned long)offset_,
//            NULL
//        );
//        return -1;
//    }
//
//    size_t ln = read(
//        fd,
//        md_record,
//        sizeof(md2_record_t)
//    );
//    if(ln != sizeof(md2_record_t)) {
//        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
//            "msg",          "%s", "Cannot read record metadata, read FAILED",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "errno",        "%s", strerror(errno),
//            NULL
//        );
//        return -1;
//    }
//
////   if(rowid != rowid) {
////        log_error(0,
////            "function",     "%s", __FUNCTION__,
////            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
////            "msg",          "%s", "md_record corrupted, item rowid not match",
////            "topic",        "%s", tranger2_topic_name(topic),
////            "rowid",        "%lu", (unsigned long)rowid,
////            "__rowid__",    "%lu", (unsigned long)rowid,
////            NULL
////        );
////        return -1;
////    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "NOT IMPLEMENTED",
        NULL
    );
    return 0;
}

/***************************************************************************
   Re-Write new record metadata to file
 ***************************************************************************/
PRIVATE int rewrite_md_record_to_file(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record
)
{
//    int fd = get_idx_fd(tranger, topic);
//    if(fd < 0) {
//        // Error already logged
//        return -1;
//    }
////    off_t offset = (off_t) ((rowid-1) * sizeof(md2_record_t));
////    off_t offset_ = lseek(fd, offset, SEEK_SET);
////    if(offset != offset_) {
////        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
////            "function",     "%s", __FUNCTION__,
////            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
////            "msg",          "%s", "topic_idx.md corrupted",
////            "topic",        "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
////            "offset",       "%lu", (unsigned long)offset,
////            "offset_",      "%lu", (unsigned long)offset_,
////            NULL
////        );
////        return -1;
////    }
//
//    size_t ln = write( // write new (record content)
//        fd,
//        md_record,
//        sizeof(md2_record_t)
//    );
//    if(ln != sizeof(md2_record_t)) {
//        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
//            "msg",          "%s", "Cannot save record metadata, write FAILED",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "errno",        "%s", strerror(errno),
//            NULL
//        );
//        return -1;
//    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
        "msg",          "%s", "NOT IMPLEMENTED",
        NULL
    );
    return 0;
}

/***************************************************************************
    Delete record
 ***************************************************************************/
PUBLIC int tranger2_delete_record(
    json_t *tranger,
    const char *topic_name,
    const char *key
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    BOOL master = json_boolean_value(json_object_get(tranger, "master"));

    /*----------------------------------------*
     *  Delete key only if master
     *----------------------------------------*/
    if(!master) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_topic() failed",
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }

    const char *topic_dir = json_string_value(json_object_get(topic, "directory"));

    char path_key[PATH_MAX];
    snprintf(path_key, sizeof(path_key), "%s/keys/%s",
        topic_dir,
        key
    );
    if(is_directory(path_key)) {
        if(rmrdir(path_key)<0) {
            gobj_log_critical(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "path",         "%s", path_key,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot delete subdir key. rmrdir() FAILED",
                "errno",        "%s", strerror(errno),
                NULL
            );
        }
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "path",         "%s", path_key,
            "msg",          "%s", "key directory not found",
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
    Write record user flag
 ***************************************************************************/
PUBLIC int tranger2_write_user_flag(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid,
    uint32_t user_flag
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot open topic",
            "topic",        "%s", topic_name,
            "errno",        "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    md2_record_t md_record;
    if(get_md_record_for_wr(
        gobj,
        tranger,
        topic,
        rowid,
        &md_record,
        TRUE
    )!=0) {
        return -1;
    }

    set_user_flag(&md_record, user_flag);

    if(rewrite_md_record_to_file(gobj, tranger, topic, &md_record)<0) {
        return -1;
    }
    return 0;
}

/***************************************************************************
    Write record user flag using mask
 ***************************************************************************/
PUBLIC int tranger2_set_user_flag(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid,
    uint32_t mask,
    BOOL set
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot open topic",
            "topic",        "%s", topic_name,
            "errno",        "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    md2_record_t md_record;
    if(get_md_record_for_wr(
        gobj,
        tranger,
        topic,
        rowid,
        &md_record,
        TRUE
    )!=0) {
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

    if(rewrite_md_record_to_file(gobj, tranger, topic, &md_record)<0) {
        return -1;
    }

    return 0;
}

/***************************************************************************
    Read record user flag (for writing mode)
 ***************************************************************************/
PUBLIC uint16_t tranger2_read_user_flag(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot open topic",
            "topic",        "%s", topic_name,
            "errno",        "%s", strerror(errno),
            NULL
        );
        return 0;
    }

    md2_record_t md_record;
    if(get_md_record_for_wr(
        gobj,
        tranger,
        topic,
        rowid,
        &md_record,
        TRUE
    )!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Tranger record NOT FOUND",
            "topic",        "%s", topic_name,
            "rowid",        "%lu", (unsigned long)rowid,
            NULL
        );
        return 0;
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
        "load_record_callback", (json_int_t)(size_t)load_record_callback
    ));
    json_object_set_new(mem, "list_type", json_string("rt_mem"));
    json_object_update_missing_new(mem, extra);

    /*
     *  Add the mem to the topic
     */
    json_array_append_new(
        kw_get_dict_value(gobj, topic, "lists", 0, KW_REQUIRED),
        mem
    );

    if(gobj_trace_level(gobj) & TRACE_FS) {
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_close_rt_mem(): mem NULL",
            NULL
        );
        return -1;
    }

    const char *id = kw_get_str(gobj, mem, "id", "", 0);
    const char *topic_name = kw_get_str(gobj, mem, "topic_name", "", KW_REQUIRED);

    if(gobj_trace_level(gobj) & TRACE_FS) {
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_close_rt_mem(): lists not found",
            NULL
        );
        return -1;
    }

    int idx = json_array_find_idx(lists, mem);
    if(idx >=0 && idx < json_array_size(lists)) {
        json_array_remove(
            lists,
            idx
        );
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_get_rt_mem_by_id: what id?",
            NULL
        );
        return 0;
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
                if(empty_string(creator) && empty_string(creator)) {
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
    const char *id,         // disk id, optional
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "what load_record_callback?",
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return NULL;
    }

    if(empty_string(id)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "what id?",
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return NULL;
    }

    json_t *disk = json_object();

    if(tranger2_get_rt_disk_by_id(tranger, topic_name, id, creator)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Disk already exists",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(disk)
        JSON_DECREF(extra)
        return NULL;
    }

    json_object_update_new(disk, json_pack("{s:s, s:s, s:s, s:s, s:o, s:I}",
        "id", id,
        "creator", creator,
        "topic_name", topic_name,
        "key", key,
        "match_cond", match_cond,
        "load_record_callback", (json_int_t)(size_t)load_record_callback
    ));
    json_object_update_missing_new(disk, extra);
    json_object_set_new(disk, "list_type", json_string("rt_disk"));

    /*
     *  Add the list to the topic
     */
    json_array_append_new(
        kw_get_dict_value(gobj, topic, "disks", 0, KW_REQUIRED),
        disk
    );

    if(gobj_trace_level(gobj) & TRACE_FS) {
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
                json_integer((json_int_t)(size_t)fs_event_client)
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_close_rt_disk(): disk NULL",
            NULL
        );
        return -1;
    }

    const char *id = kw_get_str(gobj, disk, "id", "", 0);
    const char *topic_name = kw_get_str(gobj, disk, "topic_name", "", KW_REQUIRED);

    if(gobj_trace_level(gobj) & TRACE_FS) {
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

    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, KW_REQUIRED);

    yev_loop_h yev_loop = (yev_loop_h)kw_get_int(gobj, tranger, "yev_loop", 0, KW_REQUIRED);
    if(yev_loop) {
        // MONITOR Client Unwatching (MI) topic /disks/rt_id/
        if(gobj_trace_level(gobj) & TRACE_FS) {
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

        if(gobj_trace_level(gobj) & TRACE_FS) {
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_get_rt_disk_by_id: what id?",
            NULL
        );
        return 0;
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
                if(empty_string(creator) && empty_string(creator)) {
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        0           // user_data2
    );
    if(!fs_event) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "fs_create_watcher_event() FAILED",
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
    snprintf(full_path, sizeof(full_path), "%s/%s", fs_event->directory, fs_event->filename);

    switch(fs_event->fs_type) {
        case FS_SUBDIR_CREATED_TYPE:
            {
                // (3) MONITOR Client has opened a rt disk for the topic,
                // Master to open a mem rt to update /disks/rt_id/

                char full_path2[PATH_MAX];
                snprintf(full_path2, sizeof(full_path2), "%s/%s", fs_event->directory, fs_event->filename);

                char *rt_id = pop_last_segment(full_path);
                char *disks = pop_last_segment(full_path);
                char *topic_name = pop_last_segment(full_path);

                if(gobj_trace_level(gobj) & TRACE_FS) {
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
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
                snprintf(full_path2, sizeof(full_path2), "%s/%s", fs_event->directory, fs_event->filename);

                char *rt_id = pop_last_segment(full_path);
                char *disks = pop_last_segment(full_path);
                char *topic_name = pop_last_segment(full_path);

                if(gobj_trace_level(gobj) & TRACE_FS) {
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
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "FS_FILE_CREATED_TYPE master fs_event NOT processed",
                NULL
            );
            break;
        case FS_FILE_DELETED_TYPE:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "FS_FILE_DELETED_TYPE master fs_event NOT processed",
                NULL
            );
            break;
        case FS_FILE_MODIFIED_TYPE:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "FS_FILE_MODIFIED_TYPE master fs_event NOT processed",
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
     *  Create the directory for the key
     */
    snprintf(full_path_dest, sizeof(full_path_dest), "%s/%s", disk_path, key);
    if(!is_directory(full_path_dest)) {
        if(gobj_trace_level(gobj) & TRACE_FS) {
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
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "mkdir() FAILED",
                "path",         "%s", full_path_dest,
                "errno",        "%s", strerror(errno),
                NULL
            );
        }
    }

    /*
     *  Create the hard link for the md2 file
     */
    char filename[NAME_MAX];
    system_flag2_t system_flag = md_record_ex->system_flag;

    get_t_filename(
        filename,
        sizeof(filename),
        tranger,
        topic,
        FALSE,
        (system_flag & sf_t_ms)? md_record_ex->__t__/1000 : md_record_ex->__t__
    );
    snprintf(full_path_dest, sizeof(full_path_dest), "%s/%s/%s", disk_path, key, filename);

    const char *topic_dir = json_string_value(json_object_get(topic, "directory"));
    snprintf(full_path_orig, sizeof(full_path_orig), "%s/keys/%s/%s", topic_dir, key, filename);

    if(!is_regular_file(full_path_dest)) {
        if(gobj_trace_level(gobj) & TRACE_FS) {
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
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "link() FAILED",
                "src",          "%s", full_path_orig,
                "dst",          "%s", full_path_dest,
                "errno",        "%s", strerror(errno),
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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

    if(gobj_trace_level(gobj) & TRACE_FS) {
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
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "mkdir() FAILED",
            "path",         "%s", full_path,
            "errno",        "%s", strerror(errno),
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
        0 // TODO key, only must watch the key?
    );
    if(!fs_event) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "fs_create_watcher_event() FAILED",
            NULL
        );
        return NULL;
    }
    fs_start_watcher_event(fs_event);
    return fs_event;
}

/***************************************************************************
 *  CLIENT: The MASTER signalize a new record appended
 ***************************************************************************/
PRIVATE int client_fs_callback(fs_event_t *fs_event)
{
    hgobj gobj = fs_event->gobj;
    json_t *tranger = fs_event->user_data;
    // TODO char *key = fs_event->user_data2;

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", fs_event->directory, fs_event->filename);

    switch(fs_event->fs_type) {
        case FS_SUBDIR_CREATED_TYPE:
            // (5) MONITOR notify of update directory /disks/rt_id/ on new records
            // Key directory created, ignore

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
                if(gobj_trace_level(gobj) & TRACE_FS) {
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
                scan_disks_key_for_new_file(gobj, tranger, full_path);
            }
            break;

        case FS_SUBDIR_DELETED_TYPE:
            // Key directory deleted, ignore, it's me
            // TODO this indicate that a key has been removed, do something
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "FS_SUBDIR_DELETED_TYPE client fs_event NOT processed",
                NULL
            );
            break;

        case FS_FILE_CREATED_TYPE:
            // (5) MONITOR notify of update directory /disks/rt_id/ on new records
            // Record to key added, read
            // Delete the hard link of md2 file when read
            {
                if(gobj_trace_level(gobj) & TRACE_FS) {
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
            // Key file deleted, ignore, it's me
            if(gobj_trace_level(gobj) & TRACE_FS) {
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
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "FS_FILE_MODIFIED_TYPE client fs_event NOT processed",
                NULL
            );
            break;

    }

    return 0;
}

/***************************************************************************
 *  CLIENT:
 ***************************************************************************/
PRIVATE BOOL find_rt_disk_keys_cb(
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
    json_t *tranger = user_data;

    update_key_by_hard_link(gobj, tranger, full_path); // full_path modified */

    return TRUE; // to continue
}
PRIVATE int scan_disks_key_for_new_file(
    hgobj gobj,
    json_t *tranger,
    char *path
)
{
    walk_dir_tree(
        0,
        path,
        0,
        WD_MATCH_REGULAR_FILE,
        find_rt_disk_keys_cb,
        tranger
    );
    return 0;
}

/***************************************************************************
 *  CLIENT:
 ***************************************************************************/
PRIVATE int update_key_by_hard_link(
    hgobj gobj,
    json_t *tranger,
    char *path
)
{
    if(gobj_trace_level(gobj) & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_YEV_LOOP,
            "msg",              "%s", "CLIENT: unlink hard key",
            "path",             "%s", path,
            NULL
        );
    }
    if(unlink(path)<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "unlink() FAILED",
            "path",         "%s", path,
            "errno",        "%s", strerror(errno),
            NULL
        );
    }

    char *md2 = pop_last_segment(path);
    char *key = pop_last_segment(path);
    char *rt_id = pop_last_segment(path);
    char *disks = pop_last_segment(path);
    char *topic_name = pop_last_segment(path);
    if(strcmp(disks, "disks")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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

    if(gobj_trace_level(gobj) & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_YEV_LOOP,
            "msg",              "%s", "CLIENT: update_new_records_from_disk",
            "topic_name",       "%s", topic_name,
            "disks",            "%s", disks,
            "rt_id",            "%s", rt_id,
            "key",              "%s", key,
            "md2",              "%s", md2,
            NULL
        );
    }

    json_t *topic = tranger2_topic(tranger,topic_name);
    update_new_records_from_disk(
        gobj,
        tranger,
        topic,
        key,
        md2
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

 ***************************************************************************/
PRIVATE int update_new_records_from_disk(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    char *filename
)
{
    const char *directory = json_string_value(json_object_get(topic, "directory"));
    json_t *new_cache_cell = load_cache_cell_from_disk(
        gobj,
        directory,
        key,
        filename  // warning .md2 removed
    );

    char *file_id = filename; // Now it has not .md2

    json_t *cur_cache_cell = get_last_cache_cell(
        gobj,
        tranger,
        topic,
        key,
        file_id
    );

    // Publish new data to iterator
    publish_new_rt_disk_records(gobj, tranger, topic, key, cur_cache_cell, new_cache_cell);

    /*
     *  UPDATE CACHE from disk
     */
    if(!cur_cache_cell) {
        json_t *key_cache = get_key_cache(topic, key);
        json_t *cache_files = json_object_get(key_cache, "files");
        json_array_append_new(cache_files, new_cache_cell);
    } else {
        json_object_update_new(cur_cache_cell, new_cache_cell);
    }
    update_totals_of_key_cache(gobj, topic, key);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int publish_new_rt_disk_records(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *old_cache_cell,
    json_t *new_cache_cell
)
{
    BOOL master = json_boolean_value(json_object_get(tranger, "master"));
    json_t *disks = json_object_get(topic, "disks");

    json_int_t from_rowid = json_integer_value(json_object_get(old_cache_cell, "rows"));
    json_int_t to_rowid = json_integer_value(json_object_get(new_cache_cell, "rows"));
    const char *file_id = json_string_value(json_object_get(new_cache_cell, "id"));

    for(json_int_t rowid=from_rowid; rowid<to_rowid; rowid++) {
        md2_record_ex_t md_record_ex;
        read_md(
            gobj,
            tranger,
            topic,
            key,
            file_id,
            rowid,
            &md_record_ex
        );
        json_t *record = read_record_content(
            tranger,
            topic,
            key,
            file_id,
            &md_record_ex
        );

        if(record) {
            json_t *__md_tranger__ = md2json(&md_record_ex, rowid);
            json_object_set_new(
                record,
                "__md_tranger__",
                __md_tranger__  // owned
            );
        }

        /*----------------------------*
         *      FEED the lists
         *----------------------------*/
        int idx; json_t *disk;
        json_array_foreach(disks, idx, disk) {
            const char *key_ = json_string_value(json_object_get(disk, "key"));
            if(empty_string(key_) || strcmp(key_, key)==0) {
                tranger2_load_record_callback_t load_record_callback =
                    (tranger2_load_record_callback_t)(size_t)json_integer_value(
                        json_object_get(disk, "load_record_callback")
                    );

                if(load_record_callback) {
                    // Inform to the user list: record realtime from disk
                    load_record_callback(
                        tranger,
                        topic,
                        key,
                        disk,
                        rowid,
                        &md_record_ex,
                        json_incref(record)
                    );
                }
            }
        }

        if(!master) {
            json_t *lists = json_object_get(topic, "lists");
            json_t *list;
            json_array_foreach(lists, idx, list) {
                const char *key_ = json_string_value(json_object_get(list, "key"));
                if(empty_string(key_) || strcmp(key_, key)==0) {
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
                            rowid,
                            &md_record_ex,
                            json_incref(record)
                        );
                    }
                }
            }
        }

        JSON_DECREF(record)
    }

    return 0;
}

/***************************************************************************
 *  Returns list of searched keys that exist on disk
 ***************************************************************************/
PRIVATE json_t *find_keys_in_disk(
    hgobj gobj,
    const char *directory,
    const char *rkey
)
{
    json_t *jn_keys = json_array();

    const char *pattern;
    if(!empty_string(rkey)) {
        pattern = rkey;
    } else {
        pattern = ".*";
    }

    int dirs_size;
    char **dirs = get_ordered_filename_array(
        gobj,
        directory,
        pattern,
        WD_MATCH_DIRECTORY|WD_ONLY_NAMES,
        &dirs_size
    );

    for(int i=0; i<dirs_size; i++) {
        json_array_append_new(jn_keys, json_string(dirs[i]));
    }
    free_ordered_filename_array(dirs, dirs_size);

    return jn_keys;
}

/***************************************************************************
 *  Load metadata of topic in cache:
 *      keys with its range of time available
 ***************************************************************************/
PRIVATE int build_topic_cache_from_disk(
    hgobj gobj,
    json_t *tranger,
    json_t *topic
) {
    const char *directory = json_string_value(json_object_get(topic, "directory"));
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/keys",
        directory
    );

    json_t *jn_keys = find_keys_in_disk(gobj, full_path, NULL);
    json_t *topic_cache = kw_get_dict(gobj, topic, "cache", 0, 0);
    int idx; json_t *jn_key;
    json_array_foreach(jn_keys, idx, jn_key) {
        const char *key = json_string_value(jn_key);
        json_t *key_cache = load_key_cache_from_disk(gobj, directory, key);
        json_object_set_new(topic_cache, key, key_cache);
        update_totals_of_key_cache(gobj, topic, key);
    }

    JSON_DECREF(jn_keys)
    return 0;
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
 *  WARNING Find only in the last item of the array
 *  Create the tree ("cache`%s`files", key) if not exist
 ***************************************************************************/
PRIVATE json_t *get_last_cache_cell(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id
)
{
    // "cache`%s`files", key
    json_t *key_cache = get_key_cache(topic, key);
    json_t *cache_files = json_object_get(key_cache, "files");
    if(!cache_files) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "no cache files",
            "topic",        "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            NULL
        );
        return NULL;
    }

    if (json_array_size(cache_files) == 0) {
        return NULL;
    }

    /*
     *  WARNING Find only in the last item of the array
     */
    json_int_t cur_file = (json_int_t)json_array_size(cache_files) - 1;
    json_t *cache_cell = json_array_get(cache_files, cur_file);

    const char *file_id_ = json_string_value(json_object_get(cache_cell, "id"));
    if(strcmp(file_id, file_id_)!=0) {
        // Silence
        return NULL;
    }
    return cache_cell;
}

/***************************************************************************
 *  Get range time of a key
 ***************************************************************************/
PRIVATE json_t *load_key_cache_from_disk(hgobj gobj, const char *directory, const char *key)
{
    char full_path[PATH_MAX];

    // NEW KEY in CACHE
    json_t *key_cache = create_cache_key();
    json_t *cache_files = json_object_get(key_cache, "files");

    build_path(full_path, sizeof(full_path), directory, "keys", key, NULL);

    int files_md_size;
    char **files_md = get_ordered_filename_array(
        gobj,
        full_path,
        ".*\\.md2",
        WD_MATCH_REGULAR_FILE|WD_ONLY_NAMES,
        &files_md_size
    );
    for(int i=0; i<files_md_size; i++) {
        char *filename = files_md[i];
        json_t *cache_cell = load_cache_cell_from_disk(
            gobj,
            directory,
            key,
            filename    // warning .md2 removed
        );
        json_array_append_new(cache_files, cache_cell);
    }
    free_ordered_filename_array(files_md, files_md_size);

    return key_cache;
}

/***************************************************************************
 *  Update a cache cell with a new record metadata
 ***************************************************************************/
PRIVATE json_t *update_cache_cell(
    json_t *file_cache,
    const char *file_id,
    md2_record_t *md_record,
    int operation,  // -1 to subtract, 0 to set, +1 to add
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
        rows -= rows_;
    }

    if(md_record->__t__ < file_from_t) {
        file_from_t = md_record->__t__;
    }
    if(md_record->__t__ > file_to_t) {
        file_to_t = md_record->__t__;
    }

    if(md_record->__tm__ < file_from_tm) {
        file_from_tm = md_record->__tm__;
    }
    if(md_record->__tm__ > file_to_tm) {
        file_to_tm = md_record->__tm__;
    }

    json_object_set_new(file_cache, "fr_t", json_integer((json_int_t)file_from_t));
    json_object_set_new(file_cache, "to_t", json_integer((json_int_t)file_to_t));
    json_object_set_new(file_cache, "fr_tm", json_integer((json_int_t)file_from_tm));
    json_object_set_new(file_cache, "to_tm", json_integer((json_int_t)file_to_tm));
    json_object_set_new(file_cache, "rows", json_integer((json_int_t)rows));

    return file_cache;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *load_cache_cell_from_disk(
    hgobj gobj,
    const char *directory,
    const char *key,
    char *filename  // md2 filename with extension, WARNING modified, .md2 removed
)
{
    /*----------------------------------*
     *  Get the first and last record
     *----------------------------------*/
    md2_record_t md_first_record = {0};
    md2_record_t md_last_record = {0};

    uint64_t file_rows = load_first_and_last_record_md(
        gobj,
        directory,
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

    return file_cache;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE uint64_t load_first_and_last_record_md(
    hgobj gobj,
    const char *directory,
    const char *key,
    const char *filename,
    md2_record_t *md_first_record,
    md2_record_t *md_last_record
)
{
    uint64_t file_rows = 0;

    /*----------------------------------*
     *  Open the .md2 file of the key
     *  (name relative to time __t__)
     *----------------------------------*/
    char full_path[PATH_MAX];
    build_path(full_path, sizeof(full_path), directory, "keys", key, filename, NULL);
    int fd = open(full_path, O_RDONLY|O_LARGEFILE, 0);
    if(fd<0) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot open md2 file",
            "path",         "%s", full_path,
            "errno",        "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    /*---------------------------*
     *      Read first record
     *---------------------------*/
    ssize_t ln = read(fd, md_first_record, sizeof(md2_record_t));
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
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot read first record of md2 file",
                "path",         "%s", full_path,
                "errno",        "%s", strerror(errno),
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Cannot read last record, md2 file corrupted",
            "path",         "%s", full_path,
            "offset",       "%ld", (long)offset,
            "errno",        "%s", strerror(errno),
            NULL
        );
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
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot read last record, lseek() FAILED",
                "errno",        "%s", strerror(errno),
                NULL
            );
        }

        ln = read(fd, md_last_record, sizeof(md2_record_t));
        if(ln == sizeof(md2_record_t)) {
            md_last_record->__t__ = (ntohll(md_last_record->__t__)) & TIME_FLAG_MASK;
            md_last_record->__tm__ = (ntohll(md_last_record->__tm__)) & TIME_FLAG_MASK;
            md_last_record->__offset__ = ntohll(md_last_record->__offset__);
            md_last_record->__size__ = ntohll(md_last_record->__size__);
        } else {
            gobj_log_critical(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot read last record of md2 file",
                "path",         "%s", full_path,
                "errno",        "%s", strerror(errno),
                NULL
            );
        }
    }

    close(fd);

    return file_rows;
}

/***************************************************************************
 *  Update or create the files cache of a key
 ***************************************************************************/
PRIVATE int update_new_record_from_mem(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    md2_record_t *md_record
)
{
    json_t *topic_cache = kw_get_dict(gobj, topic, "cache", 0, 0);
    if(!topic_cache) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "no cache files",
            "topic",        "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            NULL
        );
        return -1;
    }

    /*
     *  The cache id is the file_id, the file_id is based in the time __t__ of the record.
     */
    uint64_t t = get_time_t(md_record);
    if(get_system_flag(md_record) & sf_t_ms) {
        t /= 1000;
    }

    char file_id[NAME_MAX];
    get_file_id(
        file_id,
        sizeof(file_id),
        tranger,
        topic,
        t
    );

    /*
     *  See if the file cache exists
     *  WARNING only search in the last item of cell's array
     *  Create the key cache if not exist
     */
    json_t *cur_cache_cell = get_last_cache_cell(
        gobj,
        tranger,
        topic,
        key,
        file_id
    );

    /*
     *  UPDATE CACHE from mem
     */
    if(!cur_cache_cell) {
        json_t *key_cache = get_key_cache(topic, key);
        json_t *cache_files = json_object_get(key_cache, "files");
        json_t *new_cache_cell = update_cache_cell(0, file_id, md_record, 1, 1);
        json_array_append_new(cache_files, new_cache_cell);

    } else {
        update_cache_cell(cur_cache_cell, file_id, md_record, 1, 1);
    }
    update_totals_of_key_cache(gobj, topic, key);

    return 0;
}

/***************************************************************************
 *  Update totals of a key  //TODO review, optimize the  calls of this fn
 ***************************************************************************/
PRIVATE int update_totals_of_key_cache(hgobj gobj, json_t *topic, const char *key)
{
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "no cache files",
            "topic",        "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            NULL
        );
        return -1;
    }

    uint64_t total_rows = 0;
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

    json_object_set_new(total_range, "fr_t", json_integer((json_int_t)global_from_t));
    json_object_set_new(total_range, "to_t", json_integer((json_int_t)global_to_t));
    json_object_set_new(total_range, "fr_tm", json_integer((json_int_t)global_from_tm));
    json_object_set_new(total_range, "to_tm", json_integer((json_int_t)global_to_tm));
    json_object_set_new(total_range, "rows", json_integer((json_int_t)total_rows));

    return 0;
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
        return 0;
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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

    json_object_set_new(iterator, "cur_segment", json_integer(0));
    json_object_set_new(iterator, "cur_rowid", json_integer(0));
    json_object_set_new(iterator, "list_type", json_string("iterator"));

    json_object_set_new(
        iterator,
        "load_record_callback",
        json_integer((json_int_t)(size_t)load_record_callback)
    );
    json_object_update_missing_new(iterator, extra);

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
        json_int_t cur_segment = first_segment_row(
            segments,
            cache_total,
            match_cond,
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
                break;
            }
            if(tranger2_match_metadata(match_cond, total_rows, rowid, &md_record_ex, &end)) {
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
                    if(record) {
                        json_t *__md_tranger__ = md2json(&md_record_ex, rowid);
                        json_object_set_new(
                            record,
                            "__md_tranger__",
                            __md_tranger__  // owned
                        );
                    }
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
    }

    json_array_append_new(
        kw_get_list(gobj, topic, "iterators", 0, KW_REQUIRED),
        iterator
    );

    return iterator;
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_close_iterator(): iterators not found",
            NULL
        );
        return -1;
    }

    int idx = json_array_find_idx(iterators, iterator);
    if(idx >=0 && idx < json_array_size(iterators)) {
        json_array_remove(
            iterators,
            idx
        );
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_get_iterator_by_id: What id?",
            NULL
        );
        return 0;
    }

    json_t *topic = tranger2_topic(tranger, topic_name);
    json_t *iterators = json_object_get(topic, "iterators");
    int idx; json_t *iterator;
    json_array_foreach(iterators, idx, iterator) {
        const char *iterator_id = json_string_value(json_object_get(iterator, "id"));
        if(strcmp(id, iterator_id)==0) {
            const char *creator_ = json_string_value(
                json_object_get(iterator, "creator")
            );
            if(empty_string(creator) && empty_string(creator)) {
                return iterator;
            }
            if(strcmp(creator, creator_)==0) {
                return iterator;
            } else {
                continue;
            }
        }
    }

    // Be silence, check at top.
    return 0;
}

/***************************************************************************
 *  Get Iterator size (nº of rows)
 ***************************************************************************/
PUBLIC size_t tranger2_iterator_size(
    json_t *iterator
)
{
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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

    json_int_t total_rows = get_topic_key_rows(gobj, topic, key);
    if(from_rowid <= 0 || from_rowid > total_rows || limit <= 0) {
        return json_pack("{s:I, s:I, s:[]}",
            "total_rows", total_rows,
            "pages", (json_int_t)0,
            "data"
        );
    }

    json_t *match_cond = json_object();
    json_object_set_new(match_cond, "from_rowid", json_integer(from_rowid));
    json_int_t to_rowid = from_rowid + (json_int_t)limit - 1;
    json_object_set_new(match_cond, "to_rowid", json_integer(to_rowid));
    json_object_set_new(match_cond, "backward", json_boolean(backward));

    json_t *segments = json_object_get(iterator, "segments");
    json_int_t rowid = 0;
    json_t *cache_total = get_cache_total(topic, key);
    json_int_t cur_segment = first_segment_row(
        segments,
        cache_total,
        match_cond,
        &rowid
    );

    if(cur_segment < 0) {
        JSON_DECREF(match_cond)
        return json_pack("{s:I, s:I, s:[]}",
            "total_rows", total_rows,
            "pages", (json_int_t)0,
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
            break;
        }

        if(tranger2_match_metadata(match_cond, total_rows, rowid, &md_record_ex, &end)) {
            const char *file_id = json_string_value(json_object_get(segment, "id"));
            json_t *record = read_record_content(
                tranger,
                topic,
                key,
                file_id,
                &md_record_ex
            );
            if(record) {
                json_t *__md_tranger__ = md2json(&md_record_ex, rowid);
                json_object_set_new(
                    record,
                    "__md_tranger__",
                    __md_tranger__  // owned
                );
            }

            json_array_append_new(data, record);
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

    json_int_t pages = total_rows / (json_int_t)limit;
    if((total_rows%limit)!=0) {
        pages++;
    }
    JSON_DECREF(match_cond)
    return json_pack("{s:I, s:I, s:o}",
        "total_rows", total_rows,
        "pages", pages,
        "data", data
    );
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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

    // WARNING adjust
    if(from_tm == 0) {
        from_tm = total_from_tm;
    } else {
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
        if (to_tm > total_to_tm) {
            // out of range, begin at the end
            to_tm = total_to_tm;
        } else if (to_tm < total_from_tm) {
            // not exist
            return jn_segments;
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

            json_int_t rangeTM_start = kw_get_int(gobj, cache_file, "fr_tm", 0, KW_REQUIRED);
            json_int_t rangeTM_end = kw_get_int(gobj, cache_file, "to_tm", 0, KW_REQUIRED);
            if(rangeTM_start > to_tm) {
                break;
            }

            // Print only the valid ranges
            if (rangeStart <= to_rowid && rangeEnd >= from_rowid &&
                rangeT_start <= to_t && rangeT_end >= from_t &&
                rangeTM_start <= to_tm && rangeTM_end >= from_tm
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

            json_int_t rangeTM_start = kw_get_int(gobj, cache_file, "fr_tm", 0, KW_REQUIRED);
            json_int_t rangeTM_end = kw_get_int(gobj, cache_file, "to_tm", 0, KW_REQUIRED);
            if (rangeTM_end < from_tm) {
                break;
            }

            // Print only the valid ranges
            if (rangeStart <= to_rowid && rangeEnd >= from_rowid &&
                rangeT_start <= to_t && rangeT_end >= from_t &&
                rangeTM_start <= to_tm && rangeTM_end >= from_tm
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
 *  Used by tranger2_iterator_get_page() where rowid/limit is set
 *      as from_rowid/to_rowid in a self create match_cond
 *  and by tranger2_open_iterator()
 ***************************************************************************/
PRIVATE BOOL tranger2_match_metadata(
    json_t *match_cond,
    json_int_t total_rows,
    json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    BOOL *end
)
{
    BOOL backward = json_boolean_value(json_object_get(match_cond, "backward"));
    *end = FALSE;

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
            if(backward) {
                *end = TRUE;
            }
            return FALSE;
        }
    }

    if(to_t != 0) {
        if(md_record_ex->__t__ > to_t) {
            if(!backward) {
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
            if(backward) {
                *end = TRUE;
            }
            return FALSE;
        }
    }

    if(to_tm != 0) {
        if(md_record_ex->__tm__ > to_tm) {
            if(!backward) {
                *end = TRUE;
            }
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
 ***************************************************************************/
PRIVATE json_int_t first_segment_row(
    json_t *segments,
    json_t *cache_total,
    json_t *match_cond,  // not owned
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

                if(from_tm != 0) {
                    if(from_tm > seg_last_tm) {
                        // no match, break and continue
                        break;
                    }
                }

                // Match
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

                if(to_tm != 0) {
                    if(to_tm < seg_first_tm) {
                        break;
                    }
                }

                // Match
                *prowid = rowid;
                return idx;
            } while(0);
        }
    }

    return -1;
}

/***************************************************************************
 *
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
            if(cur_rowid != segment_first_row) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "next rowids not consecutive",
                    "cur_rowid",    "%ld", (long)cur_rowid,
                    "segment_first","%ld", (long)segment_first_row,
                    NULL
                );
                return -1;
            }
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
            if(cur_rowid != segment_last_row) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "previous rowids not consecutive",
                    "cur_rowid",    "%ld", (long)cur_rowid,
                    "segment_first","%ld", (long)segment_last_row,
                    NULL
                );
                return -1;
            }
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

/**rst**
    Get metadata/record in iterator that firstly match match_cond
**rst**/
PUBLIC int tranger2_iterator_find(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    json_t *match_cond,  // owned
    md2_record_ex_t *md_record_ex,
    json_t **record
);

/**rst**
    Get metadata/record of first row in iterator
**rst**/
PUBLIC int tranger2_iterator_first(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_ex_t *md_record_ex,
    json_t **record
);

/**rst**
    Get metadata/record of next row in iterator
**rst**/
PUBLIC int tranger2_iterator_next(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_ex_t *md_record_ex,
    json_t **record
);

/**rst**
    Get metadata/record of previous row in iterator
**rst**/
PUBLIC int tranger2_iterator_prev(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_ex_t *md_record_ex,
    json_t **record
);

/**rst**
    Get metadata/record of last row in iterator
**rst**/
PUBLIC int tranger2_iterator_last(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_ex_t *md_record_ex,
    json_t **record
);

/***************************************************************************
 *  Get metadata/record in iterator that firstly match match_cond
 ***************************************************************************/
PUBLIC int tranger2_iterator_find(
    json_t *tranger,
    json_t *iterator,
    json_int_t *prowid,
    json_t *match_cond,  // owned
    md2_record_ex_t *md_record_ex,
    json_t **record
)
{
//    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
//    const char *topic_name = json_string_value(json_object_get(iterator, "topic_name"));
//    json_t *topic = tranger2_topic(tranger, topic_name);
//    const char *key = json_string_value(json_object_get(iterator, "key"));
//
//    /*
//     *  Check parameters
//     */
//    if(empty_string(topic_name)) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "no topic",
//            NULL
//        );
//        return -1;
//    }
//    if(empty_string(key)) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "no key",
//            NULL
//        );
//        return -1;
//    }
//
//    if(!md_record) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "md_record NULL",
//            "topic_name",   "%s", tranger2_topic_name(topic),
//            "key",          "%s", key,
//            NULL
//        );
//        return -1;
//    } else {
//        memset(md_record, 0, sizeof(md2_record_t));
//    }
//
//    if(record) {
//        *record = NULL;
//    }
//
//    /*
//     *  Get segments
//     */
//    json_t *segments = json_object_get(iterator, "segments");
//    if(json_array_size(segments)==0) {
//        return -1;
//    }
//
//    /*
//     *  Get the pointer (cur_segment, cur_rowid)
//     */
//    json_int_t cur_rowid;
//    json_int_t cur_segment = first_segment_row(segments, match_cond, &cur_rowid);
//    if(cur_segment < 0) {
//        return -1;
//    }
//
//    json_t *segment = json_array_get(segments, cur_segment);
//
//    /*
//     *  Save the pointer, no debería salvarlo después de get_md_.. ok?
//     */
//    json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
//    json_object_set_new(iterator, "cur_rowid", json_integer(cur_rowid));
//
//    /*
//     *  Get the metadata
//     */
//    if(get_md_by_rowid(
//        gobj,
//        tranger,
//        topic,
//        key,
//        segment,
//        cur_rowid,
//        md_record
//    )<0) {
//        return -1;
//    }
//
//    //    if(system_flag & sf_deleted_record) {
//    //        return tranger_prev_record(tranger, topic, md_record);
//    //    }
//
//    if(record) {
//        *record = read_record_content(
//            tranger,
//            topic,
//            key,
//            segment,
//            md_record
//        );
//    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int tranger2_iterator_first(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_ex_t *md_record_ex,
    json_t **record
)
{
//    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
//    const char *topic_name = kw_get_str(gobj, iterator, "topic_name", "", KW_REQUIRED);
//    json_t *topic = tranger2_topic(tranger, topic_name);
//    const char *key = json_string_value(json_object_get(iterator, "key"));
//
//    /*
//     *  Check parameters
//     */
//    if(empty_string(topic_name)) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "no topic",
//            NULL
//        );
//        return -1;
//    }
//    if(empty_string(key)) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "no key",
//            NULL
//        );
//        return -1;
//    }
//
//    if(!md_record_ex) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "md_record NULL",
//            "topic_name",   "%s", tranger2_topic_name(topic),
//            "key",          "%s", key,
//            NULL
//        );
//        return -1;
//    } else {
//        memset(md_record_ex, 0, sizeof(md2_record_ex_t));
//    }
//
//    if(record) {
//        *record = NULL;
//    }
//
//    /*
//     *  Get segments
//     */
//    json_t *segments = json_object_get(iterator, "segments");
//    if(json_array_size(segments)==0) {
//        // Here silence, avoid multiple logs, only logs in first/last
//        return -1;
//    }
//
//    /*
//     *  Get the pointer (cur_segment, cur_rowid)
//     */
//    json_int_t cur_segment = 0;
//    json_t *segment = json_array_get(segments, cur_segment);
//    json_int_t cur_rowid = kw_get_int(gobj, segment, "first_row", 0, KW_REQUIRED);
//
//    /*
//     *  Save the pointer
//     */
//    json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
//    json_object_set_new(iterator, "cur_rowid", json_integer(cur_rowid));
//
//    /*
//     *  Get the metadata
//     */
//    if(get_md_by_rowid(
//        gobj,
//        tranger,
//        topic,
//        key,
//        segment,
//        cur_rowid,
//        md_record_ex
//    )<0) {
//        return -1;
//    }
//
//    //    if(system_flag & sf_deleted_record) {
//    //        return tranger_next_record(tranger, topic, md_record);
//    //    }
//
//    if(rowid) {
//        *rowid = cur_rowid;
//    }
//
//    if(record) {
//        const char *file_id = json_string_value(json_object_get(segment, "id"));
//        *record = read_record_content(
//            tranger,
//            topic,
//            key,
//            file_id,
//            md_record_ex
//        );
//    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int tranger2_iterator_next(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_ex_t *md_record_ex,
    json_t **record
)
{
//    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
//    const char *topic_name = kw_get_str(gobj, iterator, "topic_name", "", KW_REQUIRED);
//    json_t *topic = tranger2_topic(tranger, topic_name);
//    const char *key = json_string_value(json_object_get(iterator, "key"));
//
//    /*
//     *  Check parameters
//     */
//    if(empty_string(topic_name)) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "no topic",
//            NULL
//        );
//        return -1;
//    }
//    if(empty_string(key)) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "no key",
//            NULL
//        );
//        return -1;
//    }
//
//    if(!md_record_ex) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "md_record NULL",
//            "topic_name",   "%s", tranger2_topic_name(topic),
//            "key",          "%s", key,
//            NULL
//        );
//        return -1;
//    } else {
//        memset(md_record_ex, 0, sizeof(md2_record_ex_t));
//    }
//
//    if(record) {
//        *record = NULL;
//    }
//
//    /*
//     *  Get segments
//     */
//    json_t *segments = kw_get_list(gobj, iterator, "segments", 0, KW_REQUIRED);
//    size_t segments_size = json_array_size(segments);
//    if(segments_size == 0) {
//        // Here silence, avoid multiple logs, only logs in first/last
//        return -1;
//    }
//
//    /*
//     *  Get the pointer (cur_segment, cur_rowid)
//     */
//    json_int_t cur_segment = kw_get_int(gobj, iterator, "cur_segment", 0, KW_REQUIRED);
//    json_t *segment = json_array_get(segments, cur_segment);
//    json_int_t cur_rowid = kw_get_int(gobj, iterator, "cur_rowid", 0, KW_REQUIRED);
//
//    /*
//     *  Increment rowid
//     */
//    cur_rowid++;
//
//    /*
//     *  Check if is in the same segment, if not then go to the next segment
//     */
//    json_int_t segment_last_row = kw_get_int(gobj, segment, "last_row", 0, KW_REQUIRED);
//    if(cur_rowid > segment_last_row) {
//        // Go to the next segment
//        cur_segment++;
//        if(cur_segment >= segments_size) {
//            // No more
//            return -1;
//        }
//        segment = json_array_get(segments, cur_segment);
//        json_int_t segment_first_row = kw_get_int(gobj, segment, "first_row", 0, KW_REQUIRED);
//        if(cur_rowid != segment_first_row) {
//            gobj_log_error(gobj, 0,
//                "function",     "%s", __FUNCTION__,
//                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//                "msg",          "%s", "next rowids not consecutive",
//                "topic_name",   "%s", tranger2_topic_name(topic),
//                "key",          "%s", key,
//                "cur_rowid",    "%ld", (long)cur_rowid,
//                "segment_first","%ld", (long)segment_first_row,
//                NULL
//            );
//            gobj_trace_json(gobj, iterator, "next rowids not consecutive");
//            return -1;
//        }
//    }
//
//    /*
//     *  Save the pointer
//     *
//     */
//    json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
//    json_object_set_new(iterator, "cur_rowid", json_integer(cur_rowid));
//
//    /*
//     *  Get the metadata
//     */
//    if(get_md_by_rowid(
//        gobj,
//        tranger,
//        topic,
//        key,
//        segment,
//        cur_rowid,
//        md_record_ex
//    )<0) {
//        return -1;
//    }
//
//    //    if(system_flag & sf_deleted_record) {
//    //        return tranger_prev_record(tranger, topic, md_record);
//    //    }
//
//    if(rowid) {
//        *rowid = cur_rowid;
//    }
//
//    if(record) {
//        const char *file_id = json_string_value(json_object_get(segment, "id"));
//        *record = read_record_content(
//            tranger,
//            topic,
//            key,
//            file_id,
//            md_record_ex
//        );
//    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int tranger2_iterator_prev(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_ex_t *md_record_ex,
    json_t **record
)
{
//    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
//    const char *topic_name = kw_get_str(gobj, iterator, "topic_name", "", KW_REQUIRED);
//    json_t *topic = tranger2_topic(tranger, topic_name);
//    const char *key = json_string_value(json_object_get(iterator, "key"));
//
//    /*
//     *  Check parameters
//     */
//    if(empty_string(topic_name)) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "no topic",
//            NULL
//        );
//        return -1;
//    }
//    if(empty_string(key)) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "no key",
//            NULL
//        );
//        return -1;
//    }
//
//    if(!md_record_ex) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "md_record NULL",
//            "topic_name",   "%s", tranger2_topic_name(topic),
//            "key",          "%s", key,
//            NULL
//        );
//        return -1;
//    } else {
//        memset(md_record_ex, 0, sizeof(md2_record_ex_t));
//    }
//
//    if(record) {
//        *record = NULL;
//    }
//
//    /*
//     *  Get segments
//     */
//    json_t *segments = kw_get_list(gobj, iterator, "segments", 0, KW_REQUIRED);
//    size_t segments_size = json_array_size(segments);
//    if(segments_size == 0) {
//        // Here silence, avoid multiple logs, only logs in first/last
//        return -1;
//    }
//
//    /*
//     *  Get the pointer (cur_segment, cur_rowid)
//     */
//    json_int_t cur_segment = kw_get_int(gobj, iterator, "cur_segment", 0, KW_REQUIRED);
//    json_t *segment = json_array_get(segments, cur_segment);
//    json_int_t cur_rowid = kw_get_int(gobj, iterator, "cur_rowid", 0, KW_REQUIRED);
//
//    /*
//     *  Decrement rowid
//     */
//    cur_rowid--;
//    if(cur_rowid <= 0) {
//        // No more
//        return -1;
//    }
//
//    /*
//     *  Check if is in the same segment, if not then go to the previous segment
//     */
//    json_int_t segment_first_row = kw_get_int(gobj, segment, "first_row", 0, KW_REQUIRED);
//    if(cur_rowid < segment_first_row) {
//        // Go to the previous segment
//        cur_segment--;
//        if(cur_segment < 0) {
//            // No more
//            return -1;
//        }
//        segment = json_array_get(segments, cur_segment);
//
//        json_int_t segment_last_row = kw_get_int(gobj, segment, "last_row", 0, KW_REQUIRED);
//        if(cur_rowid != segment_last_row) {
//            gobj_log_error(gobj, 0,
//                "function",     "%s", __FUNCTION__,
//                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//                "msg",          "%s", "previous rowids not consecutive",
//                "topic_name",   "%s", tranger2_topic_name(topic),
//                "key",          "%s", key,
//                "cur_rowid",    "%ld", (long)cur_rowid,
//                "segment_first","%ld", (long)segment_last_row,
//                NULL
//            );
//            gobj_trace_json(gobj, iterator, "previous rowids not consecutive");
//            return -1;
//        }
//    }
//
//    /*
//     *  Save the pointer
//     *   review ALL
//     */
//    json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
//    json_object_set_new(iterator, "cur_rowid", json_integer(cur_rowid));
//
//    /*
//     *  Get the metadata
//     */
//    if(get_md_by_rowid(
//        gobj,
//        tranger,
//        topic,
//        key,
//        segment,
//        cur_rowid,
//        md_record_ex
//    )<0) {
//        return -1;
//    }
//
//    //    if(system_flag & sf_deleted_record) {
//    //        return tranger_next_record(tranger, topic, md_record);
//    //    }
//
//    if(rowid) {
//        *rowid = cur_rowid;
//    }
//
//    if(record) {
//        const char *file_id = json_string_value(json_object_get(segment, "id"));
//        *record = read_record_content(
//            tranger,
//            topic,
//            key,
//            file_id,
//            md_record_ex
//        );
//    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int tranger2_iterator_last(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_ex_t *md_record_ex,
    json_t **record
)
{
//    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
//    const char *topic_name = kw_get_str(gobj, iterator, "topic_name", "", KW_REQUIRED);
//    json_t *topic = tranger2_topic(tranger, topic_name);
//    const char *key = json_string_value(json_object_get(iterator, "key"));
//
//    /*
//     *  Check parameters
//     */
//    if(empty_string(topic_name)) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "no topic",
//            NULL
//        );
//        return -1;
//    }
//    if(empty_string(key)) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "no key",
//            NULL
//        );
//        return -1;
//    }
//
//    if(!md_record_ex) {
//        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "md_record NULL",
//            "topic_name",   "%s", tranger2_topic_name(topic),
//            "key",          "%s", key,
//            NULL
//        );
//        return -1;
//    } else {
//        memset(md_record_ex, 0, sizeof(md2_record_ex_t));
//    }
//
//    if(record) {
//        *record = NULL;
//    }
//
//    /*
//     *  Get segments
//     */
//    json_t *segments = json_object_get(iterator, "segments");
//    if(json_array_size(segments)==0) {
//        // Here silence, avoid multiple logs, only logs in first/last
//        return -1;
//    }
//
//    /*
//     *  Get the pointer (cur_segment, cur_rowid)
//     */
//    json_int_t cur_segment = (json_int_t)json_array_size(segments) - 1;
//    json_t *segment = json_array_get(segments, cur_segment);
//    json_int_t cur_rowid = kw_get_int(gobj, segment, "last_row", 0, KW_REQUIRED);
//
//    /*
//     *  Save the pointer
//     *   review ALL
//     */
//    json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
//    json_object_set_new(iterator, "cur_rowid", json_integer(cur_rowid));
//
//    /*
//     *  Get the metadata
//     */
//    if(get_md_by_rowid(
//        gobj,
//        tranger,
//        topic,
//        key,
//        segment,
//        cur_rowid,
//        md_record_ex
//    )<0) {
//        return -1;
//    }
//
//    //    if(system_flag & sf_deleted_record) {
//    //        return tranger_prev_record(tranger, topic, md_record);
//    //    }
//
//    if(rowid) {
//        *rowid = cur_rowid;
//    }
//
//    if(record) {
//        const char *file_id = json_string_value(json_object_get(segment, "id"));
//        *record = read_record_content(
//            tranger,
//            topic,
//            key,
//            file_id,
//            md_record_ex
//        );
//    }

    return 0;
}

/***************************************************************************
 *  Get key rows (topic key size)
 ***************************************************************************/
PRIVATE json_int_t get_topic_key_rows(hgobj gobj, json_t *topic, const char *key)
{
    char path[PATH_MAX];

    // Silence, please

    if(empty_string(key)) {
        return 0;
    }
    snprintf(path, sizeof(path), "cache`%s`total`rows", key);
    return kw_get_int(gobj, topic, path, 0, 0);
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
    uint64_t rowid,
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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

    json_int_t relative_rowid = (json_int_t)rowid - first_rowid;
    if(relative_rowid < 0) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read record metadata, relative_rowid negative",
            "topic",        "%s", tranger2_topic_name(topic),
            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "key",          "%s", key,
            "rowid",        "%ld", (long)rowid,
            "relative_rowid","%ld", (long)relative_rowid,
            "errno",        "%s", strerror(errno),
            NULL
        );
        gobj_trace_json(gobj, segment,  "Cannot read record metadata, relative_rowid negative");
        return -1;
    }

    /*
     *  Get file handler
     */
    const char *file_id = json_string_value(json_object_get(segment, "id"));
    return read_md(
        gobj,
        tranger,
        topic,
        key,
        file_id,
        relative_rowid,
        md_record_ex
    );
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
    uint64_t rowid,
    md2_record_ex_t *md_record_ex
)
{
    /*
     *  TODO Find in the cache the range md
     */

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

    off_t offset = (off_t) (rowid * sizeof(md2_record_t));
    off_t offset_ = lseek(fd, offset, SEEK_SET);
    if(offset != offset_) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read record metadata, lseek FAILED",
            "topic",        "%s", tranger2_topic_name(topic),
            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "key",          "%s", key,
            "errno",        "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    size_t ln = read(
        fd,
        &md_record,
        sizeof(md2_record_t)
    );
    if(ln != sizeof(md2_record_t)) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read record metadata, read FAILED",
            "topic",        "%s", tranger2_topic_name(topic),
            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "key",          "%s", key,
            "rowid",        "%ld", (long)rowid,
            "errno",        "%s", strerror(errno),
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
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "What topic?",
            NULL
        );
        return NULL;
    }
    if(empty_string(key)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "What key?",
            NULL
        );
        return NULL;
    }
    if(!md_record_ex) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
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
    get_file_id(
        file_id,
        sizeof(file_id),
        tranger,
        topic,
        t
    );

    json_t *record = read_record_content(
        tranger,
        topic,
        key,
        file_id,
        md_record_ex
    );
    json_t *__md_tranger__ = md2json(md_record_ex, md_record_ex->rowid);
    json_object_set_new(
        record,
        "__md_tranger__",
        __md_tranger__  // owned
    );

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
//// TODO   if(system_flag & sf_deleted_record) {
////        return 0;
////    }

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

    off_t offset = (off_t)md_record_ex->__offset__;
    off_t offset_ = lseek(fd, offset, SEEK_SET);
    if(offset != offset_) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read record data, lseek FAILED",
            "topic",        "%s", tranger2_topic_name(topic),
            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "key",          "%s", key,
            "errno",        "%s", strerror(errno),
            NULL
        );
        return NULL;
    }

    gbuffer_t *gbuf = gbuffer_create(md_record_ex->__size__, md_record_ex->__size__);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Cannot read record data. gbuf_create() FAILED",
            "topic",        "%s", tranger2_topic_name(topic),
            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            NULL
        );
        return NULL;
    }
    char *p = gbuffer_cur_rd_pointer(gbuf);
    size_t ln = read(
        fd,
        p,
        md_record_ex->__size__
    );

    if(ln != md_record_ex->__size__) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read record data, read FAILED",
            "topic",        "%s", tranger2_topic_name(topic),
            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "key",          "%s", key,
            "errno",        "%s", strerror(errno),
            NULL
        );
        gbuffer_decref(gbuf);
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


    json_t *jn_record;
    if(empty_string(p)) {
        jn_record = json_object();
        gbuffer_decref(gbuf);
    } else {
        jn_record = anystring2json(p, strlen(p), FALSE);
        gbuffer_decref(gbuf);
        if(!jn_record) {
            gobj_log_critical(gobj, 0, // Let continue, will be a message lost
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Bad data, anystring2json() FAILED.",
                "topic",        "%s", tranger2_topic_name(topic),
                "__t__",        "%lu", (unsigned long)md_record_ex->__t__,
                "__size__",     "%lu", (unsigned long)md_record_ex->__size__,
                "__offset__",   "%lu", (unsigned long)md_record_ex->__offset__,
                NULL
            );
            return NULL;
        }
    }

    return jn_record;
}

/***************************************************************************
 *
    Open list, load records in memory

    match_cond of second level:
        id                  (str) id
        key                 (str) key (if not exists then rkey is used)
        rkey                (str) regular expression of key (empty "" is equivalent to ".*"
                            WARNING: loading form disk keys matched in rkey)
                                   but loading realtime of all keys

        load_record_callback (tranger2_load_record_callback_t ), // called on LOADING and APPENDING

    For the first level see:

        `Iterator match_cond` in timeranger2.h

    HACK Return of callback:
        0 do nothing (callback will create their own list, or not),
        -1 break the load

    Return realtime (rt_mem or rt_disk)  or no_rt

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

    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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

    const char *key = kw_get_str(gobj, match_cond, "key", "", 0);
    if(!empty_string(key)) {
        json_t *ll = tranger2_open_iterator(
            tranger,
            topic_name,
            key,
            json_incref(match_cond),  // match_cond, owned
            load_record_callback, // called on LOADING and APPENDING
            "",     // iterator id
            "",     // creator
            NULL,   // to store LOADING data, not owned
            json_incref(extra) // extra, owned
        );
        tranger2_close_iterator(tranger, ll);

    } else {
        const char *rkey = kw_get_str(gobj, match_cond, "rkey", "", 0); // "" is equivalent to ".*"
        json_t *jn_keys = tranger2_list_keys(tranger, topic_name, rkey);
        if(json_array_size(jn_keys)>0) {
            /*
             *  Load from disk
             */
            int idx; json_t *jn_key;
            json_array_foreach(jn_keys, idx, jn_key) {
                const char *key_ = json_string_value(jn_key);

                json_t *ll = tranger2_open_iterator(
                    tranger,
                    topic_name,
                    key_,
                    json_incref(match_cond),  // match_cond, owned
                    load_record_callback, // called on LOADING and APPENDING
                    "",     // iterator id
                    "",     // creator
                    NULL,   // to store LOADING data, not owned
                    json_incref(extra) // extra, owned
                );

                tranger2_close_iterator(tranger, ll);
            }
        }
        json_decref(jn_keys);
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
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "Cannot open rt",
                "topic_name",   "%s", topic_name,
                NULL
            );
            JSON_DECREF(match_cond)
            JSON_DECREF(extra)
            return NULL;
        }

        JSON_DECREF(match_cond)
        JSON_DECREF(extra)
        return rt;

    } else {
        if(!extra) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "list with no-realtime require extra",
                "topic_name",   "%s", topic_name,
                NULL
            );
            JSON_DECREF(match_cond)
            JSON_DECREF(extra)
            return NULL;
        }
        json_object_set_new(extra, "list_type", json_string("no_rt"));

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
        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "What topic name?",
            NULL
        );
        return -1;
    }
    if(!creator) {
        creator = "";
    }
    if(!rt_id) {
        rt_id = "";
    }

    json_t *topic = tranger2_topic(tranger, topic_name);

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

    if(key_type & (sf_int_key|sf_string_key)) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key: %s",
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD metadata, without key type",
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

    if(key_type & (sf_int_key|sf_string_key)) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "uflag:0x%"PRIX32", sflag:0x%"PRIX32", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key: %s",
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD metadata, without key type",
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
    const md2_record_ex_t *md_record_ex,
    BOOL print_local_time
)
{
    system_flag2_t system_flag = md_record_ex->system_flag;

    time_t t = (time_t)md_record_ex->__t__;
    uint64_t offset = md_record_ex->__offset__;
    uint64_t size = md_record_ex->__size__;

    char filename[NAME_MAX];
    get_t_filename(
        filename,
        sizeof(filename),
        tranger,
        topic,
        TRUE,   // TRUE for data, FALSE for md2
        (system_flag & sf_t_ms)? t/1000:t  // WARNING must be in seconds!
    );

    const char *topic_dir = kw_get_str(0, topic, "directory", "", KW_REQUIRED);

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/keys/%s/%s", topic_dir, key, filename);

    snprintf(bf, bfsize,
        "rowid:%7"PRIu64", ofs:%7"PRIu64", sz:%7"PRIu64", "
        "t:%"PRIu64", "
        "f:%s",
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
