/***********************************************************************
 *          C_TEST_RESOURCE2_NAMES.C
 *
 *          Driver of the test of the names a persistent C_RESOURCE2
 *          takes.
 *
 *          The name of a resource of a persistent store is a file name,
 *          `<name>.json` in the directory of the store, and it is usually
 *          a peer's: an MQTT gate keeps its clients by client id. A name
 *          is taken when it is one file name: no '/', no leading '.', no
 *          "..", the file name within NAME_MAX. Each other name is refused
 *          with ONE warning (*"Resource name refused: not one file
 *          name ..."*) and gobj_create_resource() answers NULL.
 *
 *          The driver creates "a/b", "/", "..", ".hidden" and a name of
 *          NAME_MAX bytes: each is refused, none is kept in memory, and no
 *          file is written for any of them. Then "client-1" is created and
 *          saved: its file is the only one in the directory.
 *
 *          A reload (a stop and a start of the store) takes each file
 *          back under the name it was saved with: "x", "x.jsonz",
 *          "victim" and "victim.json" are each their own record, and a
 *          file not ending in ".json" ("other.json.bak") is no resource,
 *          nor is a file of a subdirectory ("sub/only.json": a name has no
 *          '/', so no resource is saved there; up to 7.30.17 it was loaded
 *          as "only").
 *          Up to 7.30.17 the name was cut at its FIRST ".json": "x.jsonz"
 *          came back as "x" with the record of one of the two, and
 *          "victim.json" competed with "victim" by the order of readdir().
 *
 *          A record with a top-level "gbuffer" key is a json: it is saved,
 *          less its private keys, and comes back from a reload as it was.
 *          Up to 7.30.17 the save took the integer for a gbuffer: SIGSEGV.
 *
 *          Up to 7.30.16 every one of them was made in memory, and every
 *          save of it logged an ERROR (three per connect of an MQTT client
 *          with such an id); ".hidden" was accepted and wrote
 *          `.hidden.json`.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "c_test_resource2_names.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define STORE_SERVICE   "test_names"
#define STORE_DATABASE  "clients"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int dir_of_store(hgobj gobj, char *dir, size_t dirsize);
PRIVATE int files_of_store(hgobj gobj, char *bf, size_t bfsize);

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
    hgobj gobj_store;
    char root[PATH_MAX];
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

    /*  The store of the resources, as yuneta_store_dir() makes it: wiped  */
    build_path(priv->root, sizeof(priv->root),
        getenv("HOME"), "tests_yuneta", "store", "resources", STORE_SERVICE, NULL);
    rmrdir(priv->root);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->gobj_store = gobj_create_service(
        "test_names_store",
        C_RESOURCE2,
        json_pack("{s:s, s:s, s:b}",
            "service", STORE_SERVICE,
            "database", STORE_DATABASE,
            "persistent", 1
        ),
        gobj
    );
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    rmrdir(priv->root);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    gobj_start(priv->gobj_store);

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
    if(gobj_is_running(priv->gobj_store)) {
        gobj_stop(priv->gobj_store);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 100);
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
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The directory of the store
 ***************************************************************************/
PRIVATE int dir_of_store(hgobj gobj, char *dir, size_t dirsize)
{
    char path_service[PATH_MAX];
    if(!build_path(path_service, sizeof(path_service),
            STORE_SERVICE, gobj_yuno_role_plus_name(), STORE_DATABASE, NULL) ||
            !yuneta_store_dir(dir, dirsize, "resources", path_service, FALSE)) {
        return -1;  // Error already logged
    }
    return 0;
}

/***************************************************************************
 *  The names of the files of the store, sorted, joined by a space
 ***************************************************************************/
