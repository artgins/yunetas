/****************************************************************************
 *              fs_watcher.c
 *
 *              Monitoring of directories and files with io_uring
 *
 *              Copyright (c) 2024 ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/inotify.h>

#include <helpers.h>
#include "yunetas_ev_loop.h"
#include "fs_watcher.h"

/***************************************************************************
 *  Prototypes
 ***************************************************************************/
PRIVATE int yev_callback(
    yev_event_t *event
);

/***************************************************************************
 *  Data
 ***************************************************************************/

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC fs_event_t *fs_open_watcher(
    yev_loop_t *yev_loop,
    const char *path,
    fs_type_t fs_type,
    fs_flag_t fs_flag,
    fs_callback_t callback,
    hgobj gobj
)
{
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
        gobj_log_critical(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "No memory for yev event",    // use the same string
            NULL
        );
        close(fd);
        return NULL;
    }

    fs_event->yev_loop = yev_loop;
    fs_event->type = fs_type;
    fs_event->flag = fs_flag;
    fs_event->path = GBMEM_STRDUP(path);
    fs_event->directory = NULL;
    fs_event->filename = NULL;
    fs_event->gobj = gobj;
    fs_event->callback = callback;
    fs_event->fd = fd;

    /*
     *  Alloc buffer to read
     */
    size_t len = sizeof(struct inotify_event) + NAME_MAX + 1;
    gbuffer_t *gbuf = gbuffer_create(len, len);
    if(!gbuf) {
        gobj_log_critical(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "No memory for fs_event gbuf",
            NULL
        );
        fs_close_watcher(fs_event);
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
        fs_close_watcher(fs_event);
        return NULL;
    }
    yev_set_user_data(fs_event->yev_event, fs_event);



    // Add watch recursively on the root directory and its subdirectories
    add_watch_recursive(path);


    yev_start_event(fs_event->yev_event);

    return fs_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void fs_close_watcher(
    fs_event_t *fs_event
)
{
    yev_set_fd(fs_event->yev_event, -1);
    yev_stop_event(fs_event->yev_event);
    EXEC_AND_RESET(yev_destroy_event, fs_event->yev_event)
    if(fs_event->fd != -1) {
        close(fs_event->fd);
        fs_event->fd = -1;
    }
    GBMEM_FREE(fs_event->path)
    GBMEM_FREE(fs_event)
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

    if(gobj_trace_level(gobj) & TRACE_UV) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_event->flag);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "💥 yev callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "result",       "%d", yev_event->result,
            "sres",         "%s", (yev_event->result<0)? strerror(-yev_event->result):"",
            "flag",         "%j", jn_flags,
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
                    if(gobj_trace_level(gobj) & TRACE_UV) {
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

                } else {
//                    GBUFFER_INCREF(yev_event->gbuf)
//                    json_t *kw = json_pack("{s:I}",
//                        "gbuffer", (json_int_t)(size_t)yev_event->gbuf
//                    );
//                    if(gobj_is_pure_child(gobj)) {
//                        gobj_send_event(gobj_parent(gobj), EV_RX_DATA, kw, gobj);
//                    } else {
//                        gobj_publish_event(gobj, EV_RX_DATA, kw);
//                    }

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
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "event type NOT IMPLEMENTED",
                NULL
            );
            break;
    }

    return 0;
}

// Function to handle inotify events
void handle_inotify_event(struct inotify_event *event)
{
    const char *path;
    char full_path[PATH_MAX];

    printf("  ev: %d '%s',", event->wd, event->len? event->name:"");
    for(int i=0; i< sizeof(bits_table)/sizeof(bits_table[0]); i++) {
        bits_table_t entry = bits_table[i];
        if(entry.bit & event->mask) {
            printf(" - %s (%s)", entry.name, entry.description);
        }
    }
    printf("\n");

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
        if((path=get_path(event->wd, TRUE)) != NULL) {
            printf("  %s-> Directory deleted:%s %d %s\n", On_Green BWhite, Color_Off, event->wd, path);

            remove_watch(event->wd);
            print_json2("PATHS", jn_tracked_paths);
        }
        return;
    }

    if(event->mask & (IN_IGNORED)) {
        // The Watch was removed
        if((path=get_path(event->wd, FALSE)) != NULL) {
            printf("%sERROR%s wd yet found %d %s '%s'\n", On_Red BWhite, Color_Off,
                event->wd,
                event->len? event->name:"",
                path
            );
        }
        return;
    }

    path = get_path(event->wd, TRUE);

    if(event->mask & (IN_ISDIR)) {
        /*
         *  Directory
         */
        if (event->mask & (IN_CREATE)) {
            snprintf(full_path, PATH_MAX, "%s/%s", path, event->len? event->name:"");
            printf("  %s-> Directory created:%s %s\n", On_Green BWhite, Color_Off, full_path);
            add_watch(full_path);
            print_json2("PATHS", jn_tracked_paths);
        }
        if (event->mask & (IN_DELETE)) {
            /*
             *  Is deleted in IN_DELETE_SELF event
             */
//            path = get_path(event->wd, TRUE);
//            snprintf(full_path, PATH_MAX, "%s/%s", path, event->len? event->name:"");
//
//            printf("  %s-> Directory deleted:%s %s\n", On_Green BWhite, Color_Off, full_path);
//
//            int wd = get_wd(full_path, TRUE);
//            remove_watch(wd);
//            print_json2("PATHS", jn_tracked_paths);
        }
    } else {
        /*
         *  File
         */
        path = get_path(event->wd, TRUE);
        snprintf(full_path, PATH_MAX, "%s/%s", path, event->len? event->name:"");
        if (event->mask & (IN_CREATE)) {
            printf("  %s-> File created:%s %s\n", On_Green BWhite, Color_Off, full_path);
        }
        if (event->mask & (IN_DELETE)) {
            printf("  %s-> File deleted:%s %s\n", On_Green BWhite, Color_Off, full_path);
        }
        if (event->mask & (IN_MODIFY)) {
            printf("  %s-> File modified:%s %s\n", On_Green BWhite, Color_Off, full_path);
        }
    }

}

// Recursively add inotify watches to all subdirectories
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
    add_watch(fullpath);
    return TRUE; // to continue
}

void add_watch_recursive(const char *path)
{
    add_watch(path);
    walk_dir_tree(
        0,
        path,
        0,
        WD_RECURSIVE|WD_MATCH_DIRECTORY,
        search_by_paths_cb,
        0
    );
}
