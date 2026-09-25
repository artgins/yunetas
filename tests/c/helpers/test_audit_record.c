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
 *          - a json text inside a json text (escaped quotes, one level or
 *            more): its secrets are redacted too,
 *          - whatever quotes come before it, or cut it (a stray quote, a
 *            double-quoted parameter, the parser's quote ending at an
 *            escaped one), and in generated commands with every quoting
 *            shape: no secret is written,
 *          - an escaped json key with a blank or a slash, a quote inside a
 *            quoted secret value (\", the shell's '\''), a JWT at the end
 *            of a sentence, a write-attr whose names carry \u escapes or
 *            that comes as json text in a kw string: no secret is written
 *            (review 19),
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
    kw = json_pack("{s:s}", "data", "Authorization: Basic dXNlcjpwYXNz");    // user:pass
    check_no_secret("update-node", kw, "dXNlcjpwYXNz", "(q) Basic <base64 of user:password> in a text");
    JSON_DECREF(kw)
    kw = json_pack("{s:s}", "note", "Basic setup, basic QUJD and basic abc");  // QUJD: "ABC", no ':'
    jn_record = audit_record_build("update-node", kw, DATE, command_table);
    check(jn_record && !record_holds(jn_record, "<redacted>") && record_holds(jn_record, "basic QUJD"),
        "(q) the word basic, and base64 that is no user:password: not secrets");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)
    kw = json_pack("{s:s}", "note", "use eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJTM0NSM1QifQ.S3CR3TVALUESIG now");
    check_no_secret("run-yuno", kw, "S3CR3TVALUE", "(q) a JWT in a string of a plain key");
    JSON_DECREF(kw)
}

/***************************************************************************
 *  (r) A json given as text INSIDE a json given as text: its quotes are
 *  escaped (\"password\":\"x\"), once or more, and a secret there was
 *  written in clear. A quoted run with a backslash is also scanned with
 *  its escapes decoded, one level at a time.
 ***************************************************************************/
/*
 *  `text` as a json string literal, with its quotes
 */
PRIVATE char *json_quoted(const char *text)
{
    json_t *jn = json_string(text);
    char *s = jn? json_dumps(jn, JSON_ENCODE_ANY): NULL;
    JSON_DECREF(jn)
    return s;
}

/*
 *  {"a": <quoted inner>}, `levels` times around `inner`
 */
PRIVATE char *nested_json_text(const char *inner, int levels)
{
    char *s = gbmem_strdup(inner);
    for(int i=0; s && i<levels; i++) {
        char *q = json_quoted(s);
        gbmem_free(s);
        s = NULL;
        if(q) {
            size_t n = strlen(q) + 16;
            s = gbmem_malloc(n);
            if(s) {
                snprintf(s, n, "{\"a\":%s}", q);
            }
            gbmem_free(q);
        }
    }
    return s;
}

