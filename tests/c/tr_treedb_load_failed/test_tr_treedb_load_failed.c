/****************************************************************************
 *          test_tr_treedb_load_failed.c
 *
 *  A treedb topic whose keys cannot all be read.
 *
 *  treedb loads every topic with keyless tranger2_open_list()s. A list is
 *  not refused at the first key it cannot load (the topic would come up
 *  with the keys before it only, with no realtime feed, a master would
 *  accept a create that shadows a stored record, and an active snap would
 *  be ignored): it loads every readable key and names the others; treedb
 *  remembers them:
 *
 *      1. the topic loads every key it can read, and its feed is open.
 *      2. a create of an id that did not load is refused; others are not.
 *      3. a __snaps__ that did not load whole: shoot-snap, activate-snap and
 *         the delete of a node refuse (which snap is active, or holds the
 *         node, is unknown).
 *
 *  The same after a RESTART: the damage is done with the tranger shut down,
 *  and the topic's cache is built from the damaged store. The cache build
 *  of 7.25.4 dropped an unreadable md2, so nothing failed, the registry
 *  stayed empty and every guard was open:
 *      5. the md2 of k2 cannot be read (mode 000),
 *      6. the CONTENT of k2 cut to 0 bytes (it made a node with id ""):
 *         k2 is not in memory, and a create of it is refused.
 *      7. a __snaps__ md2 that cannot be read: shoot, activate and delete
 *         refuse.
 *      8. the recovery: the key deleted, a create of its id is accepted
 *         without reopening the treedb.
 *
 *  But a md2 of 0 rows whose content file is not empty is what an append
 *  that was never acknowledged leaves (the content is written first, the
 *  md2 row after), not damage. Flagged, it would make a node with a good
 *  older version DISAPPEAR after a restart and refuse its create, or load
 *  an old version of it. The file is ignored with a warning:
 *      4a. k2's newest file is such a file: k2 is in memory with its
 *          previous version, nothing is flagged.
 *      4b. such a file between two good versions of k2: k2 is in memory
 *          with the newest one.
 *  A md2 whose size is not a whole number of rows is not damage either:
 *  its last row is torn, and a master cuts it back (timeranger2's
 *  test_torn_md2_tail.c). Cases 5 and 7 use a md2 of mode 000, and are
 *  skipped as root, who reads it anyway.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <tr_treedb.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#include "schema_sample.c"

#define APP         "test_tr_treedb_load_failed"
#define DATABASE    "tr_treedb_load_failed"
#define DATABASE2   "tr_treedb_load_failed_snaps"
#define TREEDB_NAME "treedb_load_failed"
#define TOPIC_NAME  "items"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *open_tranger(const char *path_root, const char *database)
{
    return tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", database,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    ), 0);
}

PRIVATE json_t *open_treedb(json_t *tranger)
{
    return treedb_open_db(tranger, TREEDB_NAME, legalstring2json(schema_sample, TRUE), 0);
}

PRIVATE json_t *create_item(json_t *tranger, const char *id, const char *payload)
{
    return treedb_create_node(tranger, TREEDB_NAME, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", id, "version", "v1", "payload", payload)
    );
}

/*
 *  Cut every md2 of a key to 0 rows, behind the tranger's back: its cache
 *  still counts the rows, and a load of the key fails at the first.
 */
PRIVATE int cut_key(const char *path_database, const char *topic_name, const char *key)
{
    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), path_database, topic_name, "keys", key, NULL);
    dir_array_t da;
    get_ordered_filename_array(0, key_dir, ".*\\.md2", WD_MATCH_REGULAR_FILE, &da);
    int cut = 0;
    for(int i = 0; i < da.count; i++) {
        if(truncate(da.items[i], 0) == 0) {
            cut++;
        }
    }
    dir_array_free(&da);
    if(cut == 0) {
        printf("%sERROR%s --> cannot cut the md2 of %s/%s\n", On_Red BWhite, Color_Off, topic_name, key);
        return -1;
    }
    return 0;
}

PRIVATE char *ids_in_memory(json_t *tranger, char *bf, size_t bfsize)
{
    bf[0] = 0;
    json_t *nodes = treedb_list_nodes(tranger, TREEDB_NAME, TOPIC_NAME, 0, 0);
    size_t idx; json_t *node;
    json_array_foreach(nodes, idx, node) {
        size_t ln = strlen(bf);
        snprintf(bf + ln, bfsize - ln, "%s%s", ln? " ": "", kw_get_str(0, node, "id", "", 0));
    }
    JSON_DECREF(nodes)
    return bf;
}

