/****************************************************************************
 *          testing.c
 *
 *          Auxiliary functions to do testing
 *
 *          Copyright (c) 2019, Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <inttypes.h>

#include "kwid.h"
#include "testing.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/

/***************************************************************************
 *      Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE BOOL check_log_result(int current_result);

PRIVATE BOOL match_record(
    json_t *record, // NOT owned
    json_t *expected, // NOT owned
    int verbose,
    gbuffer_t *gbuf_path
);
PRIVATE BOOL match_list(
    json_t *list, // NOT owned
    json_t *expected, // NOT owned
    int verbose,
    gbuffer_t *gbuf_path
);

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE const char *test_name = "";
PRIVATE json_t *expected_log_messages = 0;
PRIVATE json_t *unexpected_log_messages = 0;
PRIVATE json_t *expected = 0;
PRIVATE BOOL verbose = FALSE;
PRIVATE const char **ignore_keys = NULL;

PUBLIC time_measure_t yev_time_measure;

#ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
PUBLIC int measuring_times = 0;
PUBLIC int measuring_cur_type = 0;
#endif

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int capture_log_write(void* v, int priority, const char* bf, size_t len)
{
    json_t *msg = string2json(bf, FALSE);

    if(expected_log_messages) {
        json_t *expected_msg = kw_get_list_value(0, expected_log_messages, 0, 0);
        if(expected_msg) {
            JSON_INCREF(expected_msg)
            if(kw_match_simple(msg, expected_msg)) {
                kw_get_list_value(0, expected_log_messages, 0, KW_EXTRACT);
                JSON_DECREF(expected_msg)
                JSON_DECREF(msg)
                // return -1; // It's only mine
                return 0;
            }
        }
    }
    json_array_append_new(unexpected_log_messages, msg);
    // return -1; // It's only mine
    return 0;
}

/***************************************************************************
 *  Return TRUE if all is ok.
 ***************************************************************************/
PRIVATE BOOL check_log_result(int current_result)
{
    if(json_array_size(unexpected_log_messages)>0) {
        if(verbose) {
            printf("<-- %sERROR%s %s\n", On_Red BWhite,Color_Off, test_name);
            int idx; json_t *value;
            printf("      Unexpected error:\n");
            json_array_foreach(unexpected_log_messages, idx, value) {
                printf("          \"%s\"\n", kw_get_str(0, value, "msg", "?", 0));
            }
            printf("\n");
        } else {
            printf("%sX%s", On_Red BWhite,Color_Off);
        }
        return FALSE;
    }

    if(json_array_size(expected_log_messages)>0) {
        if(verbose) {
            printf("<-- %sERROR%s %s\n", On_Red BWhite, Color_Off, test_name);
            int idx; json_t *value;
            printf("      Expected error not consumed:\n");
            json_array_foreach(expected_log_messages, idx, value) {
                printf("          \"%s\"\n", kw_get_str(0, value, "msg", "?", 0));
            }
            printf("\n");
        } else {
            printf("%sX%s", On_Red BWhite, Color_Off);
        }
        return FALSE;
    }

    if(verbose) {
        if(current_result < 0) {
            printf("<-- %sERROR---%s   \"%s\"\n\n", On_Red BWhite, Color_Off, test_name);
        } else {
            printf("<-- %sOK%s   \"%s\"\n\n", On_Green BWhite, Color_Off, test_name);
        }
    }
    return TRUE;
}

/***************************************************************************
 *  Save in ghelpers as kwid_compare_records()
 ***************************************************************************/