PRIVATE void test_json_text_in_json_text(void)
{
    /*
     *  The shapes of review 17 (P1, P4), and more levels
     */
    check_no_secret(
        "update-node topic_name=x content='{\"cfg\":\"{\\\"password\\\":\\\"P1LEAK\\\"}\"}'",
        NULL, "P1LEAK", "(r) json text in json text, in the command text");
    check_no_secret(
        "update-node topic_name=x content='{\"a\":\"{\\\"b\\\":\\\"{\\\\\\\"password\\\\\\\":\\\\\\\"P1DEEP\\\\\\\"}\\\"}\"}'",
        NULL, "P1DEEP", "(r) json text in json text in json text, in the command text");

    json_t *kw = json_pack("{s:s, s:s}", "username", "u",
        "cfg", "{\"a\":\"{\\\"token\\\":\\\"P4LEAK\\\"}\"}");
    check_no_secret("create-user", kw, "P4LEAK", "(r) json text in json text, in a kw string");
    JSON_DECREF(kw)

    char *deep = nested_json_text("{\"client_secret\":\"P4DEEP\",\"user\":\"bob\"}", 3);
    kw = json_pack("{s:s}", "cfg", deep? deep: "");
    json_t *jn_record = audit_record_build("update-node", kw, DATE, command_table);
    char *s = record_text(jn_record);
    BOOL ok = jn_record && !strstr(s, "P4DEEP") && strstr(s, "<redacted>") &&
        strstr(s, "bob") && !strstr(s, "not scanned");
    check(ok, "(r) three levels of json text in a kw string: redacted, the rest kept");
    if(!ok) {
        printf("     %s\n", s);
    }
    GBMEM_FREE(s);
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)
    GBMEM_FREE(deep);

    /*
     *  The record of a redacted json text is still the json text: the kw
     *  string decodes to a json whose "cfg" is a json text with the secret
     *  redacted and the rest as it was
     */
    kw = json_pack("{s:s}", "cfg", "{\"a\":\"{\\\"password\\\":\\\"P4LEAK\\\",\\\"n\\\":1}\",\"b\":2}");
    jn_record = audit_record_build("update-node", kw, DATE, command_table);
    json_t *jn_cfg = json_loads(kw_get_str(0, jn_record, "kw`cfg", "", 0), 0, NULL);
    json_t *jn_a = jn_cfg? json_loads(kw_get_str(0, jn_cfg, "a", "", 0), 0, NULL): NULL;
    check(jn_a && strcmp(kw_get_str(0, jn_a, "password", "", 0), "<redacted>") == 0 &&
        kw_get_int(0, jn_a, "n", 0, 0) == 1 && kw_get_int(0, jn_cfg, "b", 0, 0) == 2,
        "(r) the redacted json text is still the json text");
    JSON_DECREF(jn_a)
    JSON_DECREF(jn_cfg)
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    /*
     *  The example of DEBUGGING.md 5.5, as it is written there
     */
    jn_record = audit_record_build(
        "update-node topic_name=x content='{\"cfg\":\"{\\\"password\\\":\\\"hunter2\\\",\\\"n\\\":1}\"}'",
        NULL, DATE, command_table);
    check(jn_record && strcmp(kw_get_str(0, jn_record, "command", "", 0),
        "update-node topic_name=x content='{\"cfg\":\"{\\\"password\\\":\\\"<redacted>\\\",\\\"n\\\":1}\"}'") == 0,
        "(r) the example of DEBUGGING.md: the run written back with its escapes");
    JSON_DECREF(jn_record)

    /*
     *  An array element, quotes written as \u0022, a name=value escaped
     */
    kw = json_pack("{s:s}", "list", "[\"{\\\"password\\\":\\\"PARRAY\\\"}\"]");
    check_no_secret("update-node", kw, "PARRAY", "(r) json text as an element of a list in a json text");
    JSON_DECREF(kw)
    kw = json_pack("{s:s}", "cfg", "{\"a\":\"{\\" "u0022password\\" "u0022:\\" "u0022PU0022\\" "u0022}\"}");
    check_no_secret("update-node", kw, "PU0022", "(r) json text with its quotes as \\u0022");
    JSON_DECREF(kw)
    kw = json_pack("{s:s}", "cfg", "{\"cmd\":\"set-user-pwd username=bob password=\\\"PQUOTED two\\\"\"}");
    check_no_secret("command-yuno id=x", kw, "PQUOTED", "(r) name=\\\"value\\\" in a json text");
    JSON_DECREF(kw)
    check_no_secret(
        "command-yuno id=x command=\"write-attr attribute=idp_password value=PWATTR\"",
        NULL, "PWATTR", "(r) control: write-attr of a secret attribute carried by command-yuno");

    /*
     *  Too many levels: not written (never what cannot be judged)
     */
    deep = nested_json_text("{\"password\":\"PTOODEEP\"}", 12);
    kw = json_pack("{s:s}", "cfg", deep? deep: "");
    jn_record = audit_record_build("update-node", kw, DATE, command_table);
    check(jn_record && !record_holds(jn_record, "PTOODEEP") && record_holds(jn_record, "not scanned"),
        "(r) 12 levels of json text: the deepest one is not scanned and not written");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)
    GBMEM_FREE(deep);

    /*
     *  Not secrets: a json text with escapes and no secret is written as
     *  it came
     */
    const char *plain = "{\"a\":\"{\\\"path\\\":\\\"C:\\\\\\\\dir\\\",\\\"n\\\":1}\"}";
    kw = json_pack("{s:s}", "cfg", plain);
    jn_record = audit_record_build("update-node", kw, DATE, command_table);
    check(jn_record && strcmp(kw_get_str(0, jn_record, "kw`cfg", "", 0), plain) == 0,
        "(r) a json text with escapes and no secret: written as it came");
    JSON_DECREF(jn_record)
    JSON_DECREF(kw)

    /*
     *  Hard shapes: linear, no crash
     */
    struct {
        const char *head;
        const char *unit;
    } shapes[] = {
        {"run-yuno x='{\"a\":\"",           "\\\\u005c"},       // one literal of \u005c
        {"run-yuno x='",                    "\"\\\"\""},        // "\""  "\""  ...
        {"run-yuno x='",                    "\"a\\\\\":\""},    // "a\\":"  ...
        {"run-yuno x='{\"a\":\"",           "{\\\"a\\\":\\\""},  // {\"a\":\" ... one level down each
        {"run-yuno x='",                    "\\\"a\\\":"},        // \"a\": ... keys read by their shape
        {"run-yuno x='",                    "\\\"password\\\":\\\""},  // \"password\":\" ... values of a level
        {"run-yuno ",                       "x=\"a password=\""},  // the quote of a region opens a value
        {"run-yuno ",                       "\\\" token=\""},    // \" token=" ... a value cut by a run
        {"run-yuno ",                       "\\\"password\\\":{"},  // an object of a level never closed
        {"run-yuno ",                       "password='a'\\''"},   // one shell word, pieces without end
        {"run-yuno ",                       "\" \\ password='a'\"b"},  // a word cut by runs, again and again
        {"run-yuno x='",                    "{\\\"secret key\\\":\\\"a b\\\","},  // escaped keys with a blank
        {"run-yuno x='",                    "{\\\"a b\\\":"},    // escaped keys, no value
        {"run-yuno ",                       "password=\"a\\\" "},  // \" inside "...", never closed
        {"run-yuno ",                       "eyJa.eyJb.c. "},      // JWTs at the end of a sentence
        {"run-yuno ",                       "eyJa...."},           // dots
        {"run-yuno ",                       "attribute=a\\u005fb\\\\u005fc "},  // names with escapes, decoded level by level
        {"run-yuno x=\"",                  "{\\\"attr\\\\u0069bute\\\":\\\"x\\\","},  // escaped keys with escapes, in a run
        {0, 0}
    };
    for(int i=0; shapes[i].head; i++) {
        char *s_small = repeated_text(shapes[i].head, shapes[i].unit, 100*1000);
        char *s_big = repeated_text(shapes[i].head, shapes[i].unit, 1000*1000);
        double t_small = seconds_to_build(s_small, NULL);
        double t_big = seconds_to_build(s_big, NULL);
        kw = json_pack("{s:s}", "data", s_big);
        double t_kw = seconds_to_build("run-yuno", kw);
        JSON_DECREF(kw)
        char name[200];
        snprintf(name, sizeof(name), "(r) \"%s\" + \"%s\" x N: 1 MB in %.3f s (100 KB %.3f s, kw %.3f s)",
            shapes[i].head, shapes[i].unit, t_big, t_small, t_kw);
        check(t_big < 1.0 && t_kw < 1.0 && t_big <= 30*t_small + 0.05, name);
        GBMEM_FREE(s_small);
        GBMEM_FREE(s_big);
    }
}

