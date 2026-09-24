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
 *          - the command carried by command-yuno is the one its handler
 *            reads (the key exactly `command`; COMMAND=... is not it),
 *          - bad data from a peer gives warnings, never an error,
 *          - a console write keeps only the fact (who, when, which
 *            console, how many bytes): one record per burst,
 *          - a text made to be hard to scan, and random texts: linear
 *            time, no crash; a text beyond the cap is not written,
 *          - the command word is the one the parser takes (any case,
 *            quotes, aliases), and more secret names (api_key, ...),
 *            json keys with escapes, Bearer tokens and JWTs,
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

/*
 *  The entries of the command table of the agent (c_agent.c) that the
 *  audit tells apart: names, aliases (as the parser takes them)
 */
PRIVATE const char *a_write_tty[] = {"EV_WRITE_TTY", 0};
PRIVATE const char *a_help[] = {"h", "?", 0};
PRIVATE const char *a_list_yunos[] = {"1", 0};
PRIVATE sdata_desc_t command_table[] = {
/*-CMD2--type-----------name----------------flag----------------alias---------------items-----------json_fn---------description---------- */
SDATACM2 (DTP_SCHEMA,   "help",             0,                  a_help,             0,              0,              "Command's help"),
SDATACM2 (DTP_SCHEMA,   "",                 0,                  0,                  0,              0,              "\nAgent\n-----------"),
SDATACM2 (DTP_SCHEMA,   "close-console",    0,                  0,                  0,              0,              "Close console"),
SDATACM2 (DTP_SCHEMA,   "command-agent",    SDF_WILD_CMD,       0,                  0,              0,              "Command to agent"),
SDATACM2 (DTP_SCHEMA,   "command-yuno",     SDF_WILD_CMD,       0,                  0,              0,              "Command to yuno"),
SDATACM2 (DTP_SCHEMA,   "write-tty",        0,                  a_write_tty,        0,              0,              "Write data to tty"),
SDATACM2 (DTP_SCHEMA,   "list-yunos",       0,                  a_list_yunos,       0,              0,              "List yunos"),
SDATACM2 (DTP_SCHEMA,   "run-yuno",         0,                  0,                  0,              0,              "Run yuno"),
SDATA_END()
};

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
    json_t *jn_record = audit_record_build(line, kw, DATE, command_table);
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
    jn_record = audit_record_build("install-binary", kw, DATE, command_table);
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
    jn_record = audit_record_build("install-binary", kw, DATE, command_table);
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
    json_t *jn_record = audit_record_build("run-yuno", kw, DATE, command_table);
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
        json_t *jn_record = audit_record_build(read_only[i], kw, DATE, command_table);
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
    json_t *jn_record = audit_record_build("command-yuno", kw, DATE, command_table);
    check(only_command_date_user(jn_record) &&
        strstr(kw_get_str(0, jn_record, "command", "", 0), "view-attrs") != NULL,
        "(c) command-yuno command=view-attrs: minimal, and it says view-attrs");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    jn_record = audit_record_build("command-yuno id=gate_mqtts service=__yuno__ command=view-attrs", NULL, DATE, command_table);
    check(only_command_date_user(jn_record), "(c) the same as a command line: minimal");
    JSON_DECREF(jn_record)

    kw = json_pack("{s:s, s:s, s:s, s:s}",
        "id", "gate_mqtts", "service", "__yuno__", "command", "write-attr", "attribute", "x"
    );
    jn_record = audit_record_build("command-yuno", kw, DATE, command_table);
    check(!only_command_date_user(jn_record) && json_object_get(jn_record, "kw"),
        "(c) command-yuno command=write-attr: the full record");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    const char *not_read_only[] = {
        "run-yuno", "kill-yuno", "install-binary", "write-attr", "create-node",
        "set-global-trace", "find-new-yunos", "read-file", "check-user-pwd", 0
    };
    for(int i=0; not_read_only[i]; i++) {
        jn_record = audit_record_build(not_read_only[i], NULL, DATE, command_table);
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
    json_t *jn_record = audit_record_build(command, kw, DATE, command_table);
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
    json_t *jn_record = audit_record_build("create-user", kw, DATE, command_table);
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
    json_t *jn_record = audit_record_build("stats-yuno", kw, DATE, command_table);
    check(json_object_get(jn_record, "source") && json_object_get(jn_record, "kw") &&
        record_holds(jn_record, "__reset__"),
        "(e) stats-yuno with kw stats=__reset__: the full record, with source");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    jn_record = audit_record_build("stats-agent stats=__reset__", NULL, DATE, command_table);
    check(!only_command_date_user(jn_record), "(e) stats-agent stats=__reset__ (text): the full record");
    JSON_DECREF(jn_record)

    jn_record = audit_record_build("stats-yuno id=\"gate_x\" stats=\"__reset__\"", NULL, DATE, command_table);
    check(!only_command_date_user(jn_record), "(e) stats-yuno stats=\"__reset__\" (gui_agent): the full record");
    JSON_DECREF(jn_record)

    kw = json_pack("{s:s, s:s}", "command", "stats-yuno", "stats", "__reset__");
    jn_record = audit_record_build("command-yuno", kw, DATE, command_table);
    check(!only_command_date_user(jn_record), "(e) command-yuno command=stats-yuno with a reset: the full record");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    jn_record = audit_record_build("stats-yuno id=gate_x", NULL, DATE, command_table);
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
        json_t *jn_record = audit_record_build(line, NULL, DATE, command_table);
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
    json_t *jn_record = audit_record_build(line, NULL, DATE, command_table);
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

    json_t *jn_record = audit_record_build(line, NULL, DATE, command_table);
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
        kw, DATE, command_table
    );
    check(!only_command_date_user(jn_record) && json_object_get(jn_record, "kw"),
        "(i) command-yuno: kw.command=list-yunos, text command='delete-node': the full record");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    kw = json_pack("{s:s}", "command", "delete-node id=bob");
    jn_record = audit_record_build("command-yuno id=treedb command=list-yunos", kw, DATE, command_table);
    check(only_command_date_user(jn_record),
        "(i) command-yuno: text command=list-yunos wins over kw.command: minimal");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    /*
     *  The key in another case is NOT the command that runs: the parser
     *  stores COMMAND=... under "COMMAND", the handler reads "command"
     */
    kw = json_pack("{s:s, s:s}", "command", "delete-yuno id=gate", "__username__", "mallory");
    jn_record = audit_record_build("command-yuno id=x COMMAND=list-yunos", kw, DATE, command_table);
    check(!only_command_date_user(jn_record) && json_object_get(jn_record, "kw") &&
        record_holds(jn_record, "delete-yuno"),
        "(i) command-yuno COMMAND=list-yunos, kw.command=delete-yuno: the full record, with delete-yuno");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    jn_record = audit_record_build(
        "command-yuno id=x command=write-attr attribute=api_key value=S3CR3T COMMAND=list-yunos",
        NULL, DATE, command_table
    );
    check(!only_command_date_user(jn_record) && !record_holds(jn_record, "S3CR3T"),
        "(i) command-yuno command=write-attr ... COMMAND=list-yunos: the full record, the secret redacted");
    JSON_DECREF(jn_record)

    kw = json_pack("{s:s}", "COMMAND", "delete-yuno id=gate");
    jn_record = audit_record_build("command-yuno id=x command=list-yunos", kw, DATE, command_table);
    check(!only_command_date_user(jn_record),
        "(i) command-yuno command=list-yunos with a kw key COMMAND: the full record");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    kw = json_pack("{s:s}", "COMMAND", "write-attr attribute=password value=S3CR3TVALUE");
    check_no_secret("command-yuno id=x command=list-yunos", kw, "S3CR3TVALUE",
        "(i) a kw key COMMAND (full record now): its write-attr secret is redacted");
    JSON_DECREF(kw)

    /*
     *  The console of a write-tty: the handler reads "name", not "NAME"
     */
    json_t *jn_bursts = json_object();
    json_t *jn_records = json_array();
    kw = json_pack("{s:s, s:s, s:s}", "name", "console-1", "content64", "cg==", "__username__", "mallory");
    audit_tty_command(jn_bursts, "write-tty NAME=decoy", kw, DATE, 3600, command_table, jn_records);
    JSON_DECREF(kw)
    const char *console = kw_get_str(0, json_array_get(jn_records, 0), "console", "", 0);
    check(json_array_size(jn_records) == 1 && strcmp(console, "console-1") == 0,
        "(i) write-tty NAME=decoy, kw.name=console-1: recorded on console-1, where the keystroke goes");
    if(strcmp(console, "console-1") != 0) {
        printf("     console recorded: '%s'\n", console);
    }
    audit_tty_close_bursts(jn_bursts, jn_records);
    JSON_DECREF(jn_records)
    JSON_DECREF(jn_bursts)
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
    json_t *jn_record = audit_record_build("run-yuno", kw, DATE, command_table);
    check(jn_record && s_errors == errors, "(j) a hop with fields that are not strings: no error");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    kw = json_pack("{s:s, s:{s:s}}", "id", "x", "__md_iev__", "ievent_gate_stack", "not a list");
    jn_record = audit_record_build("run-yuno", kw, DATE, command_table);
    check(jn_record && s_errors == errors, "(j) an ievent_gate_stack that is not a list: no error");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    jn_record = audit_record_build("install-binary id=x content64=AB==", NULL, DATE, command_table);
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
    json_t *jn_record = audit_record_build("command-agent", kw, DATE, command_table);
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
        if(!audit_tty_command(jn_bursts, "write-tty", kw, DATE, burst_seconds, command_table, jn_records)) {
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
    BOOL taken = audit_tty_command(jn_bursts, "list-yunos", kw, DATE, 3600, command_table, jn_records);
    check(!taken && json_array_size(jn_records) == 1,
        "(l) another command: not taken, the burst goes on");
    taken = audit_tty_command(jn_bursts, "close-console name=console-1", kw, DATE, 3600, command_table, jn_records);
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
 *  (m) A text made to be hard to scan: time linear in its size, no crash.
 *
 *  Up to 7.25.5 the scan went one level of recursion deeper for each '='
 *  in a run without blanks: "list-yunos x" + 100 000 '=' took 1.7 s, and
 *  150 000 crashed the agent (the audit runs before the parser and the
 *  authz, on any command of any peer).
 ***************************************************************************/
PRIVATE double seconds_to_build(const char *text, json_t *kw)
{
    uint64_t t0 = time_in_milliseconds_monotonic();
    json_t *jn_record = audit_record_build(text, kw, DATE, command_table);
    uint64_t t1 = time_in_milliseconds_monotonic();
    JSON_DECREF(jn_record)
    return (double)(t1 - t0)/1000.0;
}

/*
 *  "<head>" + `unit` repeated to `size` bytes
 */
PRIVATE char *repeated_text(const char *head, const char *unit, size_t size)
{
    size_t head_len = strlen(head);
    size_t unit_len = strlen(unit);
    char *s = gbmem_malloc(size + 1);
    if(!s) {
        return NULL;
    }
    memcpy(s, head, head_len);
    size_t n = head_len;
    while(n + unit_len <= size) {
        memcpy(s + n, unit, unit_len);
        n += unit_len;
    }
    s[n] = 0;
    return s;
}

PRIVATE void test_hard_texts(void)
{
    struct {
        const char *head;
        const char *unit;
    } shapes[] = {
        {"list-yunos x",                    "="},           // the shape of the crash
        {"run-yuno ",                       "a="},
        {"run-yuno x=",                     "'a="},
        {"run-yuno x=",                     "\"a="},
        {"run-yuno x='",                    "\"k\":"},
        {"run-yuno x='{",                   "\\\":"},
        {"run-yuno ",                       "attribute="},
        {"run-yuno ",                       "'password'="},
        {"run-yuno ",                       "eyJ"},
        {"run-yuno ",                       "Bearer "},
        {"run-yuno ",                       "\"a\" :"},
        {"run-yuno content='{\"password\":", "["},
        {0, 0}
    };
    size_t small = 100*1000;
    size_t big = 1000*1000;

    for(int i=0; shapes[i].head; i++) {
        char *s_small = repeated_text(shapes[i].head, shapes[i].unit, small);
        char *s_big = repeated_text(shapes[i].head, shapes[i].unit, big);
        double t_small = seconds_to_build(s_small, NULL);
        double t_big = seconds_to_build(s_big, NULL);

        /*
         *  The same text as a kw string of a write, too
         */
        json_t *kw = json_pack("{s:s}", "data", s_big);
        double t_kw = seconds_to_build("run-yuno", kw);
        JSON_DECREF(kw)

        char name[160];
        snprintf(name, sizeof(name), "(m) \"%s\" + \"%s\" x N: 1 MB in %.3f s (100 KB %.3f s, kw %.3f s)",
            shapes[i].head, shapes[i].unit, t_big, t_small, t_kw);
        check(t_big < 1.0 && t_kw < 1.0 && t_big <= 30*t_small + 0.05, name);
        GBMEM_FREE(s_small);
        GBMEM_FREE(s_big);
    }

    /*
     *  The size of the crash, ten times
     */
    char *s = repeated_text("list-yunos x", "=", 1500*1000);
    double t = seconds_to_build(s, NULL);
    char name[128];
    snprintf(name, sizeof(name), "(m) \"list-yunos x\" + 1 500 000 '=': no crash, %.3f s", t);
    check(t < 2.0, name);
    GBMEM_FREE(s);
}

/***************************************************************************
 *  (n) Random texts of the bytes that drive the scan: no crash, nothing
 *  left in memory, time linear, and a secret with a clear key never
 *  survives
 ***************************************************************************/
PRIVATE uint32_t s_rand = 0x2545F491;

PRIVATE uint32_t next_rand(void)
{
    s_rand ^= s_rand << 13;
    s_rand ^= s_rand >> 17;
    s_rand ^= s_rand << 5;
    return s_rand;
}

PRIVATE char *random_text(size_t size)
{
    static const char *pieces[] = {
        "=", "'", "\"", " ", ":", "\\", "{", "}", "[", "]", ",", ".", "\t",
        "a", "x", "password", "content64", "attribute", "value", "Bearer ", "eyJ",
        "command=", "write-tty", "QQ==", "__reset__", "api_key", "\\u0077",
        0
    };
    int n_pieces = 0;
    while(pieces[n_pieces]) {
        n_pieces++;
    }
    char *s = gbmem_malloc(size + 1);
    if(!s) {
        return NULL;
    }
    size_t n = 0;
    while(n < size) {
        const char *piece = pieces[next_rand() % (uint32_t)n_pieces];
        size_t len = strlen(piece);
        if(n + len > size) {
            break;
        }
        memcpy(s + n, piece, len);
        n += len;
    }
    s[n] = 0;
    return s;
}

PRIVATE void test_random_texts(void)
{
    int errors = s_errors;
    BOOL all_built = TRUE;
    BOOL no_secret = TRUE;
    uint64_t t0 = time_in_milliseconds_monotonic();
    for(int i=0; i<3000; i++) {
        char *s = random_text(1 + next_rand() % 2000);

        /*
         *  A secret with a clear key, somewhere in the text
         */
        gbuffer_t *gbuf = gbuffer_create(4096, 4096);
        gbuffer_printf(gbuf, "run-yuno %s password=S3CR3TVALUE %s", s, s);
        char *text = gbuffer_cur_rd_pointer(gbuf);

        json_t *kw = json_pack("{s:s, s:s}", "data", s, "token", "S3CR3TVALUE");
        json_t *jn_record = audit_record_build(text, kw, DATE, command_table);
        if(!jn_record) {
            all_built = FALSE;
        } else if(record_holds(jn_record, "S3CR3TVALUE")) {
            /*
             *  The random text may open a quote that never closes, and
             *  then password= is inside a value: it is still a key there
             */
            no_secret = FALSE;
            if(no_secret == FALSE) {
                char *r = record_text(jn_record);
                printf("     secret written: %.300s\n", r);
                GBMEM_FREE(r);
            }
        }
        JSON_DECREF(jn_record)
        JSON_DECREF(kw)

        json_t *jn_bursts = json_object();
        json_t *jn_records = json_array();
        audit_tty_command(jn_bursts, s, NULL, DATE, 60, command_table, jn_records);
        audit_tty_close_bursts(jn_bursts, jn_records);
        JSON_DECREF(jn_records)
        JSON_DECREF(jn_bursts)

        GBUFFER_DECREF(gbuf)
        GBMEM_FREE(s);
    }
    uint64_t t1 = time_in_milliseconds_monotonic();
    char name[128];
    snprintf(name, sizeof(name), "(n) 3000 random texts: every record built (%.2f s)", (double)(t1-t0)/1000.0);
    check(all_built && s_errors == errors, name);
    check(no_secret, "(n) 3000 random texts: password=<secret> never written");

    /*
     *  Linear: 1 MB against 100 KB of random text
     */
    char *s_small = random_text(100*1000);
    char *s_big = random_text(1000*1000);
    double t_small = seconds_to_build(s_small, NULL);
    double t_big = seconds_to_build(s_big, NULL);
    snprintf(name, sizeof(name), "(n) random text: 1 MB in %.3f s, 100 KB in %.3f s", t_big, t_small);
    check(t_big < 1.0 && t_big <= 30*t_small + 0.05, name);
    GBMEM_FREE(s_small);
    GBMEM_FREE(s_big);
}

/***************************************************************************
 *  (o) The work of one record has a cap: a text beyond it is not scanned
 *  and not written, only its first word, size and sha256
 ***************************************************************************/
PRIVATE void test_scan_budget(void)
{
    size_t size = 130*1024*1024;
    char *s = repeated_text("set-user-pwd password=S3CR3TVALUE ", "x", size);
    char hex[SHA256_HEX_LEN + 1];
    sha256_hex(s, strlen(s), hex, sizeof(hex));

    uint64_t t0 = time_in_milliseconds_monotonic();
    json_t *jn_record = audit_record_build(s, NULL, DATE, command_table);
    uint64_t t1 = time_in_milliseconds_monotonic();
    char expected[256];
    snprintf(expected, sizeof(expected), "set-user-pwd <%zu bytes, not scanned, sha256:%s>", strlen(s), hex);
    char name[160];
    snprintf(name, sizeof(name), "(o) a text of 130 MB: its word, size and sha256 only (%" PRIu64 " ms)", t1 - t0);
    check(jn_record && strcmp(kw_get_str(0, jn_record, "command", "", 0), expected) == 0, name);
    check(jn_record && !record_holds(jn_record, "S3CR3TVALUE"), "(o) a text of 130 MB: nothing of it written");
    JSON_DECREF(jn_record)

    json_t *kw = json_pack("{s:s}", "data", s);
    jn_record = audit_record_build("run-yuno", kw, DATE, command_table);
    check(jn_record && record_holds(jn_record, "not scanned") && !record_holds(jn_record, "S3CR3TVALUE"),
        "(o) a kw string of 130 MB: its size and sha256 only");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)
    GBMEM_FREE(s);
}

