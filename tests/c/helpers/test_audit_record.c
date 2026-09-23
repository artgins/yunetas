/****************************************************************************
 *          test_audit_record.c
 *
 *          The audit record of yuno_agent (yunos/c/yuno_agent/src/
 *          audit_record.c, compiled into this test):
 *          - a content64 is never written, only its size and sha256,
 *          - __md_iev__ is replaced by a short source summary,
 *          - a read-only command is recorded with command, date and user only,
 *          - the kw of the caller is not modified,
 *          and what one record costs, 7.25.4's way and the new way.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <yunetas.h>
#include "audit_record.h"

#define APP "test_audit_record"

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE int global_result = 0;
PRIVATE const char *DATE = "2026-09-24T10:00:00.000000000+0200";

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void check(BOOL ok, const char *name)
{
    if(ok) {
        printf("ok   %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        global_result += -1;
    }
}

/***************************************************************************
 *  The __md_iev__ of a command that came through the controlcenter from
 *  a gui_agent (the shape seen in the audit of wattyzer)
 ***************************************************************************/
PRIVATE json_t *md_iev_via_controlcenter(const char *command_line)
{
    return json_pack("{s:s, s:s, s:[{s:s, s:{}}], s:[{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s},"
                     " {s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}], s:s}",
        "console_purpose", "statnodes",
        "console_node", "wattyzer",
        "command_stack",
            "command", command_line,
            "kw",
        "ievent_gate_stack",
            "dst_yuno", "wattyzer",
            "dst_role", "yuneta_agent",
            "dst_service", "controlcenter",
            "src_yuno", "artgins.com",
            "src_role", "controlcenter",
            "src_service", "top-16",
            "user", "yuneta_agent@artgins.com",
            "host", "artgins",
            "input_service", "wattyzer",
            "input_channel", "controlcenter",

            "dst_yuno", "",
            "dst_role", "controlcenter",
            "dst_service", "controlcenter",
            "src_yuno", "gui_agent_yuno",
            "src_role", "gui_agent",
            "src_service", "agent_link",
            "host", "544f1345-65a6-455c-aa94-62b6b020b5c5",
            "input_service", "__top_side__",
            "input_channel", "top-16",
            "__username__", "claudia@artgins.com",
        "__msg_type__", "__command__"
    );
}

/***************************************************************************
 *  The record the agent wrote up to 7.25.4: the whole kw
 ***************************************************************************/
PRIVATE size_t size_of_old_record(const char *command, json_t *kw)
{
    json_t *jn = json_pack("{s:s, s:s, s:O}",
        "command", command,
        "date", DATE,
        "kw", kw
    );
    char *s = json2uglystr(jn);
    size_t len = s? strlen(s): 0;
    GBMEM_FREE(s);
    JSON_DECREF(jn)
    return len;
}

PRIVATE size_t size_of_record(json_t *jn_record)
{
    char *s = json2uglystr(jn_record);
    size_t len = s? strlen(s): 0;
    GBMEM_FREE(s);
    return len;
}

/***************************************************************************
 *  (a) install-binary with a 32 MB binary: 43 MB of base64, three times
 ***************************************************************************/
