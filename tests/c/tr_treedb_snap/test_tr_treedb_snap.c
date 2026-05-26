/****************************************************************************
 *          test_tr_treedb_snap.c
 *
 *          Regression coverage for treedb_shoot_snap()
 *          and treedb_activate_snap().
 *
 *          The agent's upgrade flow stages new versions of `binaries`
 *          and `configurations` alongside existing primaries, then commits
 *          them atomically with `deactivate-snap` (= activate `__clear__`).
 *          Snaps (`shoot-snap` + `activate-snap`) freeze a primary set
 *          for rollback.
 *
 *          This test models the same pattern with a two-topic schema:
 *            - binaries  (pkey=id role,    pkey2s=version)
 *            - yunos     (pkey=id,         pkey2s=yuno_release)
 *
 *          and walks the full lifecycle:
 *            1. seed v1 → primaries = v1
 *            2. add v2 instances → primaries still v1 (no auto-promote)
 *            3. shoot snap_v1
 *            4. deactivate-snap + reload → primaries promoted to v2
 *            5. add v3 + deactivate-snap + reload → primaries = v3
 *            6. shoot snap_v3
 *            7. activate snap_v1 + reload → primaries back to v1
 *            8. activate snap_v3 + reload → primaries to v3
 *            9. deactivate-snap + reload → primaries still v3 (already latest)
 *
 *          The reload (treedb_close_db + treedb_open_db) is what
 *          c_agent's restart_nodes() does after toggling the snap flag.
 *          Without that reload the in-memory primary index does not
 *          change — by design, see YUNO_LIFECYCLE.md §6.5.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>

#include <gobj.h>
#include <timeranger2.h>
#include <tr_treedb.h>
#include <yev_loop.h>
#include <testing.h>
#include <helpers.h>

#include "schema_sample.c"

#define APP "test_tr_treedb_snap"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE        "tr_snap"
#define TREEDB_NAME     "treedb_snap"
#define TOPIC_BINARIES  "binaries"
#define TOPIC_YUNOS     "yunos"
#define BIN_VERSION_COL "version"
#define YUNO_VERSION_COL "yuno_release"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************************
 *  Re-open the treedb so the primary index is rebuilt from disk.
 *  c_agent.c::restart_nodes() does the same via gobj_stop/start of
 *  its priv->resource (c_node); that re-runs treedb_open_db and
 *  the loader picks the current active snap (or the latest pkey2
 *  when no snap is active) as primary.
 ***************************************************************************/