/***************************************************************************
 *  (p) The command word as the parser takes it: any case, quotes, aliases
 ***************************************************************************/
PRIVATE BOOL tty_taken(const char *text, json_t *kw)
{
    json_t *jn_bursts = json_object();
    json_t *jn_records = json_array();
    BOOL taken = audit_tty_command(jn_bursts, text, kw, DATE, 60, command_table, jn_records);
    JSON_DECREF(jn_records)
    JSON_DECREF(jn_bursts)
    return taken;
}

PRIVATE void test_command_word(void)
{
    /*
     *  Every name and alias of the table in several cases and quotes:
     *  write-tty for the audit exactly when the parser says write-tty
     */
    const char *words[] = {
        "write-tty", "WRITE-TTY", "Write-Tty", "EV_WRITE_TTY", "ev_write_tty", "Ev_Write_Tty",
        "'write-tty'", "\"Write-Tty\"", "'EV_WRITE_TTY'", "'write-tty", "write-tty'",
        "write_tty", "writetty", "close-console", "list-yunos", "1", "h", "?", "help",
        "command-agent", "", " ", "\twrite-tty", "  WRITE-TTY", 0
    };
    BOOL same = TRUE;
    for(int i=0; words[i]; i++) {
        char text[128];
        snprintf(text, sizeof(text), "%s name=c content64=QQ==", words[i]);
        const sdata_desc_t *cmd_desc = command_get_cmd_desc(command_table, text);
        BOOL parser_tty = (cmd_desc && strcmp(cmd_desc->name, "write-tty") == 0)? TRUE: FALSE;
        BOOL audit_tty = tty_taken(text, NULL);
        if(parser_tty != audit_tty) {
            same = FALSE;
            printf("     [%s]: the parser says %s, the audit says %s\n", text,
                parser_tty? "write-tty": "no", audit_tty? "write-tty": "no");
        }
    }
    check(same, "(p) write-tty for the audit exactly when the parser runs write-tty");

    /*
     *  The reviewer's forms: no hash of the keystroke anywhere
     */
    const char *hA = "559aead08264d5795d3909718cdd05abd49572e84fe55590eef31a88a08fdffd";   // sha256 of "A"
    const char *forms[] = {
        "EV_WRITE_TTY name=c content64=QQ==",
        "WRITE-TTY name=c content64=QQ==",
        "'write-tty' name=c content64=QQ==",
        "\"Write-Tty\" name=c content64=QQ==",
        "command-agent command='WRITE-TTY name=c content64=QQ=='",
        "command-agent command='EV_WRITE_TTY name=c content64=QQ=='",
        "COMMAND-AGENT command='write-tty name=c content64=QQ=='",
        "command-agent COMMAND='write-tty name=c content64=QQ=='",
        0
    };
    for(int i=0; forms[i]; i++) {
        json_t *jn_bursts = json_object();
        json_t *jn_records = json_array();
        if(!audit_tty_command(jn_bursts, forms[i], NULL, DATE, 60, command_table, jn_records)) {
            json_t *jn_record = audit_record_build(forms[i], NULL, DATE, command_table);
            if(jn_record) {
                json_array_append_new(jn_records, jn_record);
            }
        }
        audit_tty_close_bursts(jn_bursts, jn_records);
        char *s = json2uglystr(jn_records);
        char name[160];
        snprintf(name, sizeof(name), "(p) %s: no hash of the keystroke", forms[i]);
        check(s && !strstr(s, hA) && !strstr(s, "QQ=="), name);
        GBMEM_FREE(s);
        JSON_DECREF(jn_records)
        JSON_DECREF(jn_bursts)
    }

    json_t *kw = json_pack("{s:s, s:s}", "name", "c", "content64", "QQ==");
    check(tty_taken("Write-Tty", kw) && tty_taken("EV_WRITE_TTY", kw), "(p) Write-Tty, EV_WRITE_TTY with a kw: write-tty");
    JSON_DECREF(kw)

    /*
     *  CLOSE-CONSOLE ends the bursts too
     */
    json_t *jn_bursts = json_object();
    json_t *jn_records = json_array();
    kw = keystroke_kw("claudia@artgins.com", "console-1", "a");
    audit_tty_command(jn_bursts, "write-tty", kw, DATE, 3600, command_table, jn_records);
    audit_tty_command(jn_bursts, "write-tty", kw, DATE, 3600, command_table, jn_records);
    JSON_DECREF(kw)
    kw = json_pack("{s:s}", "__username__", "claudia@artgins.com");
    audit_tty_command(jn_bursts, "CLOSE-CONSOLE name=console-1", kw, DATE, 3600, command_table, jn_records);
    JSON_DECREF(kw)
    check(json_object_size(jn_bursts) == 0 && json_array_size(jn_records) == 2,
        "(p) CLOSE-CONSOLE ends the burst");
    JSON_DECREF(jn_records)
    JSON_DECREF(jn_bursts)

    /*
     *  audit_command_records(), what the agent calls: one lookup, the
     *  records of either kind
     */
    jn_bursts = json_object();
    jn_records = json_array();
    kw = keystroke_kw("claudia@artgins.com", "console-9", "a");
    int ret1 = audit_command_records(jn_bursts, "EV_WRITE_TTY", kw, DATE, 3600, command_table, jn_records);
    int ret2 = audit_command_records(jn_bursts, "Write-Tty", kw, DATE, 3600, command_table, jn_records);
    JSON_DECREF(kw)
    int ret3 = audit_command_records(jn_bursts, "1", NULL, DATE, 3600, command_table, jn_records);
    check(ret1 == 0 && ret2 == 0 && ret3 == 0 && json_array_size(jn_records) == 2 &&
        kw_get_int(0, json_array_get(jn_records, 0), "writes", 0, 0) == 1 &&
        only_command_date_user(json_array_get(jn_records, 1)),
        "(p) audit_command_records(): a keystroke (first of its burst), then list-yunos");
    audit_tty_close_bursts(jn_bursts, jn_records);
    check(json_array_size(jn_records) == 3, "(p) audit_command_records(): the burst counted the second write");
    JSON_DECREF(jn_records)
    JSON_DECREF(jn_bursts)

    /*
     *  An alias of a read-only command is read-only; case of a write-attr
     */
    json_t *jn_record = audit_record_build("1", NULL, DATE, command_table);
    check(only_command_date_user(jn_record), "(p) \"1\" (list-yunos): command, date and user only");
    JSON_DECREF(jn_record)

    check_no_secret("write-attr ATTRIBUTE=password VALUE=S3CR3TVALUE", NULL, "S3CR3TVALUE",
        "(p) ATTRIBUTE=password VALUE=<secret>");
    check_no_secret("command-yuno id=x command=write-attr Attribute=api_key Value=S3CR3TVALUE", NULL,
        "S3CR3TVALUE", "(p) Attribute=api_key Value=<secret>");
}

