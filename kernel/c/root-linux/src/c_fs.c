/***********************************************************************
 *          C_FS.C
 *          Watch file system events yev-mixin.
 *
 *          A child gclass: it publishes EV_FS_CHANGED / EV_FS_RENAMED to
 *          its parent, or to the gobj in `subscriber` (the CHILD
 *          subscription model). Up to 7.25.20 it subscribed nobody and
 *          every host subscribed by hand.
 *
 *          Copyright (c) 2014 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <unistd.h>
#include <limits.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <fs_watcher.h>
#include "c_yuno.h"
#include "c_fs.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/


/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int fs_event_callback(fs_event_t *fs_event);



/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name------------flag--------default-----description---------- */
SDATA (DTP_STRING,      "path",         SDF_RD,     0,          "Path to watch"),
SDATA (DTP_BOOLEAN,     "recursive",    SDF_RD,     0,          "Watch on all sub-directory tree"),
SDATA (DTP_BOOLEAN,     "info",         SDF_RD,     0,          "Log the watched directory at start"),
SDATA (DTP_INTEGER,     "size_dl_watch",SDF_RD|SDF_STATS, 0,    "Watchers running: 1 while the path is watched, 0 if not (one watcher, whether recursive or not)"),
SDATA (DTP_POINTER,     "subscriber",   0,          0,          "Subscriber of output-events, default the parent"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_XXX         = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
//{"traffic",             "Trace dump traffic"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    fs_event_t *fs_watcher;
} PRIVATE_DATA;




            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *path = gobj_read_str_attr(gobj, "path");
    if(!path || access(path, 0)!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "path NOT EXIST",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }

    /*
     *  One watcher: a recursive one watches the whole tree itself, the
     *  subdirectories created later included. Up to 7.25.20 a recursive
     *  C_FS added a recursive watcher per subdirectory too (the walk of a
     *  watch that did not recurse), each with its own inotify fd, and a
     *  change N levels down was published N+1 times; and every watch
     *  recursed, so a C_FS without `recursive` reported the subdirectories.
     */
    if(gobj_read_bool_attr(gobj, "info")) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_MONITORING,
            "msg",          "%s", "watching directory",
            "path",         "%s", path,
            NULL
        );
    }
    priv->fs_watcher = fs_create_watcher_event(
        yuno_event_loop(),
        path,
        (gobj_read_bool_attr(gobj, "recursive")? FS_FLAG_RECURSIVE_PATHS : 0)|FS_FLAG_MODIFIED_FILES,
        fs_event_callback,
        gobj,
        NULL,
        NULL
    );
    if(!priv->fs_watcher) {
        return -1;  // Error already logged
    }
    fs_start_watcher_event(priv->fs_watcher);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->fs_watcher) {
        fs_stop_watcher_event(priv->fs_watcher);    // it frees itself
        priv->fs_watcher = NULL;
    }

    return 0;
}

/***************************************************************************
 *      Framework Method reading
 ***************************************************************************/
PRIVATE SData_Value_t mt_reading(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    SData_Value_t v = {0,{0}};
    if(strcmp(name, "size_dl_watch")==0) {
        v.found = 1;
        v.v.i = priv->fs_watcher? 1 : 0;
    }

    return v;
}




            /***************************
             *      Local Methods
             ***************************/



/***************************************************************************
 *      fs events callback
 *
 *  The types of fs_watcher are values, not bits: up to 7.25.20 they were
 *  tested with `&`, FS_FILE_MODIFIED_TYPE (5) matched a created directory,
 *  a created file and a deleted file by accident, and a deleted directory
 *  (2) matched nothing -- no event, and its kw leaked. Every change is an
 *  EV_FS_CHANGED; a rename (if the watcher ever reports one) EV_FS_RENAMED.
 ***************************************************************************/
PRIVATE int fs_event_callback(fs_event_t *fs_event)
{
    gobj_event_t event = NULL;

    switch(fs_event->fs_type) {
        case FS_SUBDIR_CREATED_TYPE:
        case FS_SUBDIR_DELETED_TYPE:
        case FS_FILE_CREATED_TYPE:
        case FS_FILE_DELETED_TYPE:
        case FS_FILE_MODIFIED_TYPE:
            event = EV_FS_CHANGED;
            break;

        case FS_FILE_RENAME_TYPE:
            event = EV_FS_RENAMED;
            break;

        case FS_OVERFLOW_TYPE:
            /*
             *  Events were lost: something changed under the watched root,
             *  and nobody knows what. Once: the pass that follows is not
             *  published.
             */
            event = EV_FS_CHANGED;
            break;

        case FS_RESCAN_DIR_TYPE:
            return 0;   // the pass after an overflow: published once, at FS_OVERFLOW_TYPE

        default:
            gobj_log_error(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "fs_type unknown",
                "fs_type",      "%d", (int)fs_event->fs_type,
                "path",         "%s", (const char *)fs_event->directory,
                NULL
            );
            return -1;
    }

    json_t *kw = json_pack("{s:s, s:s}",
        "path", fs_event->directory,
        "filename", fs_event->filename
    );
    gobj_publish_event(fs_event->gobj, event, kw);

    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_reading = mt_reading,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_FS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_FS_RENAMED);
GOBJ_DEFINE_EVENT(EV_FS_CHANGED);

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,         st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_FS_RENAMED,     EVF_OUTPUT_EVENT},
        {EV_FS_CHANGED,     EVF_OUTPUT_EVENT},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table
        0,  // command_table
        s_user_trace_level,
        0   // gcflag
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_fs(void)
{
    return create_gclass(C_FS);
}
