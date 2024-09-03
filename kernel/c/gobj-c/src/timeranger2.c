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
#include <sys/stat.h>
#include <inttypes.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <glob.h>
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

PRIVATE const char *sf_names[24+1] = {
    "sf2_string_key",           // 0x000001
    "sf2_rowid_key",            // 0x000002
    "sf2_int_key",              // 0x000004
    "",                         // 0x000008
    "sf2_zip_record",           // 0x000010
    "sf2_cipher_record",        // 0x000020
    "",                         // 0x000040
    "",                         // 0x000080
    "sf2_t_ms",                 // 0x000100
    "sf2_tm_ms",                // 0x000200
    "",                         // 0x000400
    "",                         // 0x000800
    "sf2_no_record_disk",       // 0x001000
    "sf2_no_md_disk",           // 0x002000
    "",                         // 0x004000
    "",                         // 0x008000
    "sf2_loading_from_disk",    // 0x010000
    "sf2_soft_deleted_record",  // 0x020000
    "",                         // 0x040000
    "",                         // 0x080000
    "",                         // 0x100000
    "",                         // 0x200000
    "",                         // 0x400000
    "sf2_hard_deleted_record",  // 0x800000
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

PRIVATE int json_array_find_idx(json_t *jn_list, json_t *item);
PRIVATE json_t *find_keys_in_disk(
    hgobj gobj,
    const char *directory,
    json_t *match_cond  // not owned
);
PRIVATE int load_topic_metadata(
    hgobj gobj,
    const char *directory,
    json_t *topic_cache,    // not owned
    json_t *jn_keys         // not owned
);
PRIVATE json_t *get_time_range(hgobj gobj, const char *directory, const char *key);

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *  Startup TimeRanger database
 ***************************************************************************/
PUBLIC json_t *tranger2_startup(
    hgobj gobj,
    json_t *jn_tranger // owned, See tranger2_json_desc for parameters
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

    return tranger;
}

/***************************************************************************
 *  Shutdown TimeRanger database
 ***************************************************************************/
PUBLIC int tranger2_shutdown(json_t *tranger)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
    BOOL master = kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED);

    /*-------------------------------*
     *      Some checks
     *-------------------------------*/
    if(!jn_cols) {
        jn_cols = json_object();
    }
    if(!jn_var) {
        jn_var = json_object();
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

//        /*----------------------------------------*
//         *      Create topic_idx.md
//         *----------------------------------------*/
// TODO ya no existen el index by topic, ahora es idx by key
//        char full_path[PATH_MAX];
//        snprintf(full_path, sizeof(full_path), "%s/%s",
//            directory,
//            "topic_idx.md"
//        );
//        int fp = newfile(full_path, (int)kw_get_int(gobj, tranger, "rpermission", 0, KW_REQUIRED), FALSE);
//        if(fp < 0) {
//            gobj_log_error(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
//                "function",     "%s", __FUNCTION__,
//                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
//                "msg",          "%s", "Cannot create topic_idx.md file.",
//                "filename",     "%s", full_path,
//                "errno",        "%s", strerror(errno),
//                NULL
//            );
//            JSON_DECREF(jn_cols)
//            JSON_DECREF(jn_var)
//        JSON_DECREF(jn_topic_ext)
//            return 0;
//        }
//        close(fp);
//
//        gobj_log_info(gobj, 0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_INFO,
//            "msg",          "%s", "Creating topic_idx.md",
//            "path",         "%s", directory,
//            "topic",        "%s", topic_name,
//            NULL
//        );

        /*----------------------------------------*
         *      Create topic_desc.json
         *----------------------------------------*/
        json_t *jn_topic_desc = json_object();
        kw_get_str(gobj, jn_topic_desc, "topic_name", topic_name, KW_CREATE);
        kw_get_str(gobj, jn_topic_desc, "pkey", pkey, KW_CREATE);
        kw_get_str(gobj, jn_topic_desc, "tkey", tkey, KW_CREATE);

        system_flag &= ~NOT_INHERITED_MASK2;
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

    } else {
        /*---------------------------------------------*
         *  Exists the directory but check
         *      topic_var.json
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
                0 // TODO review
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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

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
    kw_get_dict(gobj, topic, "lists", json_array(), KW_CREATE);
    kw_get_dict(gobj, topic, "cache", json_object(), KW_CREATE);

    /*-----------------------------------------*
     *      Load keys and metadata from disk
     *-----------------------------------------*/
    // TODO this cache must be update in tranger2_append_record() or open_list() ???!!!
    json_t *jn_keys = find_keys_in_disk(gobj, directory, NULL);
    json_t *topic_cache = kw_get_dict(gobj, topic, "cache", 0, KW_REQUIRED);
    if(!topic_cache) {
        json_decref(jn_keys);
        return NULL;
    }
    load_topic_metadata(
        gobj,
        directory,
        topic_cache,    // not owned
        jn_keys         // not owned
    );
    json_decref(jn_keys);

    /*
     *  Open topic index
     */
// TODO no idx in topic, moved to keys
//    system_flag2_t system_flag = kw_get_int(gobj, topic, "system_flag", 0, KW_REQUIRED);
//    if(!(system_flag & sf2_no_md_disk)) {
//        if(open_topic_idx_fd(tranger, topic)<0) {
//            json_decref(topic);
//            return 0;
//        }
//    }

    return topic;
}

/***************************************************************************
   Get topic by his topic_name.
   Topic is opened if it's not opened.
   HACK topic can exists in disk, but it's not opened until tranger_open_topic()
 ***************************************************************************/
PUBLIC json_t *tranger2_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, 0);
    if(!topic) {
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
   Get topic size (number of records)
 ***************************************************************************/
PUBLIC json_int_t tranger2_topic_size(
    json_t *topic
)
{
    // TODO
    return kw_get_int(0, topic, "__last_rowid__", 0, KW_REQUIRED);
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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

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

    // TODO no debería chequear si hay alguna lista abierta usando el topic???

    close_fd_opened_files(gobj, topic, NULL);

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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

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

    BOOL master = kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED);
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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

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

    BOOL master = kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED);
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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

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

    // TODO json_t *cols = kwid_new_list("verbose", topic, "cols");
//    json_object_set_new(desc, "cols", cols);

    return desc;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *tranger2_list_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
)
{
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        // Error already logged
        return 0;
    }
//TODO    return kwid_new_list("verbose", topic, "cols");
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *tranger2_dict_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
)
{
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        // Error already logged
        return 0;
    }
// TODO   return kwid_new_dict("verbose", topic, "cols");
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *tranger2_filter_topic_fields(
    json_t *tranger,
    const char *topic_name,
    json_t *kw  // owned
)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

    json_t *cols = tranger2_dict_topic_desc(tranger, topic_name);
    if(!cols) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic without cols",
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }
    json_t *new_record = json_object();

    const char *field; json_t *col;
    json_object_foreach(cols, field, col) {
        json_t *value = kw_get_dict_value(gobj, kw, field, 0, 0);
        json_object_set(new_record, field, value);
    }

    JSON_DECREF(cols)
    JSON_DECREF(kw)
    return new_record;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int open_topic_idx_fd(json_t *tranger, json_t *topic)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
    BOOL master = kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED);

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s",
        kw_get_str(gobj, topic, "directory", "", KW_REQUIRED),
        "topic_idx.md"
    );
    int fd;
    if(master) {
        fd = open(full_path, O_RDWR|O_LARGEFILE|O_NOFOLLOW, 0);
    } else {
        fd = open(full_path, O_RDONLY|O_LARGEFILE, 0);
    }
    if(fd<0) {
        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot open TimeRanger topic_idx fd",
            "path",         "%s", full_path,
            "errno",        "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    json_object_set_new(topic, "topic_idx_fd", json_integer(fd));

    /*
     *  Get last rowid
     */
    off64_t offset = lseek64(fd, 0, SEEK_END);
    if(offset < 0) {
        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "topic_idx.md corrupted",
            "topic",        "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
            "offset",       "%lu", (unsigned long)offset,
            NULL
        );
        return -1;
    }
    uint64_t last_rowid = offset/sizeof(md2_record_t);
    json_object_set_new(
        topic,
        "__last_rowid__",
        json_integer((json_int_t)last_rowid)
    );

    return 0;
}

