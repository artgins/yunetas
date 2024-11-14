/****************************************************************************
 *              fs_watcher.c
 *
 *              Monitoring of directories and files with io_uring
 *
 *              We only monitor:
 *                  - create/delete of sub-directories (recursive optionally)
 *                  - create/delete of files in these directories,
 *              and optionally modification of files.
 *
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/inotify.h>

#include <helpers.h>
#include <kwid.h>
#include "fs_watcher.h"

/***************************************************************************
 *  Constants
 ***************************************************************************/

#define DEFAULT_MASK (IN_DELETE_SELF|IN_MOVE_SELF|IN_CREATE|IN_DELETE | IN_DONT_FOLLOW|IN_EXCL_UNLINK)

/***************************************************************************
 *  Prototypes
 ***************************************************************************/
PRIVATE void fs_destroy_watcher_event(
    fs_event_t *fs_event
);
PRIVATE int yev_callback(
    yev_event_t *event
);
PRIVATE void handle_inotify_event(fs_event_t *fs_event, struct inotify_event *event);
PRIVATE int add_watch(fs_event_t *fs_event, const char *path);
PRIVATE int remove_watch(fs_event_t *fs_event, const char *path, int wd);
PRIVATE const char *get_path(hgobj gobj, json_t *jn_tracked_paths, int wd, BOOL verbose);
PRIVATE void add_watch_recursive(fs_event_t *fs_event, const char *path);
PRIVATE uint32_t fs_type_2_inotify_mask(fs_event_t *fs_event);

/***************************************************************************
 *  Data
 ***************************************************************************/

typedef struct {
    uint32_t bit;
    char *name;
    char *description;
} bits_table_t;

