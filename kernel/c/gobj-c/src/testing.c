/****************************************************************************
 *          testing.c
 *
 *          Auxiliary functions to do testing
 *
 *          Copyright (c) 2019, Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
#include <inttypes.h>
#include <locale.h>

#include "ansi_escape_codes.h"
#include "helpers.h"
#include "kwid.h"
#include "timeranger2.h"
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
PRIVATE BOOL check_log_result(void);

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

PRIVATE BOOL match_tranger_record(
    json_t *tranger,
    const char *topic_name,
    json_int_t rowid,
    uint32_t uflag,
    uint32_t sflag,
    const char *key,
    json_t *record
);

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE const char *name = "";
PRIVATE json_t *expected_log_messages = 0;
PRIVATE json_t *unexpected_log_messages = 0;
PRIVATE json_t *expected = 0;
PRIVATE int show_log_output;
PRIVATE BOOL verbose = FALSE;
PRIVATE const char **ignore_keys = NULL;

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int capture_log_write(void* v, int priority, const char* bf, size_t len)
{
    json_t *msg = string2json(bf, FALSE);
    if(show_log_output) {
        print_json2("", msg);
    }
    if(!expected_log_messages) {
        // Avoid log_error in kw_ function
        JSON_DECREF(msg)
        return -1;
    }
    json_t *expected_msg = kw_get_list_value(0, expected_log_messages, 0, 0);

    if(expected_msg) {
        JSON_INCREF(expected_msg)
        if(kw_match_simple(msg, expected_msg)) {
            kw_get_list_value(0, expected_log_messages, 0, KW_EXTRACT);
            JSON_DECREF(expected_msg)
            JSON_DECREF(msg)
            return -1; // It's only mine
        }
    }
    json_array_append_new(unexpected_log_messages, msg);
    return -1; // It's only mine
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void set_expected_results(
    const char *name_,
    json_t *errors_list,
    json_t *expected_,
    const char **ignore_keys_,
    BOOL verbose_
)
{
    name = name_;
    verbose = verbose_;
    if(verbose) {
        printf("Test '%s'\n", name?name:"");
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
PUBLIC int test_file(const char *file)
{
    int result = 0;
    json_t *jn_found = load_json_from_file(0, file, "", 0);

    gbuffer_t *gbuf_path = gbuffer_create(32*1024, 32*1024);
    if(!match_record(jn_found, expected, TRUE, gbuf_path)) {
        result = -1;
        if(verbose) {
            printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, name, Color_Off);
            gobj_trace_json(0, jn_found, "Record found");
            gobj_trace_json(0, expected, "Record expected");
        } else {
            printf("%sX%s", On_Red BWhite, Color_Off);
        }
    } else {
        if(!check_log_result()) {
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
PUBLIC int test_json(json_t *jn_found)
{
    int result = 0;

    gbuffer_t *gbuf_path = gbuffer_create(32*1024, 32*1024);
    if(!match_record(jn_found, expected, TRUE, gbuf_path)) {
        result = -1;
        if(verbose) {
            printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, name, Color_Off);
            gobj_trace_json(0, jn_found, "Record found");
            gobj_trace_json(0, expected, "Record expected");
        } else {
            printf("%sX%s", On_Red BWhite, Color_Off);
        }
    } else {
        if(!check_log_result()) {
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
 *  Return TRUE if all is ok.
 ***************************************************************************/
PRIVATE BOOL check_log_result(void)
{
    if(json_array_size(unexpected_log_messages)>0) {
        if(verbose) {
            printf("%s  --> ERROR %s\n", On_Red BWhite,Color_Off);
            int idx; json_t *value;
            printf("      Unexpected error:\n");
            json_array_foreach(unexpected_log_messages, idx, value) {
                printf("          \"%s\"\n", kw_get_str(0, value, "msg", "?", 0));
            }
        } else {
            printf("%sX%s", On_Red BWhite,Color_Off);
        }
        return FALSE;
    }

    if(json_array_size(expected_log_messages)>0) {
        if(verbose) {
            printf("%s  --> ERROR %s\n", On_Red BWhite, Color_Off);
            int idx; json_t *value;
            printf("      Expected error not consumed:\n");
            json_array_foreach(expected_log_messages, idx, value) {
                printf("          \"%s\"\n", kw_get_str(0, value, "msg", "?", 0));
            }
        } else {
            printf("%sX%s", On_Red BWhite,Color_Off);
        }
        return FALSE;
    }

    if(verbose) {
        printf("  --> OK\n");
    } else {
        printf(".");
    }
    return TRUE;
}

