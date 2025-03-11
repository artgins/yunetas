/****************************************************************************
 *          test.c
 *
 *          Load topic with code.
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <locale.h>
#include <time.h>

#include <kwid.h>
#include <testing.h>
#include "test_tr_treedb.h"

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int test_compound(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
)
{
    int result = 0;
    json_t *data = 0;

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Create new user, good";

        data = json_pack("{s:s, s:s, s:s, s:s, s:s, s:b}",
            "id", "xxxxxxxxxxxxxxxxxxx",
            "username", "mainop@email.com",
            "firstName", "Bequer",
            "lastName", "Martin",
            "email",  "mainop@email.com",
            "disabled", 0
        );
        json_t *expected = json_pack(
            "{s:s, s:s, s:s, s:s, s:s, s:b, s:b, s:b, s:[], s:[], s:[], s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:i, s:b}}",
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
            "roles",
            "__md_treedb__",
                "treedb_name", "treedb_test",
                "topic_name", "users",
                "g_rowid", 1,
                "i_rowid", 1,
                "t", 99999,
                "tm", 0,
                "tag", 0,
                "pure_node", true
        );

        const char *ignore_keys[]= {
            "t",
            NULL
        };
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            expected,   // expected
            ignore_keys,   // ignore_keys
            true    // verbose
        );

        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        json_t *mainop = treedb_create_node(
            tranger, treedb_name,       // treedb
            "users",              // topic_name
            data                        // data
        );
        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(json_incref(mainop));
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Link simple node";

        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,   // expected
            NULL,   // ignore_keys
            true    // verbose
        );

        time_measure_t time_measure;
        MT_START_TIME(time_measure)

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

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL);
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Link compound node";
        json_t *expected = string2json(helper_quote2doublequote(foto_final2), true);
        const char *ignore_keys[]= {
            "t",
            NULL
        };
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            expected,  // expected
            ignore_keys,   // ignore_keys
            true    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

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

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        json_t *found = kw_get_dict(0, tranger, "treedbs", 0, 0);
        result += test_json(json_incref(found));
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Unlink simple/compound node";

        json_t *expected = string2json(helper_quote2doublequote(foto_final1), true);
        const char *ignore_keys[]= {
            "t",
            NULL
        };
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            expected,  // expected
            ignore_keys,   // ignore_keys
            true    // verbose
        );

        time_measure_t time_measure;
        MT_START_TIME(time_measure)

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

        treedb_delete_node(
            tranger,
            mainop,
            0
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        json_t *found = kw_get_dict(0, tranger, "treedbs", 0, 0);
        result += test_json(json_incref(found));
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Link managers operation -> administration";
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            NULL,  // expected
            NULL,   // ignore_keys
            true    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

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

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        json_t *found = kw_get_dict(0, tranger, "treedbs", 0, 0);
        result += test_json(json_incref(found));
    }

    result += debug_json("tranger", tranger, result<0? true:false);

    return result;
}
