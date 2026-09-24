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
 *          (__md_iev__) per record. And a password (check-user-pwd,
 *          set-user-pwd) was written in clear text.
 *
 *          Now:
 *
 *          - A content64 value is never written: it is replaced by
 *            `<N bytes sha256:HEX>`, the size and the sha256 of the DECODED
 *            content (the same as `sha256sum` of the binary on disk). A
 *            value that is not base64 is replaced by
 *            `<N chars, not base64, sha256 of the text:HEX>`. In the record
 *            of a console write (write-tty, also carried by command-agent)
 *            it is `<N bytes>` only: a hash of one keystroke can be read
 *            back with a table of 256 entries.
 *
 *          - A secret is never written: its value is replaced by
 *            `<redacted>`. A secret is a parameter whose name holds (any
 *            case) "passw", "pwd", "secret", "token", "jwt", or "priv" and
 *            "key": password, user_passw, client_secret, access_token,
 *            private_key, ... And the `value` of a write-attr whose
 *            `attribute` has such a name.
 *
 *          - Both apply everywhere in the record: to a kw key of that name
 *            at any depth, and inside any string (the command text,
 *            __command__, the `command` carried by command-yuno, a json
 *            given as text) to `name=value` (quoted or not, blanks around
 *            the `=` as the command parser allows) and to `"name": value`.
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
 *            not in the kw and is not looked up. A field of a hop that is
 *            not a string is written as "" and logged as a WARNING (it is
 *            data of a peer).
 *            __command__ is dropped when it repeats the command text.
 *
 *          - A read-only command is recorded as {command, date, user} and
 *            nothing else. The command tables declare no read-only
 *            property (their flags are 0, SDF_WILD_CMD or SDF_AUTHZ_X,
 *            and AUTHZ_X means "needs the execute permission" for reads and
 *            writes alike), so the list is by NAME: see read_only_prefixes[]
 *            and read_only_commands[]. A command with a `__reset__` value
 *            (stats=__reset__ resets the counters) is a write.
 *            command-yuno / command-agent are judged by the command they
 *            carry, taken where the command parser takes it: the last
 *            top-level `command=` of the text, else kw.command. That
 *            command is named in the record.
 *            Commands that read files of the node (read-file, read-json,
 *            read-binary-file), check a password (check-user-pwd) or open
 *            something (open-list, open-treedb, ...) keep the full record.
 *
 *          - A console write (write-tty: one command for each keystroke of
 *            ycommand, ycli and gui_agent) keeps only the FACT: who, when,
 *            which console, how many writes and bytes. Nothing of what is
 *            typed. See audit_tty_command().
 *
 *          The kw of the caller is never modified.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "audit_record.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CONTENT64_KEY   "content64"
#define REDACTED        "<redacted>"
#define RESET_VALUE     "__reset__"
#define TTY_WRITE       "write-tty"
#define CLOSE_CONSOLE   "close-console"

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
    "stats", "stats-agent", "stats-yuno", "authzs-yuno",     // a __reset__ makes them writes
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

/*
 *  A parameter whose name holds one of these (any case) is a secret
 */
PRIVATE const char *secret_name_parts[] = {
    "passw",    // password, passwd, user_passw
    "pwd",
    "secret",   // secret, client_secret, kc_admin_client_secret
    "token",    // token, access_token, refresh_token
    "jwt",
    0
};

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef enum {
    KEY_PLAIN = 0,
    KEY_CONTENT64,
    KEY_SECRET,
} key_kind_t;

typedef struct {
    BOOL tty;               // content64 of a console write: its size only
    BOOL value_is_secret;   // write-attr of a secret attribute: `value` is a secret
} redact_ctx_t;

/*
 *  The copy of a text being redacted. Made at the first change only.
 */
