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
#include "timeranger2.h"

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
    "__last_rowid__",
    "topic_idx_fd",
    "fd_opened_files",
    "file_opened_files",
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
typedef struct { // TODO build a cache system.
    json_t *jn_record;  // Living with recount > 0
    json_t *jn_cache;   // Cache (clone of jn_record).
    uint16_t timeout_cache;  // Timeout to free json when refcount is 0. Max 0xFFFF = 18,2 horas.
} json_cache_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/
typedef gbuffer_t * (*filter_callback_t) (   // Remember to free returned gbuffer
    void *user_data,  // topic user_data
    json_t *tranger,
    json_t *topic,
    gbuffer_t * gbuf  // must be owned
);

PRIVATE int _get_md_record_for_wr(
    json_t *tranger,
    json_t *topic,
    uint64_t rowid,
    md2_record_t *md_record,
    BOOL verbose
);

PRIVATE int new_record_md_to_file(
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record
);

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *
 ***************************************************************************/
/***************************************************************************
 *  Startup TimeRanger database
 ***************************************************************************/
PUBLIC json_t *tranger_startup(
    hgobj gobj,
    json_t *jn_tranger // owned
)
{
    /*
     *  Como parámetro de entrada "jn_tanger",
     *  se clona para no joder el original,
     *  y porque se añaden campos de instancia, por ejemplo "fd_opened_files"
     */
    json_t *tranger = create_json_record(gobj, tranger2_json_desc); // no master by default
    json_object_update_existing(tranger, jn_tranger);
    json_object_set_new(tranger, "gobj", json_integer((json_int_t)(size_t)gobj));
    JSON_DECREF(jn_tranger)

    char path[PATH_MAX];
    const char *path_ = kw_get_str(gobj, tranger, "path", "", 0);
    build_path(path, sizeof(path), path_, "", NULL); // I want modify path
    if(empty_string(path)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot startup TimeRanger. What path?",
            NULL
        );
        json_decref(tranger);
        return 0;
    }
    kw_set_dict_value(gobj, tranger, "path", json_string(path));

    const char *database = kw_get_str(gobj, tranger, "database", "", 0);
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
        kw_set_dict_value(gobj, tranger, "path", json_string(path));
        kw_set_dict_value(gobj, tranger, "database", json_string(database));
    }

    /*-------------------------------------*
     *  Build database directory and
     *  __timeranger__.json metadata file
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
    char directory[PATH_MAX];
    build_path(directory, sizeof(directory), path, database, NULL);
    kw_set_dict_value(gobj, tranger, "directory", json_string(directory));

    int fd = -1;
    if(file_exists(directory, "__timeranger__.json")) {
        json_t *jn_disk_tranger = load_persistent_json(
            gobj,
            directory,
            "__timeranger__.json",
            on_critical_error,
            &fd,
            master? TRUE:FALSE, //exclusive
            TRUE // silence
        );
        if(!jn_disk_tranger) {
            jn_disk_tranger = load_persistent_json(
                gobj,
                directory,
                "__timeranger__.json",
                on_critical_error,
                &fd,
                FALSE, // exclusive
                TRUE // silence
            );
            if(jn_disk_tranger) {
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TRANGER_ERROR,
                    "msg",          "%s", "Open as not master, __timeranger__.json locked",
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
            }
            master = FALSE;
            json_object_set_new(tranger, "master", json_false());
        }
        json_object_update_existing(tranger, jn_disk_tranger);
        json_decref(jn_disk_tranger);
    } else {
        if(!master) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Cannot startup TimeRanger. Not found and not master",
                "path",         "%s", directory,
                NULL
            );
            json_decref(tranger);
            return 0;
        }
        /*
         *  I'm MASTER
         */
        const char *filename_mask = kw_get_str(gobj, tranger, "filename_mask", "%Y-%m-%d", KW_REQUIRED);
        int xpermission = (int)kw_get_int(gobj, tranger, "xpermission", 02770, KW_REQUIRED);
        int rpermission = (int)kw_get_int(gobj, tranger, "rpermission", 0660, KW_REQUIRED);
        json_t *jn_tranger_ = json_object();
        kw_get_str(gobj, jn_tranger_, "database", database, KW_CREATE);
        kw_get_str(gobj, jn_tranger_, "filename_mask", filename_mask, KW_CREATE);
        kw_get_int(gobj, jn_tranger_, "rpermission", rpermission, KW_CREATE);
        kw_get_int(gobj, jn_tranger_, "xpermission", xpermission, KW_CREATE);
        save_json_to_file(
            gobj,
            directory,
            "__timeranger__.json",
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
            "__timeranger__.json",
            on_critical_error,
            &fd,
            master? TRUE:FALSE, //exclusive
            TRUE // silence
        );
        if(!jn_disk_tranger) {
            jn_disk_tranger = load_persistent_json(
                gobj,
                directory,
                "__timeranger__.json",
                on_critical_error,
                &fd,
                FALSE, //exclusive
                TRUE // silence
            );

            if(jn_disk_tranger) {
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TRANGER_ERROR,
                    "msg",          "%s", "Open as not master, __timeranger__.json locked",
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
            }
            master = FALSE;
            json_object_set_new(tranger, "master", json_false());
        }

        json_object_update_existing(tranger, jn_disk_tranger);
        json_decref(jn_disk_tranger);
    }

    /*
     *  Load Only read, volatil, defining in run-time
     */
    kw_get_dict(gobj, tranger, "fd_opened_files", json_object(), KW_CREATE);
    kw_get_dict(gobj, tranger, "topics", json_object(), KW_CREATE);

    kw_set_subdict_value(gobj, tranger, "fd_opened_files", "__timeranger__.json", json_integer(fd));

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
PUBLIC json_t *tranger_create_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,    // If the topic exists then only needs (tranger, topic_name) parameters
    const char *topic_name,
    const char *pkey,
    const char *tkey,
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
            JSON_DECREF(jn_cols);
            JSON_DECREF(jn_var);
            return 0;
        }
        if(mkrdir(directory, 0, (int)kw_get_int(gobj, tranger, "xpermission", 0, KW_REQUIRED))<0) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "path",         "%s", directory,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create TimeRanger subdir. mkrdir() FAILED",
                "errno",        "%s", strerror(errno),
                NULL
            );
        }

        /*----------------------------------------*
         *      Create topic_idx.md
         *----------------------------------------*/
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s",
            directory,
            "topic_idx.md"
        );
        int fp = newfile(full_path, (int)kw_get_int(gobj, tranger, "rpermission", 0, KW_REQUIRED), FALSE);
        if(fp < 0) {
            gobj_log_error(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create topic_idx.md file.",
                "filename",     "%s", full_path,
                "errno",        "%s", strerror(errno),
                NULL
            );
            JSON_DECREF(jn_cols)
            JSON_DECREF(jn_var)
            return 0;
        }
        close(fp);

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
                system_flag |= sf2_int_key;
            }
        }
        kw_get_int(gobj, jn_topic_desc, "system_flag", system_flag, KW_CREATE);

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
            "database",     "%s", kw_get_str(gobj, tranger, "database", "", KW_REQUIRED),
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
            "database",     "%s", kw_get_str(gobj, tranger, "database", "", KW_REQUIRED),
            "topic",        "%s", topic_name,
            NULL
        );

        /*----------------------------------------*
         *      Create data directory
         *----------------------------------------*/
        snprintf(full_path, sizeof(full_path), "%s/data",
            directory
        );
        if(mkrdir(full_path, 0, (int)kw_get_int(gobj, tranger, "xpermission", 0, KW_REQUIRED))<0) {
            gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                "function",     "%s", __FUNCTION__,
                "path",         "%s", full_path,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create TimeRanger subdir. mkrdir() FAILED",
                "errno",        "%s", strerror(errno),
                NULL
            );
        }
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
            JSON_DECREF(topic_var);
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

    return tranger2_open_topic(tranger, topic_name, TRUE);
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
            "gobj",         "%s", __FILE__,
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
    uint64_t offset = lseek64(fd, 0, SEEK_END);
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
 *
 ***************************************************************************/
