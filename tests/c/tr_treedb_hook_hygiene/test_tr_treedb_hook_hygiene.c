/****************************************************************************
 *          test_tr_treedb_hook_hygiene.c
 *
 *          Regression coverage for the versioned-parent reverse-hook fixes
 *          (commit "fix(treedb): version-aware reverse-hook unlink +
 *          idempotent link dedup").
 *
 *          Two quirks, both around a versioned (pkey2) parent with a hook:
 *
 *            1. Duplicate hook entries. Linking the same child id twice used
 *               to append it twice to the parent hook (and twice to the child
 *               fkey). Link is now idempotent and warns on the skipped dup.
 *
 *            2. Unlink targeted only the primary parent version. A child's
 *               fkey ref carries the parent id but not the version, so cleaning
 *               a child hooked on a NON-primary config version used to unlink
 *               the primary (where the child isn't), leaving a stale entry on
 *               the real version. Clean now locates the holding version.
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

#define APP "test_tr_treedb_hook_hygiene"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE        "tr_hook_hygiene"
#define PKEY2_NAME      "version"
#define HOOK_NAME       "yunos"
#define CONFIG_FKEY     "config"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************************
 *  Helper: size of a node's array field (hook or fkey). -1 if absent.
 ***************************************************************************/
PRIVATE int array_field_size(json_t *node, const char *field)
{
    json_t *arr = json_object_get(node, field);
    if(!arr || !json_is_array(arr)) {
        return -1;
    }
    return (int)json_array_size(arr);
}

/***************************************************************************
 *  Quirk 2: linking the same child twice is idempotent and warns.
 ***************************************************************************/
