/****************************************************************************
 *          test_kw1.c
 *
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <locale.h>
#include <signal.h>
#include <time.h>
#include <yunetas.h>

#define APP "test_kw1"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;

/***************************************************************************
 *  Test matching true
 ***************************************************************************/
static int test1_true(void)
{
    int result = 0;
    json_t *jn_filter = 0;

    json_t *kw = json_pack("{s:s, s:b, s:i, s:f}",
        "string", "s",
        "bool",  TRUE,
        "integer", 123,
        "real", 1.5
    );
    BOOL matched = kw_match_simple(kw, NULL);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:s}",
        "string", "s"
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:b}",
        "bool", TRUE
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:i}",
        "integer", 123
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:f}",
        "real", 1.5
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:s, s:b, s:i, s:f}",
        "string", "s",
        "bool",  TRUE,
        "integer", 123,
        "real", 1.5
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("[{s:s, s:b}, {s:i, s:f}]",
        "string", "s",
        "bool",  TRUE,
        "integer", 123,
        "real", 1.5
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("[{}, {s:i, s:f}]",
        "integer", 0,
        "real", 0.0
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    KW_DECREF(kw)
    return result;
}

/***************************************************************************
 *  Test matching false
 ***************************************************************************/
static int test1_false(void)
{
    int result = 0;
    json_t *jn_filter = 0;
    BOOL matched = FALSE;

    json_t *kw = json_pack("{s:s, s:b, s:i, s:f}",
        "string", "s",
        "bool",  TRUE,
        "integer", 123,
        "real", 1.5
    );

    jn_filter = json_pack("{s:s}",
        "string", "sa"
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:b}",
        "bool", FALSE
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:i}",
        "integer", 1234
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:f}",
        "real", 1.55
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:s, s:b, s:i, s:f}",
        "string", "ss",
        "bool",  TRUE,
        "integer", 123,
        "real", 1.5
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("[{s:s, s:b}, {s:i, s:f}]",
        "string", "sa",
        "bool",  TRUE,
        "integer", 123,
        "real", 1.55
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("[{s:i, s:f}]",
        "integer", 0,
        "real", 0.0
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    KW_DECREF(kw)
    return result;
}

/***************************************************************************
 *  Test complex
 ***************************************************************************/
static int test2_true(void)
{
    int result = 0;
    json_t *jn_filter = 0;

    json_t *kw = json_pack("{s:{s:s, s:b, s:i, s:f}, s:[{s:s, s:b}, {s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  TRUE,
            "integer", 123,
            "real", 1.5,
        "list",
            "string2", "string2",
            "false",  FALSE,
            "integer", 456,
            "real", 2.6
    );

    BOOL matched = kw_match_simple(kw, NULL);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:s, s:b}]}",
        "object",
            "string1", "string1",
            "true",  TRUE,
        "list",
            "string2", "string2",
            "false",  FALSE
);
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{}, {s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  TRUE,
        "list",
            "integer", 1235,
            "real", 1.55
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:s, s:b}, {s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  TRUE,
        "list",
            "string2", "string2",
            "false",  FALSE,
            "integer", 4563,
            "real", 2.6
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    KW_DECREF(kw)
    return result;
}

/***************************************************************************
 *  Test complex
 ***************************************************************************/
static int test2_false(void)
{
    int result = 0;
    json_t *jn_filter = 0;

    json_t *kw = json_pack("{s:{s:s, s:b, s:i, s:f}, s:[{s:s, s:b, s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  TRUE,
            "integer", 123,
            "real", 1.5,
        "list",
            "string2", "string2",
            "false",  FALSE,
            "integer", 456,
            "real", 2.6
    );

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  TRUE,
        "list",
            "integer", 456,
            "real", 2.6
    );
    BOOL matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:i, s:f}]}",
        "object",
            "string1", "string2",
            "true",  TRUE,
        "list",
            "integer", 456,
            "real", 2.6
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  FALSE,
        "list",
            "integer", 456,
            "real", 2.6
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  TRUE,
        "list",
            "integer", 4566,
            "real", 2.6
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  TRUE,
        "list",
            "integer", 456,
            "real", 2.66
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }


    KW_DECREF(kw)
    return result;
}

/***************************************************************************
 *  The logger honours the precision of "%.Ns" and "%.*s".
 *  It skipped it, so every capped value of the tree was logged whole.
 ***************************************************************************/
