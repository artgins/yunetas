/****************************************************************************
 *          test_audit_record.c
 *
 *          The audit record of yuno_agent (yunos/c/yuno_agent/src/
 *          audit_record.c, compiled into this test):
 *          - a content64 is never written, only its size and sha256,
 *          - __md_iev__ is replaced by a short source summary,
 *          - a read-only command is recorded with command, date and user only,
 *          - the kw of the caller is not modified,
 *          - a secret (password, token, client secret, ...) is never written,
 *          - a stats command with a reset is a write,
 *          - the command carried by command-yuno is the one the parser takes,
 *          - bad data from a peer gives warnings, never an error,
 *          - a console write keeps only the fact (who, when, which
 *            console, how many bytes): one record per burst,
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
PRIVATE int s_errors = 0;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int count_errors(void *h, int priority, const char *bf, size_t len)
{
    s_errors++;
    return 0;
}

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
 *  The record, serialized: "" if none. Free with GBMEM_FREE().
 ***************************************************************************/
PRIVATE char *record_text(json_t *jn_record)
{
    char *s = jn_record? json2uglystr(jn_record): NULL;
    return s? s: gbmem_strdup("");
}

PRIVATE BOOL record_holds(json_t *jn_record, const char *text)
{
    char *s = record_text(jn_record);
    BOOL found = strstr(s, text)? TRUE: FALSE;
    GBMEM_FREE(s);
    return found;
}

/***************************************************************************
 *  (d) a secret is never written: passwords, tokens, client secrets, ...
 ***************************************************************************/
PRIVATE void check_no_secret(const char *command, json_t *kw, const char *secret, const char *name)
{
    json_t *jn_record = audit_record_build(command, kw, DATE);
    char *s = record_text(jn_record);
    BOOL ok = jn_record && !strstr(s, secret) && strstr(s, "<redacted>");
    check(ok, name);
    if(!ok) {
        printf("     %s\n", s);
    }
    GBMEM_FREE(s);
    JSON_DECREF(jn_record)
}

PRIVATE void test_secrets(void)
{
    check_no_secret("set-user-pwd username=bob password=hunter2", NULL, "hunter2",
        "(d) set-user-pwd password= in the command text");
    check_no_secret("check-user-pwd username=bob password = hunter2", NULL, "hunter2",
        "(d) password = with blanks around '='");
    check_no_secret("check-user-pwd username=bob password='hunter2 two words'", NULL, "hunter2",
        "(d) password='quoted value'");
    check_no_secret("check-user-pwd username=bob password=\"hunter2 two\"", NULL, "hunter2",
        "(d) password=\"double quoted value\"");

    json_t *kw = json_pack("{s:s, s:s}", "username", "bob", "password", "hunter2");
    check_no_secret("check-user-pwd", kw, "hunter2", "(d) check-user-pwd: kw.password");
    JSON_DECREF(kw)

    const char *secret_keys[] = {
        "password", "passw", "user_passw", "pwd", "secret", "client_secret",
        "kc_admin_client_secret", "token", "access_token", "jwt", "private_key",
        "privkey", "Password", 0
    };
    for(int i=0; secret_keys[i]; i++) {
        char name[128];
        snprintf(name, sizeof(name), "(d) a kw key named %s", secret_keys[i]);
        kw = json_pack("{s:s, s:{s:s}}", secret_keys[i], "s3cr3tvalue", "nested", secret_keys[i], "s3cr3tvalue");
        check_no_secret("set-kc-config", kw, "s3cr3tvalue", name);
        JSON_DECREF(kw)

        char command[256];
        snprintf(command, sizeof(command), "set-kc-config %s=s3cr3tvalue kc_realm=x", secret_keys[i]);
        snprintf(name, sizeof(name), "(d) %s= in the command text", secret_keys[i]);
        check_no_secret(command, NULL, "s3cr3tvalue", name);
    }

    kw = json_pack("{s:i}", "password", 1234567);
    check_no_secret("set-user-pwd", kw, "1234567", "(d) a secret that is not a string");
    JSON_DECREF(kw)

    check_no_secret(
        "command-yuno id=authz service=authz command='set-user-pwd username=bob password=hunter2'",
        NULL, "hunter2", "(d) in the command carried by command-yuno (text)"
    );
    kw = json_pack("{s:s, s:s, s:s}",
        "id", "authz", "service", "authz", "command", "set-user-pwd username=bob password=hunter2"
    );
    check_no_secret("command-yuno", kw, "hunter2", "(d) in the command carried by command-yuno (kw)");
    JSON_DECREF(kw)

    check_no_secret(
        "update-node topic_name=users content='{\"id\":\"bob\",\"password\":\"hunter2\",\"n\":1}'",
        NULL, "hunter2", "(d) in a json given in the command text"
    );
    kw = json_pack("{s:s, s:s}", "topic_name", "users",
        "content", "{\"id\": \"bob\", \"client_secret\": \"hunter2\"}"
    );
    check_no_secret("update-node", kw, "hunter2", "(d) in a json given as a kw string");
    JSON_DECREF(kw)

    check_no_secret(
        "write-attr gobj=idp attribute=kc_admin_client_secret value=hunter2",
        NULL, "hunter2", "(d) write-attr of a secret attribute: its value"
    );
    kw = json_pack("{s:s, s:s, s:s}", "gobj", "idp", "attribute", "kc_admin_client_secret", "value", "hunter2");
    check_no_secret("write-attr", kw, "hunter2", "(d) write-attr of a secret attribute (kw)");
    JSON_DECREF(kw)

    /*
     *  Not a secret: stays
     */
    kw = json_pack("{s:s, s:s}", "username", "bob", "authz", "create-node");
    json_t *jn_record = audit_record_build("create-user", kw, DATE);
    check(record_holds(jn_record, "\"bob\"") && record_holds(jn_record, "create-node") &&
        !record_holds(jn_record, "<redacted>"),
        "(d) what is not a secret stays");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)
}

