/****************************************************************************
 *          test_attr_types.c
 *
 *          A value written to an attribute of a gclass (gobj_create() from
 *          the config, gobj_write_attr()) is read as its declared type by
 *          the grammar of the command parser (command_parameter_from_json()):
 *          - a json STRING given to a DTP_BOOLEAN, DTP_INTEGER, DTP_REAL,
 *            DTP_LIST or DTP_DICT is read from its text: true/false,
 *            yes/no, on/off or an integer for a boolean, a JSON integer, a
 *            JSON number, the json of a list or of a dict;
 *          - a number or a boolean as the exact type: an integer to a real
 *            is its real, a boolean to an integer 1 or 0, 2.0 to an integer
 *            2;
 *          - what does not read as the type (12abc, 2.7 to an integer, an
 *            object to an integer, nan, maybe, a dict to a list, an empty
 *            text, a null) is refused with an ERROR that names the
 *            attribute, its type and why, the value masked and capped (a
 *            SDF_SECRET one never shown), and the attribute keeps its value.
 *          And the table of what a value given as json becomes, which
 *          gobj-js reads too (kernel/js/gobj-js/tests/fixtures/
 *          attr_types_parity.json): C and JS read an attribute value the
 *          same way. The table is read from the source tree this test was
 *          built from (ATTR_TYPES_PARITY_TABLE, set by its CMakeLists.txt),
 *          not from wherever YUNETAS_BASE points. A tree without the
 *          gobj-js submodule (a tarball of yunetas alone) has no table: the
 *          other checks run, and then the test exits SKIP_RETURN_CODE (77),
 *          which ctest reports as Skipped, not as Passed.
 *          A null given to a DTP_STRING is "", the convention of the
 *          default of a string attr and of empty_string() (and of gobj-js):
 *          it was the text "null", which empty_string() reads as a value.
 *          Up to 7.26.9 a string was read from its JSON dump, quotes
 *          included: "42" was 0, "true" false, "2.5" 0.0, and a list or a
 *          dict given as text was always refused; an integer to a real, a
 *          boolean to an integer, were refused.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <yunetas.h>

#define APP_NAME        "test_attr_types"
#define APP_DOC         "A value written to an attribute, read as its type"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0       // use default
#define MEM_MAX_BLOCK           0       // use default
#define MEM_SUPERBLOCK          0       // use default
#define MEM_MAX_SYSTEM_MEMORY   0       // use default

#define REFUSED "Attribute value of the wrong type, not written"

#define SKIP_RETURN_CODE        77      // the same in CMakeLists.txt

#ifndef ATTR_TYPES_PARITY_TABLE
    #error "ATTR_TYPES_PARITY_TABLE must be defined by CMakeLists.txt"
#endif

/***************************************************************
 *              Data
 ***************************************************************/
GOBJ_DEFINE_GCLASS(C_TEST_ATTR_TYPES);

typedef struct {
    hgobj   timer;
} PRIVATE_DATA;

PRIVATE int result = 0;
PRIVATE BOOL parity_skipped = FALSE;
PRIVATE int checks_failed = 0;
PRIVATE int secret_seen = 0;    // "hunter2" in any log line

/***************************************************************
 *              Config (single quotes -> double at runtime)
 ***************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"APP_NAME"',                                  \n\
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'console_log_handlers': {},                                 \n\
        'daemon_log_handlers': {}                                   \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'autoplay': true,                                           \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {},                                   \n\
        'trace_levels': {}                                          \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'attr_types',                                   \n\
            'gclass': 'C_TEST_ATTR_TYPES',                          \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'cfg_flag': 'yes',                                  \n\
                'cfg_count': '42',                                  \n\
                'cfg_ratio': '2.5',                                 \n\
                'cfg_items': '[1, 2]',                              \n\
                'cfg_settings': '{\\'a\\': 1}',                     \n\
                'cfg_bad': '12abc',                                 \n\
                'cfg_name': null                                    \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************
 *              Checks
 ***************************************************************/