typedef struct {
    const char *text;
    const char *from;       // next byte of `text` still to copy
    char *out;
    size_t out_len;
    size_t out_size;
    BOOL no_memory;
    const redact_ctx_t *ctx;
} redact_scan_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE char *redact_text(const char *text, size_t len, const redact_ctx_t *ctx);
PRIVATE json_t *redacted_copy(json_t *jn, const redact_ctx_t *ctx);

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
 *  The value of the top-level parameter `name` of a command text, taken as
 *  the command parser takes it (get_parameter() for the command, then
 *  get_key_value_parameter(); the last one wins). NULL if absent.
 *  Free the result with gbmem_free().
 ***************************************************************************/
PRIVATE char *text_param(const char *text, const char *name)
{
    if(!strchr(text, '=')) {
        return NULL;
    }
    char *copy = gbmem_strdup(text);
    if(!copy) {
        return NULL;    // Error already logged
    }
    char *p = copy;
    get_parameter(p, &p);   // the command

    const char *found = NULL;
    char *key;
    char *value;
    while((value = get_key_value_parameter(p, &key, &p))) {
        if(!key) {
            break;
        }
        if(strcmp(key, name) == 0) {
            found = value;
        }
    }
    char *ret = found? gbmem_strdup(found): NULL;
    gbmem_free(copy);
    return ret;
}

/***************************************************************************
 *  The string value of `name`, as the command parser takes it: the text
 *  first, else the kw. NULL if none. Free with gbmem_free().
 ***************************************************************************/
