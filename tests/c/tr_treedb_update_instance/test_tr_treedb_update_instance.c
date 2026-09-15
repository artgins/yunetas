/****************************************************************************
 *          test_tr_treedb_update_instance.c
 *
 *          Regression coverage for the pkey2 secondary-index staleness bug.
 *
 *          treedb_update_node() mutates the primary node object in place and
 *          appends a new tranger row, but the secondary `pkey2` index used to
 *          keep a SEPARATE object that was only populated while loading from
 *          disk. So at runtime an update was invisible through the secondary
 *          index: treedb_get_instance() / treedb_list_instances() returned the
 *          previous content (the symptom: the agent's `list-binaries` showing
 *          the old binary right after a successful `update-binary`).
 *
 *          This test creates a node, updates a field, and asserts the change
 *          is visible BOTH by the secondary-index lookups. It fails against
 *          the pre-fix treedb (secondary index keeps the old value).
 *
 *          Then an update that CHANGES the pkey2 value (to another value,
 *          and to "") must be refused and leave the node, the instances
 *          and the disk as they were.
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

#define APP "test_tr_treedb_update_instance"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE    "tr_update_instance"
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
 *  A runtime update must surface through the pkey2 secondary index:
 *      - treedb_get_instance() returns the new value
 *      - treedb_list_instances() returns the new value
 *      - primary and pkey2 lookups still return the SAME node pointer
 ***************************************************************************/