/***************************************************************************
 *  Save in ghelpers as kw_compare_dict()
 ***************************************************************************/
PRIVATE BOOL match_record(
    json_t *record_, // NOT owned
    json_t *expected_, // NOT owned
    int verbose,
    gbuffer_t *gbuf_path
)
{
    BOOL ret = TRUE;
    json_t *record = json_deep_copy(record_);
    json_t *expected = json_deep_copy(expected_);
    if(!record) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(0, "match_record('%s'): record NULL", p);
        }
        JSON_DECREF(record)
        JSON_DECREF(expected)
        return FALSE;
    }
    if(!expected) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(0, "match_record('%s'): expected NULL", p);
        }
        JSON_DECREF(record)
        JSON_DECREF(expected)
        return FALSE;
    }

    if(json_typeof(record) != json_typeof(expected)) { // json_typeof CONTROLADO
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(0, "match_record(%s): different json type", p);
        }
        ret = FALSE;
    } else {
        switch(json_typeof(record)) {
            case JSON_ARRAY:
                {
                    if(!match_list(record, expected, verbose, gbuf_path)) {
                        if(verbose) {
                            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                            gobj_trace_msg(0, "match_record(%s): match_list not match", p);
                        }
                        ret = FALSE;
                    }
                }
                break;

            case JSON_OBJECT:
                {
                    void *n; const char *key; json_t *value;
                    json_object_foreach_safe(record, n, key, value) {
                        if(!kw_has_key(expected, key)) {
                            if(verbose) {
                                char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                                gobj_trace_msg(0, "match_record('%s': object key '%s' not found",
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
                                    gobj_trace_msg(0, "match_record('%s'): object not match key '%s'",
                                        p,
                                        key
                                    );
                                    //gobj_trace_json(0, value, "value");
                                    //gobj_trace_json(0, value2, "value2");
                                }
                                ret = FALSE;
                            }
                            if(gbuf_path) {
                                gbuffer_set_wr(gbuf_path, original_position);
                            }

                            if(ret == FALSE) {
                                break;
                            }

                            json_object_del(record, key);
                            json_object_del(expected, key);

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
                                    gobj_trace_msg(0, "match_record('%s'): object array not match key '%s'",
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

                            json_object_del(record, key);
                            json_object_del(expected, key);

                        } else {
                            if(ignore_keys && str_in_list(ignore_keys, key, FALSE)) {
                                /*
                                 *  HACK keys in list are ignored
                                 */
                            } else if(!json_is_identical(value, value2)) {
                                if(verbose) {
                                    char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                                    gobj_trace_msg(0, "match_record('%s'): no identical '%s'",
                                        p,
                                        key
                                    );
                                }
                                ret = FALSE;
                                //break;
                            }
                            json_object_del(record, key);
                            json_object_del(expected, key);
                        }
                    }

                    if(ret == TRUE) {
                        if(json_object_size(record)>0) {
                            if(verbose) {
                                char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                                gobj_trace_msg(0, "match_record('%p'): remain record items", p);
                                gobj_trace_json(0, record, "match_record: remain record items");
                            }
                            ret = FALSE;
                        }
                        if(json_object_size(expected)>0) {
                            if(verbose) {
                                char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                                gobj_trace_msg(0, "match_record('%s'): remain expected items", p);
                                gobj_trace_json(0, expected, "match_record: remain expected items");
                            }
                            ret = FALSE;
                        }
                    }
                }
                break;
            default:
                if(verbose) {
                    char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                    gobj_trace_msg(0, "match_record('%s'): default", p);
                }
                ret = FALSE;
                break;
        }

    }

    JSON_DECREF(record)
    JSON_DECREF(expected)
    return ret;
}

/***************************************************************************
 *  Save in ghelpers as kw_compare_list
 ***************************************************************************/
