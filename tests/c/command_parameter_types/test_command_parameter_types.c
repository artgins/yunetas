/****************************************************************************
 *          test_command_parameter_types.c
 *
 *          What a command parameter takes, whatever gives it: the command
 *          line (text), kw (json) or its declared default (REV-CLOUD V20.4).
 *
 *          A value of another shape is refused before the handler runs: an
 *          ERROR with a stack when C code calls the command, a WARNING when
 *          a peer does (kw carries `__username__`), and the answer -1 that
 *          names the parameter. Up to 7.26.9 the value went to the handler
 *          as it came, and its kw_get_*() logged the caller's mistake as an
 *          ERROR -- or read it as something else: `"treedb_name": 5` saved
 *          every treedb, and `dry_run=yes` was atoi()'d to FALSE.
 *
 *          The rules checked, one case each:
 *            - a string, an object, an array: that shape, or refused;
 *            - an integer, a real, a boolean: a number or a boolean, never
 *              an object or an array; text read as one (a boolean as
 *              true/false, yes/no, on/off or a number), or refused;
 *            - a DTP_JSON: anything; text that is not json is a string,
 *              unless it starts as json and breaks;
 *            - a null is no value: the default, or "is required";
 *            - the line has the last word over a refused kw value.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP "test_command_parameter_types"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int global_result = 0;
PRIVATE json_t *g_seen = 0;     /* a copy of the kw the handler got */
PRIVATE int g_errors = 0;       /* logs of the case, by priority */
PRIVATE int g_warnings = 0;

GOBJ_DEFINE_GCLASS(C_PARAMTYPESTEST);

/***************************************************************
 *              Command handler
 ***************************************************************/
PRIVATE json_t *cmd_probe(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    JSON_DECREF(g_seen)
    g_seen = json_deep_copy(kw);

    KW_DECREF(kw)
    return build_command_response(gobj, 0, json_sprintf("ok"), 0, 0);
}

/***************************************************************
 *              GClass scaffolding
 ***************************************************************/
PRIVATE sdata_desc_t pm_typed[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "s",            0,              "dflt",     "A string"),
SDATAPM (DTP_INTEGER,   "n",            0,              "7",        "An integer"),
SDATAPM (DTP_BOOLEAN,   "b",            0,              0,          "A boolean"),
SDATAPM (DTP_REAL,      "r",            0,              0,          "A real"),
SDATAPM (DTP_DICT,      "d",            0,              0,          "A dict"),
SDATAPM (DTP_LIST,      "l",            0,              0,          "A list"),
SDATAPM (DTP_JSON,      "j",            0,              0,          "Any json"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_required[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_BOOLEAN,   "rb",           SDF_REQUIRED,   0,          "A required boolean"),
SDATAPM (DTP_STRING,    "rs",           SDF_REQUIRED,   0,          "A required string"),
SDATA_END()
};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name------------flag----------alias---items-------json_fn-----description--*/
SDATACM2(DTP_SCHEMA,    "typed",        0,            0,      pm_typed,   cmd_probe,  "A parameter of each type"),
SDATACM2(DTP_SCHEMA,    "required",     0,            0,      pm_required,cmd_probe,  "Required parameters"),
SDATA_END()
};

PRIVATE sdata_desc_t attrs_table[] = {
SDATA_END()
};

PRIVATE const GMETHODS gmt = {0};

PRIVATE int register_paramtypestest(void)
{
    ev_action_t st_idle[] = {
        {0, 0, 0}
    };
    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };
    event_type_t event_types[] = {
        {0, 0}
    };
    hgclass gc = gclass_create(
        C_PARAMTYPESTEST,
        event_types,
        states,
        &gmt,
        0,              // lmt
        attrs_table,
        0,              // priv_size
        0,              // authz_table
        command_table,
        0,              // trace_level
        0               // gclass_flag
    );
    return gc ? 0 : -1;
}

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int capture_logs(void *h, int priority, const char *bf, size_t len)
{
    if(priority <= LOG_ERR) {
        g_errors++;
    } else if(priority == LOG_WARNING) {
        g_warnings++;
    }
    return 0;
}

PRIVATE void check(const char *case_name, const char *what, BOOL ok)
{
    if(!ok) {
        printf("FAIL %-40s %s\n", case_name, what);
        global_result += -1;
    } else {
        printf("ok   %-40s %s\n", case_name, what);
    }
}

