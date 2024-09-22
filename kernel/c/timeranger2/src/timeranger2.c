/***********************************************************************
 *          TIMERANGER2.C
 *
 *          Time Ranger 2, a series time-key-value database over flat files
 *
 *          Copyright (c) 2017-2018 Niyamaka.
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
    "sf2_string_key",           // 0x0001
    "",                         // 0x0002
    "sf2_int_key",              // 0x0004
    "",                         // 0x0008
    "sf2_zip_record",           // 0x0010
    "sf2_cipher_record",        // 0x0020
    "sf2_save_md_in_record",    // 0x0040
    "",                         // 0x0080
    "sf2_t_ms",                 // 0x0100
    "sf2_tm_ms",                // 0x0200
    "",                         // 0x0400
    "",                         // 0x0800
    "sf2_no_record_disk",       // 0x1000
    "sf2_loading_from_disk",    // 0x2000
    "",                         // 0x4000
    "",                         // 0x8000
    0
};

/***************************************************************
 *              Structures
 ***************************************************************/

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
static inline char *get_t_filename(
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
PRIVATE json_t *load_key_cache(
    hgobj gobj,
    const char *directory,
    const char *key
);
PRIVATE json_t *load_file_cache(
    hgobj gobj,
    const char *directory,
    const char *key,
    const char *filename    // md2 filename with extension
);
PRIVATE int update_key_cache_totals(
    hgobj gobj,
    json_t *topic,
    const char *key
);

PRIVATE json_int_t get_topic_key_rows(hgobj gobj, json_t *topic, const char *key);
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
    md2_record_t *md_record
);
PRIVATE int read_md(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id,
    uint64_t rowid,
    md2_record_t *md_record
);
PRIVATE json_t *read_record_content(
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id,
    md2_record_t *md_record
);
PRIVATE json_int_t first_segment_row(
    json_t *segments,
    json_int_t totals_rows,
    json_t *match_cond,  // not owned
    json_int_t *rowid
);
PRIVATE json_int_t next_segment_row(
    json_t *segments,
    json_t *match_cond,  // not owned
    json_int_t cur_segment,
    json_int_t *rowid
);
PRIVATE BOOL match_record(
    json_t *match_cond,
    json_int_t total_rows,
    json_int_t rowid,
    md2_record_t *md_record,
    BOOL *end
);
PRIVATE json_t *find_keys_in_disk(
    hgobj gobj,
    const char *directory,
    json_t *match_cond  // not owned, uses "key" and "rkey"
);
PRIVATE int load_topic_cache(
    hgobj gobj,
    json_t *tranger,
    json_t *topic
);
PRIVATE json_t *find_file_cache(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id
);
PRIVATE fs_event_t *monitor_disks_directory_by_master(
    hgobj gobj,
    yev_loop_t *yev_loop,
    json_t *tranger,
    json_t *topic
);
PRIVATE int fs_master_callback(fs_event_t *fs_event);
PRIVATE fs_event_t *monitor_rt_disk_by_client(
    hgobj gobj,
    yev_loop_t *yev_loop,
    json_t *tranger,
    json_t *topic,
    const char *id
);
PRIVATE int fs_client_callback(fs_event_t *fs_event);
PRIVATE int mater_to_update_client_load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *rt_id,
    json_int_t rowid,
    md2_record_t *md_record,
    json_t *record      // must be owned
);
PRIVATE int update_new_records(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *md2
);
PRIVATE int publish_new_rt_disk_records(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *old_cache_file,
    json_t *new_cache_file
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
    yev_loop_t *yev_loop
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
 *  Shutdown TimeRanger database
 ***************************************************************************/
PUBLIC int tranger2_shutdown(json_t *tranger)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    const char *key;
    json_t *jn_value;
    json_t *opened_files = kw_get_dict(gobj, tranger, "fd_opened_files", 0, KW_REQUIRED);
    json_object_foreach(opened_files, key, jn_value) {
        int fd = (int)kw_get_int(gobj, opened_files, key, 0, KW_REQUIRED);
        if(fd >= 0) {
            close(fd);
        }
    }

    void *temp;
    json_t *jn_topics = kw_get_dict(gobj, tranger, "topics", 0, KW_REQUIRED);
    json_object_foreach_safe(jn_topics, temp, key, jn_value) {
        tranger2_close_topic(tranger, key);
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

    // TODO check, no hay ya una función para esto?
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
                system_flag |= sf2_string_key;
            } else {
                system_flag |= sf2_int_key;
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
    //kw_get_dict(gobj, topic, "cache", json_object(), KW_CREATE);
    kw_get_dict(gobj, topic, "lists", json_array(), KW_CREATE);
    kw_get_dict(gobj, topic, "disks", json_array(), KW_CREATE); // TODO sobra?
    kw_get_dict(gobj, topic, "iterators", json_array(), KW_CREATE);

    /*
     *  Monitoring the disk to realtime disk lists
     */
    yev_loop_t *yev_loop = (yev_loop_t *)kw_get_int(gobj, tranger, "yev_loop", 0, KW_REQUIRED);
    if(yev_loop) {
        BOOL master = json_boolean_value(json_object_get(tranger, "master"));
        if(master) {
            // (1) MONITOR (MI) /disks/
            // Master to monitor the (topic) directory where clients will mark their rt disks.
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
 *
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
        mater_to_update_client_load_record_callback,   // called on append new record
        rt_id
    );

    snprintf(full_path, PATH_MAX, "%s/%s", directory, filename);
    json_object_set_new(rt, "disk_path", json_string(full_path));
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
 *  Watch create/delete subdirectories of disk realtime id's
 *      that creates/deletes non-master
 ***************************************************************************/
PRIVATE fs_event_t *monitor_disks_directory_by_master(
    hgobj gobj,
    yev_loop_t *yev_loop,
    json_t *tranger,
    json_t *topic
)
{
    char full_path[PATH_MAX];
    const char *directory = kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED);
    snprintf(full_path, sizeof(full_path), "%s/disks",
        directory
    );

    find_rt_disk(tranger, full_path);

    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop,
        full_path,
        0,      // fs_flag,
        fs_master_callback,
        gobj,
        tranger    // user_data
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
 *  Clients will create a rt disk directory in /disks when open a rt disk
 ***************************************************************************/
#include <ansi_escape_codes.h> // TODO remove
PRIVATE int fs_master_callback(fs_event_t *fs_event)
{
    hgobj gobj = fs_event->gobj;
    json_t *tranger = fs_event->user_data;

    print_json2("fs_master_callback", fs_event->jn_tracked_paths); // TODO TEST

    char full_path[PATH_MAX];
    snprintf(full_path, PATH_MAX, "%s/%s", fs_event->directory, fs_event->filename);

    switch(fs_event->fs_type) {
        case FS_SUBDIR_CREATED_TYPE:
            {
                printf("  %sMASTER Dire created:%s %s\n", On_Green BWhite, Color_Off, full_path);
                // (3) MONITOR Client has opened a rt disk for the topic,
                // Master to open a mem rt to update /disks/rt_id/
                char *rt_id = pop_last_segment(full_path);
                char *disks = pop_last_segment(full_path);
                char *topic_name = pop_last_segment(full_path);

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
                    mater_to_update_client_load_record_callback,   // called on append new record
                    rt_id
                );

                snprintf(full_path, PATH_MAX, "%s/%s", fs_event->directory, fs_event->filename);
                json_object_set_new(rt, "disk_path", json_string(full_path));
            }
            break;
        case FS_SUBDIR_DELETED_TYPE:
            {
                printf("  %sMASTER Dire deleted:%s %s\n", On_Green BWhite, Color_Off, full_path);
                // MONITOR Client has closed a rt disk for the topic,
                // Master to close the mem rt
                char *rt_id = pop_last_segment(full_path);
                char *disks = pop_last_segment(full_path);
                //char *topic_name = pop_last_segment(full_path);

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

                json_t *rt = tranger2_get_rt_mem_by_id(tranger, rt_id);
                tranger2_close_rt_mem(tranger, rt);
            }
            break;
        case FS_FILE_CREATED_TYPE:
            printf("  %sMASTER File created:%s %s\n", On_Green BWhite, Color_Off, full_path);
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "FS_FILE_CREATED_TYPE master fs_event NOT processed",
                NULL
            );
            break;
        case FS_FILE_DELETED_TYPE:
            printf("  %sMASTER File deleted:%s %s\n", On_Green BWhite, Color_Off, full_path);
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "FS_FILE_DELETED_TYPE master fs_event NOT processed",
                NULL
            );
            break;
        case FS_FILE_MODIFIED_TYPE:
            printf("  %sMASTER File modified:%s %s\n", On_Green BWhite, Color_Off, full_path);
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
 *  Master mem rt callback to update /disks/rt_id/
 ***************************************************************************/
PRIVATE int mater_to_update_client_load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *rt_id,
    json_int_t rowid,
    md2_record_t *md_record,
    json_t *record      // must be owned
)
{
    char full_path_dest[PATH_MAX];
    char full_path_orig[PATH_MAX];

    // (4) MONITOR update directory /disks/rt_id/ on new records
    // TODO perhaps /rt_id/ is not necessary ?
    // Create a hard link of md2 file
    json_t *rt = tranger2_get_rt_mem_by_id(tranger, rt_id);
    const char *disk_path = json_string_value(json_object_get(rt, "disk_path"));

    /*
     *  Create the directory for the key
     */
    snprintf(full_path_dest, sizeof(full_path_dest), "%s/%s", disk_path, key);
    if(!is_directory(full_path_dest)) {
        mkdir(full_path_dest, json_integer_value(json_object_get(tranger, "xpermission")));
    }

    /*
     *  Create the hard link for the md2 file
     */
    char filename[NAME_MAX];
    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
    if((system_flag & sf2_no_record_disk)) {
        return -1;
    }
    get_t_filename(
        filename,
        sizeof(filename),
        tranger,
        topic,
        FALSE,
        (system_flag & sf2_t_ms)? md_record->__t__/1000 : md_record->__t__
    );
    snprintf(full_path_dest, sizeof(full_path_dest), "%s/%s/%s", disk_path, key, filename);

    const char *topic_dir = json_string_value(json_object_get(topic, "directory"));
    snprintf(full_path_orig, sizeof(full_path_orig), "%s/keys/%s/%s", topic_dir, key, filename);

    if(!is_regular_file(full_path_dest)) {
        if(link(full_path_orig, full_path_dest)<0) {
            hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
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
   Return list of keys of the topic
    match_cond:
        key
        rkey    regular expression of key
 ***************************************************************************/
PUBLIC json_t *tranger2_list_keys( // return is yours
    json_t *tranger,
    json_t *topic,
    json_t *match_cond  // owned, uses "key" and "rkey"
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    const char *directory = kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED);

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/keys",
        directory
    );

    json_t *jn_keys = find_keys_in_disk(gobj, full_path, match_cond);
    JSON_DECREF(match_cond)
    return jn_keys;
}

/***************************************************************************
   Get key size (number of records)
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
    return get_topic_key_rows(gobj, topic, key);
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
    yev_loop_t *yev_loop = (yev_loop_t *)kw_get_int(gobj, tranger, "yev_loop", 0, KW_REQUIRED);
    BOOL master = json_boolean_value(json_object_get(tranger, "master"));
    if(yev_loop && master) {
        fs_event_t *fs_event_master = (fs_event_t *)kw_get_int(
            gobj, topic, "fs_event_master", 0, KW_REQUIRED
        );
        if(fs_event_master) {
            fs_stop_watcher_event(fs_event_master);
            fs_destroy_watcher_event(fs_event_master);
        }
    }

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
     *  Close topic
     */
    tranger2_close_topic(tranger, topic_name);

    /*
     *  Get directory
     */
    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%s/%s",
        kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
        topic_name
    );

    /*
     *  Check if the topic already exists
     */
    if(!is_directory(directory)) {
        return -1;
    }

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
        NULL, // TODO get jn_topic
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

    json_t *cols = kw_get_dict_value(gobj, topic, "cols", json_null(), KW_REQUIRED);
    json_object_set(desc, "cols", cols);

    return desc;
}

