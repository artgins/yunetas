/***********************************************************************
 *          C_FS.C
 *          Watch file system events yev-mixin.
 *
 *          Copyright (c) 2014 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <unistd.h>
#include <limits.h>

#include <gobj.h>
#include <g_events.h>
#include <g_states.h>
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
typedef struct {
    DL_ITEM_FIELDS
    fs_event_t *uv_fs;
    hgobj gobj;
} SUBDIR_WATCH;


/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE SUBDIR_WATCH * create_subdir_watch(hgobj gobj, const char *path);
PRIVATE void destroy_subdir_watch(SUBDIR_WATCH * sw);
PRIVATE BOOL locate_subdirs_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *name,             // dname[255]
    int level,
    wd_option opt
);
PRIVATE int fs_event_callback(fs_event_t *fs_event);



/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name------------flag--------default-----description---------- */
SDATA (DTP_STRING,      "path",         SDF_RD,     0,          "Path to watch"),
SDATA (DTP_BOOLEAN,     "recursive",    SDF_RD,     0,          "Watch on all sub-directory tree"),
SDATA (DTP_BOOLEAN,     "info",         SDF_RD,     0,          "Inform of found subdirectories"),
SDATA (DTP_INTEGER,     "size_dl_watch",SDF_RD|SDF_STATS, 0,    "Current subdirs in dl watch"),
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
    dl_list_t dl_watches;
    BOOL info;
} PRIVATE_DATA;




            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->info = gobj_read_bool_attr(gobj, "info");

    dl_init(&priv->dl_watches, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    const char *path = gobj_read_str_attr(gobj, "path");
    if(!path || access(path, 0)!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path NOT EXIST",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }

    if(gobj_read_bool_attr(gobj, "recursive")) {
        create_subdir_watch(gobj, path); // walk_dir_tree() not return "."
        walk_dir_tree(
            gobj,
            path,
            ".*",
            WD_RECURSIVE|WD_MATCH_DIRECTORY,
            locate_subdirs_cb,
            NULL
        );
    } else {
        create_subdir_watch(gobj, path);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    dl_flush(&priv->dl_watches, (fnfree)destroy_subdir_watch);

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
        v.v.i = (json_int_t)dl_size(&priv->dl_watches);
    }

    return v;
}




            /***************************
             *      Local Methods
             ***************************/



/***************************************************************************
 *      Create a watch
 ***************************************************************************/
PRIVATE SUBDIR_WATCH * create_subdir_watch(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    SUBDIR_WATCH *sw;

    if(priv->info) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_MONITORING,
            "msg",          "%s", "watching directory",
            "path",         "%s", path,
            NULL
        );
    }

    sw = gbmem_malloc(sizeof(SUBDIR_WATCH));
    if(!sw) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "gbmem_malloc() FAILED",
            NULL
        );
        return 0;
    }

    sw->uv_fs = fs_create_watcher_event(
        yuno_event_loop(),
        path,
        FS_FLAG_RECURSIVE_PATHS|FS_FLAG_MODIFIED_FILES,
        fs_event_callback,
        gobj,
        NULL,
        NULL
    );
    fs_start_watcher_event(sw->uv_fs);
    dl_add(&priv->dl_watches, sw);

    return sw;
}

/***************************************************************************
 *      Destroy a watch
 ***************************************************************************/
PRIVATE void destroy_subdir_watch(SUBDIR_WATCH * sw)
{
    // TODO
    fs_stop_watcher_event(sw->uv_fs);

    // uv_fs_event_stop(&sw->uv_fs);
    // uv_close((uv_handle_t *)&sw->uv_fs, on_close_cb);
}

/***************************************************************************
 *  Located directories
 ***************************************************************************/
PRIVATE BOOL locate_subdirs_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *name,             // dname[255]
    int level,
    wd_option opt)
{
    create_subdir_watch(gobj, fullpath);

    return TRUE; // continue traverse tree
}

/***************************************************************************
 *      fs events callback
 ***************************************************************************/
PRIVATE int fs_event_callback(fs_event_t *fs_event)
{
    json_t *kw = json_pack("{s:s, s:s}",
        "path", fs_event->directory,
        "filename", fs_event->filename
    );

    if (fs_event->fs_type & (FS_SUBDIR_CREATED_TYPE)) {
    }
    if (fs_event->fs_type & (FS_SUBDIR_DELETED_TYPE)) {
    }
    if (fs_event->fs_type & (FS_FILE_CREATED_TYPE)) {
    }
    if (fs_event->fs_type & (FS_FILE_DELETED_TYPE)) {
    }
    if (fs_event->fs_type & (FS_FILE_MODIFIED_TYPE)) {
        gobj_publish_event(fs_event->gobj, EV_FS_CHANGED, kw);
    }

    // TODO see how implement UV_CHANGE or if it's useful
    // Original
    // if (events == UV_RENAME) {
    //     gobj_publish_event(fs_event->gobj, EV_FS_RENAMED, kw);
    // }
    // if (events == UV_CHANGE) {
    //     gobj_publish_event(fs_event->gobj, EV_FS_CHANGED, kw);
    // }

    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***********************************************************************
 *          FSM
 ***********************************************************************/
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        tattr_desc,
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