PRIVATE void check(BOOL ok, const char *name)
{
    if(ok) {
        printf("ok   %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        checks_failed++;
    }
}

PRIVATE void check_json(hgobj gobj, const char *attr, const char *expected, const char *name)
{
    json_t *jn_expected = anystring2json(expected, strlen(expected), TRUE);
    json_t *jn_got = gobj_read_attr(gobj, attr, 0);
    BOOL ok = jn_expected && jn_got && json_equal(jn_got, jn_expected) &&
        json_typeof(jn_got) == json_typeof(jn_expected);
    check(ok, name);
    if(!ok) {
        char *s = json2uglystr(jn_got);
        printf("     %s is %s, expected %s\n", attr, s? s : "NULL", expected);
        GBMEM_FREE(s)
    }
    JSON_DECREF(jn_expected)
}

/*
 *  A write that must be refused: -1, and the attribute as it was
 */
PRIVATE void check_refused(hgobj gobj, const char *attr, json_t *value, const char *name)
{
    json_t *jn_before = json_deep_copy(gobj_read_attr(gobj, attr, 0));
    int ret = gobj_write_attr(gobj, attr, value, gobj);
    json_t *jn_after = gobj_read_attr(gobj, attr, 0);
    check(ret < 0 && json_equal(jn_before, jn_after), name);
    JSON_DECREF(jn_before)
}

PRIVATE void check_written(hgobj gobj, const char *attr, json_t *value, const char *expected, const char *name)
{
    int ret = gobj_write_attr(gobj, attr, value, gobj);
    if(ret < 0) {
        check(FALSE, name);
        printf("     gobj_write_attr() answered %d\n", ret);
        return;
    }
    check_json(gobj, attr, expected, name);
}

/*
 *  The table of what a value given as json becomes for each type, the
 *  same in C and in JS: gobj-js reads it in tests/attr_types.test.js
 */
PRIVATE void check_parity_table(void)
{
    const char *path = ATTR_TYPES_PARITY_TABLE;
    if(access(path, F_OK) != 0 && errno == ENOENT) {
        /*
         *  The table lives in the gobj-js submodule: a source tree without
         *  it (a tarball of yunetas alone) cannot compare C with JS
         */
        printf("SKIP the parity table of gobj-js: %s not found (the kernel/js/gobj-js submodule is not checked out)\n", path);
        parity_skipped = TRUE;
        return;
    }
    json_error_t error;
    json_t *jn_table = json_load_file(path, JSON_ALLOW_NUL, &error);
    json_t *jn_cases = json_object_get(jn_table, "cases");
    if(!json_is_array(jn_cases) || json_array_size(jn_cases) == 0) {
        check(FALSE, "the parity table of gobj-js is read");
        printf("     %s: %s\n", path, error.text);
        JSON_DECREF(jn_table)
        return;
    }

    static const struct {
        const char *name;
        data_type_t type;
    } types[] = {
        {"boolean", DTP_BOOLEAN},
        {"integer", DTP_INTEGER},
        {"real",    DTP_REAL},
        {"list",    DTP_LIST},
        {"dict",    DTP_DICT},
        {"json",    DTP_JSON},
        {0, 0}
    };

    int failed = 0;
    size_t idx;
    json_t *jn_case;
    json_array_foreach(jn_cases, idx, jn_case) {
        const char *type_name = json_string_value(json_object_get(jn_case, "type"));
        data_type_t type = 0;
        for(int i=0; types[i].name; i++) {
            if(type_name && strcmp(types[i].name, type_name)==0) {
                type = types[i].type;
            }
        }
        json_t *jn_value = json_object_get(jn_case, "value");
        json_t *jn_expect = json_object_get(jn_case, "expect");
        json_t *jn_refused = json_object_get(jn_case, "refused");

        const char *why = NULL;
        json_t *jn_got = type? command_parameter_from_json(type, jn_value, &why) : NULL;
        BOOL ok;
        if(jn_expect) {
            ok = jn_got && json_equal(jn_got, jn_expect) && json_typeof(jn_got) == json_typeof(jn_expect);
        } else if(json_is_null(jn_refused)) {
            ok = !jn_got && !why;
        } else {
            ok = !jn_got && why && strcmp(why, json_string_value(jn_refused))==0;
        }
        if(!ok) {
            char *v = json2uglystr(jn_value);
            char *g = jn_got? json2uglystr(jn_got) : NULL;
            printf("FAIL parity case %zu: %s given %s: got %s, refused '%s'\n",
                idx, type_name? type_name : "?", v? v : "?", g? g : "nothing", why? why : "");
            GBMEM_FREE(v)
            GBMEM_FREE(g)
            failed++;
        }
        JSON_DECREF(jn_got)
    }
    char name[128];
    snprintf(name, sizeof(name), "the parity table of gobj-js: %zu cases, %d differ",
        json_array_size(jn_cases), failed);
    check(failed == 0, name);
    JSON_DECREF(jn_table)
}

PRIVATE void run_checks(hgobj gobj)
{
    check_parity_table();

    /*
     *  From the config, every value given as text
     */
    check_json(gobj, "cfg_flag", "true", "config: 'yes' to a boolean is true");
    check_json(gobj, "cfg_count", "42", "config: '42' to an integer is 42");
    check_json(gobj, "cfg_ratio", "2.5", "config: '2.5' to a real is 2.5");
    check_json(gobj, "cfg_items", "[1, 2]", "config: '[1, 2]' to a list is the list");
    check_json(gobj, "cfg_settings", "{\"a\": 1}", "config: '{\"a\": 1}' to a dict is the dict");
    check_json(gobj, "cfg_bad", "3", "config: '12abc' to an integer is refused, the default stays");
    check_json(gobj, "cfg_name", "\"\"", "config: a null to a string is the empty string, not the text 'null'");

    /*
     *  Text, written
     */
    check_written(gobj, "flag", json_string("on"), "true", "'on' to a boolean is true");
    check_written(gobj, "flag", json_string("0"), "false", "'0' to a boolean is false");
    check_written(gobj, "flag", json_string("TRUE"), "true", "'TRUE' to a boolean is true");
    check_written(gobj, "count", json_string("-5"), "-5", "'-5' to an integer is -5");
    check_written(gobj, "ratio", json_string("1E3"), "1000.0", "'1E3' to a real is 1000.0");
    check_written(gobj, "items", json_string("[\"a\"]"), "[\"a\"]", "'[\"a\"]' to a list");
    check_written(gobj, "settings", json_string("{\"b\": 2}"), "{\"b\": 2}", "'{\"b\": 2}' to a dict");

    /*
     *  A number or a boolean, as the exact type
     */
    check_written(gobj, "ratio", json_integer(3), "3.0", "an integer to a real is its real");
    check_written(gobj, "count", json_true(), "1", "true to an integer is 1");
    check_written(gobj, "count", json_real(2.0), "2", "2.0 to an integer is 2");
    check_written(gobj, "flag", json_integer(0), "false", "0 to a boolean is false");
    check_written(gobj, "name", json_null(), "\"\"", "a null to a string is the empty string, as in JS");
    check_written(gobj, "name", json_integer(8080), "\"8080\"", "a number to a string is its text");

    /*
     *  Refused, the attribute as it was. Each one is an ERROR (the FIFO of
     *  set_expected_results())
     */
    check_refused(gobj, "count", json_string("12abc"), "'12abc' to an integer: refused");
    check_refused(gobj, "count", json_real(2.7), "2.7 to an integer: refused");
    check_refused(gobj, "count", json_object(), "an object to an integer: refused");
    check_refused(gobj, "ratio", json_string("nan"), "'nan' to a real: refused");
    check_refused(gobj, "flag", json_string("maybe"), "'maybe' to a boolean: refused");
    check_refused(gobj, "items", json_string("{\"a\": 1}"), "a dict as text to a list: refused");
    check_refused(gobj, "settings", json_string("[1]"), "a list as text to a dict: refused");
    check_refused(gobj, "settings", json_integer(5), "an integer to a dict: refused");
    check_refused(gobj, "count", json_string(""), "an empty text to an integer: refused");
    check_refused(gobj, "count", json_null(), "a null to an integer: refused");
    check_refused(gobj, "pin", json_string("pin-hunter2"), "a text to a secret integer: refused");
    check_refused(gobj, "settings", json_string("{\"password\": \"cut-hunter2"),
        "a dict cut short: refused");

    check(secret_seen == 0, "no value of a secret in the log");
}

/***************************************************************
 *              Framework Methods
 ***************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    return 0;
}

PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    return 0;
}

PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 10);   // the yuno dies once it is playing, not inside its play
    return 0;
}

/***************************************************************
 *              Actions
 ***************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    run_checks(gobj);
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", checks_failed? "attr types: checks FAILED" : "attr types: checks ok",
        "failed",       "%d", checks_failed,
        NULL
    );
    set_yuno_must_die();
    KW_DECREF(kw)
    return 0;
}

/***************************************************************
 *              GClass
 ***************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag--------------------default-description---------- */
SDATA (DTP_BOOLEAN,     "flag",             SDF_WR,                 "false", "A boolean"),
SDATA (DTP_INTEGER,     "count",            SDF_WR,                 "7",    "An integer"),
SDATA (DTP_REAL,        "ratio",            SDF_WR,                 "0.5",  "A real"),
SDATA (DTP_LIST,        "items",            SDF_WR,                 "[]",   "A list"),
SDATA (DTP_DICT,        "settings",         SDF_WR,                 "{}",   "A dict"),
SDATA (DTP_INTEGER,     "pin",              SDF_WR|SDF_SECRET,      "0",    "A secret that is a number"),
SDATA (DTP_BOOLEAN,     "cfg_flag",         SDF_RD,                 "false", "Given as text by the config"),
SDATA (DTP_INTEGER,     "cfg_count",        SDF_RD,                 "0",    "Given as text by the config"),
SDATA (DTP_REAL,        "cfg_ratio",        SDF_RD,                 "0",    "Given as text by the config"),
SDATA (DTP_LIST,        "cfg_items",        SDF_RD,                 "[]",   "Given as text by the config"),
SDATA (DTP_DICT,        "cfg_settings",     SDF_RD,                 "{}",   "Given as text by the config"),
SDATA (DTP_INTEGER,     "cfg_bad",          SDF_RD,                 "3",    "Given by the config a text that is no integer"),
SDATA (DTP_STRING,      "cfg_name",         SDF_RD,                 "x",    "Given a null by the config"),
SDATA (DTP_STRING,      "name",             SDF_WR,                 "x",    "A string"),
SDATA_END()
};