PRIVATE int files_of_store(hgobj gobj, char *bf, size_t bfsize)
{
    char dir[PATH_MAX];
    bf[0] = 0;
    if(dir_of_store(gobj, dir, sizeof(dir)) < 0) {
        return -1;  // Error already logged
    }
    json_t *names = json_array();
    DIR *d = opendir(dir);
    if(!d) {
        JSON_DECREF(names)
        return -1;
    }
    struct dirent *de;
    while((de = readdir(d))) {
        if(strcmp(de->d_name, ".")==0 || strcmp(de->d_name, "..")==0) {
            continue;
        }
        json_array_append_new(names, json_string(de->d_name));
    }
    closedir(d);
    size_t idx; json_t *jn_name;
    json_array_foreach(names, idx, jn_name) {
        size_t l = strlen(bf);
        snprintf(bf + l, bfsize - l, "%s%s", l? " " : "", json_string_value(jn_name));
    }
    JSON_DECREF(names)
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The names
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int bad = 0;

    char long_name[NAME_MAX + 1];
    memset(long_name, 'n', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = 0;  // NAME_MAX bytes: with ".json", too long

    const char *refused[] = {"a/b", "/", "..", ".hidden", long_name, NULL};
    for(int i = 0; refused[i]; i++) {
        json_t *record = gobj_create_resource(
            priv->gobj_store,
            refused[i],
            json_pack("{s:s}", "id", refused[i]),
            0
        );
        json_t *kept = gobj_get_resource(priv->gobj_store, refused[i], 0, 0);
        if(record || kept) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST: a name that is not one file name was taken",
                "resource",     "%.64s", refused[i],
                NULL
            );
            bad++;
        }
    }

    json_t *record = gobj_create_resource(
        priv->gobj_store,
        "client-1",
        json_pack("{s:s}", "id", "client-1"),
        0
    );
    if(!record) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: a good name was refused",
            NULL
        );
        bad++;
    }

    char files[PATH_MAX];
    if(files_of_store(gobj, files, sizeof(files)) < 0 || strcmp(files, "client-1.json") != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: the store holds other files than the good name's",
            "files",        "%s", files,
            NULL
        );
        bad++;
    }

    /*
     *  A reload takes each file back under the name it was saved with:
     *  `<name>.json` less its LAST ".json". A file that does not end in
     *  ".json" is not a resource.
     */
    const char *dotted[] = {"x", "x.jsonz", "victim", "victim.json", NULL};
    for(int i = 0; dotted[i]; i++) {
        gobj_create_resource(
            priv->gobj_store,
            dotted[i],
            json_pack("{s:s}", "id", dotted[i]),
            0
        );
    }
    char dir[PATH_MAX];
    char stray[PATH_MAX];
    if(dir_of_store(gobj, dir, sizeof(dir)) < 0 ||
            !build_path(stray, sizeof(stray), dir, "other.json.bak", NULL)) {
        bad++;  // Error already logged
    } else {
        json_t *jn_stray = json_pack("{s:s}", "id", "other.json.bak");
        json_dump_file(jn_stray, stray, 0);
        JSON_DECREF(jn_stray)

        char subdir[PATH_MAX];
        char below[PATH_MAX];
        if(!build_path(subdir, sizeof(subdir), dir, "sub", NULL) ||
                !build_path(below, sizeof(below), subdir, "only.json", NULL)) {
            bad++;  // Error already logged
        } else {
            mkrdir(subdir, yuneta_xpermission());
            json_t *jn_below = json_pack("{s:s}", "id", "sub/only");
            if(json_dump_file(jn_below, below, 0) < 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST: cannot write a file in a subdirectory of the store",
                    "path",         "%s", below,
                    NULL
                );
                bad++;
            }
            JSON_DECREF(jn_below)
        }
    }

    gobj_stop(priv->gobj_store);
    gobj_start(priv->gobj_store);

    for(int i = 0; dotted[i]; i++) {
        json_t *kept = gobj_get_resource(priv->gobj_store, dotted[i], 0, 0);
        const char *id = kept? kw_get_str(gobj, kept, "id", "", 0) : "";
        if(strcmp(id, dotted[i]) != 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST: a reload took a resource under another name",
                "resource",     "%s", dotted[i],
                "id",           "%s", id,
                NULL
            );
            bad++;
        }
    }
    const char *not_resources[] = {"other", "other.json.bak", "other.json", "only", "sub/only", NULL};
    for(int i = 0; not_resources[i]; i++) {
        if(gobj_get_resource(priv->gobj_store, not_resources[i], 0, 0)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST: a reload took a file that is no resource",
                "resource",     "%s", not_resources[i],
                NULL
            );
            bad++;
        }
    }

    /*
     *  A record is a json: one with a top-level "gbuffer" key (a file of
     *  another version, a record a caller built) is saved and kept as it
     *  is. Up to 7.30.17 the save increfed it as a kw, gbuffer_incref() of
     *  the integer: SIGSEGV.
     */
    json_t *gb = gobj_create_resource(
        priv->gobj_store,
        "gb",
        json_pack("{s:s, s:I, s:s}", "id", "gb", "gbuffer", (json_int_t)4096, "_private", "not saved"),
        0
    );
    gobj_stop(priv->gobj_store);
    gobj_start(priv->gobj_store);
    json_t *gb_kept = gobj_get_resource(priv->gobj_store, "gb", 0, 0);
    if(!gb || !gb_kept ||
            json_integer_value(json_object_get(gb_kept, "gbuffer")) != 4096 ||
            json_object_get(gb_kept, "_private")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST: a record with a gbuffer key was not saved as it is, less its private keys",
            NULL
        );
        bad++;
    }

    if(!bad) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "TEST: a persistent C_RESOURCE2 takes only names that are one file name",
            NULL
        );
    }
    set_yuno_must_die();
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
GOBJ_DEFINE_GCLASS(C_TEST_RESOURCE2_NAMES);

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
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,           st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,        0},
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
PUBLIC int register_c_test_resource2_names(void)
{
    return create_gclass(C_TEST_RESOURCE2_NAMES);
}