PRIVATE int test_idempotent_link_dedup(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    time_measure_t time_measure;

    /*------------------------------------*
     *  Seed: one config (v1) + one yuno,
     *  link them once. No logs expected.
     *------------------------------------*/
    {
        const char *test = "dedup: seed + first link (clean)";
        set_expected_results(test, NULL, NULL, NULL, 0);
        MT_START_TIME(time_measure)

        treedb_create_node(tranger, treedb_name, "configs",
            json_pack("{s:s, s:s}", "id", "cfg-A", "version", "v1"));
        treedb_create_node(tranger, treedb_name, "yunos",
            json_pack("{s:s}", "id", "y-A"));

        json_t *cfg = treedb_get_node(tranger, treedb_name, "configs", "cfg-A");
        json_t *yuno = treedb_get_node(tranger, treedb_name, "yunos", "y-A");

        if(treedb_link_nodes(tranger, HOOK_NAME, cfg, yuno) != 0) {
            printf("%s  FAIL: first link returned non-zero%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        if(array_field_size(cfg, HOOK_NAME) != 1) {
            printf("%s  FAIL: parent hook size != 1 after first link (got %d)%s\n",
                On_Red BWhite, array_field_size(cfg, HOOK_NAME), Color_Off);
            result += -1;
        }
        if(array_field_size(yuno, CONFIG_FKEY) != 1) {
            printf("%s  FAIL: child fkey size != 1 after first link (got %d)%s\n",
                On_Red BWhite, array_field_size(yuno, CONFIG_FKEY), Color_Off);
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Re-link the SAME pair: must be a
     *  no-op that warns twice (parent hook
     *  + child fkey), in that order.
     *------------------------------------*/
    {
        const char *test = "dedup: duplicate link warns, stays single";
        set_expected_results(test,
            json_pack("[{s:s},{s:s}]",
                "msg", "Child already in parent hook, skipping duplicate link",
                "msg", "Parent ref already in child fkey, skipping duplicate"
            ),
            NULL, NULL, 0);
        MT_START_TIME(time_measure)

        json_t *cfg = treedb_get_node(tranger, treedb_name, "configs", "cfg-A");
        json_t *yuno = treedb_get_node(tranger, treedb_name, "yunos", "y-A");

        if(treedb_link_nodes(tranger, HOOK_NAME, cfg, yuno) != 0) {
            printf("%s  FAIL: duplicate link returned non-zero%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        if(array_field_size(cfg, HOOK_NAME) != 1) {
            printf("%s  FAIL: parent hook duplicated (size %d, want 1)%s\n",
                On_Red BWhite, array_field_size(cfg, HOOK_NAME), Color_Off);
            result += -1;
        }
        if(array_field_size(yuno, CONFIG_FKEY) != 1) {
            printf("%s  FAIL: child fkey duplicated (size %d, want 1)%s\n",
                On_Red BWhite, array_field_size(yuno, CONFIG_FKEY), Color_Off);
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    return result;
}

/***************************************************************************
 *  Quirk 1: clean a child hooked on a NON-primary parent version must
 *  unlink the real version, not the primary, leaving no stale entry and
 *  logging no "Child data not found" error.
 ***************************************************************************/
PRIVATE int test_clean_unlinks_nonprimary_version(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    time_measure_t time_measure;

    /*------------------------------------*
     *  Seed: config cfg-B with two
     *  versions + one yuno. Link the yuno
     *  to the NON-primary version.
     *------------------------------------*/
    json_t *target = NULL;       // the non-primary instance we hook onto
    const char *target_ver = NULL;
    json_t *other = NULL;        // the primary instance (must stay empty)
    {
        const char *test = "nonprimary: seed + link to non-primary version";
        set_expected_results(test, NULL, NULL, NULL, 0);
        MT_START_TIME(time_measure)

        treedb_create_node(tranger, treedb_name, "configs",
            json_pack("{s:s, s:s}", "id", "cfg-B", "version", "v1"));
        treedb_create_node(tranger, treedb_name, "configs",
            json_pack("{s:s, s:s}", "id", "cfg-B", "version", "v2"));
        treedb_create_node(tranger, treedb_name, "yunos",
            json_pack("{s:s}", "id", "y-B"));

        json_t *v1 = treedb_get_instance(tranger, treedb_name, "configs", PKEY2_NAME, "cfg-B", "v1");
        json_t *v2 = treedb_get_instance(tranger, treedb_name, "configs", PKEY2_NAME, "cfg-B", "v2");
        json_t *primary = treedb_get_node(tranger, treedb_name, "configs", "cfg-B");

        /*  Pick whichever instance is NOT the in-memory primary  */
        if(primary == v2) {
            target = v1; target_ver = "v1"; other = v2;
        } else {
            target = v2; target_ver = "v2"; other = v1;
        }
        if(!target || target == primary) {
            printf("%s  FAIL: could not pick a non-primary version (precondition)%s\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }

        json_t *yuno = treedb_get_node(tranger, treedb_name, "yunos", "y-B");
        if(treedb_link_nodes(tranger, HOOK_NAME, target, yuno) != 0) {
            printf("%s  FAIL: link to non-primary version returned non-zero%s\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }

        /*  The child is on the non-primary version only  */
        if(array_field_size(target, HOOK_NAME) != 1) {
            printf("%s  FAIL: non-primary (%s) hook size != 1 after link (got %d)%s\n",
                On_Red BWhite, target_ver, array_field_size(target, HOOK_NAME), Color_Off);
            result += -1;
        }
        if(array_field_size(other, HOOK_NAME) != 0) {
            printf("%s  FAIL: primary hook should be empty, got %d%s\n",
                On_Red BWhite, array_field_size(other, HOOK_NAME), Color_Off);
            result += -1;
        }
        if(array_field_size(yuno, CONFIG_FKEY) != 1) {
            printf("%s  FAIL: child fkey size != 1 after link (got %d)%s\n",
                On_Red BWhite, array_field_size(yuno, CONFIG_FKEY), Color_Off);
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Clean the child: must unlink the
     *  NON-primary version cleanly, with
     *  no "Child data not found" error.
     *------------------------------------*/
    {
        const char *test = "nonprimary: clean unlinks the holding version";
        set_expected_results(test, NULL, NULL, NULL, 0);
        MT_START_TIME(time_measure)

        json_t *yuno = treedb_get_node(tranger, treedb_name, "yunos", "y-B");
        if(treedb_clean_node(tranger, yuno, TRUE) != 0) {
            printf("%s  FAIL: clean_node returned non-zero%s\n", On_Red BWhite, Color_Off);
            result += -1;
        }

        /*  Re-fetch the instance (clean may have re-saved nodes)  */
        json_t *target_after = treedb_get_instance(
            tranger, treedb_name, "configs", PKEY2_NAME, "cfg-B", target_ver);
        if(array_field_size(target_after, HOOK_NAME) != 0) {
            printf("%s  FAIL: stale entry left on non-primary (%s) hook (size %d)%s\n",
                On_Red BWhite, target_ver, array_field_size(target_after, HOOK_NAME), Color_Off);
            result += -1;
        }
        if(array_field_size(yuno, CONFIG_FKEY) != 0) {
            printf("%s  FAIL: child fkey not cleared by clean (size %d)%s\n",
                On_Red BWhite, array_field_size(yuno, CONFIG_FKEY), Color_Off);
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

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
     *  (2 user topics + __snaps__ + __graphs__ = 4 "Creating topic")
     *------------------------------------*/
    const char *treedb_name = "treedb_hook_hygiene";
    {
        const char *test = "open treedb";
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
    result += test_idempotent_link_dedup(tranger, treedb_name);
    result += test_clean_unlinks_nonprimary_version(tranger, treedb_name);

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
