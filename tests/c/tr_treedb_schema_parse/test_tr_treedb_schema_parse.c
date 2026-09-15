/****************************************************************************
 *          test_tr_treedb_schema_parse.c
 *
 *  Regression coverage for parse_schema() on a column that declares no
 *  `flag`.
 *
 *  Every user column is validated against the `cols` topic of
 *  treedb_system_schema, where `flag` is an `enum` that is NOT `required`.
 *  A column without `flag` reached check_desc_field() with its value NULL,
 *  skipped the `required` test, and the `enum` branch switched on
 *  json_typeof(NULL): SIGSEGV, reachable from C_TREEDB's `create-topic` and
 *  from `open-treedb` with a schema. An absent value that is not required
 *  has nothing to check, as the branch for the basic json types already
 *  said.
 *
 *      1. a column without `flag` parses clean
 *      2. a column with an unknown flag is still refused
 *      3. a treedb with the flag-less column opens and takes a record
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <limits.h>

#include <gobj.h>
#include <kwid.h>
#include <helpers.h>
#include <timeranger2.h>
#include <tr_treedb.h>
#include <testing.h>

#define APP         "test_tr_treedb_schema_parse"
#define DATABASE    "tr_treedb_schema_parse"
#define TREEDB_NAME "treedb_schema_parse"
#define TOPIC_NAME  "notes"

/***************************************************************
 *              Data
 ***************************************************************/
/*
 *  `text` declares no flag at all.
 */
static char schema_no_flag[]= "\
{                                                                   \n\
    'id': 'treedb_schema_parse',                                    \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'notes',                                          \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'text': {                                           \n\
                    'header': 'Text',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string'                                \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/*
 *  `text` declares a flag that does not exist.
 */
static char schema_bad_flag[]= "\
{                                                                   \n\
    'id': 'treedb_schema_parse',                                    \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'notes',                                          \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'text': {                                           \n\
                    'header': 'Text',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'bogus']                 \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/*
 *  `children` is both the hook of the tree and an fkey: refused.
 */
static char schema_hook_fkey[]= "\
{                                                                   \n\
    'id': 'treedb_schema_parse',                                    \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'notes',                                          \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'parent': {                                         \n\
                    'header': 'Parent',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'children': {                                       \n\
                    'header': 'Children',                           \n\
                    'fillspace': 10,                                \n\
                    'type': 'array',                                \n\
                    'flag': ['hook', 'fkey'],                       \n\
                    'hook': {                                       \n\
                        'notes': 'parent'                           \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/*
 *  One hook on the `parent` fkey: parsed twice, it must stay clean.
 */
static char schema_one_hook[]= "\
{                                                                   \n\
    'id': 'treedb_schema_parse',                                    \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'notes',                                          \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'parent': {                                         \n\
                    'header': 'Parent',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'children': {                                       \n\
                    'header': 'Children',                           \n\
                    'fillspace': 10,                                \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'notes': 'parent'                           \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/*
 *  Two hooks on the same `parent` fkey: an fkey answers to ONE hook.
 */
static char schema_two_hooks[]= "\
{                                                                   \n\
    'id': 'treedb_schema_parse',                                    \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'notes',                                          \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'parent': {                                         \n\
                    'header': 'Parent',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'children': {                                       \n\
                    'header': 'Children',                           \n\
                    'fillspace': 10,                                \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'notes': 'parent'                           \n\
                    }                                               \n\
                },                                                  \n\
                'others': {                                         \n\
                    'header': 'Others',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'notes': 'parent'                           \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *load_schema(char *literal)
{
    helper_quote2doublequote(literal);
    json_t *jn_schema = legalstring2json(literal, TRUE);
    if(!jn_schema) {
        printf("%sERROR%s --> cannot decode a schema literal\n", On_Red BWhite, Color_Off);
    }
    return jn_schema;
}

/***************************************************************************
 *  1 + 2: parse_schema() alone
 ***************************************************************************/