/***************************************************************************
 *  (e) a stats command with a reset is a write: the full record
 ***************************************************************************/
PRIVATE void test_stats_reset(void)
{
    json_t *kw = json_pack("{s:s, s:s, s:s, s:o}",
        "id", "gate_x", "stats", "__reset__", "__username__", "claudia@artgins.com",
        "__md_iev__", md_iev_via_controlcenter("stats-yuno")
    );
    json_t *jn_record = audit_record_build("stats-yuno", kw, DATE);
    check(json_object_get(jn_record, "source") && json_object_get(jn_record, "kw") &&
        record_holds(jn_record, "__reset__"),
        "(e) stats-yuno with kw stats=__reset__: the full record, with source");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    jn_record = audit_record_build("stats-agent stats=__reset__", NULL, DATE);
    check(!only_command_date_user(jn_record), "(e) stats-agent stats=__reset__ (text): the full record");
    JSON_DECREF(jn_record)

    jn_record = audit_record_build("stats-yuno id=\"gate_x\" stats=\"__reset__\"", NULL, DATE);
    check(!only_command_date_user(jn_record), "(e) stats-yuno stats=\"__reset__\" (gui_agent): the full record");
    JSON_DECREF(jn_record)

    kw = json_pack("{s:s, s:s}", "command", "stats-yuno", "stats", "__reset__");
    jn_record = audit_record_build("command-yuno", kw, DATE);
    check(!only_command_date_user(jn_record), "(e) command-yuno command=stats-yuno with a reset: the full record");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    jn_record = audit_record_build("stats-yuno id=gate_x", NULL, DATE);
    check(only_command_date_user(jn_record), "(e) stats-yuno without a reset: still read-only");
    JSON_DECREF(jn_record)
}

/***************************************************************************
 *  (f) the parser trims blanks around the key: so does the redaction
 ***************************************************************************/
PRIVATE void test_content64_blank_before_equal(void)
{
    const char *b64 = "f0VMRgIBAQAAAAAAAAAAAAIAPgABAAAAAAAAAAAAAAA=";
    const char *forms[] = {
        "install-binary id=auth_bff content64 =%s",
        "install-binary id=auth_bff\tcontent64\t=%s",
        "install-binary id=auth_bff content64= %s",
        "install-binary id=auth_bff content64='%s",     // a quote that never ends
        0
    };
    for(int i=0; forms[i]; i++) {
        char line[512];
        snprintf(line, sizeof(line), forms[i], b64);
        json_t *jn_record = audit_record_build(line, NULL, DATE);
        char name[128];
        snprintf(name, sizeof(name), "(f) no base64 in the record: %.40s", forms[i]);
        check(jn_record && !record_holds(jn_record, "f0VMRgIB"), name);
        JSON_DECREF(jn_record)
    }
}

