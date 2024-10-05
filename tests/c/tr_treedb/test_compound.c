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
 *
 ***************************************************************************/
PUBLIC BOOL test_compound(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
)
{
    BOOL result = 0;
    json_t *data = 0;
    json_t *expected = 0;

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Create new user, good";
        set_expected_results(
            test,
            json_pack("[]"  // error's list
            ),
            verbose
        );

        data = json_pack("{s:s, s:s, s:s, s:s, s:s, s:b}",
            "id", "xxxxxxxxxxxxxxxxxxx",
            "username", "mainop@email.com",
            "firstName", "Bequer",
            "lastName", "Martin",
            "email",  "mainop@email.com",
            "disabled", 0
        );
        expected = json_pack("{s:s, s:s, s:s, s:s, s:s, s:b, s:b, s:b, s:[], s:[], s:[], s:[]}",
            "id", "xxxxxxxxxxxxxxxxxxx",
            "username", "mainop@email.com",
            "firstName", "Bequer",
            "lastName", "Martin",
            "email", "mainop@email.com",
            "emailVerified", 0,
            "disabled", 0,
            "online", 0,
            "departments",
            "manager",
            "attributes",
            "roles"
        );

        json_t *mainop = treedb_create_node(
            tranger, treedb_name,       // treedb
            "users",              // topic_name
            data                        // data
        );
        if(!match_record(mainop, expected, verbose, 0)) {
            result += -1;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, mainop, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                result += -1;
            }
        }
        if(show_oks) {
            log_debug_json(0, mainop, "Record found");
            log_debug_json(0, expected, "Record expected");
        }
        JSON_DECREF(expected);
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Link simple node";
        set_expected_results(
            test,
            json_pack("[]"  // error's list
            ),
            verbose
        );

        json_t *operation = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "operation"
        );
        json_t *mainop = treedb_get_node(
            tranger, treedb_name,
            "users",
            "xxxxxxxxxxxxxxxxxxx"
        );

        result += treedb_link_nodes(
            tranger,
            "managers",
            operation,
            mainop
        );
        result += treedb_link_nodes(
            tranger,
            "users",
            operation,
            mainop
        );

        if(!check_log_result(test, verbose)) {
            result += -1;
        }
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Link compound node";
        set_expected_results(
            test,
            json_pack("[]"  // error's list
            ),
            verbose
        );

        json_t *administration = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "administration"
        );
        json_t *operation = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "operation"
        );

        result += treedb_link_nodes(
            tranger,
            "managers",
            operation,
            administration
        );
        if(!check_log_result(test, verbose)) {
            result += -1;
        } else {
            if(!test_final_foto2(
                    tranger,
                    treedb_name,
                    without_ok_tests,
                    without_bad_tests,
                    show_oks,
                    verbose
                )) {
                result += -1;
            }
        }
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Unlink simple/compound node";
        set_expected_results(
            test,
            json_pack("[]"  // error's list
            ),
            verbose
        );

        json_t *administration = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "administration"
        );
        json_t *operation = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "operation"
        );
        json_t *mainop = treedb_get_node(
            tranger, treedb_name,
            "users",
            "xxxxxxxxxxxxxxxxxxx"
        );

        result += treedb_unlink_nodes(
            tranger,
            "users",
            operation,
            mainop
        );
        result += treedb_unlink_nodes(
            tranger,
            "managers",
            operation,
            mainop
        );

        result += treedb_unlink_nodes(
            tranger,
            "managers",
            operation,
            administration
        );

        JSON_INCREF(mainop);
        treedb_delete_node(
            tranger,
            mainop,
            0
        );

        if(!check_log_result(test, verbose)) {
            result += -1;
        } else {
            if(!test_final_foto(
                    tranger,
                    treedb_name,
                    without_ok_tests,
                    without_bad_tests,
                    show_oks,
                    verbose
                )) {
                result += -1;
            }
        }
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        json_t *administration = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "administration"
        );
        json_t *operation = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "operation"
        );

        result += treedb_link_nodes(
            tranger,
            "managers",
            operation,
            administration
        );
    }

    return result<0?FALSE:TRUE;
}