PRIVATE BOOL match_record(
    json_t *record_, // NOT owned
    json_t *expected_, // NOT owned
    int verbose,
    gbuffer_t *gbuf_path // vale para pintar el path del json por donde va cuando hay error
)
{
    hgobj gobj = 0;
    BOOL ret = TRUE;
    json_t *record = json_deep_copy(record_);
    json_t *expected = json_deep_copy(expected_);

    if(!record) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(gobj, "match_record(\"%s\"): record NULL", p);
        }
        JSON_DECREF(record)
        JSON_DECREF(expected)
        return FALSE;
    }
    if(!expected) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(gobj, "match_record(\"%s\"): expected NULL", p);
        }
        JSON_DECREF(record)
        JSON_DECREF(expected)
        return FALSE;
    }


    if(json_typeof(record) != json_typeof(expected)) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(gobj, "match_record(\"%s\"): different json type", p);
        }
        ret = FALSE;
    } else if(json_typeof(record) != JSON_OBJECT) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(gobj, "match_record(\"%s\"): not an array json type", p);
        }
        ret = FALSE;
    } else if(json_object_size(record) != json_object_size(expected)) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(gobj, "match_record(\"%s\"): different json size", p);
        }
        ret = FALSE;
    } else {
        void *n; const char *key; json_t *value;
        json_object_foreach_safe(record, n, key, value) {
            if(!kw_has_key(expected, key)) {
                if(verbose) {
                    char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                    gobj_trace_msg(gobj, "match_record(\"%s\": object key \"%s\" not found",
                        p,
                        key
                    );
                }
                ret = FALSE;
                break;
            }
            json_t *value2 = json_object_get(expected, key);
            if(json_typeof(value)==JSON_OBJECT) {

                size_t original_position = 0;
                if(gbuf_path) {
                    original_position = gbuffer_totalbytes(gbuf_path);
                    gbuffer_printf(gbuf_path, ".%s", key);
                }

                if(!match_record(
                        value,
                        value2,
                        verbose,
                        gbuf_path
                    )) {
                    if(verbose) {
                        char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                        gobj_trace_msg(gobj, "match_record(\"%s\"): object not match key \"%s\"",
                            p,
                            key
                        );
                        //gobj_trace_json(gobj, value, "value");
                        //gobj_trace_json(gobj, value2, "value2");
                    }
                    ret = FALSE;
                }
                if(gbuf_path) {
                    gbuffer_set_wr(gbuf_path, original_position);
                }

                if(ret == FALSE) {
                    break;
                }

            } else if(json_typeof(value)==JSON_ARRAY) {

                size_t original_position = 0;
                if(gbuf_path) {
                    original_position = gbuffer_totalbytes(gbuf_path);
                    gbuffer_printf(gbuf_path, ".%s", key);
                }

                if(!match_list(
                        value,
                        value2,
                        verbose,
                        gbuf_path
                    )) {
                    if(verbose) {
                        char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                        gobj_trace_msg(gobj, "match_record(\"%s\"): object array not match key \"%s\"",
                            p,
                            key
                        );
                    }
                    ret = FALSE;
                }
                if(gbuf_path) {
                    gbuffer_set_wr(gbuf_path, original_position);
                }

                if(ret == FALSE) {
                    break;
                }

            } else {
                if(ignore_keys && str_in_list(ignore_keys, key, FALSE)) {
                    /*
                     *  HACK keys in the list are ignored
                     */
                } else if(!json_is_identical(value, value2)) {
                    if(verbose) {
                        char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                        gobj_trace_msg(gobj, "match_record(\"%s\"): no identical \"%s\"",
                            p,
                            key
                        );
                    }
                    ret = FALSE;
                    break;
                }
            }
        }
    }

    JSON_DECREF(record)
    JSON_DECREF(expected)
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL match_list(
    json_t *list_, // NOT owned
    json_t *expected_, // NOT owned
    int verbose,
    gbuffer_t *gbuf_path // vale para pintar el path del json por donde va cuando hay error
)
{
    hgobj gobj = 0;
    BOOL ret = TRUE;
    json_t *list = json_deep_copy(list_);
    json_t *expected = json_deep_copy(expected_);

    if(!list) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(gobj, "match_list(\"%s\"): list NULL", p);
        }
        JSON_DECREF(list);
        JSON_DECREF(expected);
        return FALSE;
    }
    if(!expected) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(gobj, "match_list(\"%s\"): expected NULL", p);
        }
        JSON_DECREF(list);
        JSON_DECREF(expected);
        return FALSE;
    }

    if(json_typeof(list) != json_typeof(expected)) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(gobj, "match_list(\"%s\"): different json type", p);
        }
        ret = FALSE;
    } else if(json_typeof(list) != JSON_ARRAY) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(gobj, "match_list(\"%s\"): not an array json type", p);
        }
        ret = FALSE;
    } else if(json_array_size(list) != json_array_size(expected)) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(gobj, "match_list(\"%s\"): different json size", p);
        }
        ret = FALSE;
    } else {
        int idx1; json_t *r1;
        json_array_foreach(list, idx1, r1) {
            const char *id1 = NULL;
            if(json_is_object(r1)) {
                id1 = kw_get_str(gobj, r1, "id", 0, 0);
            }

            /*--------------------------------*
             *  List with id records
             *--------------------------------*/
            if(id1) {
                json_t *r2 = json_array_get(expected, idx1);

                size_t original_position = 0;
                if(gbuf_path) {
                    original_position = gbuffer_totalbytes(gbuf_path);
                    gbuffer_printf(gbuf_path, ".%s", id1);
                }
                if(!match_record(r1, r2, verbose, gbuf_path)) {
                    ret = FALSE;
                }
                if(gbuf_path) {
                    gbuffer_set_wr(gbuf_path, original_position);
                }

                if(ret == FALSE) {
                    break;
                }

            } else {
                /*--------------------------------*
                 *  List with any json items
                 *--------------------------------*/
                int idx2 = kw_find_json_in_list(gobj, expected, r1, 0);
                if(idx2 < 0) {
                    if(verbose) {
                        char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                        gobj_trace_msg(gobj, "match_list(\"%s\"): item not found in expected list", p);
                        //gobj_trace_json(gobj, item, "item");
                        //gobj_trace_json(gobj, expected, "expected");
                    }
                    ret = FALSE;
                    break;
                }
            }
        }
    }

    JSON_DECREF(list)
    JSON_DECREF(expected)
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void set_expected_results(
    const char *name_,
    json_t *errors_list,
    json_t *expected_, // owned
    const char **ignore_keys_,
    BOOL verbose_
)
{
    test_name = name_;
    verbose = verbose_;
    if(verbose) {
        printf("--> Test \"%s\"\n", test_name?test_name:"");
    }
    JSON_DECREF(expected_log_messages)
    JSON_DECREF(unexpected_log_messages)
    JSON_DECREF(expected)

    expected_log_messages = errors_list?errors_list:json_array();
    unexpected_log_messages = json_array();
    expected = expected_;
    ignore_keys = ignore_keys_;
}