PRIVATE int close_topic_idx_fd(json_t *tranger, json_t *topic)
{
    int fd = (int)kw_get_int(0, topic, "topic_idx_fd", -1, KW_REQUIRED);
    if(fd >= 0) {
        close(fd);
        json_object_set_new(topic, "topic_idx_fd", json_integer(-1));
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_topic_idx_fd(
    json_t *tranger,
    json_t *topic
)
{
    int fd = (int)kw_get_int(0, topic, "topic_idx_fd", -1, KW_REQUIRED);
    if(fd<0) {
        system_flag2_t system_flag = kw_get_int(0, topic, "system_flag", 0, KW_REQUIRED);
        if((system_flag & sf2_no_md_disk)) {
            return -1;
        }
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "NO topic_idx_fd",
            NULL
        );
    }

    return fd;
}

/***************************************************************************
   Open topic
 ***************************************************************************/
PUBLIC json_t *tranger_open_topic( // WARNING returned json IS NOT YOURS
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
        return 0;
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
        return 0;
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
    kw_get_int(gobj, topic, "__last_rowid__", 0, KW_CREATE);

    kw_get_int(gobj, topic, "topic_idx_fd", -1, KW_CREATE);

    kw_get_dict(gobj, topic, "fd_opened_files", json_object(), KW_CREATE);
    kw_get_dict(gobj, topic, "file_opened_files", json_object(), KW_CREATE);
    kw_get_dict(gobj, topic, "lists", json_array(), KW_CREATE);

    /*
     *  Open topic index
     */
    system_flag2_t system_flag = kw_get_int(gobj, topic, "system_flag", 0, KW_REQUIRED);
    if(!(system_flag & sf2_no_md_disk)) {
        if(open_topic_idx_fd(tranger, topic)<0) {
            json_decref(topic);
            return 0;
        }
    }

    return topic;
}

/***************************************************************************
   Get topic by his topic_name.
   Topic is opened if it's not opened.
   HACK topic can exists in disk, but it's not opened until tranger_open_topic()
 ***************************************************************************/
PUBLIC json_t *tranger_topic( // WARNING returned json IS NOT YOURS
    json_t *tranger,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

    json_t *topic = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, 0);
    if(!topic) {
        topic = tranger_open_topic(tranger, topic_name, FALSE);
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
PUBLIC json_int_t tranger_topic_size(
    json_t *topic
)
{
    return kw_get_int(0, topic, "__last_rowid__", 0, KW_REQUIRED);
}

/***************************************************************************
   Return topic name of topic.
 ***************************************************************************/
PUBLIC const char *tranger_topic_name(
    json_t *topic
)
{
    return kw_get_str(0, topic, "topic_name", "", KW_REQUIRED);
}

/***************************************************************************
   Close record topic.
 ***************************************************************************/
PUBLIC int tranger_close_topic(
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

    close_topic_idx_fd(tranger, topic);

    const char *key;
    json_t *jn_value;
    json_t *fd_opened_files = kw_get_dict(gobj, topic, "fd_opened_files", 0, KW_REQUIRED);
    json_object_foreach(fd_opened_files, key, jn_value) {
        int fd = (int)kw_get_int(gobj, fd_opened_files, key, 0, KW_REQUIRED);
        if(fd >= 0) {
            close(fd);
        }
    }
    json_t *file_opened_files = kw_get_dict(gobj, topic, "file_opened_files", 0, KW_REQUIRED);
    json_object_foreach(file_opened_files, key, jn_value) {
        FILE *file = (FILE *)(size_t)kw_get_int(gobj, file_opened_files, key, 0, KW_REQUIRED);
        if(file) {
            fclose(file);
        }
    }

    json_t *jn_topics = kw_get_dict_value(gobj, tranger, "topics", 0, KW_REQUIRED);
    json_object_del(jn_topics, topic_name);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_fd_opened_files(
    json_t *topic
)
{
    json_t *jn_value;
    const char *key;
    void *tmp;

    json_t *fd_opened_files = kw_get_dict(0, topic, "fd_opened_files", 0, KW_REQUIRED);
    json_object_foreach_safe(fd_opened_files, tmp, key, jn_value) {
        int fd = (int)kw_get_int(0, fd_opened_files, key, 0, KW_REQUIRED);
        if(fd >= 0) {
            close(fd);
        }
        json_object_del(fd_opened_files, key);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_file_opened_files(
    json_t *topic
)
{
    json_t *jn_value;
    const char *key;
    void *tmp;

    json_t *file_opened_files = kw_get_dict(0, topic, "file_opened_files", 0, KW_REQUIRED);
    json_object_foreach_safe(file_opened_files, tmp, key, jn_value) {
        FILE *file = (FILE *)(size_t)kw_get_int(0, file_opened_files, key, 0, KW_REQUIRED);
        if(file) {
            fclose(file);
        }
        json_object_del(file_opened_files, key);
    }

    return 0;
}

/***************************************************************************
   Delete topic. Alias delete table.
 ***************************************************************************/
PUBLIC int tranger_delete_topic(
    json_t *tranger,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);

    json_t *topic = tranger_topic(tranger, topic_name);
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
    tranger_close_topic(tranger, topic_name);

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
PUBLIC json_t *tranger_backup_topic(
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
    tranger_close_topic(tranger, topic_name);

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
    json_t *topic = tranger_create_topic(
        tranger,
        topic_name,
        kw_get_str(gobj, topic_desc, "pkey", "", KW_REQUIRED),
        kw_get_str(gobj, topic_desc, "tkey", "", KW_REQUIRED),
        (system_flag2_t)kw_get_int(gobj, topic_desc, "system_flag", 0, KW_REQUIRED),
        topic_cols,     // owned
        jn_topic_var    // owned
    );

    JSON_DECREF(topic_desc)
    return topic;
}

#ifdef PEPE
/***************************************************************************
   Write topic var
 ***************************************************************************/
PUBLIC int tranger_write_topic_var(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_topic_var  // owned
)
{
    if(!jn_topic_var) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "jn_topic_var EMPTY",
            NULL
        );
        return -1;
    }
    if(!json_is_object(jn_topic_var)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "jn_topic_var is NOT DICT",
            NULL
        );
        JSON_DECREF(jn_topic_var);
        return -1;
    }

    BOOL master = kw_get_bool(tranger, "master", 0, KW_REQUIRED);
    if(!master) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Only master can write",
            NULL
        );
        JSON_DECREF(jn_topic_var);
        return -1;
    }
    char directory[PATH_MAX];
    snprintf(
        directory,
        sizeof(directory),
        "%s/%s",
        kw_get_str(tranger, "directory", "", KW_REQUIRED),
        topic_name
    );

    json_t *topic_var = load_json_from_file(
        directory,
        "topic_var.json"
    );
    if(!topic_var) {
        topic_var = json_object();
    }
    json_object_update(topic_var, jn_topic_var);
    json_decref(jn_topic_var);

    json_t *topic = kw_get_subdict_value(tranger, "topics", topic_name, 0, 0);
    if(topic) {
        kw_update_except(topic, topic_var, topic_fields); // data from topic disk are inmutable!
    }

    save_json_to_file(
        directory,
        "topic_var.json",
        (int)kw_get_int(tranger, "xpermission", 0, KW_REQUIRED),
        (int)kw_get_int(tranger, "rpermission", 0, KW_REQUIRED),
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
PUBLIC int tranger_write_topic_cols(
    json_t *tranger,
    const char *topic_name,
    json_t *jn_topic_cols  // owned
)
{
    if(!jn_topic_cols) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "jn_topic_cols EMPTY",
            NULL
        );
        return -1;
    }
    if(!json_is_object(jn_topic_cols) && !json_is_array(jn_topic_cols)) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "jn_topic_cols MUST BE dict or list",
            NULL
        );
        JSON_DECREF(jn_topic_cols);
        return -1;
    }

    BOOL master = kw_get_bool(tranger, "master", 0, KW_REQUIRED);
    if(!master) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Only master can write",
            NULL
        );
        JSON_DECREF(jn_topic_cols);
        return -1;
    }
    char directory[PATH_MAX];
    snprintf(
        directory,
        sizeof(directory),
        "%s/%s",
        kw_get_str(tranger, "directory", "", KW_REQUIRED),
        topic_name
    );

    json_t *topic = kw_get_subdict_value(tranger, "topics", topic_name, 0, 0);
    if(topic) {
        json_object_set(
            topic,
            "cols",
            jn_topic_cols
        );
    }

    save_json_to_file(
        directory,
        "topic_cols.json",
        (int)kw_get_int(tranger, "xpermission", 0, KW_REQUIRED),
        (int)kw_get_int(tranger, "rpermission", 0, KW_REQUIRED),
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
PUBLIC json_t *tranger_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
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
        json_incref(topic),
        fields
    );

    json_t *cols = kwid_new_list("verbose", topic, "cols");
    json_object_set_new(desc, "cols", cols);

    return desc;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *tranger_list_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
    if(!topic) {
        // Error already logged
        return 0;
    }
    return kwid_new_list("verbose", topic, "cols");
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *tranger_dict_topic_desc( // Return MUST be decref
    json_t *tranger,
    const char *topic_name
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
    if(!topic) {
        // Error already logged
        return 0;
    }
    return kwid_new_dict("verbose", topic, "cols");
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *tranger_filter_topic_fields(
    json_t *tranger,
    const char *topic_name,
    json_t *kw  // owned
)
{
    json_t *cols = tranger_dict_topic_desc(tranger, topic_name);
    if(!cols) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic without cols",
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(kw);
        return 0;
    }
    json_t *new_record = json_object();

    const char *field; json_t *col;
    json_object_foreach(cols, field, col) {
        json_t *value = kw_get_dict_value(kw, field, 0, 0);
        json_object_set(new_record, field, value);
    }

    JSON_DECREF(cols);
    JSON_DECREF(kw);
    return new_record;
}

