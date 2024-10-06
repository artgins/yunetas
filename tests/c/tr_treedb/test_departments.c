/****************************************************************************
 *          test.c
 *
 *          Load topic with code.
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
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
PUBLIC int test_departments(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
)
{
    int result = 0;
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

        data = json_pack("{s:s, s:s}",
            "id", "direction",
            "name", "Dirección"
        );
        expected = json_pack("{s:s, s:s, s:s, s:{}, s:{}, s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:b}}",
            "id", "direction",
            "name", "Dirección",
            "department_id", "",
            "departments",
            "managers",
            "users",
            "__md_treedb__",
                "treedb_name", "treedb_test",
                "topic_name", "departments",
                "t", 9999,
                "tm", 0,
                "tag", 0,
                "rowid", 0,
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
            ignore_keys, // ignore_keys
            TRUE    // verbose
        );

        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        direction = treedb_create_node(
            tranger, treedb_name,       // treedb
            "departments",              // topic_name
            data                        // data
        );
        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(json_incref(direction), result);
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

        found = treedb_create_node( // Must return 0
            tranger, treedb_name,
            "departments",
            data                       // data
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(found, result);
    }

    /*-----------------------------------*
     *  Administración
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Create administration, good";

        data = json_pack("{s:s, s:s}",
            "id", "administration",
            "name", "Administración"
        );
        expected = json_pack("{s:s, s:s, s:s, s:{}, s:{}, s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:b}}",
            "id", "administration",
            "name", "Administración",
            "department_id", "",
            "departments",
            "managers",
            "users",
            "__md_treedb__",
                "treedb_name", "treedb_test",
                "topic_name", "departments",
                "t", 9999,
                "tm", 0,
                "tag", 0,
                "rowid", 0,
                "pure_node", true

        );
        const char *ignore_keys[]= {
            "t",
            NULL
        };
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            expected, // expected
            ignore_keys, // ignore_keys
            TRUE    // verbose
        );

        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        administration = treedb_create_node(
            tranger, treedb_name,
            "departments",
            data
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(json_incref(administration), result);
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        const char *test = "Get administration, good";
        expected = json_pack("{s:s, s:s, s:s, s:{}, s:{}, s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:b}}",
            "id", "administration",
            "name", "Administración",
            "department_id", "",
            "departments",
            "managers",
            "users",
            "__md_treedb__",
                "treedb_name", "treedb_test",
                "topic_name", "departments",
                "t", 9999,
                "tm", 0,
                "tag", 0,
                "rowid", 0,
                "pure_node", true
        );
        const char *ignore_keys[]= {
            "t",
            NULL
        };
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            expected, // expected
            ignore_keys, // ignore_keys
            TRUE    // verbose
        );

        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        found = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "administration"
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(json_incref(found), result);
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

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL, result);
    }

    if(!without_ok_tests) {
        const char *test = "link direction->administration, good";
        expected = json_pack(
            "{s:s, s:s, s:s, s:{s:{s:s, s:s, s:s, s:{}, s:{}, s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:b}}}, s:{}, s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:b}}",
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
                    "__md_treedb__",
                        "treedb_name", "treedb_test",
                        "topic_name", "departments",
                        "t", 9999,
                        "tm", 0,
                        "tag", 0,
                        "rowid", 0,
                        "pure_node", true,
            "managers",
            "users",
            "__md_treedb__",
                "treedb_name", "treedb_test",
                "topic_name", "departments",
                "t", 9999,
                "tm", 0,
                "tag", 0,
                "rowid", 0,
                "pure_node", true
        );

        const char *ignore_keys[]= {
            "t",
            NULL
        };
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            expected,  // expected
            ignore_keys,   // ignore_keys
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

        found = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "direction"
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(json_incref(found), result);
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
            result += -1;
        }

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL, result);
    }

    if(!without_ok_tests) {
        const char *test = "Create operation, good";
        data = json_pack("{s:s, s:s}",
            "id", "operation",
            "name", "Gestión"
        );
        expected = json_pack(
            "{s:s, s:s, s:s, s:{}, s:{}, s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:b}}",
            "id", "operation",
            "name", "Gestión",
            "department_id", "",
            "departments",
            "managers",
            "users",
            "__md_treedb__",
                "treedb_name", "treedb_test",
                "topic_name", "departments",
                "t", 9999,
                "tm", 0,
                "tag", 0,
                "rowid", 0,
                "pure_node", true
        );

        const char *ignore_keys[]= {
            "t",
            NULL
        };
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            expected, // expected
            ignore_keys,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        operation = treedb_create_node(
            tranger, treedb_name,
            "departments",
            data
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(json_incref(operation), result);
    }

    /*-------------------------------------*
     *      Administración -> Gestión
     *------------------------------------*/
    if(!without_ok_tests) {
        const char *test = "link administration->operation, good";
        expected = json_pack(
            "{s:s, s:s, s:s, s:{s:{s:s, s:s, s:s, s:{}, s:{}, s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:b}}}}, s:{}, s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:b}}}",
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
                    "__md_treedb__",
                        "treedb_name", "treedb_test",
                        "topic_name", "departments",
                        "t", 9999,
                        "tm", 0,
                        "tag", 0,
                        "rowid", 0,
                        "pure_node", true,
            "managers",
            "users",
            "__md_treedb__",
                "treedb_name", "treedb_test",
                "topic_name", "departments",
                "t", 9999,
                "tm", 0,
                "tag", 0,
                "rowid", 0,
                "pure_node", true
        );

        const char *ignore_keys[]= {
            "t",
            NULL
        };
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            expected, // expected
            ignore_keys,   // ignore_keys
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

        found = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "administration"
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(json_incref(found), result);
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
        data = json_pack("{s:s, s:s}",
            "id", "development",
            "name", "Desarrollo"
        );
        expected = json_pack(
            "{s:s, s:s, s:s, s:{}, s:{}, s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:b}}",
            "id", "development",
            "name", "Desarrollo",
            "department_id", "",
            "departments",
            "managers",
            "users",
            "__md_treedb__",
                "treedb_name", "treedb_test",
                "topic_name", "departments",
                "t", 9999,
                "tm", 0,
                "tag", 0,
                "rowid", 0,
                "pure_node", true
        );

        const char *ignore_keys[]= {
            "t",
            NULL
        };
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            expected, // expected
            ignore_keys,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        development = treedb_create_node(
            tranger, treedb_name,
            "departments",
            data
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(json_incref(development), result);
    }

    /*---------------------------------------------*
     *      Administración -> Desarrollo
     *---------------------------------------------*/
    if(!without_ok_tests) {
        const char *test = "link administration->development, good";
        expected = json_pack("{s:s, s:s, s:s, s:"
                "{s:{s:s, s:s, s:s, s:{}, s:{}, s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:b}}, "
                "s:{s:s, s:s, s:s, s:{}, s:{}, s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:b}}},"
                " s:{}, s:[], s:{s:s, s:s, s:i, s:i, s:i, s:i, s:b}}",
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
                    "__md_treedb__",
                        "treedb_name", "treedb_test",
                        "topic_name", "departments",
                        "t", 9999,
                        "tm", 0,
                        "tag", 0,
                        "rowid", 0,
                        "pure_node", true,
                "development",
                    "id", "development",
                    "name", "Desarrollo",
                    "department_id", "departments^administration^departments",
                    "departments",
                    "managers",
                    "users",
                    "__md_treedb__",
                        "treedb_name", "treedb_test",
                        "topic_name", "departments",
                        "t", 9999,
                        "tm", 0,
                        "tag", 0,
                        "rowid", 0,
                        "pure_node", true,
            "managers",
            "users",
            "__md_treedb__",
                "treedb_name", "treedb_test",
                "topic_name", "departments",
                "t", 9999,
                "tm", 0,
                "tag", 0,
                "rowid", 0,
                "pure_node", true
        );

        const char *ignore_keys[]= {
            "t",
            NULL
        };
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            expected, // expected
            ignore_keys,   // ignore_keys
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

        found = treedb_get_node(
            tranger, treedb_name,
            "departments",
            "administration"
        );

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(json_incref(found), result);
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

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(NULL, result);
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
                                        'rowid': 5, \n\
                                        't': 99999, \n\
                                        'tm': 0, \n\
                                        'pure_node': true, \n\
                                        'tag': 0 \n\
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
                                        'rowid': 7, \n\
                                        't': 99999, \n\
                                        'tm': 0, \n\
                                        'pure_node': true, \n\
                                        'tag': 0 \n\
                                    } \n\
                                } \n\
                            }, \n\
                            'users': [], \n\
                            'managers': {}, \n\
                            '__md_treedb__': { \n\
                                'treedb_name': 'treedb_test', \n\
                                'topic_name': 'departments', \n\
                                'rowid': 3, \n\
                                't': 99999, \n\
                                'tm': 0, \n\
                                'pure_node': true, \n\
                                'tag': 0 \n\
                            } \n\
                        } \n\
                    }, \n\
                    'users': [], \n\
                    'managers': {}, \n\
                    '__md_treedb__': { \n\
                        'treedb_name': 'treedb_test', \n\
                        'topic_name': 'departments', \n\
                        'rowid': 1, \n\
                        't': 99999, \n\
                        'tm': 0, \n\
                        'pure_node': true, \n\
                        'tag': 0 \n\
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
                                'rowid': 5, \n\
                                't': 99999, \n\
                                'tm': 0, \n\
                                'pure_node': true, \n\
                                'tag': 0 \n\
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
                                'rowid': 7, \n\
                                't': 99999, \n\
                                'tm': 0, \n\
                                'pure_node': true, \n\
                                'tag': 0 \n\
                            } \n\
                        } \n\
                    }, \n\
                    'users': [], \n\
                    'managers': {}, \n\
                    '__md_treedb__': { \n\
                        'treedb_name': 'treedb_test', \n\
                        'topic_name': 'departments', \n\
                        'rowid': 3, \n\
                        't': 99999, \n\
                        'tm': 0, \n\
                        'pure_node': true, \n\
                        'tag': 0 \n\
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
                        'rowid': 5, \n\
                        't': 99999, \n\
                        'tm': 0, \n\
                        'pure_node': true, \n\
                        'tag': 0 \n\
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
                        'rowid': 7, \n\
                        't': 99999, \n\
                        'tm': 0, \n\
                        'pure_node': true, \n\
                        'tag': 0 \n\
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
    json_t *jn_foto_final = string2json(helper_quote2doublequote(foto_final), TRUE);
    if(!jn_foto_final) {
        result += -1;
    }
    if(!without_ok_tests) {
        const char *test = "departments: foto final";
        const char *ignore_keys[]= {
            "t",
            "rowid",
            NULL
        };
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            jn_foto_final,  // expected
            ignore_keys,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(kw_get_dict(0, tranger, "treedbs", 0, 0), result);
    }

    return result;
}