static int test_log_precision_caps_the_value(void)
{
    int result = 0;
    set_expected_results(
        "log_precision_caps_the_value",
        json_pack("[{s:s, s:s, s:s, s:i}]",
            "msg", "precision",
            "value", "abcde",
            "star", "abc",
            "after", 7
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    gobj_log_warning(0, 0,
        "msg",      "%s", "precision",
        "value",    "%.5s", "abcdefghij",
        "star",     "%.*s", 3, "abcdefghij",
        "after",    "%d", 7,
        NULL
    );

    result += test_json(NULL);  // NULL: we want to check only the logs
    return result;
}

/***************************************************************************
 *  The length modifiers, and a '%' that is no conversion.
 *  "%zu" of 5000000000 logged 705032704, "%llu" of ULLONG_MAX logged -1,
 *  "%jd" took the json conversion (and crashed), and "100%" consumed an
 *  argument that was not there, losing the keys after it (a '%' that ends
 *  the format; "50% done" is no example: "% d" IS a conversion, for printf too).
 ***************************************************************************/
static int test_log_length_modifiers(void)
{
    int result = 0;
    set_expected_results(
        "log_length_modifiers",
        json_pack("[{s:s, s:I, s:s, s:I, s:I, s:s, s:s, s:i}]",
            "msg", "lengths",
            "zu", (json_int_t)5000000000LL,
            "llu", "18446744073709551615",
            "jd", (json_int_t)-7,
            "hhd", (json_int_t)-1,
            "text", "100%",
            "pct", "%",
            "after", 9
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    gobj_log_warning(0, 0,
        "msg",      "%s", "lengths",
        "zu",       "%zu", (size_t)5000000000ULL,
        "llu",      "%llu", ULLONG_MAX,
        "jd",       "%jd", (intmax_t)-7,
        "hhd",      "%hhd", 255,
        "text",     "100%",
        "pct",      "%%",
        "after",    "%d", 9,
        NULL
    );

    result += test_json(NULL);  // NULL: we want to check only the logs
    return result;
}

/***************************************************************************
 *  "%m" is glibc's, strerror(errno) and NO argument. It
 *  consumed one, so the key after it was read shifted.
 ***************************************************************************/
static int test_log_percent_m_takes_no_argument(void)
{
    int result = 0;
    set_expected_results(
        "log_percent_m_takes_no_argument",
        json_pack("[{s:s, s:s, s:i}]",
            "msg", "percent m",
            "errno", strerror(ENOENT),
            "after", 11
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    errno = ENOENT;
    gobj_log_warning(0, 0,
        "msg",      "%s", "percent m",
        "errno",    "%m",
        "after",    "%d", 11,
        NULL
    );

    result += test_json(NULL);  // NULL: we want to check only the logs
    return result;
}

/***************************************************************************
 *  A cap by bytes ended in the middle of a UTF-8
 *  character: json_sprintf() refused the comment it was in, and the log
 *  wrote a broken byte. utf8_prefix_len() backs up to the character's start,
 *  and the logger's "%.Ns" uses it.
 ***************************************************************************/
static int test_utf8_cap(void)
{
    int result = 0;
    const char *s = "abc\xc3\xb1z";   // abc, n-tilde (2 bytes), z
    struct {int max; int expected;} cases[] = {
        {0, 0}, {3, 3}, {4, 3}, {5, 5}, {6, 6}, {64, 6}
    };
    for(size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        int got = utf8_prefix_len(s, cases[i].max);
        if(got != cases[i].expected) {
            printf("%sERROR --> %s utf8_prefix_len(max %d) = %d, expected %d\n",
                On_Red BWhite, Color_Off, cases[i].max, got, cases[i].expected);
            result += -1;
        }
    }
    if(utf8_prefix_len(NULL, 10) != 0) {
        result += -1;
    }
    /*  A 3-byte character cut after its first and its second byte  */
    const char *e = "x\xe2\x82\xacy";      // x, euro sign (3 bytes), y
    if(utf8_prefix_len(e, 2) != 1 || utf8_prefix_len(e, 3) != 1 || utf8_prefix_len(e, 4) != 4) {
        printf("%sERROR --> %s utf8_prefix_len() cuts a 3-byte character\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  The comment is built, not refused  */
    json_t *comment = json_sprintf("'%.*s'", utf8_prefix_len(s, 4), s);
    if(!comment || strcmp(json_string_value(comment), "'abc'") != 0) {
        printf("%sERROR --> %s json_sprintf() of a UTF-8 cap\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    JSON_DECREF(comment)

    /*  And the logger's precision backs up the same way  */
    set_expected_results(
        "utf8_cap",
        json_pack("[{s:s, s:s, s:s}]",
            "msg", "utf8 cap",
            "value", "abc",
            "whole", "abc\xc3\xb1"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );
    gobj_log_warning(0, 0,
        "msg",      "%s", "utf8 cap",
        "value",    "%.4s", s,
        "whole",    "%.5s", s,
        NULL
    );
    result += test_json(NULL);  // NULL: we want to check only the logs
    return result;
}

/***************************************************************************
 *  The table tools padded and cut a cell by BYTES ("%-*.*s"), so every
 *  non-ASCII character pulled the columns after it one place to the left:
 *  "Espana · A Coruna" took two places more than it showed (2026-10-20).
 *  utf8_columns() and utf8_prefix_columns() measure characters.
 ***************************************************************************/
static int test_utf8_columns(void)
{
    int result = 0;
    const char *s = "a\xc2\xb7" "b\xe2\x82\xac";    // a, middle dot (2 bytes), b, euro (3)

    if(utf8_columns(s) != 4 || utf8_columns("") != 0 || utf8_columns(NULL) != 0) {
        printf("%sERROR --> %s utf8_columns() = %d, expected 4\n",
            On_Red BWhite, Color_Off, utf8_columns(s));
        result += -1;
    }

    struct {int max; int bytes; int cols;} cases[] = {
        {0, 0, 0}, {1, 1, 1}, {2, 3, 2}, {3, 4, 3}, {4, 7, 4}, {40, 7, 4}
    };
    for(size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        int cols = -1;
        int bytes = utf8_prefix_columns(s, cases[i].max, &cols);
        if(bytes != cases[i].bytes || cols != cases[i].cols) {
            printf("%sERROR --> %s utf8_prefix_columns(max %d) = %d bytes, %d columns; "
                "expected %d, %d\n",
                On_Red BWhite, Color_Off, cases[i].max, bytes, cols,
                cases[i].bytes, cases[i].cols);
            result += -1;
        }
    }
    if(utf8_prefix_columns(NULL, 10, NULL) != 0) {
        printf("%sERROR --> %s utf8_prefix_columns(NULL) != 0\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  A cell padded with the two answers is as wide as an ASCII one  */
    char cell[64];
    int cols;
    int bytes = utf8_prefix_columns(s, 6, &cols);
    snprintf(cell, sizeof(cell), "%.*s%*s|", bytes, s, 6 - cols, "");
    if(utf8_columns(cell) != 7 || strcmp(cell, "a\xc2\xb7" "b\xe2\x82\xac  |") != 0) {
        printf("%sERROR --> %s a padded cell is %d columns wide, expected 7\n",
            On_Red BWhite, Color_Off, utf8_columns(cell));
        result += -1;
    }
    return result;
}

/***************************************************************************
 *  A filter path too long for kw_match_simple() is said
 *  ONCE per process (the matchers run per publication and per record).
 ***************************************************************************/
static int test_match_simple_long_path_said_once(void)
{
    int result = 0;
    set_expected_results(
        "match_simple_long_path_said_once",
        json_pack("[{s:s}]",
            "msg", "filter path too long, not matched (said once per process)"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    char long_key[NAME_MAX + 32];
    memset(long_key, 'k', sizeof(long_key) - 1);
    long_key[sizeof(long_key) - 1] = 0;
    json_t *kw = json_object();
    for(int i = 0; i < 3; i++) {
        if(kw_match_simple(kw, json_pack("{s:s}", long_key, "v"))) {
            result += -1;   // a path that does not fit never matches
        }
    }
    json_decref(kw);

    result += test_json(NULL);  // NULL: we want to check only the logs
    return result;
}

/***************************************************************************
 *  KW_CREATE|KW_REQUIRED over a null leaf with no default
 *  answers NULL as a missing key does, and says it as a missing key does
 *  (it answered NULL in silence while the missing key logged).
 ***************************************************************************/
static int test_null_leaf_required_logs_as_missing(void)
{
    int result = 0;
    set_expected_results(
        "null_leaf_required_logs_as_missing",
        json_pack("[{s:s},{s:s}]",
            "msg", "path NOT FOUND, default value returned",
            "msg", "path NOT FOUND, default value returned"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    json_t *kw = json_pack("{s:n, s:n}", "d", "l");
    if(kw_get_dict(0, kw, "d", NULL, KW_CREATE|KW_REQUIRED) != NULL) {
        result += -1;
    }
    if(kw_get_list(0, kw, "l", NULL, KW_CREATE|KW_REQUIRED) != NULL) {
        result += -1;
    }
    json_decref(kw);

    result += test_json(NULL);  // NULL: we want to check only the logs
    return result;
}

/***************************************************************************
 *  Regression f001: gbuffer_deserialize() must reject a serialized blob whose
 *  "data" field is missing/non-string instead of crashing on strlen(NULL).
 ***************************************************************************/
static int test_reg_f001_gbuffer_deserialize_null_data(void)
{
    int result = 0;
    set_expected_results(
        "reg_f001_gbuffer_deserialize_null_data",
        json_pack("[{s:s}]",    // exactly this error must be logged, nothing else
            "msg", "gbuffer_deserialize: 'data' field missing or not a string"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    json_t *jn = json_pack("{s:s, s:i}",    // a serialized blob with NO "data" field
        "label", "x",
        "mark", 0
    );
    gbuffer_t *gbuf = gbuffer_deserialize(0, jn);
    if(gbuf != NULL) {
        result += -1;   // must reject, not return a buffer
        gbuffer_decref(gbuf);
    }
    json_decref(jn);

    result += test_json(NULL);  // NULL: we want to check only the logs
    return result;
}

/***************************************************************************
 *  Regression f001 (variant): gbuffer_deserialize() must also reject a
 *  serialized blob whose "data" field is PRESENT but is not valid base64.
 *  gbuffer_base64_to_binary() returns NULL on a decode failure; the old code
 *  fed that NULL straight into gbuffer_setmark() (gbuf->mark = mark), a NULL
 *  deref reachable pre-auth via ac_on_message -> kw_deserialize.
 ***************************************************************************/
static int test_reg_f001_gbuffer_deserialize_bad_base64(void)
{
    int result = 0;
    set_expected_results(
        "reg_f001_gbuffer_deserialize_bad_base64",
        json_pack("[{s:s},{s:s}]",  // b64_decode fails, then deserialize rejects
            "msg", "b64_decode() FAILED",
            "msg", "gbuffer_deserialize: base64 decode of 'data' field FAILED"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    /* "!!!!" is the right length to attempt a decode but is not valid base64 */
    json_t *jn = json_pack("{s:s, s:i, s:s}",
        "label", "x",
        "mark", 0,
        "data", "!!!!"
    );
    gbuffer_t *gbuf = gbuffer_deserialize(0, jn);
    if(gbuf != NULL) {
        result += -1;   // must reject, not return a buffer (and must not crash)
        gbuffer_decref(gbuf);
    }
    json_decref(jn);

    result += test_json(NULL);  // NULL: we want to check only the logs
    return result;
}

/***************************************************************************
 *  Regression f003: a deeply nested kw plus a long backtick-delimited path
 *  must not exhaust the stack. kw_find_path() walks the path in a LOOP (it
 *  recursed once per segment, and was cut at 64 segments with an ERROR even
 *  with verbose FALSE), so a deep path that is there is
 *  found, one that is not is answered NULL logging nothing, and a segment
 *  longer than 255 bytes is looked up as the key it is.
 ***************************************************************************/
static int test_reg_f003_kw_find_path_depth(void)
{
    int result = 0;
    int N = 100;    // past the 64 segments the walk used to be cut at

    /* kw nested N deep: {"a":{"a":{...{"a": 42}}}} */
    json_t *root = json_object();
    json_t *cur = root;
    for(int i=0; i<N-1; i++) {
        json_t *child = json_object();
        json_object_set_new(cur, "a", child);
        cur = child;
    }
    json_object_set_new(cur, "a", json_integer(42));

    /* path "a`a`...`a" with N segments */
    char path[256];
    int off = 0;
    for(int i=0; i<N; i++) {
        if(i) {
            path[off++] = '`';
        }
        path[off++] = 'a';
    }
    path[off] = 0;

    set_expected_results(
        "reg_f003_kw_find_path_depth",
        json_pack("[]"),    // nothing logged
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    json_t *found = kw_find_path(0, root, path, FALSE);
    if(!found || json_integer_value(found) != 42) {
        result += -1;   // a deep path that is there is found
    }

    /* the same depth, a path that is not there: NULL, and nothing logged */
    path[off-1] = 'b';
    if(kw_find_path(0, root, path, FALSE) != NULL) {
        result += -1;
    }

    /* a segment of 300 bytes is a key like any other */
    char long_key[301];
    memset(long_key, 'k', sizeof(long_key) - 1);
    long_key[sizeof(long_key) - 1] = 0;
    json_t *jn_long = json_pack("{s:{s:i}}", long_key, "x", 7);
    char long_path[320];
    snprintf(long_path, sizeof(long_path), "%s`x", long_key);
    found = kw_find_path(0, jn_long, long_path, FALSE);
    if(!found || json_integer_value(found) != 7) {
        result += -1;
    }
    json_decref(jn_long);
    json_decref(root);

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  json_refcounts_report(): what the refcounts say, as an answer, logging
 *  nothing, each json once and a cycle walked once:
 *  json_check_refcounts() logged an ERROR per fault and recursed round a
 *  cycle for ever -- a cycle is two nodes in each other's hooks.
 ***************************************************************************/
static int test_json_refcounts_report(void)
{
    int result = 0;

    set_expected_results(
        "json_refcounts_report",
        json_pack("[]"),    // nothing logged
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    /*  a node held twice (an index and a hook), and two nodes in a cycle  */
    json_t *node = json_pack("{s:s}", "id", "n1");
    json_t *a = json_object();
    json_t *b = json_object();
    json_object_set(a, "hook", b);      // b held twice: by a, and below
    json_object_set(b, "hook", a);      // a held twice: the cycle
    json_t *root = json_pack("{s:{s:O}, s:{s:O}, s:O, s:O}",
        "index", "n1", node,
        "hooks", "n1", node,
        "a", a,
        "b", b
    );

    json_t *jn_report = json_refcounts_report(root, 1, 20);
    if(!jn_report) {
        result += -1;
    }
    if(json_integer_value(json_object_get(jn_report, "corrupted")) != 0) {
        result += -1;
    }
    /*  node, a and b are each held more than once, each said once  */
    if(json_integer_value(json_object_get(jn_report, "over_max")) != 3) {
        printf("over_max %d\n", (int)json_integer_value(json_object_get(jn_report, "over_max")));
        result += -1;
    }
    json_t *shown = json_object_get(jn_report, "shown");
    if(json_array_size(shown) != 3) {
        result += -1;
    }
    BOOL path_seen = FALSE;
    size_t idx; json_t *jn_shown;
    json_array_foreach(shown, idx, jn_shown) {
        const char *path = json_string_value(json_object_get(jn_shown, "path"));
        if(path && (strcmp(path, "index`n1")==0 || strcmp(path, "hooks`n1")==0)) {
            path_seen = TRUE;
        }
    }
    if(!path_seen) {
        result += -1;   // the path of the node, as kw_find_path() reads it
    }
    JSON_DECREF(jn_report)

    /*  max_refcount 0: the corrupted only, and none  */
    jn_report = json_refcounts_report(root, 0, 20);
    if(json_integer_value(json_object_get(jn_report, "over_max")) != 0 ||
            json_array_size(json_object_get(jn_report, "shown")) != 0) {
        result += -1;
    }
    JSON_DECREF(jn_report)

    /*  max_shown caps the list, not the count  */
    jn_report = json_refcounts_report(root, 1, 1);
    if(json_integer_value(json_object_get(jn_report, "over_max")) != 3 ||
            json_array_size(json_object_get(jn_report, "shown")) != 1) {
        result += -1;
    }
    JSON_DECREF(jn_report)

    /*  break the cycle, so all of it goes  */
    json_object_del(a, "hook");
    JSON_DECREF(a)
    JSON_DECREF(b)
    JSON_DECREF(node)
    JSON_DECREF(root)

    /*
     *  A cycle whose refcounts are corrupted BELOW the truth -- what the
     *  report exists to find: `c` is held by the root and by `d`, with a
     *  refcount of 1. Only the jsons held more than once were remembered,
     *  so the walk went c, d, c, d... until its frames passed MAX_BLOCK, and
     *  answered totals of a walk it did not say was cut.
     *  Every dict and list is remembered now: it ends, each counted once.
     */
    json_t *c = json_object();
    json_t *d = json_object();
    json_object_set_new(c, "hook", d);
    json_object_set_new(d, "hook", c);  // c is d's now: refcount 1
    json_t *root2 = json_object();
    json_object_set_new(root2, "c", c); // and the root's too, with no reference of its own
    jn_report = json_refcounts_report(root2, 0, 20);
    if(json_integer_value(json_object_get(jn_report, "jsons")) != 3) {
        printf("jsons %d, expected 3\n", (int)json_integer_value(json_object_get(jn_report, "jsons")));
        result += -1;
    }
    if(!json_is_false(json_object_get(jn_report, "truncated"))) {
        printf("truncated, expected false\n");
        result += -1;
    }
    /*
     *  ... and it is REPORTED: `c` is reached twice, from the root and from
     *  `d`, with a refcount of 1 -- the walk ended, and said nothing.
     *  Its path is where it was reached once too many.
     */
    if(json_integer_value(json_object_get(jn_report, "corrupted")) != 1) {
        printf("corrupted %d, expected 1\n", (int)json_integer_value(json_object_get(jn_report, "corrupted")));
        result += -1;
    }
    json_t *jn_fault = json_array_get(json_object_get(jn_report, "shown"), 0);
    if(strcmp(json_string_value(json_object_get(jn_fault, "path"))?
            json_string_value(json_object_get(jn_fault, "path")) : "", "c`hook`hook")!=0 ||
       strcmp(json_string_value(json_object_get(jn_fault, "fault"))?
            json_string_value(json_object_get(jn_fault, "fault")) : "", "corrupted")!=0) {
        printf("the corrupted json is not shown at c`hook`hook\n");
        result += -1;
    }
    JSON_DECREF(jn_report)
    json_incref(c);                     // the truth again: held twice
    json_object_del(d, "hook");
    JSON_DECREF(root2)

    /*
     *  A json set with json_object_set_new() in two places: a refcount of 1
     *  and two holders. A dict and a string, neither in a cycle.
     */
    json_t *twice = json_pack("{s:s}", "id", "x");
    json_t *text = json_string("held twice");
    json_t *root3 = json_object();
    json_object_set_new(root3, "a", twice);
    json_object_set_new(root3, "b", twice);     // the bug: the reference was the first's
    json_object_set_new(root3, "t1", text);
    json_object_set_new(root3, "t2", text);
    jn_report = json_refcounts_report(root3, 0, 20);
    if(json_integer_value(json_object_get(jn_report, "corrupted")) != 2) {
        printf("corrupted %d, expected 2\n", (int)json_integer_value(json_object_get(jn_report, "corrupted")));
        result += -1;
    }
    if(json_integer_value(json_object_get(jn_report, "jsons")) != 4) {
        printf("jsons %d, expected 4\n", (int)json_integer_value(json_object_get(jn_report, "jsons")));
        result += -1;
    }
    JSON_DECREF(jn_report)
    json_incref(twice);                 // the truth again
    json_incref(text);
    JSON_DECREF(root3)

    /*
     *  A sound json: nothing corrupted, also with a node held by the caller
     *  and by two hooks (refcount 3, reached twice)
     */
    json_t *node3 = json_pack("{s:s}", "id", "n3");
    json_t *root4 = json_pack("{s:{s:O}, s:{s:O}}", "index", "n3", node3, "hooks", "n3", node3);
    jn_report = json_refcounts_report(root4, 0, 20);
    if(json_integer_value(json_object_get(jn_report, "corrupted")) != 0) {
        printf("a sound json reported corrupted\n");
        result += -1;
    }
    JSON_DECREF(jn_report)
    JSON_DECREF(root4)
    JSON_DECREF(node3)

    /*
     *  json_nul_fault(): a NUL in a string, at any depth (a json decoded with
     *  JSON_ALLOW_NUL); C reads a string up to it
     */
    json_t *rec = json_pack("{s:s, s:{s:[s, s%]}}",
        "id", "dx",
        "data", "list", "ok", "dx\0evil", (size_t)7
    );
    const char *nul_fault = json_nul_fault(rec);
    if(!nul_fault || strcmp(nul_fault, "a string with a NUL in it")!=0) {
        printf("json_nul_fault() did not find the NUL down the record\n");
        result += -1;
    }
    json_t *clean = json_pack("{s:s, s:[i, b, n]}", "id", "dx", "l", 1, 1);
    if(json_nul_fault(clean)) {
        printf("json_nul_fault() found a NUL in a clean record\n");
        result += -1;
    }
    json_t *deep = json_array();
    json_t *level = deep;
    for(int i = 0; i < 3000; i++) {
        json_t *next = json_array();
        json_array_append_new(level, next);
        level = next;
    }
    nul_fault = json_nul_fault(deep);
    if(!nul_fault || strcmp(nul_fault, "a json nested deeper than a parser builds")!=0) {
        printf("json_nul_fault() did not refuse a json 3000 levels deep\n");
        result += -1;
    }
    JSON_DECREF(deep)
    JSON_DECREF(clean)
    JSON_DECREF(rec)

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  json_refcounts_report() of a list of a MILLION scalars, under the
 *  default MAX_BLOCK (16 MB): walked whole. A frame was reserved for every
 *  child of the container walked, scalars too, one block of 32 MB for the
 *  root: the walk stopped there, "truncated", one json counted.
 *  Only a dict or a list gets a frame now, and no block of the
 *  walk grows with the json. And a list of 40 000 records (more frames than
 *  one chunk holds), counted right.
 ***************************************************************************/
static int test_json_refcounts_report_scales(void)
{
    int result = 0;

    set_expected_results(
        "json_refcounts_report_scales",
        json_pack("[]"),    // nothing logged: no block refused
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    const json_int_t n_scalars = 1000000;
    json_t *jn_list = json_array();
    for(json_int_t i = 0; i < n_scalars; i++) {
        json_array_append_new(jn_list, json_integer(i));
    }
    json_t *jn_report = json_refcounts_report(jn_list, 0, 20);
    if(json_is_true(json_object_get(jn_report, "truncated")) ||
            json_integer_value(json_object_get(jn_report, "jsons")) != n_scalars + 1 ||
            json_integer_value(json_object_get(jn_report, "corrupted")) != 0) {
        printf("a list of a million scalars: jsons %ld (expected %ld), truncated %s\n",
            (long)json_integer_value(json_object_get(jn_report, "jsons")),
            (long)(n_scalars + 1),
            json_is_true(json_object_get(jn_report, "truncated"))? "true" : "false"
        );
        result += -1;
    }
    JSON_DECREF(jn_report)
    JSON_DECREF(jn_list)

    const json_int_t n_records = 40000;
    json_t *jn_records = json_array();
    for(json_int_t i = 0; i < n_records; i++) {
        json_array_append_new(jn_records, json_pack("{s:I, s:s, s:[i]}",
            "n", i, "id", "r", "tags", 1
        ));
    }
    jn_report = json_refcounts_report(jn_records, 0, 20);
    /*  each record: itself, its integer, its string, its list and the item  */
    if(json_is_true(json_object_get(jn_report, "truncated")) ||
            json_integer_value(json_object_get(jn_report, "jsons")) != 5*n_records + 1) {
        printf("a list of records: jsons %ld (expected %ld)\n",
            (long)json_integer_value(json_object_get(jn_report, "jsons")),
            (long)(5*n_records + 1)
        );
        result += -1;
    }
    JSON_DECREF(jn_report)
    JSON_DECREF(jn_records)

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  Regression f004: the kwid comparators must bound recursion depth so
 *  deeply nested JSON cannot exhaust the stack or amplify via deep-copy.
 ***************************************************************************/
static int test_reg_f004_kwid_compare_records_depth(void)
{
    int result = 0;
    int N = 100;    // > KWID_MAX_COMPARE_DEPTH (64)

    /* two identical records nested N deep: {"a":{"a":{...}}} */
    json_t *rec = json_object();
    json_t *cur = rec;
    for(int i=0; i<N; i++) {
        json_t *child = json_object();
        json_object_set_new(cur, "a", child);
        cur = child;
    }
    json_t *expected = json_deep_copy(rec);

    set_expected_results(
        "reg_f004_kwid_compare_records_depth",
        json_pack("[{s:s}]",
            "msg", "compare: max nesting depth exceeded"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    BOOL eq = kwid_compare_records(0, rec, expected, NULL, FALSE, FALSE, FALSE);
    if(eq != FALSE) {
        result += -1;   // must bail to FALSE at the depth cap, not recurse off the stack
    }
    json_decref(rec);
    json_decref(expected);

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  Regression f002: kwid_find_record_in_list must return -1 (not 0) when an
 *  id is genuinely absent. The match-by-id branch of the list comparator
 *  tests `idx2 < 0`; a bogus 0 made it compare/remove the WRONG record
 *  (expected[0]) and report a false match.
 ***************************************************************************/
static int test_reg_f002_find_record_not_found(void)
{
    int result = 0;

    /*
     *  list has a record whose id ("X") is NOT present in expected.
     *  expected[0] has a different id ("A") but the same payload.
     *  Ignoring "id", a buggy not-found return of 0 makes the comparator
     *  match list[0] against the wrong expected[0] and report equal (TRUE).
     *  With the -1 fix the absent id is detected and the lists differ (FALSE).
     */
    json_t *list = json_pack("[{s:s, s:i}]",
        "id", "X",
        "v", 1
    );
    json_t *expected = json_pack("[{s:s, s:i}]",
        "id", "A",
        "v", 1
    );
    const char *ignore_keys[] = {"id", NULL};

    set_expected_results(
        "reg_f002_find_record_not_found",
        NULL,   // error's list: no error log expected on this path
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        0       // verbose off: assert on the BOOL, not on trace logs
    );

    BOOL eq = kwid_compare_lists(0, list, expected, ignore_keys, FALSE, FALSE, FALSE);
    if(eq != FALSE) {
        result += -1;   // absent id must surface as a mismatch, not a wrong-record match
    }
    JSON_DECREF(list);
    JSON_DECREF(expected);

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  json_unflatten_dict(): backtick-path flat dict -> nested json.
 *  Exercises the root-kind detection, which reads the FIRST segment: an
 *  index ('[N]') makes the root an array, anything else an object.
 *
 *  The index used to be a bare number, which forced all-digit keys to be
 *  forbidden; it is '[N]' since the flat form was rewritten, so a key like
 *  "7302" is now just a key. The whole grammar is tested in
 *  test_json_flat.c; this one only guards the two callers of the old
 *  names.
 ***************************************************************************/
static int test_json_unflatten_dict(void)
{
    int result = 0;

    /* object root: first token "a" is not an array index */
    json_t *flat1 = json_pack("{s:i, s:i}",
        "a`b", 1,
        "a`c", 2
    );
    json_t *got1 = json_unflatten_dict(flat1);
    json_t *exp1 = json_pack("{s:{s:i, s:i}}",
        "a",
            "b", 1,
            "c", 2
    );
    if(!json_equal(got1, exp1)) {
        result += -1;
        printf("FAIL json_unflatten_dict object root\n");
    }
    JSON_DECREF(flat1)
    JSON_DECREF(got1)
    JSON_DECREF(exp1)

    /* array root: the first segment IS an index -> root is an array */
    json_t *flat2 = json_pack("{s:s, s:s}",
        "[0]`name", "zero",
        "[1]`name", "one"
    );
    json_t *got2 = json_unflatten_dict(flat2);
    json_t *exp2 = json_pack("[{s:s}, {s:s}]",
        "name", "zero",
        "name", "one"
    );
    if(!json_equal(got2, exp2)) {
        result += -1;
        printf("FAIL json_unflatten_dict array root\n");
    }
    JSON_DECREF(flat2)
    JSON_DECREF(got2)
    JSON_DECREF(exp2)

    return result;
}

/***************************************************************************
 *  kw_collapse() must accept a top-level ARRAY (not only a dict): the
 *  `print-tranger path=<array>` lazy-drill depends on it. A dict element
 *  whose sub-dict exceeds dicts_limit collapses to a __collapsed__ stub
 *  whose path is `index`key`, and that path must round-trip through
 *  kw_find_path on the ORIGINAL (arrays indexed by atoi).
 ***************************************************************************/
static int test_kw_collapse_toplevel_array(void)
{
    int result = 0;

    json_t *original = json_pack("[{s:i, s:{s:i, s:i, s:i}}]",
        "small", 1,
        "big",
            "a", 1,
            "b", 2,
            "c", 3
    );

    json_t *collapsed = kw_collapse(0, original, 0, 2);  // lists: no cap, dicts: 2

    json_t *expected = json_pack("[{s:i, s:{s:{s:s, s:I}}}]",
        "small", 1,
        "big",
            "__collapsed__",
                "path", "0`big",
                "size", (json_int_t)3
    );

    if(!collapsed || !json_equal(collapsed, expected)) {
        result += -1;
        printf("FAIL kw_collapse top-level array shape\n");
    }

    /* the emitted path must resolve on the ORIGINAL (drill round-trip) */
    json_t *found = kw_find_path(0, original, "0`big", FALSE);
    if(!found || json_object_size(found) != 3) {
        result += -1;
        printf("FAIL kw_collapse array path round-trip\n");
    }

    JSON_DECREF(original)
    JSON_DECREF(collapsed)
    JSON_DECREF(expected)
    return result;
}

/***************************************************************************
 *  kw_collapse() on a primitive (neither dict nor array) must reject it,
 *  returning NULL and logging — not crash, not return a half-built tree.
 ***************************************************************************/
static int test_kw_collapse_primitive_rejected(void)
{
    int result = 0;

    set_expected_results(
        "kw_collapse_primitive_rejected",
        json_pack("[{s:s}]",
            "msg", "kw_collapse() kw must be a dictionary or array"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    json_t *prim = json_string("x");
    json_t *out = kw_collapse(0, prim, 0, 0);
    if(out != NULL) {
        result += -1;   // a primitive has nothing to collapse
    }
    JSON_DECREF(prim)
    JSON_DECREF(out)    // NULL-safe

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *              Test
 *  Open as master, check main files, add records, open rt lists
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;

    result += test1_true();
    result += test1_false();
    result += test2_true();
    result += test2_false();
    result += test_json_unflatten_dict();

    result += test_null_leaf_required_logs_as_missing();
    result += test_log_precision_caps_the_value();
    result += test_log_length_modifiers();
    result += test_log_percent_m_takes_no_argument();
    result += test_utf8_cap();
    result += test_utf8_columns();
    result += test_match_simple_long_path_said_once();
    result += test_reg_f001_gbuffer_deserialize_null_data();
    result += test_reg_f001_gbuffer_deserialize_bad_base64();
    result += test_reg_f002_find_record_not_found();
    result += test_reg_f003_kw_find_path_depth();
    result += test_json_refcounts_report();
    result += test_json_refcounts_report_scales();
    result += test_reg_f004_kwid_compare_records_depth();
    result += test_kw_collapse_toplevel_array();
    result += test_kw_collapse_primitive_rejected();

    /*-------------------------------*
     *      Shutdown timeranger
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_shutdown", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );
    result += test_json(NULL);  // NULL: we want to check only the logs

    return result;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    /*
     *  The default MAX_BLOCK (0: 16 MB), and room for the million jsons of
     *  test_json_refcounts_report_scales() (the default system memory, 64
     *  MB, does not hold them with CONFIG_DEBUG_TRACK_MEMORY)
     */
    gbmem_setup(0, 1024*1024*1024L, FALSE, 0, 0);

    gbmem_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

//    gobj_set_deep_tracing(2);           // TODO TEST
//    gobj_set_global_trace(0, TRUE);     // TODO TEST

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL    // global_authentication_parser
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
        NULL,
        &yev_loop
    );

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    int result = do_test();
    result += global_result;

    /*--------------------------------*
     *  Stop the event loop
     *--------------------------------*/
    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }
    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result<0?-1:0;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop_reset_running(yev_loop);
    if(xtimes_once > 1) {
        exit(-1);
    }
}

PUBLIC void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);   // to debug in kdevelop
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);    // ctrl+c
}