PRIVATE int test_parse(void)
{
    int result = 0;

    set_expected_results("a column without flag parses clean", NULL, NULL, NULL, 1);
    json_t *jn_schema = load_schema(schema_no_flag);
    if(!jn_schema) {
        return -1;
    }
    int ret = parse_schema(jn_schema);
    if(ret != 0) {
        printf("%sERROR%s --> parse_schema() of a flag-less column: %d, expected 0\n",
            On_Red BWhite, Color_Off, ret);
        result += -1;
    }
    JSON_DECREF(jn_schema)
    result += test_json(NULL);

    set_expected_results(
        "an unknown flag is refused",
        json_pack("[{s:s}]", "msg", "Wrong enum type"),
        NULL, NULL, 1
    );
    jn_schema = load_schema(schema_bad_flag);
    if(!jn_schema) {
        return -1;
    }
    ret = parse_schema(jn_schema);
    if(ret >= 0) {
        printf("%sERROR%s --> parse_schema() of an unknown flag: %d, expected < 0\n",
            On_Red BWhite, Color_Off, ret);
        result += -1;
    }
    JSON_DECREF(jn_schema)
    result += test_json(NULL);

    set_expected_results(
        "a column that is both hook and fkey is refused",
        json_pack("[{s:s}]", "msg", "A column cannot be both 'hook' and 'fkey'"),
        NULL, NULL, 1
    );
    jn_schema = load_schema(schema_hook_fkey);
    if(!jn_schema) {
        return -1;
    }
    ret = parse_schema(jn_schema);
    if(ret >= 0) {
        printf("%sERROR%s --> parse_schema() of a hook+fkey column: %d, expected < 0\n",
            On_Red BWhite, Color_Off, ret);
        result += -1;
    }
    JSON_DECREF(jn_schema)
    result += test_json(NULL);

    /*
     *  parse_hooks() writes `fkey` into the schema it parses, and a schema
     *  is parsed at validation and again at open: the second pass meets its
     *  own mark, and that is not a second hook.
     */
    set_expected_results("one hook, parsed twice, stays clean", NULL, NULL, NULL, 1);
    jn_schema = load_schema(schema_one_hook);
    if(!jn_schema) {
        return -1;
    }
    for(int pass = 1; pass <= 2; pass++) {
        ret = parse_schema(jn_schema);
        if(ret != 0) {
            printf("%sERROR%s --> parse_schema() pass %d of a one-hook schema: %d, expected 0\n",
                On_Red BWhite, Color_Off, pass, ret);
            result += -1;
        }
    }
    JSON_DECREF(jn_schema)
    result += test_json(NULL);

    set_expected_results(
        "two hooks on one fkey are refused",
        json_pack("[{s:s}]", "msg", "Only can be one fkey"),
        NULL, NULL, 1
    );
    jn_schema = load_schema(schema_two_hooks);
    if(!jn_schema) {
        return -1;
    }
    ret = parse_schema(jn_schema);
    if(ret >= 0) {
        printf("%sERROR%s --> parse_schema() of two hooks on one fkey: %d, expected < 0\n",
            On_Red BWhite, Color_Off, ret);
        result += -1;
    }
    JSON_DECREF(jn_schema)
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  3: a treedb with the flag-less column opens and takes a record
 ***************************************************************************/
PRIVATE int test_open(void)
{
    int result = 0;
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    set_expected_results(
        "open tranger",
        json_pack("[{s:s}]", "msg", "Creating __timeranger2__.json"),
        NULL, NULL, 1
    );
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);
    result += test_json(NULL);
    if(!tranger) {
        return -1;
    }

    /*
     *  notes + __snaps__ + __graphs__ + __assets__
     */
    set_expected_results(
        "open a treedb with a flag-less column",
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *jn_schema = load_schema(schema_no_flag);
    if(!jn_schema || !treedb_open_db(tranger, TREEDB_NAME, jn_schema, 0)) {
        printf("%sERROR%s --> cannot open a treedb with a flag-less column\n",
            On_Red BWhite, Color_Off);
        tranger2_shutdown(tranger);
        return -1;
    }
    result += test_json(NULL);

    set_expected_results("the topic takes a record", NULL, NULL, NULL, 1);
    json_t *node = treedb_create_node( // Return is NOT YOURS
        tranger,
        TREEDB_NAME,
        TOPIC_NAME,
        json_pack("{s:s, s:s}", "id", "n1", "text", "hello")
    );
    if(!node) {
        printf("%sERROR%s --> cannot create a node in the flag-less topic\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  `create-topic` is a live command: the refusal must stop the topic,
     *  not only log it.
     */
    set_expected_results(
        "create-topic refuses a hook+fkey column",
        json_pack("[{s:s}, {s:s}]",
            "msg", "A column cannot be both 'hook' and 'fkey'",
            "msg", "Topic refused: a column is both 'hook' and 'fkey'"
        ),
        NULL, NULL, 1
    );
    json_t *topic = treedb_create_topic(
        tranger,
        TREEDB_NAME,
        "bad_notes",
        1,
        "",
        0,
        json_pack("{s:{s:s, s:s, s:[s,s]}, s:{s:s, s:s, s:[s,s], s:{s:s}}}",
            "id",
                "header", "Id", "type", "string", "flag", "persistent", "required",
            "children",
                "header", "Children", "type", "array", "flag", "hook", "fkey",
                "hook", "bad_notes", "id"
        ),
        0,
        FALSE,
        FALSE
    );
    if(topic) {
        printf("%sERROR%s --> treedb_create_topic() accepted a hook+fkey column\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("close and shutdown", NULL, NULL, NULL, 1);
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    int result = 0;
    result += test_parse();
    result += test_open();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    } else {
        printf("<-- %sTEST OK%s: %s\n", On_Green BWhite, Color_Off, APP);
    }
    return result < 0? -1 : 0;
}