/***************************************************************************
 *  (g) many short content64 values: no overflow, nothing cut, no log
 ***************************************************************************/
PRIVATE void test_short_content64_values(void)
{
    const char *line = "update-binary id=x content64=QQ== content64=QQ== content64=QQ== "
        "content64=QQ== content64=QQ== id2=tail";
    int errors = s_errors;
    json_t *jn_record = audit_record_build(line, NULL, DATE);
    const char *command = kw_get_str(0, jn_record, "command", "", 0);
    check(strstr(command, "id2=tail") != NULL && strstr(command, "QQ==") == NULL,
        "(g) 5 short content64 values: all redacted, the end of the command kept");
    check(s_errors == errors, "(g) 5 short content64 values: no error logged");
    JSON_DECREF(jn_record)
}

/***************************************************************************
 *  (h) a long read-only command is not cut
 ***************************************************************************/
PRIVATE void test_long_read_only(void)
{
    gbuffer_t *gbuf = gbuffer_create(8192, 8192);
    gbuffer_printf(gbuf, "nodes treedb_name=treedb_x topic_name=users filter='{\"id\":[");
    while(gbuffer_leftbytes(gbuf) < 6000) {
        gbuffer_printf(gbuf, "\"user%05zu\",", gbuffer_leftbytes(gbuf));
    }
    gbuffer_printf(gbuf, "\"last\"]}' options='{\"list_dict\":true}'");
    const char *line = gbuffer_cur_rd_pointer(gbuf);

    json_t *jn_record = audit_record_build(line, NULL, DATE);
    const char *command = kw_get_str(0, jn_record, "command", "", 0);
    check(only_command_date_user(jn_record) && strcmp(command, line) == 0,
        "(h) a read-only command of 6000 chars is recorded whole");
    JSON_DECREF(jn_record)
    GBUFFER_DECREF(gbuf)
}

/***************************************************************************
 *  (i) command-yuno: the command that RUNS is the one of the text
 ***************************************************************************/
PRIVATE void test_wrapper_text_wins(void)
{
    json_t *kw = json_pack("{s:s, s:s}", "command", "list-yunos", "__username__", "claudia@artgins.com");
    json_t *jn_record = audit_record_build(
        "command-yuno id=treedb service=treedb_x command='delete-node topic_name=users id=bob force=1'",
        kw, DATE
    );
    check(!only_command_date_user(jn_record) && json_object_get(jn_record, "kw"),
        "(i) command-yuno: kw.command=list-yunos, text command='delete-node': the full record");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    kw = json_pack("{s:s}", "command", "delete-node id=bob");
    jn_record = audit_record_build("command-yuno id=treedb command=list-yunos", kw, DATE);
    check(only_command_date_user(jn_record),
        "(i) command-yuno: text command=list-yunos wins over kw.command: minimal");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)
}

/***************************************************************************
 *  (j) bad data from a peer: warnings, never an ERROR on the audit path
 ***************************************************************************/
PRIVATE void test_peer_bad_data(void)
{
    int errors = s_errors;
    json_t *kw = json_pack("{s:s, s:{s:[{s:s, s:i, s:[]}]}}", "id", "x",
        "__md_iev__", "ievent_gate_stack",
            "src_role", "controlcenter", "user", 5, "host"
    );
    json_t *jn_record = audit_record_build("run-yuno", kw, DATE);
    check(jn_record && s_errors == errors, "(j) a hop with fields that are not strings: no error");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    kw = json_pack("{s:s, s:{s:s}}", "id", "x", "__md_iev__", "ievent_gate_stack", "not a list");
    jn_record = audit_record_build("run-yuno", kw, DATE);
    check(jn_record && s_errors == errors, "(j) an ievent_gate_stack that is not a list: no error");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    jn_record = audit_record_build("install-binary id=x content64=AB==", NULL, DATE);
    check(jn_record && s_errors == errors && record_holds(jn_record, "not base64"),
        "(j) content64=AB== (bad base64): no error, said as not base64");
    JSON_DECREF(jn_record)
}

