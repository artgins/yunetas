/****************************************************************************
 *          test_tr_treedb_immutable.c
 *
 *          Regression coverage for immutable (non-deletable) topics and
 *          records.
 *
 *          Immutability is metadata, NOT a data column:
 *            - a record carries the md2 system_flag bit sf_immutable_record,
 *              surfaced as __md_treedb__`immutable; treedb_delete_node() and
 *              treedb_delete_instance() refuse it and `force` does NOT
 *              override. The bit is inherited across updates and survives a
 *              reload.
 *            - a topic carries `system_topic` in topic_var.json;
 *              treedb_delete_topic() refuses it, but its records stay
 *              deletable.
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

#define APP "test_tr_treedb_immutable"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE    "tr_immutable"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************************
 *  An immutable record cannot be deleted, with or without `force`.
 ***************************************************************************/
PRIVATE int test_record_immutable_delete(json_t *tranger, const char *treedb_name)
{
    int result = 0;
    const char *test = "immutable record refuses delete (force too)";
    time_measure_t time_measure;
    set_expected_results(
        test,
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot delete node: immutable record",
            "msg", "Cannot delete node: immutable record"
        ),
        NULL, NULL, 1
    );
    MT_START_TIME(time_measure)

    treedb_create_node(
        tranger, treedb_name, "items",
        json_pack("{s:s, s:s, s:s}", "id", "item-1", "version", "v1", "payload", "old")
    );

    json_t *node = treedb_get_node(tranger, treedb_name, "items", "item-1");
    if(treedb_set_node_immutable(tranger, node, TRUE) != 0) {
        printf("%s  FAIL: set_node_immutable returned error%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!kw_get_bool(0, node, "__md_treedb__`immutable", 0, 0)) {
        printf("%s  FAIL: __md_treedb__`immutable not set in memory%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  delete without force → refused  */
    if(treedb_delete_node(tranger, node, NULL) == 0) {
        printf("%s  FAIL: delete_node succeeded on immutable record%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    /*  delete WITH force → still refused (stronger than the snap-tag guard)  */
    if(treedb_delete_node(tranger, node, json_pack("{s:b}", "force", 1)) == 0) {
        printf("%s  FAIL: delete_node(force) succeeded on immutable record%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    /*  still in the index  */
    if(treedb_get_node(tranger, treedb_name, "items", "item-1") == NULL) {
        printf("%s  FAIL: immutable item-1 vanished after refused delete%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  Immutability is inherited across an update (the re-appended record
 *  must keep the bit), so the node is still non-deletable after update.
 ***************************************************************************/
PRIVATE int test_immutable_survives_update(json_t *tranger, const char *treedb_name)
{
    int result = 0;
    const char *test = "immutable survives update";
    time_measure_t time_measure;
    set_expected_results(
        test,
        json_pack("[{s:s}]", "msg", "Cannot delete node: immutable record"),
        NULL, NULL, 1
    );
    MT_START_TIME(time_measure)

    json_t *node = treedb_get_node(tranger, treedb_name, "items", "item-1");
    treedb_update_node(tranger, node, json_pack("{s:s}", "payload", "new"), TRUE);

    if(!kw_get_bool(0, node, "__md_treedb__`immutable", 0, 0)) {
        printf("%s  FAIL: immutable lost in memory after update%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_delete_node(tranger, node, NULL) == 0) {
        printf("%s  FAIL: delete_node succeeded after update%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  treedb_delete_instance() also refuses an immutable record.
 ***************************************************************************/
PRIVATE int test_delete_instance_immutable(json_t *tranger, const char *treedb_name)
{
    int result = 0;
    const char *test = "immutable record refuses delete_instance";
    time_measure_t time_measure;
    set_expected_results(
        test,
        json_pack("[{s:s}]", "msg", "Cannot delete instance: immutable record"),
        NULL, NULL, 1
    );
    MT_START_TIME(time_measure)

    treedb_create_node(
        tranger, treedb_name, "items",
        json_pack("{s:s, s:s, s:s}", "id", "item-2", "version", "v2", "payload", "x")
    );
    json_t *node = treedb_get_node(tranger, treedb_name, "items", "item-2");
    treedb_set_node_immutable(tranger, node, TRUE);

    if(treedb_delete_instance(tranger, node, "version", NULL) == 0) {
        printf("%s  FAIL: delete_instance succeeded on immutable record%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_get_node(tranger, treedb_name, "items", "item-2") == NULL) {
        printf("%s  FAIL: immutable item-2 vanished after refused delete_instance%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  Control: a non-immutable record deletes normally.
 ***************************************************************************/
PRIVATE int test_control_record_deletes(json_t *tranger, const char *treedb_name)
{
    int result = 0;
    const char *test = "non-immutable record deletes ok";
    time_measure_t time_measure;
    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    treedb_create_node(
        tranger, treedb_name, "items",
        json_pack("{s:s, s:s, s:s}", "id", "item-free", "version", "vf", "payload", "y")
    );
    json_t *node = treedb_get_node(tranger, treedb_name, "items", "item-free");
    if(treedb_delete_node(tranger, node, NULL) != 0) {
        printf("%s  FAIL: delete_node failed on a normal record%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_get_node(tranger, treedb_name, "items", "item-free") != NULL) {
        printf("%s  FAIL: normal record still present after delete%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  A system_topic cannot be deleted; a normal topic can. The system
 *  topic's RECORDS stay deletable.
 ***************************************************************************/
PRIVATE int test_topic_protection(json_t *tranger, const char *treedb_name)
{
    int result = 0;
    const char *test = "system_topic refuses delete, records stay deletable";
    time_measure_t time_measure;
    set_expected_results(
        test,
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot delete topic: system topic",
            "msg", "Deleting topic"
        ),
        NULL, NULL, 1
    );
    MT_START_TIME(time_measure)

    /*  A record inside a system topic deletes normally  */
    treedb_create_node(
        tranger, treedb_name, "sys_items",
        json_pack("{s:s, s:s}", "id", "sys-1", "payload", "z")
    );
    json_t *sys_node = treedb_get_node(tranger, treedb_name, "sys_items", "sys-1");
    if(treedb_delete_node(tranger, sys_node, NULL) != 0) {
        printf("%s  FAIL: record of a system topic refused delete%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  The system topic itself cannot be deleted  */
    if(treedb_delete_topic(tranger, treedb_name, "sys_items") == 0) {
        printf("%s  FAIL: system topic was deleted%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_topic_size(tranger, treedb_name, "sys_items") < 0) {
        printf("%s  FAIL: system topic vanished after refused delete%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  Control: a normal topic deletes ("Deleting topic" log)  */
    if(treedb_delete_topic(tranger, treedb_name, "tmp_items") != 0) {
        printf("%s  FAIL: normal topic refused delete%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  Persistence: after a reload the immutable bit survives, so the node
 *  is still non-deletable.
 ***************************************************************************/
PRIVATE int test_persistence_after_reload(json_t *tranger, const char *treedb_name)
{
    int result = 0;
    const char *test = "immutable survives reload";
    time_measure_t time_measure;
    set_expected_results(
        test,
        json_pack("[{s:s}]", "msg", "Cannot delete node: immutable record"),
        NULL, NULL, 1
    );
    MT_START_TIME(time_measure)

    json_t *node = treedb_get_node(tranger, treedb_name, "items", "item-1");
    if(!node) {
        printf("%s  FAIL: item-1 missing after reload%s\n", On_Red BWhite, Color_Off);
        result += -1;
        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
        return result;
    }
    if(!kw_get_bool(0, node, "__md_treedb__`immutable", 0, 0)) {
        printf("%s  FAIL: immutable bit lost after reload%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(treedb_delete_node(tranger, node, NULL) == 0) {
        printf("%s  FAIL: delete_node succeeded after reload%s\n", On_Red BWhite, Color_Off);
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
     *  Open treedb (5 topics created:
     *  __snaps__ __graphs__ items sys_items tmp_items)
     *------------------------------------*/
    const char *treedb_name = "treedb_immutable";
    {
        const char *test = "open treedb";
        set_expected_results(
            test,
            json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s}]",
                "msg", "Creating topic",
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
     *  Scenarios
     *------------------------------------*/
    result += test_record_immutable_delete(tranger, treedb_name);
    result += test_immutable_survives_update(tranger, treedb_name);
    result += test_delete_instance_immutable(tranger, treedb_name);
    result += test_control_record_deletes(tranger, treedb_name);
    result += test_topic_protection(tranger, treedb_name);

    /*------------------------------------*
     *  Reload the treedb from disk
     *------------------------------------*/
    {
        const char *test = "reload treedb";
        /*  tmp_items was deleted in test_topic_protection, so the schema
            re-creates it on reopen (one "Creating topic").  */
        set_expected_results(
            test,
            json_pack("[{s:s}]", "msg", "Creating topic"),
            NULL, NULL, 1
        );
        treedb_close_db(tranger, treedb_name);
        json_t *jn_schema2 = legalstring2json(schema_sample, TRUE);
        if(!treedb_open_db(tranger, treedb_name, jn_schema2, 0)) {
            result += -1;
        }
        result += test_json(NULL);
    }

    result += test_persistence_after_reload(tranger, treedb_name);

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

    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    yuno_catch_signals();

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