/***************************************************************************
 *  (q) More secret names (the attributes of the SDK and of the projects),
 *  and json keys written with escapes
 ***************************************************************************/
PRIVATE void test_more_secrets(void)
{
    const char *secret_keys[] = {
        "api_key", "apikey", "x-api-key", "API_KEY", "esios_api_key", "api.key",
        "http_cookie", "cookie", "__session_id__", "session_key", "auth_data",
        "passphrase", "credentials", "authorization", "visitor_salt", "assets_sign_secret",
        0
    };
    for(int i=0; secret_keys[i]; i++) {
        char name[128];
        snprintf(name, sizeof(name), "(q) a kw key named %s", secret_keys[i]);
        json_t *kw = json_pack("{s:s}", secret_keys[i], "S3CR3TVALUE");
        check_no_secret("set-config", kw, "S3CR3TVALUE", name);
        JSON_DECREF(kw)

        char command[256];
        snprintf(command, sizeof(command), "set-config %s=S3CR3TVALUE x=1", secret_keys[i]);
        snprintf(name, sizeof(name), "(q) %s= in the command text", secret_keys[i]);
        check_no_secret(command, NULL, "S3CR3TVALUE", name);
    }

    /*
     *  The runbook of wattyzer gate_pvpc
     */
    check_no_secret("command-yuno id=5006 command=write-attr attribute=api_key value=S3CR3TVALUE", NULL,
        "S3CR3TVALUE", "(q) command-yuno write-attr attribute=api_key value=<token>");
    json_t *kw = json_pack("{s:s, s:{s:s, s:s}}", "command", "write-attr",
        "kw", "attribute", "token", "value", "S3CR3TVALUE");
    check_no_secret("command-yuno id=x", kw, "S3CR3TVALUE", "(q) {attribute, value} in a nested kw object");
    JSON_DECREF(kw)

    /*
     *  Not secrets: they stay
     */
    kw = json_pack("{s:s, s:s, s:s, s:s, s:s}",
        "ssl_certificate_key", "/yuneta/store/certs/private/key.pem",
        "cert_pem", "-----BEGIN CERTIFICATE-----",
        "pkey", "id",
        "in_session", "yes",
        "authz", "create-node"
    );
    json_t *jn_record = audit_record_build("set-config", kw, DATE, command_table);
    check(!record_holds(jn_record, "<redacted>") && record_holds(jn_record, "key.pem"),
        "(q) key paths, certificates, pkey, in_session, authz: not secrets");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    /*
     *  Escapes in the name of a json key ("pass" "w" "ord")
     */
    check_no_secret(
        "command-yuno id=x service=s command=update-node topic_name=t record='{\"pass\\" "u0077ord\":\"S3CR3TVALUE\"}'",
        NULL, "S3CR3TVALUE", "(q) a json key with a \\u escape, in the text"
    );
    kw = json_pack("{s:s}", "record", "{\"\\" "u0070assword\":\"S3CR3TVALUE\"}");
    check_no_secret("update-node", kw, "S3CR3TVALUE", "(q) a json key with a \\u escape, in a kw string");
    JSON_DECREF(kw)
    kw = json_pack("{s:s}", "record", "{\"a\\\"b\":1, \"to\\" "u006ben\":\"S3CR3TVALUE\"}");
    check_no_secret("update-node", kw, "S3CR3TVALUE", "(q) an escaped quote before, a \\u escape in the key");
    JSON_DECREF(kw)

    /*
     *  A token by its shape
     */
    check_no_secret("x headers='Authorization: Bearer S3CR3TVALUE'", NULL, "S3CR3TVALUE",
        "(q) Bearer <token> after a plain key");
    kw = json_pack("{s:s}", "note", "use eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJTM0NSM1QifQ.S3CR3TVALUESIG now");
    check_no_secret("run-yuno", kw, "S3CR3TVALUE", "(q) a JWT in a string of a plain key");
    JSON_DECREF(kw)
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
            json_t *jn = audit_record_build(commands[c], kw, DATE, command_table);
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
    test_hard_texts();
    test_random_texts();
    test_scan_budget();
    test_command_word();
    test_more_secrets();
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