PRIVATE void test_content64(void)
{
    size_t bin_len = 32*1024*1024;
    char *binary = gbmem_malloc(bin_len);
    for(size_t i=0; i<bin_len; i++) {
        binary[i] = (char)(i*2654435761u >> 13);
    }
    char expected_hex[SHA256_HEX_LEN + 1];
    sha256_hex(binary, bin_len, expected_hex, sizeof(expected_hex));

    gbuffer_t *gbuf_b64 = gbuffer_binary_to_base64(binary, bin_len);
    GBMEM_FREE(binary);
    const char *b64 = gbuffer_cur_rd_pointer(gbuf_b64);
    size_t b64_len = gbuffer_leftbytes(gbuf_b64);

    /*
     *  ycommand: content64 inside the command TEXT, in __command__, and in
     *  the command_stack of __md_iev__
     */
    gbuffer_t *gbuf_line = gbuffer_create(b64_len + 256, b64_len + 256);
    gbuffer_printf(gbuf_line, "install-binary id=auth_bff content64='");
    gbuffer_append(gbuf_line, (void *)b64, b64_len);
    gbuffer_printf(gbuf_line, "'");
    const char *line = gbuffer_cur_rd_pointer(gbuf_line);

    json_t *kw = json_pack("{s:s, s:s, s:o}",
        "__command__", line,
        "__username__", "yuneta",
        "__md_iev__", md_iev_via_controlcenter(line)
    );
    json_t *kw_copy = json_deep_copy(kw);

    size_t old_size = size_of_old_record(line, kw);
    uint64_t t0 = time_in_milliseconds_monotonic();
    json_t *jn_record = audit_record_build(line, kw, DATE);
    uint64_t t1 = time_in_milliseconds_monotonic();
    char *record = json2uglystr(jn_record);
    size_t new_size = record? strlen(record): 0;

    char expected[256];
    snprintf(expected, sizeof(expected), "content64='<%zu bytes sha256:%s>'", bin_len, expected_hex);
    check(record && strstr(record, expected) != NULL,
        "(a) content64 in the command text: size + sha256 of the binary");
    check(record && strstr(record, "f0VMRgIB") == NULL && new_size < 4096,
        "(a) no base64 in the record");
    check(json_equal(kw, kw_copy), "(a) the kw of the caller is not modified");
    printf("     install-binary record: %zu bytes up to 7.25.4, %zu bytes now (%" PRIu64 " ms to build)\n",
        old_size, new_size, t1 - t0);
    if(record) {
        printf("     %s\n", record);
    }
    GBMEM_FREE(record);
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)
    JSON_DECREF(kw_copy)

    /*
     *  A gui/controlcenter: content64 as a kw KEY
     */
    kw = json_pack("{s:s, s:s#, s:s}",
        "id", "auth_bff",
        "content64", b64, (int)b64_len,
        "__username__", "claudia@artgins.com"
    );
    jn_record = audit_record_build("install-binary", kw, DATE);
    record = json2uglystr(jn_record);
    snprintf(expected, sizeof(expected), "\"content64\":\"<%zu bytes sha256:%s>\"", bin_len, expected_hex);
    check(record && strstr(record, expected) != NULL && strlen(record) < 1024,
        "(a) content64 as a kw key: size + sha256");
    GBMEM_FREE(record);
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    /*
     *  Not base64 (a path, as ycommand also accepts): hashed as text
     */
    kw = json_pack("{s:s}", "content64", "/yuneta/bin/auth_bff?");
    jn_record = audit_record_build("install-binary", kw, DATE);
    record = json2uglystr(jn_record);
    check(record && strstr(record, "/yuneta/bin/auth_bff") == NULL &&
        strstr(record, "not base64") != NULL,
        "(a) a content64 that is not base64: never written either");
    GBMEM_FREE(record);
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    GBUFFER_DECREF(gbuf_line)
    GBUFFER_DECREF(gbuf_b64)
}

/***************************************************************************
 *  (b) __md_iev__ becomes a short source summary
 ***************************************************************************/
PRIVATE void test_source_summary(void)
{
    json_t *kw = json_pack("{s:s, s:s, s:s, s:o}",
        "id", "gate_mqtts",
        "cmd2agent", "run-yuno",
        "__username__", "claudia@artgins.com",
        "__md_iev__", md_iev_via_controlcenter("run-yuno id=gate_mqtts")
    );
    json_t *jn_record = audit_record_build("run-yuno", kw, DATE);
    json_t *jn_kw = json_object_get(jn_record, "kw");
    json_t *jn_source = json_object_get(jn_record, "source");
    json_t *jn_hops = json_object_get(jn_source, "hops");

    check(jn_kw && !json_object_get(jn_kw, "__md_iev__"), "(b) no __md_iev__ in the record");
    check(json_is_string(json_object_get(jn_record, "user")) &&
        strcmp(json_string_value(json_object_get(jn_record, "user")), "claudia@artgins.com") == 0,
        "(b) the user is the end user (__username__)");
    check(jn_source &&
        strcmp(kw_get_str(0, jn_source, "console_purpose", "", 0), "statnodes") == 0 &&
        json_array_size(jn_hops) == 2 &&
        strcmp(kw_get_str(0, json_array_get(jn_hops, 0), "role", "", 0), "controlcenter") == 0 &&
        strcmp(kw_get_str(0, json_array_get(jn_hops, 0), "yuno", "", 0), "artgins.com") == 0 &&
        strcmp(kw_get_str(0, json_array_get(jn_hops, 0), "service", "", 0), "top-16") == 0 &&
        strcmp(kw_get_str(0, json_array_get(jn_hops, 1), "role", "", 0), "gui_agent") == 0,
        "(b) source: console purpose and each hop (role, yuno, service, user, host)");
    check(strcmp(kw_get_str(0, jn_kw, "id", "", 0), "gate_mqtts") == 0,
        "(b) the parameters stay");
    size_t old_size = size_of_old_record("run-yuno", kw);
    size_t new_size = size_of_record(jn_record);
    printf("     run-yuno via controlcenter: %zu bytes up to 7.25.4, %zu bytes now\n", old_size, new_size);
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)
}

/***************************************************************************
 *  (c) a read-only command: command, date and user, nothing else
 ***************************************************************************/
PRIVATE BOOL only_command_date_user(json_t *jn_record)
{
    return json_object_size(jn_record) == 3 &&
        json_object_get(jn_record, "command") &&
        json_object_get(jn_record, "date") &&
        json_object_get(jn_record, "user");
}

