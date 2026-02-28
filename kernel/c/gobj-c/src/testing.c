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

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE const char *test_name = "";
PRIVATE json_t *expected_log_messages = 0;
PRIVATE json_t *unexpected_log_messages = 0;
PRIVATE json_t *expected = 0;
PRIVATE int verbose = FALSE;
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
 *
 ***************************************************************************/
PUBLIC void set_expected_results(
    const char *name_,
    json_t *errors_list,
    json_t *expected_, // owned
    const char **ignore_keys_,
    int verbose_
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

    if(!kwid_compare_records(0, jn_found, expected, ignore_keys, FALSE, FALSE, verbose)) {
        result = -1;
        if(verbose > 1) {
            gobj_trace_json(0, expected, "Record expected");
            gobj_trace_json(0, jn_found, "Record found");
            printf("  <-- %sERROR%s in test: \"%s\"\n", On_Red BWhite, Color_Off, test_name);
        }
    } else {
        if(!check_log_result(result)) {
            result = -1;
        }
    }

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
    if(jn_found && expected) {
        if(!kwid_compare_records(0, jn_found, expected, ignore_keys, FALSE, FALSE, verbose)) {
            result = -1;
            if(verbose > 1) {
                gobj_trace_json(0, expected, "Record expected");
                gobj_trace_json(0, jn_found, "Record found");
            }
            printf("  <-- %sERROR%s in test: \"%s\"\n", On_Red BWhite, Color_Off, test_name);
        }
    } else {
        if(!check_log_result(result)) {
            result = -1;
        }
    }

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
        //print_json("found", list_found);
        //print_json("expected", list_expected);
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
