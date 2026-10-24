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
 *              true/false, yes/no, on/off or an integer), or refused;
 *            - and the handler gets the EXACT type (REV-CLOUD V21.3.2): a
 *              number given to a boolean is true or false, a boolean given
 *              to an integer 1 or 0, a real given to an integer that
 *              integer -- and one with decimals, or out of range, refused
 *              as the text "2.7" is; an integer given to a real a real;
 *            - a number written as text: nothing before or after it, and
 *              in its range ("nan", "inf", an overflow refused: strtoll()
 *              saturated in silence, V21.3.3); the text of a real a
 *              decimal number (no hex) whose underflow is a number, that
 *              of an integer or a boolean an integer (V22.4.3); of the
 *              JSON grammar, ".5", "5.", "+5" and "007" refused, and read
 *              in the "C" locale whatever LC_NUMERIC is (V23.4.6);
 *            - a text of kw with a NUL inside refused, but for a DTP_JSON
 *              (V22.4.2);
 *            - a parameter given in another case on the line reaches the
 *              handler under its declared name (V22.4.1);
 *            - a DTP_JSON: anything; text that is not json is a string,
 *              unless it starts as json and breaks;
 *            - a null, or an empty text for a number, is no value: the
 *              default, or absent -- the kw's "" reached the handler
 *              (V21.3.4) --, or "is required";
 *            - the line has the last word over a refused kw value, and the
 *              refused value does not reach the handler under the name;
 *            - command_parameter_from_text() reads a text as the parser
 *              does (write-attr reads its value as the attribute's type).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <locale.h>
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
    sys_free_fn_t free_func;
    gbmem_get_json_allocators(&malloc_func, &free_func);
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

    /*
     *  The EXACT type: a number to a boolean, a boolean to a number, a real
     *  to an integer (REV-CLOUD V21.3.2). As it came, a `1` reached a
     *  kw_get_bool() that logged an ERROR with a stack and took its default.
     */
    expect_value(yuno, "boolean given the integer 1: true", "typed",
        json_pack("{s:i}", "b", 1), "b", json_true());
    expect_value(yuno, "boolean given the integer 0: false", "typed",
        json_pack("{s:i}", "b", 0), "b", json_false());
    expect_value(yuno, "boolean given a real: true", "typed",
        json_pack("{s:f}", "b", 2.5), "b", json_true());
    expect_value(yuno, "integer given true: 1", "typed",
        json_pack("{s:b}", "n", 1), "n", json_integer(1));
    expect_value(yuno, "integer given a real of no decimals", "typed",
        json_pack("{s:f}", "n", 2.0), "n", json_integer(2));
    expect_refused(yuno, "integer given a real with decimals", "typed",
        json_pack("{s:f}", "n", 2.7), "parameter 'n' must be an integer, not a real with decimals", FALSE);
    expect_refused(yuno, "integer given a real out of its range, by a peer", "typed",
        json_pack("{s:f, s:s}", "n", 1e30, "__username__", "someone"),
        "parameter 'n' must be an integer, not a number out of its range", TRUE);
    expect_value(yuno, "real given an integer: a real", "typed",
        json_pack("{s:i}", "r", 3), "r", json_real(3.0));
    expect_value(yuno, "real given true: 1.0", "typed",
        json_pack("{s:b}", "r", 1), "r", json_real(1.0));

    /*
     *  A number written as text: nothing before or after it, in its range
     *  (REV-CLOUD V21.3.3)
     */
    expect_refused(yuno, "integer text past 64 bits", "typed",
        json_pack("{s:s}", "n", "99999999999999999999"),
        "parameter 'n' must be an integer, not a number out of its range", FALSE);
    expect_refused(yuno, "integer text past 64 bits, on the line", "typed n=99999999999999999999",
        0, "parameter 'n' must be an integer, not a number out of its range", FALSE);
    expect_refused(yuno, "integer text with a blank before", "typed",
        json_pack("{s:s}", "n", " 5"), "parameter 'n' must be an integer, not a text", FALSE);
    expect_refused(yuno, "integer text with a blank after", "typed",
        json_pack("{s:s}", "n", "5 "), "parameter 'n' must be an integer, not a text", FALSE);
    expect_refused(yuno, "real text nan", "typed",
        json_pack("{s:s}", "r", "nan"), "parameter 'r' must be a number, not a text", FALSE);
    expect_refused(yuno, "real text inf", "typed",      // no JSON number (V23.4.6): it was "out of its range"
        json_pack("{s:s}", "r", "inf"), "parameter 'r' must be a number, not a text", FALSE);
    expect_refused(yuno, "real text past a double", "typed",
        json_pack("{s:s}", "r", "1e999"), "parameter 'r' must be a number, not a number out of its range", FALSE);

    /*
     *  An empty text for a number with no default is no value, absent: kw
     *  passed the "" on to the handler (REV-CLOUD V21.3.4)
     */
    expect_value(yuno, "real empty in kw, no default: absent", "typed",
        json_pack("{s:s}", "r", ""), "r", NULL);
    expect_value(yuno, "boolean empty in kw, no default: absent", "typed",
        json_pack("{s:s}", "b", ""), "b", NULL);
    expect_value(yuno, "a refused kw value the line gives in another case", "typed S=five",
        json_pack("{s:i}", "s", 5), "s", json_string("five"));

    /*
     *  A parameter reaches the handler under its DECLARED name: `N=5` was
     *  stored as "N", the handler read `n` and took its default, and a kw
     *  `n` beat the line (REV-CLOUD V22.4.1)
     */
    expect_value(yuno, "a key in another case is the declared parameter", "typed N=5",
        0, "n", json_integer(5));
    expect_value(yuno, "the line in another case beats kw", "typed N=5",
        json_pack("{s:i}", "n", 3), "n", json_integer(5));

    /*
     *  A NUL inside a text of kw (an ievent frame is decoded with
     *  JSON_ALLOW_NUL): C read it up to the NUL (REV-CLOUD V22.4.2)
     */
    expect_refused(yuno, "integer given a text with a NUL", "typed",
        json_pack("{s:s%}", "n", "5\0junk", (size_t)6),
        "parameter 'n' must be an integer, not a text with a NUL in it", FALSE);
    expect_refused(yuno, "boolean given a text with a NUL, by a peer", "typed",
        json_pack("{s:s%, s:s}", "b", "yes\0no", (size_t)6, "__username__", "someone"),
        "parameter 'b' must be a boolean", TRUE);
    expect_refused(yuno, "string given a text with a NUL", "typed",
        json_pack("{s:s%}", "s", "ab\0cd", (size_t)5),
        "parameter 's' must be a string, not a text with a NUL in it", FALSE);
    expect_value(yuno, "json given a text with a NUL: as it came", "typed",
        json_pack("{s:s%}", "j", "5\0junk", (size_t)6), "j", json_stringn("5\0junk", 6));

    /*
     *  The text of a real is a decimal number, as jansson reads one; that
     *  of an integer or a boolean an integer (REV-CLOUD V22.4.3)
     */
    expect_refused(yuno, "real text in hex", "typed",
        json_pack("{s:s}", "r", "0x10"), "parameter 'r' must be a number, not a text", FALSE);
    expect_refused(yuno, "real text in hex, on the line", "typed r=0x1p3",
        0, "parameter 'r' must be a number, not a text", FALSE);
    expect_value(yuno, "real text that underflows is a number", "typed",
        json_pack("{s:s}", "r", "1e-400"), "r", json_real(0.0));
    expect_refused(yuno, "integer text of a real of no decimals", "typed n=2.0",
        0, "parameter 'n' must be an integer, not a text", FALSE);
    expect_refused(yuno, "boolean text of a real", "typed b=1.0",
        0, "parameter 'b' must be a boolean", FALSE);

    /*
     *  A number written as text is one of the JSON grammar, the whole
     *  text: strtod() and strtoll() took ".5", "5.", "+5" and "007" (as 7)
     *  (REV-CLOUD V23.4.6)
     */
    expect_refused(yuno, "real text .5", "typed r=.5",
        0, "parameter 'r' must be a number, not a text", FALSE);
    expect_refused(yuno, "real text 5.", "typed r=5.",
        0, "parameter 'r' must be a number, not a text", FALSE);
    expect_refused(yuno, "real text +5", "typed",
        json_pack("{s:s}", "r", "+5"), "parameter 'r' must be a number, not a text", FALSE);
    expect_refused(yuno, "real text 007", "typed r=007",
        0, "parameter 'r' must be a number, not a text", FALSE);
    expect_refused(yuno, "real text 1e", "typed r=1e",
        0, "parameter 'r' must be a number, not a text", FALSE);
    expect_refused(yuno, "integer text +5", "typed n=+5",
        0, "parameter 'n' must be an integer, not a text", FALSE);
    expect_refused(yuno, "integer text 007, by a peer", "typed",
        json_pack("{s:s, s:s}", "n", "007", "__username__", "someone"),
        "parameter 'n' must be an integer, not a text", TRUE);
    expect_refused(yuno, "boolean text 007", "typed b=007",
        0, "parameter 'b' must be a boolean", FALSE);
    expect_refused(yuno, "boolean text +1", "typed b=+1",
        0, "parameter 'b' must be a boolean", FALSE);
    expect_value(yuno, "integer text -0", "typed n=-0",
        0, "n", json_integer(0));
    expect_value(yuno, "boolean text -1", "typed b=-1",
        0, "b", json_true());
    expect_value(yuno, "real text 1E5", "typed r=1E5",
        0, "r", json_real(100000.0));
    expect_value(yuno, "real text -0.5e-1", "typed",
        json_pack("{s:s}", "r", "-0.5e-1"), "r", json_real(-0.05));
    expect_value(yuno, "real text 0", "typed r=0",
        0, "r", json_real(0.0));

    /*
     *  ... read in the "C" locale: with the LC_NUMERIC of es_ES, strtod()
     *  read "2.5" as 2 and stopped at the '.', and took "2,5"
     */
    if(setlocale(LC_NUMERIC, "es_ES.UTF-8") || setlocale(LC_NUMERIC, "es_ES.utf8")) {
        expect_value(yuno, "real text 2.5, LC_NUMERIC es_ES", "typed r=2.5",
            0, "r", json_real(2.5));
        expect_refused(yuno, "real text 2,5, LC_NUMERIC es_ES", "typed",
            json_pack("{s:s}", "r", "2,5"), "parameter 'r' must be a number, not a text", FALSE);
        setlocale(LC_NUMERIC, "C");
    } else {
        printf("skip %-40s %s\n", "LC_NUMERIC es_ES", "the locale is not installed here");
    }

    /*
     *  command_parameter_from_text(): the parser's reader of a text
     */
    {
        const char *why = NULL;
        json_t *jn = command_parameter_from_text(DTP_BOOLEAN, "yes", &why);
        check("from_text boolean yes", "true", json_is_true(jn) && why == NULL);
        JSON_DECREF(jn)
        jn = command_parameter_from_text(DTP_INTEGER, "12abc", &why);
        check("from_text integer 12abc", "refused, and why",
            jn == NULL && why && strcmp(why, "a text that does not read as one")==0);
        jn = command_parameter_from_text(DTP_REAL, "-3.5", &why);
        check("from_text real -3.5", "-3.5", json_is_real(jn) && json_real_value(jn) == -3.5);
        JSON_DECREF(jn)
        jn = command_parameter_from_text(DTP_INTEGER, "", &why);
        check("from_text integer empty", "0, as on the line",
            json_is_integer(jn) && json_integer_value(jn) == 0);
        JSON_DECREF(jn)
        jn = command_parameter_from_text(DTP_REAL, "0x10", &why);
        check("from_text real 0x10", "refused: no hex",
            jn == NULL && why && strcmp(why, "a text that does not read as one")==0);
        jn = command_parameter_from_text(DTP_JSON, "word", &why);
        check("from_text json word", "the text itself",
            json_is_string(jn) && strcmp(json_string_value(jn), "word")==0);
        JSON_DECREF(jn)
        jn = command_parameter_from_text(DTP_DICT, "[1]", &why);
        check("from_text dict [1]", "refused: not its shape", jn == NULL && why != NULL);
        jn = command_parameter_from_text(DTP_REAL, ".5", &why);
        check("from_text real .5", "refused: no JSON number",
            jn == NULL && why && strcmp(why, "a text that does not read as one")==0);
        jn = command_parameter_from_text(DTP_INTEGER, "007", &why);
        check("from_text integer 007", "refused: no JSON integer",
            jn == NULL && why && strcmp(why, "a text that does not read as one")==0);
        jn = command_parameter_from_text(DTP_REAL, "1e999", &why);
        check("from_text real 1e999", "refused: out of its range",
            jn == NULL && why && strcmp(why, "a number out of its range")==0);
    }

    JSON_DECREF(g_seen)
    gobj_end();

    size_t leaked = get_cur_system_memory();
    check("memory", "no leak", leaked == 0);

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result;
}