PRIVATE int reload_treedb(json_t *tranger, const char *treedb_name)
{
    treedb_close_db(tranger, treedb_name);

    helper_quote2doublequote(schema_sample);
    json_t *jn_schema = legalstring2json(schema_sample, TRUE);
    if(!jn_schema) {
        printf("%s  FAIL: cannot decode schema on reload%s\n",
            On_Red BWhite, Color_Off);
        return -1;
    }
    if(!treedb_open_db(tranger, treedb_name, jn_schema, 0)) {
        printf("%s  FAIL: cannot reopen treedb%s\n",
            On_Red BWhite, Color_Off);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Read the column value of the current primary for (topic, id).
 *  Returns "" if no primary, or static buffer with the value.
 ***************************************************************************/
PRIVATE const char *primary_version_col(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *id,
    const char *col
)
{
    json_t *node = treedb_get_node(tranger, treedb_name, topic_name, id);
    if(!node) {
        return "";
    }
    const char *v = kw_get_str(0, node, col, "", 0);
    return v?v:"";
}

/***************************************************************************
 *  Count the instances under (topic, id) on the given pkey2_name.
 ***************************************************************************/
PRIVATE int instance_count(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *pkey2_name,
    const char *id
)
{
    json_t *instances = treedb_list_instances(
        tranger, treedb_name, topic_name, pkey2_name,
        json_pack("{s:s}", "id", id),
        0
    );
    int n = (int)json_array_size(instances);
    JSON_DECREF(instances)
    return n;
}

/***************************************************************************
 *  Assert: primary version for (topic, id) on `col` equals expected.
 ***************************************************************************/
PRIVATE int assert_primary(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *id,
    const char *col,
    const char *expected,
    const char *phase
)
{
    const char *actual = primary_version_col(
        tranger, treedb_name, topic_name, id, col
    );
    if(strcmp(actual, expected) != 0) {
        printf("%s  FAIL [%s]: %s id=%s primary %s='%s', expected '%s'%s\n",
            On_Red BWhite, phase, topic_name, id, col, actual, expected,
            Color_Off);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Seed binary + yuno records for a given version.
 ***************************************************************************/
PRIVATE void seed_version(
    json_t *tranger,
    const char *treedb_name,
    const char *version
)
{
    treedb_create_node(
        tranger, treedb_name, TOPIC_BINARIES,
        json_pack("{s:s, s:s, s:i}",
            "id", "role_a",
            "version", version,
            "size", 1000
        )
    );
    treedb_create_node(
        tranger, treedb_name, TOPIC_BINARIES,
        json_pack("{s:s, s:s, s:i}",
            "id", "role_b",
            "version", version,
            "size", 2000
        )
    );
    treedb_create_node(
        tranger, treedb_name, TOPIC_YUNOS,
        json_pack("{s:s, s:s, s:s}",
            "id", "100",
            "yuno_role", "role_a",
            "yuno_release", version
        )
    );
    treedb_create_node(
        tranger, treedb_name, TOPIC_YUNOS,
        json_pack("{s:s, s:s, s:s}",
            "id", "200",
            "yuno_role", "role_b",
            "yuno_release", version
        )
    );
}

/***************************************************************************
 *  Verify the four primaries (binaries role_a/role_b + yunos 100/200)
 *  all sit on `version`.
 ***************************************************************************/
PRIVATE int assert_all_primaries(
    json_t *tranger,
    const char *treedb_name,
    const char *version,
    const char *phase
)
{
    int r = 0;
    r += assert_primary(tranger, treedb_name, TOPIC_BINARIES,
        "role_a", BIN_VERSION_COL, version, phase);
    r += assert_primary(tranger, treedb_name, TOPIC_BINARIES,
        "role_b", BIN_VERSION_COL, version, phase);
    r += assert_primary(tranger, treedb_name, TOPIC_YUNOS,
        "100", YUNO_VERSION_COL, version, phase);
    r += assert_primary(tranger, treedb_name, TOPIC_YUNOS,
        "200", YUNO_VERSION_COL, version, phase);
    return r;
}

/***************************************************************************
 *              Phase: seed v1, verify primaries
 ***************************************************************************/
PRIVATE int phase_seed_v1(json_t *tranger, const char *treedb_name)
{
    int result = 0;
    const char *test = "phase 1: seed v1, primaries=v1";
    time_measure_t time_measure;
    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    seed_version(tranger, treedb_name, "v1");

    result += assert_all_primaries(tranger, treedb_name, "v1", test);

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *              Phase: add v2, primary stays at v1 (no auto-promote)
 ***************************************************************************/
PRIVATE int phase_add_v2_no_promote(json_t *tranger, const char *treedb_name)
{
    int result = 0;
    const char *test = "phase 2: add v2 instances, primary stays v1";
    time_measure_t time_measure;
    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    seed_version(tranger, treedb_name, "v2");

    /*  Primary index untouched — still v1 in memory  */
    result += assert_all_primaries(tranger, treedb_name, "v1", test);

    /*  Instances list shows both v1 and v2  */
    if(instance_count(tranger, treedb_name, TOPIC_BINARIES,
            BIN_VERSION_COL, "role_a") != 2)
    {
        printf("%s  FAIL [%s]: role_a should have 2 instances%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }
    if(instance_count(tranger, treedb_name, TOPIC_YUNOS,
            YUNO_VERSION_COL, "100") != 2)
    {
        printf("%s  FAIL [%s]: yuno 100 should have 2 instances%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *              Phase: shoot snap_v1, then deactivate + reload → v2
 ***************************************************************************/
PRIVATE int phase_shoot_snap_v1_then_promote_v2(
    json_t *tranger, const char *treedb_name
)
{
    int result = 0;
    const char *test = "phase 3+4: shoot snap_v1, deactivate, reload → v2";
    time_measure_t time_measure;
    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    if(treedb_shoot_snap(tranger, treedb_name, "snap_v1", "v1 frozen") < 0) {
        printf("%s  FAIL [%s]: shoot_snap failed%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }

    /*  deactivate (activate "__clear__") drops the active flag  */
    if(treedb_activate_snap(tranger, treedb_name, "__clear__") < 0) {
        printf("%s  FAIL [%s]: deactivate-snap failed%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }

    /*  Reload — c_agent's restart_nodes does this. After reload the
        primary index is rebuilt; no active snap → newest pkey2 wins. */
    result += reload_treedb(tranger, treedb_name);

    result += assert_all_primaries(tranger, treedb_name, "v2", test);

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *              Phase: add v3, deactivate + reload → v3
 ***************************************************************************/
PRIVATE int phase_add_v3_promote(json_t *tranger, const char *treedb_name)
{
    int result = 0;
    const char *test = "phase 5: add v3, deactivate, reload → v3";
    time_measure_t time_measure;
    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    seed_version(tranger, treedb_name, "v3");

    /*  no active snap; deactivate is a no-op but the agent's flow
        calls it anyway for the side-effect reload  */
    treedb_activate_snap(tranger, treedb_name, "__clear__");
    result += reload_treedb(tranger, treedb_name);

    result += assert_all_primaries(tranger, treedb_name, "v3", test);

    if(instance_count(tranger, treedb_name, TOPIC_BINARIES,
            BIN_VERSION_COL, "role_a") != 3)
    {
        printf("%s  FAIL [%s]: role_a should have 3 instances%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *              Phase: shoot snap_v3, activate snap_v1, reload → v1
 ***************************************************************************/
PRIVATE int phase_rollback_to_snap_v1(
    json_t *tranger, const char *treedb_name
)
{
    int result = 0;
    const char *test = "phase 6+7: shoot snap_v3, activate snap_v1 → v1";
    time_measure_t time_measure;
    /*  treedb_open_db logs "loading snap_tag N" when reloading with
        a snap active (snap_v1 → tag id=1)  */
    set_expected_results(
        test,
        json_pack("[{s:s}]", "msg", "loading snap_tag 1"),
        NULL, NULL, 1
    );
    MT_START_TIME(time_measure)

    if(treedb_shoot_snap(tranger, treedb_name, "snap_v3", "v3 frozen") < 0) {
        printf("%s  FAIL [%s]: shoot snap_v3 failed%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }

    if(treedb_activate_snap(tranger, treedb_name, "snap_v1") < 0) {
        printf("%s  FAIL [%s]: activate snap_v1 failed%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }
    result += reload_treedb(tranger, treedb_name);

    result += assert_all_primaries(tranger, treedb_name, "v1", test);

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *              Phase: activate snap_v3, reload → v3
 ***************************************************************************/
PRIVATE int phase_jump_to_snap_v3(json_t *tranger, const char *treedb_name)
{
    int result = 0;
    const char *test = "phase 8: activate snap_v3 → v3";
    time_measure_t time_measure;
    /*  snap_v3 → tag id=2  */
    set_expected_results(
        test,
        json_pack("[{s:s}]", "msg", "loading snap_tag 2"),
        NULL, NULL, 1
    );
    MT_START_TIME(time_measure)

    if(treedb_activate_snap(tranger, treedb_name, "snap_v3") < 0) {
        printf("%s  FAIL [%s]: activate snap_v3 failed%s\n",
            On_Red BWhite, test, Color_Off);
        result += -1;
    }
    result += reload_treedb(tranger, treedb_name);

    result += assert_all_primaries(tranger, treedb_name, "v3", test);

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *              Phase: deactivate-snap from snap_v3 → still v3
 *              (no newer release exists)
 ***************************************************************************/
PRIVATE int phase_deactivate_stays_at_latest(
    json_t *tranger, const char *treedb_name
)
{
    int result = 0;
    const char *test = "phase 9: deactivate-snap → stays v3 (latest)";
    time_measure_t time_measure;
    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    treedb_activate_snap(tranger, treedb_name, "__clear__");
    result += reload_treedb(tranger, treedb_name);

    result += assert_all_primaries(tranger, treedb_name, "v3", test);

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
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
    {
        const char *test = "open treedb";
        /*  Creates __snaps__ + __graphs__ + binaries + yunos = 4 topics */
        set_expected_results(
            test,
            json_pack("[{s:s},{s:s},{s:s},{s:s}]",
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

        if(!treedb_open_db(tranger, TREEDB_NAME, jn_schema, 0)) {
            result += -1;
        }
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Execute phases
     *------------------------------------*/
    result += phase_seed_v1(tranger, TREEDB_NAME);
    result += phase_add_v2_no_promote(tranger, TREEDB_NAME);
    result += phase_shoot_snap_v1_then_promote_v2(tranger, TREEDB_NAME);
    result += phase_add_v3_promote(tranger, TREEDB_NAME);
    result += phase_rollback_to_snap_v1(tranger, TREEDB_NAME);
    result += phase_jump_to_snap_v3(tranger, TREEDB_NAME);
    result += phase_deactivate_stays_at_latest(tranger, TREEDB_NAME);

    /*------------------------------------*
     *  Shutdown
     *------------------------------------*/
    {
        const char *test = "close and shutdown";
        set_expected_results(test, NULL, NULL, NULL, 1);
        treedb_close_db(tranger, TREEDB_NAME);
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

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result < 0) {
        printf("%sTEST FAILED%s\n", On_Red BWhite, Color_Off);
    } else {
        printf("%sTEST PASSED%s\n", On_Green BWhite, Color_Off);
    }
    return result < 0 ? -1 : 0;
}

/***************************************************************************
 *      Signal handlers
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

PRIVATE void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);

    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
}