/***************************************************************************
 *  (k) a console keystroke: nothing of what is typed, not even a hash
 ***************************************************************************/
PRIVATE void test_tty_keystroke_in_a_wrapper(void)
{
    json_t *kw = json_pack("{s:s, s:s, s:s}", "command", "write-tty", "name", "console-1", "content64", "cg==");
    json_t *jn_record = audit_record_build("command-agent", kw, DATE);
    check(jn_record && !record_holds(jn_record, "sha256") && !record_holds(jn_record, "cg==") &&
        record_holds(jn_record, "<1 bytes>"),
        "(k) command-agent command=write-tty: the size of the keystroke only");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)
}

/***************************************************************************
 *  (l) console writes: one record per burst, nothing of what is typed
 ***************************************************************************/
PRIVATE json_t *keystroke_kw(const char *user, const char *console, const char *key)
{
    gbuffer_t *gbuf = gbuffer_binary_to_base64(key, strlen(key));
    json_t *kw = json_pack("{s:s, s:s, s:s, s:s, s:s, s:o}",
        "agent_id", "wattyzer",
        "cmd2agent", "write-tty",
        "name", console,
        "content64", (const char *)gbuffer_cur_rd_pointer(gbuf),
        "__username__", user,
        "__md_iev__", md_iev_via_controlcenter("write-tty")
    );
    GBUFFER_DECREF(gbuf)
    return kw;
}

PRIVATE BOOL type_keys(json_t *jn_bursts, const char *user, const char *console,
    const char *keys, unsigned burst_seconds, json_t *jn_records)
{
    BOOL all_tty = TRUE;
    for(size_t i=0; keys[i]; i++) {
        char key[2] = {keys[i], 0};
        json_t *kw = keystroke_kw(user, console, key);
        if(!audit_tty_command(jn_bursts, "write-tty", kw, DATE, burst_seconds, jn_records)) {
            all_tty = FALSE;
        }
        JSON_DECREF(kw)
    }
    return all_tty;
}

PRIVATE json_int_t sum_of(json_t *jn_records, const char *field)
{
    json_int_t sum = 0;
    size_t idx;
    json_t *jn_record;
    json_array_foreach(jn_records, idx, jn_record) {
        sum += json_integer_value(json_object_get(jn_record, field));
    }
    return sum;
}