/***************************************************************************
 *  Get fullpath of filename in content or md2 level
 *  The directory will be create if it's master
 ***************************************************************************/
static inline char *get_t_filename(
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    BOOL for_data,  // TRUE for data, FALSE for md2
    uint64_t __t__ // WARNING must be in seconds!
)
{
    struct tm *tm = gmtime((time_t *)&__t__);
    if(!tm) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "gmtime() FAILED",
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return NULL;
    }

    char format[NAME_MAX];
    const char *filename_mask = json_string_value(json_object_get(topic, "filename_mask"));
    if(empty_string(filename_mask)) {
        filename_mask = json_string_value(json_object_get(tranger, "filename_mask"));
    }

    strftime(format, sizeof(format), filename_mask, tm);

    snprintf(bf, bfsize, "%s.%s",
        format,
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
            "msg",          "%s", "file not found",
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
    if(access(path_key, 0)!=0) {
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
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "path",         "%s", full_path,
                "msg",          "%s", "TOO MANY OPEN FILES",
                NULL
            );
            close_fd_wr_files(gobj, topic, key);

            fp = newfile(full_path, (int)kw_get_int(gobj, tranger, "rpermission", 0, KW_REQUIRED), FALSE);
            if(fp < 0) {
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
 *
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
    if((system_flag & sf2_no_record_disk)) {
        return -1;
    }

    /*-----------------------------*
     *      Check file
     *-----------------------------*/
    get_t_filename(
        filename,
        sizeof(filename),
        tranger,
        topic,
        for_data,
        (system_flag & sf2_t_ms)? __t__/1000:__t__
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
                fd = create_file(gobj, tranger, topic, key, full_path);
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

    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
    if((system_flag & sf2_no_record_disk)) {
        return -1;
    }

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
    md2_record_t *md_record,
    json_int_t rowid
)
{
    json_t *jn_md = json_object();
    json_object_set_new(jn_md, "rowid", json_integer(rowid));
    json_object_set_new(jn_md, "t", json_integer((json_int_t)md_record->__t__));
    json_object_set_new(jn_md, "tm", json_integer((json_int_t)md_record->__tm__));
    json_object_set_new(jn_md, "offset", json_integer((json_int_t)md_record->__offset__));
    json_object_set_new(jn_md, "size", json_integer((json_int_t)md_record->__size__));
//  TODO  json_object_set_new(jn_md, "__user_flag__", json_integer(user_flag));
//    json_object_set_new(jn_md, "__system_flag__", json_integer(system_flag));

    return jn_md;
}

/***************************************************************************
 *  Append a new record.
 *  Return the new record's metadata.
 ***************************************************************************/
PUBLIC int tranger2_append_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t __t__,         // if 0 then the time will be set by TimeRanger with now time
    uint32_t user_flag,
    md2_record_t *md_record, // required
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
    //uint32_t __system_flag__ = json_integer_value(json_object_get(topic, "system_flag"));
    system_flag2_t __system_flag__ = json_integer_value(json_object_get(topic, "system_flag"));
    if(!__t__) {
        if(__system_flag__ & (sf2_t_ms)) {
            __t__ = time_in_miliseconds();
        } else {
            __t__ = time_in_seconds();
        }
    }

    /*--------------------------------------------*
     *  Prepare new record metadata
     *--------------------------------------------*/
    memset(md_record, 0, sizeof(md2_record_t));
    md_record->__t__ = __t__;

    // TEST performance 700000

    /*-----------------------------------*
     *  Get the primary-key
     *-----------------------------------*/
    //const char *pkey = kw_get_str(gobj, topic, "pkey", "", KW_REQUIRED);
    const char *pkey = json_string_value(json_object_get(topic, "pkey"));
    system_flag2_t system_flag_key_type = __system_flag__ & KEY_TYPE_MASK2;

    const char *key_value = NULL;
    char key_int[NAME_MAX+1];

    switch(system_flag_key_type) {
        case sf2_string_key:
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

        case sf2_int_key:
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
            md_record->__tm__ = 0; // No tkey value, mark with 0
        } else {
            if(json_is_string(jn_tval)) {
                timestamp_t timestamp;
                timestamp = approxidate(json_string_value(jn_tval));
                // TODO if(__system_flag__ & (sf2_tm_ms)) {
                //    timestamp *= 1000; // TODO lost milisecond precision?
                //}
                md_record->__tm__ = timestamp;
            } else if(json_is_integer(jn_tval)) {
                md_record->__tm__ = json_integer_value(jn_tval);
            } else {
                md_record->__tm__ = 0; // No tkey value, mark with 0
            }
        }
    } else {
        md_record->__tm__ = 0;  // No tkey value, mark with 0
    }

    // TEST performance 600000

    /*------------------------------------------------------*
     *  Save content, to file
     *------------------------------------------------------*/
    int content_fp = get_topic_wr_fd(gobj, tranger, topic, key_value, TRUE, __t__);  // Can be -1, if sf_no_disk

    // TEST performance 475000

    /*--------------------------------------------*
     *  New record always at the end
     *--------------------------------------------*/
    off64_t __offset__ = 0;
    if(content_fp >= 0) {
        __offset__ = lseek64(content_fp, 0, SEEK_END);
        if(__offset__ < 0) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot append record, lseek64() FAILED",
                "topic",        "%s", topic_name,
                "errno",        "%s", strerror(errno),
                NULL
            );
            gobj_trace_json(gobj, jn_record, "Cannot append record, lseek64() FAILED");
            JSON_DECREF(jn_record)
            return -1;
        }
        md_record->__offset__ = __offset__;

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
         *  Saving: first compress, second encrypt (TODO sure this order?)
         */