/***************************************************************************
 *  1 and 2: a topic with a key that cannot be read
 ***************************************************************************/
PRIVATE int test_topic_with_an_unreadable_key(const char *path_root)
{
    int result = 0;
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    set_expected_results("load failed: setup", NULL, NULL, NULL, 0);
    json_t *tranger = open_tranger(path_root, DATABASE);
    open_treedb(tranger);
    const char *ids[] = {"k1", "k2", "k3", "k4", "k5", "k6", NULL};
    for(int i = 0; ids[i]; i++) {
        create_item(tranger, ids[i], "a");
    }
    treedb_close_db(tranger, TREEDB_NAME);
    test_json(NULL);    // the setup logs are not what is tested

    /*-------------------------------------*
     *  1. The md2 of k2 cut: the topic
     *  loads the other five and its feed
     *-------------------------------------*/
    const char *test = "1. a topic with an unreadable key loads every other key";
    set_expected_results_unordered(test,
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Cannot read record metadata, short read",
            "msg", "Cannot load the whole history of a key of the list: the records read before the failure "
                   "were handed, the list goes on with the next key",
            "msg", "treedb topic loaded WITHOUT the whole history of keys that cannot be read: "
                   "their node is in memory only if its newest record was read, and a create of those ids is refused"
        ),
        NULL, NULL, 1
    );
    result += cut_key(path_database, TOPIC_NAME, "k2");
    open_treedb(tranger);

    char bf[256];
    ids_in_memory(tranger, bf, sizeof(bf));
    if(strcmp(bf, "k1 k3 k4 k5 k6") != 0) {
        printf("%sERROR%s --> nodes in memory: [%s], expected [k1 k3 k4 k5 k6]\n",
            On_Red BWhite, Color_Off, bf);
        result += -1;
    }
    json_t *topic = tranger2_topic(tranger, TOPIC_NAME);
    size_t rt_lists = json_array_size(json_object_get(topic, "lists"));
    if(rt_lists != 2) {
        printf("%sERROR%s --> %d realtime lists on the topic, expected 2 (id and pkey2)\n",
            On_Red BWhite, Color_Off, (int)rt_lists);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  2. A create of k2 is refused, one
     *  of k7 is not and the feed has it
     *-------------------------------------*/
    test = "2. a create of an id that did not load is refused";
    set_expected_results(test,
        json_pack("[{s:s}]",
            "msg", "Cannot create node, its id has records on disk that could not be loaded"
        ),
        NULL, NULL, 1
    );
    if(create_item(tranger, "k2", "OVERWRITTEN")) {
        printf("%sERROR%s --> create of k2 ACCEPTED: its records on disk did not load\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!create_item(tranger, "k7", "a") ||
            !treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "k7")) {
        printf("%sERROR%s --> create of k7 refused\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("load failed: close", NULL, NULL, NULL, 1);
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  3: a __snaps__ that did not load whole
 ***************************************************************************/
PRIVATE int test_snaps_that_did_not_load(const char *path_root)
{
    int result = 0;
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), path_root, DATABASE2, NULL);
    rmrdir(path_database);

    set_expected_results("load failed snaps: setup", NULL, NULL, NULL, 0);
    json_t *tranger = open_tranger(path_root, DATABASE2);
    open_treedb(tranger);
    create_item(tranger, "k1", "a");
    create_item(tranger, "k2", "a");
    int ret = treedb_shoot_snap(tranger, TREEDB_NAME, "s1", "first");
    ret += treedb_activate_snap(tranger, TREEDB_NAME, "s1") > 0? 0: -1;
    treedb_close_db(tranger, TREEDB_NAME);
    test_json(NULL);    // the setup logs are not what is tested
    if(ret < 0) {
        printf("%sERROR%s --> cannot shoot and activate s1\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    const char *test = "3. a __snaps__ that did not load whole: shoot, activate and delete refuse";
    set_expected_results_unordered(test,
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Cannot read record metadata, short read",
            "msg", "Cannot load the whole history of a key of the list: the records read before the failure "
                   "were handed, the list goes on with the next key",
            "msg", "treedb topic loaded WITHOUT the whole history of keys that cannot be read: "
                   "their node is in memory only if its newest record was read, and a create of those ids is refused",
            "msg", "__snaps__ loaded without some snaps: the active snap is unknown, "
                   "shoot-snap, activate-snap and gc-files refuse",
            "msg", "Cannot shoot a snap: __snaps__ did not load whole, the active snap is unknown",
            "msg", "Cannot activate a snap: __snaps__ did not load whole, the active snap is unknown",
            "msg", "cannot tell which snaps exist: __snaps__ did not load whole",
            "msg", "cannot delete node, cannot tell whether a snapshot holds it (see the log)"
        ),
        NULL, NULL, 1
    );
    result += cut_key(path_database, "__snaps__", "1");     // s1: rowid id 1
    open_treedb(tranger);

    if(treedb_shoot_snap(tranger, TREEDB_NAME, "s2", "second") == 0) {
        printf("%sERROR%s --> shoot-snap ACCEPTED with the active snap unknown\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_activate_snap(tranger, TREEDB_NAME, "s1") >= 0) {
        printf("%sERROR%s --> activate-snap ACCEPTED with the active snap unknown\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    json_t *k1 = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "k1");
    if(!k1 || treedb_delete_node(tranger, k1, 0) == 0) {
        printf("%sERROR%s --> delete of k1 %s with the snaps unknown\n",
            On_Red BWhite, Color_Off, k1? "ACCEPTED": "not tried (k1 not in memory)");
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("load failed snaps: close", NULL, NULL, NULL, 1);
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/*
 *  Damage the files `ext` of a key, with no tranger running
 */
PRIVATE int damage_key(const char *path_database, const char *topic_name, const char *key,
    const char *ext, BOOL unreadable)
{
    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), path_database, topic_name, "keys", key, NULL);
    char pattern[32];
    snprintf(pattern, sizeof(pattern), ".*\\.%s", ext);
    dir_array_t da;
    get_ordered_filename_array(0, key_dir, pattern, WD_MATCH_REGULAR_FILE, &da);
    int done = 0;
    for(int i = 0; i < da.count; i++) {
        if(unreadable) {
            if(chmod(da.items[i], 0) == 0) {
                done++;
            }
        } else if(truncate(da.items[i], 0) == 0) {
            done++;
        }
    }
    dir_array_free(&da);
    if(done == 0) {
        printf("%sERROR%s --> cannot damage the %s of %s/%s\n",
            On_Red BWhite, Color_Off, ext, topic_name, key);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  4, 5, 6: a key damaged with the tranger down, found at the restart
 ***************************************************************************/
PRIVATE int test_damaged_at_restart(const char *path_root, const char *case_name,
    const char *ext, BOOL unreadable)
{
    int result = 0;
    if(unreadable && geteuid() == 0) {
        printf("%s: SKIPPED, running as root: a file of mode 000 is still read\n", case_name);
        return 0;
    }
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    set_expected_results("load failed at restart: setup", NULL, NULL, NULL, 0);
    json_t *tranger = open_tranger(path_root, DATABASE);
    open_treedb(tranger);
    const char *ids[] = {"k1", "k2", "k3", NULL};
    for(int i = 0; ids[i]; i++) {
        create_item(tranger, ids[i], "a");
    }
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
    test_json(NULL);    // the setup logs are not what is tested

    result += damage_key(path_database, TOPIC_NAME, "k2", ext, unreadable);

    /*
     *  What each damage logs, then what treedb says of it
     */
    json_t *expected = json_array();
    if(strcmp(ext, "json") == 0) {
        json_array_append_new(expected, json_pack("{s:s}",
            "msg", "Bad on-disk record: __offset__/__size__ out of range"));
    } else {
        if(unreadable) {
            json_array_append_new(expected, json_pack("{s:s}",
                "msg", "Cannot open md2 file"));
        }
        json_array_append_new(expected, json_pack("{s:s}",
            "msg", "md2 file of the key unreadable when its cache was built: every load of the key says load_failed"));
        json_array_append_new(expected, json_pack("{s:s}",
            "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built"));
    }
    json_array_append_new(expected, json_pack("{s:s}",
        "msg", "Cannot load the whole history of a key of the list: the records read before the failure "
               "were handed, the list goes on with the next key"));
    json_array_append_new(expected, json_pack("{s:s}",
        "msg", "treedb topic loaded WITHOUT the whole history of keys that cannot be read: "
               "their node is in memory only if its newest record was read, and a create of those ids is refused"));
    json_array_append_new(expected, json_pack("{s:s}",
        "msg", "Cannot create node, its id has records on disk that could not be loaded"));
    set_expected_results_unordered(case_name, expected, NULL, NULL, 1);

    tranger = open_tranger(path_root, DATABASE);
    open_treedb(tranger);

    char bf[256];
    ids_in_memory(tranger, bf, sizeof(bf));
    if(strcmp(bf, "k1 k3") != 0) {
        printf("%sERROR%s --> %s: nodes in memory: [%s], expected [k1 k3]\n",
            On_Red BWhite, Color_Off, case_name, bf);
        result += -1;
    }
    if(!json_object_get(
            json_object_get(json_object_get(json_object_get(tranger, "treedbs_load_failed"),
                TREEDB_NAME), TOPIC_NAME), "k2")) {
        printf("%sERROR%s --> %s: k2 is not in the registry of the keys that did not load\n",
            On_Red BWhite, Color_Off, case_name);
        result += -1;
    }
    if(create_item(tranger, "k2", "OVERWRITTEN")) {
        printf("%sERROR%s --> %s: create of k2 ACCEPTED: its records on disk did not load\n",
            On_Red BWhite, Color_Off, case_name);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  8. The recovery: the key deleted,
     *  its id is free, no reopen needed
     *-------------------------------------*/
    if(unreadable && strcmp(ext, "md2") == 0) {
        set_expected_results("8. the key deleted: a create of its id is accepted",
            json_pack("[{s:s}]",
                "msg", "A key that did not load has been deleted since: it is not a key that did not load any more"
            ),
            NULL, NULL, 1
        );
        if(tranger2_delete_key(tranger, TOPIC_NAME, "k2") < 0) {
            printf("%sERROR%s --> cannot delete the key k2\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        if(!create_item(tranger, "k2", "NEW")) {
            printf("%sERROR%s --> create of k2 refused after its key was deleted\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }
        result += test_json(NULL);
    }

    set_expected_results("load failed at restart: close", NULL, NULL, NULL, 1);
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/*
 *  Move every file of a key to the day file `day` (the key has one day)
 */
PRIVATE int move_key_to_day(const char *path_database, const char *key, const char *day)
{
    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), path_database, TOPIC_NAME, "keys", key, NULL);
    const char *exts[] = {"md2", "json", NULL};
    int moved = 0;
    for(int e = 0; exts[e]; e++) {
        char pattern[32];
        snprintf(pattern, sizeof(pattern), ".*\\.%s", exts[e]);
        dir_array_t da;
        get_ordered_filename_array(0, key_dir, pattern, WD_MATCH_REGULAR_FILE, &da);
        for(int i = 0; i < da.count; i++) {
            char name[NAME_MAX];
            char dst[PATH_MAX];
            snprintf(name, sizeof(name), "%s.%s", day, exts[e]);
            build_path(dst, sizeof(dst), key_dir, name, NULL);
            if(rename(da.items[i], dst) == 0) {
                moved++;
            }
        }
        dir_array_free(&da);
    }
    if(moved != 2) {
        printf("%sERROR%s --> cannot move the files of %s to %s\n", On_Red BWhite, Color_Off, key, day);
        return -1;
    }
    return 0;
}

/*
 *  The shape an update of `key` never acknowledged leaves in the day file
 *  `day`: its content written, its md2 created with no row
 */
PRIVATE int leave_uncommitted_file(const char *path_database, const char *key,
    const char *from_day, const char *day)
{
    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), path_database, TOPIC_NAME, "keys", key, NULL);
    char name[NAME_MAX];
    char src[PATH_MAX];
    char dst[PATH_MAX];
    snprintf(name, sizeof(name), "%s.json", from_day);
    build_path(src, sizeof(src), key_dir, name, NULL);
    snprintf(name, sizeof(name), "%s.json", day);
    build_path(dst, sizeof(dst), key_dir, name, NULL);
    int ret = copyfile(src, dst, 0660, TRUE);
    snprintf(name, sizeof(name), "%s.md2", day);
    build_path(dst, sizeof(dst), key_dir, name, NULL);
    FILE *f = fopen(dst, "w");
    if(ret < 0 || !f) {
        printf("%sERROR%s --> cannot leave an uncommitted file in %s\n", On_Red BWhite, Color_Off, key_dir);
        ret = -1;
    }
    if(f) {
        fclose(f);
    }
    return ret < 0? -1: 0;
}

PRIVATE int expect_payload(json_t *tranger, const char *case_name, const char *id, const char *payload)
{
    json_t *node = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, id);
    const char *found = node? kw_get_str(0, node, "payload", "", 0) : "(absent)";
    if(strcmp(found, payload) != 0) {
        printf("%sERROR%s --> %s: %s has payload [%s], expected [%s]\n",
            On_Red BWhite, Color_Off, case_name, id, found, payload);
        return -1;
    }
    return 0;
}

PRIVATE int expect_not_flagged(json_t *tranger, const char *case_name, const char *id)
{
    if(json_object_get(
            json_object_get(json_object_get(json_object_get(tranger, "treedbs_load_failed"),
                TREEDB_NAME), TOPIC_NAME), id)) {
        printf("%sERROR%s --> %s: %s is in the registry of the keys that did not load\n",
            On_Red BWhite, Color_Off, case_name, id);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  4a, 4b: a file whose update was never acknowledged, at the restart
 ***************************************************************************/
PRIVATE int test_uncommitted_at_restart(const char *path_root, BOOL in_the_middle)
{
    int result = 0;
    const char *case_name = in_the_middle?
        "4b. an uncommitted file between two versions of k2, at restart":
        "4a. an uncommitted newest file of k2, at restart";
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    set_expected_results("uncommitted at restart: setup", NULL, NULL, NULL, 0);
    json_t *tranger = open_tranger(path_root, DATABASE);
    open_treedb(tranger);
    const char *ids[] = {"k1", "k2", "k3", NULL};
    for(int i = 0; ids[i]; i++) {
        create_item(tranger, ids[i], "a");
    }
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);

    /*
     *  k2's version "a" in an older file
     */
    result += move_key_to_day(path_database, "k2", "2000-01-01");

    if(in_the_middle) {
        /*
         *  and its version "c" in today's file
         */
        tranger = open_tranger(path_root, DATABASE);
        open_treedb(tranger);
        json_t *k2 = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "k2");
        if(!k2 || !treedb_update_node(tranger, k2, json_pack("{s:s}", "payload", "c"), TRUE)) {
            printf("%sERROR%s --> %s: cannot update k2\n", On_Red BWhite, Color_Off, case_name);
            result += -1;
        }
        treedb_close_db(tranger, TREEDB_NAME);
        tranger2_shutdown(tranger);
    }
    test_json(NULL);    // the setup logs are not what is tested

    result += leave_uncommitted_file(path_database, "k2", "2000-01-01",
        in_the_middle? "2001-01-01": "2099-01-01");

    set_expected_results_unordered(case_name,
        json_pack("[{s:s}]",
            "msg", "md2 file of the key with no rows and a content file that is not empty: "
                   "an append that was never acknowledged, the file is ignored"
        ),
        NULL, NULL, 1
    );
    tranger = open_tranger(path_root, DATABASE);
    open_treedb(tranger);

    char bf[256];
    ids_in_memory(tranger, bf, sizeof(bf));
    if(strcmp(bf, "k1 k2 k3") != 0) {
        printf("%sERROR%s --> %s: nodes in memory: [%s], expected [k1 k2 k3]\n",
            On_Red BWhite, Color_Off, case_name, bf);
        result += -1;
    }
    result += expect_payload(tranger, case_name, "k2", in_the_middle? "c": "a");
    result += expect_not_flagged(tranger, case_name, "k2");
    result += test_json(NULL);

    /*
     *  A create of k2 is refused because k2 exists, not because it failed
     */
    set_expected_results(case_name,
        json_pack("[{s:s}]", "msg", "Node already exists"),
        NULL, NULL, 1
    );
    if(create_item(tranger, "k2", "OVERWRITTEN")) {
        printf("%sERROR%s --> %s: create of k2 ACCEPTED\n", On_Red BWhite, Color_Off, case_name);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("uncommitted at restart: close", NULL, NULL, NULL, 1);
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  7: a __snaps__ damaged with the tranger down
 ***************************************************************************/
PRIVATE int test_snaps_damaged_at_restart(const char *path_root)
{
    int result = 0;
    if(geteuid() == 0) {
        printf("7: SKIPPED, running as root: a file of mode 000 is still read\n");
        return 0;
    }
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), path_root, DATABASE2, NULL);
    rmrdir(path_database);

    set_expected_results("load failed snaps at restart: setup", NULL, NULL, NULL, 0);
    json_t *tranger = open_tranger(path_root, DATABASE2);
    open_treedb(tranger);
    create_item(tranger, "k1", "a");
    int ret = treedb_shoot_snap(tranger, TREEDB_NAME, "s1", "first");
    ret += treedb_activate_snap(tranger, TREEDB_NAME, "s1") > 0? 0: -1;
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
    test_json(NULL);    // the setup logs are not what is tested
    if(ret < 0) {
        printf("%sERROR%s --> cannot shoot and activate s1\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += damage_key(path_database, "__snaps__", "1", "md2", TRUE);

    const char *test = "7. a __snaps__ damaged at restart: shoot, activate and delete refuse";
    set_expected_results_unordered(test,
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Cannot open md2 file",
            "msg", "md2 file of the key unreadable when its cache was built: every load of the key says load_failed",
            "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built",
            "msg", "Cannot load the whole history of a key of the list: the records read before the failure "
                   "were handed, the list goes on with the next key",
            "msg", "treedb topic loaded WITHOUT the whole history of keys that cannot be read: "
                   "their node is in memory only if its newest record was read, and a create of those ids is refused",
            "msg", "__snaps__ loaded without some snaps: the active snap is unknown, "
                   "shoot-snap, activate-snap and gc-files refuse",
            "msg", "Cannot shoot a snap: __snaps__ did not load whole, the active snap is unknown",
            "msg", "Cannot activate a snap: __snaps__ did not load whole, the active snap is unknown",
            "msg", "cannot tell which snaps exist: __snaps__ did not load whole",
            "msg", "cannot delete node, cannot tell whether a snapshot holds it (see the log)"
        ),
        NULL, NULL, 1
    );
    tranger = open_tranger(path_root, DATABASE2);
    open_treedb(tranger);

    if(treedb_shoot_snap(tranger, TREEDB_NAME, "s2", "second") == 0) {
        printf("%sERROR%s --> shoot-snap ACCEPTED with the active snap unknown\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_activate_snap(tranger, TREEDB_NAME, "s1") >= 0) {
        printf("%sERROR%s --> activate-snap ACCEPTED with the active snap unknown\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    json_t *k1 = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "k1");
    if(!k1 || treedb_delete_node(tranger, k1, 0) == 0) {
        printf("%sERROR%s --> delete of k1 %s with the snaps unknown\n",
            On_Red BWhite, Color_Off, k1? "ACCEPTED": "not tried (k1 not in memory)");
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("load failed snaps at restart: close", NULL, NULL, NULL, 1);
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  9: the delete of a parent whose child topic did not load whole
 *
 *  boxes.things is a hook of things.box. t2 hangs from b1, and its key
 *  does not load: memory does not know it hangs from b1. A delete of b1,
 *  forced or not, is refused -- it unlinked t1, deleted b1, and left t2
 *  naming a parent that is gone.
 ***************************************************************************/
static char schema_boxes[]= "\
{                                                                   \n\
    'id': 'treedb_load_failed_boxes',                               \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'boxes',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {'header': 'Id', 'type': 'string', 'flag': ['persistent','required']}, \n\
                'things': {'header': 'Things', 'type': 'array', 'flag': ['hook'], 'hook': {'things': 'box'}} \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'things',                                 \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {'header': 'Id', 'type': 'string', 'flag': ['persistent','required']}, \n\
                'box': {'header': 'Box', 'type': 'string', 'flag': ['fkey']} \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

PRIVATE int test_delete_with_a_child_that_did_not_load(const char *path_root)
{
    int result = 0;
    const char *database = "tr_treedb_load_failed_boxes";
    const char *treedb_name = "treedb_load_failed_boxes";
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), path_root, database, NULL);
    rmrdir(path_database);
    helper_quote2doublequote(schema_boxes);

    set_expected_results("load failed boxes: setup", NULL, NULL, NULL, 0);
    json_t *tranger = open_tranger(path_root, database);
    treedb_open_db(tranger, treedb_name, legalstring2json(schema_boxes, TRUE), 0);
    json_t *b1 = treedb_create_node(tranger, treedb_name, "boxes", json_pack("{s:s}", "id", "b1"));
    json_t *b2 = treedb_create_node(tranger, treedb_name, "boxes", json_pack("{s:s}", "id", "b2"));
    json_t *t1 = treedb_create_node(tranger, treedb_name, "things", json_pack("{s:s}", "id", "t1"));
    json_t *t2 = treedb_create_node(tranger, treedb_name, "things", json_pack("{s:s}", "id", "t2"));
    if(!b1 || !b2 || !t1 || !t2 ||
            treedb_link_nodes(tranger, "things", b1, t1) < 0 ||
            treedb_link_nodes(tranger, "things", b1, t2) < 0) {
        printf("%sERROR%s --> setup of boxes failed\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    treedb_close_db(tranger, treedb_name);
    test_json(NULL);    // the setup logs are not what is tested

    result += cut_key(path_database, "things", "t2");
    set_expected_results("load failed boxes: reopen", NULL, NULL, NULL, 0);
    treedb_open_db(tranger, treedb_name, legalstring2json(schema_boxes, TRUE), 0);
    test_json(NULL);    // the load of the cut key is case 1's

    const char *test = "9. a parent whose child topic did not load whole is not deleted";
    set_expected_results(test,
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot delete node: a topic its hooks hold did not load whole, a child that did not load may hang from it",
            "msg", "Cannot delete node: a topic its hooks hold did not load whole, a child that did not load may hang from it"
        ),
        NULL, NULL, 1
    );
    b1 = treedb_get_node(tranger, treedb_name, "boxes", "b1");
    b2 = treedb_get_node(tranger, treedb_name, "boxes", "b2");
    t1 = treedb_get_node(tranger, treedb_name, "things", "t1");
    if(!b1 || !b2 || !t1 || treedb_get_node(tranger, treedb_name, "things", "t2")) {
        printf("%sERROR%s --> %s: the reload is not b1, b2, t1 without t2\n",
            On_Red BWhite, Color_Off, test);
        result += -1;
    } else {
        if(treedb_delete_node(tranger, b1, json_pack("{s:b}", "force", 1)) >= 0) {
            printf("%sERROR%s --> %s: the forced delete of b1 answered success\n",
                On_Red BWhite, Color_Off, test);
            result += -1;
        }
        if(treedb_delete_node(tranger, b2, 0) >= 0) {
            printf("%sERROR%s --> %s: the delete of b2, with no child in memory, answered success\n",
                On_Red BWhite, Color_Off, test);
            result += -1;
        }
        if(!treedb_get_node(tranger, treedb_name, "boxes", "b1") ||
                strcmp(kw_get_str(0, t1, "box", "", 0), "boxes^b1^things") != 0) {
            printf("%sERROR%s --> %s: b1 is gone, or t1 was unlinked\n",
                On_Red BWhite, Color_Off, test);
            result += -1;
        }
    }
    result += test_json(NULL);

    set_expected_results("load failed boxes: close", NULL, NULL, NULL, 1);
    treedb_close_db(tranger, treedb_name);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    rmrdir(path_database);
    return result;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    helper_quote2doublequote(schema_sample);

    result += test_topic_with_an_unreadable_key(path_root);
    result += test_snaps_that_did_not_load(path_root);
    result += test_uncommitted_at_restart(path_root, FALSE);
    result += test_uncommitted_at_restart(path_root, TRUE);
    result += test_damaged_at_restart(path_root, "5. the md2 of k2 cannot be read, at restart", "md2", TRUE);
    result += test_damaged_at_restart(path_root, "6. content of k2 cut to 0 bytes, at restart", "json", FALSE);
    result += test_snaps_damaged_at_restart(path_root);
    result += test_delete_with_a_child_that_did_not_load(path_root);

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    yev_loop_create(0, 2024, 10, NULL, &yev_loop);

    int result = do_test();

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    } else {
        printf("<-- %sTEST OK%s: %s\n", On_Green BWhite, Color_Off, APP);
    }
    return result < 0? -1 : 0;
}
