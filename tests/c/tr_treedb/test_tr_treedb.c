/****************************************************************************
 *          test.c
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

#include "schema_sample.c"
#include "test_tr_treedb.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/
#define APP_NAME    "test_tr_treedb"
#define DOC         "Test Treedb."

#define VERSION     __ghelpers_version__
#define SUPPORT     "<niyamaka at yuneta.io>"
#define DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *      Structures
 ***************************************************************************/

/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 0
#define MAX_ARGS 0
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */

    int without_ok_tests;
    int without_bad_tests;
    int show_oks;
    int print_tranger;
    int print_treedb;
    int verbose;
};
PRIVATE int show_log_output;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE json_t *expected_log_messages = 0;
PRIVATE json_t *unexpected_log_messages = 0;

// Set by yuneta_entry_point()
const char *argp_program_version = APP_NAME " " VERSION;
const char *argp_program_bug_address = SUPPORT;

/* Program documentation. */
static char doc[] = DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-----------------key-----arg---------flags---doc-----------------group */
{"without-ok-tests",    1,      0,          0,      "Not execute ok tests", 1},
{"without-bad-tests",   2,      0,          0,      "Not execute bad tests", 1},
{"show-log-output",     3,      0,          0,      "Show log ouputs", 1},
{"show-oks",            4,      0,          0,      "Show log ouputs (verbose required)", 1},
{"print-tranger",       5,      0,          0,      "Print tranger json", 1},
{"print-treedb",        6,      0,          0,      "Print treedb json", 1},
{"verbose",             7,      0,          0,      "Verbose", 1},

{0}
};

/* Our argp parser. */
static struct argp argp = {
    options,
    parse_opt,
    args_doc,
    doc
};