//        if(system_flag & sf_zip_record) {
//            // if(topic->compress_callback) { TODO
//            //     gbuf = topic->compress_callback(
//            //         topic,
//            //         srecord
//            //     );
//            // }
//        }
//        if(system_flag & sf_cipher_record) {
//            // if(topic->encrypt_callback) { TODO
//            //     gbuf = topic->encrypt_callback(
//            //         topic,
//            //         srecord
//            //     );
//            // }
//        }

        md_record->__size__ = size + 1; // put the final null

        /*-------------------------*
         *  Write record content
         *-------------------------*/
        size_t ln = write( // write new (record content)
            content_fp,
            p,
            md_record->__size__
        );
        if(ln != md_record->__size__) {
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
    json_t *__md_tranger__ = NULL;
    json_int_t relative_rowid = 0;
    int md2_fp = get_topic_wr_fd(gobj, tranger, topic, key_value, FALSE, __t__);  // Can be -1, if sf_no_disk
    if(md2_fp >= 0) {
        off64_t offset = lseek64(md2_fp, 0, SEEK_END);
        if(offset < 0) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot append record, lseek64() FAILED",
                "topic",        "%s", topic_name,
                "errno",        "%s", strerror(errno),
                NULL
            );
            gobj_trace_json(gobj, jn_record, "Cannot append record, lseek64() FAILED");
            JSON_DECREF(jn_record)
            return -1;
        }

        relative_rowid = (json_int_t)(offset/sizeof(md2_record_t));

        /*--------------------------------------------*
         *  NEW: write __md_tranger__ to json file
         *--------------------------------------------*/
        __md_tranger__ = md2json(md_record, relative_rowid);
        if(__system_flag__ & (sf2_save_md_in_record) && content_fp >= 0) {
            /*-------------------------------------------------------*
             *  Continue if this part fails, it's extra information
             *-------------------------------------------------------*/
            char *srecord = json_dumps(__md_tranger__, JSON_COMPACT|JSON_ENCODE_ANY);
            if(!srecord) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_JSON_ERROR,
                    "msg",          "%s", "Cannot append __md_tranger__, json_dumps() FAILED",
                    "topic",        "%s", topic_name,
                    NULL
                );
                gobj_trace_json(gobj, jn_record, "Cannot append __md_tranger__, json_dumps() FAILED");
            } else {
                size_t size = strlen(srecord) + 1; // include the null
                char *p = srecord;
                size_t ln = write(
                    content_fp,
                    p,
                    size
                );
                if(ln != size) {
                    gobj_log_critical(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                        "msg",          "%s", "Cannot append __md_tranger__, write FAILED",
                        "topic",        "%s", topic_name,
                        "errno",        "%s", strerror(errno),
                        NULL
                    );
                    gobj_trace_json(gobj, jn_record, "Cannot append __md_tranger__, write FAILED");
                }
                jsonp_free(srecord);
            }
        }

        /*--------------------------------------------*
         *  write md2 in big endian
         *--------------------------------------------*/
        md2_record_t big_endian;
        big_endian.__t__ = htonll(md_record->__t__);
        big_endian.__tm__ = htonll(md_record->__tm__);
        big_endian.__offset__ = htonll(md_record->__offset__);
        big_endian.__size__ = htonll(md_record->__size__);

        size_t ln = write(
            md2_fp,
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
    }

    // TEST performance with sf2_save_md_in_record 98000
    // TEST performance sin sf2_save_md_in_record 124000

    /*-----------------------------------------------------*
     *  Add the message metadata to the record
     *  Could be useful for records with the same __t__
     *  for example, to distinguish them by the readers.
     *-----------------------------------------------------*/
    json_object_set_new(
        jn_record,
        "__md_tranger__",
        __md_tranger__  // owned
    );

    /*--------------------------------------------*
     *      Call callbacks of realtime lists
     *--------------------------------------------*/
    json_t *lists = json_object_get(topic, "lists");
    int idx;
    json_t *list;
    json_array_foreach(lists, idx, list) {
        const char *key_ = json_string_value(json_object_get(list, "key"));
        if(empty_string(key_) || strcmp(key_, key_value)==0) {
            const char *id = json_string_value(json_object_get(list, "id"));
//            tranger2_load_record_callback_t load_record_callback =
//                (tranger2_load_record_callback_t)(size_t)kw_get_int(
//                gobj,
//                list,
//                "load_record_callback",
//                0,
//                KW_REQUIRED
//            );
            tranger2_load_record_callback_t load_record_callback =
                (tranger2_load_record_callback_t)(size_t)json_integer_value(
                    json_object_get(list, "load_record_callback")
                );

            set_user_flag(md_record, sf2_loading_from_disk);

            if(load_record_callback) {
                // Inform to the user list: record from memory in real time
                load_record_callback(
                    tranger,
                    topic,
                    key_value,
                    id,
                    relative_rowid,
                    md_record,
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
//    json_int_t __last_rowid__ = get_topic_key_rows(gobj, topic, key); TODO  es last_rowid
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
//    off64_t offset = (off64_t) ((rowid-1) * sizeof(md2_record_t));
//    off64_t offset_ = lseek64(fd, offset, SEEK_SET);
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
//// TODO   if(rowid != rowid) {
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
//// TODO   off64_t offset = (off64_t) ((rowid-1) * sizeof(md2_record_t));
////    off64_t offset_ = lseek64(fd, offset, SEEK_SET);
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

    return 0;
}

/***************************************************************************
 *   Delete record
 ***************************************************************************/
PUBLIC int tranger2_delete_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid
)
{
//    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
//    json_t *topic = tranger2_topic(tranger, topic_name);
//    if(!topic) {
//        gobj_log_error(gobj, 0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
//            "msg",          "%s", "Cannot open topic",
//            "topic",        "%s", topic_name,
//            NULL
//        );
//        return -1;
//    }
//
//    md2_record_t md_record;
//    if(get_md_record_for_wr(
//        gobj,
//        tranger,
//        topic,
//        rowid,
//        &md_record,
//        TRUE
//    )!=0) {
//        // Error already logged
//        return -1;
//    }
//
//    /*--------------------------------------------*
//     *  Recover file corresponds to __t__
//     *--------------------------------------------*/
//    int fd = get_topic_wr_fd(tranger, topic, "TODO", TRUE, md_record.__t__); // TODO
//    if(fd<0) {
//        // Error already logged
//        return -1;
//    }
//
//    off64_t __offset__ = md_record.__offset__;
//    if(lseek64(fd, __offset__, SEEK_SET) != __offset__) {
//        gobj_log_error(gobj, 0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
//            "msg",          "%s", "Cannot read record data. lseek FAILED",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
//            "errno",        "%s", strerror(errno),
//            "__t__",        "%lu", (unsigned long)md_record.__t__,
//            NULL
//        );
//        return -1;
//    }
//
//    uint64_t __t__ = md_record.__t__;
//    uint64_t __size__ = md_record.__size__;
//
//    gbuffer_t *gbuf = gbuffer_create(__size__, __size__);
//    if(!gbuf) {
//        gobj_log_error(gobj, 0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_MEMORY_ERROR,
//            "msg",          "%s", "Cannot delete record content. gbuf_create() FAILED",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
//            NULL
//        );
//        return -1;
//    }
//    char *p = gbuffer_cur_rd_pointer(gbuf);
//
//    size_t ln = write(fd, p, __size__);    // blank content
//    gbuffer_decref(gbuf);
//    if(ln != __size__) {
//        gobj_log_error(gobj, 0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
//            "msg",          "%s", "Cannot delete record content, write FAILED",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
//            "errno",        "%s", strerror(errno),
//            "ln",           "%d", ln,
//            "__t__",        "%lu", (unsigned long)__t__,
//            "__size__",     "%lu", (unsigned long)__size__,
//            "__offset__",   "%lu", (unsigned long)__offset__,
//            NULL
//        );
//        return -1;
//    }
//
////    TODO md_record.__system_flag__ |= sf_deleted_record;
//    if(rewrite_md_record_to_file(gobj, tranger, topic, &md_record)<0) {
//        return -1;
//    }

    return 0;
}

/***************************************************************************
    Write record mark1
 ***************************************************************************/
PUBLIC int tranger2_write_mark1(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid,
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

    if(set) {
        /*
         *  Set
         */
//TODO        md_record.__system_flag__ |= sf_mark1;
    } else {
        /*
         *  Reset
         */
//      TODO  md_record.__system_flag__ &= ~sf_mark1;
    }

    if(rewrite_md_record_to_file(gobj, tranger, topic, &md_record)<0) {
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

//   TODO md_record.__user_flag__= user_flag;

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

    if(set) {
        /*
         *  Set
         */
//TODO        md_record.__user_flag__ |= mask;
    } else {
        /*
         *  Reset
         */
//    TODO    md_record.__user_flag__ &= ~mask;
    }

    if(rewrite_md_record_to_file(gobj, tranger, topic, &md_record)<0) {
        return -1;
    }

    return 0;
}

/***************************************************************************
    Read record user flag (for writing mode)
 ***************************************************************************/
PUBLIC uint32_t tranger2_read_user_flag(
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
    return 0; // TODO md_record.__user_flag__;
}

/***************************************************************************
 *  Open realtime list
 ***************************************************************************/
PUBLIC json_t *tranger2_open_rt_mem(
    json_t *tranger,
    const char *topic_name,
    const char *key,        // if empty receives all keys, else only this key
    json_t *match_cond,     // owned
    tranger2_load_record_callback_t load_record_callback,   // called on append new record on mem
    const char *id     // list id, optional
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    if(!match_cond) {
        match_cond = json_object();
    }
    if(!key) {
        key="";
    }

    /*
     *  Here the topic is opened if it's not opened
     */
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_open_rt_mem: what topic?",
            NULL
        );
        JSON_DECREF(match_cond)
        return NULL;
    }

    if(!load_record_callback) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_open_rt_mem: what load_record_callback?",
            NULL
        );
        JSON_DECREF(match_cond)
        return NULL;
    }

    json_t *list = json_object();
    char id_[64];
    if(empty_string(id)) {
        snprintf(id_, sizeof(id_), "%"PRIu64, (uint64_t )(size_t)list);
        id = id_;
    }

    if(tranger2_get_rt_mem_by_id(tranger, id)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_open_rt_mem(): Mem already exists",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(list)
        return NULL;
    }

    json_object_update_new(list, json_pack("{s:s, s:s, s:s, s:o, s:I}",
        "id", id,
        "topic_name", topic_name,
        "key", key,
        "match_cond", match_cond,
        "load_record_callback", (json_int_t)(size_t)load_record_callback
    ));

    /*
     *  Add the list to the topic
     */
    json_array_append_new(
        kw_get_dict_value(gobj, topic, "lists", 0, KW_REQUIRED),
        list
    );

    return list;
}

/***************************************************************************
 *  Close realtime list
 ***************************************************************************/
PUBLIC int tranger2_close_rt_mem(
    json_t *tranger,
    json_t *list
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    if(!list) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_close_rt_mem(): list NULL",
            NULL
        );
        return -1;
    }

    const char *topic_name = kw_get_str(gobj, list, "topic_name", "", KW_REQUIRED);
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

    int idx = json_array_find_idx(lists, list);
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
    const char *id
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(empty_string(id)) {
        return 0;
    }
    json_t *topics = kw_get_dict_value(gobj, tranger, "topics", 0, KW_REQUIRED);

    const char *topic_name; json_t *topic;
    json_object_foreach(topics, topic_name, topic) {
        json_t *lists = kw_get_list(gobj, topic, "lists", 0, KW_REQUIRED);
        int idx; json_t *list;
        json_array_foreach(lists, idx, list) {
            const char *list_id = kw_get_str(gobj, list, "id", "", 0);
            if(strcmp(id, list_id)==0) {
                return list;
            }
        }
    }
    return 0;
}

