/****************************************************************************
 *          test_glogger_utf8.c
 *
 *          Unit test: the logger must never emit a record that a strict
 *          JSON/UTF-8 parser (the logcenter) cannot read.
 *
 *          A corrupted device payload logged verbatim used to leak raw
 *          invalid-UTF-8 bytes (0x8a, 0xc2, ...) into the log record,
 *          which then broke the logcenter's gbuf2json(). _ul_str_escape()
 *          now escapes invalid bytes to \u00XX while keeping valid UTF-8
 *          multibyte sequences readable.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP "test_glogger_utf8"

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE int global_result = 0;
PRIVATE char captured[16*1024];
PRIVATE size_t captured_len = 0;

/***************************************************************************
 *  Capture handler: stores the last emitted log record verbatim,
 *  exactly as it would travel to the logcenter.
 ***************************************************************************/
PRIVATE int capture_write(void *h, int priority, const char *bf, size_t len)
{
    if(len >= sizeof(captured)) {
        len = sizeof(captured) - 1;
    }
    memcpy(captured, bf, len);
    captured[len] = 0;
    captured_len = len;
    return 0;
}

/***************************************************************************
 *  Mimic the logcenter: parse the captured record as JSON.
 *  jansson rejects invalid UTF-8, so a successful parse proves the
 *  record is both valid JSON and valid UTF-8.
 ***************************************************************************/
PRIVATE void check_record_parses(const char *name, const char *expected_str_substr)
{
    json_t *jn = anystring2json(captured, captured_len, FALSE);
    if(!jn) {
        printf("FAIL %-40s logcenter could NOT parse the record\n", name);
        global_result += -1;
        return;
    }
    if(expected_str_substr) {
        const char *s = json_string_value(json_object_get(jn, "str"));
        if(!s || !strstr(s, expected_str_substr)) {
            printf("FAIL %-40s missing readable substring '%s'\n", name, expected_str_substr);
            global_result += -1;
            json_decref(jn);
            return;
        }
    }
    printf("ok   %-40s record parses (valid JSON/UTF-8)\n", name);
    json_decref(jn);
}

/***************************************************************************
 *  Tests
 ***************************************************************************/
PRIVATE void test_invalid_utf8_payload(void)
{
    /*
     *  Real-world shape: valid JSON head + invalid UTF-8 + modem AT tail.
     *  0x8a is a lone continuation byte (invalid), 0xc2 not followed by a
     *  continuation byte (invalid).
     */
    const char bad[] =
        "{\"pm25\":3,\"temperature\":21.1,\"h\x8a\x6b\x4b\"deviceid\xc2:\"2605250001\"}AT+CGREG?";

    captured_len = 0;
    gobj_log_error(0, 0,
        "function",     "%s", "string2json",
        "msgset",       "%s", "Parameter",
        "msg",          "%s", "json_loads() FAILED",
        "str",          "%s", bad,
        NULL
    );
    check_record_parses("invalid-utf8 device payload", NULL);
}

PRIVATE void test_valid_utf8_preserved(void)
{
    /*
     *  Valid UTF-8 (é = 0xc3 0xa9, € = 0xe2 0x82 0xac) must stay readable,
     *  i.e. copied verbatim and parse back to the same text.
     */
    const char good[] = "café costs 5€ — ok";

    captured_len = 0;
    gobj_log_error(0, 0,
        "function",     "%s", "test",
        "msgset",       "%s", "Internal",
        "msg",          "%s", "valid utf8",
        "str",          "%s", good,
        NULL
    );
    check_record_parses("valid-utf8 preserved", "café costs 5€");

    /*
     *  And the bytes must be present verbatim in the raw record (not
     *  expanded into \u00XX escapes), so logs stay human-readable.
     */
    if(!strstr(captured, "café costs 5\xe2\x82\xac")) {
        printf("FAIL %-40s valid UTF-8 was not copied verbatim\n", "valid-utf8 verbatim");
        global_result += -1;
    } else {
        printf("ok   %-40s valid UTF-8 copied verbatim\n", "valid-utf8 verbatim");
    }
}

PRIVATE void test_control_chars_escaped(void)
{
    /*
     *  Embedded control characters (0x1b ESC, newline) must be escaped so
     *  the record stays valid JSON.
     */
    const char ctrl[] = "dev\x1bice\nline";

    captured_len = 0;
    gobj_log_error(0, 0,
        "function",     "%s", "test",
        "msgset",       "%s", "Internal",
        "msg",          "%s", "control chars",
        "str",          "%s", ctrl,
        NULL
    );
    check_record_parses("control chars escaped", NULL);
}

/***************************************************************************
 *      Main
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

    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

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

    gobj_log_register_handler(
        "capture",
        NULL,           // close_fn
        capture_write,  // write_fn
        NULL            // fwrite_fn
    );
    gobj_log_add_handler("capture", "capture", LOG_OPT_ALL, 0);

    /*----------------------------------*
     *      Tests
     *----------------------------------*/
    test_invalid_utf8_payload();
    test_valid_utf8_preserved();
    test_control_chars_escaped();

    gobj_end();

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result;
}
