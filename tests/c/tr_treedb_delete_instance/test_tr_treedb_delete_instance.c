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

    int rc = treedb_delete_node(
        tranger,
        node,
        json_pack("{s:b}", "force", 1)
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
        /*  treedb_open_db creates __snaps__ + __graphs__ + items  */
        set_expected_results(
            test,
            json_pack("[{s:s}, {s:s}, {s:s}]",
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
