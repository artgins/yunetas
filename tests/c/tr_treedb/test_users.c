/****************************************************************************
 *          test.c
 *
 *          Load topic from external json file.
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
PUBLIC int load_topic_new_data(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *new_data // owned
)
{
    int result = 0;

    /*--------------------------------------------------------------*
     *  load topic records except hook fields content
     *--------------------------------------------------------------*/
    int idx; json_t *new_record;
    json_array_foreach(new_data, idx, new_record) {
        JSON_INCREF(new_record);
        json_t *record = treedb_create_node( // Return is NOT YOURS
            tranger,
            treedb_name,
            topic_name,
            new_record
        );
        if(!record) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "treedb_create_node() FAILED",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                NULL
            );
            result += -1;
        }

        // Override loading id by id set by treedb
        if(record) {
            json_object_set_new(
                new_record,
                "id",
                json_string(json_string_value(json_object_get(record, "id")))
            );
        } else {
            json_object_set_new(new_record, "id", json_null()); // Mark wrong record
        }
    }

    /*--------------------------------------------------------------*
     *  load topic hooks
     *--------------------------------------------------------------*/
    /*
     *  Loop desc cols searching fkey
     */
    json_t *cols= kwid_new_dict("", tranger, "topics`%s`cols", topic_name);

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *fkey = kwid_get("", col, "fkey");
        if(!fkey) {
            continue;
        }
        /*
         *  Loop fkey desc searching reverse links
         */
        const char *parent_topic_name; json_t *jn_parent_field_name;
        json_object_foreach(fkey, parent_topic_name, jn_parent_field_name) {
            const char *hook_name = json_string_value(jn_parent_field_name);
            /*
             *  Loop new data searching links
             */
            int new_record_idx; json_t *new_record;
            json_array_foreach(new_data, new_record_idx, new_record) {
                /*
                 *  In "id" of new_record there we put the "true" id of child record
                 */
                const char *child_id = kw_get_str(new_record, "id", 0, 0);
                if(empty_string(child_id)) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "Record without id",
                        "treedb_name",  "%s", treedb_name,
                        "topic_name",   "%s", topic_name,
                        "col_name",     "%s", col_name,
                        "new_record",   "%j", new_record,
                        NULL
                    );
                    result += -1;
                    continue;
                }

                /*
                 *  Get child node
                 */
                json_t *child_record = treedb_get_node( // Return is NOT YOURS
                    tranger,
                    treedb_name,
                    topic_name,
                    child_id
                );
                if(!child_record) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "Node not found",
                        "treedb_name",  "%s", treedb_name,
                        "topic_name",   "%s", topic_name,
                        "col_name",     "%s", col_name,
                        "id",           "%j", child_id,
                        NULL
                    );
                    result += -1;
                    continue;
                }

                /*
                 *  Get ids from new_record fkey field
                 */
                json_t *ids = kwid_get_ids(kw_get_dict_value(new_record, col_name, 0, 0));
                int ids_idx; json_t *jn_mix_id;
                json_array_foreach(ids, ids_idx, jn_mix_id) {
                    const char *topic_and_id = json_string_value(jn_mix_id);
                    if(empty_string((topic_and_id))) {
                        continue;
                    }

                    int tai_size;
                    const char **ss = split2(topic_and_id, "^", &tai_size);
                    if(tai_size != 2) {
                        split_free2(ss);
                        log_error(0,
                            "gobj",         "%s", __FILE__,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                            "msg",          "%s", "Wrong mix fkey",
                            "treedb_name",  "%s", treedb_name,
                            "topic_name",   "%s", topic_name,
                            "col_name",     "%s", col_name,
                            "mix_id",       "%j", jn_mix_id,
                            "record",       "%j", new_record,
                            NULL
                        );
                        result += -1;
                        continue;
                    }

                    if(strcmp(parent_topic_name, ss[0])!=0) {
                        split_free2(ss);
                        continue;
                    }
                    const char *id = ss[1];
                    json_t *parent_record = treedb_get_node( // Return is NOT YOURS
                        tranger,
                        treedb_name,
                        parent_topic_name,
                        id
                    );
                    if(!parent_record) {
                        log_error(0,
                            "gobj",         "%s", __FILE__,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                            "msg",          "%s", "Node not found",
                            "treedb_name",  "%s", treedb_name,
                            "topic_name",   "%s", parent_topic_name,
                            "col_name",     "%s", col_name,
                            "id",           "%s", id,
                            NULL
                        );
                        result += -1;
                        split_free2(ss);
                        continue;
                    }
                    split_free2(ss);

                    if(treedb_link_nodes(
                        tranger,
                        hook_name,
                        parent_record,    // not owned
                        child_record      // not owned
                    )<0) {
                        log_error(0,
                            "gobj",         "%s", __FILE__,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                            "msg",          "%s", "treedb_link_nodes() FAILED",
                            "treedb_name",  "%s", treedb_name,
                            "hook_name",    "%s", hook_name,
                            "col_name",     "%s", col_name,
                            "parent_record","%j", parent_record,
                            "child_record", "%j", child_record,
                            NULL
                        );
                        result += -1;
                    }
                }

                JSON_DECREF(ids);
            }
        }
    }
    JSON_DECREF(cols);
    JSON_DECREF(new_data);

    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int load_treedbs(
    json_t *tranger,
    json_t *jn_treedbs, // owned
    const char *operation
)
{
    int result = 0;

    /*--------------------------------------------------------*
     *  Firt step:  Check file json
     *      first key level must be the treedb name
     *      second key level must be the topic_name
     *      third level are the rows, can be a list or a dict.
     *--------------------------------------------------------*/
    const char *treedb_name; json_t *jn_treedb;
    json_object_foreach(jn_treedbs, treedb_name, jn_treedb) {
        json_t *treedb = kwid_get("", tranger, "treedbs`%s", treedb_name);
        if(!treedb) {
            log_error(0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Treedb not found in tranger",
                "treedb_name",  "%s", treedb_name,
                NULL
            );
            result += -1;
            continue;
        }

        const char *topic_name; json_t *jn_new_data;
        json_object_foreach(jn_treedb, topic_name, jn_new_data) {
            json_t *topic_desc = kwid_get("", tranger, "topics`%s", topic_name);
            if(!topic_desc) {
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Topic not found in treedb",
                    "treedb_name",  "%s", treedb_name,
                    "topic_name",   "%s", topic_name,
                    NULL
                );
                result += -1;
                continue;
            }

            json_t *current_data = treedb_get_id_index(tranger, treedb_name, topic_name);
            json_int_t current_data_size = json_object_size(current_data);

            if(strcmp(operation, "update")==0) {
                log_error(0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "update operation not implemented",
                    "treedb_name",  "%s", treedb_name,
                    "topic_name",   "%s", topic_name,
                    NULL
                );
                result += -1;
                continue;
            } else { // "create"
                if(current_data_size>0) {
                    log_error(0,
                        "gobj",         "%s", __FILE__,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "Topic data NOT EMPTY",
                        "treedb_name",  "%s", treedb_name,
                        "topic_name",   "%s", topic_name,
                        NULL
                    );
                    result += -1;
                    continue;
                }
                json_t *new_data = kwid_new_list("", jn_treedb, topic_name);
                result = load_topic_new_data(tranger, treedb_name, topic_name, new_data);
            }
        }
    }

    JSON_DECREF(jn_treedbs);
    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL test_users(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
)
{
    BOOL result = TRUE;
    json_t *expected;

char foto_final[]= "\
{\n\
    'ba115bf6-5a7c-4ac7-9852-ce32135b427f': {\n\
        'id': 'ba115bf6-5a7c-4ac7-9852-ce32135b427f',\n\
        'username': 'dueño@email.com',\n\
        'firstName': 'Don',\n\
        'lastName': 'Duenño',\n\
        'email': 'dueño@email.com',\n\
        'emailVerified': false,\n\
        'disabled': true,\n\
        'online': false,\n\
        'departments': [\n\
            'departments^direction^users'\n\
        ],\n\
        'manager': [\n\
            'departments^direction^managers'\n\
        ],\n\
        'attributes': [],\n\
        'roles': [],\n\
        '__md_treedb__': {\n\
            'treedb_name': 'treedb_test',\n\
            'topic_name': 'users',\n\
            '__rowid__': 17,\n\
            '__t__': 1568451438,\n\
            '__tag__': 0\n\
        }\n\
    },\n\
    'b6eb2c61-622c-41d3-afa0-548b3fa882cd': {\n\
        'id': 'b6eb2c61-622c-41d3-afa0-548b3fa882cd',\n\
        'username': 'admin@email.com',\n\
        'firstName': 'Admin',\n\
        'lastName': 'Martínez',\n\
        'email': 'admin@email.com',\n\
        'emailVerified': false,\n\
        'disabled': true,\n\
        'online': false,\n\
        'departments': [\n\
            'departments^administration^users'\n\
        ],\n\
        'manager': [\n\
            'departments^administration^managers'\n\
        ],\n\
        'attributes': [],\n\
        'roles': [],\n\
        '__md_treedb__': {\n\
            'treedb_name': 'treedb_test',\n\
            'topic_name': 'users',\n\
            '__rowid__': 18,\n\
            '__t__': 1568451438,\n\
            '__tag__': 0\n\
        }\n\
    },\n\
    '9594f724-c029-4190-8311-a42438f74a99': {\n\
        'id': '9594f724-c029-4190-8311-a42438f74a99',\n\
        'username': 'escritor@email.com',\n\
        'firstName': 'Escritor',\n\
        'lastName': 'Martínez',\n\
        'email': 'escritor@email.com',\n\
        'emailVerified': false,\n\
        'disabled': true,\n\
        'online': false,\n\
        'departments': [\n\
            'departments^administration^users'\n\
        ],\n\
        'manager': [],\n\
        'attributes': [],\n\
        'roles': [],\n\
        '__md_treedb__': {\n\
            'treedb_name': 'treedb_test',\n\
            'topic_name': 'users',\n\
            '__rowid__': 11,\n\
            '__t__': 1568451438,\n\
            '__tag__': 0\n\
        }\n\
    },\n\
    'cda33bc2-d191-41e6-b779-471977869463': {\n\
        'id': 'cda33bc2-d191-41e6-b779-471977869463',\n\
        'username': 'op1@email.com',\n\
        'firstName': 'Op1',\n\
        'lastName': 'Martínez',\n\
        'email': 'op1@email.com',\n\
        'emailVerified': false,\n\
        'disabled': true,\n\
        'online': false,\n\
        'departments': [\n\
            'departments^operation^users'\n\
        ],\n\
        'manager': [],\n\
        'attributes': [],\n\
        'roles': [],\n\
        '__md_treedb__': {\n\
            'treedb_name': 'treedb_test',\n\
            'topic_name': 'users',\n\
            '__rowid__': 12,\n\
            '__t__': 1568451438,\n\
            '__tag__': 0\n\
        }\n\
    },\n\
    '95a4820d-89b0-4bd0-8144-2ef997638b80': {\n\
        'id': '95a4820d-89b0-4bd0-8144-2ef997638b80',\n\
        'username': 'op2@email.com',\n\
        'firstName': 'Op2',\n\
        'lastName': 'Martínez',\n\
        'email': 'op2@email.com',\n\
        'emailVerified': false,\n\
        'disabled': true,\n\
        'online': false,\n\
        'departments': [\n\
            'departments^operation^users'\n\
        ],\n\
        'manager': [],\n\
        'attributes': [],\n\
        'roles': [],\n\
        '__md_treedb__': {\n\
            'treedb_name': 'treedb_test',\n\
            'topic_name': 'users',\n\
            '__rowid__': 13,\n\
            '__t__': 1568451438,\n\
            '__tag__': 0\n\
        }\n\
    },\n\
    'b24aff31-36da-4014-95ec-8b10a1279cf3': {\n\
        'id': 'b24aff31-36da-4014-95ec-8b10a1279cf3',\n\
        'username': 'dev1@email.com',\n\
        'firstName': 'Dev1',\n\
        'lastName': 'Martínez',\n\
        'email': 'dev1@email.com',\n\
        'emailVerified': false,\n\
        'disabled': true,\n\
        'online': false,\n\
        'departments': [\n\
            'departments^development^users'\n\
        ],\n\
        'manager': [],\n\
        'attributes': [],\n\
        'roles': [],\n\
        '__md_treedb__': {\n\
            'treedb_name': 'treedb_test',\n\
            'topic_name': 'users',\n\
            '__rowid__': 14,\n\
            '__t__': 1568451438,\n\
            '__tag__': 0\n\
        }\n\
    },\n\
    'a3586af9-2e57-489e-b6d3-ede04ea77bd0': {\n\
        'id': 'a3586af9-2e57-489e-b6d3-ede04ea77bd0',\n\
        'username': 'dev2@email.com',\n\
        'firstName': 'Dev2',\n\
        'lastName': 'Martínez',\n\
        'email': 'dev2@email.com',\n\
        'emailVerified': false,\n\
        'disabled': true,\n\
        'online': false,\n\
        'departments': [\n\
            'departments^development^users'\n\
        ],\n\
        'manager': [],\n\
        'attributes': [],\n\
        'roles': [],\n\
        '__md_treedb__': {\n\
            'treedb_name': 'treedb_test',\n\
            'topic_name': 'users',\n\
            '__rowid__': 15,\n\
            '__t__': 1568451438,\n\
            '__tag__': 0\n\
        }\n\
    },\n\
    'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8': {\n\
        'id': 'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8',\n\
        'username': 'devboss@email.com',\n\
        'firstName': 'DevBoss',\n\
        'lastName': 'Martínez',\n\
        'email': 'devboss@email.com',\n\
        'emailVerified': false,\n\
        'disabled': true,\n\
        'online': false,\n\
        'departments': [\n\
            'departments^development^users'\n\
        ],\n\
        'manager': [\n\
            'departments^development^managers'\n\
        ],\n\
        'attributes': [],\n\
        'roles': [],\n\
        '__md_treedb__': {\n\
            'treedb_name': 'treedb_test',\n\
            'topic_name': 'users',\n\
            '__rowid__': 19,\n\
            '__t__': 1568451438,\n\
            '__tag__': 0\n\
        }\n\
    }\n\
}\n\
";

    if(!without_ok_tests) {
        const char *path =
        "/yuneta/development/yuneta/^gobj-ecosistema/tests-g/ghelpers/tests/tr_treedb/users.json";

        size_t flags = 0;
        json_error_t error;
        json_t *file_json = json_load_file(path, flags, &error);
        if(!file_json) {
            printf("Can't decode %s json file\n", path);
            return FALSE;
        }

        const char *test = "Load users from a treedb-json-file";
        set_expected_results(
            test,
            json_pack("[]"  // error's list
            ),
            verbose
        );

        const char *operation = "create"; // "update" TODO

        json_t *jn_treedbs = kw_get_dict(file_json, "treedbs", 0, 0);
        if(!jn_treedbs) {
            printf("First level keyword 'treedbs' not found in this file\n  %s\n", path);
            JSON_DECREF(file_json);
            return FALSE;
        }

        JSON_INCREF(jn_treedbs);
        if(load_treedbs(tranger, jn_treedbs, operation)<0) {
            result = FALSE;
        }

        json_t *users = treedb_get_id_index(tranger, treedb_name, "users"); // Return is NOT YOURS

        helper_quote2doublequote(foto_final);
        expected = legalstring2json(foto_final, TRUE);

        if(!match_record(users, expected, verbose, 0)) {
            result = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, users, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                result = FALSE;
            }
        }
        JSON_DECREF(expected);

        int result;
        json_check_refcounts(file_json, 1000, &result);
        JSON_DECREF(file_json);
    }

    return result;
}
