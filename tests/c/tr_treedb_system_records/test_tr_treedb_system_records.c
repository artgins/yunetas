/****************************************************************************
 *          test_tr_treedb_system_records.c
 *
 *          Regression coverage for the treedb system-protection marks:
 *
 *            - record-level `__system__` field:
 *                * treedb_delete_node() refuses, force does NOT override;
 *                * treedb_delete_instance() refuses, force does NOT override;
 *                * treedb_update_node() cannot clear it once TRUE;
 *                * a refused delete leaves the node indexed (no ref leak);
 *            - topic-level `system_topic` flag (schema -> topic_var.json):
 *                * treedb_delete_topic() refuses;
 *            - both marks persist across a full close + reopen.
 *
 *          Only a full store wipe removes a system record/topic.
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

#define APP "test_tr_treedb_system_records"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE    "tr_system_records"
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
 *  treedb_delete_node() must refuse a __system__ record,
 *  with and without force; a plain record still deletes.
 ***************************************************************************/
PRIVATE int test_system_record_not_deletable(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "delete_node refuses __system__ record (force ignored)";
    time_measure_t time_measure;
    set_expected_results(test,
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot delete node: system record",
            "msg", "Cannot delete node: system record"
        ),
        NULL, NULL, 1);
    MT_START_TIME(time_measure)

    treedb_create_node(
        tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s, s:b}",
            "id", "seed-1",
            "version", "v1",
            "payload", "protected",
            "__system__", 1
        )
    );
    treedb_create_node(
        tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}",
            "id", "plain-1",
            "version", "v1",
            "payload", "deletable"
        )
    );

    json_t *seed = treedb_get_node(tranger, treedb_name, TOPIC_NAME, "seed-1");
    if(!seed) {
        printf("%s  FAIL: seed-1 missing after create%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!kw_get_bool(0, seed, "__system__", 0, 0)) {
        printf("%s  FAIL: seed-1 lost the __system__ mark on create%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  No force: refused  */
    if(treedb_delete_node(tranger, seed, NULL) != -1) {
        printf("%s  FAIL: delete_node accepted a system record%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  force=1: STILL refused (unlike the snap-tag guard)  */
    seed = treedb_get_node(tranger, treedb_name, TOPIC_NAME, "seed-1");
    if(treedb_delete_node(tranger, seed, json_pack("{s:b}", "force", 1)) != -1) {
        printf("%s  FAIL: delete_node force overrode the system guard%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  Still present and intact after both refusals  */
    seed = treedb_get_node(tranger, treedb_name, TOPIC_NAME, "seed-1");
    if(!seed) {
        printf("%s  FAIL: seed-1 vanished after refused deletes%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  Control: a plain record still deletes normally  */
    json_t *plain = treedb_get_node(tranger, treedb_name, TOPIC_NAME, "plain-1");
    if(treedb_delete_node(tranger, plain, json_pack("{s:b}", "force", 1)) != 0) {
        printf("%s  FAIL: delete_node refused a plain record%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_get_node(tranger, treedb_name, TOPIC_NAME, "plain-1") != NULL) {
        printf("%s  FAIL: plain-1 still present after delete%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  treedb_delete_instance() must refuse a __system__ instance,
 *  with and without force; a plain instance still deletes.
 ***************************************************************************/
PRIVATE int test_system_instance_not_deletable(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "delete_instance refuses __system__ record (force ignored)";
    time_measure_t time_measure;
    set_expected_results(test,
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot delete instance: system record",
            "msg", "Cannot delete instance: system record"
        ),
        NULL, NULL, 1);
    MT_START_TIME(time_measure)

    /*  Two versions of a system id: v2 (last) is primary, v1 is instance  */
    treedb_create_node(
        tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s, s:b}",
            "id", "seed-2", "version", "v1", "payload", "a", "__system__", 1
        )
    );
    treedb_create_node(
        tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s, s:b}",
            "id", "seed-2", "version", "v2", "payload", "b", "__system__", 1
        )
    );

    /*  Two versions of a plain id, control case  */
    treedb_create_node(
        tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}",
            "id", "plain-2", "version", "v1", "payload", "a"
        )
    );
    treedb_create_node(
        tranger, treedb_name, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}",
            "id", "plain-2", "version", "v2", "payload", "b"
        )
    );

    /*  No force: refused  */
    json_t *v1 = treedb_get_instance(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "seed-2", "v1"
    );
    if(treedb_delete_instance(tranger, v1, PKEY2_NAME, NULL) != -1) {
        printf("%s  FAIL: delete_instance accepted a system record%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  force=1: STILL refused  */
    v1 = treedb_get_instance(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "seed-2", "v1"
    );
    if(treedb_delete_instance(
            tranger, v1, PKEY2_NAME, json_pack("{s:b}", "force", 1)) != -1)
    {
        printf("%s  FAIL: delete_instance force overrode the system guard%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  Both versions still present after refusals  */
    if(treedb_get_instance(
            tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "seed-2", "v1") == NULL ||
       treedb_get_instance(
            tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "seed-2", "v2") == NULL)
    {
        printf("%s  FAIL: seed-2 instance vanished after refused deletes%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  Control: a plain non-primary instance still deletes  */
    json_t *pv1 = treedb_get_instance(
        tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "plain-2", "v1"
    );
    if(treedb_delete_instance(tranger, pv1, PKEY2_NAME, NULL) != 0) {
        printf("%s  FAIL: delete_instance refused a plain instance%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_get_instance(
            tranger, treedb_name, TOPIC_NAME, PKEY2_NAME, "plain-2", "v1") != NULL)
    {
        printf("%s  FAIL: plain-2 v1 still present after delete%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  treedb_update_node() must not clear __system__ once TRUE,
 *  while other fields keep updating normally.
 ***************************************************************************/
PRIVATE int test_system_mark_immutable(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "__system__ is immutable once set";
    time_measure_t time_measure;
    set_expected_results(test,
        json_pack("[{s:s}]",
            "msg", "__system__ is immutable once set, change ignored"
        ),
        NULL, NULL, 1);
    MT_START_TIME(time_measure)

    json_t *seed = treedb_get_node(tranger, treedb_name, TOPIC_NAME, "seed-1");
    treedb_update_node(
        tranger, seed,
        json_pack("{s:b, s:s}",
            "__system__", 0,
            "payload", "still-protected"
        ),
        TRUE
    );

    seed = treedb_get_node(tranger, treedb_name, TOPIC_NAME, "seed-1");
    if(!kw_get_bool(0, seed, "__system__", 0, 0)) {
        printf("%s  FAIL: update_node cleared __system__%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(strcmp(kw_get_str(0, seed, "payload", "", 0), "still-protected")!=0) {
        printf("%s  FAIL: regular field did not update%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  treedb_delete_topic() must refuse a system topic;
 *  a plain topic still deletes.
 ***************************************************************************/
PRIVATE int test_system_topic_not_deletable(
    json_t *tranger,
    const char *treedb_name
)
{
    int result = 0;
    const char *test = "delete_topic refuses system topic";
    time_measure_t time_measure;
    set_expected_results(test,
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot delete topic: system topic",
            "msg", "Deleting topic"   /* control: scratch removed */
        ),
        NULL, NULL, 1);
    MT_START_TIME(time_measure)

    if(treedb_delete_topic(tranger, treedb_name, TOPIC_NAME) != -1) {
        printf("%s  FAIL: delete_topic accepted a system topic%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_get_node(tranger, treedb_name, TOPIC_NAME, "seed-1") == NULL) {
        printf("%s  FAIL: system topic content lost after refused delete%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  Control: a plain topic still deletes  */
    if(treedb_delete_topic(tranger, treedb_name, "scratch") != 0) {
        printf("%s  FAIL: delete_topic refused a plain topic%s\n",
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

    const char *treedb_name = "treedb_system_records";

    /*------------------------------------*
     *  Phase 1: create, exercise guards
     *------------------------------------*/
    json_t *tranger;
    {
        const char *test = "open tranger + treedb";
        /*  treedb_open_db creates __snaps__ + __graphs__ + items + scratch  */
        set_expected_results(
            test,
            json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
                "msg", "Creating __timeranger2__.json",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic"
            ),
            NULL, NULL, 1
        );

        json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        );
        tranger = tranger2_startup(0, jn_tranger, 0);

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

    result += test_system_record_not_deletable(tranger, treedb_name);
    result += test_system_instance_not_deletable(tranger, treedb_name);
    result += test_system_mark_immutable(tranger, treedb_name);
    result += test_system_topic_not_deletable(tranger, treedb_name);

    {
        const char *test = "close db (phase 1)";
        set_expected_results(test, NULL, NULL, NULL, 1);
        treedb_close_db(tranger, treedb_name);
        tranger2_shutdown(tranger);
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Phase 2: reopen, both marks must
     *  persist (topic_var + record field)
     *------------------------------------*/
    {
        const char *test = "system marks persist across reopen";
        set_expected_results(test,
            json_pack("[{s:s},{s:s},{s:s}]",
                "msg", "Creating topic",  /* scratch re-created from schema */
                "msg", "Cannot delete node: system record",
                "msg", "Cannot delete topic: system topic"
            ),
            NULL, NULL, 1);

        json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        );
        tranger = tranger2_startup(0, jn_tranger, 0);

        json_t *jn_schema = legalstring2json(schema_sample, TRUE);
        if(!treedb_open_db(tranger, treedb_name, jn_schema, 0)) {
            result += -1;
        }

        json_t *seed = treedb_get_node(tranger, treedb_name, TOPIC_NAME, "seed-1");
        if(!seed) {
            printf("%s  FAIL: seed-1 missing after reopen%s\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }
        if(!kw_get_bool(0, seed, "__system__", 0, 0)) {
            printf("%s  FAIL: __system__ mark lost across reopen%s\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }
        if(treedb_delete_node(tranger, seed, json_pack("{s:b}", "force", 1)) != -1) {
            printf("%s  FAIL: delete_node accepted system record after reopen%s\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }
        if(treedb_delete_topic(tranger, treedb_name, TOPIC_NAME) != -1) {
            printf("%s  FAIL: delete_topic accepted system topic after reopen%s\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }

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