/***************************************************************************
 *  (s) A quote before a json text inside a json text: the quoted runs
 *  were paired by alternation, so ONE quote earlier (the closing quote of
 *  a double-quoted parameter, a stray quote in a value) shifted the pairs
 *  and the escaped json was never decoded: its secret was written in
 *  clear. And an escaped json that the parser's quotes cut
 *  (x="{\"password\":...}", '{\"password\":...}'): the audit sees the
 *  secret as text, it redacts it whatever the parser does with it.
 ***************************************************************************/
PRIVATE void check_text_kept(const char *command, json_t *kw, const char *name)
{
    json_t *jn_record = audit_record_build(command, kw, DATE, command_table);
    BOOL ok = jn_record && !record_holds(jn_record, "<redacted>");
    if(ok && !kw) {
        ok = strcmp(kw_get_str(0, jn_record, "command", "", 0), command) == 0;
    }
    check(ok, name);
    if(!ok) {
        char *s = record_text(jn_record);
        printf("     %s\n", s);
        GBMEM_FREE(s);
    }
    JSON_DECREF(jn_record)
}

PRIVATE void test_quote_before_json_text(void)
{
    check_no_secret(
        "update-node topic_name=x id=\"a b\" content='{\"cfg\":\"{\\\"password\\\":\\\"LEAK2\\\"}\"}'",
        NULL, "LEAK2", "(s) a double-quoted parameter before a json text in a json text");
    check_no_secret(
        "update-node topic_name=x note=5\" content='{\"cfg\":\"{\\\"password\\\":\\\"LEAK3\\\"}\"}'",
        NULL, "LEAK3", "(s) a stray quote in a value before a json text in a json text");

    json_t *kw = json_pack("{s:s}", "cfg", "x=\"{\\\"password\\\":\\\"LEAK4\\\"}\"");
    check_no_secret("update-node", kw, "LEAK4", "(s) kw string name=\"escaped json\"");
    JSON_DECREF(kw)
    check_no_secret(
        "update-node topic_name=x content=\"{\\\"password\\\":\\\"LEAK4T\\\"}\"",
        NULL, "LEAK4T", "(s) text name=\"escaped json\" (the parser's quote ends at the first \\\")");

    kw = json_pack("{s:s}", "note", "5\" pipe {\"cfg\":\"{\\\"password\\\":\\\"LEAK5\\\"}\"}");
    check_no_secret("update-node", kw, "LEAK5", "(s) kw string: a stray quote, then a json text in a json text");
    JSON_DECREF(kw)
    kw = json_pack("{s:s}", "cfg", "{\"a\":\"b\",\"cfg\":\"{\\\"password\\\":\\\"LEAK6\\\"}\"}");
    check_no_secret("update-node", kw, "LEAK6", "(s) kw json text: two members, the second a json text");
    JSON_DECREF(kw)

    check_no_secret(
        "update-node topic_name=x content='{\\\"password\\\":\\\"LEAKSQ\\\"}'",
        NULL, "LEAKSQ", "(s) escaped json keys in a single-quoted value (no quote of its own)");
    check_no_secret(
        "update-node x=\"a\" y=\"{\\\"token\\\":\\\"LEAKXY\\\"}\"",
        NULL, "LEAKXY", "(s) two double-quoted values, the second an escaped json");
    check_no_secret(
        "update-node a='{\"c\":\"{\\\"password\\\":\\\"R1\\\"}\"}' b='{\"c\":\"{\\\"token\\\":\\\"R2LEAK\\\"}\"}'",
        NULL, "R2LEAK", "(s) a json text in a json text after another one that was redacted");
    check_no_secret(
        "update-node a='{\"c\":\"{\\\"password\\\":\\\"R1\\\"}\" \"x\" \"{\\\"token\\\":\\\"R3LEAK\\\"}\"}'",
        NULL, "R3LEAK", "(s) a redacted run, a plain string, another run: every run looked at");
    check_no_secret(
        "set-user-pwd username=bob password=\\\"LEAKBS two\\\"",
        NULL, "two", "(s) password=\\\"two words\\\": the escaped quotes hold the value");
    check_no_secret(
        "update-node x=\"a password=\"LEAKRG b\"",
        NULL, "LEAKRG", "(s) the quote that closes the parser's value opens the secret's value");
    check_no_secret(
        "update-node x=\"\\\"password\\\":\\\"LEAKLV1\\\"\" y=2",
        NULL, "LEAKLV1", "(s) an escaped json member in a parser's value, cut at its first \\\"");
    check_no_secret(
        "update-node cfg='{\"a\":\"{\\\"b\\\":\\\"{\\\\\\\"password\\\\\\\":\\\\\\\"LEAKLV2\\\\\\\"}\\\"}\"}' n=\"x\"",
        NULL, "LEAKLV2", "(s) two levels down, a double-quoted parameter after it");
    check_no_secret(
        "update-node id=\"a b\" cfg='{\"a\":\"{\\\"b\\\":\\\"{\\\\\\\"password\\\\\\\":\\\\\\\"LEAKLV3\\\\\\\"}\\\"}\"}'",
        NULL, "LEAKLV3", "(s) two levels down, a double-quoted parameter before it");
    check_no_secret(
        "update-node id=\"a b\" content='{\"cfg\":\"{\\\"password\\\":{\\\"x\\\":\\\"LEAKOBJ\\\"}}\"}'",
        NULL, "LEAKOBJ", "(s) an object as the value of an escaped secret key");
    check_no_secret(
        "update-node x=\"{\\\"password\\\":12345678}\"",
        NULL, "12345678", "(s) a number as the value of an escaped secret key");

    /*
     *  Controls: the same shapes without a secret are written as they came
     */
    check_text_kept(
        "update-node topic_name=x id=\"a b\" content='{\"cfg\":\"{\\\"theme\\\":\\\"dark\\\"}\"}'", NULL,
        "(s) control: a double-quoted parameter before a json text in a json text, no secret");
    check_text_kept(
        "update-node topic_name=x note=5\" content='{\"cfg\":\"{\\\"theme\\\":\\\"dark\\\"}\"}'", NULL,
        "(s) control: a stray quote before a json text in a json text, no secret");
    check_text_kept(
        "update-node x=\"{\\\"theme\\\":\\\"dark\\\"}\" y='{\\\"n\\\":1}'", NULL,
        "(s) control: escaped json cut by the parser's quotes, no secret");
    kw = json_pack("{s:s}", "note", "5\" pipe {\"cfg\":\"{\\\"theme\\\":\\\"dark\\\"}\"}");
    check_text_kept("update-node", kw, "(s) control: kw string with a stray quote and a json text, no secret");
    JSON_DECREF(kw)

    json_t *jn_record = audit_record_build(
        "update-node topic_name=x id=\"a b\" content='{\"cfg\":\"{\\\"password\\\":\\\"LEAK2\\\",\\\"n\\\":1}\"}'",
        NULL, DATE, command_table);
    check(jn_record && strcmp(kw_get_str(0, jn_record, "command", "", 0),
        "update-node topic_name=x id=\"a b\" content='{\"cfg\":\"{\\\"password\\\":\\\"<redacted>\\\",\\\"n\\\":1}\"}'") == 0,
        "(s) the redacted run is written back with its escapes, the rest as it came");
    JSON_DECREF(jn_record)

    /*
     *  The examples of DEBUGGING.md 5.5, as they are written there
     */
    struct {
        const char *command;
        const char *record;
    } examples[] = {
        {"update-node x=\"{\\\"password\\\":\\\"hunter2\\\"}\"",
         "update-node x=\"{\\\"password\\\":\\\"<redacted>\\\"}\""},
        {"update-node x='{\\\"password\\\":\\\"hunter2\\\"}'",
         "update-node x='{\\\"password\\\":\\\"<redacted>\\\"}'"},
        {"set-user-pwd username=bob password=\\\"two words\\\" n=1",
         "set-user-pwd username=bob password=\\\"<redacted>\\\" n=1"},
        {0, 0}
    };
    for(int i=0; examples[i].command; i++) {
        jn_record = audit_record_build(examples[i].command, NULL, DATE, command_table);
        const char *got = kw_get_str(0, jn_record, "command", "", 0);
        BOOL ok = jn_record && strcmp(got, examples[i].record) == 0;
        char name[256];
        snprintf(name, sizeof(name), "(s) the example of DEBUGGING.md: %s", examples[i].command);
        check(ok, name);
        if(!ok) {
            printf("     %s\n", got);
        }
        JSON_DECREF(jn_record)
    }
}