/***************************************************************************
 *  Return 0 if ok, -1 if error
 ***************************************************************************/
PUBLIC int test_json_file(const char *file)
{
    int result = 0;
    json_t *jn_found = load_json_from_file(0, file, "", 0);

    gbuffer_t *gbuf_path = NULL; //gbuffer_create(32*1024, 32*1024); // vale para pintar el path del json por donde va cuando hay error
    if(!match_record(jn_found, expected, TRUE, gbuf_path)) {
        result = -1;
        if(verbose) {
            gobj_trace_json(0, expected, "Record expected");
            gobj_trace_json(0, jn_found, "Record found");
            printf("  <-- %sERROR%s in test: \"%s\"\n", On_Red BWhite, Color_Off, test_name);
        } else {
            printf("%sX%s", On_Red BWhite, Color_Off);
        }
    } else {
        if(!check_log_result(result)) {
            result = -1;
        }
    }

    GBUFFER_DECREF(gbuf_path)
    JSON_DECREF(jn_found)

    JSON_DECREF(expected_log_messages)
    JSON_DECREF(unexpected_log_messages)
    JSON_DECREF(expected)

    return result;
}

/***************************************************************************
 *  Return 0 if ok, -1 if error
 ***************************************************************************/
PUBLIC int test_json(
    json_t *jn_found   // owned
)
{
    int result = 0;

    /*
     *  If jn_found && expected are NULL we want to check only the logs
     */
    gbuffer_t *gbuf_path = gbuffer_create(32*1024, 32*1024); // vale para pintar el path del json por donde va cuando hay error
    if(jn_found && expected) {
        if(json_typeof(jn_found) == JSON_ARRAY) {
            if(!match_list(jn_found, expected, TRUE, gbuf_path)) {
                result = -1;
            }
        } else {
            if(!match_record(jn_found, expected, TRUE, gbuf_path)) {
                result = -1;
            }
        }
        if(result == -1) {
            if(verbose) {
                gobj_trace_json(0, expected, "Record expected");
                gobj_trace_json(0, jn_found, "Record found");
            }
            printf("  <-- %sERROR%s in test: \"%s\"\n", On_Red BWhite, Color_Off, test_name);
        } else {
            printf("%sX%s", On_Red BWhite, Color_Off);
        }
    } else {
        if(!check_log_result(result)) {
            result = -1;
        }
    }

    GBUFFER_DECREF(gbuf_path)
    JSON_DECREF(jn_found)

    JSON_DECREF(expected_log_messages)
    JSON_DECREF(unexpected_log_messages)
    JSON_DECREF(expected)

    return result;
}