/***************************************************************************
 *  Open realtime disk,
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
    const char *id     // disk id, optional
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    if(!match_cond) {
        match_cond = json_object();
    }
    if(!key) {
        key="";
    }

    /*
     *  Here the topic is opened if it's not opened
     */
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_open_rt_disk: what topic?",
            NULL
        );
        JSON_DECREF(match_cond)
        return NULL;
    }

    if(!load_record_callback) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_open_rt_disk: what load_record_callback?",
            NULL
        );
        JSON_DECREF(match_cond)
        return NULL;
    }


    json_t *disk = json_object();
    char id_[64];
    if(empty_string(id)) {
        snprintf(id_, sizeof(id_), "%"PRIu64, (uint64_t )(size_t)disk);
        id = id_;
    }

    if(tranger2_get_rt_disk_by_id(tranger, id)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_open_rt_disk(): Disk already exists",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(match_cond)
        JSON_DECREF(disk)
        return NULL;
    }

    json_object_update_new(disk, json_pack("{s:s, s:s, s:s, s:o, s:I}",
        "id", id,
        "topic_name", topic_name,
        "key", key,
        "match_cond", match_cond,
        "load_record_callback", (json_int_t)(size_t)load_record_callback
    ));

    /*
     *  Add the list to the topic
     */
    json_array_append_new(
        kw_get_dict_value(gobj, topic, "disks", 0, KW_REQUIRED),
        disk
    );

    /*
     *  Create in disk the realtime disk directory and monitor
     */
    yev_loop_t *yev_loop = (yev_loop_t *)kw_get_int(gobj, tranger, "yev_loop", 0, KW_REQUIRED);
    if(yev_loop) {
        // Master can operate as a non-master and operate through the disk (NOT SENSE, only to test)
        // BOOL master = json_boolean_value(json_object_get(tranger, "master"));
        if(1) {
            // (2) MONITOR (C) (MI)r /disks/rt_id/
            // The directory is created inside this function
            fs_event_t *fs_event_client = monitor_rt_disk_by_client(
                gobj, yev_loop, tranger, topic, id
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
 *  Watch create/delete subdirectories and files of disk realtime id's
 *      that creates/deletes master
 ***************************************************************************/
PRIVATE fs_event_t *monitor_rt_disk_by_client(
    hgobj gobj,
    yev_loop_t *yev_loop,
    json_t *tranger,
    json_t *topic,
    const char *id
)
{
    char full_path[PATH_MAX];
    const char *directory = kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED);
    snprintf(full_path, sizeof(full_path), "%s/disks/%s",
        directory,
        id
    );

    if(!is_directory(full_path)) {
        mkdir(full_path, json_integer_value(json_object_get(tranger, "xpermission")));
    }

    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop,
        full_path,
        FS_FLAG_RECURSIVE_PATHS,      // fs_flag,
        fs_client_callback,
        gobj,
        tranger    // user_data
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
 *  The master signalize a new record appended
 ***************************************************************************/
PRIVATE int fs_client_callback(fs_event_t *fs_event)
{
    hgobj gobj = fs_event->gobj;
    json_t *tranger = fs_event->user_data;

    char full_path[PATH_MAX];
    snprintf(full_path, PATH_MAX, "%s/%s", fs_event->directory, fs_event->filename);

    switch(fs_event->fs_type) {
        case FS_SUBDIR_CREATED_TYPE:
            // (5) MONITOR notify of update directory /disks/rt_id/ on new records
            // Key directory created, ignore

            printf("  %sCLIENT Dire created:%s %s\n", On_Green BWhite, Color_Off, full_path);
            {
                pop_last_segment(full_path);    // char *key =
                pop_last_segment(full_path);    // char *rt_id =
                char *disks = pop_last_segment(full_path);
                //pop_last_segment(full_path);    // char *topic_name =

                if(strcmp(disks, "disks")!=0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "Bad path client 1 /disks/rt_id/key",
                        "directory",    "%s", fs_event->directory,
                        "filename",     "%s", fs_event->filename,
                        NULL
                    );
                    break;
                }

            }
            break;
        case FS_SUBDIR_DELETED_TYPE:
            // Key directory deleted, ignore, it's me
            printf("  %sCLIENT Dire deleted:%s %s\n", On_Green BWhite, Color_Off, full_path);
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
                //printf("  %sCLIENT File created:%s %s\n", On_Green BWhite, Color_Off, full_path);
                if(unlink(full_path)<0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                        "msg",          "%s", "unlink() FAILED",
                        "errno",        "%s", strerror(errno),
                        NULL
                    );
                }

                char *md2 = pop_last_segment(full_path);
                char *key = pop_last_segment(full_path);
                char *rt_id = pop_last_segment(full_path);
                char *disks = pop_last_segment(full_path);
                char *topic_name = pop_last_segment(full_path);
                json_t *topic = tranger2_topic(tranger,topic_name);

                if(strcmp(disks, "disks")!=0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "Bad path client 2 /disks/rt_id/key/md2",
                        "directory",    "%s", fs_event->directory,
                        "filename",     "%s", fs_event->filename,
                        NULL
                    );
                    break;
                }
                json_t *iterator = tranger2_get_iterator_by_id(tranger, rt_id);
                if(!iterator) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "iterator NOT FOUND",
                        "topic_name",   "%s", topic_name,
                        "rt_id",        "%s", rt_id,
                        NULL
                    );
                    print_json2("iterator NOT FOUND", tranger);
                    break;
                }

                char *p = strrchr(md2, '.'); // pass file_id
                if(p) {
                    *p =0;
                }
                update_new_records(
                    gobj,
                    tranger,
                    topic,
                    key,
                    md2
                );
            }
            break;
        case FS_FILE_DELETED_TYPE:
            //printf("  %sCLIENT File deleted:%s %s\n", On_Green BWhite, Color_Off, full_path);
            break;
        case FS_FILE_MODIFIED_TYPE:
            printf("  %sCLIENT File modified:%s %s\n", On_Green BWhite, Color_Off, full_path);
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
 *
    The last record of files  - {topic}/cache/{key}/files/[{r}] -
    has the last record appended.
        {
            "id": "2000-01-03",
            "fr_t": 946857600,
            "to_t": 946864799,
            "fr_tm": 946857600,
            "to_tm": 946864799,
            "rows": 226651,
            "wr_time": 1726943703371895964
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
            (or only news segments, see how to use the wr_time to
            update only what is necessary)

            IT'S necessary to load and publish only the new records!
            !!! How are you going to repeats records to the client? You fool? !!!

 ***************************************************************************/
PRIVATE int update_new_records(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id
)
{
    const char *directory = json_string_value(json_object_get(topic, "directory"));
    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.md2", file_id);
    json_t *new_cache_file = load_file_cache(
        gobj,
        directory,
        key,
        filename
    );

    json_t *cur_cache_file = find_file_cache(
        gobj,
        tranger,
        topic,
        key,
        file_id
    );

    // Publish new data to iterator
    publish_new_rt_disk_records(gobj, tranger, topic, key, cur_cache_file, new_cache_file);

    /*
     *  Update cache
     */
    if(!cur_cache_file) {
        // Update cache
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
        json_array_append_new(cache_files, new_cache_file);
    } else {
        json_object_update_new(cur_cache_file, new_cache_file);
    }
    update_key_cache_totals(gobj, topic, key);

    // Mark cache as new data available to others iterators.
    // TODO

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
    json_t *old_cache_file,
    json_t *new_cache_file
)
{
    json_t *disks = json_object_get(topic, "disks");

    json_int_t from_rowid = json_integer_value(json_object_get(old_cache_file, "rows"));
    json_int_t to_rowid = json_integer_value(json_object_get(new_cache_file, "rows"));
    const char *file_id = json_string_value(json_object_get(new_cache_file, "id"));

    for(json_int_t rowid=from_rowid; rowid<to_rowid; rowid++) {
        md2_record_t md_record;
        read_md(
            gobj,
            tranger,
            topic,
            key,
            file_id,
            rowid,
            &md_record
        );
        json_t *record = read_record_content(
            tranger,
            topic,
            key,
            file_id,
            &md_record
        );

        int idx; json_t *disk;
        json_array_foreach(disks, idx, disk) {
            const char *key_ = json_string_value(json_object_get(disk, "key"));
            if(empty_string(key_) || strcmp(key_, key)==0) {
                tranger2_load_record_callback_t load_record_callback =
                    (tranger2_load_record_callback_t)(size_t)json_integer_value(
                        json_object_get(disk, "load_record_callback")
                    );

                set_user_flag(&md_record, sf2_loading_from_disk);

                if(load_record_callback) {
                    // Inform to the user list: record from disk in real time
                    const char *id = json_string_value(json_object_get(disk, "id"));
                    load_record_callback(
                        tranger,
                        topic,
                        key,
                        id,
                        rowid,
                        &md_record,
                        json_incref(record)
                    );
                }
            }
        }

        JSON_DECREF(record)
    }

    return 0;
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

    const char *topic_name = kw_get_str(gobj, disk, "topic_name", "", KW_REQUIRED);
    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, KW_REQUIRED);

    yev_loop_t *yev_loop = (yev_loop_t *)kw_get_int(gobj, tranger, "yev_loop", 0, KW_REQUIRED);
    if(yev_loop) {
        // MONITOR Client Unwatching (MI) topic /disks/rt_id/
        fs_event_t *fs_event_client = (fs_event_t *)kw_get_int(
            gobj, disk, "fs_event_client", 0, KW_REQUIRED
        );
        if(fs_event_client) {
            fs_stop_watcher_event(fs_event_client);
            fs_destroy_watcher_event(fs_event_client);
        }

        // MONITOR (D) /disks/rt_id/
        char full_path[PATH_MAX];
        const char *directory = kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED);
        snprintf(full_path, sizeof(full_path), "%s/disks/%s",
            directory,
            kw_get_str(gobj, disk, "id", "", KW_REQUIRED)
        );
        rmrdir(full_path); // TODO para testear quita y pon
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
    const char *id
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(empty_string(id)) {
        return 0;
    }
    json_t *topics = kw_get_dict_value(gobj, tranger, "topics", 0, KW_REQUIRED);

    const char *topic_name; json_t *topic;
    json_object_foreach(topics, topic_name, topic) {
        json_t *disks = kw_get_list(gobj, topic, "disks", 0, KW_REQUIRED);
        int idx; json_t *disk;
        json_array_foreach(disks, idx, disk) {
            const char *disk_id = kw_get_str(gobj, disk, "id", "", 0);
            if(strcmp(id, disk_id)==0) {
                return disk;
            }
        }
    }
    return 0;
}