/***************************************************************************
 *  (u) The shapes of review 19, as they were found:
 *  - an escaped json key read by its shape was only its last word, so a
 *    key with a blank or a slash ("secret key", "password/db") was not a
 *    secret there;
 *  - a quoted secret value ended at the first quote of its kind, escaped
 *    or not, and a json string inside a '...' value at the end of that
 *    value: a quote inside the secret left the rest of it in the record;
 *  - a JWT followed by '.' (the end of a sentence) was not one.
 ***************************************************************************/
PRIVATE void check_command_written(const char *command, const char *expected, const char *name)
{
    json_t *jn_record = audit_record_build(command, NULL, DATE, command_table);
    const char *written = kw_get_str(0, jn_record, "command", "", 0);
    BOOL ok = strcmp(written, expected) == 0;
    check(ok, name);
    if(!ok) {
        printf("     written:  %s\n     expected: %s\n", written, expected);
    }
    JSON_DECREF(jn_record)
}

PRIVATE void test_review19_shapes(void)
{
    check_no_secret("x cfg='{\\\"secret key\\\":\\\"S3CRa\\\"}'", NULL, "S3CRa",
        "(u) '{\\\"secret key\\\":...}': an escaped key with a blank");
    check_no_secret("x cfg='{\\\"private key\\\":\\\"S3CRb\\\"}'", NULL, "S3CRb",
        "(u) '{\\\"private key\\\":...}': priv and key in two words");
    check_no_secret("x cfg='{\\\"password/db\\\":\\\"S3CRc\\\"}'", NULL, "S3CRc",
        "(u) '{\\\"password/db\\\":...}': an escaped key with a slash");
    check_no_secret("x cfg='{\\\"a\\\":\\\"b c\\\",\\\"secret key\\\":\\\"S3CRd\\\"}'", NULL, "S3CRd",
        "(u) an escaped key with a blank after another member");
    json_t *kw = json_pack("{s:s}", "c2", "'{\\\"private key\\\":\\\"S3CRe\\\"}'");
    check_no_secret("update-node", kw, "S3CRe", "(u) the same shape in a kw string");
    JSON_DECREF(kw)
    check_command_written("x cfg='{\\\"note\\\":\\\"a key\\\"}'",
        "x cfg='{\\\"note\\\":\\\"a key\\\"}'",
        "(u) an escaped key that is no secret, a value with a blank: kept");

    check_no_secret("set-user-pwd username=bob password=\"a\\\" S3CR1\"", NULL, "S3CR1",
        "(u) password=\"a\\\" S\": an escaped quote inside the value");
    check_command_written("set-user-pwd username=bob password=\"a\\\" S3CR1\" n=1",
        "set-user-pwd username=bob password=\"<redacted>\" n=1",
        "(u) password=\"a\\\" S\" n=1: the value up to its closing quote, n=1 kept");
    check_no_secret("set-user-pwd username=bob password='it'\\''s S3CR2' n=1", NULL, "S3CR2",
        "(u) password='it'\\''s S': the shell's quote inside a quoted value");
    check_command_written("set-user-pwd username=bob password='it'\\''s S3CR2' n=1",
        "set-user-pwd username=bob password='<redacted>' n=1",
        "(u) password='it'\\''s S' n=1: the whole shell word, n=1 kept");
    check_no_secret("x cfg='{\"password\":\"it's S3CR3\"}'", NULL, "S3CR3",
        "(u) '{\"password\":\"it's S\"}': a ' in a json string inside a '...' value");
    check_command_written("x cfg='{\"password\":\"it's S3CR3\"}' n=1",
        "x cfg='{\"password\":\"<redacted>\"}' n=1",
        "(u) '{\"password\":\"it's S\"}' n=1: the string up to its closing quote");

    check_no_secret("update-node x='{\"attribute\":\"api\\u005fkey\",\"value\":\"S3CRu1\"}'", NULL, "S3CRu1",
        "(u) write-attr json, the attribute name with a \\u escape");
    check_no_secret("update-node x='{\"attr\\u0069bute\":\"api_key\",\"value\":\"S3CRu2\"}'", NULL, "S3CRu2",
        "(u) write-attr json, the key `attribute` with a \\u escape");
    kw = json_pack("{s:s}", "x", "{\"attribute\":\"api_key\",\"value\":\"S3CRu3\"}");
    check_no_secret("update-node", kw, "S3CRu3", "(u) write-attr json in a kw string");
    JSON_DECREF(kw)
    kw = json_pack("{s:s}", "x", "{\"attribute\":\"api\\u005fkey\",\"value\":\"S3CRu4\"}");
    check_no_secret("update-node", kw, "S3CRu4", "(u) write-attr json in a kw string, the name with a \\u escape");
    JSON_DECREF(kw)
    check_no_secret("update-node x='{\"a\":\"{\\\"attribute\\\":\\\"api\\\\u005fkey\\\",\\\"value\\\":\\\"S3CRu5\\\"}\"}'",
        NULL, "S3CRu5", "(u) write-attr json one json text down, the name escaped twice");
    check_no_secret("update-node pass\\u0077ord=S3CRu6", NULL, "S3CRu6",
        "(u) a parameter key with a \\u escape");
    check_command_written("command-yuno id=x command=\"write-attr attribute=api\\u005fkey value=S3CRq1\" n=1",
        "command-yuno id=x command=\"write-attr attribute=api_key value=<redacted>\" n=1",
        "(u) a secret value at the end of a \"...\" value: its closing quote and n=1 stay");
    check_command_written("command-yuno id=x command=\"update-node x=\\\"1\\\" password=S3CRq2\"",
        "command-yuno id=x command=\"update-node x=\\\"1\\\" password=<redacted>\"",
        "(u) a secret value at the end of a \"...\" value with escapes: its closing quote stays");
    check_no_secret("x \" \\ password='a'\" S3CR4\" n2=1", NULL, "S3CR4",
        "(u) a stray quote, a backslash, then password='a'\" S\": the word goes on after the run");
    check_command_written("update-node x=\"\\\\ password='S3CR5'\" n2=1",
        "update-node x=\"\\\\ password='<redacted>'\" n2=1",
        "(u) the quote that ends a \"...\" value, a blank after it: n2=1 kept");

    check_no_secret("x msg=\"login with eyJhbGciOi.eyJzdWIi.S3CRsig.\"", NULL, "S3CRsig",
        "(u) a JWT followed by '.'");
    check_command_written("x msg=\"login with eyJhbGciOi.eyJzdWIi.S3CRsig. Bye\"",
        "x msg=\"login with <redacted>. Bye\"",
        "(u) a JWT followed by '.': its dot stays");
    check_no_secret("x eyJhbGciOi.eyJzdWIi.S3CRsig...", NULL, "S3CRsig",
        "(u) a JWT followed by '...'");
    check_command_written("x see eyJhbGciOi.eyJzdWIi. and eyJhbGciOi.",
        "x see eyJhbGciOi.eyJzdWIi. and eyJhbGciOi.",
        "(u) two parts and a dot, one part and a dot: no JWT, kept");
}