PRIVATE bits_table_t bits_table[] = {
{IN_ACCESS,         "IN_ACCESS",        "File was accessed"},
{IN_MODIFY,         "IN_MODIFY",        "File was modified"},
{IN_ATTRIB,         "IN_ATTRIB",        "Metadata changed"},
{IN_CLOSE_WRITE,    "IN_CLOSE_WRITE",   "Writable file was closed"},
{IN_CLOSE_NOWRITE,  "IN_CLOSE_NOWRITE", "Unwritable file closed"},
{IN_OPEN,           "IN_OPEN",          "File was opened"},
{IN_MOVED_FROM,     "IN_MOVED_FROM",    "File was moved from X"},
{IN_MOVED_TO,       "IN_MOVED_TO",      "File was moved to Y"},
{IN_CREATE,         "IN_CREATE",        "Subfile was created"},
{IN_DELETE,         "IN_DELETE",        "Subfile was deleted"},
{IN_DELETE_SELF,    "IN_DELETE_SELF",   "Self was deleted"},
{IN_MOVE_SELF,      "IN_MOVE_SELF",     "Self was moved"},

//" Events sent by the kernel"
{IN_UNMOUNT,        "IN_UNMOUNT",       "Backing fs was unmounted"},
{IN_Q_OVERFLOW,     "IN_Q_OVERFLOW",    "Event queued overflowed"},
{IN_IGNORED,        "IN_IGNORED",       "File was ignored"},

// " Special flags"
{IN_ONLYDIR,        "IN_ONLYDIR",       "Only watch the path if it is a directory"},
{IN_DONT_FOLLOW,    "IN_DONT_FOLLOW",   "Do not follow a sym link"},
{IN_EXCL_UNLINK,    "IN_EXCL_UNLINK",   "Exclude events on unlinked objects"},
{IN_MASK_CREATE,    "IN_MASK_CREATE",   "Only create watches"},
{IN_MASK_ADD,       "IN_MASK_ADD",      "Add to the mask of an already existing watch"},
{IN_ISDIR,          "IN_ISDIR",         "Event occurred against dir"},
{IN_ONESHOT,        "IN_ONESHOT",       "Only send event once"},
{0,0,0}
};

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC fs_event_t *fs_create_watcher_event(
    yev_loop_t *yev_loop,
    const char *path,
    fs_flag_t fs_flag,
    fs_callback_t callback,
    hgobj gobj,
    void *user_data
)
{
    if(!is_directory(path)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "No is a directory",
            "path",         "%s", path?path:"",
            NULL
        );
        return NULL;

    }
    int fd = inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
    if(fd < 0) {
        gobj_log_critical(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "inotify_init1() FAILED",
            "serror",       "%s", strerror(errno),
            NULL
        );
        return NULL;
    }

    fs_event_t *fs_event = GBMEM_MALLOC(sizeof(fs_event_t));
    if(!fs_event) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "No memory for yev event",    // use the same string
            NULL
        );
        close(fd);
        return NULL;
    }

    fs_event->yev_loop = yev_loop;
    fs_event->path = GBMEM_STRDUP(path);
    fs_event->fs_flag = fs_flag;
    fs_event->fs_type = 0;
    fs_event->directory = NULL;
    fs_event->filename = NULL;
    fs_event->gobj = gobj;
    fs_event->user_data = user_data;
    fs_event->callback = callback;
    fs_event->fd = fd;
    fs_event->jn_tracked_paths = json_object();

    if(gobj_trace_level(fs_event->gobj) & (TRACE_URING|TRACE_CREATE_DELETE|TRACE_CREATE_DELETE2|TRACE_FS)) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "fs_create_watcher_event",
            "msg2",         "%s", "💾🟦 fs_create_watcher_event",
            "path",         "%s", path,
            "fd",           "%d", fd,
            "p",            "%p", fs_event,
            NULL
        );
    }

    /*
     *  Alloc buffer to read
     */
    size_t len = sizeof(struct inotify_event) + NAME_MAX + 1;
    gbuffer_t *gbuf = gbuffer_create(len, len);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "No memory for fs_event gbuf",
            NULL
        );
        fs_destroy_watcher_event(fs_event);
        return NULL;
    }

    fs_event->yev_event = yev_create_read_event(
        yev_loop,
        yev_callback,
        gobj,
        fd,
        gbuf
    );
    if(!fs_event->yev_event) {
        fs_destroy_watcher_event(fs_event);
        return NULL;
    }
    yev_set_user_data(fs_event->yev_event, fs_event);

    if(fs_flag & FS_FLAG_RECURSIVE_PATHS) {
        add_watch_recursive(fs_event, path);
    } else {
        add_watch(fs_event, path);
    }

    return fs_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int fs_start_watcher_event(
    fs_event_t *fs_event
)
{
    if(!fs_event) {
        return -1;
    }
    return yev_start_event(fs_event->yev_event);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int fs_stop_watcher_event(
    fs_event_t *fs_event
)
{
    if(!fs_event) {
        return -1;
    }
    if(yev_event_is_running(fs_event->yev_event)) {
        return yev_stop_event(fs_event->yev_event);
    } else {
        fs_destroy_watcher_event(fs_event);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void fs_destroy_watcher_event(
    fs_event_t *fs_event
)
{
    if(!fs_event) {
        return;
    }

    if(gobj_trace_level(fs_event->gobj) & (TRACE_URING|TRACE_CREATE_DELETE|TRACE_CREATE_DELETE2|TRACE_FS)) {
        gobj_log_debug(fs_event->gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "fs_destroy_watcher_event",
            "msg2",         "%s", "💾🟥🟥 fs_destroy_watcher_event",
            "path",         "%s", fs_event->path,
            "fd",           "%d", fs_event->fd,
            "p",            "%p", fs_event,
            NULL
        );
    }

    if(fs_event->fd != -1) {
        if(gobj_trace_level(0) & (TRACE_URING|TRACE_CREATE_DELETE|TRACE_CREATE_DELETE2|TRACE_FS)) {
            gobj_log_debug(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "close socket",
                "msg2",         "%s", "💾🟥 close socket",
                "fd",           "%d", fs_event->fd ,
                "p",            "%p", fs_event,
                NULL
            );
        }
        close(fs_event->fd);
        fs_event->fd = -1;
    }
    yev_set_fd(fs_event->yev_event, -1);
    EXEC_AND_RESET(yev_destroy_event, fs_event->yev_event)
    GBMEM_FREE(fs_event->path)
    JSON_DECREF(fs_event->jn_tracked_paths)
    GBMEM_FREE(fs_event)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE uint32_t fs_type_2_inotify_mask(fs_event_t *fs_event)
{
    uint32_t inotify_mask = DEFAULT_MASK;
    if(fs_event->fs_flag & FS_FLAG_MODIFIED_FILES) {
        inotify_mask |= IN_MODIFY;
    }

    return inotify_mask;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_callback(
    yev_event_t *yev_event
)
{
    fs_event_t *fs_event = yev_event->user_data;
    hgobj gobj = fs_event->gobj;

    if(gobj_trace_level(gobj) & (TRACE_URING|TRACE_FS)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_event->flag);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "💾💥 yev callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "state",        "%s", yev_get_state_name(yev_event),
            "result",       "%d", yev_event->result,
            "sres",         "%s", (yev_event->result<0)? strerror(-yev_event->result):"",
            "flag",         "%j", jn_flags,
            "fd",           "%d", yev_event->fd,
            "gbuffer",      "%p", yev_event->gbuf,
            "p",            "%p", yev_event,
            NULL
        );
        json_decref(jn_flags);
    }
    switch(yev_event->type) {
        case YEV_READ_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    if(gobj_trace_level(gobj) & (TRACE_URING|TRACE_FS)) {
                        if(yev_event->result != -ECANCELED) {
                            gobj_log_info(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                                "msg",          "%s", "read FAILED",
                                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                                "errno",        "%d", -yev_event->result,
                                "strerror",     "%s", strerror(-yev_event->result),
                                "p",            "%p", yev_event,
                                NULL
                            );
                        }
                    }
                    fs_destroy_watcher_event(fs_event);

                } else {
                    size_t len = gbuffer_leftbytes(yev_event->gbuf);
                    char *buffer = gbuffer_cur_rd_pointer(yev_event->gbuf);
                    char *ptr = buffer;
                    while (ptr < buffer + len) {
                        struct inotify_event *event = (struct inotify_event *) ptr;

                        // Handle the file modification event
                        handle_inotify_event(fs_event, event);

                        ptr += sizeof(struct inotify_event) + event->len;
                    }

                    /*
                     *  Clear buffer
                     *  Re-arm read
                     */
                    if(yev_event->gbuf) {
                        gbuffer_clear(yev_event->gbuf);
                        yev_start_event(yev_event);
                    }
                }
            }
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "event type NOT IMPLEMENTED",
                NULL
            );
            break;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void handle_inotify_event(fs_event_t *fs_event, struct inotify_event *event)
{
    hgobj gobj = fs_event->gobj;
    const char *path;
    char full_path[PATH_MAX];

    if(gobj_trace_level(gobj) & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "inotify_event",
            "msg2",         "%s", "💾💾💥 inotify_event",
            "event->wd",    "%d", event->wd,
            "event->name",  "%s", event->len? event->name:"",
            "p",            "%p", fs_event,
            NULL
        );

        for(int i=0; i< sizeof(bits_table)/sizeof(bits_table[0]); i++) {
            bits_table_t entry = bits_table[i];
            if(entry.bit & event->mask) {
                gobj_trace_msg(gobj, "  - %s (%s)", entry.name, entry.description);
            }
        }
    }

    /*
     *  being:
     *      "tr_topic_pkey_integer/topic_pkey_integer/keys": 7,
     *      "tr_topic_pkey_integer/topic_pkey_integer/keys/0000000000000000002": 8,
     *
     *  TRICK when a watched directory .../keys/0000000000000000002 is deleted, the events are:
     *
     *      - IN_DELETE_SELF 8  ""  with the wd you can know what directory is. !!!HACK use this!!!
     *      - IN_IGNORED     8  ""  the wd of deleted directory has been removed of watchers
     *
     *      - IN_DELETE      7  "0000000000000000002"
     *                          comes with the wd of the directory parent (keys),
     *                          informing that his child has been deleted (0000000000000000002).
     *                          but in a tree, in the final first subdirectories deleting
     *                          this event is not arriving.
     *
     *      HACK don't use IN_MODIFY in intense writing, cause IN_Q_OVERFLOW and event lost.
     */

    if(event->mask & (IN_DELETE_SELF)) {
        // The directory is removed or moved
        path=get_path(gobj, fs_event->jn_tracked_paths, event->wd, TRUE);
        if(path != NULL) {
            char path_[PATH_MAX];
            snprintf(path_, sizeof(path_), "%s", path);
            char *filename = pop_last_segment(path_);

            fs_event->fs_type = FS_SUBDIR_DELETED_TYPE;
            fs_event->directory = path_;
            fs_event->filename = filename;
            fs_event->callback(fs_event);
            remove_watch(fs_event, path, event->wd);
        }
        return;
    }

    if(event->mask & (IN_IGNORED)) {
        // The Watch was removed
        if((path=get_path(gobj, fs_event->jn_tracked_paths, event->wd, FALSE)) != NULL) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "wd yet found",
                "path" ,        "%s", path,
                "wd",           "%d", event->wd,
                "event",        "%s", event->len? event->name:"",
                "p",            "%p", event,
                NULL
            );
        }
        return;
    }

    path = get_path(gobj, fs_event->jn_tracked_paths, event->wd, TRUE);

    if(event->mask & (IN_ISDIR)) {
        /*
         *  Directory
         */
        if (event->mask & (IN_CREATE)) {
            char *filename = event->len? event->name:"";
            if(fs_event->fs_flag & FS_FLAG_RECURSIVE_PATHS) {
                snprintf(full_path, sizeof(full_path), "%s/%s", path, filename);
                add_watch(fs_event, full_path);
            }
            fs_event->fs_type = FS_SUBDIR_CREATED_TYPE;
            fs_event->directory = (volatile char *)path;
            fs_event->filename = filename;
            fs_event->callback(fs_event);
        }
        if (event->mask & (IN_DELETE)) {
            char *filename = event->len? event->name:"";
            path=get_path(gobj, fs_event->jn_tracked_paths, event->wd, TRUE);
            if(path != NULL) {
                fs_event->fs_type = FS_SUBDIR_DELETED_TYPE;
                fs_event->directory = (volatile char *)path;
                fs_event->filename = filename;
                fs_event->callback(fs_event);
            }
        }
    } else {
        /*
         *  File
         */
        path = get_path(gobj, fs_event->jn_tracked_paths, event->wd, TRUE);
        char *filename = event->len? event->name:"";
        if (event->mask & (IN_CREATE)) {
            fs_event->fs_type = FS_FILE_CREATED_TYPE;
            fs_event->directory = (volatile char *)path;
            fs_event->filename = filename;
            fs_event->callback(fs_event);
        }
        if (event->mask & (IN_DELETE)) {
            fs_event->fs_type = FS_FILE_DELETED_TYPE;
            fs_event->directory = (volatile char *)path;
            fs_event->filename = filename;
            fs_event->callback(fs_event);
        }
        if (event->mask & (IN_MODIFY)) {
            fs_event->fs_type = FS_FILE_MODIFIED_TYPE;
            fs_event->directory = (volatile char *)path;
            fs_event->filename = filename;
            fs_event->callback(fs_event);
        }
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_watch(fs_event_t *fs_event, const char *path)
{
    hgobj gobj = fs_event->gobj;

    json_t *watch = json_object_get(fs_event->jn_tracked_paths, path);
    if(watch) {
        gobj_log_error(fs_event->gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Watch directory EXISTS",
            "path" ,        "%s", path,
            "p",            "%p", fs_event,
            NULL
        );
        return -1;
    }

    int wd = inotify_add_watch(fs_event->fd, path, fs_type_2_inotify_mask(fs_event));
    if (wd == -1) {
        gobj_log_error(fs_event->gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "inotify_add_watch() FAILED",
            "path" ,        "%s", path,
            "serrno" ,      "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    json_object_set_new(fs_event->jn_tracked_paths, path, json_integer(wd));

    if(gobj_trace_level(gobj) & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_YEV_LOOP,
            "msg",              "%s", "add watch",
            "msg2",             "%s", "💾🔷 add watch",
            "path",             "%s", path,
            "wd",               "%d", wd,
            "fs_flag",          "%d", fs_event->fs_flag,
            "recursive",        "%d", fs_event->fs_flag & FS_FLAG_RECURSIVE_PATHS,
            "p",                "%p", fs_event,
            NULL
        );
        gobj_trace_json(gobj, fs_event->jn_tracked_paths, "💾🔷 add watch tracked_paths");
    }

    return wd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int remove_watch(fs_event_t *fs_event, const char *path, int wd)
{
    hgobj gobj = fs_event->gobj;

    json_object_del(fs_event->jn_tracked_paths, path);

    if(gobj_trace_level(gobj) & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_YEV_LOOP,
            "msg",              "%s", "remove watch",
            "msg2",             "%s", "💾🔶 remove watch",
            "path",             "%s", path,
            "wd",               "%d", wd,
            "fs_flag",          "%d", fs_event->fs_flag,
            "recursive",        "%d", fs_event->fs_flag & FS_FLAG_RECURSIVE_PATHS,
            "p",                "%p", fs_event,
            NULL
        );
        gobj_trace_json(gobj, fs_event->jn_tracked_paths, "💾🔶 remove watch tracked_paths");
    }

    if(inotify_rm_watch(fs_event->fd, wd)<0) {
        if(errno != EINVAL) { // Han borrado el directorio
            gobj_log_error(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "inotify_rm_watch() FAILED",
                "path" ,        "%s", path,
                "serrno" ,      "%s", strerror(errno),
                NULL
            );
        }
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *get_path(hgobj gobj, json_t *jn_tracked_paths, int wd, BOOL verbose)
{
    const char *path; json_t *jn_wd;
    json_object_foreach(jn_tracked_paths, path, jn_wd) {
        int wd_ = (int)json_integer_value(jn_wd);
        if(wd_ == wd) {
            return path;
        }

    }
    if(verbose) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "wd not found",
            "wd" ,          "%d", wd,
            NULL
        );
        gobj_trace_json(gobj, jn_tracked_paths, "wd not found");
    }
    return NULL;
}

/***************************************************************************
 *  Recursively add inotify watches to all subdirectories
 ***************************************************************************/
PRIVATE BOOL search_by_paths_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    wd_option opt           // option parameter
)
{
    fs_event_t *fs_event = user_data;
    add_watch(fs_event, fullpath);
    return TRUE; // to continue
}

PRIVATE void add_watch_recursive(fs_event_t *fs_event, const char *path)
{
    add_watch(fs_event, path);
    walk_dir_tree(
        0,
        path,
        0,
        WD_RECURSIVE|WD_MATCH_DIRECTORY,
        search_by_paths_cb,
        fs_event
    );
}