/***************************************************************************
 *  Get fullpath of filename in content/data level
 *  The directory will be create if it's master
 ***************************************************************************/
PRIVATE char *get_record_content_fullpath(
    json_t *tranger,
    json_t *topic,
    char *bf,
    int bfsize,
    uint64_t __t__ // WARNING must be in seconds!
)
{
    struct tm *tm = gmtime((time_t *)&__t__);
    const char *topic_name = tranger_topic_name(topic);

    char format[64];
    const char *filename_mask = kw_get_str(tranger, "filename_mask", "%Y-%m-%d.json", KW_REQUIRED);

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

    const char *topic_dir = kw_get_str(topic, "directory", "", KW_REQUIRED);

    snprintf(bf, bfsize, "%s/data/%s-%s.json",
        topic_dir,
        topic_name,
        format
    );

    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_content_fd(json_t *tranger, json_t *topic, uint64_t __t__)
{
    system_flag_t system_flag = kw_get_int(topic, "system_flag", 0, KW_REQUIRED);
    if((system_flag & sf_no_record_disk)) {
        return -1;
    }

    BOOL master = kw_get_bool(tranger, "master", 0, KW_REQUIRED);

    /*-----------------------------*
     *      Check file
     *-----------------------------*/
    char full_path[PATH_MAX];
    get_record_content_fullpath(
        tranger,
        topic,
        full_path,
        sizeof(full_path),
        (system_flag & sf_t_ms)? __t__/1000:__t__
    );

    if(access(full_path, 0)!=0) {
        /*----------------------------------------*
         *  Create (only)the new file if master
         *----------------------------------------*/
        if(!master) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "path",         "%s", full_path,
                "msg",          "%s", "content file not found",
                NULL
            );
            return -1;
        }

        int fp = newfile(full_path, (int)kw_get_int(tranger, "rpermission", 0, KW_REQUIRED), FALSE);
        if(fp < 0) {
            if(errno == EMFILE) {
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "path",         "%s", full_path,
                    "msg",          "%s", "TOO MANY OPEN FILES",
                    NULL
                );
                close_fd_opened_files(topic);

                fp = newfile(full_path, (int)kw_get_int(tranger, "rpermission", 0, KW_REQUIRED), FALSE);
                if(fp < 0) {
                    log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                        "msg",          "%s", "Cannot create json file",
                        "filename",     "%s", full_path,
                        "errno",        "%d", errno,
                        "serrno",       "%s", strerror(errno),
                        NULL
                    );
                    return -1;
                }
            } else {
                log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "Cannot create json file",
                    "filename",     "%s", full_path,
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
    int fd = (int)kw_get_int(
        kw_get_dict(topic, "fd_opened_files", 0, KW_REQUIRED),
        full_path,
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
            log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot open content file",
                "path",         "%s", full_path,
                "errno",        "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        json_object_set_new(
            kw_get_dict(topic, "fd_opened_files", 0, KW_REQUIRED),
            full_path,
            json_integer(fd)
        );
    }
    return fd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE FILE * get_content_file(json_t *tranger, json_t *topic, uint64_t __t__)
{
    system_flag_t system_flag = kw_get_int(topic, "system_flag", 0, KW_REQUIRED);
    if((system_flag & sf_no_record_disk)) {
        return 0;
    }

    /*-----------------------------*
     *      Check file
     *-----------------------------*/
    char full_path[PATH_MAX];
    get_record_content_fullpath(
        tranger,
        topic,
        full_path,
        sizeof(full_path),
        (system_flag & sf_t_ms)? __t__/1000:__t__
    );

    if(access(full_path, 0)!=0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "path",         "%s", full_path,
            "msg",          "%s", "content file not found",
            NULL
        );
        return 0;
    }

    /*-----------------------------*
     *      Open content file
     *-----------------------------*/
    FILE *file = (FILE *)(size_t)kw_get_int(
        kw_get_dict(topic, "file_opened_files", 0, KW_REQUIRED),
        full_path,
        0,
        0
    );

    if(!file) {
        file = fopen(full_path, "r");
        if(!file) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot open content file",
                "path",         "%s", full_path,
                "errno",        "%s", strerror(errno),
                NULL
            );
            return 0;
        }
        json_object_set_new(
            kw_get_dict(topic, "file_opened_files", 0, KW_REQUIRED),
            full_path,
            json_integer((json_int_t)(size_t)file)
        );
    }

    json_object_set_new(
        kw_get_dict(topic, "file_opened_files", 0, KW_REQUIRED),
        full_path,
        json_integer((json_int_t)(size_t)file)
    );
    return file;
}

/***************************************************************************
 *  Return json object with record metadata
 ***************************************************************************/
PUBLIC json_t *tranger_md2json(md2_record_t *md_record)
{
    json_t *jn_md = json_object();
    json_object_set_new(jn_md, "__rowid__", json_integer(md_record->__rowid__));
    json_object_set_new(jn_md, "__t__", json_integer(md_record->__t__));
    json_object_set_new(jn_md, "__tm__", json_integer(md_record->__tm__));
    json_object_set_new(jn_md, "__offset__", json_integer(md_record->__offset__));
    json_object_set_new(jn_md, "__size__", json_integer(md_record->__size__));
    json_object_set_new(jn_md, "__user_flag__", json_integer(md_record->__user_flag__));
    json_object_set_new(jn_md, "__system_flag__", json_integer(md_record->__system_flag__));

    return jn_md;
}

/***************************************************************************
 *  Append a new record.
 *  Return the new record's metadata.
 ***************************************************************************/