/***************************************************************************
 *  (t) Generated commands: the secret parameter in every quoting shape
 *  (plain, blanks, '', "", \"\", a quoted key, a json in a value, a json
 *  text in a json text at 0-3 levels, cut or not by the parser's quotes,
 *  carried by command-yuno), among parameters with stray and unclosed
 *  quotes, in the text and in a kw string. No secret survives; the
 *  parameter before them all is kept. Deterministic seed.
 *
 *  And the shapes of review 19: a json key with blanks or a slash
 *  ("secret key", "private key", "password/db"), escaped or not; a quote
 *  inside a quoted secret value (password="a\" S", the shell's
 *  password='it'\''s S', a json string holding a "'" inside a '...'
 *  value); and a JWT followed by '.' (the end of a sentence).
 ***************************************************************************/
PRIVATE uint32_t g_rand = 0x9E3779B9;

PRIVATE uint32_t gen_rand(void)
{
    g_rand ^= g_rand << 13;
    g_rand ^= g_rand >> 17;
    g_rand ^= g_rand << 5;
    return g_rand;
}

PRIVATE uint32_t pick(uint32_t n)
{
    return gen_rand() % n;
}

PRIVATE const char *gen_secret_keys[] = {
    "password", "token", "client_secret", "api_key", "access_token", "passw",
    "jwt", "private_key", "pwd", "secret", "x-api-key", "Password",
    "password/db", "db/token", 0
};