PRIVATE int register_c_test_attr_types(void)
{
    static const GMETHODS gmt = {
        .mt_create = mt_create,
        .mt_start = mt_start,
        .mt_stop = mt_stop,
        .mt_play = mt_play,
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,    0},
        {0, 0}
    };

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,    ac_timeout,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,   st_idle},
        {0, 0}
    };

    hgclass gc = gclass_create(
        C_TEST_ATTR_TYPES,
        event_types,
        states,
        &gmt,
        0,                          // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,                          // authz_table
        0,                          // command_table
        0,                          // s_user_trace_level
        0                           // gclass_flag
    );
    return gc ? 0 : -1;     // Error already logged
}

/***************************************************************************
 *  A log handler that only looks for the secret
 ***************************************************************************/
PRIVATE int watch_secret(void *h, int priority, const char *bf, size_t len)
{
    if(strstr(bf, "hunter2")) {
        secret_seen++;
    }
    return 0;
}

PRIVATE int register_yuno_and_more(void)
{
    int ret = register_c_test_attr_types();

    /*
     *  Each refusal is an ERROR naming the attribute; the value as a log
     *  may show it: a secret's never, any other masked
     */
    set_expected_results(
        APP_NAME,
        json_pack("[{s:s}, {s:s, s:s, s:s, s:s}, {s:s}, "
            "{s:s, s:s, s:s, s:s}, {s:s, s:s, s:s, s:s}, {s:s, s:s, s:s, s:s}, {s:s, s:s, s:s, s:s}, "
            "{s:s, s:s, s:s, s:s}, {s:s, s:s, s:s, s:s}, {s:s, s:s, s:s, s:s}, {s:s, s:s, s:s, s:s}, "
            "{s:s, s:s, s:s, s:s}, {s:s, s:s, s:s, s:s}, {s:s, s:s, s:s, s:s}, {s:s, s:s, s:s}, "
            "{s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Starting yuno",
            "msg", REFUSED, "attr", "cfg_bad", "value", "12abc", "__priority__", "ERROR",
            "msg", "Playing yuno",
            "msg", REFUSED, "attr", "count", "value", "12abc", "__priority__", "ERROR",
            "msg", REFUSED, "attr", "count", "value", "2.7", "__priority__", "ERROR",
            "msg", REFUSED, "attr", "count", "value", "{}", "__priority__", "ERROR",
            "msg", REFUSED, "attr", "ratio", "value", "nan", "__priority__", "ERROR",
            "msg", REFUSED, "attr", "flag", "value", "maybe", "__priority__", "ERROR",
            "msg", REFUSED, "attr", "items", "value", "{\"a\": 1}", "__priority__", "ERROR",
            "msg", REFUSED, "attr", "settings", "value", "[1]", "__priority__", "ERROR",
            "msg", REFUSED, "attr", "settings", "value", "5", "__priority__", "ERROR",
            "msg", REFUSED, "attr", "count", "value", "", "__priority__", "ERROR",
            "msg", REFUSED, "attr", "count", "value", "null", "__priority__", "ERROR",
            "msg", REFUSED, "attr", "pin", "value", "********", "__priority__", "ERROR",
            "msg", REFUSED, "attr", "settings", "__priority__", "ERROR",
            "msg", "attr types: checks ok",
            "msg", "Exit to die",
            "msg", "Pausing yuno",
            "msg", "Yuno stopped, gobj end"
        ),
        NULL,   // expected: only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    return ret;
}

PRIVATE void cleaning(void)
{
    result += test_json(NULL);
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    glog_init();
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);
    gobj_log_register_handler("watch_secret", 0, watch_secret, 0);
    gobj_log_add_handler("watch_secret", "watch_secret", LOG_OPT_ALL, 0);

    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs
        NULL,       // command_parser
        NULL,       // stats_parser
        NULL,       // authz_checker
        NULL,       // authentication_parser
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    result += yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        cleaning
    );

    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP_NAME);
        return -1;
    }
    if(parity_skipped) {
        printf("<-- SKIPPED (the parity table with gobj-js): %s\n", APP_NAME);
        return SKIP_RETURN_CODE;
    }
    return 0;
}