/***************************************************************************
 *  Run `command` with `kw` (owned) and say what came of it: the answer's
 *  result, its comment, and the logs it left
 ***************************************************************************/
PRIVATE int run(hgobj yuno, const char *command, json_t *kw, char *comment, size_t comment_size)
{
    JSON_DECREF(g_seen)
    g_errors = 0;
    g_warnings = 0;
    json_t *r = gobj_command(yuno, command, kw, yuno);
    int result = (int)kw_get_int(0, r, "result", -999, 0);
    snprintf(comment, comment_size, "%s", kw_get_str(0, r, "comment", "", 0));
    JSON_DECREF(r)
    return result;
}

/***************************************************************************
 *  A refusal: -1, an answer that says `needle`, the handler not run, and
 *  one log of the expected priority
 ***************************************************************************/
PRIVATE void expect_refused(hgobj yuno, const char *case_name, const char *command,
    json_t *kw, const char *needle, BOOL by_peer)
{
    char comment[512];
    int result = run(yuno, command, kw, comment, sizeof(comment));
    check(case_name, "answers -1", result == -1);
    check(case_name, "names the parameter and the shape", strstr(comment, needle) != NULL);
    if(!strstr(comment, needle)) {
        printf("     comment: %s\n", comment);
    }
    check(case_name, "the handler did not run", g_seen == NULL);
    if(by_peer) {
        check(case_name, "a peer's mistake: one WARNING, no ERROR", g_warnings == 1 && g_errors == 0);
    } else {
        check(case_name, "a C caller's mistake: one ERROR, no WARNING", g_errors == 1 && g_warnings == 0);
    }
}

/***************************************************************************
 *  Accepted, the handler run, nothing logged, and the parameter `name`
 *  reaching it as `expected` (NULL: absent)
 ***************************************************************************/
