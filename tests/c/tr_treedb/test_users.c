/****************************************************************************
 *          test.c
 *
 *          Load topic from external json file.
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
#include <string.h>
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
    hgobj gobj = 0;
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
            gobj_log_error(gobj, 0,
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
    json_t *cols= kwid_new_dict(gobj, tranger, 0, "topics`%s`cols", topic_name);

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *fkey = kwid_get(gobj, col, 0, "fkey");
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
                const char *child_id = kw_get_str(gobj, new_record, "id", 0, 0);
                if(empty_string(child_id)) {
                    gobj_log_error(gobj, 0,
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
                    gobj_log_error(gobj, 0,
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
                json_t *ids = kwid_get_ids(kw_get_dict_value(gobj, new_record, col_name, 0, 0));
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
                        gobj_log_error(gobj, 0,
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
                        gobj_log_error(gobj, 0,
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
                        gobj_log_error(gobj, 0,
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
    hgobj gobj = 0;
    int result = 0;

    /*--------------------------------------------------------*
     *  Firt step:  Check file json
     *      first key level must be the treedb name
     *      second key level must be the topic_name
     *      third level are the rows, can be a list or a dict.
     *--------------------------------------------------------*/
    const char *treedb_name; json_t *jn_treedb;
    json_object_foreach(jn_treedbs, treedb_name, jn_treedb) {
        json_t *treedb = kwid_get(gobj, tranger, 0, "treedbs`%s", treedb_name);
        if(!treedb) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Treedb not found in tranger",
                "treedb_name",  "%s", treedb_name,
                NULL
            );
            gobj_trace_json(gobj, jn_treedb, "Treedb not found in tranger");
            gobj_trace_json(gobj, tranger, "Treedb not found in tranger");
            result += -1;
            continue;
        }

        const char *topic_name; json_t *jn_new_data;
        json_object_foreach(jn_treedb, topic_name, jn_new_data) {
            json_t *topic_desc = kwid_get(gobj, tranger, 0, "topics`%s", topic_name);
            if(!topic_desc) {
                gobj_log_error(gobj, 0,
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
            json_int_t current_data_size = (json_int_t)json_object_size(current_data);

            if(strcmp(operation, "update")==0) {
                gobj_log_error(gobj, 0,
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
                    gobj_log_error(gobj, 0,
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
                json_t *new_data = kwid_new_list(gobj, jn_treedb, 0, "%s", topic_name);
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
PUBLIC int test_users(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
)
{
    hgobj gobj = 0;
    int result = 0;


    if(!without_ok_tests) {
        const char *test = "Load users from a treedb-json-file";

        const char *ignore_keys[]= {
            "t",
            NULL
        };
        json_t *expected = string2json(helper_quote2doublequote(foto_final_users), TRUE);
print_json2("USERS expected", expected);
        set_expected_results( // Check that no logs happen
            test,   // test name
            NULL,   // error's list
            expected,  // expected
            ignore_keys,   // ignore_keys
            TRUE    // verbose
        );
        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        const char *operation = "create"; // "update" TODO


        json_t *jn_users_file = string2json(helper_quote2doublequote(users_file), TRUE);
        json_t *jn_treedbs = kw_get_dict(gobj, jn_users_file, "treedbs", 0, KW_REQUIRED);
        JSON_INCREF(jn_treedbs)
        if(load_treedbs(tranger, jn_treedbs, operation)<0) {
            result += -1;
        }

        json_t *users = treedb_get_id_index(tranger, treedb_name, "users"); // Return is NOT YOURS
print_json2("USERS found", users);

        MT_INCREMENT_COUNT(time_measure, 1)
        MT_PRINT_TIME(time_measure, test)
        result += test_json(users, result);

        json_check_refcounts(jn_treedbs, 1000, &result);
        JSON_DECREF(jn_treedbs)
    }

    return result;
}