/***************************************************************************
 *  Get fullpath of filename in content or md2 level
 *  The directory will be create if it's master
 ***************************************************************************/
PRIVATE char *get_t_filename(
    hgobj gobj,
    char *bf,
    int bfsize,
    json_t *tranger,
    json_t *topic,
    BOOL for_data,  // TRUE for data, FALSE for md2
    uint64_t __t__ // WARNING must be in seconds!
)
{
    struct tm *tm = gmtime((time_t *)&__t__);

    char format[NAME_MAX];
    const char *filename_mask = kw_get_str(
        gobj,
        topic,
        "filename_mask",
        kw_get_str(gobj, tranger, "filename_mask", "%Y-%m-%d", KW_REQUIRED),
        0
    );

    if(strchr(filename_mask, '%')) {
        strftime(format, sizeof(format), filename_mask, tm);
    } else {
        // HACK backward compatibility
        char sfechahora[64];
        /* Pon en formato DD/MM/CCYY-ZZZ-HH (day/month/year-yearday-hour) */
        snprintf(sfechahora, sizeof(sfechahora), "%02d/%02d/%4d/%03d/%02d",
            tm->tm_mday,            // 01-31
            tm->tm_mon+1,           // 01-12
            tm->tm_year + 1900,
            tm->tm_yday+1,          // 001-365
            tm->tm_hour
        );
        translate_string(format, sizeof(format), sfechahora, filename_mask, "DD/MM/CCYY/ZZZ/HH");
    }

    snprintf(bf, bfsize, "%s.%s",
        format,
        for_data?"json":"md2"
    );

    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_topic_fd(
    json_t *tranger,
    json_t *topic,
    const char *key,
    BOOL for_data,
    BOOL for_write,
    uint64_t __t__
)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
    system_flag2_t system_flag = kw_get_int(gobj, topic, "system_flag", 0, KW_REQUIRED);
    if((system_flag & sf2_no_record_disk)) {
        return -1;
    }

    if(empty_string(key)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "topic",        "%s", topic,
            "msg",          "%s", "key is empty",
            NULL
        );
        return -1;
    }

    BOOL master = kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED);

    /*------------------------------*
     *      Check key directory
     *      create it not exist
     *------------------------------*/
    const char *topic_dir = kw_get_str(gobj, topic, "directory", "", KW_REQUIRED);
    char path_key[PATH_MAX];
    snprintf(path_key, sizeof(path_key), "%s/%s",
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

    /*-----------------------------*
     *      Check file
     *-----------------------------*/
    char filename[NAME_MAX];
    get_t_filename(
        gobj,
        filename,
        sizeof(filename),
        tranger,
        topic,
        for_data,
        (system_flag & sf2_t_ms)? __t__/1000:__t__
    );
    char full_path[PATH_MAX];
    build_path(full_path, sizeof(full_path), topic_dir, key, filename, NULL);
    if(access(full_path, 0)!=0) {
        /*----------------------------------------*
         *  Create (only)the new file if master
         *----------------------------------------*/
        if(!master || !for_write) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "path",         "%s", full_path,
                "msg",          "%s", "file not found",
                NULL
            );
            return -1;
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
        close(fp);
    }

    /*-----------------------------*
     *      Open content file
     *-----------------------------*/
    char relative_path[PATH_MAX];
    snprintf(relative_path, sizeof(relative_path), "%s`%s", key, filename);
    int fd = (int)kw_get_int(
        gobj,
        kw_get_dict(
            gobj,
            topic,
            for_write?"wr_fd_files":"rd_fd_files",
            0,
            KW_REQUIRED
        ),
        relative_path,
        -1,
        0
    );

    if(fd<0) {
        if(master) {
            fd = open(full_path, O_RDWR|O_LARGEFILE|O_NOFOLLOW, 0);
        } else {
            fd = open(full_path, O_RDONLY|O_LARGEFILE, 0);
        }
        if(fd<0) {
            gobj_log_critical(gobj,
                master?kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED):0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot open file",
                "path",         "%s", full_path,
                "errno",        "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        kw_set_dict_value(
            gobj,
            kw_get_dict(
                gobj,
                topic,
                for_write?"wr_fd_files":"rd_fd_files",
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
PRIVATE int _close_fd_files(
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
    return _close_fd_files(gobj, fd_files, key);
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
    return _close_fd_files(gobj, fd_files, key);
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
PUBLIC json_t *tranger2_md2json(md2_record_t *md_record)
{
    json_t *jn_md = json_object();
    // TODO
//    json_object_set_new(jn_md, "__rowid__", json_integer(md_record->__rowid__));
//    json_object_set_new(jn_md, "__t__", json_integer(md_record->__t__));
//    json_object_set_new(jn_md, "__tm__", json_integer(md_record->__tm__));
//    json_object_set_new(jn_md, "__offset__", json_integer(md_record->__offset__));
//    json_object_set_new(jn_md, "__size__", json_integer(md_record->__size__));
//    json_object_set_new(jn_md, "__user_flag__", json_integer(md_record->__user_flag__));
//    json_object_set_new(jn_md, "__system_flag__", json_integer(md_record->__system_flag__));

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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
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

    BOOL master = kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED);
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
    uint32_t __system_flag__ = kw_get_int(gobj, topic, "system_flag", 0, KW_REQUIRED);
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

    /*-----------------------------------*
     *  Get the primary-key
     *-----------------------------------*/
    const char *pkey = kw_get_str(gobj, topic, "pkey", "", KW_REQUIRED);
    system_flag2_t system_flag_key_type = __system_flag__ & KEY_TYPE_MASK2;

    const char *key_value = NULL;
    char key_int[NAME_MAX+1];

    switch(system_flag_key_type) {
        case sf2_string_key:
            {
                key_value = kw_get_str(gobj, jn_record, pkey, 0, 0);
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
                uint64_t i = (uint64_t)kw_get_int(
                    gobj,
                    jn_record,
                    pkey,
                    0,
                    KW_REQUIRED|KW_WILD_NUMBER
                );
                if(!i) {
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

    /*--------------------------------------------*
     *  Get and save the t-key if exists
     *--------------------------------------------*/
    const char *tkey = kw_get_str(gobj, topic, "tkey", "", KW_REQUIRED);
    if(!empty_string(tkey)) {
        json_t *jn_tval = kw_get_dict_value(gobj, jn_record, tkey, 0, 0);
        if(!jn_tval) {
            md_record->__tm__ = 0; // No tkey value, mark with 0
        } else {
            if(json_is_string(jn_tval)) {
                int offset;
                timestamp_t timestamp; // TODO if is it a millisecond time?
                parse_date_basic(json_string_value(jn_tval), &timestamp, &offset);
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

    /*------------------------------------------------------*
     *  Save content, to file
     *------------------------------------------------------*/
    int content_fp = get_topic_fd(tranger, topic, key_value, TRUE, TRUE, __t__);  // Can be -1, if sf_no_disk

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
//        if(md_record->__system_flag__ & sf_zip_record) {
//            // if(topic->compress_callback) { TODO
//            //     gbuf = topic->compress_callback(
//            //         topic,
//            //         srecord
//            //     );
//            // }
//        }
//        if(md_record->__system_flag__ & sf_cipher_record) {
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

    /*--------------------------------------------*
     *  Save md, to file
     *--------------------------------------------*/
    int md2_fp = get_topic_fd(tranger, topic, key_value, FALSE, TRUE, __t__);  // Can be -1, if sf_no_disk
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

        md2_record_t big_endian;
        big_endian.__t__ = htonll(md_record->__t__);
        big_endian.__tm__ = htonll(md_record->__tm__);
        big_endian.__offset__ = htonll(md_record->__offset__);
        big_endian.__size__ = htonll(md_record->__size__);

        size_t ln = write( // write new (record content)
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

    /*--------------------------------------------*
     *  Call callbacks
     *--------------------------------------------*/
    json_t *lists = kw_get_list(gobj, topic, "lists", 0, KW_REQUIRED);
    int idx;
    json_t *list;
    json_array_foreach(lists, idx, list) {
        if(tranger2_match_record(
                tranger,
                topic,
                kw_get_dict(gobj, list, "match_cond", 0, 0),
                md_record,
                0
            )) {
            tranger2_load_record_callback_t load_record_callback =
                (tranger2_load_record_callback_t)(size_t)kw_get_int(
                gobj,
                list,
                "load_record_callback",
                0,
                0
            );
            if(load_record_callback) {
                // Inform to the user list: record in real time
                JSON_INCREF(jn_record)
                int ret = load_record_callback(
                    tranger,
                    topic,
                    list,
                    md_record,
                    jn_record
                );
                if(ret < 0) {
                    JSON_DECREF(jn_record)
                    return -1;
                } else if(ret>0) {
                    json_object_set_new(jn_record, "__md_tranger__", tranger2_md2json(md_record));
                    json_array_append(
                        kw_get_list(gobj, list, "data", 0, KW_REQUIRED),
                        jn_record
                    );
                }
            } else {
                json_object_set_new(jn_record, "__md_tranger__", tranger2_md2json(md_record));
                json_array_append(
                    kw_get_list(gobj, list, "data", 0, KW_REQUIRED),
                    jn_record
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
//// TODO   if(md_record->__rowid__ != rowid) {
////        log_error(0,
////            "function",     "%s", __FUNCTION__,
////            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
////            "msg",          "%s", "md_record corrupted, item rowid not match",
////            "topic",        "%s", tranger2_topic_name(topic),
////            "rowid",        "%lu", (unsigned long)rowid,
////            "__rowid__",    "%lu", (unsigned long)md_record->__rowid__,
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
//// TODO   off64_t offset = (off64_t) ((md_record->__rowid__-1) * sizeof(md2_record_t));
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
//    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
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
//    int fd = get_topic_fd(tranger, topic, "TODO", TRUE, md_record.__t__); // TODO
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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
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
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
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
    Read records
 ***************************************************************************/
PUBLIC json_t *tranger2_open_list(
    json_t *tranger,
    json_t *jn_list // owned
)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
    char title[256];

    json_t *list = create_json_record(gobj, list_json_desc);
    json_object_update(list, jn_list);
    JSON_DECREF(jn_list)

    BOOL master = kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED);

    /*
     *  Here the topic is opened if it's not opened
     */
    const char *topic_name = kw_get_str(gobj, list, "topic_name", "", KW_REQUIRED);
    json_t *topic = tranger2_topic(tranger, topic_name);
    if(!topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger_open_list: what topic?",
            NULL
        );
        JSON_DECREF(list)
        return 0;
    }

    json_t *match_cond = kw_get_dict(gobj, list, "match_cond", 0, KW_REQUIRED);

    int trace_level = (int)kw_get_int(gobj, tranger, "trace_level", 0, 0);

    tranger2_load_record_callback_t load_record_callback =
        (tranger2_load_record_callback_t)(size_t)kw_get_int(
        gobj,
        list,
        "load_record_callback",
        0,
        0
    );

    /*
     *  Add the list to the topic
     */
    json_array_append_new(
        kw_get_dict_value(gobj, topic, "lists", 0, KW_REQUIRED),
        list
    );

    /*
     *  Load volatil, defining in run-time
     */
    json_t *data = kw_get_list(gobj, list, "data", json_array(), KW_CREATE);

    /*
     *  Load from disk
     */
    // TODO esto no debe estar aquí, en topic_open(), y actualizado con append_record()
    const char *directory = kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED);
    json_t *jn_keys = find_keys_in_disk(gobj, directory, match_cond);
    json_t *topic_cache = kw_get_dict(gobj, topic, "cache", 0, KW_REQUIRED);
    if(!topic_cache) {
        json_decref(jn_keys);
        return NULL;
    }
    load_topic_metadata(
        gobj,
        directory,
        topic_cache,    // not owned
        jn_keys         // not owned
    );
    json_decref(jn_keys);

    /*
     *  Search
     */
    BOOL only_md = kw_get_bool(gobj, match_cond, "only_md", 0, 0);
    BOOL backward = kw_get_bool(gobj, match_cond, "backward", 0, 0);
size_t __last_rowid__ = 0; // TODO quita
return list;

//    int content_fp = get_topic_fd(tranger, topic, key, TRUE, __t__);  // Can be -1, if sf_no_disk
//    int md2_fp = get_topic_fd(tranger, topic, key, FALSE, __t__);  // Can be -1, if sf_no_disk

    BOOL end = FALSE;
    md2_record_t md_record;
    memset(&md_record, 0, sizeof(md2_record_t));
    if(!backward) {
        json_int_t from_rowid = kw_get_int(gobj, match_cond, "from_rowid", 0, 0);
        if(from_rowid>0) {
            end = tranger2_get_record(tranger, topic, from_rowid, &md_record, TRUE);
        } else if(from_rowid<0 && (__last_rowid__ + from_rowid)>0) {
            from_rowid = __last_rowid__ + from_rowid;
            end = tranger2_get_record(tranger, topic, from_rowid, &md_record, TRUE);
        } else {
            end = tranger2_first_record(tranger, topic, &md_record);
        }
    } else {
        json_int_t to_rowid = kw_get_int(gobj, match_cond, "to_rowid", 0, 0);
        if(to_rowid>0) {
            end = tranger2_get_record(tranger, topic, to_rowid, &md_record, TRUE);
        } else if(to_rowid<0 && (__last_rowid__ + to_rowid)>0) {
            to_rowid = __last_rowid__ + to_rowid;
            end = tranger2_get_record(tranger, topic, to_rowid, &md_record, TRUE);
        } else {
            end = tranger2_last_record(tranger, topic, &md_record);
        }
    }

    while(!end) {
        if(trace_level) {
            tr2_print_md1_record(tranger, topic, &md_record, title, sizeof(title));
        }
        if(tranger2_match_record(
                tranger,
                topic,
                match_cond,
                &md_record,
                &end
            )) {

            if(trace_level) {
                // TODO trace_msg0("ok - %s", title);
            }

            json_t *jn_record = 0;
// TODO           md_record.__system_flag__ |= sf_loading_from_disk;
//            if(!only_md) {
//                jn_record = tranger_read_record_content(tranger, topic, &md_record);
//            }

            if(load_record_callback) {
                /*--------------------------------------------*
                 *  Put record metadata in json record
                 *--------------------------------------------*/
                // Inform user list: record from disk
                JSON_INCREF(jn_record)
                int ret = load_record_callback(
                    tranger,
                    topic,
                    list,
                    &md_record,
                    jn_record
                );
                /*
                 *  Return:
                 *      0 do nothing (callback will create their own list, or not),
                 *      1 add record to returned list.data,
                 *      -1 break the load
                 */
                if(ret < 0) {
                    JSON_DECREF(jn_record)
                    break;
                } else if(ret > 0) {
                    if(!jn_record) {
                        jn_record = json_object();
                    }
                    json_object_set_new(jn_record, "__md_tranger__", tranger2_md2json(&md_record));
                    json_array_append_new(
                        data,
                        jn_record // owned
                    );
                } else { // == 0
                    // user's callback manages the record
                    JSON_DECREF(jn_record)
                }
            } else {
                if(!jn_record) {
                    jn_record = json_object();
                }
                json_object_set_new(jn_record, "__md_tranger__", tranger2_md2json(&md_record));
                json_array_append_new(
                    data,
                    jn_record // owned
                );
            }
        } else {
            if(trace_level) {
                // TODO trace_msg0("XX - %s", title);
            }
        }
        if(end) {
            break;
        }
        if(!backward) {
            end = tranger2_next_record(tranger, topic, &md_record);
        } else {
            end = tranger2_prev_record(tranger, topic, &md_record);
        }
    }

    return list;
}

/***************************************************************************
    Close list
 ***************************************************************************/
PUBLIC int tranger2_close_list(
    json_t *tranger,
    json_t *list
)
{
    if(!list) {
        // silence
        return -1;
    }
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(gobj, list, "topic_name", "", KW_REQUIRED);
    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, 0);
    if(topic) {
        json_array_remove(
            kw_get_dict_value(gobj, topic, "lists", 0, KW_REQUIRED),
            json_array_find_idx(kw_get_dict_value(gobj, topic, "lists", 0, KW_REQUIRED), list)
        );
    }
    return 0;
}

/***************************************************************************

 ***************************************************************************/
PRIVATE int json_array_find_idx(json_t *jn_list, json_t *item)
{
    int idx=-1;
    json_t *jn_value;
    json_array_foreach(jn_list, idx, jn_value) {
        if(jn_value == item) {
            return idx;
        }
    }
    return idx;
}

/***************************************************************************
 *  Returns list of searched keys that exist on disk
 ***************************************************************************/
PRIVATE json_t *find_keys_in_disk(
    hgobj gobj,
    const char *directory,
    json_t *match_cond  // not owned
)
{
    json_t *jn_keys = json_array();

    /*
     *  Only wants a key ?
     */
    const char *key = kw_get_str(gobj, match_cond, "key", 0, 0);
    if(!empty_string(key)) {
        if(file_exists(directory, key)) {
            json_array_append_new(jn_keys, json_string(key));
            return jn_keys;
        }
    }

    const char *pattern;
    const char *rkey = kw_get_str(gobj, match_cond, "rkey", 0, 0);
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
 *      list of keys with its range of time available
 ***************************************************************************/
PRIVATE int load_topic_metadata(
    hgobj gobj,
    const char *directory,
    json_t *topic_cache,    // not owned
    json_t *jn_keys         // not owned
) {
    int idx; json_t *jn_key;
    json_array_foreach(jn_keys, idx, jn_key) {
        const char *key = json_string_value(jn_key);
        json_t *t_range = get_time_range(gobj, directory, key);
        json_object_set_new(topic_cache, key, t_range);
    }
    return 0;
}

/***************************************************************************
 *  Get range time of a key
 ***************************************************************************/
PRIVATE json_t *get_time_range(hgobj gobj, const char *directory, const char *key)
{
    uint64_t total_rows = 0;
    uint64_t global_from_t = (uint64_t)(-1);
    uint64_t global_to_t = 0;
    uint64_t global_from_tm = (uint64_t)(-1);
    uint64_t global_to_tm = 0;
    char path[PATH_MAX];

    json_t *t_range = json_object();
    json_t *t_range_files = kw_get_dict(gobj, t_range, "files", json_array(), KW_CREATE);

    build_path(path, sizeof(path), directory, key, NULL);

    int files_md_size;
    char **files_md = get_ordered_filename_array(
        gobj,
        path,
        ".*\\.md2",
        WD_MATCH_REGULAR_FILE|WD_ONLY_NAMES,
        &files_md_size
    );
    for(int i=0; i<files_md_size; i++) {
        md2_record_t md_first_record;
        md2_record_t md_last_record;
        build_path(path, sizeof(path), directory, key, files_md[i], NULL);
        int fd = open(path, O_RDONLY|O_LARGEFILE, 0);
        if(fd<0) {
            gobj_log_critical(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot open md2 file",
                "path",         "%s", path,
                "errno",        "%s", strerror(errno),
                NULL
            );
            continue;
        }

        /*
         *  Read first record
         */
        ssize_t ln = read(fd, &md_first_record, sizeof(md_first_record));
        if(ln == sizeof(md_first_record)) {
            md_first_record.__t__ = htonll(md_first_record.__t__);
            md_first_record.__tm__ = htonll(md_first_record.__tm__);
            md_first_record.__offset__ = htonll(md_first_record.__offset__);
            md_first_record.__size__ = htonll(md_first_record.__size__);
        } else {
            if(ln<0) {
                gobj_log_critical(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "Cannot read md2 file",
                    "path",         "%s", path,
                    "errno",        "%s", strerror(errno),
                    NULL
                );
            } else if(ln==0) {
                // No data
            }
            close(fd);
            continue;
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
                "path",         "%s", path,
                "offset",       "%ld", (long)offset,
                "errno",        "%s", strerror(errno),
                NULL
            );
            close(fd);
            continue;
        }
        if(offset == 0) {
            // No records
            close(fd);
            continue;
        }

        uint64_t partial_rows = offset/sizeof(md2_record_t);

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
            continue;
        }

        ln = read(fd, &md_last_record, sizeof(md_last_record));
        if(ln == sizeof(md_last_record)) {
            md_last_record.__t__ = htonll(md_last_record.__t__);
            md_last_record.__tm__ = htonll(md_last_record.__tm__);
            md_last_record.__offset__ = htonll(md_last_record.__offset__);
            md_last_record.__size__ = htonll(md_last_record.__size__);
        } else {
            if(ln<0) {
                gobj_log_critical(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "Cannot read md2 file",
                    "path",         "%s", path,
                    "errno",        "%s", strerror(errno),
                    NULL
                );
            } else if(ln==0) {
                // No data
            }
            close(fd);
            continue;
        }

        if(md_first_record.__t__ < global_from_t) {
            global_from_t = md_first_record.__t__;
        }
        if(md_first_record.__t__ > global_to_t) {
            global_to_t = md_first_record.__t__;
        }

        if(md_first_record.__tm__ < global_from_tm) {
            global_from_tm = md_first_record.__tm__;
        }
        if(md_first_record.__tm__ > global_to_tm) {
            global_to_tm = md_first_record.__tm__;
        }

        if(md_last_record.__t__ < global_from_t) {
            global_from_t = md_last_record.__t__;
        }
        if(md_last_record.__t__ > global_to_t) {
            global_to_t = md_last_record.__t__;
        }

        if(md_last_record.__tm__ < global_from_tm) {
            global_from_tm = md_last_record.__tm__;
        }
        if(md_last_record.__tm__ > global_to_tm) {
            global_to_tm = md_last_record.__tm__;
        }

        uint64_t partial_from_t = (uint64_t)(-1);
        uint64_t partial_to_t = 0;
        uint64_t partial_from_tm = (uint64_t)(-1);
        uint64_t partial_to_tm = 0;

        if(md_first_record.__t__ < partial_from_t) {
            partial_from_t = md_first_record.__t__;
        }
        if(md_first_record.__t__ > partial_to_t) {
            partial_to_t = md_first_record.__t__;
        }

        if(md_first_record.__tm__ < partial_from_tm) {
            partial_from_tm = md_first_record.__tm__;
        }
        if(md_first_record.__tm__ > partial_to_tm) {
            partial_to_tm = md_first_record.__tm__;
        }

        if(md_last_record.__t__ < partial_from_t) {
            partial_from_t = md_last_record.__t__;
        }
        if(md_last_record.__t__ > partial_to_t) {
            partial_to_t = md_last_record.__t__;
        }

        if(md_last_record.__tm__ < partial_from_tm) {
            partial_from_tm = md_last_record.__tm__;
        }
        if(md_last_record.__tm__ > partial_to_tm) {
            partial_to_tm = md_last_record.__tm__;
        }

        char *p = strrchr(files_md[i], '.');    // save file name without extension
        if(p) {
            *p = 0;
        }
        json_t *partial_range = json_object();
        json_object_set_new(partial_range, "name", json_string(files_md[i]));
        if(p) {
            *p = '.';
        }

        json_object_set_new(partial_range, "fr_t", json_integer((json_int_t)partial_from_t));
        json_object_set_new(partial_range, "to_t", json_integer((json_int_t)partial_to_t));
        json_object_set_new(partial_range, "fr_tm", json_integer((json_int_t)partial_from_tm));
        json_object_set_new(partial_range, "to_tm", json_integer((json_int_t)partial_to_tm));
        json_object_set_new(partial_range, "rows", json_integer((json_int_t)partial_rows));

        json_array_append_new(t_range_files, partial_range);

        total_rows += partial_rows;

        close(fd);
    }

    free_ordered_filename_array(files_md, files_md_size);

    json_t *total_range = kw_get_dict(gobj, t_range, "total", json_object(), KW_CREATE);
    json_object_set_new(total_range, "fr_t", json_integer((json_int_t)global_from_t));
    json_object_set_new(total_range, "to_t", json_integer((json_int_t)global_to_t));
    json_object_set_new(total_range, "fr_tm", json_integer((json_int_t)global_from_tm));
    json_object_set_new(total_range, "to_tm", json_integer((json_int_t)global_to_tm));
    json_object_set_new(total_range, "rows", json_integer((json_int_t)total_rows));

    return t_range;
}

/***************************************************************************
    Get list by his (optional) id
 ***************************************************************************/
PUBLIC json_t *tranger2_get_list(
    json_t *tranger,
    const char *id
)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

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
    Get md record by rowid (by FILE, for reads)
 ***************************************************************************/
PUBLIC int tranger2_get_record(
    json_t *tranger,
    json_t *topic,
    uint64_t rowid,
    md2_record_t *md_record,
    BOOL verbose
)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
    BOOL master = kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED);

    memset(md_record, 0, sizeof(md2_record_t));

    if(rowid == 0) {
        if(verbose) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "rowid 0",
                "topic",        "%s", tranger2_topic_name(topic),
                "rowid",        "%lu", (unsigned long)rowid,
                NULL
            );
        }
        return -1;
    }

    uint64_t __last_rowid__ = (uint64_t)kw_get_int(gobj, topic, "__last_rowid__", 0, KW_REQUIRED);

    // HACK no "master" (tranger readonly) don't have updated __last_rowid__
    if(master) {
        if(__last_rowid__ <= 0) {
            return -1;
        }
        if(rowid > __last_rowid__) {
            if(verbose) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "rowid greater than last_rowid",
                    "topic",        "%s", tranger2_topic_name(topic),
                    "rowid",        "%lu", (unsigned long)rowid,
                    "last_rowid",   "%lu", (unsigned long)__last_rowid__,
                    NULL
                );
            }
            return -1;
        }
    }

    {
        /*----------------------------------*
         *      topic idx by fd
         *----------------------------------*/
//        int fd = get_idx_fd(tranger, topic);
//        if(fd < 0) {
//            // Error already logged
//            return -1;
//        }
//
//        off64_t offset = (off64_t) ((rowid-1) * sizeof(md2_record_t));
//        off64_t offset_ = lseek64(fd, offset, SEEK_SET);
//        if(offset != offset_) {
//            if(master) {
//                gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
//                    "function",     "%s", __FUNCTION__,
//                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//                    "msg",          "%s", "topic_idx.md corrupted, lseek64 FAILED",
//                    "topic",        "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
//                    NULL
//                );
//            }
//            return -1;
//        }
//
//        size_t ln = read(
//            fd,
//            md_record,
//            sizeof(md2_record_t)
//        );
//        if(ln != sizeof(md2_record_t)) {
//            // HACK no "master" (tranger readonly): we try to read new records
//            if(master) {
//                gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
//                    "function",     "%s", __FUNCTION__,
//                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
//                    "msg",          "%s", "Cannot read record metadata, read FAILED",
//                    "topic",        "%s", tranger2_topic_name(topic),
//                    "errno",        "%s", strerror(errno),
//                    NULL
//                );
//            } else {
//                if(ln != 0) {
//                    gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
//                        "function",     "%s", __FUNCTION__,
//                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
//                        "msg",          "%s", "Cannot read record metadata, read FAILED",
//                        "topic",        "%s", tranger2_topic_name(topic),
//                        "errno",        "%s", strerror(errno),
//                        NULL
//                    );
//                }
//            }
//            return -1;
//        }
    }

// TODO   if(md_record->__rowid__ != rowid) {
//        gobj_log_error(gobj, 0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//            "msg",          "%s", "md_record corrupted, item rowid not match",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "rowid",        "%lu", (unsigned long)rowid,
////       TODO     "__rowid__",    "%lu", (unsigned long)md_record->__rowid__,
//            NULL
//        );
//        return -1;
//    }

    if(!master) {
        // HACK no "master" (tranger readonly): found new records
//  TODO      if(md_record->__rowid__ > __last_rowid__) {
//            json_object_set_new(topic, "__last_rowid__", json_integer(md_record->__rowid__));
//        }
    }
    return 0;
}

/***************************************************************************
 *   Read record data
 ***************************************************************************/
PUBLIC json_t *tranger2_read_record_content(
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record
)
{
//    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
//// TODO   if(md_record->__system_flag__ & sf_deleted_record) {
////        return 0;
////    }
//
//    /*--------------------------------------------*
//     *  Recover file corresponds to __t__
//     *--------------------------------------------*/
//    FILE *file = get_content_file(tranger, topic, "TODO", md_record->__t__); // TODO
//    if(!file) {
//        // Error already logged
//        return 0;
//    }
//
//    uint64_t __offset__ = md_record->__offset__;
//    if(fseeko64(file, __offset__, SEEK_SET)<0) {
//        gobj_log_error(gobj, 0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
//            "msg",          "%s", "Cannot read record data. fseeko64 FAILED",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
//            "errno",        "%s", strerror(errno),
//            "__t__",        "%lu", (unsigned long)md_record->__t__,
//            NULL
//        );
//        return 0;
//    }
//
//    gbuffer_t *gbuf = gbuffer_create(md_record->__size__, md_record->__size__);
//    if(!gbuf) {
//        gobj_log_error(gobj, 0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_MEMORY_ERROR,
//            "msg",          "%s", "Cannot read record data. gbuf_create() FAILED",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
//            NULL
//        );
//        return 0;
//    }
//    char *p = gbuffer_cur_rd_pointer(gbuf);
//
//    if(fread(p, md_record->__size__, 1, file)<=0) {
//        gobj_log_critical(gobj, 0, // Let continue, will be a message lost
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
//            "msg",          "%s", "Cannot read record content, read FAILED",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "directory",    "%s", kw_get_str(gobj, topic, "directory", 0, KW_REQUIRED),
//            "errno",        "%s", strerror(errno),
//            "__t__",        "%lu", (unsigned long)md_record->__t__,
//            "__size__",     "%lu", (unsigned long)md_record->__size__,
//            "__offset__",   "%lu", (unsigned long)md_record->__offset__,
//            NULL
//        );
//        gbuffer_decref(gbuf);
//        return 0;
//    }
//
//// TODO   /*
////     *  Restoring: first decrypt, second decompress
////     */
////    if(md_record->__system_flag__ & sf_cipher_record) {
////        // if(topic->decrypt_callback) { TODO
////        //     gbuf = topic->decrypt_callback(
////        //         topic->user_data,
////        //         topic,
////        //         gbuf    // must be owned
////        //     );
////        // }
////    }
////    if(md_record->__system_flag__ & sf_zip_record) {
////        // if(topic->decompress_callback) { TODO
////        //     gbuf = topic->decompress_callback(
////        //         topic->user_data,
////        //         topic,
////        //         gbuf    // must be owned
////        //     );
////        // }
////    }
//
//    json_t *jn_record;
//    if(empty_string(p)) {
//        jn_record = json_object();
//        gbuffer_decref(gbuf);
//    } else {
//        jn_record = anystring2json(p, strlen(p), FALSE);
//        gbuffer_decref(gbuf);
//        if(!jn_record) {
//            gobj_log_critical(gobj, 0, // Let continue, will be a message lost
//                "function",     "%s", __FUNCTION__,
//                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
//                "msg",          "%s", "Bad data, json_loadfd() FAILED.",
//                "topic",        "%s", tranger2_topic_name(topic),
//                "__t__",        "%lu", (unsigned long)md_record->__t__,
//                "__size__",     "%lu", (unsigned long)md_record->__size__,
//                "__offset__",   "%lu", (unsigned long)md_record->__offset__,
//                NULL
//            );
////         TODO   log_debug_dump(0, p, md_record->__size__, "no jn_record");
//            return 0;
//        }
//    }
//
//    return jn_record;
}

/***************************************************************************
 *  Match md record
 ***************************************************************************/
PUBLIC BOOL tranger2_match_record(
    json_t *tranger,
    json_t *topic,
    json_t *match_cond,  // not owned
    const md2_record_t *md_record,
    BOOL *end
)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
    if(end) {
        *end = FALSE;
    }

    if(!match_cond || (json_object_size(match_cond)==0 && json_array_size(match_cond)==0)) {
        // No conditions, match all
        return TRUE;
    }
    md2_record_t md_record_last;
    tranger2_last_record(tranger, topic, &md_record_last);
    uint64_t __last_rowid__ = 0; // TODO md_record_last.__rowid__;
    uint64_t __last_t__ = md_record_last.__t__;
    uint64_t __last_tm__ = md_record_last.__tm__;

    BOOL backward = kw_get_bool(gobj, match_cond, "backward", 0, 0);

    uint32_t __system_flag__ = get_system_flag(md_record);
    uint32_t __user_flag__ = get_user_flag(md_record);
    if(kw_has_key(match_cond, "key")) {
        int json_type = json_typeof(json_object_get(match_cond, "key"));
        if(__system_flag__ & (sf2_int_key)) {
            switch(json_type) {
            case JSON_OBJECT:
                {
                    BOOL some = FALSE;
                    const char *key_; json_t *jn_value;
                    json_object_foreach(json_object_get(match_cond, "key"), key_, jn_value) {
// TODO                      if(md_record->key.i == atoi(key_)) {
//                            some = TRUE;
//                            break; // Con un match de key ya es true
//                        }
                    }
                    if(!some) {
                        return FALSE;
                    }
                }
                break;
            case JSON_ARRAY:
                {
                    BOOL some = FALSE;
                    int idx; json_t *jn_value;
                    json_array_foreach(json_object_get(match_cond, "key"), idx, jn_value) {
// TODO                       if(json_is_integer(jn_value)) {
//                            if(md_record->key.i == json_integer_value(jn_value)) {
//                                some = TRUE;
//                                break; // Con un match de key ya es true
//                            }
//                        } else if(json_is_string(jn_value)) {
//                            if(md_record->key.i == atoi(json_string_value(jn_value))) {
//                                some = TRUE;
//                                break; // Con un match de key ya es true
//                            }
//                        }
                    }
                    if(!some) {
                        return FALSE;
                    }
                }
                break;
            default:
                {
                    json_int_t key = kw_get_int(gobj, match_cond, "key", 0, KW_REQUIRED|KW_WILD_NUMBER);
// TODO                   if(md_record->key.i != key) {
//                        return FALSE;
//                    }
                }
                break;
            }

        } else if(__system_flag__ & sf2_string_key) {
            switch(json_type) {
            case JSON_OBJECT:
                {
                    BOOL some = FALSE;
                    const char *key_; json_t *jn_value;
                    json_object_foreach(json_object_get(match_cond, "key"), key_, jn_value) {
// TODO                       if(strncmp(md_record->key.s, key_, sizeof(md_record->key.s)-1)==0) {
//                            some = TRUE;
//                            break; // Con un match de key ya es true
//                        }
                    }
                    if(!some) {
                        return FALSE;
                    }
                }
                break;
            case JSON_ARRAY:
                {
                    BOOL some = FALSE;
                    int idx; json_t *jn_value;
                    json_array_foreach(json_object_get(match_cond, "key"), idx, jn_value) {
                        if(json_is_string(jn_value)) {
// TODO                           if(strncmp(
//                                    md_record->key.s,
//                                    json_string_value(jn_value),
//                                    sizeof(md_record->key.s)-1)==0) {
//                                some = TRUE;
//                                break; // Con un match de key ya es true
//                            }
                        } else {
                            return FALSE;
                        }
                    }
                    if(!some) {
                        return FALSE;
                    }
                }
                break;
            default:
                {
                    const char *key = kw_get_str(gobj, match_cond, "key", 0, 0);
                    if(!key) {
                        return FALSE;
                    }
//  TODO                  if(strncmp(md_record->key.s, key, sizeof(md_record->key.s)-1)!=0) {
//                        return FALSE;
//                    }
                }
                break;
            }
        } else {
            return FALSE;
        }
    }

    if(kw_has_key(match_cond, "rkey")) {
        if(__system_flag__ & sf2_string_key) {
            const char *rkey = kw_get_str(gobj, match_cond, "rkey", 0, 0);
            if(!rkey) {
                return FALSE;
            }

            regex_t _re_name;
            if(regcomp(&_re_name, rkey, REG_EXTENDED | REG_NOSUB)!=0) {
                return FALSE;
            }
            int ret = 0; // TODO regexec(&_re_name, md_record->key.s, 0, 0, 0);
            regfree(&_re_name);
            if(ret!=0) {
                return FALSE;
            }
        } else {
            return FALSE;
        }
    }

    if(kw_has_key(match_cond, "from_rowid")) {
        json_int_t from_rowid = kw_get_int(gobj, match_cond, "from_rowid", 0, KW_WILD_NUMBER);
        if(from_rowid >= 0) {
// TODO           if(md_record->__rowid__ < from_rowid) {
//                if(backward) {
//                    if(end) {
//                        *end = TRUE;
//                    }
//                }
//                return FALSE;
//            }
        } else {
            uint64_t x = __last_rowid__ + from_rowid;
//  TODO          if(md_record->__rowid__ <= x) {
//                if(backward) {
//                    if(end) {
//                        *end = TRUE;
//                    }
//                }
//                return FALSE;
//            }
        }
    }

    if(kw_has_key(match_cond, "to_rowid")) {
        json_int_t to_rowid = kw_get_int(gobj, match_cond, "to_rowid", 0, KW_WILD_NUMBER);
        if(to_rowid >= 0) {
// TODO           if(md_record->__rowid__ > to_rowid) {
//                if(!backward) {
//                    if(end) {
//                        *end = TRUE;
//                    }
//                }
//                return FALSE;
//            }
        } else {
            uint64_t x = __last_rowid__ + to_rowid;
//  TODO          if(md_record->__rowid__ > x) {
//                if(!backward) {
//                    if(end) {
//                        *end = TRUE;
//                    }
//                }
//                return FALSE;
//            }
        }
    }

    if(kw_has_key(match_cond, "from_t")) {
        json_int_t from_t = 0;
        json_t *jn_from_t = json_object_get(match_cond, "from_t");
        if(json_is_string(jn_from_t)) {
            int offset;
            timestamp_t timestamp;
            parse_date_basic(json_string_value(jn_from_t), &timestamp, &offset);
            from_t = timestamp;
        } else {
            from_t = json_integer_value(jn_from_t);
        }

        if(from_t >= 0) {
            if(md_record->__t__ < from_t) {
                if(backward) {
                    if(end) {
                        *end = TRUE;
                    }
                }
                return FALSE;
            }
        } else {
            uint64_t x = __last_t__ + from_t;
            if(md_record->__t__ <= x) {
                if(backward) {
                    if(end) {
                        *end = TRUE;
                    }
                }
                return FALSE;
            }
        }
    }

    if(kw_has_key(match_cond, "to_t")) {
        json_int_t to_t = 0;
        json_t *jn_to_t = json_object_get(match_cond, "to_t");
        if(json_is_string(jn_to_t)) {
            int offset;
            timestamp_t timestamp;
            parse_date_basic(json_string_value(jn_to_t), &timestamp, &offset);
            to_t = timestamp;
        } else {
            to_t = json_integer_value(jn_to_t);
        }

        if(to_t >= 0) {
            if(md_record->__t__ > to_t) {
                if(!backward) {
                    if(end) {
                        *end = TRUE;
                    }
                }
                return FALSE;
            }
        } else {
            uint64_t x = __last_t__ + to_t;
            if(md_record->__t__ > x) {
                if(!backward) {
                    if(end) {
                        *end = TRUE;
                    }
                }
                return FALSE;
            }
        }
    }

    if(kw_has_key(match_cond, "from_tm")) {
        json_int_t from_tm = 0;
        json_t *jn_from_tm = json_object_get(match_cond, "from_tm");
        if(json_is_string(jn_from_tm)) {
            int offset;
            timestamp_t timestamp;
            parse_date_basic(json_string_value(jn_from_tm), &timestamp, &offset);
            if(__system_flag__ & (sf2_tm_ms)) {
                timestamp *= 1000; // TODO lost milisecond precision?
            }
            from_tm = timestamp;
        } else {
            from_tm = json_integer_value(jn_from_tm);
        }

        if(from_tm >= 0) {
            if(md_record->__tm__ < from_tm) {
                if(backward) {
                    if(end) {
                        *end = TRUE;
                    }
                }
                return FALSE;
            }
        } else {
            uint64_t x = __last_tm__ + from_tm;
            if(md_record->__tm__ <= x) {
                if(backward) {
                    if(end) {
                        *end = TRUE;
                    }
                }
                return FALSE;
            }
        }
    }

    if(kw_has_key(match_cond, "to_tm")) {
        json_int_t to_tm = 0;
        json_t *jn_to_tm = json_object_get(match_cond, "to_tm");
        if(json_is_string(jn_to_tm)) {
            int offset;
            timestamp_t timestamp;
            parse_date_basic(json_string_value(jn_to_tm), &timestamp, &offset);
            if(__system_flag__ & (sf2_tm_ms)) {
                timestamp *= 1000; // TODO lost milisecond precision?
            }
            to_tm = timestamp;
        } else {
            to_tm = json_integer_value(jn_to_tm);
        }

        if(to_tm >= 0) {
            if(md_record->__tm__ > to_tm) {
                if(!backward) {
                    if(end) {
                        *end = TRUE;
                    }
                }
                return FALSE;
            }
        } else {
            uint64_t x = __last_tm__ + to_tm;
            if(md_record->__tm__ > x) {
                if(!backward) {
                    if(end) {
                        *end = TRUE;
                    }
                }
                return FALSE;
            }
        }
    }

    if(kw_has_key(match_cond, "user_flag")) {
        uint32_t user_flag = kw_get_int(gobj, match_cond, "user_flag", 0, 0);
        if((__user_flag__ != user_flag)) {
            return FALSE;
        }
    }
    if(kw_has_key(match_cond, "not_user_flag")) {
        uint32_t not_user_flag = kw_get_int(gobj, match_cond, "not_user_flag", 0, 0);
        if((__user_flag__ == not_user_flag)) {
            return FALSE;
        }
    }

    if(kw_has_key(match_cond, "user_flag_mask_set")) {
        uint32_t user_flag_mask_set = kw_get_int(gobj, match_cond, "user_flag_mask_set", 0, 0);
        if((__user_flag__ & user_flag_mask_set) != user_flag_mask_set) {
            return FALSE;
        }
    }
    if(kw_has_key(match_cond, "user_flag_mask_notset")) {
        uint32_t user_flag_mask_notset = kw_get_int(gobj, match_cond, "user_flag_mask_notset", 0, 0);
        if((__user_flag__ | ~user_flag_mask_notset) != ~user_flag_mask_notset) {
            return FALSE;
        }
    }

    if(kw_has_key(match_cond, "notkey")) {
        if(__system_flag__ & (sf2_int_key)) {
            json_int_t notkey = kw_get_int(gobj, match_cond, "key", 0, KW_REQUIRED|KW_WILD_NUMBER);
//  TODO          if(md_record->key.i == notkey) {
//                return FALSE;
//            }
        } else if(__system_flag__ & sf2_string_key) {
            const char *notkey = kw_get_str(gobj, match_cond, "key", 0, 0);
            if(!notkey) {
                return FALSE;
            }
// TODO           if(strncmp(md_record->key.s, notkey, sizeof(md_record->key.s)-1)==0) {
//                return FALSE;
//            }
        } else {
            return FALSE;
        }
    }

    return TRUE;
}

/***************************************************************************
    Get the first matched md record
    Return 0 if found. Set metadata in md_record
    Return -1 if not found
 ***************************************************************************/
PUBLIC int tranger2_find_record(
    json_t *tranger,
    json_t *topic,
    json_t *match_cond,  // owned
    md2_record_t *md_record
)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
    if(!match_cond) {
        match_cond = json_object();
    }
    BOOL backward = kw_get_bool(gobj, match_cond, "backward", 0, 0);

    BOOL end = FALSE;
    if(!backward) {
        end = tranger2_first_record(tranger, topic, md_record);
    } else {
        end = tranger2_last_record(tranger, topic, md_record);
    }
    while(!end) {
        if(tranger2_match_record(tranger, topic, match_cond, md_record, &end)) {
            JSON_DECREF(match_cond)
            return 0;
        }
        if(end) {
            break;
        }
        if(!backward) {
            end = tranger2_next_record(tranger, topic, md_record);
        } else {
            end = tranger2_prev_record(tranger, topic, md_record);
        }
    }
    JSON_DECREF(match_cond)
    return -1;
}

/***************************************************************************
 *  Read first md record
 ***************************************************************************/
PUBLIC int tranger2_first_record(
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record
)
{
    json_int_t rowid = 1;
    if(tranger2_get_record(
        tranger,
        topic,
        rowid,
        md_record,
        FALSE
    )<0) {
        return -1;
    }

// TODO   if(md_record->__rowid__ != 1) {
//        log_error(0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//            "msg",          "%s", "md_records corrupted, first item is not 1",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "rowid",        "%lu", (unsigned long)md_record->__rowid__,
//            NULL
//        );
//        return -1;
//    }

// TODO   if(md_record->__system_flag__ & sf_deleted_record) {
//        return tranger_next_record(tranger, topic, md_record);
//    }

    return 0;
}

/***************************************************************************
 *  Read last md record
 ***************************************************************************/
PUBLIC int tranger2_last_record(
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record
)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
    json_int_t rowid = kw_get_int(gobj, topic, "__last_rowid__", 0, KW_REQUIRED);
    if(tranger2_get_record(
        tranger,
        topic,
        rowid,
        md_record,
        FALSE
    )<0) {
        return -1;
    }

// TODO   if(md_record->__rowid__ != rowid) {
//        log_error(0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//            "msg",          "%s", "md_records corrupted, last item is not last_rowid",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "rowid",        "%lu", (unsigned long)md_record->__rowid__,
//            "last_rowid",   "%lu", (unsigned long)rowid,
//            NULL
//        );
//        return -1;
//    }
//
//    if(md_record->__system_flag__ & sf_deleted_record) {
//        return tranger_prev_record(tranger, topic, md_record);
//    }

    return 0;
}

