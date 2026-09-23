/****************************************************************************
 *          audit_record.c
 *
 *          What the agent writes to its audit file for one command.
 *
 *          Up to 7.25.4 the record was the command, the date and the WHOLE
 *          kw. On wattyzer that was 0.6-1.2 GB on every deploy day: an
 *          install-binary carries the binary as base64 (43 MB for a 32 MB
 *          yuno) THREE times -- in the command text (ycommand puts
 *          `content64=...` inside it), in kw.__command__ and in the
 *          command_stack of kw.__md_iev__ -- about 130 MB per record. And
 *          the GUI's read-only polling wrote ~1 KB of routing metadata
 *          (__md_iev__) per record.
 *
 *          Now:
 *
 *          - A content64 value is never written: it is replaced by
 *            `<N bytes sha256:HEX>`, the size and the sha256 of the DECODED
 *            content (the same as `sha256sum` of the binary on disk). A
 *            value that is not base64 is replaced by
 *            `<N chars, not base64, sha256 of the text:HEX>`. This applies
 *            to `content64=` inside any string (the command text,
 *            __command__) and to any kw key named `content64`.
 *
 *          - __md_iev__ is not written. In its place, `source`:
 *              {
 *                  "console_purpose": "<if any>",
 *                  "hops": [   // one per inter-yuno hop, nearest first
 *                      {"role": src_role, "yuno": src_yuno,
 *                       "service": src_service, "user": user, "host": host}
 *                  ]
 *              }
 *            and `user` at the top: kw.__username__ (the end user), or the
 *            user of the nearest hop. The peer address of the channel is
 *            not in the kw and is not looked up.
 *            __command__ is dropped when it repeats the command text.
 *
 *          - A read-only command is recorded as {command, date, user} and
 *            nothing else. The command tables declare no read-only
 *            property (their flags are 0, SDF_WILD_CMD or SDF_AUTHZ_X,
 *            and AUTHZ_X means "needs the execute permission" for reads and
 *            writes alike), so the list is by NAME: see read_only_prefixes[]
 *            and read_only_commands[]. command-yuno / command-agent are
 *            judged by the command they carry (kw.command, or command= in
 *            the text), and that command is named in the record.
 *            Commands that read files of the node (read-file, read-json,
 *            read-binary-file), check a password (check-user-pwd) or open
 *            something (open-list, open-treedb, ...) keep the full record.
 *
 *          The kw of the caller is never modified.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <ctype.h>
#include "audit_record.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CONTENT64_KEY   "content64"
#define CONTENT64_PARAM "content64="

/*
 *  Read-only commands, by name
 */
PRIVATE const char *read_only_prefixes[] = {
    "list-",    // list-yunos, list-binaries, list-keys, list-jwk, ...
    "view-",    // view-config, view-attrs, view-cert, ...
    "get-",     // get-node, get-page, get-global-trace, ...
    "info-",    // info-cpus, info-mem, ...
    "dir-",     // dir-logs, dir-realms, ...
    0
};
PRIVATE const char *read_only_commands[] = {
    "help", "authzs", "ping", "node-uuid",
    "top", "top-services", "services",
    "stats", "stats-agent", "stats-yuno", "authzs-yuno",
    "treedbs", "treedb-info", "topics", "desc", "descs", "system-schema", "schema-file",
    "saved-schema", "diff-schema", "jtree",
    "nodes", "node", "instances", "hooks", "links", "parents", "children", "pkey2s",
    "snaps", "snap-content",
    "print-role", "print-tranger", "check-json", "check-realm",
    "cert-expiry-status", "cert-sync-status", "global-variables",
    "running-keys", "running-bin",
    "users", "accesses", "roles", "user-roles", "user-authzs",
    0
};

/*
 *  Commands that carry another command in their `command` parameter
 */
PRIVATE const char *wrapper_commands[] = {
    "command-yuno",
    "command-agent",
    0
};

/***************************************************************************
 *  The first word of a command text
 ***************************************************************************/
PRIVATE void first_word(const char *s, char *bf, size_t bfsize)
{
    while(*s && isspace((unsigned char)*s)) {
        s++;
    }
    size_t i = 0;
    while(*s && !isspace((unsigned char)*s) && i < bfsize - 1) {
        bf[i++] = *s++;
    }
    bf[i] = 0;
}