PRIVATE void test_tty_bursts(void)
{
    const char *typed = "rm -rf /x\r";
    json_t *jn_bursts = json_object();
    json_t *jn_records = json_array();

    /*
     *  A burst: its first write at once, the rest when the burst ends
     */
    BOOL all_tty = type_keys(jn_bursts, "claudia@artgins.com", "console-1", typed, 3600, jn_records);
    check(all_tty, "(l) write-tty is taken by audit_tty_command()");
    json_t *jn_first = json_array_get(jn_records, 0);
    check(json_array_size(jn_records) == 1 &&
        strcmp(kw_get_str(0, jn_first, "command", "", 0), "write-tty") == 0 &&
        strcmp(kw_get_str(0, jn_first, "user", "", 0), "claudia@artgins.com") == 0 &&
        strcmp(kw_get_str(0, jn_first, "console", "", 0), "console-1") == 0 &&
        strcmp(kw_get_str(0, jn_first, "date", "", 0), DATE) == 0 &&
        kw_get_int(0, jn_first, "writes", 0, 0) == 1 &&
        kw_get_int(0, jn_first, "bytes", 0, 0) == 1 &&
        json_object_get(jn_first, "source"),
        "(l) the first write of a burst is recorded at once: who, when, which console, bytes"
    );

    json_t *kw = json_pack("{s:s}", "__username__", "claudia@artgins.com");
    BOOL taken = audit_tty_command(jn_bursts, "list-yunos", kw, DATE, 3600, jn_records);
    check(!taken && json_array_size(jn_records) == 1,
        "(l) another command: not taken, the burst goes on");
    taken = audit_tty_command(jn_bursts, "close-console name=console-1", kw, DATE, 3600, jn_records);
    JSON_DECREF(kw)
    json_t *jn_tail = json_array_get(jn_records, 1);
    check(!taken && json_array_size(jn_records) == 2 &&
        kw_get_int(0, jn_tail, "writes", 0, 0) == (json_int_t)strlen(typed) - 1 &&
        kw_get_int(0, jn_tail, "bytes", 0, 0) == (json_int_t)strlen(typed) - 1 &&
        json_object_get(jn_tail, "until") &&
        json_object_size(jn_bursts) == 0,
        "(l) close-console ends the burst: one record for the other writes"
    );
    check(sum_of(jn_records, "writes") == (json_int_t)strlen(typed) &&
        sum_of(jn_records, "bytes") == (json_int_t)strlen(typed),
        "(l) the records of a burst count every write and byte once"
    );

    /*
     *  Nothing of what was typed, not even a hash
     */
    BOOL clean = TRUE;
    size_t idx;
    json_t *jn_record;
    json_array_foreach(jn_records, idx, jn_record) {
        if(record_holds(jn_record, "sha256") || record_holds(jn_record, "content64") ||
                record_holds(jn_record, "cm0") || record_holds(jn_record, "rm -rf")) {
            clean = FALSE;
        }
    }
    check(clean, "(l) no content64, no hash, nothing of what was typed");
    json_array_clear(jn_records);

    /*
     *  Another user into the same console: another burst
     */
    type_keys(jn_bursts, "claudia@artgins.com", "console-1", "ab", 3600, jn_records);
    type_keys(jn_bursts, "intruder@example.com", "console-1", "cd", 3600, jn_records);
    check(json_array_size(jn_records) == 3 &&
        strcmp(kw_get_str(0, json_array_get(jn_records, 2), "user", "", 0), "intruder@example.com") == 0,
        "(l) another user writes into the console: recorded at once, the burst before is closed"
    );
    audit_tty_close_bursts(jn_bursts, jn_records);
    check(json_array_size(jn_records) == 4 && json_object_size(jn_bursts) == 0,
        "(l) audit_tty_close_bursts() (the agent stops) ends every burst");
    json_array_clear(jn_records);

    /*
     *  The interval: a burst ends when its time has passed
     */
    type_keys(jn_bursts, "claudia@artgins.com", "console-2", "abc", 0, jn_records);
    check(json_array_size(jn_records) >= 2,
        "(l) a burst of 0 seconds ends at the next command");
    json_array_clear(jn_records);
    audit_tty_close_bursts(jn_bursts, jn_records);
    json_array_clear(jn_records);

    /*
     *  What it saves: a typed session of 1000 keystrokes
     */
    char session[1001];
    for(int i=0; i<1000; i++) {
        session[i] = (char)('a' + (i % 26));
    }
    session[1000] = 0;
    size_t old_size = 0;
    for(int i=0; i<1000; i++) {
        char key[2] = {session[i], 0};
        kw = keystroke_kw("claudia@artgins.com", "console-3", key);
        old_size += size_of_old_record("write-tty", kw);
        JSON_DECREF(kw)
    }
    type_keys(jn_bursts, "claudia@artgins.com", "console-3", session, 3600, jn_records);
    audit_tty_close_bursts(jn_bursts, jn_records);
    size_t new_size = 0;
    json_array_foreach(jn_records, idx, jn_record) {
        new_size += size_of_record(jn_record);
    }
    printf("     1000 keystrokes in one burst: %zu bytes up to 7.25.4, %zu bytes now (%zu records)\n",
        old_size, new_size, json_array_size(jn_records));

    JSON_DECREF(jn_records)
    JSON_DECREF(jn_bursts)
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
    gobj_log_register_handler("count_errors", 0, count_errors, 0);
    gobj_log_add_handler("count_errors", "count_errors", LOG_OPT_UP_ERROR, 0);

    test_content64();
    test_source_summary();
    test_read_only();
    test_secrets();
    test_stats_reset();
    test_content64_blank_before_equal();
    test_short_content64_values();
    test_long_read_only();
    test_wrapper_text_wins();
    test_peer_bad_data();
    test_tty_keystroke_in_a_wrapper();
    test_tty_bursts();
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