PUBLIC int tranger_append_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t __t__,         // if 0 then the time will be set by TimeRanger with now time
    uint32_t user_flag,
    md2_record_t *md_record, // required
    json_t *jn_record       // owned
)
{
    if(!jn_record || jn_record->refcount <= 0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "jn_record NULL",
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }

    BOOL master = kw_get_bool(tranger, "master", 0, KW_REQUIRED);
    if(!master) {
        log_error(LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot append record, NO master",
            "topic",        "%s", topic_name,
            NULL
        );
        log_debug_json(0, jn_record, "Cannot append record, NO master");
        JSON_DECREF(jn_record);
        return -1;
    }

    json_t *topic = tranger_topic(tranger, topic_name);
    if(!topic) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Cannot append record, topic not found",
            "topic",        "%s", topic_name,
            NULL
        );
        log_debug_json(0, jn_record, "Cannot append record, topic not found");
        JSON_DECREF(jn_record);
        return -1;
    }

    /*--------------------------------------------*
     *  If time not specified, use the now time
     *--------------------------------------------*/
    uint32_t __system_flag__ = kw_get_int(topic, "system_flag", 0, KW_REQUIRED);
    if(!__t__) {
        if(__system_flag__ & (sf_t_ms)) {
            __t__ = time_in_miliseconds();
        } else {
            __t__ = time_in_seconds();
        }
    }

    /*--------------------------------------------*
     *  Get last_rowid
     *--------------------------------------------*/
    json_int_t __last_rowid__ = kw_get_int(topic, "__last_rowid__", 0, KW_REQUIRED);

    /*--------------------------------------------*
     *  Recover file corresponds to __t__
     *--------------------------------------------*/
    int content_fp = get_content_fd(tranger, topic, __t__);  // Can be -1, sf_no_disk

    /*--------------------------------------------*
     *  New record always at the end
     *--------------------------------------------*/
    uint64_t __offset__ = 0;
    if(content_fp >= 0) {
        __offset__ = lseek64(content_fp, 0, SEEK_END);
        if(__offset__ == -1) {
            log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot append record, lseek64() FAILED",
                "topic",        "%s", topic_name,
                "errno",        "%s", strerror(errno),
                NULL
            );
            log_debug_json(0, jn_record, "Cannot append record, lseek64() FAILED");
            JSON_DECREF(jn_record);
            return -1;
        }
    }

    /*--------------------------------------------*
     *  Prepare new record metadata
     *--------------------------------------------*/
    memset(md_record, 0, sizeof(md2_record_t));
    md_record->__user_flag__ = user_flag;
    md_record->__system_flag__ = __system_flag__;
    md_record->__t__ = __t__;
    md_record->__rowid__ = __last_rowid__ + 1;
    md_record->__offset__ = __offset__;

    /*--------------------------------------------*
     *  Get and save the t-key if exists
     *--------------------------------------------*/
    const char *tkey = kw_get_str(topic, "tkey", "", KW_REQUIRED);
    if(!empty_string(tkey)) {
        json_t *jn_tval = kw_get_dict_value(jn_record, tkey, 0, 0);
        if(!jn_tval) {
            md_record->__tm__ = 0; // No tkey value, mark with 0
        } else {
            if(json_is_string(jn_tval)) {
                int offset;
                timestamp_t timestamp; // TODO if is it a millisecond time?
                parse_date_basic(json_string_value(jn_tval), &timestamp, &offset);
                md_record->__tm__ = timestamp;
            } else if(json_is_number(jn_tval)) {
                md_record->__tm__ = json_number_value(jn_tval);
            } else {
                md_record->__tm__ = 0; // No tkey value, mark with 0
            }
        }
    } else {
        md_record->__tm__ = 0;  // No tkey value, mark with 0
    }

    /*--------------------------------------------*
     *  Get and save the primary-key if exists
     *--------------------------------------------*/
    const char *pkey = kw_get_str(topic, "pkey", "", KW_REQUIRED);
    system_flag_t system_flag_key_type = md_record->__system_flag__ & KEY_TYPE_MASK;

    switch(system_flag_key_type) {
        case sf_string_key:
            {
                const char *key_value = kw_get_str(jn_record, pkey, 0, 0);
                if(!key_value) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_JSON_ERROR,
                        "msg",          "%s", "Cannot append record, Record without pkey",
                        "topic",        "%s", topic_name,
                        "pkey",         "%s", pkey,
                        NULL
                    );
                    log_debug_json(0, jn_record, "Cannot append record, Record without pkey");
                    JSON_DECREF(jn_record);
                    return -1;
                }
                if(strlen(key_value) > sizeof(md_record->key.s)-1) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "key value TOO large",
                        "topic",        "%s", topic_name,
                        "key_value",    "%s", pkey,
                        "size",         "%d", strlen(key_value),
                        "maxsize",      "%d", sizeof(md_record->key.s)-1,
                        NULL
                    );
                }
                strncpy(md_record->key.s, key_value, sizeof(md_record->key.s)-1);
            }
            break;

        case sf_int_key:
            if(kw_has_path(jn_record, pkey)) {
                md_record->key.i = kw_get_int(
                    jn_record,
                    pkey,
                    0,
                    KW_REQUIRED|KW_WILD_NUMBER
                );
            } else {
                md_record->key.i = md_record->__rowid__;
                json_object_set_new(
                    jn_record,
                    pkey,
                    json_integer(md_record->key.i)
                );
            }
            break;

        case sf_rowid_key:
            md_record->key.i = md_record->__rowid__;
            json_object_set_new(
                jn_record,
                pkey,
                json_integer(md_record->key.i)
            );
            break;

        default:
            // No pkey
            break;
    }

    /*--------------------------------------------*
     *  Get the record's content, always json
     *--------------------------------------------*/
    if(content_fp >= 0) {
        char *srecord = json_dumps(jn_record, JSON_COMPACT|JSON_ENCODE_ANY);
        if(!srecord) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_JSON_ERROR,
                "msg",          "%s", "Cannot append record, json_dumps() FAILED",
                "topic",        "%s", topic_name,
                NULL
            );
            log_debug_json(0, jn_record, "Cannot append record, json_dumps() FAILED");
            JSON_DECREF(jn_record);
            return -1;
        }
        size_t size = strlen(srecord);

        gbuffer_t *gbuf = gbuf_create(size, size, 0, 0);
        if(!gbuf) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "Cannot append record. gbuf_create() FAILED",
                "topic",        "%s", topic_name,
                NULL
            );
            log_debug_json(0, jn_record, "Cannot append record, gbuf_create() FAILED");
            jsonp_free(srecord);
            JSON_DECREF(jn_record);
            return -1;
        }
        gbuf_append(gbuf, srecord, strlen(srecord));
        jsonp_free(srecord);

        /*
         *  Saving: first compress, second encrypt
         */
        if(md_record->__system_flag__ & sf_zip_record) {
            // if(topic->compress_callback) { TODO
            //     gbuf = topic->compress_callback(
            //         topic->user_data,
            //         topic,
            //         gbuf    // must be owned
            //     );
            // }
        }
        if(md_record->__system_flag__ & sf_cipher_record) {
            // if(topic->encrypt_callback) { TODO
            //     gbuf = topic->encrypt_callback(
            //         topic->user_data,
            //         topic,
            //         gbuf    // must be owned
            //     );
            // }
        }
        md_record->__size__ = gbuf_leftbytes(gbuf) + 1; // put the final null

        /*-------------------------*
         *  Write record content
         *-------------------------*/
        char *p = gbuf_cur_rd_pointer(gbuf);
        int ln = write( // write new (record content)
            content_fp,
            p,
            md_record->__size__
        );
        gbuf_decref(gbuf);
        if(ln != md_record->__size__) {
            log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot append record, write FAILED",
                "topic",        "%s", topic_name,
                "errno",        "%s", strerror(errno),
                NULL
            );
            log_debug_json(0, jn_record, "Cannot append record, write FAILED");
            JSON_DECREF(jn_record);
            return -1;
        }
    }

    /*--------------------------------------------*
     *  Save md, to file
     *--------------------------------------------*/
    new_record_md_to_file(tranger, topic, md_record);
    json_object_set_new(topic, "__last_rowid__", json_integer(md_record->__rowid__));

    /*--------------------------------------------*
     *  Call callbacks
     *--------------------------------------------*/
    json_t *lists = kw_get_list(topic, "lists", 0, KW_REQUIRED);
    int idx;
    json_t *list;
    json_array_foreach(lists, idx, list) {
        if(tranger_match_record(
                tranger,
                topic,
                kw_get_dict(list, "match_cond", 0, 0),
                md_record,
                0
            )) {
            tranger_load_record_callback_t load_record_callback =
                (tranger_load_record_callback_t)(size_t)kw_get_int(
                list,
                "load_record_callback",
                0,
                0
            );
            if(load_record_callback) {
                // Inform user list: record in real time
                JSON_INCREF(jn_record);
                int ret = load_record_callback(
                    tranger,
                    topic,
                    list,
                    md_record,
                    jn_record
                );
                if(ret < 0) {
                    JSON_DECREF(jn_record);
                    return -1;
                } else if(ret>0) {
                    json_object_set_new(jn_record, "__md_tranger__", tranger_md2json(md_record));
                    json_array_append(
                        kw_get_list(list, "data", 0, KW_REQUIRED),
                        jn_record
                    );
                }
            } else {
                json_object_set_new(jn_record, "__md_tranger__", tranger_md2json(md_record));
                json_array_append(
                    kw_get_list(list, "data", 0, KW_REQUIRED),
                    jn_record
                );
            }
        }
    }

    JSON_DECREF(jn_record);
    return 0;
}

/***************************************************************************
   Write new record metadata to file
 ***************************************************************************/
