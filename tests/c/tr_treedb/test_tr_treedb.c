/****************************************************************************
 *          test.c
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <signal.h>

#include <gobj.h>
#include <timeranger2.h>
#include <stacktrace_with_bfd.h>
#include <yunetas_ev_loop.h>
#include <kwid.h>
#include <testing.h>

#include "schema_sample.c"
#include "test_tr_treedb.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/
#define DATABASE    "tr_treedb"


/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE yev_loop_t *yev_loop;
PRIVATE int global_result = 0;

PRIVATE int print_tranger = 0;
PRIVATE int print_treedb = 0;
PRIVATE int without_ok_tests = 0;
PRIVATE int without_bad_tests = 0;
PRIVATE int show_oks = 1;
PRIVATE int verbose = 1;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int test_treedb_schema(
    json_t *tranger,
    json_t *topic_cols_desc
)
{
    int result = 0;
    const char *test;
    hgobj gobj = 0;
    time_measure_t time_measure;

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        test = "good_desc_01";
        json_t *topic = tranger2_create_topic(
            tranger,
            test,
            "id",
            "",
            NULL,
            sf_string_key,
            json_pack("[{s:s, s:s, s:i, s:s, s:s, s:i}]",
                "id", "id",
                "header", "Id",
                "fillspace", 20,
                "type", "integer",
                "flag", "required",
                "default", 0
            ),
            0
        );
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error_list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        MT_START_TIME(time_measure)

        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list(gobj, topic, KW_VERBOSE, "cols")
        );
        tranger2_delete_topic(tranger, test);

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)

        result += test_json(NULL, result);  // NULL: we want to check only the logs

    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        test = "good_desc_02";
        json_t *topic = tranger2_create_topic(
            tranger,
            test,
            "id",
            "",
            NULL,
            sf_string_key,
            json_pack("[{s:s, s:s, s:i, s:[s], s:[s,s], s:s}]",
                "id", "name",
                "header", "Name",
                "fillspace", 20,
                "type", "string",
                "flag",
                    "persistent", "required",
                "default", ""
            ),
            0
        );
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error_list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        MT_START_TIME(time_measure)

        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list(gobj, topic, KW_VERBOSE, "cols")
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL, result);  // NULL: we want to check only the logs

        tranger2_delete_topic(tranger, test);
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        test = "good_desc_03";
        json_t *topic = tranger2_create_topic(
            tranger,
            test,
            "id",
            "",
            NULL,
            sf_string_key,
            json_pack("[{s:s, s:s, s:i, s:[s], s:[s,s], s:s}]",
                "id", "name",
                "header", "Name",
                "fillspace", 20,
                "type", "string",
                "flag",
                    "persistent", "required",
                "default",
                    "Pepe"
            ),
            0
        );
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error_list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        MT_START_TIME(time_measure)

        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list(gobj, topic, KW_VERBOSE, "cols")
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL, result);  // NULL: we want to check only the logs

        tranger2_delete_topic(tranger, test);
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_bad_tests) {
        test = "bad_desc_01";
        json_t *topic = tranger2_create_topic(
            tranger,
            test,
            "id",
            "",
            NULL,
            sf_string_key,
            json_pack("[{s:b, s:[s], s:i, s:s, s:s}]",
                "id", 1,
                "header", "Xx",
                "fillspace", 20,
                "type", "xinteger",
                "flag", "xrequired"
            ),
            0
        );
        set_expected_results( // Check that no logs happen
            test,   // test name
            json_pack("[{s:s, s:s}, {s:s, s:s}, {s:s, s:s}, {s:s, s:s}]",  // error_list
                "msg", "Wrong basic type",
                "field", "id",
                "msg", "Wrong basic type",
                "field", "header",
                "msg", "Wrong enum type",
                "field", "type",
                "msg", "Wrong enum type",
                "field", "flag"
            ),
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        MT_START_TIME(time_measure)

        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list(gobj, topic, KW_VERBOSE, "cols")
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL, result);  // NULL: we want to check only the logs

        tranger2_delete_topic(tranger, test);
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_bad_tests) {
        test = "bad_desc_02";
        json_t *topic = tranger2_create_topic(
            tranger,
            test,
            "id",
            "",
            NULL,
            sf_string_key,
            json_pack("[{s:b, s:[s], s:i, s:s, s:s, s:[s]}]",
                "id", 1,
                "header", "Xx",
                "fillspace", 20,
                "type", "xinteger",
                "flag", "xrequired",
                "default", "pepe"
            ),
            0
        );
        set_expected_results( // Check that no logs happen
            test,   // test name
            json_pack("[{s:s, s:s}, {s:s, s:s}, {s:s, s:s}, {s:s, s:s}]",  // error_list
                "msg", "Wrong basic type",
                "field", "id",
                "msg", "Wrong basic type",
                "field", "header",
                "msg", "Wrong enum type",
                "field", "type",
                "msg", "Wrong enum type",
                "field", "flag"
            ),
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        MT_START_TIME(time_measure)

        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list(gobj, topic, KW_VERBOSE, "cols")
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL, result);  // NULL: we want to check only the logs

        tranger2_delete_topic(tranger, test);
    }

    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int test_schema(
    json_t *tranger,
    json_t *topic_cols_desc,
    const char *treedb_name,
    int verbose
)
{
    int result = 0;
    hgobj gobj = 0;
    const char *test = "test_schema";

    const char *topic_name; json_t *topic;
    json_object_foreach(kw_get_dict(gobj, tranger, "topics", 0, KW_REQUIRED), topic_name, topic)
    {
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error_list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list(gobj, topic, KW_VERBOSE, "cols")
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL, result);  // NULL: we want to check only the logs
    }

    return result;
}

/***************************************************************************
 *              Test
 *  Open as master, open iterator (realtime by disk) with callback
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;

    /*
     *  Write the tests in ~/tests_yuneta/
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    /*-------------------------------------------------*
     *      Startup the timeranger db
     *-------------------------------------------------*/
    json_t *tranger;
    if(1) {
        const char *test = "open tranger";

        set_expected_results( // Check that no logs happen
            test,   // test name
            json_pack("[{s:s}]",  // error_list
                "msg", "Creating __timeranger2__.json"
            ),
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        json_t *jn_tranger = json_pack("{s:s, s:s, s:b}",
            "path", path_root,
            "database", DATABASE,
            "master", 1
        );
        tranger = tranger2_startup(0, jn_tranger, 0);

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL, result);  // NULL: we want to check only the logs
    }

    /*------------------------------*
     *  Check treedb internals
     *------------------------------*/
    json_t *topic_cols_desc =_treedb_create_topic_cols_desc();
    result += test_treedb_schema(
        tranger,
        topic_cols_desc
    );

    /*------------------------------*
     *      Open treedb
     *------------------------------*/
    const char *treedb_name = "treedb_test";
    if(1) {
        const char *test = "open treedb";

        json_t *error_list = json_pack(
            "[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}"
            "{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}"
            "{s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Creating topic",
            "msg", "Creating topic_desc.json",
            "msg", "Creating topic_cols.json",
            "msg", "Creating topic_var.json",
            "msg", "Creating topic",
            "msg", "Creating topic_desc.json",
            "msg", "Creating topic_cols.json",
            "msg", "Creating topic_var.json",
            "msg", "Creating topic",
            "msg", "Creating topic_desc.json",
            "msg", "Creating topic_cols.json",
            "msg", "Creating topic_var.json",
            "msg", "Creating topic",
            "msg", "Creating topic_desc.json",
            "msg", "Creating topic_cols.json",
            "msg", "Creating topic_var.json",
            "msg", "Creating topic",
            "msg", "Creating topic_desc.json",
            "msg", "Creating topic_cols.json",
            "msg", "Creating topic_var.json",
            "msg", "Creating topic",
            "msg", "Creating topic_desc.json",
            "msg", "Creating topic_cols.json",
            "msg", "Creating topic_var.json"
        );

        set_expected_results( // Check that no logs happen
            test,   // test name
            error_list,   // error_list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        helper_quote2doublequote(schema_sample);
        json_t *jn_schema_sample = legalstring2json(schema_sample, TRUE);
        if(!jn_schema_sample) {
            printf("Can't decode schema_sample json\n");
            exit(-1);
        }

        if(!treedb_open_db(
            tranger,  // owned
            treedb_name,
            jn_schema_sample,
            0
        )) {
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL, result);  // NULL: we want to check only the logs
    }

    /*------------------------------*
     *  Execute schema test
     *------------------------------*/
    result += test_schema(tranger, topic_cols_desc, treedb_name, verbose);

    /*------------------------------*
     *  Execute department test
     *------------------------------*/
    result += test_departments(
        tranger,
        treedb_name,
        without_ok_tests,
        without_bad_tests,
        show_oks,
        verbose
    );

    /*
     *  Close treedb
     */
    if(0) {
        const char *test = "close treedb";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error_list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        /*
         *  Check refcounts
         */
        // result += debug_json("tranger", tranger, verbose);
        json_check_refcounts(tranger, 1000, &result);

        result += treedb_close_db(
            tranger,
            treedb_name
        );

        /*
         *  Check refcounts
         */
        // result += debug_json("tranger", tranger, verbose);
        json_check_refcounts(tranger, 1000, &result);

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL, result);  // NULL: we want to check only the logs
    }

    /*
     *  Open treedb
     */
    if(0) {
        const char *test = "re-open treedb";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error_list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        helper_quote2doublequote(schema_sample);
        json_t *jn_schema_sample = legalstring2json(schema_sample, TRUE);
        if(!jn_schema_sample) {
            printf("Can't decode schema_sample json\n");
            exit(-1);
        }

        treedb_open_db(
            tranger,  // owned
            treedb_name,
            jn_schema_sample,
            0
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL, result);  // NULL: we want to check only the logs
    }

    /*
     *  Check refcounts
     */
    result += test_departments_final(
        tranger,
        treedb_name,
        without_ok_tests,
        without_bad_tests,
        show_oks,
        verbose
    );
    debug_json("tranger", tranger, FALSE);

    /*------------------------------*
     *  Execute user test
     *------------------------------*/
    result += test_users(
        tranger,
        treedb_name,
        without_ok_tests,
        without_bad_tests,
        show_oks,
        verbose
    );

    /*
     *  Check refcounts
     */
    json_check_refcounts(tranger, 1000, &result);

    /*---------------------------------------*
     *      Close and re-open the treedb
     *      Check foto_final1
     *---------------------------------------*/
    if(1) {
        treedb_close_db(tranger, treedb_name);
        const char *test = "Load treedb from tranger";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error_list
            string2json(helper_quote2doublequote(foto_final1), TRUE),  // expected
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        json_t *jn_schema_sample = legalstring2json(schema_sample, TRUE);

        treedb_open_db(
            tranger,  // owned
            treedb_name,
            jn_schema_sample,
            "persistent"
        );

        if(tranger2_topic_size(tranger, "departments") != 10) {
            // Comprueba que no se ha añadido ningún nodo nuevo en la carga
            printf("%s  --> ERROR departments!=10%s\n", On_Red BWhite,Color_Off);
            result += -1;
        }

        if(tranger2_topic_size(tranger, "users") != 19) {
            // Comprueba que no se ha añadido ningún nodo nuevo en la carga
            printf("%s  --> ERROR users!=19 %s\n", On_Red BWhite,Color_Off);
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)

        result += test_json(kw_get_dict(0, tranger, "treedbs", 0, 0), result);
    }

    /*---------------------------------------*
     *      Link compound node
     *---------------------------------------*/
    if(1) {
        if(!test_compound(
                tranger,
                treedb_name,
                without_ok_tests,
                without_bad_tests,
                show_oks,
                verbose
            )) {
            result += -1;
        }
        const char *test = "tranger match";

        json_t *expected = json_pack(
            "{s:s, s:s, s:s, s:s, s:s, s:b, s:b, s:[], s:[], s:[], s:[]}",
            "id", "xxxxxxxxxxxxxxxxxxx",
            "username", "mainop@email.com",
            "firstName", "Bequer",
            "lastName", "Martin",
            "email", "mainop@email.com",
            "emailVerified", 0,
            "disabled", 0,
            "departments",
            "manager",
            "attributes",
            "roles"
        );
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error_list
            expected, // expected
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        if(tranger2_topic_size(tranger, "departments") != 13) {
            printf("%s  --> ERROR departments!=13%s\n", On_Red BWhite,Color_Off);
            result += -1;
        }

        if(tranger2_topic_size(tranger, "users") != 24) {
            // Comprueba que no se ha añadido ningún nodo nuevo en la carga
            printf("%s  --> ERROR users!=24 %s\n", On_Red BWhite,Color_Off);
            result += -1;
        }

        json_t *user_record = treedb_get_node( // WARNING Return is NOT YOURS, pure node
            tranger,
            treedb_name,
            "users",
            "24"
        );
        result += test_json(user_record, result);

        expected = json_pack(
            "{s:s, s:s, s:s, s:{}, s:[s], s:{}}",
            "id", "administration",
            "name", "Administración",
            "department_id", "departments^direction^departments",
            "departments",
            "users", "departments^operation^managers",
            "managers"
        );
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error_list
            expected, // expected
            NULL,   // ignore_keys
            TRUE    // verbose
        );

        json_t *department_record = treedb_get_node( // WARNING Return is NOT YOURS, pure node
            tranger,
            treedb_name,
            "departments",
            "13"
        );

        MT_INCREMENT_COUNT(time_measure, 2)
        MT_PRINT_TIME(time_measure, test)

        result += test_json(department_record, result);
    }

    /*---------------------------------------*
     *      Delete node with links
     *---------------------------------------*/
    if(1) {
        const char *test = "Delete operation, a node with links";

        set_expected_results( // Check that no logs happen
            test,   // test name
            json_pack("[{s:s}, {s:s}]",  // error_list
                "msg", "Cannot delete node: has down links",
                "msg", "Cannot delete node: has up links"
            ),
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        json_t *operation = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "operation"
        );
        treedb_delete_node(
            tranger,
            operation,
            0
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)

        result += test_json(NULL, result);  // NULL: we want to check only the logs
    }

    if(1) {
        const char *test = "Delete administration, a node with links";

        set_expected_results( // Check that no logs happen
            test,   // test name
            json_pack("[{s:s}, {s:s}]",  // error_list
                "msg", "Cannot delete node: has down links",
                "msg", "Cannot delete node: has up links"
            ),
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        json_t *operation = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "administration"
        );

        treedb_delete_node(
            tranger,
            operation,
            0
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)

        result += test_json(NULL, result);  // NULL: we want to check only the logs
    }

    /*
     *  Check refcounts
     */
    json_check_refcounts(tranger, 1000, &result);

    /*---------------------------------------*
     *      Shutdown
     *---------------------------------------*/
    if(print_tranger) {
        print_json2("tranger", tranger);
    } else if(print_treedb) {
        print_json2("print_treedb", kw_get_dict(0, tranger, "treedbs", 0, KW_REQUIRED));
    }

    if(1) {
        const char *test = "Close and shutdown";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error_list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        /*------------------------------*
         *  Cierra la bbdd
         *------------------------------*/
        treedb_close_db(tranger, treedb_name);

        tranger2_shutdown(tranger);

        /*---------------------------*
         *      Destroy all
         *---------------------------*/
        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)

        result += test_json(NULL, result);  // NULL: we want to check only the logs
    }

    JSON_DECREF(topic_cols_desc)

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gobj_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

