/***********************************************************************
 *          C_TEST_FS.C
 *
 *          Driver of the C_FS test: what C_FS publishes for each change
 *          of the watched tree.
 *
 *          A root with a subdirectory `sub` is watched. With C_FS running,
 *          the driver creates a file `f`, writes it, removes it, creates
 *          a directory `d2` and removes `sub`. Every one of those is a
 *          change, and C_FS publishes EV_FS_CHANGED for each, with the
 *          name in `filename`:
 *
 *              f   3 (created, modified, deleted)
 *              d2  1 (directory created)
 *              sub 1 (directory deleted)
 *
 *          Up to 7.25.20 C_FS read the types of fs_watcher as bits: the
 *          types are values (1..8), and FS_FILE_MODIFIED_TYPE (5) matched
 *          a created directory, a created file and a deleted file by
 *          accident -- and a deleted directory (2) matched nothing: no
 *          event, and the kw built for it was leaked.
 *
 *          A file `keep/n`, one level down, is not published: that C_FS
 *          is not recursive. A second C_FS, recursive, watches `a/b`, and
 *          a file `a/b/g` there is published 3 times, once per change. Up
 *          to 7.25.20 every C_FS watch recursed, and a recursive C_FS
 *          watched each subdirectory again with its own recursive watcher:
 *          `n` was published, and `g` 9 times (root, a and b).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "c_test_fs.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void touch_and_remove(hgobj gobj, const char *path);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj gobj_fs;
    hgobj gobj_fs_rec;
    char root[PATH_MAX];
    char root_rec[PATH_MAX];
    json_t *changed;    // filename -> times EV_FS_CHANGED, of the non-recursive C_FS
    json_t *changed_rec;// filename -> times EV_FS_CHANGED, of the recursive C_FS
    int renamed;
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

    build_path(priv->root, sizeof(priv->root), getenv("HOME"), "tests_yuneta", "c_fs", NULL);
    rmrdir(priv->root);
    mkrdir(priv->root, 02770);
    char sub[PATH_MAX];
    build_path(sub, sizeof(sub), priv->root, "sub", NULL);
    mkrdir(sub, 02770);
    build_path(sub, sizeof(sub), priv->root, "keep", NULL);
    mkrdir(sub, 02770);

    build_path(priv->root_rec, sizeof(priv->root_rec), getenv("HOME"), "tests_yuneta", "c_fs_rec", NULL);
    rmrdir(priv->root_rec);
    build_path(sub, sizeof(sub), priv->root_rec, "a", "b", NULL);
    mkrdir(sub, 02770);

    priv->changed = json_object();
    priv->changed_rec = json_object();
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->gobj_fs = gobj_create(
        "fs",
        C_FS,
        json_pack("{s:s, s:b}",
            "path", priv->root,
            "recursive", 0
        ),
        gobj
    );
    gobj_subscribe_event(priv->gobj_fs, NULL, 0, gobj);
    priv->gobj_fs_rec = gobj_create(
        "fs_rec",
        C_FS,
        json_pack("{s:s, s:b}",
            "path", priv->root_rec,
            "recursive", 1
        ),
        gobj
    );
    gobj_subscribe_event(priv->gobj_fs_rec, NULL, 0, gobj);
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->changed)
    JSON_DECREF(priv->changed_rec)
    rmrdir(priv->root);
    rmrdir(priv->root_rec);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->gobj_fs);
    gobj_start(priv->gobj_fs_rec);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    if(gobj_is_running(priv->gobj_fs)) {
        gobj_stop(priv->gobj_fs);
    }
    if(gobj_is_running(priv->gobj_fs_rec)) {
        gobj_stop(priv->gobj_fs_rec);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char path[PATH_MAX];
    build_path(path, sizeof(path), priv->root, "f", NULL);
    touch_and_remove(gobj, path);

    /*
     *  One level down: not in the tree of a non-recursive C_FS
     */
    build_path(path, sizeof(path), priv->root, "keep", "n", NULL);
    touch_and_remove(gobj, path);

    /*
     *  Two levels down: in the tree of a recursive C_FS, once
     */
    build_path(path, sizeof(path), priv->root_rec, "a", "b", "g", NULL);
    touch_and_remove(gobj, path);

    build_path(path, sizeof(path), priv->root, "d2", NULL);
    mkdir(path, 02770);

    build_path(path, sizeof(path), priv->root, "sub", NULL);
    rmdir(path);

    set_timeout(priv->timer, 300);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Create a file, write one byte, remove it: three changes
 ***************************************************************************/
PRIVATE void touch_and_remove(hgobj gobj, const char *path)
{
    int fd = open(path, O_CREAT|O_WRONLY, 0600);
    if(fd < 0 || write(fd, "x", 1) != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot create and write the file",
            "path",         "%s", path,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }
    if(fd >= 0) {
        close(fd);
    }
    unlink(path);
}

                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  What C_FS published
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *expected = json_pack("{s:i, s:i, s:i}",
        "f", 3,
        "d2", 1,
        "sub", 1
    );
    json_t *expected_rec = json_pack("{s:i}",
        "g", 3
    );
    if(!json_equal(priv->changed, expected) ||
            !json_equal(priv->changed_rec, expected_rec) ||
            priv->renamed != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: C_FS did not publish one EV_FS_CHANGED per change",
            "changed",      "%j", priv->changed,
            "expected",     "%j", expected,
            "changed_recursive",  "%j", priv->changed_rec,
            "expected_recursive", "%j", expected_rec,
            "renamed",      "%d", priv->renamed,
            NULL
        );
    } else {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "TEST: C_FS published one EV_FS_CHANGED per change",
            NULL
        );
    }
    JSON_DECREF(expected)
    JSON_DECREF(expected_rec)

    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_fs_changed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *changed = (src == priv->gobj_fs_rec)? priv->changed_rec : priv->changed;
    const char *filename = kw_get_str(gobj, kw, "filename", "", 0);
    json_object_set_new(changed, filename,
        json_integer(json_integer_value(json_object_get(changed, filename)) + 1)
    );

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_fs_renamed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->renamed++;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_FS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

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
        {EV_TIMEOUT,        ac_timeout,     0},
        {EV_FS_CHANGED,     ac_fs_changed,  0},
        {EV_FS_RENAMED,     ac_fs_renamed,  0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,           st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,        0},
        {EV_FS_CHANGED,     0},
        {EV_FS_RENAMED,     0},
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
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
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
PUBLIC int register_c_test_fs(void)
{
    return create_gclass(C_TEST_FS);
}