/*
 *  A json key can hold blanks too (a parameter of the text cannot: its
 *  key is the last word before the '=')
 */
PRIVATE const char *gen_json_keys[] = {
    "secret key", "private key", "password/db", "db password", "the api key",
    "Access Token", "my-secret/x y",
    "pass\\u0077ord", "\\u0074oken", "api\\u005fkey", "Secret\\u0020Key", "cl\\u0069ent_secret", 0
};

/*
 *  A write-attr of a secret attribute: its `value` is the secret. The
 *  names with json escapes too.
 */
PRIVATE const char *gen_attribute_keys[] = {
    "attribute", "ATTRIBUTE", "attr\\u0069bute", 0
};
PRIVATE const char *gen_attribute_names[] = {
    "api_key", "api\\u005fkey", "pass\\u0077ord", "\\u0074oken", "client_secret", 0
};
PRIVATE const char *gen_value_keys[] = {
    "value", "Value", "v\\u0061lue", 0
};

PRIVATE const char *gen_pick(const char **list)
{
    int n = 0;
    while(list[n]) {
        n++;
    }
    return list[pick((uint32_t)n)];
}

PRIVATE const char *gen_secret_key(void)
{
    int n = 0;
    while(gen_secret_keys[n]) {
        n++;
    }
    return gen_secret_keys[pick((uint32_t)n)];
}

PRIVATE const char *gen_json_key(void)
{
    if(pick(2) == 0) {
        return gen_secret_key();
    }
    int n = 0;
    while(gen_json_keys[n]) {
        n++;
    }
    return gen_json_keys[pick((uint32_t)n)];
}

/*
 *  A json text holding the secret, `levels` json texts deep. Free with
 *  gbmem_free().
 */
PRIVATE char *gen_json(const char *secret1, const char *secret2, int levels)
{
    const char *k = gen_json_key();
    char bf[512];
    switch(pick(9)) {
        case 0:
            snprintf(bf, sizeof(bf), "{\"%s\":\"%s\"}", k, secret1);
            break;
        case 1:
            snprintf(bf, sizeof(bf), "{\"n\":1,\"%s\":\"%s %s\",\"m\":\"x\"}", k, secret1, secret2);
            break;
        case 2:
            snprintf(bf, sizeof(bf), "{\"%s\":{\"inner\":\"%s\"}}", k, secret1);
            break;
        case 3:
            snprintf(bf, sizeof(bf), "{\"%s\":[\"%s\",\"%s\"]}", k, secret1, secret2);
            break;
        case 4:
            snprintf(bf, sizeof(bf), "{\"list\":[{\"%s\":\"%s\"}],\"note\":\"5\\\"\"}", k, secret1);
            break;
        case 5:
            snprintf(bf, sizeof(bf), "{\"%s\":\"it's %s %s\"}", k, secret1, secret2);   // a ' in the string
            break;
        case 6:
            snprintf(bf, sizeof(bf), "{\"%s\":\"a\\\" %s %s\"}", k, secret1, secret2); // a \" in the string
            break;
        case 7:
            snprintf(bf, sizeof(bf), "{\"%s\":\"%s\",\"n\":1,\"%s\":\"%s %s\"}",      // a write-attr
                gen_pick(gen_attribute_keys), gen_pick(gen_attribute_names),
                gen_pick(gen_value_keys), secret1, secret2);
            break;
        default:
            snprintf(bf, sizeof(bf), "{\"note\":\"a \\\" b\",\"%s\" : \"%s\"}", k, secret1);
            break;
    }
    char *s = nested_json_text(bf, levels);
    return s;
}

/*
 *  `text` as a json string literal WITHOUT its quotes
 */
PRIVATE char *json_escaped(const char *text)
{
    char *q = json_quoted(text);
    if(!q) {
        return NULL;
    }
    size_t n = strlen(q);
    if(n >= 2) {
        memmove(q, q + 1, n - 2);
        q[n - 2] = 0;
    }
    return q;
}