//    gobj_set_deep_tracing(2);           // TODO TEST
//    gobj_set_global_trace(0, TRUE);     // TODO TEST

    unsigned long memory_check_list[] = {5580, 7253, 0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_bfd(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_bfd);

    gobj_start_up(
        argc,
        argv,
        NULL, // jn_global_settings
        NULL, // startup_persistent_attrs
        NULL, // end_persistent_attrs
        0,  // load_persistent_attrs
        0,  // save_persistent_attrs
        0,  // remove_persistent_attrs
        0,  // list_persistent_attrs
        NULL, // global_command_parser
        NULL, // global_stats_parser
        NULL, // global_authz_checker
        NULL, // global_authenticate_parser
        256*1024L,    // max_block, largest memory block
        100*1024*1024L   // max_system_memory, maximum system memory
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);
    gobj_log_add_handler(
        "test_stdout",
        "stdout",
        LOG_OPT_UP_WARNING,
        0
    );

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        &yev_loop
    );

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    int result = do_test();
    result += global_result;

    /*--------------------------------*
     *  Stop the event loop
     *--------------------------------*/
    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "system memory not free");
        result += -1;
    }

    return result;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop->running = 0;
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
    sigaction(SIGALRM, &sigIntHandler, NULL);   // to debug in kdevelop
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);    // ctrl+c
}