PRIVATE void test_read_only(void)
{
    const char *read_only[] = {
        "list-yunos", "view-config", "stats-yuno", "services", "nodes", "treedb-info",
        "help", "topics", "get-gobj-trace", "dir-logs", "top", "ping", 0
    };
    for(int i=0; read_only[i]; i++) {
        json_t *kw = json_pack("{s:s, s:o}",
            "__username__", "claudia@artgins.com",
            "__md_iev__", md_iev_via_controlcenter(read_only[i])
        );
        json_t *jn_record = audit_record_build(read_only[i], kw, DATE);
        char name[128];
        snprintf(name, sizeof(name), "(c) %s: command, date and user only", read_only[i]);
        check(only_command_date_user(jn_record), name);
        if(i == 0) {
            size_t old_size = size_of_old_record(read_only[i], kw);
            size_t new_size = size_of_record(jn_record);
            printf("     list-yunos via controlcenter: %zu bytes up to 7.25.4, %zu bytes now\n",
                old_size, new_size);
        }
        JSON_DECREF(jn_record)
        JSON_DECREF(kw)
    }

    /*
     *  Wrappers: by the command they carry
     */
    json_t *kw = json_pack("{s:s, s:s, s:s}",
        "id", "gate_mqtts", "service", "__yuno__", "command", "view-attrs"
    );
    json_t *jn_record = audit_record_build("command-yuno", kw, DATE);
    check(only_command_date_user(jn_record) &&
        strstr(kw_get_str(0, jn_record, "command", "", 0), "view-attrs") != NULL,
        "(c) command-yuno command=view-attrs: minimal, and it says view-attrs");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    jn_record = audit_record_build("command-yuno id=gate_mqtts service=__yuno__ command=view-attrs", NULL, DATE);
    check(only_command_date_user(jn_record), "(c) the same as a command line: minimal");
    JSON_DECREF(jn_record)

    kw = json_pack("{s:s, s:s, s:s, s:s}",
        "id", "gate_mqtts", "service", "__yuno__", "command", "write-attr", "attribute", "x"
    );
    jn_record = audit_record_build("command-yuno", kw, DATE);
    check(!only_command_date_user(jn_record) && json_object_get(jn_record, "kw"),
        "(c) command-yuno command=write-attr: the full record");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    const char *not_read_only[] = {
        "run-yuno", "kill-yuno", "install-binary", "write-attr", "create-node",
        "set-global-trace", "find-new-yunos", "read-file", "check-user-pwd", 0
    };
    for(int i=0; not_read_only[i]; i++) {
        jn_record = audit_record_build(not_read_only[i], NULL, DATE);
        char name[128];
        snprintf(name, sizeof(name), "(c) %s: the full record", not_read_only[i]);
        check(!only_command_date_user(jn_record) && json_object_get(jn_record, "kw"), name);
        JSON_DECREF(jn_record)
    }
}

/***************************************************************************
 *  What one record costs (the agent serializes it too)
 ***************************************************************************/
PRIVATE void test_cost(void)
{
    json_t *kw = json_pack("{s:s, s:s, s:o}",
        "agent_id", "wattyzer",
        "__username__", "claudia@artgins.com",
        "__md_iev__", md_iev_via_controlcenter("list-yunos")
    );
    const char *commands[] = {"list-yunos", "run-yuno"};
    for(int c=0; c<2; c++) {
        int n = 100000;
        uint64_t t0 = time_in_milliseconds_monotonic();
        for(int i=0; i<n; i++) {
            json_t *jn = json_pack("{s:s, s:s, s:O}", "command", commands[c], "date", DATE, "kw", kw);
            char *s = json2uglystr(jn);
            GBMEM_FREE(s);
            JSON_DECREF(jn)
        }
        uint64_t t1 = time_in_milliseconds_monotonic();
        for(int i=0; i<n; i++) {
            json_t *jn = audit_record_build(commands[c], kw, DATE);
            char *s = json2uglystr(jn);
            GBMEM_FREE(s);
            JSON_DECREF(jn)
        }
        uint64_t t2 = time_in_milliseconds_monotonic();
        printf("     %s: %.2f us per record up to 7.25.4, %.2f us now (build + serialize)\n",
            commands[c], (double)(t1-t0)*1000.0/n, (double)(t2-t1)*1000.0/n);
    }
    JSON_DECREF(kw)
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

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

    gbmem_setup(
        512*1024*1024L,         // max_block: a 130 MB record of 7.25.4 is measured
        4*1024*1024*1024L,      // max_system_memory
        FALSE,
        0,
        0
    );

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

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

    test_content64();
    test_source_summary();
    test_read_only();
    test_cost();

    gobj_end();

    int result = global_result;
    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }
    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    } else {
        printf("\n%s: PASS\n", APP);
    }
    return result<0?-1:0;
}