PRIVATE int new_record_md_to_file(
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record)
{
    int fd = get_topic_idx_fd(tranger, topic);
    if(fd < 0) {
        // Error already logged
        return -1;
    }
    uint64_t offset = lseek64(fd, 0, SEEK_END);
    if(offset != ((md_record->__rowid__-1) * sizeof(md2_record_t))) {
        log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "topic_idx.md corrupted",
            "topic",        "%s", kw_get_str(topic, "directory", 0, KW_REQUIRED),
            "offset",       "%lu", (unsigned long)offset,
            "rowid",        "%lu", (unsigned long)md_record->__rowid__,
            NULL
        );
        return -1;
    }

    size_t ln = write( // write new (record content)
        fd,
        md_record,
        sizeof(md2_record_t)
    );
    if(ln != sizeof(md2_record_t)) {
        log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot save record metadata, write FAILED",
            "topic",        "%s", tranger_topic_name(topic),
            "errno",        "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
    Get md record by rowid (by fd, for write)
 ***************************************************************************/
PRIVATE int _get_md_record_for_wr(
    json_t *tranger,
    json_t *topic,
    uint64_t rowid,
    md2_record_t *md_record,
    BOOL verbose
)
{
    memset(md_record, 0, sizeof(md2_record_t));

    if(rowid == 0) {
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "rowid 0",
                "topic",        "%s", tranger_topic_name(topic),
                "rowid",        "%lu", (unsigned long)rowid,
                NULL
            );
        }
        return -1;
    }

    json_int_t __last_rowid__ = kw_get_int(topic, "__last_rowid__", 0, KW_REQUIRED);
    if(__last_rowid__ <= 0) {
        return -1;
    }

    if(rowid > __last_rowid__) {
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "rowid greater than last_rowid",
                "topic",        "%s", tranger_topic_name(topic),
                "rowid",        "%lu", (unsigned long)rowid,
                "last_rowid",   "%lu", (unsigned long)__last_rowid__,
                NULL
            );
        }
        return -1;
    }

    int fd = get_topic_idx_fd(tranger, topic);
    if(fd < 0) {
        // Error already logged
        return -1;
    }

    off64_t offset = (off64_t) ((rowid-1) * sizeof(md2_record_t));
    off64_t offset_ = lseek64(fd, offset, SEEK_SET);
    if(offset != offset_) {
        log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "topic_idx.md corrupted",
            "topic",        "%s", kw_get_str(topic, "directory", 0, KW_REQUIRED),
            "offset",       "%lu", (unsigned long)offset,
            "offset_",      "%lu", (unsigned long)offset_,
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
        log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read record metadata, read FAILED",
            "topic",        "%s", tranger_topic_name(topic),
            "errno",        "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    if(md_record->__rowid__ != rowid) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "md_record corrupted, item rowid not match",
            "topic",        "%s", tranger_topic_name(topic),
            "rowid",        "%lu", (unsigned long)rowid,
            "__rowid__",    "%lu", (unsigned long)md_record->__rowid__,
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
   Re-Write new record metadata to file
 ***************************************************************************/
PRIVATE int rewrite_md_record_to_file(json_t *tranger, json_t *topic, md2_record_t *md_record)
{
    int fd = get_topic_idx_fd(tranger, topic);
    if(fd < 0) {
        // Error already logged
        return -1;
    }
    off64_t offset = (off64_t) ((md_record->__rowid__-1) * sizeof(md2_record_t));
    off64_t offset_ = lseek64(fd, offset, SEEK_SET);
    if(offset != offset_) {
        log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "topic_idx.md corrupted",
            "topic",        "%s", kw_get_str(topic, "directory", 0, KW_REQUIRED),
            "offset",       "%lu", (unsigned long)offset,
            "offset_",      "%lu", (unsigned long)offset_,
            NULL
        );
        return -1;
    }

    size_t ln = write( // write new (record content)
        fd,
        md_record,
        sizeof(md2_record_t)
    );
    if(ln != sizeof(md2_record_t)) {
        log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot save record metadata, write FAILED",
            "topic",        "%s", tranger_topic_name(topic),
            "errno",        "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 *   Delete record
 ***************************************************************************/
PUBLIC int tranger_delete_record(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
    if(!topic) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot open topic",
            "topic",        "%s", topic_name,
            NULL
        );
        return -1;
    }

    md2_record_t md_record;
    if(_get_md_record_for_wr(
        tranger,
        topic,
        rowid,
        &md_record,
        TRUE
    )!=0) {
        // Error already logged
        return -1;
    }

    /*--------------------------------------------*
     *  Recover file corresponds to __t__
     *--------------------------------------------*/
    int fd = get_content_fd(tranger, topic, md_record.__t__);
    if(fd<0) {
        // Error already logged
        return -1;
    }

    uint64_t __offset__ = md_record.__offset__;
    if(lseek64(fd, __offset__, SEEK_SET) != __offset__) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read record data. lseek FAILED",
            "topic",        "%s", tranger_topic_name(topic),
            "directory",    "%s", kw_get_str(topic, "directory", 0, KW_REQUIRED),
            "errno",        "%s", strerror(errno),
            "__t__",        "%lu", (unsigned long)md_record.__t__,
            NULL
        );
        return -1;
    }

    uint64_t __t__ = md_record.__t__;
    uint64_t __size__ = md_record.__size__;

    gbuffer_t *gbuf = gbuf_create(__size__, __size__, 0, 0);
    if(!gbuf) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Cannot delete record content. gbuf_create() FAILED",
            "topic",        "%s", tranger_topic_name(topic),
            "directory",    "%s", kw_get_str(topic, "directory", 0, KW_REQUIRED),
            NULL
        );
        return -1;
    }
    char *p = gbuf_cur_rd_pointer(gbuf);

    size_t ln = write(fd, p, __size__);    // blank content
    gbuf_decref(gbuf);
    if(ln != __size__) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot delete record content, write FAILED",
            "topic",        "%s", tranger_topic_name(topic),
            "directory",    "%s", kw_get_str(topic, "directory", 0, KW_REQUIRED),
            "errno",        "%s", strerror(errno),
            "ln",           "%d", ln,
            "__t__",        "%lu", (unsigned long)__t__,
            "__size__",     "%lu", (unsigned long)__size__,
            "__offset__",   "%lu", (unsigned long)__offset__,
            NULL
        );
        return -1;
    }

    md_record.__system_flag__ |= sf_deleted_record;
    if(rewrite_md_record_to_file(tranger, topic, &md_record)<0) {
        return -1;
    }

    return 0;
}

/***************************************************************************
    Write record mark1
 ***************************************************************************/