/***************************************************************************
 *  The value of `name=` in a command text (quoted or not), "" if absent
 ***************************************************************************/
PRIVATE void param_in_text(const char *text, const char *name, char *bf, size_t bfsize)
{
    size_t name_len = strlen(name);
    *bf = 0;

    const char *p = text;
    while((p = strstr(p, name)) != NULL) {
        BOOL at_token_start = (p == text || isspace((unsigned char)p[-1]));
        if(at_token_start && p[name_len] == '=') {
            const char *v = p + name_len + 1;
            char quote = 0;
            if(*v == '\'' || *v == '"') {
                quote = *v++;
            }
            size_t i = 0;
            while(*v && i < bfsize - 1) {
                if(quote? (*v == quote) : isspace((unsigned char)*v)) {
                    break;
                }
                bf[i++] = *v++;
            }
            bf[i] = 0;
            return;
        }
        p += name_len;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL name_in_list(const char *s, const char **list)
{
    for(int i=0; list[i]; i++) {
        if(strcmp(s, list[i]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

PRIVATE BOOL verb_is_read_only(const char *verb)
{
    for(int i=0; read_only_prefixes[i]; i++) {
        if(strncmp(verb, read_only_prefixes[i], strlen(read_only_prefixes[i])) == 0) {
            return TRUE;
        }
    }
    return name_in_list(verb, read_only_commands);
}

/***************************************************************************
 *  The command carried by a wrapper (command-yuno, command-agent), "" if none
 ***************************************************************************/
PRIVATE void inner_command(const char *command, json_t *kw, char *bf, size_t bfsize)
{
    *bf = 0;
    json_t *jn_inner = json_is_object(kw)? json_object_get(kw, "command"): NULL;
    if(json_is_string(jn_inner)) {
        first_word(json_string_value(jn_inner), bf, bfsize);
    } else {
        char value[NAME_MAX];
        param_in_text(command, "command", value, sizeof(value));
        first_word(value, bf, bfsize);
    }
}

/***************************************************************************
 *  TRUE if the command changes nothing (by name, see the header)
 ***************************************************************************/
PRIVATE BOOL command_is_read_only(const char *command, json_t *kw)
{
    char verb[NAME_MAX];
    first_word(command, verb, sizeof(verb));

    if(name_in_list(verb, wrapper_commands)) {
        char inner[NAME_MAX];
        inner_command(command, kw, inner, sizeof(inner));
        if(!*inner || name_in_list(inner, wrapper_commands)) {
            return FALSE;
        }
        return verb_is_read_only(inner);
    }
    return verb_is_read_only(verb);
}

/***************************************************************************
 *  `<N bytes sha256:HEX>` of a content64 value
 ***************************************************************************/
PRIVATE BOOL looks_like_base64(const char *s, size_t len)
{
    if(len == 0 || (len % 4) != 0) {
        return FALSE;
    }
    for(size_t i=0; i<len; i++) {
        char c = s[i];
        if(isalnum((unsigned char)c) || c == '+' || c == '/') {
            continue;
        }
        if(c == '=' && i >= len - 2) {
            continue;
        }
        return FALSE;
    }
    return TRUE;
}

PRIVATE void content64_summary(const char *value, size_t len, char *bf, size_t bfsize)
{
    char hex[SHA256_HEX_LEN + 1];

    if(looks_like_base64(value, len)) {
        gbuffer_t *gbuf = gbuffer_base64_to_binary(value, len);
        if(gbuf) {
            size_t n = gbuffer_leftbytes(gbuf);
            sha256_hex(gbuffer_cur_rd_pointer(gbuf), n, hex, sizeof(hex));
            snprintf(bf, bfsize, "<%zu bytes sha256:%s>", n, hex);
            GBUFFER_DECREF(gbuf)
            return;
        }
        // Error already logged: said below as not base64
    }
    sha256_hex(value, len, hex, sizeof(hex));
    snprintf(bf, bfsize, "<%zu chars, not base64, sha256 of the text:%s>", len, hex);
}

/***************************************************************************
 *  A copy of `text` with every `content64=<value>` replaced by its
 *  summary. NULL if the text has none (use the text as it is).
 *  Free the result with gbmem_free().
 ***************************************************************************/
PRIVATE char *redact_content64_in_text(const char *text)
{
    const char *p = strstr(text, CONTENT64_PARAM);
    if(!p) {
        return NULL;
    }

    size_t text_len = strlen(text);
    gbuffer_t *gbuf = gbuffer_create(256, text_len + 256);
    if(!gbuf) {
        // Error already logged. Never the text: it holds the content
        return gbmem_strdup("<not written: no memory to take content64 out of it>");
    }

    const char *from = text;
    while(p) {
        BOOL at_token_start = (p == text || isspace((unsigned char)p[-1]) || p[-1] == '"');
        const char *v = p + strlen(CONTENT64_PARAM);
        if(!at_token_start) {
            p = strstr(v, CONTENT64_PARAM);
            continue;
        }

        char quote = 0;
        if(*v == '\'' || *v == '"') {
            quote = *v++;
        }
        const char *end = v;
        while(*end) {
            if(quote? (*end == quote) : isspace((unsigned char)*end)) {
                break;
            }
            end++;
        }

        char summary[SHA256_HEX_LEN + 128];
        content64_summary(v, (size_t)(end - v), summary, sizeof(summary));

        if(p > from) {
            gbuffer_append(gbuf, (void *)from, (size_t)(p - from));
        }
        gbuffer_append(gbuf, CONTENT64_PARAM, strlen(CONTENT64_PARAM));
        if(quote) {
            gbuffer_append(gbuf, &quote, 1);
        }
        gbuffer_append(gbuf, summary, strlen(summary));
        if(quote && *end == quote) {
            gbuffer_append(gbuf, &quote, 1);
            end++;
        }
        from = end;
        p = strstr(from, CONTENT64_PARAM);
    }
    if(*from) {
        gbuffer_append(gbuf, (void *)from, strlen(from));
    }

    size_t len = gbuffer_leftbytes(gbuf);
    char *redacted = gbmem_strndup(gbuffer_cur_rd_pointer(gbuf), len);
    GBUFFER_DECREF(gbuf)
    return redacted;
}

/***************************************************************************
 *  A copy of a json value without any content64 (see the header).
 *  Values with nothing to redact are shared, not copied.
 ***************************************************************************/
PRIVATE json_t *redacted_copy(json_t *jn);

PRIVATE json_t *redacted_member(const char *key, json_t *value)
{
    if(strcmp(key, CONTENT64_KEY) == 0 && json_is_string(value)) {
        char summary[SHA256_HEX_LEN + 128];
        content64_summary(
            json_string_value(value),
            json_string_length(value),
            summary,
            sizeof(summary)
        );
        return json_string(summary);
    }
    return redacted_copy(value);
}

PRIVATE json_t *redacted_copy(json_t *jn)
{
    if(json_is_object(jn)) {
        json_t *jn_copy = json_object();
        const char *key;
        json_t *value;
        json_object_foreach(jn, key, value) {
            json_object_set_new(jn_copy, key, redacted_member(key, value));
        }
        return jn_copy;
    }

    if(json_is_array(jn)) {
        json_t *jn_copy = json_array();
        size_t idx;
        json_t *value;
        json_array_foreach(jn, idx, value) {
            json_array_append_new(jn_copy, redacted_copy(value));
        }
        return jn_copy;
    }

    if(json_is_string(jn)) {
        char *redacted = redact_content64_in_text(json_string_value(jn));
        if(redacted) {
            json_t *jn_redacted = json_string(redacted);
            gbmem_free(redacted);
            return jn_redacted;
        }
    }

    return json_incref(jn);
}

/***************************************************************************
 *  The source summary of __md_iev__, NULL if the kw has none
 ***************************************************************************/
PRIVATE json_t *source_summary(json_t *kw)
{
    json_t *jn_md = json_is_object(kw)? json_object_get(kw, "__md_iev__"): NULL;
    if(!json_is_object(jn_md)) {
        return NULL;
    }

    json_t *jn_source = json_object();
    json_t *jn_purpose = json_object_get(jn_md, "console_purpose");
    if(json_is_string(jn_purpose) && json_string_length(jn_purpose) > 0) {
        json_object_set(jn_source, "console_purpose", jn_purpose);
    }

    json_t *jn_stack = json_object_get(jn_md, "ievent_gate_stack");
    json_t *jn_hops = json_array();
    size_t idx;
    json_t *jn_hop;
    json_array_foreach(jn_stack, idx, jn_hop) {
        if(!json_is_object(jn_hop)) {
            continue;
        }
        const char *user = kw_get_str(0, jn_hop, "user", "", 0);
        if(empty_string(user)) {
            user = kw_get_str(0, jn_hop, "__username__", "", 0);
        }
        json_array_append_new(jn_hops, json_pack("{s:s, s:s, s:s, s:s, s:s}",
            "role",     kw_get_str(0, jn_hop, "src_role", "", 0),
            "yuno",     kw_get_str(0, jn_hop, "src_yuno", "", 0),
            "service",  kw_get_str(0, jn_hop, "src_service", "", 0),
            "user",     user,
            "host",     kw_get_str(0, jn_hop, "host", "", 0)
        ));
    }
    if(json_array_size(jn_hops) > 0) {
        json_object_set_new(jn_source, "hops", jn_hops);
    } else {
        JSON_DECREF(jn_hops)
    }
    return jn_source;
}

/***************************************************************************
 *  The user of the command: the end user if known
 ***************************************************************************/
PRIVATE const char *command_user(json_t *kw)
{
    if(!json_is_object(kw)) {
        return "";
    }
    json_t *jn_user = json_object_get(kw, "__username__");
    if(json_is_string(jn_user) && json_string_length(jn_user) > 0) {
        return json_string_value(jn_user);
    }
    json_t *jn_md = json_object_get(kw, "__md_iev__");
    json_t *jn_stack = json_is_object(jn_md)? json_object_get(jn_md, "ievent_gate_stack"): NULL;
    json_t *jn_hop = json_array_get(jn_stack, 0);
    if(json_is_object(jn_hop)) {
        return kw_get_str(0, jn_hop, "user", "", 0);
    }
    return "";
}

/***************************************************************************
 *  Build the audit record of a command
 ***************************************************************************/
PUBLIC json_t *audit_record_build(
    const char *command,
    json_t *kw,         // not owned
    const char *date
)
{
    if(!command) {
        command = "";
    }
    if(!date) {
        date = "";
    }

    char *command_redacted = redact_content64_in_text(command);
    const char *command_text = command_redacted? command_redacted: command;
    json_t *jn_record = NULL;

    if(command_is_read_only(command, kw)) {
        /*
         *  command, date, user: nothing else. A wrapper names the command
         *  it carries when the text does not.
         */
        char verb[NAME_MAX];
        first_word(command, verb, sizeof(verb));
        char command_named[PATH_MAX];
        snprintf(command_named, sizeof(command_named), "%s", command_text);
        if(name_in_list(verb, wrapper_commands) && !strstr(command_text, "command=")) {
            char inner[NAME_MAX];
            inner_command(command, kw, inner, sizeof(inner));
            snprintf(command_named, sizeof(command_named), "%s command=%s", command_text, inner);
        }
        jn_record = json_pack("{s:s, s:s, s:s}",
            "command", command_named,
            "date", date,
            "user", command_user(kw)
        );

    } else {
        json_t *jn_kw = json_object();
        if(json_is_object(kw)) {
            const char *key;
            json_t *value;
            json_object_foreach(kw, key, value) {
                if(strcmp(key, "__md_iev__") == 0) {
                    continue;   // See source_summary()
                }
                if(strcmp(key, "__command__") == 0 && json_is_string(value) &&
                        strcmp(json_string_value(value), command) == 0) {
                    continue;   // The command text again
                }
                json_object_set_new(jn_kw, key, redacted_member(key, value));
            }
        }

        jn_record = json_pack("{s:s, s:s, s:s}",
            "command", command_text,
            "date", date,
            "user", command_user(kw)
        );
        if(jn_record) {
            json_t *jn_source = source_summary(kw);
            if(jn_source) {
                json_object_set_new(jn_record, "source", jn_source);
            }
            json_object_set_new(jn_record, "kw", jn_kw);
        } else {
            JSON_DECREF(jn_kw)
        }
    }

    if(command_redacted) {
        gbmem_free(command_redacted);
    }
    if(!jn_record) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Cannot build the audit record",
            "command",      "%.64s", command,
            NULL
        );
    }
    return jn_record;
}