PRIVATE int test_update_refreshes_secondary_index(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "update refreshes pkey2 secondary index";
    time_measure_t time_measure;
    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    /*------------------------------------*
     *  The node was created and the treedb
     *  was reloaded from disk by the caller,
     *  so the primary and pkey2 indexes now
     *  hold SEPARATE objects (the condition
     *  that triggered the staleness bug).
     *------------------------------------*
     *  Sanity: visible as "old" by pkey2
     *------------------------------------*/
    json_t *inst = treedb_get_instance(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "item-1", "v1"
    );
    if(!inst || strcmp(kw_get_str(0, inst, "payload", "", 0), "old") != 0) {
        printf("%s  FAIL: pkey2 instance not 'old' before update%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*------------------------------------*
     *  Update the node to payload "new"
     *------------------------------------*/
    json_t *node = treedb_get_node(tranger, treedb_name, TOPIC_NAME, "item-1");
    treedb_update_node(
        tranger,
        node,
        json_pack("{s:s}", "payload", "new"),
        TRUE  // save
    );

    /*------------------------------------*
     *  THE regression check:
     *  the pkey2 secondary index must now
     *  reflect "new", not the stale "old"
     *------------------------------------*/
    json_t *inst2 = treedb_get_instance(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "item-1", "v1"
    );
    const char *payload2 = inst2? kw_get_str(0, inst2, "payload", "", 0) : "";
    if(strcmp(payload2, "new") != 0) {
        printf("%s  FAIL: pkey2 instance shows '%s' after update, expected 'new'%s\n",
            On_Red BWhite, payload2, Color_Off);
        result += -1;
    }

    /*  Same node object shared by both indexes  */
    if(inst2 != node) {
        printf("%s  FAIL: primary and pkey2 lookups return different pointers%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*------------------------------------*
     *  Same through treedb_list_instances
     *  (the path the agent's list-binaries
     *  uses via gobj_list_instances)
     *------------------------------------*/
    json_t *list = treedb_list_instances(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME,
        json_pack("{s:s}", "id", "item-1"),
        NULL
    );
    BOOL found_new = FALSE;
    int idx; json_t *row;
    json_array_foreach(list, idx, row) {
        if(strcmp(kw_get_str(0, row, "version", "", 0), "v1") == 0 &&
           strcmp(kw_get_str(0, row, "payload", "", 0), "new") == 0) {
            found_new = TRUE;
        }
    }
    JSON_DECREF(list)
    if(!found_new) {
        printf("%s  FAIL: list_instances did not surface the updated payload%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  A pkey2 value names an instance, so an update cannot change it.
 *  It used to be applied in place: on disk the new value became a second
 *  instance, while in memory BOTH slots held the live node (listed twice
 *  with the new content), and a delete of the old instance tombstoned the
 *  rows of the new one. An empty value is the way in from the wire:
 *  `required` only asks for the key, and C_NODE's lookup skips an empty
 *  pkey2, so `update-node id=X version=""` reached the primary.
 ***************************************************************************/
PRIVATE int test_update_cannot_change_pkey2(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "update cannot change a pkey2 value";
    time_measure_t time_measure;
    set_expected_results(
        test,
        json_pack("[{s:s}, {s:s}]",
            "msg", "An update cannot change a pkey2 value, create the instance",
            "msg", "An update cannot change a pkey2 value, create the instance"
        ),
        NULL, NULL, 1
    );
    MT_START_TIME(time_measure)

    json_t *node = treedb_get_node(tranger, treedb_name, TOPIC_NAME, "item-1");
    json_int_t g_rowid = kw_get_int(0, node, "__md_treedb__`g_rowid", 0, 0);

    const char *new_values[] = {"v2", ""};
    for(size_t i=0; i<sizeof(new_values)/sizeof(new_values[0]); i++) {
        json_t *ret = treedb_update_node(
            tranger,
            node,
            json_pack("{s:s, s:s}", "version", new_values[i], "payload", "moved"),
            TRUE  // save
        );
        if(ret) {
            printf("%s  FAIL: update to version '%s' was accepted%s\n",
                On_Red BWhite, new_values[i], Color_Off);
            result += -1;
        }
    }

    if(strcmp(kw_get_str(0, node, "version", "", 0), "v1") != 0 ||
       strcmp(kw_get_str(0, node, "payload", "", 0), "new") != 0) {
        printf("%s  FAIL: a refused update touched the node: version '%s', payload '%s'%s\n",
            On_Red BWhite,
            kw_get_str(0, node, "version", "", 0),
            kw_get_str(0, node, "payload", "", 0),
            Color_Off);
        result += -1;
    }
    if(kw_get_int(0, node, "__md_treedb__`g_rowid", 0, 0) != g_rowid) {
        printf("%s  FAIL: a refused update appended a record%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    json_t *list = treedb_list_instances(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME,
        json_pack("{s:s}", "id", "item-1"),
        NULL
    );
    if(json_array_size(list) != 1) {
        printf("%s  FAIL: %d instances after the refused updates, expected 1%s\n",
            On_Red BWhite, (int)json_array_size(list), Color_Off);
        result += -1;
    }
    JSON_DECREF(list)

    /*  Carrying the SAME value is an ordinary update (C_NODE sends it)  */
    if(!treedb_update_node(
        tranger,
        node,
        json_pack("{s:s, s:s}", "version", "v1", "payload", "same-version"),
        TRUE
    )) {
        printf("%s  FAIL: an update carrying the same pkey2 value was refused%s\n",
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
    const char *treedb_name = "treedb_update_instance";
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
     *  Seed a node and persist it
     *------------------------------------*/
    {
        const char *test = "seed node";
        set_expected_results(test, NULL, NULL, NULL, 1);
        treedb_create_node(
            tranger, treedb_name, TOPIC_NAME,
            json_pack("{s:s, s:s, s:s}",
                "id", "item-1",
                "version", "v1",
                "payload", "old"
            )
        );
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Reload the treedb from disk so the
     *  primary and pkey2 indexes are rebuilt
     *  as SEPARATE objects (the load callbacks
     *  run), reproducing the agent's startup
     *  state where the staleness bug appears.
     *------------------------------------*/
    {
        const char *test = "reload treedb";
        set_expected_results(test, NULL, NULL, NULL, 1);
        treedb_close_db(tranger, treedb_name);
        json_t *jn_schema2 = legalstring2json(schema_sample, TRUE);
        if(!treedb_open_db(tranger, treedb_name, jn_schema2, 0)) {
            result += -1;
        }
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Execute scenario
     *------------------------------------*/
    result += test_update_refreshes_secondary_index(tranger, treedb_name);
    result += test_update_cannot_change_pkey2(tranger, treedb_name);

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