PUBLIC int tranger_write_mark1(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid,
    BOOL set
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
    if(!topic) {
        log_error(0,
            "gobj",         "%s", __FILE__,
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
    if(_get_md_record_for_wr(
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
        md_record.__system_flag__ |= sf_mark1;
    } else {
        /*
         *  Reset
         */
        md_record.__system_flag__ &= ~sf_mark1;
    }

    if(rewrite_md_record_to_file(tranger, topic, &md_record)<0) {
        return -1;
    }
    return 0;
}

/***************************************************************************
    Write record user flag
 ***************************************************************************/
PUBLIC int tranger_write_user_flag(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid,
    uint32_t user_flag
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
    if(!topic) {
        log_error(0,
            "gobj",         "%s", __FILE__,
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
    if(_get_md_record_for_wr(
        tranger,
        topic,
        rowid,
        &md_record,
        TRUE
    )!=0) {
        return -1;
    }

    md_record.__user_flag__= user_flag;

    if(rewrite_md_record_to_file(tranger, topic, &md_record)<0) {
        return -1;
    }
    return 0;
}

/***************************************************************************
    Write record user flag using mask
 ***************************************************************************/
PUBLIC int tranger_set_user_flag(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid,
    uint32_t mask,
    BOOL set
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
    if(!topic) {
        log_error(0,
            "gobj",         "%s", __FILE__,
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
    if(_get_md_record_for_wr(
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
        md_record.__user_flag__ |= mask;
    } else {
        /*
         *  Reset
         */
        md_record.__user_flag__ &= ~mask;
    }

    if(rewrite_md_record_to_file(tranger, topic, &md_record)<0) {
        return -1;
    }

    return 0;
}

/***************************************************************************
    Read record user flag (for writing mode)
 ***************************************************************************/
PUBLIC uint32_t tranger_read_user_flag(
    json_t *tranger,
    const char *topic_name,
    uint64_t rowid
)
{
    json_t *topic = tranger_topic(tranger, topic_name);
    if(!topic) {
        log_error(0,
            "gobj",         "%s", __FILE__,
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
    if(_get_md_record_for_wr(
        tranger,
        topic,
        rowid,
        &md_record,
        TRUE
    )!=0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Tranger record NOT FOUND",
            "topic",        "%s", topic_name,
            "rowid",        "%lu", (unsigned long)rowid,
            NULL
        );
        return 0;
    }
    return md_record.__user_flag__;
}

/***************************************************************************
    Read records
 ***************************************************************************/
PUBLIC json_t *tranger_open_list(
    json_t *tranger,
    json_t *jn_list // owned
)
{
    char title[256];

    json_t *list = create_json_record(list_json_desc);
    json_object_update(list, jn_list);
    JSON_DECREF(jn_list);

    BOOL master = kw_get_bool(tranger, "master", 0, KW_REQUIRED);

    json_t *topic = tranger_topic(tranger, kw_get_str(list, "topic_name", "", KW_REQUIRED));
    if(!topic) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "tranger_open_list: what topic?",
            NULL
        );
        JSON_DECREF(list);
        return 0;
    }

    json_t *match_cond = kw_get_dict(list, "match_cond", 0, KW_REQUIRED);

    int trace_level = (int)kw_get_int(tranger, "trace_level", 0, 0);

    tranger_load_record_callback_t load_record_callback =
        (tranger_load_record_callback_t)(size_t)kw_get_int(
        list,
        "load_record_callback",
        0,
        0
    );

    /*
     *  Add list to topic
     */
    json_array_append_new(
        kw_get_dict_value(topic, "lists", 0, KW_REQUIRED),
        list
    );
    /*
     *  Load volatil, defining in run-time
     */
    json_t *data = kw_get_list(list, "data", json_array(), KW_CREATE);

    /*
     *  Load from disk
     */
    json_int_t __last_rowid__ = kw_get_int(topic, "__last_rowid__", 0, KW_REQUIRED);
    if(__last_rowid__ <= 0) {
        if(master) {
            return list;
        }
        // HACK no "master" (tranger readonly) don't have updated __last_rowid__
    }

    BOOL only_md = kw_get_bool(match_cond, "only_md", 0, 0);
    BOOL backward = kw_get_bool(match_cond, "backward", 0, 0);

    BOOL end = FALSE;
    md2_record_t md_record;
    memset(&md_record, 0, sizeof(md2_record_t));
    if(!backward) {
        json_int_t from_rowid = kw_get_int(match_cond, "from_rowid", 0, 0);
        if(from_rowid>0) {
            end = tranger_get_record(tranger, topic, from_rowid, &md_record, TRUE);
        } else if(from_rowid<0 && (__last_rowid__ + from_rowid)>0) {
            from_rowid = __last_rowid__ + from_rowid;
            end = tranger_get_record(tranger, topic, from_rowid, &md_record, TRUE);
        } else {
            end = tranger_first_record(tranger, topic, &md_record);
        }
    } else {
        json_int_t to_rowid = kw_get_int(match_cond, "to_rowid", 0, 0);
        if(to_rowid>0) {
            end = tranger_get_record(tranger, topic, to_rowid, &md_record, TRUE);
        } else if(to_rowid<0 && (__last_rowid__ + to_rowid)>0) {
            to_rowid = __last_rowid__ + to_rowid;
            end = tranger_get_record(tranger, topic, to_rowid, &md_record, TRUE);
        } else {
            end = tranger_last_record(tranger, topic, &md_record);
        }
    }

    while(!end) {
        if(trace_level) {
            print_md1_record(tranger, topic, &md_record, title, sizeof(title));
        }
        if(tranger_match_record(
                tranger,
                topic,
                match_cond,
                &md_record,
                &end
            )) {

            if(trace_level) {
                trace_msg0("ok - %s", title);
            }

            json_t *jn_record = 0;
            md_record.__system_flag__ |= sf_loading_from_disk;
            if(!only_md) {
                jn_record = tranger_read_record_content(tranger, topic, &md_record);
            }

            if(load_record_callback) {
                /*--------------------------------------------*
                 *  Put record metadata in json record
                 *--------------------------------------------*/
                // Inform user list: record from disk
                JSON_INCREF(jn_record);
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
                    JSON_DECREF(jn_record);
                    break;
                } else if(ret > 0) {
                    if(!jn_record) {
                        jn_record = json_object();
                    }
                    json_object_set_new(jn_record, "__md_tranger__", tranger_md2json(&md_record));
                    json_array_append_new(
                        data,
                        jn_record // owned
                    );
                } else { // == 0
                    // user's callback manages the record
                    JSON_DECREF(jn_record);
                }
            } else {
                if(!jn_record) {
                    jn_record = json_object();
                }
                json_object_set_new(jn_record, "__md_tranger__", tranger_md2json(&md_record));
                json_array_append_new(
                    data,
                    jn_record // owned
                );
            }
        } else {
            if(trace_level) {
                trace_msg0("XX - %s", title);
            }
        }
        if(end) {
            break;
        }
        if(!backward) {
            end = tranger_next_record(tranger, topic, &md_record);
        } else {
            end = tranger_prev_record(tranger, topic, &md_record);
        }
    }

    return list;
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
    Get list by his (optional) id
 ***************************************************************************/
PUBLIC json_t *tranger_get_list(
    json_t *tranger,
    const char *id
)
{
    if(empty_string(id)) {
        return 0;
    }
    json_t *topics = kw_get_dict_value(tranger, "topics", 0, KW_REQUIRED);

    const char *topic_name; json_t *topic;
    json_object_foreach(topics, topic_name, topic) {
        json_t *lists = kw_get_list(topic, "lists", 0, KW_REQUIRED);
        int idx; json_t *list;
        json_array_foreach(lists, idx, list) {
            const char *list_id = kw_get_str(list, "id", "", 0);
            if(strcmp(id, list_id)==0) {
                return list;
            }
        }
    }
    return 0;
}

/***************************************************************************
    Close list
 ***************************************************************************/
PUBLIC int tranger_close_list(
    json_t *tranger,
    json_t *list
)
{
    if(!list) {
        // silence
        return -1;
    }
    const char *topic_name = kw_get_str(list, "topic_name", "", KW_REQUIRED);
    json_t *topic = kw_get_subdict_value(tranger, "topics", topic_name, 0, 0);
    if(topic) {
        json_array_remove(
            kw_get_dict_value(topic, "lists", 0, KW_REQUIRED),
            json_array_find_idx(kw_get_dict_value(topic, "lists", 0, KW_REQUIRED), list)
        );
    }
    return 0;
}

/***************************************************************************
    Get md record by rowid (by FILE, for reads)
 ***************************************************************************/
PUBLIC int tranger_get_record(
    json_t *tranger,
    json_t *topic,
    uint64_t rowid,
    md2_record_t *md_record,
    BOOL verbose
)
{
    BOOL master = kw_get_bool(tranger, "master", 0, KW_REQUIRED);

    memset(md_record, 0, sizeof(md2_record_t));

    if(rowid == 0) {
        if(verbose) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "rowid 0",
                "topic",        "%s", tranger_topic_name(topic),
                "rowid",        "%lu", (unsigned long)rowid,
                NULL
            );
        }
        return -1;
    }

    uint64_t __last_rowid__ = (uint64_t)kw_get_int(topic, "__last_rowid__", 0, KW_REQUIRED);

    // HACK no "master" (tranger readonly) don't have updated __last_rowid__
    if(master) {
        if(__last_rowid__ <= 0) {
            return -1;
        }
        if(rowid > __last_rowid__) {
            if(verbose) {
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "rowid greater than last_rowid",
                    "topic",        "%s", tranger_topic_name(topic),
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
        int fd = get_topic_idx_fd(tranger, topic);
        if(fd < 0) {
            // Error already logged
            return -1;
        }

        off64_t offset = (off64_t) ((rowid-1) * sizeof(md2_record_t));
        off64_t offset_ = lseek64(fd, offset, SEEK_SET);
        if(offset != offset_) {
            if(master) {
                log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "topic_idx.md corrupted, lseek64 FAILED",
                    "topic",        "%s", kw_get_str(topic, "directory", 0, KW_REQUIRED),
                    NULL
                );
            }
            return -1;
        }

        size_t ln = read(
            fd,
            md_record,
            sizeof(md2_record_t)
        );
        if(ln != sizeof(md2_record_t)) {
            // HACK no "master" (tranger readonly): we try to read new records
            if(master) {
                log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "Cannot read record metadata, read FAILED",
                    "topic",        "%s", tranger_topic_name(topic),
                    "errno",        "%s", strerror(errno),
                    NULL
                );
            } else {
                if(ln != 0) {
                    log_critical(kw_get_int(tranger, "on_critical_error", 0, KW_REQUIRED),
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                        "msg",          "%s", "Cannot read record metadata, read FAILED",
                        "topic",        "%s", tranger_topic_name(topic),
                        "errno",        "%s", strerror(errno),
                        NULL
                    );
                }
            }
            return -1;
        }
    }

    if(md_record->__rowid__ != rowid) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "md_record corrupted, item rowid not match",
            "topic",        "%s", tranger_topic_name(topic),
            "rowid",        "%lu", (unsigned long)rowid,
            "__rowid__",    "%lu", (unsigned long)md_record->__rowid__,
            NULL
        );
        return -1;
    }

    if(!master) {
        // HACK no "master" (tranger readonly): found new records
        if(md_record->__rowid__ > __last_rowid__) {
            json_object_set_new(topic, "__last_rowid__", json_integer(md_record->__rowid__));
        }
    }
    return 0;
}

/***************************************************************************
 *   Read record data
 ***************************************************************************/
PUBLIC json_t *tranger_read_record_content(
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record
)
{
    if(md_record->__system_flag__ & sf_deleted_record) {
        return 0;
    }

    /*--------------------------------------------*
     *  Recover file corresponds to __t__
     *--------------------------------------------*/
    FILE *file = get_content_file(tranger, topic, md_record->__t__);
    if(!file) {
        // Error already logged
        return 0;
    }

    uint64_t __offset__ = md_record->__offset__;
    if(fseeko64(file, __offset__, SEEK_SET)<0) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read record data. fseeko64 FAILED",
            "topic",        "%s", tranger_topic_name(topic),
            "directory",    "%s", kw_get_str(topic, "directory", 0, KW_REQUIRED),
            "errno",        "%s", strerror(errno),
            "__t__",        "%lu", (unsigned long)md_record->__t__,
            NULL
        );
        return 0;
    }

    gbuffer_t *gbuf = gbuf_create(md_record->__size__, md_record->__size__, 0, 0);
    if(!gbuf) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "Cannot read record data. gbuf_create() FAILED",
            "topic",        "%s", tranger_topic_name(topic),
            "directory",    "%s", kw_get_str(topic, "directory", 0, KW_REQUIRED),
            NULL
        );
        return 0;
    }
    char *p = gbuf_cur_rd_pointer(gbuf);

    if(fread(p, md_record->__size__, 1, file)<=0) {
        log_critical(0, // Let continue, will be a message lost
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot read record content, read FAILED",
            "topic",        "%s", tranger_topic_name(topic),
            "directory",    "%s", kw_get_str(topic, "directory", 0, KW_REQUIRED),
            "errno",        "%s", strerror(errno),
            "__t__",        "%lu", (unsigned long)md_record->__t__,
            "__size__",     "%lu", (unsigned long)md_record->__size__,
            "__offset__",   "%lu", (unsigned long)md_record->__offset__,
            NULL
        );
        gbuf_decref(gbuf);
        return 0;
    }

    /*
     *  Restoring: first decrypt, second decompress
     */
    if(md_record->__system_flag__ & sf_cipher_record) {
        // if(topic->decrypt_callback) { TODO
        //     gbuf = topic->decrypt_callback(
        //         topic->user_data,
        //         topic,
        //         gbuf    // must be owned
        //     );
        // }
    }
    if(md_record->__system_flag__ & sf_zip_record) {
        // if(topic->decompress_callback) { TODO
        //     gbuf = topic->decompress_callback(
        //         topic->user_data,
        //         topic,
        //         gbuf    // must be owned
        //     );
        // }
    }

    json_t *jn_record;
    if(empty_string(p)) {
        jn_record = json_object();
        gbuf_decref(gbuf);
    } else {
        jn_record = nonlegalstring2json(p, FALSE);
        gbuf_decref(gbuf);
        if(!jn_record) {
            log_critical(0, // Let continue, will be a message lost
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Bad data, json_loadfd() FAILED.",
                "topic",        "%s", tranger_topic_name(topic),
                "__t__",        "%lu", (unsigned long)md_record->__t__,
                "__size__",     "%lu", (unsigned long)md_record->__size__,
                "__offset__",   "%lu", (unsigned long)md_record->__offset__,
                NULL
            );
            log_debug_dump(0, p, md_record->__size__, "no jn_record");
            return 0;
        }
    }

    return jn_record;
}