PRIVATE BOOL match_list(
    json_t *list_, // NOT owned
    json_t *expected_, // NOT owned
    int verbose,
    gbuffer_t *gbuf_path
)
{
    BOOL ret = TRUE;
    json_t *list = json_deep_copy(list_);
    json_t *expected = json_deep_copy(expected_);
    if(!list) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(0, "match_list('%s'): list NULL", p);
        }
        JSON_DECREF(list)
        JSON_DECREF(expected)
        return FALSE;
    }
    if(!expected) {
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(0, "match_list('%s'): expected NULL", p);
        }
        JSON_DECREF(list)
        JSON_DECREF(expected)
        return FALSE;
    }

    if(json_typeof(list) != json_typeof(expected)) { // json_typeof CONTROLADO
        if(verbose) {
            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
            gobj_trace_msg(0, "match_list('%s'): different json type", p);
        }
        ret = FALSE;
    } else {
        switch(json_typeof(list)) {
        case JSON_ARRAY:
            {
                int idx1; json_t *r1;
                json_array_foreach(list, idx1, r1) {
                    const char *id1 = kw_get_str(0, r1, "id", 0, 0);
                    /*--------------------------------*
                     *  List with id records
                     *--------------------------------*/
                    if(id1) {
                        int idx2 = kwid_find_record_in_list(0, expected, id1, FALSE);
                        if(idx2 < 0) {
                            if(verbose) {
                                char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                                gobj_trace_msg(0, "match_list('%s'): record not found in expected list", p);
                                //gobj_trace_json(0, r1, "record");
                                //gobj_trace_json(0, expected, "expected");
                            }
                            ret = FALSE;
                            continue;
                        }
                        json_t *r2 = json_array_get(expected, idx2);

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

                        if(json_array_remove(list, idx1)==0) {
                            idx1--;
                        }
                        json_array_remove(expected, idx2);
                    } else {
                        /*--------------------------------*
                         *  List with any json items
                         *--------------------------------*/
                        int idx2 = kw_find_json_in_list(expected, r1);
                        if(idx2 < 0) {
                            if(verbose) {
                                char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                                gobj_trace_msg(0, "match_list('%s'): item not found in expected list", p);
                                //gobj_trace_json(0, item, "item");
                                //gobj_trace_json(0, expected, "expected");
                            }
                            ret = FALSE;
                            break;
                        }
                        if(json_array_remove(list, idx1)==0) {
                            idx1--;
                        }
                        json_array_remove(expected, idx2);
                    }
                }

                if(ret == TRUE) {
                    if(json_array_size(list)>0) {
                        if(verbose) {
                            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                            gobj_trace_msg(0, "match_list('%s'): remain list items", p);
                            gobj_trace_json(0, list, "match_list: remain list items");
                        }
                        ret = FALSE;
                    }
                    if(json_array_size(expected)>0) {
                        if(verbose) {
                            char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                            gobj_trace_msg(0, "match_list('%s': remain expected items", p);
                            gobj_trace_json(0, expected, "match_list: remain expected items");
                        }
                        ret = FALSE;
                    }
                }
            }
            break;

        case JSON_OBJECT:
            {
                if(!match_record(list, expected, verbose, gbuf_path)) {
                    if(verbose) {
                        char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                        gobj_trace_msg(0, "match_list('%s'): match_record not match", p);
                    }
                    ret = FALSE;
                }
            }
            break;
        default:
            {
                if(verbose) {
                    char *p = gbuf_path?gbuffer_cur_rd_pointer(gbuf_path):"";
                    gobj_trace_msg(0, "match_list('%s'): default", p);
                }
                ret = FALSE;
            }
            break;
        }
    }

    JSON_DECREF(list)
    JSON_DECREF(expected)
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL match_tranger_record(
    json_t *tranger,
    const char *topic_name,
    json_int_t rowid,
    uint32_t uflag,
    uint32_t sflag,
    const char *key,
    json_t *record
)
{
    md2_record_t md_record;

    if(tranger2_get_record(tranger, tranger2_topic(tranger, topic_name), rowid, &md_record, TRUE)<0) {
        return FALSE;
    }

    json_t *record_ = tranger2_read_record_content(
        tranger,
        tranger2_topic(tranger, topic_name),
        &md_record
    );
    if(!record) {
        return FALSE;
    }
    BOOL ret = match_record(record, record_, TRUE, 0);
    json_decref(record_);
    return ret;
}