/***************************************************************************
 *  Read next md record
 ***************************************************************************/
PUBLIC int tranger2_next_record(
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record
)
{
//   TODO json_int_t rowid = md_record->__rowid__ + 1;
//
//    if(tranger2_get_record(
//        tranger,
//        topic,
//        rowid,
//        md_record,
//        FALSE
//    )<0) {
//        return -1;
//    }
//
//    if(md_record->__rowid__ != rowid) {
//        log_error(0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//            "msg",          "%s", "md_records corrupted, last item is not last_rowid",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "rowid",        "%lu", (unsigned long)rowid,
//            "next_rowid",   "%lu", (unsigned long)md_record->__rowid__,
//            NULL
//        );
//        return -1;
//    }
//
//    if(md_record->__system_flag__ & sf_deleted_record) {
//        return tranger_next_record(tranger, topic, md_record);
//    }

    return 0;
}

/***************************************************************************
 *  Read prev md record
 ***************************************************************************/
PUBLIC int tranger2_prev_record(
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record
)
{
//   TODO  json_int_t rowid = md_record->__rowid__ - 1;
//    if(md_record->__rowid__ < 1) {
//        return 0;
//    }
//    if(tranger2_get_record(
//        tranger,
//        topic,
//        rowid,
//        md_record,
//        FALSE
//    )<0) {
//        return -1;
//    }
//
//    if(md_record->__rowid__ != rowid) {
//        log_error(0,
//            "function",     "%s", __FUNCTION__,
//            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
//            "msg",          "%s", "md_records corrupted, last item is not last_rowid",
//            "topic",        "%s", tranger2_topic_name(topic),
//            "rowid",        "%lu", (unsigned long)rowid,
////           TODO "prev_rowid",   "%lu", (unsigned long)md_record->__rowid__,
//            NULL
//        );
//        return -1;
//    }

// TODO   if(md_record->__system_flag__ & sf_deleted_record) {
//        return tranger_prev_record(tranger, topic, md_record);
//    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void tr2_print_md0_record(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    char *bf,
    int bfsize
)
{
    uint64_t __rowid__ = 0; // TODO get from ???
    const char *key = "";         // TODO get from ???
    uint64_t ikey = 0;

    char fecha[90];
    char stamp[64];
    uint32_t __system_flag__ = get_system_flag(md_record);
    time_t t = get_time_t(md_record);
    if(__system_flag__ & sf2_t_ms) {
        t /= 1000;
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%S%z", localtime(&t));
    } else {
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%S%z",
            localtime((time_t *)&t)
        );
    }

    time_t t_m = get_time_tm(md_record);
    char fecha_tm[80];
    if(__system_flag__ & sf2_tm_ms) {
        t_m /= 1000;
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S%z", gmtime(&t_m));
    } else {
        strftime(fecha_tm, sizeof(fecha_tm), "%Y-%m-%dT%H:%M:%S%z",
            gmtime((time_t *)&t_m)
        );
    }

    system_flag2_t key_type = __system_flag__ & KEY_TYPE_MASK2;

    if(!key_type) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s",
            (uint64_t)__rowid__,
            (uint64_t)md_record->__t__,
            fecha,
            (uint64_t)md_record->__tm__,
            fecha_tm
        );
    } else if(key_type & (sf2_int_key)) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key: %"PRIu64,
            (uint64_t)__rowid__,
            (uint64_t)md_record->__t__,
            fecha,
            (uint64_t)md_record->__tm__,
            fecha_tm,
            ikey
        );
    } else if(key_type & sf2_string_key) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key:'%s'",
            (uint64_t)__rowid__,
            (uint64_t)md_record->__t__,
            fecha,
            (uint64_t)md_record->__tm__,
            fecha_tm,
            key
        );
    } else {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD metadata, without key type",
            "topic",        "%s", tranger2_topic_name(topic),
            NULL
        );
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void tr2_print_md1_record(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    char *bf,
    int bfsize
)
{
    uint64_t __rowid__ = 0; // TODO get from ???
    const char *key = "";         // TODO get from ???
    uint64_t ikey = 0;

    struct tm *tm;
    char fecha[90];
    char stamp[64];

    uint32_t __system_flag__ = get_system_flag(md_record);
    uint32_t __user_flag__ = get_user_flag(md_record);
    time_t t_m = get_time_tm(md_record);
    time_t t = get_time_t(md_record);

    if(__system_flag__ & sf2_t_ms) {
        int ms = t % 1000;
        t /= 1000;
        tm = gmtime(&t);
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", tm);
        snprintf(fecha, sizeof(fecha), "%s.%03uZ", stamp, ms);
    } else {
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%SZ", gmtime((time_t *)&t));
    }

    char fecha_tm[80];
    if(__system_flag__ & sf2_tm_ms) {
        int ms = t_m % 1000;
        time_t t_m_ = t_m/1000;
        tm = gmtime(&t_m_);
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", tm);
        snprintf(fecha_tm, sizeof(fecha_tm), "%s.%03uZ", stamp, ms);
    } else {
        strftime(fecha_tm, sizeof(fecha_tm), "%Y-%m-%dT%H:%M:%SZ", gmtime((time_t *)&md_record->__tm__));
    }

    system_flag2_t key_type = __system_flag__ & KEY_TYPE_MASK2;

    if(!key_type) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "uflag:0x%"PRIX32", sflag:0x%"PRIX32", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s",
            (uint64_t)__rowid__,
            __user_flag__,
            __system_flag__,
            t,
            fecha,
            t_m,
            fecha_tm
        );
    } else if(key_type & (sf2_int_key)) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "uflag:0x%"PRIX32", sflag:0x%"PRIX32", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key: %"PRIu64,
            __rowid__,
            __user_flag__,
            __system_flag__,
            t,
            fecha,
            t_m,
            fecha_tm,
            ikey
        );
    } else if(key_type & sf2_string_key) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "uflag:0x%"PRIX32", sflag:0x%"PRIX32", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key:'%s'",
            __rowid__,
            __user_flag__,
            __system_flag__,
            t,
            fecha,
            t_m,
            fecha_tm,
            key
        );
    } else {
        gobj_log_error(0,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD metadata, without key type",
            "topic",        "%s", tranger2_topic_name(topic),
            NULL
        );
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void tr2_print_md2_record(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    char *bf,
    int bfsize
)
{
// TODO   time_t t = md_record->__t__;
//    uint64_t offset = md_record->__offset__;
//    uint64_t size = md_record->__size__;
//
//    char path[1024];
//    get_t_filename(
//        tranger,
//        topic,
//        path,
//        sizeof(path),
//        (md_record->__system_flag__ & sf_t_ms)? t/1000:t
//    );
//
//
//    snprintf(bf, bfsize,
//        "rowid:%"PRIu64", ofs:%"PRIu64", sz:%-5"PRIu64", "
//        "t:%"PRIu64", "
//        "f:%s",
//        (uint64_t)md_record->__rowid__,
//        offset,
//        size,
//        (uint64_t)t,
//        path
//    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void tr2_print_record_filename(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    char *bf,
    int bfsize
)
{
// TODO   time_t t = md_record->__t__;
//    get_t_filename(
//        tranger,
//        topic,
//        bf,
//        bfsize,
//        t
//    );
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
