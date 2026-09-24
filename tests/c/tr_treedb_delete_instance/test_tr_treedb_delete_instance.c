/****************************************************************************
 *          test_tr_treedb_delete_instance.c
 *
 *          Regression coverage for treedb_delete_instance().
 *
 *          The function deletes ONE instance, one `pkey2` index slot:
 *            - the primary `id` index stays untouched,
 *            - every `.md2` row of (id, pkey2 value) is tombstoned
 *              (tranger2_delete_instance()), oldest first, and a
 *              tombstone that fails keeps the instance,
 *            - the other secondary indexes (when more than one is declared)
 *              stay untouched,
 *            - the instance leaves the hooks of its parents, and the
 *              children it holds go to the primary (the LINKS scenarios,
 *              which also pin treedb_delete_node() over every instance of
 *              a key, and the save that refuses a node no index holds).
 *
 *          The whole-node wipe is the job of treedb_delete_node()
 *          (which calls tranger2_delete_key() internally).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include <gobj.h>
#include <timeranger2.h>
#include <tr_treedb.h>
#include <yev_loop.h>
#include <testing.h>
#include <helpers.h>

#include "schema_sample.c"
#include "schema_links.c"

#define APP "test_tr_treedb_delete_instance"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE    "tr_delete_instance"
#define TOPIC_NAME  "items"
#define PKEY2_NAME  "version"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************************
 *  Verify treedb_delete_instance() drops the secondary slot
 *  without disturbing the primary index or the sibling items.
 ***************************************************************************/
