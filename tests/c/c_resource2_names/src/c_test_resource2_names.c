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
 *  The names of the files of the store, sorted, joined by a space
 ***************************************************************************/
PRIVATE int files_of_store(hgobj gobj, char *bf, size_t bfsize)
{
    char path_service[PATH_MAX];
    char dir[PATH_MAX];
    bf[0] = 0;
    if(!build_path(path_service, sizeof(path_service),
            STORE_SERVICE, gobj_yuno_role_plus_name(), STORE_DATABASE, NULL) ||
            !yuneta_store_dir(dir, sizeof(dir), "resources", path_service, FALSE)) {
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