/***************************************************************************
 *  Match md record
 ***************************************************************************/
PUBLIC BOOL tranger_match_record(
    json_t *tranger,
    json_t *topic,
    json_t *match_cond,  // not owned
    const md2_record_t *md_record,
    BOOL *end
)
{
    if(end) {
        *end = FALSE;
    }

    if(!match_cond || (json_object_size(match_cond)==0 && json_array_size(match_cond)==0)) {
        // No conditions, match all
        return TRUE;
    }
    md2_record_t md_record_last;
    tranger_last_record(tranger, topic, &md_record_last);
    uint64_t __last_rowid__ = md_record_last.__rowid__;
    uint64_t __last_t__ = md_record_last.__t__;
    uint64_t __last_tm__ = md_record_last.__tm__;

    BOOL backward = kw_get_bool(match_cond, "backward", 0, 0);

    if(kw_has_key(match_cond, "key")) {
        int json_type = json_typeof(json_object_get(match_cond, "key"));
        if(md_record->__system_flag__ & (sf_int_key|sf_rowid_key)) {
            switch(json_type) {
            case JSON_OBJECT:
                {
                    BOOL some = FALSE;
                    const char *key_; json_t *jn_value;
                    json_object_foreach(json_object_get(match_cond, "key"), key_, jn_value) {
                        if(md_record->key.i == atoi(key_)) {
                            some = TRUE;
                            break; // Con un match de key ya es true
                        }
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
                        if(json_is_integer(jn_value)) {
                            if(md_record->key.i == json_integer_value(jn_value)) {
                                some = TRUE;
                                break; // Con un match de key ya es true
                            }
                        } else if(json_is_string(jn_value)) {
                            if(md_record->key.i == atoi(json_string_value(jn_value))) {
                                some = TRUE;
                                break; // Con un match de key ya es true
                            }
                        }
                    }
                    if(!some) {
                        return FALSE;
                    }
                }
                break;
            default:
                {
                    json_int_t key = kw_get_int(match_cond, "key", 0, KW_REQUIRED|KW_WILD_NUMBER);
                    if(md_record->key.i != key) {
                        return FALSE;
                    }
                }
                break;
            }

        } else if(md_record->__system_flag__ & sf_string_key) {
            switch(json_type) {
            case JSON_OBJECT:
                {
                    BOOL some = FALSE;
                    const char *key_; json_t *jn_value;
                    json_object_foreach(json_object_get(match_cond, "key"), key_, jn_value) {
                        if(strncmp(md_record->key.s, key_, sizeof(md_record->key.s)-1)==0) {
                            some = TRUE;
                            break; // Con un match de key ya es true
                        }
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
                            if(strncmp(
                                    md_record->key.s,
                                    json_string_value(jn_value),
                                    sizeof(md_record->key.s)-1)==0) {
                                some = TRUE;
                                break; // Con un match de key ya es true
                            }
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
                    const char *key = kw_get_str(match_cond, "key", 0, 0);
                    if(!key) {
                        return FALSE;
                    }
                    if(strncmp(md_record->key.s, key, sizeof(md_record->key.s)-1)!=0) {
                        return FALSE;
                    }
                }
                break;
            }
        } else {
            return FALSE;
        }
    }

    if(kw_has_key(match_cond, "rkey")) {
        if(md_record->__system_flag__ & sf_string_key) {
            const char *rkey = kw_get_str(match_cond, "rkey", 0, 0);
            if(!rkey) {
                return FALSE;
            }

            regex_t _re_name;
            if(regcomp(&_re_name, rkey, REG_EXTENDED | REG_NOSUB)!=0) {
                return FALSE;
            }
            int ret = regexec(&_re_name, md_record->key.s, 0, 0, 0);
            regfree(&_re_name);
            if(ret!=0) {
                return FALSE;
            }
        } else {
            return FALSE;
        }
    }

    if(kw_has_key(match_cond, "from_rowid")) {
        json_int_t from_rowid = kw_get_int(match_cond, "from_rowid", 0, KW_WILD_NUMBER);
        if(from_rowid >= 0) {
            if(md_record->__rowid__ < from_rowid) {
                if(backward) {
                    if(end) {
                        *end = TRUE;
                    }
                }
                return FALSE;
            }
        } else {
            uint64_t x = __last_rowid__ + from_rowid;
            if(md_record->__rowid__ <= x) {
                if(backward) {
                    if(end) {
                        *end = TRUE;
                    }
                }
                return FALSE;
            }
        }
    }

    if(kw_has_key(match_cond, "to_rowid")) {
        json_int_t to_rowid = kw_get_int(match_cond, "to_rowid", 0, KW_WILD_NUMBER);
        if(to_rowid >= 0) {
            if(md_record->__rowid__ > to_rowid) {
                if(!backward) {
                    if(end) {
                        *end = TRUE;
                    }
                }
                return FALSE;
            }
        } else {
            uint64_t x = __last_rowid__ + to_rowid;
            if(md_record->__rowid__ > x) {
                if(!backward) {
                    if(end) {
                        *end = TRUE;
                    }
                }
                return FALSE;
            }
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
            if(md_record->__system_flag__ & (sf_tm_ms)) {
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
            if(md_record->__system_flag__ & (sf_tm_ms)) {
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
        uint32_t user_flag = kw_get_int(match_cond, "user_flag", 0, 0);
        if((md_record->__user_flag__ != user_flag)) {
            return FALSE;
        }
    }
    if(kw_has_key(match_cond, "not_user_flag")) {
        uint32_t not_user_flag = kw_get_int(match_cond, "not_user_flag", 0, 0);
        if((md_record->__user_flag__ == not_user_flag)) {
            return FALSE;
        }
    }

    if(kw_has_key(match_cond, "user_flag_mask_set")) {
        uint32_t user_flag_mask_set = kw_get_int(match_cond, "user_flag_mask_set", 0, 0);
        if((md_record->__user_flag__ & user_flag_mask_set) != user_flag_mask_set) {
            return FALSE;
        }
    }
    if(kw_has_key(match_cond, "user_flag_mask_notset")) {
        uint32_t user_flag_mask_notset = kw_get_int(match_cond, "user_flag_mask_notset", 0, 0);
        if((md_record->__user_flag__ | ~user_flag_mask_notset) != ~user_flag_mask_notset) {
            return FALSE;
        }
    }

    if(kw_has_key(match_cond, "notkey")) {
        if(md_record->__system_flag__ & (sf_int_key|sf_rowid_key)) {
            json_int_t notkey = kw_get_int(match_cond, "key", 0, KW_REQUIRED|KW_WILD_NUMBER);
            if(md_record->key.i == notkey) {
                return FALSE;
            }
        } else if(md_record->__system_flag__ & sf_string_key) {
            const char *notkey = kw_get_str(match_cond, "key", 0, 0);
            if(!notkey) {
                return FALSE;
            }
            if(strncmp(md_record->key.s, notkey, sizeof(md_record->key.s)-1)==0) {
                return FALSE;
            }
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
PUBLIC int tranger_find_record(
    json_t *tranger,
    json_t *topic,
    json_t *match_cond,  // owned
    md2_record_t *md_record
)
{
    if(!match_cond) {
        match_cond = json_object();
    }
    BOOL backward = kw_get_bool(match_cond, "backward", 0, 0);

    BOOL end = FALSE;
    if(!backward) {
        end = tranger_first_record(tranger, topic, md_record);
    } else {
        end = tranger_last_record(tranger, topic, md_record);
    }
    while(!end) {
        if(tranger_match_record(tranger, topic, match_cond, md_record, &end)) {
            JSON_DECREF(match_cond);
            return 0;
        }
        if(end) {
            break;
        }
        if(!backward) {
            end = tranger_next_record(tranger, topic, md_record);
        } else {
            end = tranger_prev_record(tranger, topic, md_record);
        }
    }
    JSON_DECREF(match_cond);
    return -1;
}

/***************************************************************************
 *  Read first md record
 ***************************************************************************/
PUBLIC int tranger_first_record(
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record
)
{
    json_int_t rowid = 1;
    if(tranger_get_record(
        tranger,
        topic,
        rowid,
        md_record,
        FALSE
    )<0) {
        return -1;
    }

    if(md_record->__rowid__ != 1) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "md_records corrupted, first item is not 1",
            "topic",        "%s", tranger_topic_name(topic),
            "rowid",        "%lu", (unsigned long)md_record->__rowid__,
            NULL
        );
        return -1;
    }

    if(md_record->__system_flag__ & sf_deleted_record) {
        return tranger_next_record(tranger, topic, md_record);
    }

    return 0;
}

/***************************************************************************
 *  Read last md record
 ***************************************************************************/
PUBLIC int tranger_last_record(
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record
)
{
    json_int_t rowid = kw_get_int(topic, "__last_rowid__", 0, KW_REQUIRED);
    if(tranger_get_record(
        tranger,
        topic,
        rowid,
        md_record,
        FALSE
    )<0) {
        return -1;
    }

    if(md_record->__rowid__ != rowid) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "md_records corrupted, last item is not last_rowid",
            "topic",        "%s", tranger_topic_name(topic),
            "rowid",        "%lu", (unsigned long)md_record->__rowid__,
            "last_rowid",   "%lu", (unsigned long)rowid,
            NULL
        );
        return -1;
    }

    if(md_record->__system_flag__ & sf_deleted_record) {
        return tranger_prev_record(tranger, topic, md_record);
    }

    return 0;
}

/***************************************************************************
 *  Read next md record
 ***************************************************************************/
PUBLIC int tranger_next_record(
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record
)
{
    json_int_t rowid = md_record->__rowid__ + 1;

    if(tranger_get_record(
        tranger,
        topic,
        rowid,
        md_record,
        FALSE
    )<0) {
        return -1;
    }

    if(md_record->__rowid__ != rowid) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "md_records corrupted, last item is not last_rowid",
            "topic",        "%s", tranger_topic_name(topic),
            "rowid",        "%lu", (unsigned long)rowid,
            "next_rowid",   "%lu", (unsigned long)md_record->__rowid__,
            NULL
        );
        return -1;
    }

    if(md_record->__system_flag__ & sf_deleted_record) {
        return tranger_next_record(tranger, topic, md_record);
    }

    return 0;
}

/***************************************************************************
 *  Read prev md record
 ***************************************************************************/
PUBLIC int tranger_prev_record(
    json_t *tranger,
    json_t *topic,
    md2_record_t *md_record
)
{
    json_int_t rowid = md_record->__rowid__ - 1;
    if(md_record->__rowid__ < 1) {
        return 0;
    }
    if(tranger_get_record(
        tranger,
        topic,
        rowid,
        md_record,
        FALSE
    )<0) {
        return -1;
    }

    if(md_record->__rowid__ != rowid) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "md_records corrupted, last item is not last_rowid",
            "topic",        "%s", tranger_topic_name(topic),
            "rowid",        "%lu", (unsigned long)rowid,
            "prev_rowid",   "%lu", (unsigned long)md_record->__rowid__,
            NULL
        );
        return -1;
    }

    if(md_record->__system_flag__ & sf_deleted_record) {
        return tranger_prev_record(tranger, topic, md_record);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void print_md0_record(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    char *bf,
    int bfsize
)
{
    char fecha[90];
    char stamp[64];
    if(md_record->__system_flag__ & sf_t_ms) {
        time_t t = md_record->__t__;
        t /= 1000;
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%S%z", localtime(&t));
    } else {
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%S%z",
            localtime((time_t *)&md_record->__t__)
        );
    }

    char fecha_tm[80];
    if(md_record->__system_flag__ & sf_tm_ms) {
        time_t t_m = md_record->__tm__;
        t_m /= 1000;
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S%z", gmtime(&t_m));
    } else {
        strftime(fecha_tm, sizeof(fecha_tm), "%Y-%m-%dT%H:%M:%S%z",
            gmtime((time_t *)&md_record->__tm__)
        );
    }

    system_flag_t key_type = md_record->__system_flag__ & KEY_TYPE_MASK;

    if(!key_type) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s",
            (uint64_t)md_record->__rowid__,
            (uint64_t)md_record->__t__,
            fecha,
            (uint64_t)md_record->__tm__,
            fecha_tm
        );
    } else if(key_type & (sf_int_key|sf_rowid_key)) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key: %"PRIu64,
            (uint64_t)md_record->__rowid__,
            (uint64_t)md_record->__t__,
            fecha,
            (uint64_t)md_record->__tm__,
            fecha_tm,
            md_record->key.i
        );
    } else if(key_type & sf_string_key) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key:'%s'",
            (uint64_t)md_record->__rowid__,
            (uint64_t)md_record->__t__,
            fecha,
            (uint64_t)md_record->__tm__,
            fecha_tm,
            md_record->key.s
        );
    } else {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD metadata, without key type",
            "topic",        "%s", tranger_topic_name(topic),
            NULL
        );
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void print_md1_record(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    char *bf,
    int bfsize
)
{
    struct tm *tm;
    char fecha[90];
    char stamp[64];
    if(md_record->__system_flag__ & sf_t_ms) {
        time_t t = md_record->__t__;
        int ms = t % 1000;
        t /= 1000;
        tm = gmtime(&t);
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", tm);
        snprintf(fecha, sizeof(fecha), "%s.%03uZ", stamp, ms);
    } else {
        strftime(fecha, sizeof(fecha), "%Y-%m-%dT%H:%M:%SZ", gmtime((time_t *)&md_record->__t__));
    }

    char fecha_tm[80];
    if(md_record->__system_flag__ & sf_tm_ms) {
        time_t t_m = md_record->__tm__;
        int ms = t_m % 1000;
        time_t t_m_ = t_m/1000;
        tm = gmtime(&t_m_);
        strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", tm);
        snprintf(fecha_tm, sizeof(fecha_tm), "%s.%03uZ", stamp, ms);
    } else {
        strftime(fecha_tm, sizeof(fecha_tm), "%Y-%m-%dT%H:%M:%SZ", gmtime((time_t *)&md_record->__tm__));
    }

    system_flag_t key_type = md_record->__system_flag__ & KEY_TYPE_MASK;

    if(!key_type) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "uflag:0x%"PRIX32", sflag:0x%"PRIX32", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s",
            (uint64_t)md_record->__rowid__,
            md_record->__user_flag__,
            md_record->__system_flag__,
            (uint64_t)md_record->__t__,
            fecha,
            (uint64_t)md_record->__tm__,
            fecha_tm
        );
    } else if(key_type & (sf_int_key|sf_rowid_key)) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "uflag:0x%"PRIX32", sflag:0x%"PRIX32", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key: %"PRIu64,
            (uint64_t)md_record->__rowid__,
            md_record->__user_flag__,
            md_record->__system_flag__,
            (uint64_t)md_record->__t__,
            fecha,
            (uint64_t)md_record->__tm__,
            fecha_tm,
            md_record->key.i
        );
    } else if(key_type & sf_string_key) {
        snprintf(bf, bfsize,
            "rowid:%"PRIu64", "
            "uflag:0x%"PRIX32", sflag:0x%"PRIX32", "
            "t:%"PRIu64" %s, "
            "tm:%"PRIu64" %s, "
            "key:'%s'",
            (uint64_t)md_record->__rowid__,
            md_record->__user_flag__,
            md_record->__system_flag__,
            (uint64_t)md_record->__t__,
            fecha,
            (uint64_t)md_record->__tm__,
            fecha_tm,
            md_record->key.s
        );
    } else {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD metadata, without key type",
            "topic",        "%s", tranger_topic_name(topic),
            NULL
        );
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void print_md2_record(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    char *bf,
    int bfsize
)
{
    time_t t = md_record->__t__;
    uint64_t offset = md_record->__offset__;
    uint64_t size = md_record->__size__;

    char path[1024];
    get_record_content_fullpath(
        tranger,
        topic,
        path,
        sizeof(path),
        (md_record->__system_flag__ & sf_t_ms)? t/1000:t
    );


    snprintf(bf, bfsize,
        "rowid:%"PRIu64", ofs:%"PRIu64", sz:%-5"PRIu64", "
        "t:%"PRIu64", "
        "f:%s",
        (uint64_t)md_record->__rowid__,
        offset,
        size,
        (uint64_t)t,
        path
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void print_record_filename(
    json_t *tranger,
    json_t *topic,
    const md2_record_t *md_record,
    char *bf,
    int bfsize
)
{
    time_t t = md_record->__t__;
    get_record_content_fullpath(
        tranger,
        topic,
        bf,
        bfsize,
        t
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void tranger_set_trace_level(
    json_t *tranger,
    int trace_level
)
{
    json_object_set_new(tranger, "trace_level", json_integer(trace_level));
}
#endif