PRIVATE void gen_secret_param(gbuffer_t *gbuf, const char *secret1, const char *secret2, BOOL inner)
{
    const char *k = gen_secret_key();
    int form = (int)pick(inner? 21: 24);
    char *json = NULL;
    char *esc = NULL;
    switch(form) {
        case 0:
            gbuffer_printf(gbuf, "%s=%s", k, secret1);
            break;
        case 1:
            gbuffer_printf(gbuf, "%s = %s", k, secret1);
            break;
        case 2:
            gbuffer_printf(gbuf, "%s=\"%s %s\"", k, secret1, secret2);
            break;
        case 3:
            gbuffer_printf(gbuf, "%s='%s %s'", k, secret1, secret2);
            break;
        case 4:
            gbuffer_printf(gbuf, "\"%s\"=%s", k, secret1);
            break;
        case 5:
            gbuffer_printf(gbuf, "'%s'=%s", k, secret1);
            break;
        case 6:
            gbuffer_printf(gbuf, "%s=\\\"%s %s\\\"", k, secret1, secret2);
            break;
        case 7:
            json = gen_json(secret1, secret2, (int)pick(4));
            gbuffer_printf(gbuf, "cfg='%s'", json? json: "");
            break;
        case 8:
            json = gen_json(secret1, secret2, (int)pick(3));
            esc = json? json_quoted(json): NULL;
            gbuffer_printf(gbuf, "cfg=%s", esc? esc: "");     // "{\"k\":...}": the parser cuts it
            break;
        case 9:
            json = gen_json(secret1, secret2, (int)pick(3));
            esc = json? json_escaped(json): NULL;
            gbuffer_printf(gbuf, "cfg='%s'", esc? esc: "");   // '{\"k\":...}'
            break;
        case 10:
            json = gen_json(secret1, secret2, (int)pick(3));
            esc = json? json_quoted(json): NULL;
            gbuffer_printf(gbuf, "cfg='%s'", esc? esc: "");   // '"{\"k\":...}"'
            break;
        case 11:
            gbuffer_printf(gbuf, "cfg={\"%s\":\"%s\"}", gen_json_key(), secret1);
            break;
        case 12:
            gbuffer_printf(gbuf, "%s=\"a\\\" %s %s\"", k, secret1, secret2);     // k="a\" S T"
            break;
        case 13:
            gbuffer_printf(gbuf, "%s='it'\\''s %s %s'", k, secret1, secret2);   // k='it'\''s S T'
            break;
        case 14:
            gbuffer_printf(gbuf, "%s=\"a\\\"%s\\\" %s\"", k, secret1, secret2);  // k="a\"S\" T"
            break;
        case 15:
            gbuffer_printf(gbuf, "%s='a'\"b %s\"'c %s'", k, secret1, secret2);  // k='a'"b S"'c T'
            break;
        case 16:
            gbuffer_printf(gbuf, "msg=\"login with eyJhbGciOi.eyJzdWIi.%s.\"", secret1);
            break;
        case 17:
            gbuffer_printf(gbuf, "eyJhbGciOi.eyJzdWIi.%s%s", secret1, pick(2)? "..": ".");
            break;
        case 18:
            gbuffer_printf(gbuf, "cfg='{\"%s\":\"it's %s %s\"}'", gen_json_key(), secret1, secret2);
            break;
        case 19:
            json = gen_json(secret1, secret2, 1 + (int)pick(2));
            esc = json? json_escaped(json): NULL;
            gbuffer_printf(gbuf, "cfg='%s'", esc? esc: "");   // '{\"k\":...}', keys with blanks
            break;
        case 20:
            gbuffer_printf(gbuf, "attribute=%s value='%s %s'", gen_pick(gen_attribute_names), secret1, secret2);
            break;
        default:
            {
                /*
                 *  Carried by command-yuno: quoted as is, or as a json string
                 */
                gbuffer_t *g = gbuffer_create(1024, 64*1024);
                gbuffer_printf(g, "update-node ");
                gen_secret_param(g, secret1, secret2, TRUE);
                const char *inner_text = g? gbuffer_cur_rd_pointer(g): NULL;
                if(!inner_text) {
                    inner_text = "";
                }
                if(form == 21) {
                    gbuffer_printf(gbuf, "command='%s'", inner_text);
                } else if(form == 22) {
                    gbuffer_printf(gbuf, "command=\"%s\"", inner_text);
                } else {
                    esc = json_quoted(inner_text);
                    gbuffer_printf(gbuf, "command=%s", esc? esc: "");
                }
                GBUFFER_DECREF(g)
            }
            break;
    }
    GBMEM_FREE(json);
    GBMEM_FREE(esc);
}