/***************************************************************************
 *  Returns list of searched keys that exist on disk
 ***************************************************************************/
PRIVATE json_t *find_keys_in_disk(
    hgobj gobj,
    const char *directory,
    json_t *match_cond  // not owned, uses "key" and "rkey"
)
{
    json_t *jn_keys = json_array();

    /*
     *  Only wants a key ?
     */
    const char *key = json_string_value(json_object_get(match_cond, "key"));
    if(!empty_string(key)) {
        if(subdir_exists(directory, key)) {
            json_array_append_new(jn_keys, json_string(key));
            return jn_keys;
        }
    }

    const char *pattern;
    const char *rkey = json_string_value(json_object_get(match_cond, "rkey"));
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
 *  IDEMPOTENT
 ***************************************************************************/
PRIVATE int load_topic_cache(
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
    if(!topic_cache) {
        /*
         *  Create cache
         */
        topic_cache = kw_get_dict(gobj, topic, "cache", json_object(), KW_CREATE);

        int idx; json_t *jn_key;
        json_array_foreach(jn_keys, idx, jn_key) {
            const char *key = json_string_value(jn_key);
            json_t *key_cache = load_key_cache(gobj, directory, key);
            json_object_set_new(topic_cache, key, key_cache);
            update_key_cache_totals(gobj, topic, key);
        }
    } else {
        /*
         *  Update cache
         */
        // TODO
    }

    JSON_DECREF(jn_keys)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *find_file_cache(
    hgobj gobj,
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id
)
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
        return NULL;
    }

    if (json_array_size(cache_files) == 0) {
        return NULL;
    }

    json_int_t cur_file = (json_int_t)json_array_size(cache_files) - 1;
    json_t *cache_file = json_array_get(cache_files, cur_file);

    const char *file_id_ = json_string_value(json_object_get(cache_file, "id"));
    if(strcmp(file_id, file_id_)!=0) {
        return NULL;
    }
    return cache_file;
}

/***************************************************************************
 *  Get modify time of a file in nanoseconds
 ***************************************************************************/
PRIVATE uint64_t get_modify_time_ns(hgobj gobj, int fd)
{
    struct stat file_stat;

    // Retrieve the file status
    if (fstat(fd, &file_stat) == -1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "fstat() FAILED",
            "errno",        "%s", strerror(errno),
            NULL
        );
        return 0;  // Return 0 in case of an error
    }

    // Combine seconds and nanoseconds into a single uint64_t value representing nanoseconds
    // since the epoch
    uint64_t mtime_ns = (uint64_t)file_stat.st_mtim.tv_sec * 1000000000ULL +
        (uint64_t)file_stat.st_mtim.tv_nsec;

    return mtime_ns;
}

/***************************************************************************
 *  Get range time of a key
 ***************************************************************************/
PRIVATE json_t *load_key_cache(hgobj gobj, const char *directory, const char *key)
{
    char full_path[PATH_MAX];

    json_t *key_cache = json_object();
    json_t *cache_files = kw_get_list(gobj, key_cache, "files", json_array(), KW_CREATE);
    kw_get_dict(gobj, key_cache, "total", json_object(), KW_CREATE);

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
        json_t *file_cache = load_file_cache(gobj, directory, key, filename);
        json_array_append_new(cache_files, file_cache);
    }
    free_ordered_filename_array(files_md, files_md_size);

    return key_cache;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *load_file_cache(
    hgobj gobj,
    const char *directory,
    const char *key,
    const char *filename    // md2 filename with extension
)
{
    char full_path[PATH_MAX];

    md2_record_t md_first_record;
    md2_record_t md_last_record;
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
        return NULL;
    }

    /*
     *  Read first record
     */
    ssize_t ln = read(fd, &md_first_record, sizeof(md_first_record));
    if(ln == sizeof(md_first_record)) {
        md_first_record.__t__ = ntohll(md_first_record.__t__);
        md_first_record.__tm__ = ntohll(md_first_record.__tm__);
        md_first_record.__offset__ = ntohll(md_first_record.__offset__);
        md_first_record.__size__ = ntohll(md_first_record.__size__);
    } else {
        if(ln<0) {
            gobj_log_critical(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot read md2 file",
                "path",         "%s", full_path,
                "errno",        "%s", strerror(errno),
                NULL
            );
        } else if(ln==0) {
            // No data
        }
        close(fd);
        return NULL;
    }

    /*
     *  Seek the last record
     */
    off64_t offset = lseek64(fd, 0, SEEK_END);
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
        close(fd);
        return NULL;
    }
    if(offset == 0) {
        // No records
        close(fd);
        return NULL;
    }

    uint64_t file_rows = offset/sizeof(md2_record_t);

    /*
     *  Read last record
     */
    offset -= sizeof(md2_record_t);
    off64_t offset2 = lseek64(fd, offset, SEEK_SET);
    if(offset2 < 0 || offset2 != offset) {
        gobj_log_critical(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read last record, lseek64() FAILED",
            "errno",        "%s", strerror(errno),
            NULL
        );
        close(fd);
        return NULL;
    }

    ln = read(fd, &md_last_record, sizeof(md_last_record));
    if(ln == sizeof(md_last_record)) {
        md_last_record.__t__ = ntohll(md_last_record.__t__);
        md_last_record.__tm__ = ntohll(md_last_record.__tm__);
        md_last_record.__offset__ = ntohll(md_last_record.__offset__);
        md_last_record.__size__ = ntohll(md_last_record.__size__);
    } else {
        if(ln<0) {
            gobj_log_critical(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot read md2 file",
                "path",         "%s", full_path,
                "errno",        "%s", strerror(errno),
                NULL
            );
        } else if(ln==0) {
            // No data
        }
        close(fd);
        return NULL;
    }
    uint64_t modify_time = get_modify_time_ns(gobj, fd);
    close(fd);

    uint64_t file_from_t = (uint64_t)(-1);
    uint64_t file_to_t = 0;
    uint64_t file_from_tm = (uint64_t)(-1);
    uint64_t file_to_tm = 0;

    if(md_first_record.__t__ < file_from_t) {
        file_from_t = md_first_record.__t__;
    }
    if(md_first_record.__t__ > file_to_t) {
        file_to_t = md_first_record.__t__;
    }

    if(md_first_record.__tm__ < file_from_tm) {
        file_from_tm = md_first_record.__tm__;
    }
    if(md_first_record.__tm__ > file_to_tm) {
        file_to_tm = md_first_record.__tm__;
    }

    if(md_last_record.__t__ < file_from_t) {
        file_from_t = md_last_record.__t__;
    }
    if(md_last_record.__t__ > file_to_t) {
        file_to_t = md_last_record.__t__;
    }

    if(md_last_record.__tm__ < file_from_tm) {
        file_from_tm = md_last_record.__tm__;
    }
    if(md_last_record.__tm__ > file_to_tm) {
        file_to_tm = md_last_record.__tm__;
    }

    char *p = strrchr(filename, '.');    // save file name without extension
    if(p) {
        *p = 0;
    }
    json_t *file_cache = json_object();
    json_object_set_new(file_cache, "id", json_string(filename));
    if(p) {
        *p = '.';
    }

    json_object_set_new(file_cache, "fr_t", json_integer((json_int_t)file_from_t));
    json_object_set_new(file_cache, "to_t", json_integer((json_int_t)file_to_t));
    json_object_set_new(file_cache, "fr_tm", json_integer((json_int_t)file_from_tm));
    json_object_set_new(file_cache, "to_tm", json_integer((json_int_t)file_to_tm));
    json_object_set_new(file_cache, "rows", json_integer((json_int_t)file_rows));

    json_object_set_new(file_cache, "wr_time", json_integer((json_int_t)modify_time));

    return file_cache;
}

/***************************************************************************
 *  Update totals of a key
 ***************************************************************************/