/***************************************************************************
 *  Parse a single option
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    /*
     *  Get the input argument from argp_parse,
     *  which we know is a pointer to our arguments structure.
     */
    struct arguments *arguments = state->input;

    switch (key) {
    case 1:
        arguments->without_ok_tests = 1;
        break;

    case 2:
        arguments->without_bad_tests = 1;
        break;

    case 3:
        show_log_output = 1;
        break;

    case 4:
        arguments->show_oks = 1;
        break;

    case 5:
        arguments->print_tranger = 1;
        break;

    case 6:
        arguments->print_treedb = 1;
        break;

    case 7:
        arguments->verbose = 1;
        break;

    case ARGP_KEY_ARG:
        if (state->arg_num >= MAX_ARGS) {
            /* Too many arguments. */
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        break;

    case ARGP_KEY_END:
        if (state->arg_num < MIN_ARGS) {
            /* Not enough arguments. */
            argp_usage (state);
        }
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/***************************************************************************
 *  Prints to the provided buffer a nice number of bytes (KB, MB, GB, etc)
 *  https://www.mbeckler.org/blog/?p=114
 ***************************************************************************/
PRIVATE void pretty_bytes(char* bf, int bfsize, uint64_t bytes)
{
    const char* suffixes[7];
    suffixes[0] = "B";
    suffixes[1] = "Miles";
    suffixes[2] = "Millones";
    suffixes[3] = "GB";
    suffixes[4] = "TB";
    suffixes[5] = "PB";
    suffixes[6] = "EB";
    uint s = 0; // which suffix to use
    double count = bytes;
    while (count >= 1000 && s < 7)
    {
        s++;
        count /= 1000;
    }
    if (count - floor(count) == 0.0)
        snprintf(bf, bfsize, "%d %s", (int)count, suffixes[s]);
    else
        snprintf(bf, bfsize, "%.1f %s", count, suffixes[s]);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE inline double ts_diff2 (struct timespec start, struct timespec end)
{
    uint64_t s, e;
    s = ((uint64_t)start.tv_sec)*1000000 + ((uint64_t)start.tv_nsec)/1000;
    e = ((uint64_t)end.tv_sec)*1000000 + ((uint64_t)end.tv_nsec)/1000;
    return ((double)(e-s))/1000000;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE inline struct timespec ts_diff (struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int capture_log_write(void* v, int priority, const char* bf, size_t len)
{
    json_t *msg = legalstring2json(bf, FALSE);
    if(show_log_output) {
        print_json(msg);
    }
    if(!expected_log_messages) {
        // Avoid log_error in kw_ function
        JSON_DECREF(msg);
        return -1;
    }
    json_t *expected_msg = kw_get_list_value(expected_log_messages, 0, 0);

    if(expected_msg) {
        JSON_INCREF(expected_msg);
        if(kw_match_simple(msg, expected_msg)) {
            kw_get_list_value(expected_log_messages, 0, KW_EXTRACT);
            JSON_DECREF(expected_msg);
            JSON_DECREF(msg);
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
    const char *name,
    json_t *errors_list,
    int verbose
)
{
    if(verbose) {
        printf("Test '%s'\n", name);
    }
    JSON_DECREF(expected_log_messages);
    json_array_clear(unexpected_log_messages);
    expected_log_messages = errors_list;
}

/***************************************************************************
 *  Return TRUE if all is ok.
 ***************************************************************************/
PUBLIC BOOL check_log_result(const char *test, int verbose)
{
    if(json_array_size(unexpected_log_messages)>0) {
        if(verbose) {
            printf("%s  --> ERROR %s\n", On_Red BWhite,Color_Off);
            int idx; json_t *value;
            printf("      Unexpected error:\n");
            json_array_foreach(unexpected_log_messages, idx, value) {
                printf("          \"%s\"\n", kw_get_str(value, "msg", "?", 0));
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
                printf("          \"%s\"\n", kw_get_str(value, "msg", "?", 0));
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
PUBLIC BOOL match_record(
    json_t *record_, // NOT owned
    json_t *expected_, // NOT owned
    int verbose,
    GBUFFER *gbuf_path
)
{
    BOOL ret = TRUE;
    json_t *record = json_deep_copy(record_);
    json_t *expected = json_deep_copy(expected_);
    if(!record) {
        if(verbose) {
            char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
            trace_msg("match_record('%s'): record NULL", p);
        }
        JSON_DECREF(record);
        JSON_DECREF(expected);
        return FALSE;
    }
    if(!expected) {
        if(verbose) {
            char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
            trace_msg("match_record('%s'): expected NULL", p);
        }
        JSON_DECREF(record);
        JSON_DECREF(expected);
        return FALSE;
    }

    if(json_typeof(record) != json_typeof(expected)) { // json_typeof CONTROLADO
        if(verbose) {
            char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
            trace_msg("match_record(%s): diferent json type", p);
        }
        ret = FALSE;
    } else {
        switch(json_typeof(record)) {
            case JSON_ARRAY:
                {
                    if(!match_list(record, expected, verbose, gbuf_path)) {
                        if(verbose) {
                            char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                            trace_msg("match_record(%s): match_list not match", p);
                        }
                        ret = FALSE;
                    }
                }
                break;

            case JSON_OBJECT:
                {
                    json_object_del(record, "__md_treedb__");
                    json_object_del(expected, "__md_treedb__");
                    void *n; const char *key; json_t *value;
                    json_object_foreach_safe(record, n, key, value) {
                        if(!kw_has_key(expected, key)) {
                            if(verbose) {
                                char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                                trace_msg("match_record('%s': object key '%s' not found",
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
                                original_position = gbuf_totalbytes(gbuf_path);
                                gbuf_printf(gbuf_path, ".%s", key);
                            }

                            if(!match_record(
                                    value,
                                    value2,
                                    verbose,
                                    gbuf_path
                                )) {
                                if(verbose) {
                                    char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                                    trace_msg("match_record('%s'): object not match key '%s'",
                                        p,
                                        key
                                    );
                                    //log_debug_json(0, value, "value");
                                    //log_debug_json(0, value2, "value2");
                                }
                                ret = FALSE;
                            }
                            if(gbuf_path) {
                                gbuf_set_wr(gbuf_path, original_position);
                            }

                            if(ret == FALSE) {
                                break;
                            }

                            json_object_del(record, key);
                            json_object_del(expected, key);

                        } else if(json_typeof(value)==JSON_ARRAY) {

                            size_t original_position = 0;
                            if(gbuf_path) {
                                original_position = gbuf_totalbytes(gbuf_path);
                                gbuf_printf(gbuf_path, ".%s", key);
                            }

                            if(!match_list(
                                    value,
                                    value2,
                                    verbose,
                                    gbuf_path
                                )) {
                                if(verbose) {
                                    char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                                    trace_msg("match_record('%s'): object array not match key '%s'",
                                        p,
                                        key
                                    );
                                }
                                ret = FALSE;
                            }
                            if(gbuf_path) {
                                gbuf_set_wr(gbuf_path, original_position);
                            }

                            if(ret == FALSE) {
                                break;
                            }

                            json_object_del(record, key);
                            json_object_del(expected, key);

                        } else {
                            if(!kw_is_identical(value, value2)) {
                                if(verbose) {
                                    char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                                    trace_msg("match_record('%s'): no identical '%s'",
                                        p,
                                        key
                                    );
                                }
                                ret = FALSE;
                                break;
                            } else {
                                json_object_del(record, key);
                                json_object_del(expected, key);
                            }
                        }
                    }

                    if(ret == TRUE) {
                        if(json_object_size(record)>0) {
                            if(verbose) {
                                char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                                trace_msg("match_record('%p'): remain record items", p);
                                log_debug_json(0, record, "match_record: remain record items");
                            }
                            ret = FALSE;
                        }
                        if(json_object_size(expected)>0) {
                            if(verbose) {
                                char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                                trace_msg("match_record('%s'): remain expected items", p);
                                log_debug_json(0, expected, "match_record: remain expected items");
                            }
                            ret = FALSE;
                        }
                    }
                }
                break;
            default:
                if(verbose) {
                    char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                    trace_msg("match_record('%s'): default", p);
                }
                ret = FALSE;
                break;
        }

    }

    JSON_DECREF(record);
    JSON_DECREF(expected);
    return ret;
}

/***************************************************************************
 *  Save in ghelpers as kw_compare_list
 ***************************************************************************/
PUBLIC BOOL match_list(
    json_t *list_, // NOT owned
    json_t *expected_, // NOT owned
    int verbose,
    GBUFFER *gbuf_path
)
{
    BOOL ret = TRUE;
    json_t *list = json_deep_copy(list_);
    json_t *expected = json_deep_copy(expected_);
    if(!list) {
        if(verbose) {
            char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
            trace_msg("match_list('%s'): list NULL", p);
        }
        JSON_DECREF(list);
        JSON_DECREF(expected);
        return FALSE;
    }
    if(!expected) {
        if(verbose) {
            char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
            trace_msg("match_list('%s'): expected NULL", p);
        }
        JSON_DECREF(list);
        JSON_DECREF(expected);
        return FALSE;
    }

    if(json_typeof(list) != json_typeof(expected)) { // json_typeof CONTROLADO
        if(verbose) {
            char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
            trace_msg("match_list('%s'): diferent json type", p);
        }
        ret = FALSE;
    } else {
        switch(json_typeof(list)) {
        case JSON_ARRAY:
            {
                int idx1; json_t *r1;
                json_array_foreach(list, idx1, r1) {
                    const char *id1 = kw_get_str(r1, "id", 0, 0);
                    /*--------------------------------*
                     *  List with id records
                     *--------------------------------*/
                    if(id1) {
                        size_t idx2 = kwid_find_record_in_list("", expected, id1);
                        if(idx2 < 0) {
                            if(verbose) {
                                char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                                trace_msg("match_list('%s'): record not found in expected list", p);
                                //log_debug_json(0, r1, "record");
                                //log_debug_json(0, expected, "expected");
                            }
                            ret = FALSE;
                            continue;
                        }
                        json_t *r2 = json_array_get(expected, idx2);

                        size_t original_position = 0;
                        if(gbuf_path) {
                            original_position = gbuf_totalbytes(gbuf_path);
                            gbuf_printf(gbuf_path, ".%s", id1);
                        }
                        if(!match_record(r1, r2, verbose, gbuf_path)) {
                            ret = FALSE;
                        }
                        if(gbuf_path) {
                            gbuf_set_wr(gbuf_path, original_position);
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
                        int idx2 = kw_find_json_in_list("", expected, r1);
                        if(idx2 < 0) {
                            if(verbose) {
                                char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                                trace_msg("match_list('%s'): item not found in expected list", p);
                                //log_debug_json(0, item, "item");
                                //log_debug_json(0, expected, "expected");
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
                            char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                            trace_msg("match_list('%s'): remain list items", p);
                            log_debug_json(0, list, "match_list: remain list items");
                        }
                        ret = FALSE;
                    }
                    if(json_array_size(expected)>0) {
                        if(verbose) {
                            char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                            trace_msg("match_list('%s': remain expected items", p);
                            log_debug_json(0, expected, "match_list: remain expected items");
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
                        char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                        trace_msg("match_list('%s'): match_record not match", p);
                    }
                    ret = FALSE;
                }
            }
            break;
        default:
            {
                if(verbose) {
                    char *p = gbuf_path?gbuf_cur_rd_pointer(gbuf_path):"";
                    trace_msg("match_list('%s'): default", p);
                }
                ret = FALSE;
            }
            break;
        }
    }

    JSON_DECREF(list);
    JSON_DECREF(expected);
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
    md_record_t md_record;

    if(tranger_get_record(tranger, tranger_topic(tranger, topic_name), rowid, &md_record, TRUE)<0) {
        return FALSE;
    }

    json_t *record_ = tranger_read_record_content(
        tranger,
        tranger_topic(tranger, topic_name),
        &md_record
    );
    if(!record) {
        return FALSE;
    }
    BOOL ret = match_record(record, record_, TRUE, 0);
    json_decref(record_);
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL test_treedb_schema(
    json_t *tranger,
    json_t *topic_cols_desc,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
)
{
    BOOL ret = TRUE;
    const char *test;

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        test = "good_desc_01";
        json_t *topic = tranger_create_topic(
            tranger,
            test,
            "id",
            "",
            sf_rowid_key,
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
        set_expected_results(
            test,
            json_pack("[]"  // error's list
            ),
            verbose
        );
        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list("verbose", topic, "cols")
        );
        if(!check_log_result(test, verbose)) {
            ret = FALSE;
        }
        tranger_delete_topic(tranger, test);
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        test = "good_desc_02";
        json_t *topic = tranger_create_topic(
            tranger,
            test,
            "id",
            "",
            sf_rowid_key,
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
        set_expected_results(
            test,
            json_pack("[]"  // error's list
            ),
            verbose
        );
        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list("verbose", topic, "cols")
        );
        if(!check_log_result(test, verbose)) {
            ret = FALSE;
        }
        tranger_delete_topic(tranger, test);
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_ok_tests) {
        test = "good_desc_03";
        json_t *topic = tranger_create_topic(
            tranger,
            test,
            "id",
            "",
            sf_rowid_key,
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
        set_expected_results(
            test,
            json_pack("[]"  // error's list
            ),
            verbose
        );
        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list("verbose", topic, "cols")
        );
        if(!check_log_result(test, verbose)) {
            ret = FALSE;
        }

        tranger_delete_topic(tranger, test);
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_bad_tests) {
        test = "bad_desc_01";
        json_t *topic = tranger_create_topic(
            tranger,
            test,
            "id",
            "",
            sf_rowid_key,
            json_pack("[{s:b, s:[s], s:i, s:s, s:s}]",
                "id", 1,
                "header", "Xx",
                "fillspace", 20,
                "type", "xinteger",
                "flag", "xrequired"
            ),
            0
        );
        set_expected_results(
            test,
            json_pack("[{s:s, s:s}, {s:s, s:s}, {s:s, s:s}, {s:s, s:s}]",  // error's list
                "msg", "Wrong basic type",
                "field", "id",
                "msg", "Wrong basic type",
                "field", "header",
                "msg", "Wrong enum type",
                "field", "type",
                "msg", "Wrong enum type",
                "field", "flag"
            ),
            verbose
        );

        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list("verbose", topic, "cols")
        );
        if(!check_log_result(test, verbose)) {
            ret = FALSE;
        }
        tranger_delete_topic(tranger, test);
    }

    /*-----------------------------------*
     *
     *-----------------------------------*/
    if(!without_bad_tests) {
        test = "bad_desc_02";
        json_t *topic = tranger_create_topic(
            tranger,
            test,
            "id",
            "",
            sf_rowid_key,
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
        set_expected_results(
            test,
            json_pack("[{s:s, s:s}, {s:s, s:s}, {s:s, s:s}, {s:s, s:s}]",  // error's list
                "msg", "Wrong basic type",
                "field", "id",
                "msg", "Wrong basic type",
                "field", "header",
                "msg", "Wrong enum type",
                "field", "type",
                "msg", "Wrong enum type",
                "field", "flag"
            ),
            verbose
        );

        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list("verbose", topic, "cols")
        );
        if(!check_log_result(test, verbose)) {
            ret = FALSE;
        }
        tranger_delete_topic(tranger, test);
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL test_schema(
    json_t *tranger,
    json_t *topic_cols_desc,
    const char *treedb_name,
    int verbose
)
{
    BOOL ret = TRUE;

    const char *topic_name; json_t *topic;
    json_object_foreach(kw_get_dict(tranger, "topics", 0, KW_REQUIRED), topic_name, topic)
    {
        set_expected_results(
            topic_name,
            json_pack("[]"  // error's list
            ),
            verbose
        );
        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list("verbose", topic, "cols")
        );
        if(!check_log_result(topic_name, verbose)) {
            ret = FALSE;
        }
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
void test_performance(
    json_t *rc2,
    int caso,
    const char *desc,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks
)
{
    uint64_t cnt;
    struct timespec st, et;
    double dt;

    cnt = without_ok_tests * without_bad_tests;

    /*-------------------------------------*
     *  Loop
     *-------------------------------------*/
    switch(caso) {
    case 1:
        {
            clock_gettime (CLOCK_MONOTONIC, &st);


            clock_gettime (CLOCK_MONOTONIC, &et);
        }
        break;

    default:
        printf("MIERDA\n");
    }


    /*-------------------------------------*
     *  Your end code
     *-------------------------------------*/

    /*-------------------------------------*
     *  Print times
     *-------------------------------------*/
    char bf[128];
    pretty_bytes(bf, sizeof(bf), cnt);

    dt = ts_diff2(st, et);

    setlocale(LC_ALL, "");
    printf("# test %d %-16s ==> without_ok_tests %'d; without_bad_tests %'d; %'"PRIu64" items; %f seconds; %'lu op/sec\n\n",
        caso,
        desc,
        without_ok_tests,
        without_bad_tests,
        cnt,
        dt,
        (unsigned long)(((double)cnt)/dt)
    );
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    int ret = 0;

    struct arguments arguments;
    /*
     *  Default values
     */
    memset(&arguments, 0, sizeof(arguments));
    arguments.without_ok_tests = 0;
    arguments.without_bad_tests = 0;

    // TODO en verbose pintar OK\n, en no-verbose pintar ..... y punto rojo si error.

    /*
     *  Parse arguments
     */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    /*-------------------------------------*
     *  Your start code
     *-------------------------------------*/
    init_ghelpers_library(APP_NAME);
    log_startup(
        APP_NAME,       // application name
        VERSION,        // applicacion version
        APP_NAME        // executable program, to can trace stack
    );

    BOOL TEST_MEM = 0;

    static uint32_t mem_list[] = {0, 0};
    gbmem_trace_alloc_free(TEST_MEM, mem_list);

    /*------------------------------------------------*
     *          Setup memory
     *------------------------------------------------*/
    #define MEM_MIN_BLOCK   512
    uint64_t MEM_MAX_SYSTEM_MEMORY = free_ram_in_kb() * 1024LL;
    MEM_MAX_SYSTEM_MEMORY /= 100LL;
    MEM_MAX_SYSTEM_MEMORY *= 90LL;  // Coge el 90% de la memoria

    uint64_t MEM_MAX_BLOCK = (MEM_MAX_SYSTEM_MEMORY / sizeof(md_record_t)) * sizeof(md_record_t);
    MEM_MAX_BLOCK = MIN(1*1024*1024*1024LL, MEM_MAX_BLOCK);  // 1*G max

    uint64_t MEM_SUPERBLOCK = MEM_MAX_BLOCK;

    if(TEST_MEM || 1) {
        gbmem_startup(
            MEM_MIN_BLOCK,
            MEM_MAX_BLOCK,
            MEM_SUPERBLOCK,
            MEM_MAX_SYSTEM_MEMORY,
            NULL,
            0
        );
    } else {
        gbmem_startup_system(
            MEM_MAX_BLOCK,
            MEM_MAX_SYSTEM_MEMORY
        );
    }

    /*
     *  WARNING now all json is gbmem allocated
     */
    json_set_alloc_funcs(
        gbmem_malloc,
        gbmem_free
    );
    uv_replace_allocator(
        gbmem_malloc,
        gbmem_realloc,
        gbmem_calloc,
        gbmem_free
    );

    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    if(!TEST_MEM) {
        log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);
        log_add_handler(
            "test_stdout",
            "stdout",
            arguments.verbose?LOG_OPT_ALL:LOG_OPT_UP_WARNING|LOG_HND_OPT_BEATIFUL_JSON,
            0
        );
    } else {
        log_add_handler(
            "test_stdout",
            "stdout",
            LOG_OPT_ALL,
            0
        );
    }

    expected_log_messages = json_array();
    unexpected_log_messages = json_array();

    /*------------------------------*
     *  La bbddd de pruebas
     *------------------------------*/
    char *path = "/test/treedb/db_test1";

    /*------------------------------*
     *  Destruye la bbdd previa
     *------------------------------*/
    rmrdir(path);

    /*------------------------------*
     *      Startup tranger
     *------------------------------*/
    json_t *jn_tranger = json_pack("{s:s, s:b, s:i}",
        "path", path,
        "master", 1,
        "on_critical_error", 0
    );
    json_t *tranger = tranger_startup(
        jn_tranger // owned
    );

    /*------------------------------*
     *  Check treedb internals
     *------------------------------*/
    json_t *topic_cols_desc =_treedb_create_topic_cols_desc();
    if(!test_treedb_schema(
        tranger,
        topic_cols_desc,
        arguments.without_ok_tests,
        arguments.without_bad_tests,
        arguments.show_oks,
        arguments.verbose
    )) {
        ret += -1;
    }

    /*------------------------------*
     *      Check treedb sample
     *------------------------------*/
    helper_quote2doublequote(schema_sample);
    json_t *jn_schema_sample = legalstring2json(schema_sample, TRUE);
    if(!jn_schema_sample) {
        printf("Can't decode schema_sample json\n");
        exit(-1);
    }

    const char *treedb_name = "treedb_test";
    treedb_open_db(
        tranger,  // owned
        treedb_name,
        jn_schema_sample,
        0
    );

    if(!check_log_result("clean start", arguments.verbose)) { // Be sure there is no previous error
        ret += -1;
    }

    /*------------------------------*
     *  Ejecuta los tests
     *------------------------------*/
    if(!test_schema(tranger, topic_cols_desc, treedb_name, arguments.verbose)) {
        ret += -1;
    }

    if(!test_departments(
            tranger,
            treedb_name,
            arguments.without_ok_tests,
            arguments.without_bad_tests,
            arguments.show_oks,
            arguments.verbose
        )) {
        ret += -1;
    }

    if(!test_users(
            tranger,
            treedb_name,
            arguments.without_ok_tests,
            arguments.without_bad_tests,
            arguments.show_oks,
            arguments.verbose
        )) {
        ret += -1;
    }

    /*
     *  Check refcounts
     */
    int result = 0;
    json_check_refcounts(tranger, 1000, &result);

//print_json(tranger);

    /*---------------------------------------*
     *      Close and re-open the treedb
     *---------------------------------------*/
    if(1) {
        treedb_close_db(tranger, treedb_name);
        const char *test = "Load treedb from tranger";
        set_expected_results(
            test,
            json_pack("[]"  // error's list
            ),
            arguments.verbose
        );

        jn_schema_sample = legalstring2json(schema_sample, TRUE);

        treedb_open_db(
            tranger,  // owned
            treedb_name,
            jn_schema_sample,
            "persistent"
        );

        if(tranger_topic_size(tranger_topic(tranger, "departments")) != 10) {
            // Comprueba que no se ha añadido ningún nodo nuevo en la carga
            if(arguments.verbose) {
                printf("%s  --> ERROR departments!=10%s\n", On_Red BWhite,Color_Off);
                int idx; json_t *value;
                printf("      Unexpected error:\n");
                json_array_foreach(unexpected_log_messages, idx, value) {
                    printf("          \"%s\"\n", kw_get_str(value, "msg", "?", 0));
                }
            } else {
                printf("%sX%s", On_Red BWhite,Color_Off);
            }
            ret += -1;
        }

        if(tranger_topic_size(tranger_topic(tranger, "users")) != 19) {
            // Comprueba que no se ha añadido ningún nodo nuevo en la carga
            if(arguments.verbose) {
                printf("%s  --> ERROR users!=19 %s\n", On_Red BWhite,Color_Off);
                int idx; json_t *value;
                printf("      Unexpected error:\n");
                json_array_foreach(unexpected_log_messages, idx, value) {
                    printf("          \"%s\"\n", kw_get_str(value, "msg", "?", 0));
                }
            } else {
                printf("%sX%s", On_Red BWhite,Color_Off);
            }
            ret += -1;
        }

        if(!check_log_result(test, arguments.verbose)) {
            ret += -1;
        } else {
            if(!test_final_foto(
                    tranger,
                    treedb_name,
                    arguments.without_ok_tests,
                    arguments.without_bad_tests,
                    arguments.show_oks,
                    arguments.verbose
                )) {
                ret += -1;
            }
        }
    }

    /*---------------------------------------*
     *      Link compound node
     *---------------------------------------*/
    if(1) {
        if(!test_compound(
                tranger,
                treedb_name,
                arguments.without_ok_tests,
                arguments.without_bad_tests,
                arguments.show_oks,
                arguments.verbose
            )) {
            ret += -1;
        }
        const char *test = "tranger match";

        set_expected_results(
            test,
            json_pack("[]"  // error's list
            ),
            arguments.verbose
        );
        if(tranger_topic_size(tranger_topic(tranger, "departments")) != 13) {
            // Comprueba que no se ha añadido ningún nodo nuevo en la carga
            if(arguments.verbose) {
                printf("%s  --> ERROR departments!=13%s\n", On_Red BWhite,Color_Off);
                int idx; json_t *value;
                printf("      Unexpected error:\n");
                json_array_foreach(unexpected_log_messages, idx, value) {
                    printf("          \"%s\"\n", kw_get_str(value, "msg", "?", 0));
                }
            } else {
                printf("%sX%s", On_Red BWhite,Color_Off);
            }
            ret += -1;
        }

        if(tranger_topic_size(tranger_topic(tranger, "users")) != 24) {
            // Comprueba que no se ha añadido ningún nodo nuevo en la carga
            if(arguments.verbose) {
                printf("%s  --> ERROR users!=24 %s\n", On_Red BWhite,Color_Off);
                int idx; json_t *value;
                printf("      Unexpected error:\n");
                json_array_foreach(unexpected_log_messages, idx, value) {
                    printf("          \"%s\"\n", kw_get_str(value, "msg", "?", 0));
                }
            } else {
                printf("%sX%s", On_Red BWhite,Color_Off);
            }
            ret += -1;
        }

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

        if(!match_tranger_record(
                tranger,
                "users",                // topic
                24,                     // rowid
                0,                      // uflag
                0x3000001,              // sflag
                "xxxxxxxxxxxxxxxxxxx",  // key
                expected
            )) {
            ret += -1;
        }
        json_decref(expected);

        expected = json_pack(
            "{s:s, s:s, s:s, s:{}, s:[s], s:{}}",
            "id", "administration",
            "name", "Administración",
            "department_id", "departments^direction^departments",
            "departments",
            "users", "departments^operation^managers",
            "managers"
        );

        if(!match_tranger_record(
                tranger,
                "departments",      // topic
                13,                 // rowid
                0,                  // uflag
                0x1000001,          // sflag
                "administration",   // key
                expected
            )) {
            ret += -1;
        }
        json_decref(expected);

        if(!check_log_result(test, arguments.verbose)) {
            ret += -1;
        }
    }

    /*---------------------------------------*
     *      Delete node with links
     *---------------------------------------*/
    if(1) {
        const char *test = "Delete operation, a node with links";

        set_expected_results(
            test,
            json_pack("[{s:s}, {s:s}]",  // error's list
                "msg", "Cannot delete node: has down links",
                "msg", "Cannot delete node: has up links"
            ),
            arguments.verbose
        );

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

        if(!check_log_result(test, arguments.verbose)) {
            ret += -1;
        }
    }

    if(1) {
        const char *test = "Delete administration, a node with links";

        set_expected_results(
            test,
            json_pack("[{s:s}, {s:s}]",  // error's list
                "msg", "Cannot delete node: has down links",
                "msg", "Cannot delete node: has up links"
            ),
            arguments.verbose
        );

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

        if(!check_log_result(test, arguments.verbose)) {
            ret += -1;
        }
    }

    /*
     *  Check refcounts
     */
    result = 0;
    json_check_refcounts(tranger, 1000, &result);

    /*---------------------------------------*
     *      Shutdown
     *---------------------------------------*/
    if(arguments.print_tranger) {
        print_json(tranger);
    } else if(arguments.print_treedb) {
        print_json(kw_get_dict(tranger, "treedbs", 0, KW_REQUIRED));
    }

    if(1) {
        const char *test = "Close and shutdown";
        set_expected_results(
            test,
            json_pack("[]"  // error's list
            ),
            arguments.verbose
        );

        /*------------------------------*
         *  Cierra la bbdd
         *------------------------------*/
        treedb_close_db(tranger, treedb_name);

        tranger_shutdown(tranger);

        /*---------------------------*
         *      Destroy all
         *---------------------------*/
        if(!check_log_result(test, arguments.verbose)) {
            ret += -1;
        }
    }

    printf("\n");

    JSON_DECREF(topic_cols_desc);
    JSON_DECREF(expected_log_messages);
    JSON_DECREF(unexpected_log_messages);
    log_del_handler("test_capture");
    gbmem_shutdown();

    end_ghelpers_library();

    return ret;
}