/***************************************************************************
 *  Return 0 if ok, -1 if error
 ***************************************************************************/
PUBLIC int test_directory_permission(const char *path, mode_t permission)
{
    mode_t mode = file_permission(path);

    if(mode != permission) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Return 0 if ok, -1 if error
 ***************************************************************************/
PUBLIC int test_file_permission_and_size(const char *path, mode_t permission, off_t size)
{
    mode_t mode = file_permission(path);

    if(mode != permission) {
        return -1;
    }

    off_t sz = file_size(path);
    if(sz != size) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  list and match must be two json arrays of objects
 *  sizes of both must match
 *  keys in 'matches' must match with keys in 'list'
 ***************************************************************************/
PUBLIC int test_list(json_t *list_found, json_t *list_expected, const char *msg, ...)
{
    char message[256];
    int ret = 0;

    va_list ap;
    va_start(ap, msg);
    vsnprintf(message, sizeof(message), msg, ap);
    va_end(ap);

    if(json_array_size(list_found) != json_array_size(list_expected)) {
        printf("  <-- %sERROR%s in test: \"%s\", sizes don't match (found %d, expected %d)\n", On_Red BWhite, Color_Off,
               message,
               (int)json_array_size(list_found),
               (int)json_array_size(list_expected)
        );
        //print_json2("found", list_found);
        //print_json2("expected", list_expected);
        ret += -1;
    }

    int idx;
    json_t *record;
    json_array_foreach(list_found, idx, record) {
        json_t *object_expected = json_array_get(list_expected, idx);
        if(!object_expected) {
            // Error already logged with sizes don't object_expected
            ret += -1;
            break;
        }

        const char *key;
        json_t *expected_value;
        json_object_foreach(object_expected, key, expected_value) {
            json_t *found_value = json_object_get(record, key);
            if(!json_is_identical(expected_value, found_value)) {
                char *expected_ = json2uglystr(expected_value);
                char *found_ = json2uglystr(found_value);
                // Error already logged with sizes don't object_expected
                printf("  <-- %sERROR%s in test: \"%s\", %s don't object_expected, idx %d, expected %s, found %s\n",
                    On_Red BWhite, Color_Off, message, key, idx, expected_, found_
                );
                ret += -1;
                GBMEM_FREE(expected_)
                GBMEM_FREE(found_)
                break;
            }
        }
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void set_measure_times(int types) // Set the measure of times of types (-1 all)
{
#ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
    measuring_times = types;
#else
    gobj_log_error(0, LOG_OPT_TRACE_STACK,
         "function",     "%s", __FUNCTION__,
         "msgset",       "%s", MSGSET_PARAMETER_ERROR,
         "msg",          "%s", "CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES not set",
         NULL
    );
#endif
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int get_measure_times(void)
{
#ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
    return measuring_times;
#else
    gobj_log_error(0, LOG_OPT_TRACE_STACK,
         "function",     "%s", __FUNCTION__,
         "msgset",       "%s", MSGSET_PARAMETER_ERROR,
         "msg",          "%s", "CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES not set",
         NULL
    );
    return 0;
#endif
}