PRIVATE char *param_value(const char *command, json_t *kw, const char *name)
{
    char *value = text_param(command, name);
    if(value) {
        return value;
    }
    json_t *jn_value = json_is_object(kw)? json_object_get(kw, name): NULL;
    if(json_is_string(jn_value)) {
        return gbmem_strdup(json_string_value(jn_value));
    }
    return NULL;
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
 *  The command carried by a wrapper (command-yuno, command-agent), "" if
 *  none. Where the parser takes it: the text wins over the kw.
 ***************************************************************************/
PRIVATE void inner_command(
    const char *command,
    json_t *kw,
    char *bf,
    size_t bfsize,
    BOOL *in_text
)
{
    *bf = 0;
    *in_text = FALSE;
    char *value = text_param(command, "command");
    if(value) {
        *in_text = TRUE;
        first_word(value, bf, bfsize);
        gbmem_free(value);
        return;
    }
    json_t *jn_inner = json_is_object(kw)? json_object_get(kw, "command"): NULL;
    if(json_is_string(jn_inner)) {
        first_word(json_string_value(jn_inner), bf, bfsize);
    }
}

/***************************************************************************
 *  TRUE if a `__reset__` value is in the text or in a top-level string of
 *  the kw: `stats=__reset__` resets counters, it is a write
 ***************************************************************************/
PRIVATE BOOL has_reset(const char *command, json_t *kw)
{
    if(strstr(command, RESET_VALUE)) {
        return TRUE;
    }
    if(!json_is_object(kw)) {
        return FALSE;
    }
    const char *key;
    json_t *value;
    json_object_foreach(kw, key, value) {
        if(json_is_string(value) && strstr(json_string_value(value), RESET_VALUE)) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  TRUE if `name` (of `len` bytes) is the name of a secret parameter
 ***************************************************************************/
PRIVATE BOOL is_secret_name(const char *name, size_t len)
{
    char lower[NAME_MAX];
    if(len == 0) {
        return FALSE;
    }
    if(len >= sizeof(lower)) {
        len = sizeof(lower) - 1;
    }
    for(size_t i=0; i<len; i++) {
        lower[i] = (char)tolower((unsigned char)name[i]);
    }
    lower[len] = 0;

    for(int i=0; secret_name_parts[i]; i++) {
        if(strstr(lower, secret_name_parts[i])) {
            return TRUE;
        }
    }
    return (strstr(lower, "priv") && strstr(lower, "key"))? TRUE: FALSE;
}

PRIVATE key_kind_t key_kind(const char *key, size_t len, const redact_ctx_t *ctx)
{
    if(len == strlen(CONTENT64_KEY) && strncasecmp(key, CONTENT64_KEY, len) == 0) {
        return KEY_CONTENT64;
    }
    if(is_secret_name(key, len)) {
        return KEY_SECRET;
    }
    if(ctx->value_is_secret && len == 5 && strncmp(key, "value", 5) == 0) {
        return KEY_SECRET;
    }
    return KEY_PLAIN;
}

/***************************************************************************
 *  TRUE if a write-attr names a secret attribute (its `value` is then a
 *  secret), in the text, anywhere, or in kw.attribute
 ***************************************************************************/
PRIVATE BOOL names_secret_attribute(const char *text, json_t *kw)
{
    json_t *jn_attribute = json_is_object(kw)? json_object_get(kw, "attribute"): NULL;
    if(json_is_string(jn_attribute) &&
            is_secret_name(json_string_value(jn_attribute), json_string_length(jn_attribute))) {
        return TRUE;
    }

    const char *p = text;
    while((p = strstr(p, "attribute")) != NULL) {
        p += strlen("attribute");
        const char *v = p;
        while(*v == ' ' || *v == '\t' || *v == '"') {
            v++;
        }
        if(*v != '=' && *v != ':') {
            continue;
        }
        v++;
        while(*v == ' ' || *v == '\t' || *v == '\'' || *v == '"') {
            v++;
        }
        size_t n = strcspn(v, " \t'\",}");
        if(is_secret_name(v, n)) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  TRUE if the command is read-only (by name, see the header)
 ***************************************************************************/
PRIVATE BOOL command_is_read_only(
    const char *command,
    json_t *kw,
    const char *verb,
    BOOL wrapper,
    const char *inner
)
{
    if(has_reset(command, kw)) {
        return FALSE;
    }
    if(wrapper) {
        if(!*inner || name_in_list(inner, wrapper_commands)) {
            return FALSE;
        }
        return verb_is_read_only(inner);
    }
    return verb_is_read_only(verb);
}

/***************************************************************************
 *  Decode a base64 value, strictly and without a log: NULL if it is not
 *  base64 (the value comes from a peer; the command handler says its own
 *  error). Blanks are skipped, as the decoder of the handlers does.
 *  Free the result with gbmem_free().
 ***************************************************************************/
PRIVATE int b64_value(char c)
{
    if(c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if(c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if(c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if(c == '+') {
        return 62;
    }
    if(c == '/') {
        return 63;
    }
    return -1;
}

/*
 *  The decoded size of a base64 value, -1 if it is not base64
 */
PRIVATE ssize_t base64_decoded_size(const char *s, size_t len)
{
    size_t count = 0;
    size_t pads = 0;
    int last = 0;   // the value of the last character before the padding
    for(size_t i=0; i<len; i++) {
        char c = s[i];
        if(isspace((unsigned char)c)) {
            continue;
        }
        if(c == '=') {
            pads++;
            if(pads > 2) {
                return -1;
            }
        } else {
            last = b64_value(c);
            if(pads > 0 || last < 0) {
                return -1;  // a character after the padding, or not of base64
            }
        }
        count++;
    }
    if(count == 0 || (count % 4) != 0) {
        return -1;
    }
    if((pads == 2 && (last & 0x0F)) || (pads == 1 && (last & 0x03))) {
        return -1;  // bits after the last byte: refused by the decoder of the handlers too
    }
    return (ssize_t)((count / 4) * 3 - pads);
}

PRIVATE uint8_t *base64_decode(const char *s, size_t len, size_t *out_len)
{
    ssize_t size = base64_decoded_size(s, len);
    if(size < 0) {
        return NULL;
    }
    uint8_t *out = gbmem_malloc((size_t)size + 1);
    if(!out) {
        return NULL;    // Error already logged
    }
    size_t n = 0;
    uint32_t acc = 0;
    int bits = 0;
    for(size_t i=0; i<len; i++) {
        int v = b64_value(s[i]);
        if(v < 0) {
            continue;   // a blank or the padding
        }
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if(bits >= 8) {
            bits -= 8;
            if(n < (size_t)size) {
                out[n++] = (uint8_t)(acc >> bits);
            }
        }
    }
    *out_len = n;
    return out;
}

/***************************************************************************
 *  What a content64 value becomes in the record
 ***************************************************************************/
PRIVATE void content64_summary(
    const char *value,
    size_t len,
    BOOL tty,
    char *bf,
    size_t bfsize
)
{
    char hex[SHA256_HEX_LEN + 1];

    if(tty) {
        ssize_t size = base64_decoded_size(value, len);
        if(size >= 0) {
            snprintf(bf, bfsize, "<%zd bytes>", size);
        } else {
            snprintf(bf, bfsize, "<%zu chars, not base64>", len);
        }
        return;
    }

    size_t n = 0;
    uint8_t *binary = base64_decode(value, len, &n);
    if(binary) {
        sha256_hex(binary, n, hex, sizeof(hex));
        snprintf(bf, bfsize, "<%zu bytes sha256:%s>", n, hex);
        gbmem_free(binary);
        return;
    }
    sha256_hex(value, len, hex, sizeof(hex));
    snprintf(bf, bfsize, "<%zu chars, not base64, sha256 of the text:%s>", len, hex);
}

/***************************************************************************
 *  The copy of a text: append
 ***************************************************************************/
PRIVATE void out_append(redact_scan_t *sc, const char *data, size_t len)
{
    if(sc->no_memory || len == 0) {
        return;
    }
    if(sc->out_len + len + 1 > sc->out_size) {
        size_t size = sc->out_size? sc->out_size: 256;
        while(size < sc->out_len + len + 1) {
            size *= 2;
        }
        char *p = gbmem_realloc(sc->out, size);
        if(!p) {
            sc->no_memory = TRUE;   // Error already logged
            return;
        }
        sc->out = p;
        sc->out_size = size;
    }
    memcpy(sc->out + sc->out_len, data, len);
    sc->out_len += len;
    sc->out[sc->out_len] = 0;
}

/*
 *  Replace the bytes [begin, end) of the text by `replacement`
 */
PRIVATE void out_replace(
    redact_scan_t *sc,
    const char *begin,
    const char *end,
    const char *replacement
)
{
    out_append(sc, sc->from, (size_t)(begin - sc->from));
    out_append(sc, replacement, strlen(replacement));
    sc->from = end;
}

/*
 *  The replacement of a value of this kind
 */
PRIVATE void replace_value(
    redact_scan_t *sc,
    key_kind_t kind,
    const char *begin,
    const char *end
)
{
    if(kind == KEY_CONTENT64) {
        char summary[SHA256_HEX_LEN + 128];
        content64_summary(begin, (size_t)(end - begin), sc->ctx->tty, summary, sizeof(summary));
        out_replace(sc, begin, end, summary);
    } else {
        out_replace(sc, begin, end, REDACTED);
    }
}

/***************************************************************************
 *  `key=value` at `eq`, the parser's way: the key is the last word before
 *  the '=' (blanks allowed around it); the value is quoted ('' or "",
 *  up to the same quote, or to the end if it never closes) or runs to a
 *  blank. Return where the scan goes on.
 ***************************************************************************/
PRIVATE void scan_range(redact_scan_t *sc, const char *begin, const char *end);

PRIVATE const char *scan_param(
    redact_scan_t *sc,
    const char *begin,
    const char *end,
    const char *eq
)
{
    const char *k_end = eq;
    while(k_end > begin && (k_end[-1] == ' ' || k_end[-1] == '\t')) {
        k_end--;
    }
    const char *k = k_end;
    while(k > begin && !strchr(" \t'\"=", k[-1])) {
        k--;
    }
    key_kind_t kind = key_kind(k, (size_t)(k_end - k), sc->ctx);

    const char *v = eq + 1;
    if(kind != KEY_PLAIN) {
        while(v < end && (*v == ' ' || *v == '\t')) {
            v++;    // the value goes to the next word, better said than written
        }
    }

    const char *v_begin = v;
    const char *v_end = end;
    const char *next = end;
    if(v < end && (*v == '\'' || *v == '"')) {
        v_begin = v + 1;
        const char *q = memchr(v_begin, *v, (size_t)(end - v_begin));
        if(q) {
            v_end = q;
            next = q + 1;
        }
    } else {
        const char *b = v;
        while(b < end && *b != ' ' && *b != '\t') {
            b++;
        }
        v_end = b;
        next = b;
    }

    if(kind == KEY_PLAIN) {
        scan_range(sc, v_begin, v_end);     // a command or a json inside the value
    } else {
        replace_value(sc, kind, v_begin, v_end);
    }
    return next;
}

/***************************************************************************
 *  `"key": value` at `colon` (a json inside a string). The key is the
 *  string before the ':'. The value is a string (with its escapes), an
 *  object or a list (up to the bracket that closes it), or a scalar.
 *  Return where the scan goes on.
 ***************************************************************************/
PRIVATE const char *scan_json_member(
    redact_scan_t *sc,
    const char *begin,
    const char *end,
    const char *colon
)
{
    const char *k_end = colon;
    while(k_end > begin && isspace((unsigned char)k_end[-1])) {
        k_end--;
    }
    if(k_end <= begin || k_end[-1] != '"') {
        return colon + 1;
    }
    k_end--;
    const char *k = k_end;
    while(k > begin && k[-1] != '"') {
        k--;
    }
    if(k <= begin) {
        return colon + 1;
    }
    key_kind_t kind = key_kind(k, (size_t)(k_end - k), sc->ctx);
    if(kind == KEY_PLAIN) {
        return colon + 1;
    }

    const char *v = colon + 1;
    while(v < end && isspace((unsigned char)*v)) {
        v++;
    }
    if(v >= end) {
        return end;
    }

    if(*v == '"') {
        const char *e = v + 1;
        while(e < end && *e != '"') {
            if(*e == '\\' && e + 1 < end) {
                e++;
            }
            e++;
        }
        replace_value(sc, kind, v + 1, e);  // the quotes stay
        return (e < end)? e + 1: end;
    }

    const char *e = v;
    if(*v == '{' || *v == '[') {
        int depth = 0;
        BOOL in_string = FALSE;
        for(; e < end; e++) {
            if(in_string) {
                if(*e == '\\' && e + 1 < end) {
                    e++;
                } else if(*e == '"') {
                    in_string = FALSE;
                }
                continue;
            }
            if(*e == '"') {
                in_string = TRUE;
            } else if(*e == '{' || *e == '[') {
                depth++;
            } else if(*e == '}' || *e == ']') {
                depth--;
                if(depth == 0) {
                    e++;
                    break;
                }
            }
        }
    } else {
        while(e < end && !strchr(",}] \t\r\n", *e)) {
            e++;
        }
    }
    out_replace(sc, v, e, "\"" REDACTED "\"");
    return e;
}

/***************************************************************************
 *  Scan the bytes [begin, end) of the text
 ***************************************************************************/
PRIVATE void scan_range(redact_scan_t *sc, const char *begin, const char *end)
{
    const char *p = begin;
    while(p < end) {
        if(*p == '=') {
            p = scan_param(sc, begin, end, p);
        } else if(*p == ':') {
            p = scan_json_member(sc, begin, end, p);
        } else {
            p++;
        }
    }
}

/***************************************************************************
 *  A copy of `text` with every content64 and secret replaced (see the
 *  header). NULL if there is nothing to replace (use the text as it is).
 *  Free the result with gbmem_free().
 ***************************************************************************/
PRIVATE char *redact_text(const char *text, size_t len, const redact_ctx_t *ctx)
{
    if(!memchr(text, '=', len) && !memchr(text, ':', len)) {
        return NULL;
    }

    redact_scan_t sc = {
        .text = text,
        .from = text,
        .ctx = ctx
    };
    scan_range(&sc, text, text + len);

    if(sc.from == text) {
        return NULL;    // Nothing replaced
    }
    out_append(&sc, sc.from, (size_t)(text + len - sc.from));
    if(sc.no_memory) {
        gbmem_free(sc.out);
        // Error already logged. Never the text: it holds what is redacted
        return gbmem_strdup("<not written: no memory to redact it>");
    }
    return sc.out;
}

/***************************************************************************
 *  A copy of a json value with every content64 and secret replaced
 *  (see the header). Values with nothing to redact are shared, not copied.
 ***************************************************************************/
PRIVATE json_t *redacted_member(const char *key, json_t *value, const redact_ctx_t *ctx)
{
    key_kind_t kind = key_kind(key, strlen(key), ctx);
    if(kind == KEY_SECRET) {
        return json_string(REDACTED);
    }
    if(kind == KEY_CONTENT64 && json_is_string(value)) {
        char summary[SHA256_HEX_LEN + 128];
        content64_summary(
            json_string_value(value),
            json_string_length(value),
            ctx->tty,
            summary,
            sizeof(summary)
        );
        return json_string(summary);
    }
    return redacted_copy(value, ctx);
}

PRIVATE json_t *redacted_copy(json_t *jn, const redact_ctx_t *ctx)
{
    if(json_is_object(jn)) {
        json_t *jn_copy = json_object();
        const char *key;
        json_t *value;
        json_object_foreach(jn, key, value) {
            json_object_set_new(jn_copy, key, redacted_member(key, value, ctx));
        }
        return jn_copy;
    }

    if(json_is_array(jn)) {
        json_t *jn_copy = json_array();
        size_t idx;
        json_t *value;
        json_array_foreach(jn, idx, value) {
            json_array_append_new(jn_copy, redacted_copy(value, ctx));
        }
        return jn_copy;
    }

    if(json_is_string(jn)) {
        char *redacted = redact_text(json_string_value(jn), json_string_length(jn), ctx);
        if(redacted) {
            json_t *jn_redacted = json_string(redacted);
            gbmem_free(redacted);
            return jn_redacted;
        }
    }

    return json_incref(jn);
}

/***************************************************************************
 *  A string field of a hop of __md_iev__ (data of a peer): "" if absent,
 *  and "" + the name in `bad` if it is not a string
 ***************************************************************************/
PRIVATE const char *hop_str(json_t *jn_hop, const char *key, const char **bad)
{
    json_t *jn = json_object_get(jn_hop, key);
    if(json_is_string(jn)) {
        return json_string_value(jn);
    }
    if(jn && !*bad) {
        *bad = key;
    }
    return "";
}

PRIVATE void warn_bad_md_iev(const char *what, const char *field, json_t *jn)
{
    char *text = jn? json2uglystr(jn): NULL;
    gobj_log_warning(0, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PROTOCOL,
        "msg",          "%s", "Audit: bad __md_iev__ from a peer, written as empty",
        "what",         "%s", what,
        "field",        "%s", field?field:"",
        "value",        "%.256s", text?text:"",
        NULL
    );
    GBMEM_FREE(text);
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
    if(jn_stack && !json_is_array(jn_stack)) {
        warn_bad_md_iev("ievent_gate_stack is not a list", "ievent_gate_stack", jn_stack);
    }
    json_t *jn_hops = json_array();
    size_t idx;
    json_t *jn_hop;
    json_array_foreach(jn_stack, idx, jn_hop) {
        if(!json_is_object(jn_hop)) {
            warn_bad_md_iev("a hop is not an object", "", jn_hop);
            continue;
        }
        const char *bad = NULL;
        const char *user = hop_str(jn_hop, "user", &bad);
        if(empty_string(user)) {
            user = hop_str(jn_hop, "__username__", &bad);
        }
        json_array_append_new(jn_hops, json_pack("{s:s, s:s, s:s, s:s, s:s}",
            "role",     hop_str(jn_hop, "src_role", &bad),
            "yuno",     hop_str(jn_hop, "src_yuno", &bad),
            "service",  hop_str(jn_hop, "src_service", &bad),
            "user",     user,
            "host",     hop_str(jn_hop, "host", &bad)
        ));
        if(bad) {
            warn_bad_md_iev("a field of a hop is not a string", bad, json_object_get(jn_hop, bad));
        }
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
        const char *bad = NULL;
        return hop_str(jn_hop, "user", &bad);   // a bad one is logged by source_summary()
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

    char verb[NAME_MAX];
    first_word(command, verb, sizeof(verb));
    char inner[NAME_MAX] = "";
    BOOL inner_in_text = FALSE;
    BOOL wrapper = name_in_list(verb, wrapper_commands);
    if(wrapper) {
        inner_command(command, kw, inner, sizeof(inner), &inner_in_text);
    }

    redact_ctx_t ctx = {
        .tty = (strcmp(verb, TTY_WRITE) == 0 || strcmp(inner, TTY_WRITE) == 0)? TRUE: FALSE,
        .value_is_secret = names_secret_attribute(command, kw)
    };
    if(!ctx.value_is_secret && wrapper && json_is_object(kw)) {
        json_t *jn_inner = json_object_get(kw, "command");
        if(json_is_string(jn_inner)) {
            ctx.value_is_secret = names_secret_attribute(json_string_value(jn_inner), NULL);
        }
    }

    char *command_redacted = redact_text(command, strlen(command), &ctx);
    const char *command_text = command_redacted? command_redacted: command;
    json_t *jn_record = NULL;

    if(command_is_read_only(command, kw, verb, wrapper, inner)) {
        /*
         *  command, date, user: nothing else. A wrapper names the command
         *  it carries when the text does not.
         */
        json_t *jn_command = (wrapper && !inner_in_text)?
            json_sprintf("%s command=%s", command_text, inner):
            json_string(command_text);
        jn_record = json_pack("{s:o, s:s, s:s}",
            "command", jn_command,
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
                json_object_set_new(jn_kw, key, redacted_member(key, value, &ctx));
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
            "command",      "%.64s", verb,
            NULL
        );
    }
    return jn_record;
}

/***************************************************************************
 *  The record of a run of writes of one burst
 ***************************************************************************/
PRIVATE json_t *tty_record(
    const char *date,
    const char *until,
    const char *user,
    const char *console,
    json_int_t writes,
    json_int_t bytes,
    json_t *jn_source    // not owned
)
{
    json_t *jn_record = json_pack("{s:s, s:s, s:s, s:s, s:I, s:I}",
        "command",  TTY_WRITE,
        "date",     date,
        "user",     user,
        "console",  console,
        "writes",   writes,
        "bytes",    bytes
    );
    if(!jn_record) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Cannot build the audit record of a console write",
            "console",      "%.64s", console,
            NULL
        );
        return NULL;
    }
    if(until) {
        json_object_set_new(jn_record, "until", json_string(until));
    }
    if(json_is_object(jn_source)) {
        json_object_set(jn_record, "source", jn_source);
    }
    return jn_record;
}

/*
 *  End a burst: the record of the writes after its first one, if any
 */
PRIVATE void tty_close_burst(json_t *jn_burst, const char *console, json_t *jn_records)
{
    json_int_t writes = json_integer_value(json_object_get(jn_burst, "writes"));
    if(writes <= 0) {
        return;
    }
    json_t *jn_record = tty_record(
        json_string_value(json_object_get(jn_burst, "date")),
        json_string_value(json_object_get(jn_burst, "until")),
        json_string_value(json_object_get(jn_burst, "user")),
        console,
        writes,
        json_integer_value(json_object_get(jn_burst, "bytes")),
        json_object_get(jn_burst, "source")
    );
    if(jn_record) {
        json_array_append_new(jn_records, jn_record);
    }
}

/***************************************************************************
 *  End the bursts: all, or those whose interval has passed
 ***************************************************************************/
PRIVATE void tty_close_bursts(json_t *jn_bursts, BOOL all, json_t *jn_records)
{
    const char *console;
    json_t *jn_burst;
    void *tmp;
    json_object_foreach_safe(jn_bursts, tmp, console, jn_burst) {
        uint64_t deadline = (uint64_t)json_integer_value(json_object_get(jn_burst, "deadline"));
        if(all || test_msectimer(deadline)) {
            tty_close_burst(jn_burst, console, jn_records);
            json_object_del(jn_bursts, console);
        }
    }
}

PUBLIC void audit_tty_close_bursts(json_t *jn_bursts, json_t *jn_records)
{
    if(!json_is_object(jn_bursts) || !json_is_array(jn_records)) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "jn_bursts must be a dict and jn_records a list",
            NULL
        );
        return;
    }
    tty_close_bursts(jn_bursts, TRUE, jn_records);
}

/***************************************************************************
 *  Console writes, see audit_record.h
 ***************************************************************************/
PUBLIC BOOL audit_tty_command(
    json_t *jn_bursts,
    const char *command,
    json_t *kw,
    const char *date,
    unsigned burst_seconds,
    json_t *jn_records
)
{
    if(!json_is_object(jn_bursts) || !json_is_array(jn_records)) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "jn_bursts must be a dict and jn_records a list",
            NULL
        );
        return FALSE;
    }
    if(!command) {
        command = "";
    }
    if(!date) {
        date = "";
    }

    tty_close_bursts(jn_bursts, FALSE, jn_records);

    char verb[NAME_MAX];
    first_word(command, verb, sizeof(verb));
    if(strcmp(verb, CLOSE_CONSOLE) == 0) {
        tty_close_bursts(jn_bursts, TRUE, jn_records);
        return FALSE;   // close-console has its own record
    }
    if(strcmp(verb, TTY_WRITE) != 0) {
        return FALSE;
    }

    char *console = param_value(command, kw, "name");
    char *content64 = param_value(command, kw, CONTENT64_KEY);
    ssize_t size = content64? base64_decoded_size(content64, strlen(content64)): -1;
    json_int_t bytes = (size > 0)? (json_int_t)size: 0;
    const char *user = command_user(kw);
    json_t *jn_source = source_summary(kw);
    const char *console_name = console? console: "";

    json_t *jn_burst = json_object_get(jn_bursts, console_name);
    if(jn_burst) {
        const char *burst_user = json_string_value(json_object_get(jn_burst, "user"));
        json_t *burst_source = json_object_get(jn_burst, "source");
        BOOL same_source = (!jn_source && json_is_null(burst_source)) ||
            (jn_source && json_equal(jn_source, burst_source));
        if(strcmp(burst_user? burst_user: "", user) != 0 || !same_source) {
            tty_close_burst(jn_burst, console_name, jn_records);
            json_object_del(jn_bursts, console_name);
            jn_burst = NULL;
        }
    }

    if(jn_burst) {
        /*
         *  Counted in its burst: written when the burst ends
         */
        json_int_t writes = json_integer_value(json_object_get(jn_burst, "writes"));
        if(writes == 0) {
            json_object_set_new(jn_burst, "date", json_string(date));
        }
        json_object_set_new(jn_burst, "writes", json_integer(writes + 1));
        json_object_set_new(jn_burst, "bytes",
            json_integer(json_integer_value(json_object_get(jn_burst, "bytes")) + bytes)
        );
        json_object_set_new(jn_burst, "until", json_string(date));

    } else {
        /*
         *  A new burst: its first write is written at once
         */
        json_t *jn_record = tty_record(date, NULL, user, console_name, 1, bytes, jn_source);
        if(jn_record) {
            json_array_append_new(jn_records, jn_record);
        }
        json_object_set_new(jn_bursts, console_name, json_pack("{s:s, s:O, s:I, s:I, s:I, s:s, s:s}",
            "user",     user,
            "source",   jn_source? jn_source: json_null(),
            "deadline", (json_int_t)start_msectimer((uint64_t)burst_seconds * 1000),
            "writes",   (json_int_t)0,
            "bytes",    (json_int_t)0,
            "date",     "",
            "until",    ""
        ));
    }

    JSON_DECREF(jn_source)
    GBMEM_FREE(console);
    GBMEM_FREE(content64);
    return TRUE;
}