PRIVATE int update_key_cache_totals(hgobj gobj, json_t *topic, const char *key)
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
    json_t *topic,
    const char *key,    // ONLY one key by iterator
    json_t *match_cond, // owned
    tranger2_load_record_callback_t load_record_callback, // called on loading and appending new record
    const char *iterator_id,     // iterator id, optional, if empty will be the key
    json_t *data    // JSON array, if not empty, fills it with the LOADING data, not owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

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
            "msg",          "%s", "tranger2_open_iterator(): What key?",
            "topic_name",   "%s", tranger2_topic_name(topic),
            NULL
        );
        JSON_DECREF(match_cond)
        return NULL;
    }
    if(empty_string(iterator_id)) {
        iterator_id = key;
    }

    if(tranger2_get_iterator_by_id(tranger, iterator_id)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "tranger2_open_iterator(): Iterator already exists",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            "id",           "%s", iterator_id,
            NULL
        );
        JSON_DECREF(match_cond)
        return NULL;
    }

    /*-----------------------------------------*
     *      Load keys and metadata from disk
     *-----------------------------------------*/
    load_topic_cache(gobj, tranger, topic);  // idempotent

    BOOL realtime;
    json_t *segments = get_segments(
        gobj,
        tranger,
        topic,
        key,
        match_cond, // NOT owned but can be modified
        &realtime
    );

    json_t *iterator = json_object();
    json_object_set_new(iterator, "id", json_string(iterator_id));
    json_object_set_new(iterator, "key", json_string(key));
    json_object_set_new(iterator, "topic_name", json_string(tranger2_topic_name(topic)));
    json_object_set_new(iterator, "match_cond", match_cond);    // owned
    json_object_set_new(iterator, "segments", segments);        // owned

    json_object_set_new(iterator, "cur_segment", json_integer(0));
    json_object_set_new(iterator, "cur_rowid", json_integer(0));

    json_object_set_new(
        iterator,
        "load_record_callback",
        json_integer((json_int_t)(size_t)load_record_callback)
    );

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
        md2_record_t md_record;

        json_int_t total_rows = get_topic_key_rows(gobj, topic, key);
        json_int_t cur_segment = first_segment_row(segments, total_rows, match_cond, &rowid);

        /*
         *  Save the pointer
         */
        if(cur_segment >= 0) {
            json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
            json_object_set_new(iterator, "cur_rowid", json_integer(rowid));
        }

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
                &md_record
            )<0) {
                break;
            }

            if(match_record(match_cond, total_rows, rowid, &md_record, &end)) {
                const char *file_id = json_string_value(json_object_get(segment, "id"));
                json_t *record = NULL;
                if(!only_md) {
                    record = read_record_content(
                        tranger,
                        topic,
                        key,
                        file_id,
                        &md_record
                    );
                    if(record) {
                        json_t *__md_tranger__ = md2json(&md_record, rowid);
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
                        iterator_id,
                        rowid,  // rowid
                        &md_record,
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

        /*-------------------------------*
         *  Open realtime for iterator
         *-------------------------------*/
        if(realtime) {
            BOOL master = json_boolean_value(json_object_get(tranger, "master"));
            BOOL rt_by_mem = json_boolean_value(json_object_get(match_cond, "rt_by_mem"));
            if(!master) {
                rt_by_mem = FALSE;
            }
            json_t *rt;
            if(rt_by_mem) {
                rt = tranger2_open_rt_mem(
                    tranger,
                    tranger2_topic_name(topic),
                    key,                    // if empty receives all keys, else only this key
                    json_incref(match_cond),
                    load_record_callback,   // called on append new record
                    iterator_id
                );
                json_object_set(iterator, "rt_mem", rt);
            } else {
                rt = tranger2_open_rt_disk(
                    tranger,
                    tranger2_topic_name(topic),
                    key,                    // if empty receives all keys, else only this key
                    json_incref(match_cond),
                    load_record_callback,   // called on append new record
                    iterator_id
                );
                json_object_set(iterator, "rt_disk", rt);
            }
            if(!rt) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "tranger2_open_iterator(): Cannot open rt",
                    "topic_name",   "%s", tranger2_topic_name(topic),
                    "key",          "%s", key,
                    "id",           "%s", iterator_id,
                    NULL
                );
                JSON_DECREF(iterator)
                return NULL;
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
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    if(!iterator) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
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
    const char *id
)
{
    if(empty_string(id)) {
        return 0;
    }
    json_t *topics = json_object_get(tranger, "topics");

    const char *topic_name; json_t *topic;
    json_object_foreach(topics, topic_name, topic) {
        json_t *iterators = json_object_get(topic, "iterators");
        int idx; json_t *iterator;
        json_array_foreach(iterators, idx, iterator) {
            const char *iterator_id = json_string_value(json_object_get(iterator, "id"));
            if(strcmp(id, iterator_id)==0) {
                return iterator;
            }
        }
    }
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
    json_int_t cur_segment = first_segment_row(segments, total_rows, match_cond, &rowid);

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
    md2_record_t md_record;

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
            &md_record
        )<0) {
            break;
        }

        if(match_record(match_cond, total_rows, rowid, &md_record, &end)) {
            const char *file_id = json_string_value(json_object_get(segment, "id"));
            json_t *record = read_record_content(
                tranger,
                topic,
                key,
                file_id,
                &md_record
            );
            if(record) {
                json_t *__md_tranger__ = md2json(&md_record, rowid);
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
 *  Return a list of segments that match conditions
 *  match_cond cah be modified in (times in string)
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
    char path[NAME_MAX];
    snprintf(path, sizeof(path), "cache`%s`files", key);
    json_t *cache_files = kw_get_list(gobj, topic, path, 0, 0); // Use `, don't use json_object_get()
    if(!cache_files) {
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
    snprintf(path, sizeof(path), "cache`%s`total", key);
    json_t *cache_total = kw_get_dict(gobj, topic, path, 0, 0);
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
            // TODO if(__system_flag__ & (sf2_tm_ms)) {
            //    timestamp *= 1000; // TODO lost milisecond precision?
            //}
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
            // TODO if(__system_flag__ & (sf2_tm_ms)) {
            //    timestamp *= 1000; // TODO lost milisecond precision?
            //}
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
            // TODO if(__system_flag__ & (sf2_tm_ms)) {
            //    timestamp *= 1000; // TODO lost milisecond precision?
            //}
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
            // TODO if(__system_flag__ & (sf2_tm_ms)) {
            //    timestamp *= 1000; // TODO lost milisecond precision?
            //}
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
     *------------------------------------------*/
    if(!backward) {
        int idx; json_t *cache_file;
        json_int_t partial_rows2 = 1;
        json_int_t rows2;
        json_array_foreach(cache_files, idx, cache_file) {
// TODO           json_int_t from_t_2 = kw_get_int(gobj, cache_file, "fr_t", 0, KW_REQUIRED);
//            json_int_t to_t_2 = kw_get_int(gobj, cache_file, "to_t", 0, KW_REQUIRED);
//            json_int_t from_tm_2 = kw_get_int(gobj, cache_file, "fr_tm", 0, KW_REQUIRED);
//            json_int_t to_tm_2 = kw_get_int(gobj, cache_file, "to_tm", 0, KW_REQUIRED);
            rows2 = kw_get_int(gobj, cache_file, "rows", 0, KW_REQUIRED);

            json_int_t rangeStart = partial_rows2; // first row of this segment
            json_int_t rangeEnd = partial_rows2 + rows2 - 1; // last row of this segment

            // If the current range starts after the input range ends, stop further checks
            if (rangeStart > to_rowid) {
                break;
            }

            // Print only the valid ranges
            if (rangeStart <= to_rowid && rangeEnd >= from_rowid) {
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
        json_int_t rows2;
        json_array_backward(cache_files, idx, cache_file) {
// TODO           json_int_t from_t_2 = kw_get_int(gobj, cache_file, "fr_t", 0, KW_REQUIRED);
//            json_int_t to_t_2 = kw_get_int(gobj, cache_file, "to_t", 0, KW_REQUIRED);
//            json_int_t from_tm_2 = kw_get_int(gobj, cache_file, "fr_tm", 0, KW_REQUIRED);
//            json_int_t to_tm_2 = kw_get_int(gobj, cache_file, "to_tm", 0, KW_REQUIRED);
            rows2 = kw_get_int(gobj, cache_file, "rows", 0, KW_REQUIRED);

            json_int_t rangeStart = partial_rows2 - rows2 + 1; // first row of this segment
            json_int_t rangeEnd = partial_rows2;  // last row of this segment

            // If the current range ends before the input range starts, stop further checks
            if (rangeEnd < from_rowid) {
                break;
            }

            // Print only the valid ranges
            if (rangeStart <= to_rowid && rangeEnd >= from_rowid) {
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
 *
 ***************************************************************************/
PRIVATE BOOL match_record(
    json_t *match_cond,
    json_int_t total_rows,
    json_int_t rowid,
    md2_record_t *md_record,
    BOOL *end
)
{
    BOOL backward = json_boolean_value(json_object_get(match_cond, "backward"));
    json_int_t from_rowid = json_integer_value(json_object_get(match_cond, "from_rowid"));
    json_int_t to_rowid = json_integer_value(json_object_get(match_cond, "to_rowid"));

    *end = FALSE;

//            json_int_t seg_from_t = json_integer_value(json_object_get(segment, "fr_t"));
//            json_int_t seg_to_t = json_integer_value(json_object_get(segment, "to_t"));
//            json_int_t seg_from_tm = json_integer_value(json_object_get(segment, "fr_tm"));
//            json_int_t seg_to_tm = json_integer_value(json_object_get(segment, "to_tm"));

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

    return TRUE;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_int_t first_segment_row(
    json_t *segments,
    json_int_t total_rows,
    json_t *match_cond,  // not owned
    json_int_t *prowid
)
{
    BOOL backward = json_boolean_value(json_object_get(match_cond, "backward"));

    *prowid = -1;
    size_t segments_size = json_array_size(segments);
    if(segments_size == 0) {
        // Here silence, avoid multiple logs, only logs in first/last
        return -1;
    }

    json_int_t rowid;

    if(!backward) {
        json_int_t from_rowid = json_integer_value(json_object_get(match_cond, "from_rowid"));

        // WARNING adjust REPEATED
        if(from_rowid == 0) {
            from_rowid = 1;
        } else if(from_rowid > 0) {
            // positive offset
            if(from_rowid > total_rows) {
                // not exist
                return -1;
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

        rowid = from_rowid;

    } else {
        json_int_t to_rowid = json_integer_value(json_object_get(match_cond, "to_rowid"));

        // WARNING adjust REPEATED
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
                return -1;
            } else {
                to_rowid = total_rows + to_rowid + 1;
            }
        }

        rowid = to_rowid;
    }

    // TODO   if(system_flag & sf_deleted_record) {
    //        return tranger_prev_record(tranger, topic, md_record);
    //    }

    int idx; json_t *segment;
    if(!backward) {
        json_array_foreach(segments, idx, segment) {
            json_int_t seg_first_rowid = json_integer_value(json_object_get(segment, "first_row"));
            json_int_t seg_last_rowid = json_integer_value(json_object_get(segment, "last_row"));
//            json_int_t seg_from_t = json_integer_value(json_object_get(segment, "fr_t"));
//            json_int_t seg_to_t = json_integer_value(json_object_get(segment, "to_t"));
//            json_int_t seg_from_tm = json_integer_value(json_object_get(segment, "fr_tm"));
//            json_int_t seg_to_tm = json_integer_value(json_object_get(segment, "to_tm"));

            do {
                if(!(seg_first_rowid <= rowid && rowid <= seg_last_rowid)) {
                    // no match, break and continue
                    break;
                }

                // Match
                *prowid = rowid;
                return idx;
            } while(0);
        }

    } else {
        json_array_backward(segments, idx, segment) {
            json_int_t seg_first_rowid = json_integer_value(json_object_get(segment, "first_row"));
            json_int_t seg_last_rowid = json_integer_value(json_object_get(segment, "last_row"));
//            json_int_t seg_from_t = json_integer_value(json_object_get(segment, "fr_t"));
//            json_int_t seg_to_t = json_integer_value(json_object_get(segment, "to_t"));
//            json_int_t seg_from_tm = json_integer_value(json_object_get(segment, "fr_tm"));
//            json_int_t seg_to_tm = json_integer_value(json_object_get(segment, "to_tm"));

            do {
                if(!(seg_first_rowid <= rowid && rowid <= seg_last_rowid)) {
                    // no match, break and continue
                    break;
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
                gobj_log_error(0, 0,
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
                gobj_log_error(0, 0,
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


/**rst**
    Get metadata/record in iterator that firstly match match_cond
**rst**/
PUBLIC int tranger2_iterator_find(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    json_t *match_cond,  // owned
    md2_record_t *md_record,
    json_t **record
);

/**rst**
    Get metadata/record of first row in iterator
**rst**/
PUBLIC int tranger2_iterator_first(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_t *md_record,
    json_t **record
);

/**rst**
    Get metadata/record of next row in iterator
**rst**/
PUBLIC int tranger2_iterator_next(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_t *md_record,
    json_t **record
);

/**rst**
    Get metadata/record of previous row in iterator
**rst**/
PUBLIC int tranger2_iterator_prev(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_t *md_record,
    json_t **record
);

/**rst**
    Get metadata/record of last row in iterator
**rst**/
PUBLIC int tranger2_iterator_last(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_t *md_record,
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
    md2_record_t *md_record,
    json_t **record
)
{
    // TODO review
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
//     *  Save the pointer TODO no debería salvarlo después de get_md_.. ok?
//     *  TODO review ALL
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
//    // TODO   if(system_flag & sf_deleted_record) {
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
    md2_record_t *md_record,
    json_t **record
)
{
    // TODO review
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    const char *topic_name = kw_get_str(gobj, iterator, "topic_name", "", KW_REQUIRED);
    json_t *topic = tranger2_topic(tranger, topic_name);
    const char *key = json_string_value(json_object_get(iterator, "key"));

    /*
     *  Check parameters
     */
    if(empty_string(topic_name)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "no topic",
            NULL
        );
        return -1;
    }
    if(empty_string(key)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "no key",
            NULL
        );
        return -1;
    }

    if(!md_record) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "md_record NULL",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            NULL
        );
        return -1;
    } else {
        memset(md_record, 0, sizeof(md2_record_t));
    }

    if(record) {
        *record = NULL;
    }

    /*
     *  Get segments
     */
    json_t *segments = json_object_get(iterator, "segments");
    if(json_array_size(segments)==0) {
        // Here silence, avoid multiple logs, only logs in first/last
        return -1;
    }

    /*
     *  Get the pointer (cur_segment, cur_rowid)
     */
    json_int_t cur_segment = 0;
    json_t *segment = json_array_get(segments, cur_segment);
    json_int_t cur_rowid = kw_get_int(gobj, segment, "first_row", 0, KW_REQUIRED);

    /*
     *  Save the pointer
     *  TODO review ALL
     */
    json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
    json_object_set_new(iterator, "cur_rowid", json_integer(cur_rowid));

    /*
     *  Get the metadata
     */
    if(get_md_by_rowid(
        gobj,
        tranger,
        topic,
        key,
        segment,
        cur_rowid,
        md_record
    )<0) {
        return -1;
    }

    // TODO   if(system_flag & sf_deleted_record) {
    //        return tranger_next_record(tranger, topic, md_record);
    //    }

    if(rowid) {
        *rowid = cur_rowid;
    }

    if(record) {
        const char *file_id = json_string_value(json_object_get(segment, "id"));
        *record = read_record_content(
            tranger,
            topic,
            key,
            file_id,
            md_record
        );
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int tranger2_iterator_next(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_t *md_record,
    json_t **record
)
{
    // TODO review
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    const char *topic_name = kw_get_str(gobj, iterator, "topic_name", "", KW_REQUIRED);
    json_t *topic = tranger2_topic(tranger, topic_name);
    const char *key = json_string_value(json_object_get(iterator, "key"));

    /*
     *  Check parameters
     */
    if(empty_string(topic_name)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "no topic",
            NULL
        );
        return -1;
    }
    if(empty_string(key)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "no key",
            NULL
        );
        return -1;
    }

    if(!md_record) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "md_record NULL",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            NULL
        );
        return -1;
    } else {
        memset(md_record, 0, sizeof(md2_record_t));
    }

    if(record) {
        *record = NULL;
    }

    /*
     *  Get segments
     */
    json_t *segments = kw_get_list(gobj, iterator, "segments", 0, KW_REQUIRED);
    size_t segments_size = json_array_size(segments);
    if(segments_size == 0) {
        // Here silence, avoid multiple logs, only logs in first/last
        return -1;
    }

    /*
     *  Get the pointer (cur_segment, cur_rowid)
     */
    json_int_t cur_segment = kw_get_int(gobj, iterator, "cur_segment", 0, KW_REQUIRED);
    json_t *segment = json_array_get(segments, cur_segment);
    json_int_t cur_rowid = kw_get_int(gobj, iterator, "cur_rowid", 0, KW_REQUIRED);

    /*
     *  Increment rowid
     */
    cur_rowid++;

    /*
     *  Check if is in the same segment, if not then go to the next segment
     */
    json_int_t segment_last_row = kw_get_int(gobj, segment, "last_row", 0, KW_REQUIRED);
    if(cur_rowid > segment_last_row) {
        // Go to the next segment
        cur_segment++;
        if(cur_segment >= segments_size) {
            // No more
            return -1;
        }
        segment = json_array_get(segments, cur_segment);
        json_int_t segment_first_row = kw_get_int(gobj, segment, "first_row", 0, KW_REQUIRED);
        if(cur_rowid != segment_first_row) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "next rowids not consecutive",
                "topic_name",   "%s", tranger2_topic_name(topic),
                "key",          "%s", key,
                "cur_rowid",    "%ld", (long)cur_rowid,
                "segment_first","%ld", (long)segment_first_row,
                NULL
            );
            gobj_trace_json(gobj, iterator, "next rowids not consecutive");
            return -1;
        }
    }

    /*
     *  Save the pointer
     *  TODO review ALL
     */
    json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
    json_object_set_new(iterator, "cur_rowid", json_integer(cur_rowid));

    /*
     *  Get the metadata
     */
    if(get_md_by_rowid(
        gobj,
        tranger,
        topic,
        key,
        segment,
        cur_rowid,
        md_record
    )<0) {
        return -1;
    }

    // TODO   if(system_flag & sf_deleted_record) {
    //        return tranger_prev_record(tranger, topic, md_record);
    //    }

    if(rowid) {
        *rowid = cur_rowid;
    }

    if(record) {
        const char *file_id = json_string_value(json_object_get(segment, "id"));
        *record = read_record_content(
            tranger,
            topic,
            key,
            file_id,
            md_record
        );
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int tranger2_iterator_prev(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_t *md_record,
    json_t **record
)
{
    // TODO review
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    const char *topic_name = kw_get_str(gobj, iterator, "topic_name", "", KW_REQUIRED);
    json_t *topic = tranger2_topic(tranger, topic_name);
    const char *key = json_string_value(json_object_get(iterator, "key"));

    /*
     *  Check parameters
     */
    if(empty_string(topic_name)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "no topic",
            NULL
        );
        return -1;
    }
    if(empty_string(key)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "no key",
            NULL
        );
        return -1;
    }

    if(!md_record) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "md_record NULL",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            NULL
        );
        return -1;
    } else {
        memset(md_record, 0, sizeof(md2_record_t));
    }

    if(record) {
        *record = NULL;
    }

    /*
     *  Get segments
     */
    json_t *segments = kw_get_list(gobj, iterator, "segments", 0, KW_REQUIRED);
    size_t segments_size = json_array_size(segments);
    if(segments_size == 0) {
        // Here silence, avoid multiple logs, only logs in first/last
        return -1;
    }

    /*
     *  Get the pointer (cur_segment, cur_rowid)
     */
    json_int_t cur_segment = kw_get_int(gobj, iterator, "cur_segment", 0, KW_REQUIRED);
    json_t *segment = json_array_get(segments, cur_segment);
    json_int_t cur_rowid = kw_get_int(gobj, iterator, "cur_rowid", 0, KW_REQUIRED);

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
    json_int_t segment_first_row = kw_get_int(gobj, segment, "first_row", 0, KW_REQUIRED);
    if(cur_rowid < segment_first_row) {
        // Go to the previous segment
        cur_segment--;
        if(cur_segment < 0) {
            // No more
            return -1;
        }
        segment = json_array_get(segments, cur_segment);

        json_int_t segment_last_row = kw_get_int(gobj, segment, "last_row", 0, KW_REQUIRED);
        if(cur_rowid != segment_last_row) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "previous rowids not consecutive",
                "topic_name",   "%s", tranger2_topic_name(topic),
                "key",          "%s", key,
                "cur_rowid",    "%ld", (long)cur_rowid,
                "segment_first","%ld", (long)segment_last_row,
                NULL
            );
            gobj_trace_json(gobj, iterator, "previous rowids not consecutive");
            return -1;
        }
    }

    /*
     *  Save the pointer
     *  TODO review ALL
     */
    json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
    json_object_set_new(iterator, "cur_rowid", json_integer(cur_rowid));

    /*
     *  Get the metadata
     */
    if(get_md_by_rowid(
        gobj,
        tranger,
        topic,
        key,
        segment,
        cur_rowid,
        md_record
    )<0) {
        return -1;
    }

    // TODO   if(system_flag & sf_deleted_record) {
    //        return tranger_next_record(tranger, topic, md_record);
    //    }

    if(rowid) {
        *rowid = cur_rowid;
    }

    if(record) {
        const char *file_id = json_string_value(json_object_get(segment, "id"));
        *record = read_record_content(
            tranger,
            topic,
            key,
            file_id,
            md_record
        );
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int tranger2_iterator_last(
    json_t *tranger,
    json_t *iterator,
    json_int_t *rowid,
    md2_record_t *md_record,
    json_t **record
)
{
    // TODO review
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    const char *topic_name = kw_get_str(gobj, iterator, "topic_name", "", KW_REQUIRED);
    json_t *topic = tranger2_topic(tranger, topic_name);
    const char *key = json_string_value(json_object_get(iterator, "key"));

    /*
     *  Check parameters
     */
    if(empty_string(topic_name)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "no topic",
            NULL
        );
        return -1;
    }
    if(empty_string(key)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "no key",
            NULL
        );
        return -1;
    }

    if(!md_record) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "md_record NULL",
            "topic_name",   "%s", tranger2_topic_name(topic),
            "key",          "%s", key,
            NULL
        );
        return -1;
    } else {
        memset(md_record, 0, sizeof(md2_record_t));
    }

    if(record) {
        *record = NULL;
    }

    /*
     *  Get segments
     */
    json_t *segments = json_object_get(iterator, "segments");
    if(json_array_size(segments)==0) {
        // Here silence, avoid multiple logs, only logs in first/last
        return -1;
    }

    /*
     *  Get the pointer (cur_segment, cur_rowid)
     */
    json_int_t cur_segment = (json_int_t)json_array_size(segments) - 1;
    json_t *segment = json_array_get(segments, cur_segment);
    json_int_t cur_rowid = kw_get_int(gobj, segment, "last_row", 0, KW_REQUIRED);

    /*
     *  Save the pointer
     *  TODO review ALL
     */
    json_object_set_new(iterator, "cur_segment", json_integer(cur_segment));
    json_object_set_new(iterator, "cur_rowid", json_integer(cur_rowid));

    /*
     *  Get the metadata
     */
    if(get_md_by_rowid(
        gobj,
        tranger,
        topic,
        key,
        segment,
        cur_rowid,
        md_record
    )<0) {
        return -1;
    }

    // TODO   if(system_flag & sf_deleted_record) {
    //        return tranger_prev_record(tranger, topic, md_record);
    //    }

    if(rowid) {
        *rowid = cur_rowid;
    }

    if(record) {
        const char *file_id = json_string_value(json_object_get(segment, "id"));
        *record = read_record_content(
            tranger,
            topic,
            key,
            file_id,
            md_record
        );
    }

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
    return kw_get_int(gobj, topic, path, 0, KW_REQUIRED);
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
    md2_record_t *md_record
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
        md_record
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
    md2_record_t *md_record
)
{
    /*
     *  TODO Find in the cache the range md
     */

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

    off64_t offset = (off64_t) (rowid * sizeof(md2_record_t));
    off64_t offset_ = lseek64(fd, offset, SEEK_SET);
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
        md_record,
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

    md_record->__t__ = ntohll(md_record->__t__);
    md_record->__tm__ = ntohll(md_record->__tm__);
    md_record->__offset__ = ntohll(md_record->__offset__);
    md_record->__size__ = ntohll(md_record->__size__);

    return 0;
}

/***************************************************************************
 *   Read record data
 ***************************************************************************/
PRIVATE json_t *read_record_content(
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *file_id,
    md2_record_t *md_record
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

    off64_t offset = (off64_t)md_record->__offset__;
    off64_t offset_ = lseek64(fd, offset, SEEK_SET);
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

    gbuffer_t *gbuf = gbuffer_create(md_record->__size__, md_record->__size__);
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
        md_record->__size__
    );

    if(ln != md_record->__size__) {
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

//    if(system_flag & sf_cipher_record) {
//        // if(topic->decrypt_callback) { TODO
//        //     gbuf = topic->decrypt_callback(
//        //         topic->user_data,
//        //         topic,
//        //         gbuf    // must be owned
//        //     );
//        // }
//    }
//    if(system_flag & sf_zip_record) {
//        // if(topic->decompress_callback) { TODO
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
                "__t__",        "%lu", (unsigned long)md_record->__t__,
                "__size__",     "%lu", (unsigned long)md_record->__size__,
                "__offset__",   "%lu", (unsigned long)md_record->__offset__,
                NULL
            );
            return NULL;
        }
    }

    return jn_record;
}