PRIVATE void gen_noise_param(gbuffer_t *gbuf, int i)
{
    switch(pick(17)) {
        case 0:
            gbuffer_printf(gbuf, "n%d=v%d", i, i);
            break;
        case 1:
            gbuffer_printf(gbuf, "n%d=\"a b\"", i);
            break;
        case 2:
            gbuffer_printf(gbuf, "n%d='a b'", i);
            break;
        case 3:
            gbuffer_printf(gbuf, "n%d=5\"", i);
            break;
        case 4:
            gbuffer_printf(gbuf, "n%d=\"a", i);
            break;
        case 5:
            gbuffer_printf(gbuf, "\"");
            break;
        case 6:
            gbuffer_printf(gbuf, "n%d=it's", i);
            break;
        case 7:
            gbuffer_printf(gbuf, "n%d='a", i);
            break;
        case 8:
            gbuffer_printf(gbuf, "n%d={\"x\":\"y\"}", i);
            break;
        case 9:
            gbuffer_printf(gbuf, "n%d=\\\"", i);
            break;
        case 10:
            gbuffer_printf(gbuf, "n%d=\"{\\\"a\\\":\\\"b\\\"}\"", i);
            break;
        case 11:
            gbuffer_printf(gbuf, "n%d=\\\\\\\"", i);
            break;
        case 12:
            gbuffer_printf(gbuf, "n%d='{\"a\":\"\\\"'", i);
            break;
        case 13:
            gbuffer_printf(gbuf, "n%d=\"{\\\"a\\\":", i);
            break;
        case 14:
            gbuffer_printf(gbuf, "\\");
            break;
        case 15:
            gbuffer_printf(gbuf, "n%d:\"x\"", i);
            break;
        default:
            gbuffer_printf(gbuf, "'");
            break;
    }
}

PRIVATE void test_generated_commands(void)
{
    const char *verbs[] = {"update-node", "set-user-pwd", "command-yuno id=x", "create-node"};
    int n_cases = 20000;
    int leaks = 0;
    int kept = 0;
    int errors = s_errors;
    uint64_t t0 = time_in_milliseconds_monotonic();
    for(int i=0; i<n_cases; i++) {
        char secret1[32];
        char secret2[32];
        char control[32];
        snprintf(secret1, sizeof(secret1), "Zs%dQ", i);
        snprintf(secret2, sizeof(secret2), "Zt%dQ", i);
        snprintf(control, sizeof(control), "Vk%dQ", i);

        gbuffer_t *gbuf = gbuffer_create(1024, 64*1024);
        gbuffer_printf(gbuf, "%s keep=%s", verbs[pick(4)], control);
        int before = (int)pick(5);
        int after = (int)pick(4);
        for(int j=0; j<before; j++) {
            gbuffer_printf(gbuf, " ");
            gen_noise_param(gbuf, j);
        }
        gbuffer_printf(gbuf, " ");
        gen_secret_param(gbuf, secret1, secret2, FALSE);
        for(int j=0; j<after; j++) {
            gbuffer_printf(gbuf, " ");
            gen_noise_param(gbuf, before + j);
        }
        char *text = gbuffer_cur_rd_pointer(gbuf);

        /*
         *  In the command text, and as a kw string
         */
        for(int where=0; where<2; where++) {
            json_t *kw = where? json_pack("{s:s}", "data", text): NULL;
            json_t *jn_record = audit_record_build(where? "update-node": text, kw, DATE, command_table);
            char *s = record_text(jn_record);
            if(strstr(s, secret1) || strstr(s, secret2)) {
                leaks++;
                if(leaks <= 5) {
                    printf("     secret written (%s): %s\n     record: %.600s\n",
                        where? "kw": "text", text, s);
                }
            }
            if(strstr(s, control)) {
                kept++;
            }
            GBMEM_FREE(s);
            JSON_DECREF(jn_record)
            JSON_DECREF(kw)
        }
        GBUFFER_DECREF(gbuf)
    }
    uint64_t t1 = time_in_milliseconds_monotonic();
    char name[160];
    snprintf(name, sizeof(name), "(t) %d generated commands, text and kw: no secret written (%d leaks, %.2f s)",
        n_cases, leaks, (double)(t1-t0)/1000.0);
    check(leaks == 0, name);
    snprintf(name, sizeof(name), "(t) the parameter before them all is kept (%d of %d)", kept, 2*n_cases);
    check(kept == 2*n_cases, name);
    check(s_errors == errors, "(t) no error logged");
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

    /*
     *  A write with a json given as text in the kw: plain, and with a
     *  json text inside it (escaped quotes, scanned decoded too)
     */
    const char *contents[] = {
        "{\"id\":\"bob\",\"name\":\"Bob Smith\",\"email\":\"bob@example.com\",\"n\":1,\"tags\":[\"a\",\"b\"]}",
        "{\"id\":\"bob\",\"name\":\"Bob Smith\",\"cfg\":\"{\\\"theme\\\":\\\"dark\\\",\\\"lang\\\":\\\"es\\\"}\",\"n\":1}",
    };
    const char *content_names[] = {"a json text", "a json text holding a json text"};
    for(int c=0; c<2; c++) {
        kw = json_pack("{s:s, s:s, s:s, s:o}",
            "topic_name", "users",
            "content", contents[c],
            "__username__", "claudia@artgins.com",
            "__md_iev__", md_iev_via_controlcenter("update-node")
        );
        int n = 100000;
        uint64_t t0 = time_in_milliseconds_monotonic();
        for(int i=0; i<n; i++) {
            json_t *jn = audit_record_build("update-node", kw, DATE, command_table);
            char *s = json2uglystr(jn);
            GBMEM_FREE(s);
            JSON_DECREF(jn)
        }
        uint64_t t1 = time_in_milliseconds_monotonic();
        printf("     update-node with %s in the kw: %.2f us per record (build + serialize)\n",
            content_names[c], (double)(t1-t0)*1000.0/n);
        JSON_DECREF(kw)
    }
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
    test_json_text_in_json_text();
    test_quote_before_json_text();
    test_review19_shapes();
    test_generated_commands();
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
