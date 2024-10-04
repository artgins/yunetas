/****************************************************************************
 *          test.c
 *
 *          Load topic with code.
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <locale.h>
#include <time.h>

#include <kwid.h>
#include <testing.h>
#include "test_tr_treedb.h"


/***************************************************************************
    Dirección
        |
         -> Administración
                |
                |-> Gestión
                |
                 -> Desarrollo
 ***************************************************************************/
PUBLIC BOOL test_departments(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
)
{
    BOOL ret = TRUE;
    json_t *direction = 0;
    json_t *administration = 0;
    json_t *operation = 0;
    json_t *development = 0;

/**------------------------------------------------------------*
    Dirección
        |
         -> Administración
*------------------------------------------------------------*/

    json_t *data = 0;
    json_t *found = 0;
    json_t *expected = 0;

    /*-----------------------------------*
     *  Dirección
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Create direction, good";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        data = json_pack("{s:s, s:s}",
            "id", "direction",
            "name", "Dirección"
        );
        expected = json_pack("{s:s, s:s, s:s, s:{}, s:{}, s:[]}",
            "id", "direction",
            "name", "Dirección",
            "department_id", "",
            "departments",
            "managers",
            "users"

        );

        direction = treedb_create_node(
            tranger, treedb_name,       // treedb
            "departments",              // topic_name
            data                        // data
        );
        if(!match_record(direction, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, direction, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        if(show_oks) {
            log_debug_json(0, direction, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(expected);
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_bad_tests) {
        const char *test = "Create administration, wrong, no id";
        set_expected_results( // Check that no logs happen
            test,   // test name
            json_pack("[{s:s}]",  // error's list
                "msg", "Field 'id' required"
            ),
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        data = json_pack("{s:s}",
            "name", "Administración"
        );
        expected = 0;

        found = treedb_create_node( // Must return 0
            tranger, treedb_name,
            "departments",
            data                       // data
        );

        if(found) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR Administracion %s\n", On_Red BWhite, Color_Off);
                log_debug_json(0, found, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }

        if(show_oks) {
            log_debug_json(0, found, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(found);
        JSON_DECREF(expected);
    }

    /*-----------------------------------*
     *  Administración
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Create administration, good";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        data = json_pack("{s:s, s:s}",
            "id", "administration",
            "name", "Administración"
        );
        expected = json_pack("{s:s, s:s, s:s, s:{}, s:{}, s:[]}",
            "id", "administration",
            "name", "Administración",
            "department_id", "",
            "departments",
            "managers",
            "users"
        );

        administration = treedb_create_node(
            tranger, treedb_name,
            "departments",
            data
        );
        if(!match_record(administration, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, administration, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        if(show_oks) {
            log_debug_json(0, administration, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(expected);
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Get administration, good";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        expected = json_pack("{s:s, s:s, s:s, s:{}, s:{}, s:[]}",
            "id", "administration",
            "name", "Administración",
            "department_id", "",
            "departments",
            "managers",
            "users"
        );

        found = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "administration"
        );
        if(!match_record(found, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, found, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        if(show_oks) {
            log_debug_json(0, found, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(expected);
    }

    /*-------------------------------------*
     *      Dirección->Administración
     *------------------------------------*/
    if(!without_bad_tests) {
        const char *test = "link direction->administration: wrong hook";
        set_expected_results( // Check that no logs happen
            test,   // test name
            json_pack("[{s:s}]",  // error's list
                "msg", "hook field not found"
            ),
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        treedb_link_nodes(
            tranger,
            "departmentsx",
            direction,
            administration
        );
        if(!check_log_result(test, verbose)) {
            ret = FALSE;
        }
    }

    if(!without_ok_tests) {
        const char *test = "link direction->administration, good";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        treedb_link_nodes(
            tranger,
            "departments",
            direction,
            administration
        );

        expected = json_pack("{s:s, s:s, s:s, s:{s:{s:s, s:s, s:s, s:{}, s:{}, s:[]}}, s:{}, s:[]}",
            "id", "direction",
            "name", "Dirección",
            "department_id", "",
            "departments",
                "administration",
                    "id", "administration",
                    "name", "Administración",
                    "department_id", "departments^direction^departments",
                    "departments",
                    "managers",
                    "users",
            "managers",
            "users"
        );

        found = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "direction"
        );
        if(!match_record(found, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, found, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        if(show_oks) {
            log_debug_json(0, found, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(expected);
    }

    if(!without_ok_tests) {
        // Comprueba con timeranger
        const char *test = "match treedb with timeranger rowid 1, id 1 Dirección";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        md_record_t md_record;
        tranger_get_record(
            tranger,
            tranger_topic(tranger, "departments"),
            1, //__rowid__
            &md_record,
            TRUE
        );
        found = tranger_read_record_content(
            tranger,
            tranger_topic(tranger, "departments"),
            &md_record
        );
        expected = json_pack("{s:s, s:s, s:s, s:{}, s:{}, s:[]}",
            "id", "direction",
            "name", "Dirección",
            "department_id", "",
            "departments",
            "managers",
            "users"
        );
        if(!expected) {
            if(verbose) {
                printf("%s  --> expected NULL in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        }
        if(!match_record(found, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, found, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        if(show_oks) {
            log_debug_json(0, found, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(found);
        JSON_DECREF(expected);
    }

    if(!without_ok_tests) {
        // Comprueba con timeranger
        const char *test = "match treedb with timeranger rowid 3, id 2 Administración";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        md_record_t md_record;
        tranger_get_record(
            tranger,
            tranger_topic(tranger, "departments"),
            3, //__rowid__
            &md_record,
            TRUE
        );
        found = tranger_read_record_content(
            tranger,
            tranger_topic(tranger, "departments"),
            &md_record
        );
        expected = json_pack("{s:s, s:s, s:s, s:{}, s:{}, s:[]}",
            "id", "administration",
            "name", "Administración",
            "department_id", "departments^direction^departments",
            "departments",
            "managers",
            "users"
        );
        if(!expected) {
            if(verbose) {
                printf("%s  --> expected NULL in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        }

        if(!match_record(found, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, found, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        if(show_oks) {
            log_debug_json(0, found, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(found);
        JSON_DECREF(expected);
    }

/*------------------------------------------------------------*
         -> Administración
                |
                |-> Gestión
*------------------------------------------------------------*/
    /*-----------------------------------*
     *  Gestión
     *-----------------------------------*/
    if(!without_bad_tests) {
        const char *test = "Create operation, wrong, duplicated key";
        set_expected_results( // Check that no logs happen
            test,   // test name
            json_pack("[{s:s}]",  // error's list
                "msg", "Node already exists"
            ),
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        data = json_pack("{s:s, s:s}",
            "id", "administration",
            "name", "Gestión"
        );

        operation = treedb_create_node(
            tranger, treedb_name,
            "departments",
            data
        );
        if(operation) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, operation, "Record found");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
    }

    if(!without_ok_tests) {
        const char *test = "Create operation, good";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        data = json_pack("{s:s, s:s}",
            "id", "operation",
            "name", "Gestión"
        );
        expected = json_pack("{s:s, s:s, s:s, s:{}, s:{}, s:[]}",
            "id", "operation",
            "name", "Gestión",
            "department_id", "",
            "departments",
            "managers",
            "users"
        );

        operation = treedb_create_node(
            tranger, treedb_name,
            "departments",
            data
        );
        if(!match_record(operation, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, operation, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        if(show_oks) {
            log_debug_json(0, operation, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(expected);
    }

    /*-------------------------------------*
     *      Administración -> Gestión
     *------------------------------------*/
    if(!without_ok_tests) {
        const char *test = "link administration->operation, good";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        treedb_link_nodes(
            tranger,
            "departments",
            administration,
            operation
        );

        expected = json_pack("{s:s, s:s, s:s, s:{s:{s:s, s:s, s:s, s:{}, s:{}, s:[]}}, s:{}, s:[]}",
            "id", "administration",
            "name", "Administración",
            "department_id", "departments^direction^departments",
            "departments",
                "operation",
                    "id", "operation",
                    "name", "Gestión",
                    "department_id", "departments^administration^departments",
                    "departments",
                    "managers",
                    "users",
            "managers",
            "users"
        );

        found = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "administration"
        );
        if(!match_record(found, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, found, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        if(show_oks) {
            log_debug_json(0, found, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(expected);
    }

    if(!without_ok_tests) {
        // Comprueba con timeranger
        const char *test = "match treedb with timeranger rowid 5, id 3 Gestión";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        md_record_t md_record;
        tranger_get_record(
            tranger,
            tranger_topic(tranger, "departments"),
            5, //__rowid__
            &md_record,
            TRUE
        );
        found = tranger_read_record_content(
            tranger,
            tranger_topic(tranger, "departments"),
            &md_record
        );
        expected = json_pack("{s:s, s:s, s:s, s:{}, s:{}, s:[]}",
            "id", "operation",
            "name", "Gestión",
            "department_id", "departments^administration^departments",
            "departments",
            "managers",
            "users"
        );
        if(!expected) {
            if(verbose) {
                printf("%s  --> expected NULL in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        }

        if(!match_record(found, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, found, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        if(show_oks) {
            log_debug_json(0, found, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(found);
        JSON_DECREF(expected);
    }

/*------------------------------------------------------------**
         -> Administración
                |
                 -> Desarrollo
*------------------------------------------------------------*/
    /*-----------------------------------*
     *  Desarrollo
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Create development, good";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        data = json_pack("{s:s, s:s}",
            "id", "development",
            "name", "Desarrollo"
        );
        expected = json_pack("{s:s, s:s, s:s, s:{}, s:{}, s:[]}",
            "id", "development",
            "name", "Desarrollo",
            "department_id", "",
            "departments",
            "managers",
            "users"
        );

        development = treedb_create_node(
            tranger, treedb_name,
            "departments",
            data
        );
        if(!match_record(development, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, development, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        if(show_oks) {
            log_debug_json(0, development, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(expected);
    }

    /*---------------------------------------------*
     *      Administración -> Desarrollo
     *---------------------------------------------*/
    if(!without_ok_tests) {
        const char *test = "link administration->development, good";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        treedb_link_nodes(
            tranger,
            "departments",
            administration,
            development
        );

        expected = json_pack("{s:s, s:s, s:s, s:"
                "{s:{s:s, s:s, s:s, s:{}, s:{}, s:[]}, "
                "s:{s:s, s:s, s:s, s:{}, s:{}, s:[]}},"
                " s:{}, s:[]}",
            "id", "administration",
            "name", "Administración",
            "department_id", "departments^direction^departments",
            "departments",
                "operation",
                    "id", "operation",
                    "name", "Gestión",
                    "department_id", "departments^administration^departments",
                    "departments",
                    "managers",
                    "users",
                "development",
                    "id", "development",
                    "name", "Desarrollo",
                    "department_id", "departments^administration^departments",
                    "departments",
                    "managers",
                    "users",
            "managers",
            "users"
        );

        found = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "administration"
        );
        if(!match_record(found, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, found, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        if(show_oks) {
            log_debug_json(0, found, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(expected);
    }

    if(!without_ok_tests) {
        // Comprueba con timeranger
        const char *test = "match treedb with timeranger rowid 7, id 4 Desarrollo";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        md_record_t md_record;
        tranger_get_record(
            tranger,
            tranger_topic(tranger, "departments"),
            7, //__rowid__
            &md_record,
            TRUE
        );
        found = tranger_read_record_content(
            tranger,
            tranger_topic(tranger, "departments"),
            &md_record
        );
        expected = json_pack("{s:s, s:s, s:s, s:{}, s:{}, s:[]}",
            "id", "development",
            "name", "Desarrollo",
            "department_id", "departments^administration^departments",
            "departments",
            "managers",
            "users"
        );
        if(!expected) {
            if(verbose) {
                printf("%s  --> expected NULL in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        }

        if(!match_record(found, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, found, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        if(show_oks) {
            log_debug_json(0, found, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(found);
        JSON_DECREF(expected);
    }

    /*------------------------------------------------------------*
     *          Link and unlink
     *------------------------------------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Link and unlink node";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        data = json_pack("{s:s, s:s}",
            "id", "xxx",
            "name", "Xxxx"
        );

        json_t *xxx = treedb_create_node(
            tranger, treedb_name,
            "departments",
            data
        );
        treedb_link_nodes(
            tranger,
            "departments",
            development,
            xxx
        );
        treedb_unlink_nodes(
            tranger,
            "departments",
            development,
            xxx
        );
        JSON_INCREF(xxx);
        treedb_delete_node(
            tranger,
            xxx,
            0
        );

        if(!check_log_result(test, verbose)) {
            ret = FALSE;
        }
        // La foto final tiene que seguir igual
    }

    /*------------------------------------------------------------*
     *          Foto final
     *------------------------------------------------------------*/
char foto_final[]= "\
{ \n\
    'treedb_test': { \n\
        '__schema_version__': 0, \n\
        '__snaps__': { \n\
            'id': {} \n\
        }, \n\
        '__graphs__': { \n\
            'id': {} \n\
        }, \n\
        'users': { \n\
            'id': {} \n\
        }, \n\
        'departments': { \n\
            'id': { \n\
                'direction': { \n\
                    'id': 'direction', \n\
                    'name': 'Dirección', \n\
                    'department_id': '', \n\
                    'departments': { \n\
                        'administration': { \n\
                            'id': 'administration', \n\
                            'name': 'Administración', \n\
                            'department_id': 'departments^direction^departments', \n\
                            'departments': { \n\
                                'operation': { \n\
                                    'id': 'operation', \n\
                                    'name': 'Gestión', \n\
                                    'department_id': 'departments^administration^departments', \n\
                                    'departments': {}, \n\
                                    'users': [], \n\
                                    'managers': {}, \n\
                                    '__md_treedb__': { \n\
                                        'treedb_name': 'treedb_test', \n\
                                        'topic_name': 'departments', \n\
                                        '__rowid__': 5, \n\
                                        '__t__': 1568007729, \n\
                                        '__tag__': 0 \n\
                                    } \n\
                                }, \n\
                                'development': { \n\
                                    'id': 'development', \n\
                                    'name': 'Desarrollo', \n\
                                    'department_id': 'departments^administration^departments', \n\
                                    'departments': {}, \n\
                                    'users': [], \n\
                                    'managers': {}, \n\
                                    '__md_treedb__': { \n\
                                        'treedb_name': 'treedb_test', \n\
                                        'topic_name': 'departments', \n\
                                        '__rowid__': 7, \n\
                                        '__t__': 1568007729, \n\
                                        '__tag__': 0 \n\
                                    } \n\
                                } \n\
                            }, \n\
                            'users': [], \n\
                            'managers': {}, \n\
                            '__md_treedb__': { \n\
                                'treedb_name': 'treedb_test', \n\
                                'topic_name': 'departments', \n\
                                '__rowid__': 3, \n\
                                '__t__': 1568007729, \n\
                                '__tag__': 0 \n\
                            } \n\
                        } \n\
                    }, \n\
                    'users': [], \n\
                    'managers': {}, \n\
                    '__md_treedb__': { \n\
                        'treedb_name': 'treedb_test', \n\
                        'topic_name': 'departments', \n\
                        '__rowid__': 1, \n\
                        '__t__': 1568007729, \n\
                        '__tag__': 0 \n\
                    } \n\
                }, \n\
                'administration': { \n\
                    'id': 'administration', \n\
                    'name': 'Administración', \n\
                    'department_id': 'departments^direction^departments', \n\
                    'departments': { \n\
                        'operation': { \n\
                            'id': 'operation', \n\
                            'name': 'Gestión', \n\
                            'department_id': 'departments^administration^departments', \n\
                            'departments': {}, \n\
                            'users': [], \n\
                            'managers': {}, \n\
                            '__md_treedb__': { \n\
                                'treedb_name': 'treedb_test', \n\
                                'topic_name': 'departments', \n\
                                '__rowid__': 5, \n\
                                '__t__': 1568007729, \n\
                                '__tag__': 0 \n\
                            } \n\
                        }, \n\
                        'development': { \n\
                            'id': 'development', \n\
                            'name': 'Desarrollo', \n\
                            'department_id': 'departments^administration^departments', \n\
                            'departments': {}, \n\
                            'users': [], \n\
                            'managers': {}, \n\
                            '__md_treedb__': { \n\
                                'treedb_name': 'treedb_test', \n\
                                'topic_name': 'departments', \n\
                                '__rowid__': 7, \n\
                                '__t__': 1568007729, \n\
                                '__tag__': 0 \n\
                            } \n\
                        } \n\
                    }, \n\
                    'users': [], \n\
                    'managers': {}, \n\
                    '__md_treedb__': { \n\
                        'treedb_name': 'treedb_test', \n\
                        'topic_name': 'departments', \n\
                        '__rowid__': 3, \n\
                        '__t__': 1568007729, \n\
                        '__tag__': 0 \n\
                    } \n\
                }, \n\
                'operation': { \n\
                    'id': 'operation', \n\
                    'name': 'Gestión', \n\
                    'department_id': 'departments^administration^departments', \n\
                    'departments': {}, \n\
                    'users': [], \n\
                    'managers': {}, \n\
                    '__md_treedb__': { \n\
                        'treedb_name': 'treedb_test', \n\
                        'topic_name': 'departments', \n\
                        '__rowid__': 5, \n\
                        '__t__': 1568007729, \n\
                        '__tag__': 0 \n\
                    } \n\
                }, \n\
                'development': { \n\
                    'id': 'development', \n\
                    'name': 'Desarrollo', \n\
                    'department_id': 'departments^administration^departments', \n\
                    'departments': {}, \n\
                    'users': [], \n\
                    'managers': {}, \n\
                    '__md_treedb__': { \n\
                        'treedb_name': 'treedb_test', \n\
                        'topic_name': 'departments', \n\
                        '__rowid__': 7, \n\
                        '__t__': 1568007729, \n\
                        '__tag__': 0 \n\
                    } \n\
                } \n\
            } \n\
        }, \n\
        'attributes': { \n\
            'id': {} \n\
        }, \n\
        'roles': { \n\
            'id': {} \n\
        } \n\
    } \n\
} \n\
";

    if(!without_ok_tests) {
        const char *test = "departments: foto final";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        helper_quote2doublequote(foto_final);
        expected = legalstring2json(foto_final, TRUE);
        json_t *treedb = kw_get_dict(tranger, "treedbs", 0, 0);

        if(!match_record(treedb, expected, verbose, 0)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, found, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        JSON_DECREF(expected);
    }

    return ret;
}