/***************************************************************************
 *  Print rowid, t, tm, key
 ***************************************************************************/
PUBLIC void tranger2_print_md0_record(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    const char *key,
    json_int_t rowid,
    char *bf,
    int bfsize
)
{
    char fecha[90];
    char stamp[64];

    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));

    if(system_flag & sf2_t_ms) {
        time_t t = (time_t)md_record->__t__;
        t /= 1000;
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%S%z", localtime(&t));
    } else {
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%S%z",
            localtime((time_t *)&md_record->__t__)
        );
    }

    char fecha_tm[80];
    if(system_flag & sf2_tm_ms) {
        time_t t_m = (time_t)md_record->__tm__;
        t_m /= 1000;
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S%z", gmtime(&t_m));
    } else {
        strftime(fecha_tm, sizeof(fecha_tm), "%Y-%m-%dT%H:%M:%S%z",
            gmtime((time_t *)&md_record->__tm__)
        );
    }

    system_flag2_t key_type = system_flag & KEY_TYPE_MASK2;

    if(key_type & (sf2_int_key|sf2_string_key)) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key: %s",
            (uint64_t)rowid,
            (uint64_t)md_record->__t__,
            fecha,
            (uint64_t)md_record->__tm__,
            fecha_tm,
            key
        );
    } else {
        gobj_log_error(0, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD metadata, without key type",
            "topic",        "%s", tranger2_topic_name(topic),
            NULL
        );
    }
}