PRIVATE void expect_value(hgobj yuno, const char *case_name, const char *command,
    json_t *kw, const char *name, json_t *expected)
{
    char comment[512];
    int result = run(yuno, command, kw, comment, sizeof(comment));
    check(case_name, "answers 0", result == 0);
    if(result != 0) {
        printf("     comment: %s\n", comment);
    }
    check(case_name, "nothing logged", g_errors == 0 && g_warnings == 0);
    json_t *got = json_object_get(g_seen, name);
    BOOL same = expected? (got && json_equal(got, expected)) : (got == NULL);
    check(case_name, "the handler gets the value", same);
    if(!same) {
        char *s = got? json_dumps(got, JSON_ENCODE_ANY) : NULL;
        printf("     got: %s\n", s? s : "(absent)");
        if(s) {
            gbmem_free(s);
        }
    }
    JSON_DECREF(expected)
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

    unsigned long memory_check_list[] = {0};
    set_memory_check_list(memory_check_list);

    gobj_start_up(
        argc, argv,
        NULL,                   // jn_global_settings
        NULL,                   // persistent_attrs
        command_parser,         // global_command_parser  <-- function under test
        NULL,                   // global_stats_parser
        NULL,                   // global_authz_checker
        NULL                    // global_authentication_parser
    );
    gobj_log_register_handler("capture_logs", 0, capture_logs, 0);
    gobj_log_add_handler("capture_logs", "capture_logs", LOG_OPT_ALL, 0);
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    if(register_paramtypestest() != 0) {
        printf("%s: FAIL (gclass_create)\n", APP);
        gobj_end();
        return -1;
    }

    hgobj yuno = gobj_create_yuno("paramtypestest_yuno", C_PARAMTYPESTEST, 0);
    if(!yuno) {
        printf("%s: FAIL (gobj_create_yuno)\n", APP);
        gobj_end();
        return -1;
    }

    /*
     *  The shape of a string, an object, an array; who made the mistake
     */
    expect_refused(yuno, "string given a number, by C", "typed",
        json_pack("{s:i}", "s", 5), "parameter 's' must be a string, not an integer", FALSE);
    expect_refused(yuno, "string given a number, by a peer", "typed",
        json_pack("{s:i, s:s}", "s", 5, "__username__", "someone"),
        "parameter 's' must be a string, not an integer", TRUE);
    expect_refused(yuno, "dict given a list", "typed",
        json_pack("{s:[i]}", "d", 1), "parameter 'd' must be a json object, not an array", FALSE);
    expect_refused(yuno, "dict given a list, on the line", "typed d=[1]",
        0, "parameter 'd' must be a json object", FALSE);
    expect_value(yuno, "dict given its json as text", "typed",
        json_pack("{s:s}", "d", "{\"a\": 1}"), "d", json_pack("{s:i}", "a", 1));
    expect_refused(yuno, "list given an object", "typed",
        json_pack("{s:{}}", "l"), "parameter 'l' must be a json array, not an object", FALSE);

    /*
     *  Numbers and booleans
     */
    expect_refused(yuno, "integer given an object", "typed",
        json_pack("{s:{}}", "n"), "parameter 'n' must be an integer, not an object", FALSE);
    expect_value(yuno, "integer given its text", "typed",
        json_pack("{s:s}", "n", "12"), "n", json_integer(12));
    expect_refused(yuno, "integer given other text, on the line", "typed n=12abc",
        0, "parameter 'n' must be an integer", FALSE);
    expect_value(yuno, "integer empty on the line is 0", "typed n=",
        0, "n", json_integer(0));
    expect_value(yuno, "integer empty in kw is no value: the default", "typed",
        json_pack("{s:s}", "n", ""), "n", json_integer(7));
    expect_value(yuno, "real given its text", "typed",
        json_pack("{s:s}", "r", "-3.67"), "r", json_real(-3.67));
    expect_refused(yuno, "real given other text", "typed",
        json_pack("{s:s}", "r", "west"), "parameter 'r' must be a number", FALSE);
    expect_value(yuno, "boolean yes in kw", "typed",
        json_pack("{s:s}", "b", "yes"), "b", json_true());
    expect_value(yuno, "boolean yes on the line", "typed b=yes",
        0, "b", json_true());
    expect_value(yuno, "boolean off on the line", "typed b=off",
        0, "b", json_false());
    expect_value(yuno, "boolean a number on the line", "typed b=1",
        0, "b", json_true());
    expect_refused(yuno, "boolean other text on the line", "typed b=maybe",
        0, "parameter 'b' must be a boolean", FALSE);
    expect_refused(yuno, "boolean other text in kw", "typed",
        json_pack("{s:s}", "b", "maybe"), "parameter 'b' must be a boolean", FALSE);

    /*
     *  DTP_JSON: anything, and text that is not json is a string
     */
    expect_value(yuno, "json given a bare word on the line", "typed j=UPDATE_PASSWORD",
        0, "j", json_string("UPDATE_PASSWORD"));
    expect_value(yuno, "json given its text on the line", "typed j=[1]",
        0, "j", json_pack("[i]", 1));
    expect_value(yuno, "json given a number", "typed",
        json_pack("{s:i}", "j", 5), "j", json_integer(5));
    expect_refused(yuno, "json written wrong on the line", "typed j={\"a\":1",
        0, "parameter 'j' must be a valid json", FALSE);
    expect_value(yuno, "json given a string in kw that is no json", "typed",
        json_pack("{s:s}", "j", "{not json"), "j", json_string("{not json"));

    /*
     *  A null is no value; the line has the last word
     */
    expect_value(yuno, "string given null: the default", "typed",
        json_pack("{s:n}", "s"), "s", json_string("dflt"));
    expect_value(yuno, "boolean given null, no default: absent", "typed",
        json_pack("{s:n}", "b"), "b", NULL);
    expect_value(yuno, "a refused kw value the line gives right", "typed s=five",
        json_pack("{s:i}", "s", 5), "s", json_string("five"));

    /*
     *  Required parameters, given in kw
     */
    expect_value(yuno, "required boolean given its text", "required",
        json_pack("{s:s, s:s}", "rb", "1", "rs", "x"), "rb", json_true());
    {
        char comment[512];
        int result = run(yuno, "required",
            json_pack("{s:n, s:s}", "rb", "rs", "x"), comment, sizeof(comment));
        check("required given null", "answers -1, is required",
            result == -1 && strstr(comment, "parameter 'rb' is required") != NULL);
    }
    expect_refused(yuno, "required string given a list", "required",
        json_pack("{s:b, s:[]}", "rb", 1, "rs"), "parameter 'rs' must be a string, not an array", FALSE);

    JSON_DECREF(g_seen)
    gobj_end();

    size_t leaked = get_cur_system_memory();
    check("memory", "no leak", leaked == 0);

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result;
}