PRIVATE int test_delete_instance_drops_secondary_keeps_primary(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "delete_instance drops secondary, keeps primary";
    time_measure_t time_measure;
    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    /*------------------------------------*
     *  Seed three nodes with the same
     *  shape but different ids/versions
     *------------------------------------*/
    treedb_create_node(
        tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}",
            "id", "item-1",
            "version", "v1",
            "payload", "alpha"
        )
    );
    treedb_create_node(
        tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}",
            "id", "item-2",
            "version", "v2",
            "payload", "beta"
        )
    );
    treedb_create_node(
        tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}",
            "id", "item-3",
            "version", "v3",
            "payload", "gamma"
        )
    );

    /*------------------------------------*
     *  Pre-conditions:
     *  - item-2 reachable by primary id
     *  - item-2 reachable by (id, pkey2)
     *  - both lookups return the SAME node pointer
     *------------------------------------*/
    json_t *node_by_id = treedb_get_node(
        tranger, treedb_name, TOPIC_NAME, "item-2"
    );
    json_t *node_by_pkey2 = treedb_get_instance(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "item-2", "v2"
    );
    if(!node_by_id) {
        printf("%s  FAIL: item-2 missing in primary index before delete%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!node_by_pkey2) {
        printf("%s  FAIL: item-2 missing in pkey2 index before delete%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(node_by_id != node_by_pkey2) {
        printf("%s  FAIL: primary and pkey2 lookups return different pointers%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*------------------------------------*
     *  Act: drop item-2 from the pkey2
     *  index.  Mirror c_node.c's calling
     *  convention: pass the pure_node
     *  pointer as-is (no extra incref,
     *  despite the "owned" annotation —
     *  see c_node.c::ac_delete_node).
     *------------------------------------*/
    int rc = treedb_delete_instance(
        tranger,
        node_by_pkey2,
        PKEY2_NAME,
        NULL
    );
    if(rc != 0) {
        printf("%s  FAIL: treedb_delete_instance() returned %d, expected 0%s\n",
            On_Red BWhite, rc, Color_Off);
        result += -1;
    }

    /*------------------------------------*
     *  Post-conditions
     *------------------------------------*/
    if(treedb_get_instance(
            tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "item-2", "v2") != NULL)
    {
        printf("%s  FAIL: item-2 still in pkey2 index after delete%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_get_node(tranger, treedb_name, TOPIC_NAME, "item-2") == NULL) {
        printf("%s  FAIL: item-2 vanished from primary index (should stay)%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*------------------------------------*
     *  Siblings untouched in BOTH indexes
     *------------------------------------*/
    if(treedb_get_node(tranger, treedb_name, TOPIC_NAME, "item-1") == NULL ||
       treedb_get_node(tranger, treedb_name, TOPIC_NAME, "item-3") == NULL)
    {
        printf("%s  FAIL: sibling items vanished from primary index%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_get_instance(
            tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "item-1", "v1") == NULL ||
       treedb_get_instance(
            tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "item-3", "v3") == NULL)
    {
        printf("%s  FAIL: sibling items vanished from pkey2 index%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  An instance a SNAPSHOT froze is not deleted, updated or not.
 *
 *  A delete-instance tombstones every md2 row of (id, pkey2 value), the
 *  frozen one included, so the guard has to ask the RECORDS of the key --
 *  the ones of this instance -- and not the tag the node carries in
 *  memory: a save is untagged, so an instance updated after the shot
 *  carries 0 while the record the snap froze is still under it. That is
 *  the case this pins. `force` does NOT override it (it is about links,
 *  which a delete-instance does not look at); `ignore_snaps` does.
 ***************************************************************************/
PRIVATE int test_instance_held_by_a_snap(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "an instance a snapshot holds is not deleted";
    time_measure_t time_measure;
    set_expected_results(
        test,
        json_pack("[{s:s}, {s:s}]",
            "msg", "cannot delete instance, a snapshot still holds it",
            "msg", "cannot delete instance, a snapshot still holds it"
        ),
        NULL, NULL, 1
    );
    MT_START_TIME(time_measure)

    treedb_create_node(
        tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "item-9", "version", "v9", "payload", "shot")
    );

    if(treedb_shoot_snap(tranger, treedb_name, "S", "")<0) {
        printf("%s  FAIL: cannot shoot the snap%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  The update leaves the primary at tag 0, with the frozen record below  */
    json_t *node = treedb_get_instance(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "item-9", "v9"
    );
    treedb_update_node(tranger, node, json_pack("{s:s}", "payload", "after"), TRUE);
    if(kw_get_int(0, node, "__md_treedb__`tag", -1, 0) != 0) {
        printf("%s  FAIL: the updated instance carries tag %d, expected 0%s\n",
            On_Red BWhite, (int)kw_get_int(0, node, "__md_treedb__`tag", -1, 0), Color_Off);
        result += -1;
    }

    /*  Refused: the snap still holds a record of this instance  */
    if(treedb_delete_instance(tranger, node, PKEY2_NAME, NULL) == 0) {
        printf("%s  FAIL: an instance snap S froze was deleted%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "item-9", "v9")) {
        printf("%s  FAIL: the refused delete took the instance anyway%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  Another instance of the same key, shot by NO snap, still goes  */
    treedb_create_node(
        tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "item-9", "version", "v10", "payload", "free")
    );
    json_t *free_inst = treedb_get_instance(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "item-9", "v10"
    );
    if(treedb_delete_instance(tranger, free_inst, PKEY2_NAME, NULL) != 0) {
        printf("%s  FAIL: an instance no snap holds was refused%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  force does not override; ignore_snaps does  */
    node = treedb_get_instance(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "item-9", "v9"
    );
    if(treedb_delete_instance(tranger, node, PKEY2_NAME, json_pack("{s:b}", "force", 1)) == 0) {
        printf("%s  FAIL: force deleted an instance a snap froze%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    node = treedb_get_instance(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "item-9", "v9"
    );
    if(treedb_delete_instance(tranger, node, PKEY2_NAME, json_pack("{s:b}", "ignore_snaps", 1)) != 0) {
        printf("%s  FAIL: ignore_snaps did not delete the held instance%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  A snapshot guard that cannot READ the key refuses (fails closed).
 *
 *  Both guards read the key's records from disk when the tag in memory
 *  does not answer. A read that failed half way (a row of the md2 that is
 *  not there) was taken as "no snapshot holds it", and the delete went on.
 *  The md2 of the key is cut behind treedb's back to make the read fail.
 ***************************************************************************/
PRIVATE int test_guard_that_cannot_read_refuses(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "a snapshot guard that cannot read refuses the delete";
    set_expected_results_unordered(
        test,
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Cannot read record metadata, short read",
            "msg", "cannot read the records of a key",
            "msg", "cannot delete instance, cannot tell whether a snapshot holds it (see the log)",
            "msg", "cannot delete node, cannot tell whether a snapshot holds it (see the log)"
        ),
        NULL, NULL, 1
    );

    /*  a snap exists (test_instance_held_by_a_snap shot "S"); this key is
     *  newer than it, so the tag in memory does not answer: the disk does  */
    treedb_create_node(
        tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "item-11", "version", "v11", "payload", "x")
    );
    json_t *node = treedb_get_instance(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "item-11", "v11"
    );
    treedb_update_node(tranger, node, json_pack("{s:s}", "payload", "y"), TRUE);

    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir),
        getenv("HOME"), "tests_yuneta", DATABASE, TOPIC_NAME, "keys", "item-11", NULL);
    dir_array_t da;
    get_ordered_filename_array(0, key_dir, ".*\\.md2", WD_MATCH_REGULAR_FILE, &da);
    for(int i=0; i<da.count; i++) {
        if(truncate(da.items[i], 0) < 0) {
            printf("%s  FAIL: cannot cut %s%s\n", On_Red BWhite, da.items[i], Color_Off);
            result += -1;
        }
    }
    if(da.count == 0) {
        printf("%s  FAIL: no md2 of the key under %s%s\n", On_Red BWhite, key_dir, Color_Off);
        result += -1;
    }
    dir_array_free(&da);

    if(treedb_delete_instance(tranger, node, PKEY2_NAME, NULL) == 0) {
        printf("%s  FAIL: delete_instance went on with a guard that could not read%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    node = treedb_get_node(tranger, treedb_name, TOPIC_NAME, "item-11");
    if(!node || treedb_delete_node(tranger, node, json_pack("{s:b}", "force", 1)) == 0) {
        printf("%s  FAIL: delete_node went on with a guard that could not read%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  Verify treedb_delete_node() still wipes the whole record
 *  (primary index + on-disk row) after the prior delete_instance.
 ***************************************************************************/
PRIVATE int test_delete_node_clears_everything(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "delete_node clears primary index";
    time_measure_t time_measure;
    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    json_t *node = treedb_get_node(
        tranger, treedb_name, TOPIC_NAME, "item-2"
    );
    if(!node) {
        printf("%s  FAIL: item-2 missing before whole-node delete%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
        return result;
    }

    /*  The snap of the test above holds item-2 too: this one is about the
     *  whole-key delete, so it says so  */
    int rc = treedb_delete_node(
        tranger,
        node,
        json_pack("{s:b, s:b}", "force", 1, "ignore_snaps", 1)
    );
    if(rc != 0) {
        printf("%s  FAIL: treedb_delete_node() returned %d, expected 0%s\n",
            On_Red BWhite, rc, Color_Off);
        result += -1;
    }

    if(treedb_get_node(tranger, treedb_name, TOPIC_NAME, "item-2") != NULL) {
        printf("%s  FAIL: item-2 still in primary index after delete_node%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  Siblings remain  */
    if(treedb_get_node(tranger, treedb_name, TOPIC_NAME, "item-1") == NULL ||
       treedb_get_node(tranger, treedb_name, TOPIC_NAME, "item-3") == NULL)
    {
        printf("%s  FAIL: siblings vanished after item-2 delete_node%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  Durability: a per-instance delete must survive a full close + reopen,
 *  even when the instance accumulated MULTIPLE md2 rows (create + update +
 *  link re-saves all append a row with the same (id, pkey2)). Tombstoning
 *  only the latest row lets the loader fall back to an earlier one on reopen.
 *
 *  Self-contained (own database) so it never entangles do_test's tranger.
 ***************************************************************************/
PRIVATE int test_durable_delete_across_reopen(void)
{
    int result = 0;
    const char *test = "delete_instance is durable across reopen (multi-row)";
    const char *treedb_name = "treedb_delete_durable";
    const char *DB = "tr_delete_instance_durable";
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    build_path(path_database, sizeof(path_database), path_root, DB, NULL);
    rmrdir(path_database);

    helper_quote2doublequote(schema_sample);

    /*------------------------------------*
     *  Phase 1: open, seed, multi-row,
     *  delete the non-primary v2
     *------------------------------------*/
    json_t *tranger;
    {
        set_expected_results(test,
            json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s}]",
                "msg", "Creating __timeranger2__.json",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic"),
            NULL, NULL, 0);
        json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root, "database", DB, "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        );
        tranger = tranger2_startup(0, jn_tranger, 0);
        json_t *jn_schema = legalstring2json(schema_sample, TRUE);
        if(!treedb_open_db(tranger, treedb_name, jn_schema, 0)) {
            result += -1;
        }

        treedb_create_node(tranger, treedb_name, TOPIC_NAME,
            json_pack("{s:s, s:s, s:s}", "id", "rel-1", "version", "v1", "payload", "a"));
        treedb_create_node(tranger, treedb_name, TOPIC_NAME,
            json_pack("{s:s, s:s, s:s}", "id", "rel-1", "version", "v2", "payload", "b"));
        treedb_create_node(tranger, treedb_name, TOPIC_NAME,
            json_pack("{s:s, s:s, s:s}", "id", "rel-1", "version", "v3", "payload", "c"));

        /*  Make v2 a MULTI-ROW instance: update it so a 2nd md2 row exists.
         *  (mirrors create-yuno + gobj_link_nodes re-appending the same id/pkey2)  */
        json_t *v2 = treedb_get_instance(
            tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v2");
        treedb_update_node(tranger, v2,
            json_pack("{s:s}", "payload", "b-updated"), TRUE);
        treedb_update_node(tranger,
            treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v2"),
            json_pack("{s:s}", "payload", "b-updated-again"), TRUE);

        /*  v2 is non-primary (v3 highest rowid = primary). Delete the instance.  */
        json_t *v2b = treedb_get_instance(
            tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v2");
        if(treedb_delete_instance(tranger, v2b, PKEY2_NAME, json_pack("{s:b}", "force", 1)) != 0) {
            printf("%s  FAIL: delete_instance v2 failed%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        if(treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v2") != NULL) {
            printf("%s  FAIL: v2 still present in memory after delete%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }

        /*
         *  A pkey2 VALUE is data, not a file name: one with a '/' was the id
         *  of the transient rt_disk feed of the delete, and logged two
         *  errors per delete ("Invalid rt id", "Cannot open rt"). No log now,
         *  and the rows are tombstoned all the same.
         */
        treedb_create_node(tranger, treedb_name, TOPIC_NAME,
            json_pack("{s:s, s:s, s:s}", "id", "rel-1", "version", "v/4", "payload", "d"));
        treedb_update_node(tranger,
            treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v/4"),
            json_pack("{s:s}", "payload", "d-updated"), TRUE);
        treedb_create_node(tranger, treedb_name, TOPIC_NAME,
            json_pack("{s:s, s:s, s:s}", "id", "rel-1", "version", "v5", "payload", "e"));
        json_t *v4 = treedb_get_instance(
            tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v/4");
        if(!v4 || treedb_delete_instance(tranger, v4, PKEY2_NAME, NULL) != 0) {
            printf("%s  FAIL: delete_instance of a pkey2 value with '/' failed%s\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }

        treedb_close_db(tranger, treedb_name);
        tranger2_shutdown(tranger);
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Phase 2: reopen, v2 must NOT
     *  resurrect from an earlier row
     *------------------------------------*/
    {
        set_expected_results(test, NULL, NULL, NULL, 0);
        json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root, "database", DB, "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        );
        tranger = tranger2_startup(0, jn_tranger, 0);
        json_t *jn_schema = legalstring2json(schema_sample, TRUE);
        if(!treedb_open_db(tranger, treedb_name, jn_schema, 0)) {
            result += -1;
        }

        if(treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v2") != NULL) {
            printf("%s  FAIL: v2 RESURRECTED after reopen (multi-row delete not durable)%s\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }
        if(treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v/4") != NULL) {
            printf("%s  FAIL: v/4 RESURRECTED after reopen%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        if(treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v1") == NULL ||
           treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v3") == NULL) {
            printf("%s  FAIL: v1/v3 lost after reopen%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        if(treedb_get_node(tranger, treedb_name, TOPIC_NAME, "rel-1") == NULL) {
            printf("%s  FAIL: rel-1 missing from primary after reopen%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        treedb_close_db(tranger, treedb_name);
        tranger2_shutdown(tranger);
        result += test_json(NULL);
    }

    return result;
}

/***************************************************************************
 *  A delete that cannot READ every row of the instance refuses.
 *
 *  delete_instance tombstones every md2 row of (id, pkey2 value), read with
 *  a one-shot iterator of the key. It ignored the iterator's load_failed:
 *  a read that stopped half way tombstoned what it had read (or nothing),
 *  dropped the secondary slot, answered 0 -- and the instance came back at
 *  the next open (7.25.4). And a
 *  row whose CONTENT could not be read was taken, silently, as a row of
 *  another instance.
 *
 *  Self-contained (own database): the md2 and the content of the keys are
 *  cut behind treedb's back.
 ***************************************************************************/
PRIVATE int cut_files_of_key(const char *db, const char *id, const char *pattern, int keep_rows)
{
    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir),
        getenv("HOME"), "tests_yuneta", db, TOPIC_NAME, "keys", id, NULL);
    dir_array_t da;
    get_ordered_filename_array(0, key_dir, pattern, WD_MATCH_REGULAR_FILE, &da);
    int cut = 0;
    for(int i=0; i<da.count; i++) {
        if(truncate(da.items[i], (off_t)keep_rows * 32) == 0) {
            cut++;
        }
    }
    dir_array_free(&da);
    return cut;
}

PRIVATE json_t *open_db(const char *path_root, const char *db, const char *treedb_name)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root, "database", db, "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);
    treedb_open_db(tranger, treedb_name, legalstring2json(schema_sample, TRUE), 0);
    return tranger;
}

PRIVATE int test_delete_that_cannot_read_refuses(void)
{
    int result = 0;
    const char *test = "a delete_instance that cannot read every row refuses";
    const char *treedb_name = "treedb_delete_unreadable";
    const char *DB = "tr_delete_instance_unreadable";
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    build_path(path_database, sizeof(path_database), path_root, DB, NULL);
    rmrdir(path_database);
    helper_quote2doublequote(schema_sample);

    set_expected_results(test,
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic"),
        NULL, NULL, 1);
    json_t *tranger = open_db(path_root, DB, treedb_name);

    treedb_create_node(tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "rel-1", "version", "v1", "payload", "a"));
    treedb_create_node(tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "rel-1", "version", "v2", "payload", "b"));
    treedb_create_node(tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "rel-1", "version", "v3", "payload", "c"));
    treedb_create_node(tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "rel-2", "version", "w1", "payload", "a"));
    treedb_create_node(tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "rel-2", "version", "w2", "payload", "b"));
    result += test_json(NULL);

    /*------------------------------------*
     *  The newest md2 row of rel-1 is cut
     *------------------------------------*/
    set_expected_results(test,
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot read record metadata, short read",
            "msg", "Cannot delete instance, cannot read every row of its key"),
        NULL, NULL, 1);
    if(cut_files_of_key(DB, "rel-1", ".*\\.md2", 2) != 1) {
        printf("%s  FAIL: cannot cut the md2 of rel-1%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    json_t *node = treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v2");
    if(!node || treedb_delete_instance(tranger, node, PKEY2_NAME, NULL) == 0) {
        printf("%s  FAIL: delete_instance answered 0 with a row it could not read%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v2")) {
        printf("%s  FAIL: the refused delete dropped the instance from memory%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*------------------------------------*
     *  The content of rel-2 cannot be read:
     *  it ends the walk, as a row whose
     *  metadata cannot be read does
     *------------------------------------*/
    set_expected_results(test,
        json_pack("[{s:s},{s:s}]",
            "msg", "Bad on-disk record: __offset__/__size__ out of range",
            "msg", "Cannot delete instance, cannot read every row of its key"),
        NULL, NULL, 1);
    if(cut_files_of_key(DB, "rel-2", ".*\\.json", 0) != 1) {
        printf("%s  FAIL: cannot cut the content of rel-2%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    node = treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-2", "w1");
    if(!node || treedb_delete_instance(tranger, node, PKEY2_NAME, NULL) == 0) {
        printf("%s  FAIL: delete_instance answered 0 with a content it could not read%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-2", "w1")) {
        printf("%s  FAIL: the refused delete dropped w1 from memory%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    treedb_close_db(tranger, treedb_name);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    /*------------------------------------*
     *  Reopen: v2 was not deleted, and
     *  says so (it did not resurrect: it
     *  never went)
     *------------------------------------*/
    set_expected_results(test, NULL, NULL, NULL, 0);
    tranger = open_db(path_root, DB, treedb_name);
    if(!treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-1", "v2")) {
        printf("%s  FAIL: v2 is gone after the reopen%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    treedb_close_db(tranger, treedb_name);
    tranger2_shutdown(tranger);
    test_json(NULL);    // what the load of the cut content says is not this test's

    return result;
}

/***************************************************************************
 *  __wrap_write(): with `fail_writes_key` set (a key directory, ending in
 *  '/'), the writes into an md2 file of that key go through until
 *  `fail_writes_after` of them were written, and fail with EIO after.
 *  Every other write goes through (see CMakeLists.txt).
 ***************************************************************************/
PRIVATE char fail_writes_key[PATH_MAX];
PRIVATE int fail_writes_after = 0;

ssize_t __real_write(int fd, const void *buf, size_t count);
ssize_t __wrap_write(int fd, const void *buf, size_t count);

ssize_t __wrap_write(int fd, const void *buf, size_t count)
{
    if(fail_writes_key[0]) {
        char link[PATH_MAX];
        char target[PATH_MAX];
        snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
        ssize_t ln = readlink(link, target, sizeof(target) - 1);
        if(ln > 4) {
            target[ln] = 0;
            if(strncmp(target, fail_writes_key, strlen(fail_writes_key)) == 0 &&
                    strcmp(target + ln - 4, ".md2") == 0) {
                if(fail_writes_after <= 0) {
                    errno = EIO;
                    return -1;
                }
                fail_writes_after--;
            }
        }
    }
    return __real_write(fd, buf, count);
}

/***************************************************************************
 *  A tombstone that fails part way: the instance stays.
 *
 *  delete_instance tombstones every md2 row of (id, pkey2 value). It
 *  tombstoned them newest first, logged a failure and went on, dropped the
 *  instance and answered 0: the rows before the failure stayed alive, and
 *  the instance came back at the next open, from an OLDER row (7.25.4). A
 *  tombstone cannot be taken back, so the rows go oldest first and the
 *  first failure stops the delete: the newest row stays alive, the delete
 *  answers -1, and memory keeps the instance, as the next open finds it.
 *
 *  rel-3/v1 has two rows (created, then updated). The second tombstone
 *  write fails (__wrap_write()).
 ***************************************************************************/
PRIVATE int test_tombstone_that_fails_part_way(void)
{
    int result = 0;
    const char *test = "a delete_instance whose tombstone fails part way keeps the instance";
    const char *treedb_name = "treedb_delete_tombstone";
    const char *DB = "tr_delete_instance_tombstone";
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    build_path(path_database, sizeof(path_database), path_root, DB, NULL);
    rmrdir(path_database);
    helper_quote2doublequote(schema_sample);

    set_expected_results(test,
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic"),
        NULL, NULL, 1);
    json_t *tranger = open_db(path_root, DB, treedb_name);
    treedb_create_node(tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "rel-3", "version", "v1", "payload", "old"));
    json_t *node = treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-3", "v1");
    if(!node || !treedb_update_node(tranger, node, json_pack("{s:s}", "payload", "new"), TRUE)) {
        printf("%s  FAIL: setup of rel-3/v1 failed%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results(test,
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot re-write record metadata, write FAILED",
            "msg", "Cannot delete instance, a row of it cannot be tombstoned: the instance stays, its newest rows alive"),
        NULL, NULL, 1);
    build_path(fail_writes_key, sizeof(fail_writes_key) - 1,
        path_database, TOPIC_NAME, "keys", "rel-3", NULL);
    strcat(fail_writes_key, "/");
    fail_writes_after = 1;
    node = treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-3", "v1");
    int ret = node? treedb_delete_instance(tranger, node, PKEY2_NAME, NULL) : 0;
    fail_writes_key[0] = 0;
    if(ret == 0) {
        printf("%s  FAIL: delete_instance answered 0 with a tombstone that failed%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-3", "v1")) {
        printf("%s  FAIL: the refused delete dropped the instance from memory%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    treedb_close_db(tranger, treedb_name);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    /*
     *  Reopen: the instance is there, as memory had it (the newest row)
     */
    set_expected_results(test, NULL, NULL, NULL, 1);
    tranger = open_db(path_root, DB, treedb_name);
    node = treedb_get_instance(tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "rel-3", "v1");
    if(!node || strcmp(kw_get_str(0, node, "payload", "", 0), "new") != 0) {
        printf("%s  FAIL: after the reopen rel-3/v1 is not the newest row: %s%s\n",
            On_Red BWhite, node? kw_get_str(0, node, "payload", "", 0) : "(gone)", Color_Off);
        result += -1;
    }
    treedb_close_db(tranger, treedb_name);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    rmrdir(path_database);

    return result;
}

/***************************************************************************
 *  LINKS: instances that hang from, or hold, other nodes
 *
 *  A treedb instance is one pkey2 value of a key, and every instance is a
 *  node object of its own: a link made at run time lands on the instance it
 *  is given, not on the primary. The fkey of a child names the parent's ID,
 *  never one of its instances, so a reload hangs every child from the
 *  PRIMARY of its parent, and links only the primary of a child.
 *
 *  These tests pin what the deletes do with those links:
 *    - treedb_delete_node() deletes the KEY, every instance of it, so it
 *      looks at the children and the parents of every instance (it looked
 *      at the one it was given: 7.25.4 left a child with a dangling fkey,
 *      and a ghost in a parent's hook that a later forced delete saved back
 *      into the deleted key);
 *    - treedb_delete_instance() takes the instance out of the hooks of its
 *      parents, where the primary takes its place when it names that
 *      parent too, and hands the children the instance holds to the
 *      primary -- what a reload does (7.25.4 left the instance in the
 *      hooks, and a forced delete of the parent saved it back: after a
 *      reopen it was the newest row, the primary);
 *    - treedb_save_node() refuses a node no index holds: its record would
 *      bring back a key or an instance that was deleted.
 *
 *  Every scenario is self-contained (its own database) and reopens it: what
 *  memory said must be what the disk says.
 ***************************************************************************/
#define L_TREEDB    "treedb_links"
#define L_PARENTS   "parents"
#define L_KIDS      "kids"

PRIVATE json_t *open_links_db(const char *path_root, const char *db)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root, "database", db, "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);
    treedb_open_db(tranger, L_TREEDB, legalstring2json(schema_links, TRUE), 0);
    return tranger;
}

PRIVATE void close_links_db(json_t *tranger)
{
    treedb_close_db(tranger, L_TREEDB);
    tranger2_shutdown(tranger);
}

PRIVATE json_t *new_links_db(const char *test, const char *db, char *path_root, size_t size, int *result)
{
    char path_database[PATH_MAX];
    build_path(path_root, size, getenv("HOME"), "tests_yuneta", NULL);
    build_path(path_database, sizeof(path_database), path_root, db, NULL);
    rmrdir(path_database);
    helper_quote2doublequote(schema_links);

    set_expected_results(test,
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic"),
        NULL, NULL, 1);
    json_t *tranger = open_links_db(path_root, db);
    *result += test_json(NULL);
    return tranger;
}

PRIVATE json_t *l_create(json_t *tranger, const char *topic, const char *id, const char *version)
{
    return treedb_create_node(tranger, L_TREEDB, topic,
        json_pack("{s:s, s:s}", "id", id, "version", version)
    );
}

PRIVATE json_t *l_instance(json_t *tranger, const char *topic, const char *id, const char *version)
{
    return treedb_get_instance(tranger, L_TREEDB, topic, "version", id, version);
}

PRIVATE json_t *l_node(json_t *tranger, const char *topic, const char *id)
{
    return treedb_get_node(tranger, L_TREEDB, topic, id);
}

PRIVATE size_t l_hook_size(json_t *node, const char *hook)
{
    json_t *data = node? json_object_get(node, hook) : NULL;
    if(json_is_array(data)) {
        return json_array_size(data);
    }
    if(json_is_object(data)) {
        return json_object_size(data);
    }
    return 0;
}

PRIVATE BOOL l_hook_holds(json_t *node, const char *hook, json_t *child)
{
    json_t *data = node? json_object_get(node, hook) : NULL;
    if(json_is_array(data)) {
        size_t idx; json_t *v;
        json_array_foreach(data, idx, v) {
            if(v == child) {
                return TRUE;
            }
        }
    } else if(json_is_object(data)) {
        const char *key; json_t *v;
        json_object_foreach(data, key, v) {
            if(v == child) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/*
 *  How many parent refs the fkey columns of `node` hold
 */
PRIVATE size_t l_up_refs(json_t *node)
{
    size_t n = 0;
    const char *parent = json_string_value(json_object_get(node, "parent"));
    if(!empty_string(parent)) {
        n++;
    }
    n += json_array_size(json_object_get(node, "tags"));
    return n;
}

PRIVATE int l_check(BOOL ok, const char *what)
{
    if(!ok) {
        printf("%s  FAIL: %s%s\n", On_Red BWhite, what, Color_Off);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  A delete of a parent sees the children of EVERY instance of its key.
 *
 *  P/v1 is the primary, and kid a hangs from P/v2 through the list hook,
 *  kid b through the dict hook. A delete takes the key whole: without
 *  force it is refused, with force a and b are unlinked and saved. 7.25.4
 *  looked at P/v1 alone: the delete went, a and b named a parent that is
 *  gone, and the reopen said "Node not found".
 ***************************************************************************/
PRIVATE int test_delete_node_sees_children_of_every_instance(void)
{
    int result = 0;
    const char *test = "a delete of a parent sees the children of every instance";
    const char *DB = "tr_delete_instance_links1";
    char path_root[PATH_MAX];
    json_t *tranger = new_links_db(test, DB, path_root, sizeof(path_root), &result);

    set_expected_results(test,
        json_pack("[{s:s}]",
            "msg", "Cannot delete node: has down links"),
        NULL, NULL, 1);

    l_create(tranger, L_PARENTS, "P", "v1");
    l_create(tranger, L_PARENTS, "P", "v2");
    l_create(tranger, L_KIDS, "a", "k1");
    l_create(tranger, L_KIDS, "b", "k1");
    json_t *p1 = l_instance(tranger, L_PARENTS, "P", "v1");
    json_t *p2 = l_instance(tranger, L_PARENTS, "P", "v2");
    result += l_check(l_node(tranger, L_PARENTS, "P") == p1, "P/v1 is not the primary");
    result += l_check(
        treedb_link_nodes(tranger, "kids", p2, l_node(tranger, L_KIDS, "a")) == 0,
        "cannot link P/v2 <- a"
    );
    result += l_check(
        treedb_link_nodes(tranger, "tags", p2, l_node(tranger, L_KIDS, "b")) == 0,
        "cannot link P/v2 <- b"
    );

    result += l_check(
        treedb_delete_node(tranger, p1, json_object()) < 0,
        "a delete WITHOUT force of a parent whose other instance holds children went"
    );
    result += l_check(l_node(tranger, L_PARENTS, "P") != NULL, "the refused delete took P");
    result += l_check(l_up_refs(l_node(tranger, L_KIDS, "a")) == 1, "the refused delete unlinked a");

    result += l_check(
        treedb_delete_node(tranger, l_node(tranger, L_PARENTS, "P"), json_pack("{s:b}", "force", 1)) == 0,
        "the forced delete of P was refused"
    );
    result += l_check(l_node(tranger, L_PARENTS, "P") == NULL, "P is still in memory");
    result += l_check(l_up_refs(l_node(tranger, L_KIDS, "a")) == 0, "a still names the deleted P");
    result += l_check(l_up_refs(l_node(tranger, L_KIDS, "b")) == 0, "b still names the deleted P");
    close_links_db(tranger);
    result += test_json(NULL);

    /*
     *  Reopen: nothing names P, so the load says nothing
     */
    set_expected_results(test, NULL, NULL, NULL, 1);
    tranger = open_links_db(path_root, DB);
    result += l_check(l_node(tranger, L_PARENTS, "P") == NULL, "P is back after the reopen");
    result += l_check(l_up_refs(l_node(tranger, L_KIDS, "a")) == 0, "after the reopen a names the deleted P");
    result += l_check(l_up_refs(l_node(tranger, L_KIDS, "b")) == 0, "after the reopen b names the deleted P");
    close_links_db(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  A delete of a child takes EVERY instance of its key out of the hooks
 *  of its parents.
 *
 *  x/v1 and y/v1 are the primaries, hanging from nothing; x/v2 hangs from
 *  O2 through the list hook, y/v2 through the dict hook. Without force the
 *  delete is refused (up links); with force the instances leave O2's
 *  hooks. 7.25.4 looked at the primaries alone: the non-forced delete went,
 *  O2's hooks kept the ghosts, and the forced delete of O2 that followed
 *  saved them back into the deleted keys -- x and y were back after the
 *  reopen.
 ***************************************************************************/
PRIVATE int test_delete_node_unhooks_every_instance(void)
{
    int result = 0;
    const char *test = "a delete of a child takes every instance out of its parents' hooks";
    const char *DB = "tr_delete_instance_links2";
    char path_root[PATH_MAX];
    json_t *tranger = new_links_db(test, DB, path_root, sizeof(path_root), &result);

    set_expected_results(test,
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot delete node: has up links",
            "msg", "Cannot delete node: has up links"),
        NULL, NULL, 1);

    l_create(tranger, L_PARENTS, "O2", "1");
    l_create(tranger, L_KIDS, "x", "v1");
    l_create(tranger, L_KIDS, "x", "v2");
    l_create(tranger, L_KIDS, "y", "v1");
    l_create(tranger, L_KIDS, "y", "v2");
    json_t *o2 = l_node(tranger, L_PARENTS, "O2");
    json_t *x2 = l_instance(tranger, L_KIDS, "x", "v2");
    json_t *y2 = l_instance(tranger, L_KIDS, "y", "v2");
    result += l_check(l_node(tranger, L_KIDS, "x") != x2, "x/v2 is the primary");
    result += l_check(treedb_link_nodes(tranger, "kids", o2, x2) == 0, "cannot link O2 <- x/v2");
    result += l_check(treedb_link_nodes(tranger, "tags", o2, y2) == 0, "cannot link O2 <- y/v2");

    result += l_check(
        treedb_delete_node(tranger, l_node(tranger, L_KIDS, "x"), json_object()) < 0,
        "a delete WITHOUT force of x went, with x/v2 in a hook of O2"
    );
    result += l_check(
        treedb_delete_node(tranger, l_node(tranger, L_KIDS, "y"), json_object()) < 0,
        "a delete WITHOUT force of y went, with y/v2 in a hook of O2"
    );
    result += l_check(l_hook_holds(o2, "kids", x2), "the refused delete took x/v2 out of O2");

    result += l_check(
        treedb_delete_node(tranger, l_node(tranger, L_KIDS, "x"), json_pack("{s:b}", "force", 1)) == 0,
        "the forced delete of x was refused"
    );
    result += l_check(
        treedb_delete_node(tranger, l_node(tranger, L_KIDS, "y"), json_pack("{s:b}", "force", 1)) == 0,
        "the forced delete of y was refused"
    );
    result += l_check(l_hook_size(o2, "kids") == 0, "O2's list hook keeps a ghost of x");
    result += l_check(l_hook_size(o2, "tags") == 0, "O2's dict hook keeps a ghost of y");

    result += l_check(
        treedb_delete_node(tranger, o2, json_pack("{s:b}", "force", 1)) == 0,
        "the forced delete of O2 was refused"
    );
    close_links_db(tranger);
    result += test_json(NULL);

    /*
     *  Reopen: x and y stay deleted
     */
    set_expected_results(test, NULL, NULL, NULL, 1);
    tranger = open_links_db(path_root, DB);
    result += l_check(l_node(tranger, L_KIDS, "x") == NULL, "x is back after the reopen");
    result += l_check(l_node(tranger, L_KIDS, "y") == NULL, "y is back after the reopen");
    result += l_check(l_node(tranger, L_PARENTS, "O2") == NULL, "O2 is back after the reopen");
    close_links_db(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  A deleted CHILD instance leaves the hooks of its parents.
 *
 *  x/v2 hangs from O through the list hook, y/v2 through the dict hook,
 *  and z/v2 from Q; the primaries hang from nothing. Once the three
 *  instances are deleted, O holds nothing (a delete of it without force
 *  goes), and the forced delete of Q saves nothing. 7.25.4 left the
 *  instances in the hooks: the delete of O was refused, and the forced
 *  delete of Q saved z/v2 back -- the newest row of z, its primary after
 *  the reopen.
 ***************************************************************************/
PRIVATE int test_deleted_instance_leaves_parents_hooks(void)
{
    int result = 0;
    const char *test = "a deleted child instance leaves the hooks of its parents";
    const char *DB = "tr_delete_instance_links3";
    char path_root[PATH_MAX];
    json_t *tranger = new_links_db(test, DB, path_root, sizeof(path_root), &result);

    set_expected_results(test, NULL, NULL, NULL, 1);

    l_create(tranger, L_PARENTS, "O", "1");
    l_create(tranger, L_PARENTS, "Q", "1");
    l_create(tranger, L_KIDS, "x", "v1");
    l_create(tranger, L_KIDS, "x", "v2");
    l_create(tranger, L_KIDS, "y", "v1");
    l_create(tranger, L_KIDS, "y", "v2");
    l_create(tranger, L_KIDS, "z", "v1");
    l_create(tranger, L_KIDS, "z", "v2");
    json_t *o = l_node(tranger, L_PARENTS, "O");
    json_t *q = l_node(tranger, L_PARENTS, "Q");
    result += l_check(
        treedb_link_nodes(tranger, "kids", o, l_instance(tranger, L_KIDS, "x", "v2")) == 0,
        "cannot link O <- x/v2"
    );
    result += l_check(
        treedb_link_nodes(tranger, "tags", o, l_instance(tranger, L_KIDS, "y", "v2")) == 0,
        "cannot link O <- y/v2"
    );
    result += l_check(
        treedb_link_nodes(tranger, "kids", q, l_instance(tranger, L_KIDS, "z", "v2")) == 0,
        "cannot link Q <- z/v2"
    );

    result += l_check(
        treedb_delete_instance(tranger, l_instance(tranger, L_KIDS, "x", "v2"), "version", NULL) == 0,
        "cannot delete the instance x/v2"
    );
    result += l_check(
        treedb_delete_instance(tranger, l_instance(tranger, L_KIDS, "y", "v2"), "version", NULL) == 0,
        "cannot delete the instance y/v2"
    );
    result += l_check(
        treedb_delete_instance(tranger, l_instance(tranger, L_KIDS, "z", "v2"), "version", NULL) == 0,
        "cannot delete the instance z/v2"
    );
    result += l_check(l_hook_size(o, "kids") == 0, "O's list hook keeps the deleted x/v2");
    result += l_check(l_hook_size(o, "tags") == 0, "O's dict hook keeps the deleted y/v2");
    result += l_check(l_hook_size(q, "kids") == 0, "Q's list hook keeps the deleted z/v2");

    result += l_check(
        treedb_delete_node(tranger, o, json_object()) == 0,
        "a delete WITHOUT force of O was refused, for instances that are gone"
    );
    result += l_check(
        treedb_delete_node(tranger, q, json_pack("{s:b}", "force", 1)) == 0,
        "the forced delete of Q was refused"
    );
    result += l_check(l_instance(tranger, L_KIDS, "z", "v2") == NULL, "the forced delete of Q brought z/v2 back");
    close_links_db(tranger);
    result += test_json(NULL);

    /*
     *  Reopen: the deleted instances stay deleted, the primaries stay
     */
    set_expected_results(test, NULL, NULL, NULL, 1);
    tranger = open_links_db(path_root, DB);
    const char *ids[] = {"x", "y", "z"};
    for(size_t i = 0; i < ARRAY_SIZE(ids); i++) {
        char what[80];
        snprintf(what, sizeof(what), "%s/v2 is back after the reopen", ids[i]);
        result += l_check(l_instance(tranger, L_KIDS, ids[i], "v2") == NULL, what);
        snprintf(what, sizeof(what), "the primary of %s is not v1 after the reopen", ids[i]);
        json_t *primary = l_node(tranger, L_KIDS, ids[i]);
        result += l_check(
            primary && strcmp(kw_get_str(0, primary, "version", "", 0), "v1") == 0, what
        );
    }
    result += l_check(l_node(tranger, L_PARENTS, "Q") == NULL, "Q is back after the reopen");
    close_links_db(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  The PRIMARY takes the place of a deleted instance, when it names that
 *  parent too.
 *
 *  x/v2 hangs from O's list hook first; then the primary x/v1 is linked to
 *  O as well: its fkey names O, but the hook keeps the entry it has (one
 *  per child id). Once x/v2 is deleted, x/v1 is what O holds -- what a
 *  reload says, since x/v1 names O on disk.
 ***************************************************************************/
PRIVATE int test_primary_takes_place_of_deleted_instance(void)
{
    int result = 0;
    const char *test = "the primary takes the place of a deleted instance";
    const char *DB = "tr_delete_instance_links4";
    char path_root[PATH_MAX];
    json_t *tranger = new_links_db(test, DB, path_root, sizeof(path_root), &result);

    set_expected_results(test,
        json_pack("[{s:s}]",
            "msg", "Child already in parent hook, skipping duplicate link"),
        NULL, NULL, 1);

    l_create(tranger, L_PARENTS, "O", "1");
    l_create(tranger, L_KIDS, "x", "v1");
    l_create(tranger, L_KIDS, "x", "v2");
    json_t *o = l_node(tranger, L_PARENTS, "O");
    json_t *x1 = l_instance(tranger, L_KIDS, "x", "v1");
    json_t *x2 = l_instance(tranger, L_KIDS, "x", "v2");
    result += l_check(l_node(tranger, L_KIDS, "x") == x1, "x/v1 is not the primary");
    result += l_check(treedb_link_nodes(tranger, "kids", o, x2) == 0, "cannot link O <- x/v2");
    result += l_check(treedb_link_nodes(tranger, "kids", o, x1) == 0, "cannot link O <- x/v1");
    result += l_check(l_hook_holds(o, "kids", x2) && !l_hook_holds(o, "kids", x1),
        "O's list hook does not hold x/v2 alone"
    );

    result += l_check(
        treedb_delete_instance(tranger, x2, "version", NULL) == 0,
        "cannot delete the instance x/v2"
    );
    result += l_check(
        l_hook_size(o, "kids") == 1 && l_hook_holds(o, "kids", l_node(tranger, L_KIDS, "x")),
        "the primary x/v1 did not take the place of x/v2 in O"
    );
    close_links_db(tranger);
    result += test_json(NULL);

    /*
     *  Reopen: O holds x, as memory said
     */
    set_expected_results(test, NULL, NULL, NULL, 1);
    tranger = open_links_db(path_root, DB);
    o = l_node(tranger, L_PARENTS, "O");
    result += l_check(
        l_hook_size(o, "kids") == 1 && l_hook_holds(o, "kids", l_node(tranger, L_KIDS, "x")),
        "after the reopen O does not hold x"
    );
    close_links_db(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  A deleted PARENT instance hands its children to the primary.
 *
 *  P/v1 is the primary; kid a hangs from P/v2 through the list hook, kid b
 *  through the dict hook. Their fkeys name P, the key, so a reload hangs
 *  them from P's primary. Once P/v2 is deleted, the primary holds them in
 *  memory too -- 7.25.4 left them under no visible parent until the
 *  reload, and a delete of P then went without force, leaving a and b
 *  naming a parent that is gone.
 ***************************************************************************/
PRIVATE int test_deleted_parent_instance_hands_children_to_primary(void)
{
    int result = 0;
    const char *test = "a deleted parent instance hands its children to the primary";
    const char *DB = "tr_delete_instance_links5";
    char path_root[PATH_MAX];
    json_t *tranger = new_links_db(test, DB, path_root, sizeof(path_root), &result);

    set_expected_results(test,
        json_pack("[{s:s}]",
            "msg", "Cannot delete node: has down links"),
        NULL, NULL, 1);

    l_create(tranger, L_PARENTS, "P", "v1");
    l_create(tranger, L_PARENTS, "P", "v2");
    l_create(tranger, L_KIDS, "a", "k1");
    l_create(tranger, L_KIDS, "b", "k1");
    json_t *p1 = l_instance(tranger, L_PARENTS, "P", "v1");
    json_t *p2 = l_instance(tranger, L_PARENTS, "P", "v2");
    json_t *a = l_node(tranger, L_KIDS, "a");
    json_t *b = l_node(tranger, L_KIDS, "b");
    result += l_check(l_node(tranger, L_PARENTS, "P") == p1, "P/v1 is not the primary");
    result += l_check(treedb_link_nodes(tranger, "kids", p2, a) == 0, "cannot link P/v2 <- a");
    result += l_check(treedb_link_nodes(tranger, "tags", p2, b) == 0, "cannot link P/v2 <- b");

    result += l_check(
        treedb_delete_instance(tranger, p2, "version", NULL) == 0,
        "cannot delete the instance P/v2"
    );
    result += l_check(l_hook_holds(p1, "kids", a), "the primary P/v1 did not take a from P/v2");
    result += l_check(l_hook_holds(p1, "tags", b), "the primary P/v1 did not take b from P/v2");
    result += l_check(
        treedb_delete_node(tranger, p1, json_object()) < 0,
        "a delete WITHOUT force of P went, with a and b naming it"
    );
    close_links_db(tranger);
    result += test_json(NULL);

    /*
     *  Reopen: the primary holds a and b, as memory said
     */
    set_expected_results(test, NULL, NULL, NULL, 1);
    tranger = open_links_db(path_root, DB);
    p1 = l_node(tranger, L_PARENTS, "P");
    result += l_check(l_instance(tranger, L_PARENTS, "P", "v2") == NULL, "P/v2 is back after the reopen");
    result += l_check(
        l_hook_size(p1, "kids") == 1 && l_hook_holds(p1, "kids", l_node(tranger, L_KIDS, "a")),
        "after the reopen P does not hold a"
    );
    result += l_check(
        l_hook_size(p1, "tags") == 1 && l_hook_holds(p1, "tags", l_node(tranger, L_KIDS, "b")),
        "after the reopen P does not hold b"
    );
    close_links_db(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  A forced delete that is refused puts back what it moved, in EVERY
 *  instance.
 *
 *  The key cannot be deleted (its directory is read-only). For the parent
 *  P, kid a of the instance P/v2 was unlinked and saved: it goes back into
 *  P/v2, its fkey names P again, on disk too. For the child x, the other
 *  instance x/v2 was taken out of O2's hooks (list and dict): it goes back,
 *  in its place.
 ***************************************************************************/
PRIVATE int chmod_links_key_dir(const char *db, const char *topic, const char *id, mode_t mode, mode_t *old)
{
    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir),
        getenv("HOME"), "tests_yuneta", db, topic, "keys", id, NULL);
    struct stat st;
    if(old) {
        if(stat(key_dir, &st) < 0) {
            printf("%s  FAIL: cannot stat %s%s\n", On_Red BWhite, key_dir, Color_Off);
            return -1;
        }
        *old = st.st_mode & 07777;
    }
    if(chmod(key_dir, mode) < 0) {
        printf("%s  FAIL: cannot chmod %s%s\n", On_Red BWhite, key_dir, Color_Off);
        return -1;
    }
    return 0;
}

PRIVATE int test_refused_forced_delete_puts_every_instance_back(void)
{
    int result = 0;
    const char *test = "a refused forced delete puts back every instance";
    const char *DB = "tr_delete_instance_links7";
    char path_root[PATH_MAX];
    json_t *tranger = new_links_db(test, DB, path_root, sizeof(path_root), &result);

    set_expected_results(test,
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "remove() FAILED",
            "msg", "Cannot delete subdir key. rmrdir() FAILED",
            "msg", "Cannot delete node",
            "msg", "remove() FAILED",
            "msg", "Cannot delete subdir key. rmrdir() FAILED",
            "msg", "Cannot delete node"),
        NULL, NULL, 1);

    l_create(tranger, L_PARENTS, "P", "v1");
    l_create(tranger, L_PARENTS, "P", "v2");
    l_create(tranger, L_PARENTS, "O1", "1");
    l_create(tranger, L_PARENTS, "O2", "1");
    l_create(tranger, L_KIDS, "a", "k1");
    l_create(tranger, L_KIDS, "k0", "k1");
    l_create(tranger, L_KIDS, "k2", "k1");
    l_create(tranger, L_KIDS, "x", "v1");
    l_create(tranger, L_KIDS, "x", "v2");
    json_t *p1 = l_instance(tranger, L_PARENTS, "P", "v1");
    json_t *p2 = l_instance(tranger, L_PARENTS, "P", "v2");
    json_t *o1 = l_node(tranger, L_PARENTS, "O1");
    json_t *o2 = l_node(tranger, L_PARENTS, "O2");
    json_t *a = l_node(tranger, L_KIDS, "a");
    json_t *x1 = l_instance(tranger, L_KIDS, "x", "v1");
    json_t *x2 = l_instance(tranger, L_KIDS, "x", "v2");
    result += l_check(treedb_link_nodes(tranger, "kids", p2, a) == 0, "cannot link P/v2 <- a");
    result += l_check(treedb_link_nodes(tranger, "kids", o1, x1) == 0, "cannot link O1 <- x/v1");
    result += l_check(
        treedb_link_nodes(tranger, "kids", o2, l_node(tranger, L_KIDS, "k0")) == 0,
        "cannot link O2 <- k0"
    );
    result += l_check(treedb_link_nodes(tranger, "kids", o2, x2) == 0, "cannot link O2 <- x/v2");
    result += l_check(
        treedb_link_nodes(tranger, "kids", o2, l_node(tranger, L_KIDS, "k2")) == 0,
        "cannot link O2 <- k2"
    );
    result += l_check(treedb_link_nodes(tranger, "tags", o2, x2) == 0, "cannot link O2 tags <- x/v2");

    /*
     *  The parent P: a is put back into P/v2
     */
    mode_t mode = 0;
    result += chmod_links_key_dir(DB, L_PARENTS, "P", 0550, &mode);
    result += l_check(
        treedb_delete_node(tranger, p1, json_pack("{s:b}", "force", 1)) < 0,
        "the forced delete of P went, with a key that cannot be deleted"
    );
    result += chmod_links_key_dir(DB, L_PARENTS, "P", mode, NULL);
    result += l_check(l_hook_holds(p2, "kids", a), "a is not back in P/v2");
    result += l_check(
        strcmp(kw_get_str(0, a, "parent", "", 0), "parents^P^kids") == 0,
        "the fkey of a does not name P again"
    );

    /*
     *  The child x: x/v1 back in O1, x/v2 back in O2, in its place
     */
    result += chmod_links_key_dir(DB, L_KIDS, "x", 0550, &mode);
    result += l_check(
        treedb_delete_node(tranger, x1, json_pack("{s:b}", "force", 1)) < 0,
        "the forced delete of x went, with a key that cannot be deleted"
    );
    result += chmod_links_key_dir(DB, L_KIDS, "x", mode, NULL);
    result += l_check(l_hook_holds(o1, "kids", x1), "x/v1 is not back in O1");
    json_t *o2_kids = json_object_get(o2, "kids");
    result += l_check(
        json_array_size(o2_kids) == 3 && json_array_get(o2_kids, 1) == x2,
        "x/v2 is not back in its place in O2's list hook"
    );
    result += l_check(l_hook_holds(o2, "tags", x2), "x/v2 is not back in O2's dict hook");
    close_links_db(tranger);
    result += test_json(NULL);

    /*
     *  Reopen: a still hangs from P (from its primary now)
     */
    set_expected_results(test, NULL, NULL, NULL, 1);
    tranger = open_links_db(path_root, DB);
    result += l_check(
        l_hook_holds(l_node(tranger, L_PARENTS, "P"), "kids", l_node(tranger, L_KIDS, "a")),
        "after the reopen P does not hold a"
    );
    close_links_db(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  A save of a node no index holds is refused.
 *
 *  A node whose key was deleted, or an instance that was deleted, is out
 *  of the indexes; a pointer to it kept by somebody (a hook, a caller)
 *  saved a record into the key, and brought it back at the next open
 *  (7.25.4). The save refuses it now, whatever kept the pointer.
 ***************************************************************************/
PRIVATE int test_save_of_unindexed_node_refused(void)
{
    int result = 0;
    const char *test = "a save of a node no index holds is refused";
    const char *DB = "tr_delete_instance_links6";
    char path_root[PATH_MAX];
    json_t *tranger = new_links_db(test, DB, path_root, sizeof(path_root), &result);

    set_expected_results(test,
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot save a node that no index holds: its record would bring back what was deleted",
            "msg", "Cannot save a node that no index holds: its record would bring back what was deleted"),
        NULL, NULL, 1);

    l_create(tranger, L_KIDS, "x", "v1");
    l_create(tranger, L_KIDS, "x", "v2");
    l_create(tranger, L_KIDS, "w", "v1");
    json_t *x2 = json_incref(l_instance(tranger, L_KIDS, "x", "v2"));
    json_t *w = json_incref(l_node(tranger, L_KIDS, "w"));

    result += l_check(
        treedb_delete_instance(tranger, x2, "version", NULL) == 0,
        "cannot delete the instance x/v2"
    );
    result += l_check(
        treedb_delete_node(tranger, w, NULL) == 0,
        "cannot delete w"
    );
    result += l_check(treedb_save_node(tranger, x2) < 0, "the deleted instance x/v2 was saved");
    result += l_check(treedb_save_node(tranger, w) < 0, "the deleted w was saved");
    result += l_check(
        treedb_save_node(tranger, l_node(tranger, L_KIDS, "x")) == 0,
        "the primary x/v1 cannot be saved"
    );
    JSON_DECREF(x2)
    JSON_DECREF(w)
    close_links_db(tranger);
    result += test_json(NULL);

    set_expected_results(test, NULL, NULL, NULL, 1);
    tranger = open_links_db(path_root, DB);
    result += l_check(l_instance(tranger, L_KIDS, "x", "v2") == NULL, "x/v2 is back after the reopen");
    result += l_check(l_node(tranger, L_KIDS, "w") == NULL, "w is back after the reopen");
    result += l_check(l_instance(tranger, L_KIDS, "x", "v1") != NULL, "x/v1 is gone after the reopen");
    close_links_db(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *              do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    /*------------------------------------*
     *  Open tranger as master
     *------------------------------------*/
    json_t *tranger;
    {
        const char *test = "open tranger";
        set_expected_results(
            test,
            json_pack("[{s:s}]", "msg", "Creating __timeranger2__.json"),
            NULL, NULL, 1
        );

        json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        );
        tranger = tranger2_startup(0, jn_tranger, 0);
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Open treedb with the schema
     *------------------------------------*/
    const char *treedb_name = "treedb_delete_instance";
    {
        const char *test = "open treedb";
        /*  treedb_open_db creates __snaps__ + __graphs__ + items + __assets__  */
        set_expected_results(
            test,
            json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic"
            ),
            NULL, NULL, 1
        );

        helper_quote2doublequote(schema_sample);
        json_t *jn_schema = legalstring2json(schema_sample, TRUE);
        if(!jn_schema) {
            printf("Can't decode schema_sample json\n");
            exit(-1);
        }

        if(!treedb_open_db(tranger, treedb_name, jn_schema, 0)) {
            result += -1;
        }
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Execute scenarios
     *------------------------------------*/
    result += test_delete_instance_drops_secondary_keeps_primary(tranger, treedb_name);
    result += test_instance_held_by_a_snap(tranger, treedb_name);
    result += test_guard_that_cannot_read_refuses(tranger, treedb_name);
    result += test_delete_node_clears_everything(tranger, treedb_name);

    /*------------------------------------*
     *  Shutdown
     *------------------------------------*/
    {
        const char *test = "close and shutdown";
        set_expected_results(test, NULL, NULL, NULL, 1);
        treedb_close_db(tranger, treedb_name);
        tranger2_shutdown(tranger);
        result += test_json(NULL);
    }

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

    gbmem_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(
        argc,
        argv,
        NULL, NULL, NULL, NULL, NULL, NULL
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    /*--------------------------------*
     *      Event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
        NULL,
        &yev_loop
    );

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    int result = do_test();
    result += test_durable_delete_across_reopen();
    result += test_delete_that_cannot_read_refuses();
    result += test_tombstone_that_fails_part_way();
    result += test_delete_node_sees_children_of_every_instance();
    result += test_delete_node_unhooks_every_instance();
    result += test_deleted_instance_leaves_parents_hooks();
    result += test_primary_takes_place_of_deleted_instance();
    result += test_deleted_parent_instance_hands_children_to_primary();
    result += test_refused_forced_delete_puts_every_instance_back();
    result += test_save_of_unindexed_node_refused();

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
    }
    return result < 0 ? -1 : 0;
}

/***************************************************************************
 *              Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop_reset_running(yev_loop);
    if(xtimes_once > 1) {
        exit(-1);
    }
}

PUBLIC void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);
}