/***************************************************************************
 *  Print rowid, uflag, sflag, t, tm, key
 ***************************************************************************/
PUBLIC void tranger2_print_md1_record(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    const char *key,
    json_int_t rowid,
    char *bf,
    int bfsize
)
{
    struct tm *tm;
    char fecha[90];
    char stamp[64];

    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));
    unsigned user_flag = 0; // TODO

    if(system_flag & sf2_t_ms) {
        time_t t = (time_t)md_record->__t__;
        unsigned ms = t % 1000;
        t /= 1000;
        tm = gmtime(&t);
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", tm);
        snprintf(fecha, sizeof(fecha), "%s.%03uZ", stamp, ms);
    } else {
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%SZ", gmtime((time_t *)&md_record->__t__));
    }

    char fecha_tm[80];
    if(system_flag & sf2_tm_ms) {
        time_t t_m = (time_t)md_record->__tm__;
        unsigned ms = t_m % 1000;
        time_t t_m_ = t_m/1000;
        tm = gmtime(&t_m_);
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", tm);
        snprintf(fecha_tm, sizeof(fecha_tm), "%s.%03uZ", stamp, ms);
    } else {
        strftime(fecha_tm, sizeof(fecha_tm), "%Y-%m-%dT%H:%M:%SZ", gmtime((time_t *)&md_record->__tm__));
    }

    system_flag2_t key_type = system_flag & KEY_TYPE_MASK2;

    if(key_type & (sf2_int_key|sf2_string_key)) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "uflag:0x%"PRIX32", sflag:0x%"PRIX32", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key: %s",
            (uint64_t)rowid,
            user_flag,
            system_flag,
            (uint64_t)md_record->__t__,
            fecha,
            (uint64_t)md_record->__tm__,
            fecha_tm,
            key
        );
    } else {
        gobj_log_error(0, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD metadata, without key type",
            "topic",        "%s", tranger2_topic_name(topic),
            NULL
        );
    }
}

/***************************************************************************
 *  print rowid, offset, size, t, path
 ***************************************************************************/
PUBLIC void tranger2_print_md2_record(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    const char *key,
    json_int_t rowid,
    char *bf,
    int bfsize
)
{
    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));

    time_t t = (time_t)md_record->__t__;
    uint64_t offset = md_record->__offset__;
    uint64_t size = md_record->__size__;

    char path[PATH_MAX];
    get_t_filename(
        bf,
        bfsize,
        tranger,
        topic,
        TRUE,   // TRUE for data, FALSE for md2
        (system_flag & sf2_t_ms)? t/1000:t  // WARNING must be in seconds!
    );

    snprintf(bf, bfsize,
        "rowid:%"PRIu64", ofs:%"PRIu64", sz:%-5"PRIu64", "
        "t:%"PRIu64", "
        "f:%s",
        (uint64_t)rowid,
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
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    char *bf,
    int bfsize
)
{
    system_flag2_t system_flag = json_integer_value(json_object_get(topic, "system_flag"));

    time_t t = (time_t)md_record->__t__;
    get_t_filename(
        bf,
        bfsize,
        tranger,
        topic,
        TRUE,   // TRUE for data, FALSE for md2
        (system_flag & sf2_t_ms)? t/1000:t  // WARNING must be in seconds!
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
