/****************************************************************************
 *          test_tr_treedb_delete_instance.c
 *
 *          Regression coverage for treedb_delete_instance().
 *
 *          The function cleans only ONE secondary `pkey2` index slot:
 *            - the primary `id` index stays untouched,
 *            - the on-disk `.md2` row stays untouched (no
 *              tranger2_delete_key() / tranger2_delete_instance() call),
 *            - the other secondary indexes (when more than one is declared)
 *              stay untouched.
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

#include <gobj.h>
#include <timeranger2.h>
#include <tr_treedb.h>
#include <yev_loop.h>
#include <testing.h>
#include <helpers.h>

#include "schema_sample.c"

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
            "msg", "Cannot read record metadata, read FAILED",
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
 *  the next open (M1 of the 2026-09-23 independent review of 7.25.4). And a
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
            "msg", "Cannot read record metadata, read FAILED",
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
     *  The content of rel-2 cannot be read
     *------------------------------------*/
    set_expected_results(test,
        json_pack("[{s:s},{s:s}]",
            "msg", "Bad on-disk record: __offset__/__size__ out of range",
            "msg", "Cannot delete instance, a row of its key cannot be read"),
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
