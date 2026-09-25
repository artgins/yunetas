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
 *          - The command word is taken as the command parser takes it: the
 *            first word (blanks are ' ' and '\t'), without its quotes, and
 *            looked up in the command table of the agent, which knows the
 *            case (strcasecmp) and the aliases (EV_WRITE_TTY, "1", ...).
 *            WRITE-TTY, 'write-tty' and EV_WRITE_TTY are write-tty. A word
 *            the table does not know is compared in lower case. The
 *            command carried by command-agent is looked up the same way
 *            (it runs in the agent); the one carried by command-yuno runs
 *            in another yuno, whose table is not known here: lower case.
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
 *            case) one of secret_name_parts[], or one of
 *            secret_name_joined[] once '_', '-', '.' and blanks are taken
 *            out, or "priv" and "key": password, user_passw, client_secret,
 *            access_token, api_key, x-api-key, http_cookie, private_key,
 *            ... (see those lists for why each one is there). And the
 *            `value` of a write-attr whose `attribute` has such a name (any
 *            case, in the text, in the kw, or in the same json object).
 *            And the token after "Bearer ", the credentials after "Basic "
 *            (when they are base64 of "user:password"), and anything with
 *            the shape of a JWT (eyJ..., three parts; a '.' after it, the
 *            end of a sentence, is not a fourth), wherever they are.
 *
 *          - Both apply everywhere in the record: to a kw key of that name
 *            at any depth, and inside any string (the command text,
 *            __command__, the `command` carried by command-yuno, a json
 *            given as text) to `name=value` (quoted or not, blanks around
 *            the `=` as the command parser allows) and to `"name": value`
 *            (the name of a json key is read with its escapes: "\u0070"
 *            is "p"). The value of a `name=value` secret is its whole WORD,
 *            the shell's way: quoted pieces with their blanks, a \" inside
 *            "..." and the shell's '\'' (password="a\" S",
 *            password='it'\''s S'). A json string value of a secret goes to
 *            its own closing quote, also beyond a '...' that the first "'"
 *            of the string would end ('{"password":"it's S"}').
 *            And inside a json text given inside a json text,
 *            whose quotes are escaped ({"cfg":"{\"password\":\"x\"}"}),
 *            at any depth up to MAX_ESCAPE_LEVELS: a quoted run with a
 *            backslash is scanned again with its escapes decoded (see
 *            scan_escaped_run()). Deeper, such a run is not written: its
 *            size and sha256 only. Whatever quotes come before it (a stray
 *            quote, a "..." parameter: every quote begins a run), and when
 *            no run holds it whole (the parser's "..." ends at its first
 *            \", or it has no quote of its own: '{\"password\":\"x\"}'):
 *            its key is then read by its shape (\"name\": with the
 *            backslashes of its level, the whole string back to the quote
 *            of that level: \"secret key\" is a secret) and its value
 *            taken with the quotes of that level. What the audit sees as text is judged as text,
 *            whatever the parser does with it later.
 *
 *          - The scan of a string is ONE pass, in linear time: a value
 *            inside a quoted value is followed with a small stack of
 *            regions (MAX_REGIONS), and the key of a json member is the
 *            last string seen before its ':'. The only recursion is one
 *            level for each level of escaped json (a quoted run with a
 *            backslash, decoded), at most MAX_ESCAPE_LEVELS deep, each
 *            level scanned in one pass and charged to the budget. The audit
 *            runs before the parser and the authz, on the text that any
 *            peer sends: a first version went one level of recursion
 *            deeper for each '=' in a run without blanks (O(n^2) time,
 *            O(n) stack), and 150 000 '=' in one command crashed the
 *            agent. And the work of one record has a cap
 *            (AUDIT_SCAN_BUDGET bytes scanned): a string beyond it is not
 *            scanned and not written, only its size and its sha256:
 *            `<N bytes, not scanned, sha256:HEX>`, and for the command
 *            text its first word before that.
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
 *            carry, taken where their handler takes it: the last
 *            top-level `command=` of the text, else kw.command. The key
 *            EXACTLY `command`: the parser matches a key of the text in any
 *            case, but it stores the value under the key as typed, and the
 *            handler reads kw["command"]. So `COMMAND=list-yunos` is not the
 *            command that runs: a text or kw key `command` in another case
 *            makes the record full (never read-only). That command is
 *            named in the record. The same for `name` and `content64` of a
 *            console write.
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
 *  The bytes scanned for one record (all its strings). An install-binary of
 *  a 96 MB binary (128 MB of base64) is still scanned: its record says the
 *  size and the sha256 of the binary.
 */
#define AUDIT_SCAN_BUDGET   (128*1024*1024)

/*
 *  Quoted values inside quoted values that the scan follows. Deeper, the
 *  scan goes on over the text all the same (nothing is left unscanned):
 *  only the end of a value is then the end of the outer one.
 */
#define MAX_REGIONS     16

/*
 *  Levels of a json text inside a json text (quotes escaped once more at
 *  each level) that the scan decodes. A quoted run with a backslash below
 *  the last level is not written: its size and sha256 only.
 */
#define MAX_ESCAPE_LEVELS   8

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
#define COMMAND_AGENT   "command-agent"
#define COMMAND_YUNO    "command-yuno"

/*
 *  A parameter whose name holds one of these (any case) is a secret.
 *  Taken from the attributes and parameters of the SDK and of the projects
 *  (a scan of every SDATA / SDATAPM name, 2026-09-24):
 */
PRIVATE const char *secret_name_parts[] = {
    "passw",        // password, passwd, user_passw
    "pwd",
    "passphrase",   // the key of a private key file
    "secret",       // secret, client_secret, kc_admin_client_secret, sign_secret
    "token",        // token, access_token, refresh_token
    "jwt",
    "bearer",
    "authorization",// an HTTP Authorization header: "Bearer <token>", "Basic <user:pass>"
    "cookie",       // http_cookie (the session of a browser); cookie_domain is redacted too
    "credential",
    "salt",         // visitor_salt of webstats: with it the visitors can be told again
    0
};

/*
 *  ... or one of these, once '_', '-', '.' and blanks are taken out
 *  (api_key, apikey, x-api-key, ESIOS_API_KEY; session_id, __session_id__)
 */
PRIVATE const char *secret_name_joined[] = {
    "apikey",       // api_key of wattyzer gate_pvpc (write-attr attribute=api_key value=...)
    "sessionid",    // __session_id__ of c_ievent_srv / c_prot_mqtt2
    "sessionkey",
    "authdata",     // auth_data of c_prot_mqtt2 (the MQTT 5 auth data)
    0
};
/*
 *  Not secrets, and kept: the PATH of a key or a certificate
 *  (ssl_certificate_key, ssl_trusted_certificate), cert_pem (a public
 *  certificate), the ids of treedb (pkey, rkey, tkey), in_session,
 *  mqtt_clean_session, max_sessions, authz, auth_method, ignore_private.
 */

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef enum {
    KEY_PLAIN = 0,
    KEY_CONTENT64,
    KEY_SECRET,
} key_kind_t;

/*
 *  A value of a json text decoded from a quoted run that reaches the end
 *  of that text (see redact_param_value())
 */
typedef enum {
    CUT_NONE = 0,
    CUT_PARAM_EMPTY,        // `key=` at the end: the value is all after the run
    CUT_PARAM_TAIL,         // `key=va` at the end: the value goes on after the run
    CUT_PARAM_WORD,         // `key='a'` at the end: its word can go on after the run
    CUT_JSON_EMPTY,         // `"key":` at the end
} cut_t;

typedef struct {
    BOOL tty;               // content64 of a console write: its size only
    BOOL value_is_secret;   // write-attr of a secret attribute: `value` is a secret
    size_t budget;          // bytes that can still be scanned for this record
    int escape_level;       // the json text being scanned is inside this many quoted runs
    cut_t cut;              // how the last value of the decoded text was cut by its end
    key_kind_t cut_kind;
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
    BOOL broken;            // a replacement behind the copy (never): nothing is written
    redact_ctx_t *ctx;
} redact_scan_t;

/*
 *  The state of the scan of one string
 */
typedef struct {
    const char *end;        // the end of the text
    const char *lo;         // the region: where a key can begin
    const char *hi;         //   and where a value ends (its closing quote)
    int depth;
    const char *region_begin[MAX_REGIONS];
    const char *region_end[MAX_REGIONS];
    const char *q_last;     // the last '"' not escaped, and the one before:
    const char *q_prev;     //   a json key is the string between them
    const char *eq_last;    // the last '"' escaped (an odd run of '\' before
    const char *eq_prev;    //   it), and the one before, with their runs:
    size_t eq_last_r;       //   an escaped json key is the string between
    size_t eq_prev_r;       //   two of the same run (see scan_json_member())
    size_t backslashes;     // the run of '\' just before the current byte
    const char *run_end;    // the closing '"' of the last quoted run looked at
} scan_state_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE char *redact_text(const char *text, size_t len, redact_ctx_t *ctx);
PRIVATE char *not_scanned_text(const char *text, size_t len);
PRIVATE json_t *redacted_copy(json_t *jn, redact_ctx_t *ctx);
PRIVATE const char *scan_escaped_run(redact_scan_t *sc, scan_state_t *st, const char *q, BOOL value_run);
PRIVATE const char *redact_json_value(
    redact_scan_t *sc,
    key_kind_t kind,
    const char *v,
    const char *bound,
    size_t n
);

/***************************************************************************
 *  The descriptor of the command `word` (a word without blanks), found as
 *  command_get_cmd_desc() finds it: the same order, any case, an alias
 *  first when the command has no json_fn. Without its copy of the text
 *  and with a look at the first letter before the compare: the audit
 *  runs it twice for every command (test_audit_record checks that both
 *  answer the same).
 ***************************************************************************/
PRIVATE BOOL same_word(const char *name, const char *word, int first)
{
    return (tolower((unsigned char)name[0]) == first && strcasecmp(name, word) == 0)?
        TRUE: FALSE;
}

PRIVATE BOOL alias_matches(const char **alias, const char *word, int first)
{
    while(alias && *alias) {
        if(same_word(*alias, word, first)) {
            return TRUE;
        }
        alias++;
    }
    return FALSE;
}

PRIVATE const sdata_desc_t *find_command_in_table(const sdata_desc_t *command_table, const char *word)
{
    int first = tolower((unsigned char)word[0]);
    const sdata_desc_t *pcmd = command_table;
    while(pcmd->name) {
        BOOL alias_checked = FALSE;
        if(!pcmd->json_fn && pcmd->alias) {
            alias_checked = TRUE;
            if(alias_matches(pcmd->alias, word, first)) {
                return pcmd;
            }
        }
        if(same_word(pcmd->name, word, first)) {
            return pcmd;
        }
        if(!alias_checked && alias_matches(pcmd->alias, word, first)) {
            return pcmd;
        }
        pcmd++;
    }
    return NULL;
}

/*
 *  The last words looked up: a few commands come again and again (the
 *  polling of a GUI, the keystrokes of a console). The tables are static
 *  arrays of the gclasses, so an answer stays valid.
 */
#define LOOKUP_CACHE_SIZE   16

typedef struct {
    const sdata_desc_t *command_table;
    char word[64];
    const sdata_desc_t *cmd_desc;
} lookup_cache_t;

PRIVATE lookup_cache_t lookup_cache[LOOKUP_CACHE_SIZE];   // one place for each hash of a word

PRIVATE const sdata_desc_t *find_command(const sdata_desc_t *command_table, const char *word)
{
    size_t len = strlen(word);
    if(len >= sizeof(lookup_cache[0].word)) {
        return find_command_in_table(command_table, word);
    }
    unsigned hash = (unsigned)len;
    for(size_t i=0; i<len; i++) {
        hash = hash*31 + (unsigned char)word[i];
    }
    lookup_cache_t *c = &lookup_cache[hash % LOOKUP_CACHE_SIZE];
    if(c->command_table == command_table && strcmp(c->word, word) == 0) {
        return c->cmd_desc;
    }
    const sdata_desc_t *cmd_desc = find_command_in_table(command_table, word);
    c->command_table = command_table;
    memcpy(c->word, word, len + 1);
    c->cmd_desc = cmd_desc;
    return cmd_desc;
}

/***************************************************************************
 *  The command word of a text, taken as the command parser takes it
 *  (get_parameter(), then command_get_cmd_desc()): the first word after
 *  ' ' and '\t', without its quotes if they close, looked up in the
 *  command table (any case, aliases). The name of the command if the
 *  table knows it, else the word in lower case.
 ***************************************************************************/
PRIVATE void command_word(
    const char *text,
    const sdata_desc_t *command_table,
    char *bf,
    size_t bfsize
)
{
    const char *s = text;
    while(*s == ' ' || *s == '\t') {
        s++;
    }
    const char *e = NULL;
    if(*s == '\'' || *s == '"') {
        const char *q = strchr(s + 1, *s);
        if(q) {
            e = q;
            s++;
        }
    }
    if(!e) {
        e = s;
        while(*e && *e != ' ' && *e != '\t') {
            e++;
        }
    }

    size_t len = (size_t)(e - s);
    if(len >= bfsize) {
        len = bfsize - 1;   // not a command name: the parser does not know it either
    }
    memcpy(bf, s, len);
    bf[len] = 0;

    if(command_table && len > 0 && !strpbrk(bf, " \t")) {
        const sdata_desc_t *cmd_desc = find_command(command_table, bf);
        if(cmd_desc && cmd_desc->name) {
            size_t name_len = strlen(cmd_desc->name);
            if(name_len >= bfsize) {
                name_len = bfsize - 1;
            }
            memcpy(bf, cmd_desc->name, name_len);
            bf[name_len] = 0;
            return;
        }
    }
    for(size_t i=0; i<len; i++) {
        bf[i] = (char)tolower((unsigned char)bf[i]);
    }
}

/***************************************************************************
 *  The value of the top-level parameter `name` of a command text, as the
 *  handler gets it: get_parameter() for the command, then
 *  get_key_value_parameter(), the last one wins. The key EXACTLY `name`:
 *  build_cmd_kw() stores a value under the key as typed, and the handler
 *  reads the exact key. `*other_case` (optional): the value of the last
 *  key that is `name` in another case (COMMAND=...), a value the handler
 *  does not read, else NULL. NULL if absent. Free the results with
 *  gbmem_free().
 ***************************************************************************/
PRIVATE char *text_param(const char *text, const char *name, char **other_case)
{
    if(other_case) {
        *other_case = NULL;
    }
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
    const char *found_other_case = NULL;
    char *key;
    char *value;
    while((value = get_key_value_parameter(p, &key, &p))) {
        if(!key) {
            break;
        }
        if(strcmp(key, name) == 0) {
            found = value;
        } else if(strcasecmp(key, name) == 0) {
            found_other_case = value;
        }
    }
    char *ret = found? gbmem_strdup(found): NULL;
    if(other_case && found_other_case) {
        *other_case = gbmem_strdup(found_other_case);
    }
    gbmem_free(copy);
    return ret;
}

/***************************************************************************
 *  The string value of `name`, as the command parser takes it: the text
 *  first, else the kw. NULL if none. Free with gbmem_free().
 ***************************************************************************/
PRIVATE char *param_value(const char *command, json_t *kw, const char *name)
{
    char *value = text_param(command, name, NULL);
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

PRIVATE BOOL is_wrapper(const char *verb)
{
    return (strcmp(verb, COMMAND_AGENT) == 0 || strcmp(verb, COMMAND_YUNO) == 0)? TRUE: FALSE;
}

/***************************************************************************
 *  The command carried by a wrapper (command-yuno, command-agent), "" if
 *  none. Where the handler takes it: the text wins over the kw, the key
 *  exactly `command`.
 *  `other_bf`: the word of a command under a key `command` in another
 *  case (COMMAND=..., in the text or in the kw), "" if none. It does not
 *  run, but it is judged all the same: its presence makes the record full
 *  (see command_is_read_only()), and a write-tty there hides its
 *  keystroke (the redaction is stricter than the parser, never looser).
 *  command-agent runs it in the agent: looked up in its table.
 ***************************************************************************/
PRIVATE void inner_command(
    const char *command,
    json_t *kw,
    const char *verb,
    const sdata_desc_t *command_table,
    char *bf,
    size_t bfsize,
    BOOL *in_text,
    char *other_bf,
    size_t other_bfsize,
    BOOL *other_case
)
{
    const sdata_desc_t *table = (strcmp(verb, COMMAND_AGENT) == 0)? command_table: NULL;

    *bf = 0;
    *in_text = FALSE;
    *other_bf = 0;
    *other_case = FALSE;

    char *value_other_case = NULL;
    char *value = text_param(command, "command", &value_other_case);
    if(value_other_case) {
        *other_case = TRUE;
        command_word(value_other_case, table, other_bf, other_bfsize);
        gbmem_free(value_other_case);
    }
    if(json_is_object(kw)) {
        const char *key;
        json_t *jn_value;
        json_object_foreach(kw, key, jn_value) {
            if(strcmp(key, "command") != 0 && strcasecmp(key, "command") == 0) {
                *other_case = TRUE;
                if(!*other_bf && json_is_string(jn_value)) {
                    command_word(json_string_value(jn_value), table, other_bf, other_bfsize);
                }
            }
        }
    }

    if(value) {
        *in_text = TRUE;
        command_word(value, table, bf, bfsize);
        gbmem_free(value);
        return;
    }
    json_t *jn_inner = json_is_object(kw)? json_object_get(kw, "command"): NULL;
    if(json_is_string(jn_inner)) {
        command_word(json_string_value(jn_inner), table, bf, bfsize);
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
 *  TRUE if `name` (of `len` bytes) is the name of a secret parameter.
 *  One pass over the name, no copy: at each byte, only the parts that
 *  begin with that letter are compared. Any case; for the joined parts the
 *  bytes '_', '-', '.' and blanks of the name are skipped.
 ***************************************************************************/
PRIVATE BOOL is_joiner(char c)
{
    return (c == '_' || c == '-' || c == '.' || c == ' ' || c == '\t')? TRUE: FALSE;
}

PRIVATE char ascii_lower(char c)
{
    return (c >= 'A' && c <= 'Z')? (char)(c + ('a' - 'A')): c;
}

/*
 *  TRUE if `part` (lower case) begins at name[i]
 */
PRIVATE BOOL part_at(const char *name, size_t len, size_t i, const char *part, BOOL joined)
{
    size_t j = i;
    size_t k = 0;
    while(part[k]) {
        if(j >= len) {
            return FALSE;
        }
        char c = name[j];
        if(joined && k > 0 && is_joiner(c)) {
            j++;
            continue;
        }
        if(ascii_lower(c) != part[k]) {
            return FALSE;
        }
        j++;
        k++;
    }
    return TRUE;
}

/*
 *  The letters that begin a part (made once from the lists): at any other
 *  byte of a name nothing is compared
 */
PRIVATE BOOL first_letters_done = FALSE;
PRIVATE BOOL first_letters[256];

PRIVATE void make_first_letters(void)
{
    for(int p=0; secret_name_parts[p]; p++) {
        first_letters[(unsigned char)secret_name_parts[p][0]] = TRUE;
    }
    for(int p=0; secret_name_joined[p]; p++) {
        first_letters[(unsigned char)secret_name_joined[p][0]] = TRUE;
    }
    first_letters['p'] = TRUE;  // priv
    first_letters['k'] = TRUE;  // key
    first_letters_done = TRUE;
}

PRIVATE BOOL is_secret_name(const char *name, size_t len)
{
    if(!first_letters_done) {
        make_first_letters();
    }
    BOOL has_priv = FALSE;
    BOOL has_key = FALSE;
    for(size_t i=0; i<len; i++) {
        char c = ascii_lower(name[i]);
        if(!first_letters[(unsigned char)c]) {
            continue;
        }
        for(int p=0; secret_name_parts[p]; p++) {
            if(secret_name_parts[p][0] == c && part_at(name, len, i, secret_name_parts[p], FALSE)) {
                return TRUE;
            }
        }
        for(int p=0; secret_name_joined[p]; p++) {
            if(secret_name_joined[p][0] == c && part_at(name, len, i, secret_name_joined[p], TRUE)) {
                return TRUE;
            }
        }
        if(c == 'p' && !has_priv) {
            has_priv = part_at(name, len, i, "priv", FALSE);
        } else if(c == 'k' && !has_key) {
            has_key = part_at(name, len, i, "key", FALSE);
        }
    }
    return (has_priv && has_key)? TRUE: FALSE;
}

PRIVATE key_kind_t key_kind(const char *key, size_t len, const redact_ctx_t *ctx)
{
    if(len == strlen(CONTENT64_KEY) && strncasecmp(key, CONTENT64_KEY, len) == 0) {
        return KEY_CONTENT64;
    }
    if(is_secret_name(key, len)) {
        return KEY_SECRET;
    }
    if(ctx->value_is_secret && len == 5 && strncasecmp(key, "value", 5) == 0) {
        return KEY_SECRET;
    }
    return KEY_PLAIN;
}

/***************************************************************************
 *  The kind of a json key written in a text: its escapes are decoded
 *  first ("pass\u0077ord" is "password"). A \u of a code point that is
 *  not ASCII becomes a byte that no secret name holds.
 ***************************************************************************/
PRIVATE int hex_value(char c)
{
    if(c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)tolower((unsigned char)c);
    if(c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

/*
 *  The 4 hex digits of a \u at s[i] (s[i] is the 'u'), -1 if they are not
 */
PRIVATE int u_escape_code(const char *s, size_t len, size_t i)
{
    if(i + 4 >= len) {
        return -1;
    }
    int code = 0;
    for(int h=1; h<=4; h++) {
        int v = hex_value(s[i+h]);
        if(v < 0) {
            return -1;
        }
        code = code*16 + v;
    }
    return code;
}

PRIVATE size_t put_utf8(char *out, unsigned code)
{
    if(code < 0x80) {
        out[0] = (char)code;
        return 1;
    }
    if(code < 0x800) {
        out[0] = (char)(0xC0 | (code >> 6));
        out[1] = (char)(0x80 | (code & 0x3F));
        return 2;
    }
    if(code < 0x10000) {
        out[0] = (char)(0xE0 | (code >> 12));
        out[1] = (char)(0x80 | ((code >> 6) & 0x3F));
        out[2] = (char)(0x80 | (code & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (code >> 18));
    out[1] = (char)(0x80 | ((code >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((code >> 6) & 0x3F));
    out[3] = (char)(0x80 | (code & 0x3F));
    return 4;
}

#define REPLACEMENT_CHAR    0xFFFD

/*
 *  The text of a json string with its escapes decoded, leniently (it is
 *  scanned, not validated): an escape that json does not know stays as it
 *  is, backslash and char (the shell's \' of password='it'\''s S': taken
 *  as "'" it made a different word of the value, and the rest of the
 *  secret was written; up to review 19), a \u
 *  without 4 hex digits is "u", a code point that is not ASCII is written
 *  in UTF-8, and a lone surrogate and \u0000 as U+FFFD (the text stays a
 *  C string). `out` holds `len` + 1 bytes: a decoded text is never longer.
 *  Return its length.
 */
PRIVATE size_t json_unescape(const char *s, size_t len, char *out)
{
    size_t n = 0;
    for(size_t i=0; i<len; i++) {
        char c = s[i];
        if(c != '\\' || i + 1 >= len) {
            out[n++] = c;
            continue;
        }
        char e = s[++i];
        switch(e) {
            case 'b':
                out[n++] = '\b';
                break;
            case 'f':
                out[n++] = '\f';
                break;
            case 'n':
                out[n++] = '\n';
                break;
            case 'r':
                out[n++] = '\r';
                break;
            case 't':
                out[n++] = '\t';
                break;
            case 'u':
                {
                    int code = u_escape_code(s, len, i);
                    if(code < 0) {
                        out[n++] = e;
                        break;
                    }
                    i += 4;
                    unsigned cp = (unsigned)code;
                    if(cp >= 0xD800 && cp <= 0xDBFF && i + 2 < len &&
                            s[i+1] == '\\' && s[i+2] == 'u') {
                        int low = u_escape_code(s, len, i + 2);
                        if(low >= 0xDC00 && low <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + ((unsigned)low - 0xDC00);
                            i += 6;
                        }
                    }
                    if(cp == 0 || (cp >= 0xD800 && cp <= 0xDFFF)) {
                        cp = REPLACEMENT_CHAR;
                    }
                    n += put_utf8(out + n, cp);
                }
                break;
            case '"':
            case '\\':
            case '/':
                out[n++] = e;
                break;
            default:    // not a json escape: as it is (n+2 <= i+1, in place too)
                out[n++] = '\\';
                out[n++] = e;
                break;
        }
    }
    out[n] = 0;
    return n;
}

PRIVATE key_kind_t json_key_kind(const char *key, size_t len, const redact_ctx_t *ctx)
{
    if(!memchr(key, '\\', len)) {
        return key_kind(key, len, ctx);
    }

    char *bf = gbmem_malloc(len + 1);
    if(!bf) {
        return KEY_SECRET;  // Error already logged. Never write what cannot be judged
    }
    /*
     *  A key of a json text N levels deep has its escapes written N times
     *  ("pass\\u0077ord" two levels down): decoded while a backslash is
     *  left. A decoded text is never longer: in place.
     */
    memcpy(bf, key, len);
    size_t n = len;
    for(int level=0; level<=MAX_ESCAPE_LEVELS && memchr(bf, '\\', n); level++) {
        n = json_unescape(bf, n, bf);
    }
    key_kind_t kind = key_kind(bf, n, ctx);
    gbmem_free(bf);
    return kind;
}

/***************************************************************************
 *  TRUE if a write-attr names a secret attribute (its `value` is then a
 *  secret): `attribute=<name>` or `"attribute": "<name>"` (the key in any
 *  case, blanks and quotes around, escaped or not), anywhere in the text, or
 *  kw.attribute. One pass over the text.
 ***************************************************************************/
#define ATTRIBUTE_KEY   "attribute"
#define ATTRIBUTE_AROUND " \t'\"\\"    // blanks, quotes and the backslashes of an escaped json

PRIVATE BOOL kw_names_secret_attribute(json_t *kw)
{
    if(!json_is_object(kw)) {
        return FALSE;
    }
    const char *key;
    json_t *value;
    json_object_foreach(kw, key, value) {
        if((key[0] == 'a' || key[0] == 'A') && strcasecmp(key, ATTRIBUTE_KEY) == 0 &&
                json_is_string(value) &&
                is_secret_name(json_string_value(value), json_string_length(value))) {
            return TRUE;
        }
    }
    return FALSE;
}

PRIVATE BOOL names_secret_attribute(const char *text, size_t len, json_t *kw)
{
    if(kw_names_secret_attribute(kw)) {
        return TRUE;
    }

    size_t key_len = strlen(ATTRIBUTE_KEY);
    const char *end = text + strnlen(text, len);   // strcasestr() stops at a nul
    const char *p = text;
    while(p + key_len <= end && (p = strcasestr(p, ATTRIBUTE_KEY)) != NULL) {
        const char *v = p + key_len;
        while(v < end && strchr(ATTRIBUTE_AROUND, *v)) {
            v++;
        }
        if(v >= end || (*v != '=' && *v != ':')) {
            p = v;
            continue;
        }
        v++;
        while(v < end && strchr(ATTRIBUTE_AROUND, *v)) {
            v++;
        }
        const char *e = v;
        while(e < end && !strchr(" \t'\",}\\", *e)) {
            e++;
        }
        if(is_secret_name(v, (size_t)(e - v))) {
            return TRUE;
        }
        p = (e > p)? e: p + 1;
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
    const char *inner,
    BOOL inner_other_case
)
{
    if(has_reset(command, kw)) {
        return FALSE;
    }
    if(wrapper) {
        if(!*inner || is_wrapper(inner) || inner_other_case) {
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
 *  Copy the text up to `p`. Every scan replaces ahead of what was
 *  copied: a `p` behind it is a broken invariant, and then the text is
 *  not written at all (it may hold what was to be redacted).
 */
PRIVATE BOOL out_copy_upto(redact_scan_t *sc, const char *p)
{
    if(p < sc->from) {
        if(!sc->broken) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "audit scan: a replacement behind the copy",
                "offset",       "%zu", (size_t)(p - sc->text),
                "from",         "%zu", (size_t)(sc->from - sc->text),
                NULL
            );
        }
        sc->broken = TRUE;
        return FALSE;
    }
    out_append(sc, sc->from, (size_t)(p - sc->from));
    return TRUE;
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
    if(end < begin || !out_copy_upto(sc, begin)) {
        sc->broken = TRUE;  // Error already logged (or a value that ends before it begins: never)
        return;
    }
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
 *  The scan jumped over a value: what came before is no json key
 ***************************************************************************/
PRIVATE void forget_quotes(scan_state_t *st)
{
    st->q_last = NULL;
    st->q_prev = NULL;
    st->eq_last = NULL;
    st->eq_prev = NULL;
    st->eq_last_r = 0;
    st->eq_prev_r = 0;
    st->backslashes = 0;
}

PRIVATE BOOL is_blank(char c)
{
    return (c == ' ' || c == '\t')? TRUE: FALSE;
}

/***************************************************************************
 *  A json text inside a json text has its quotes escaped once more at
 *  each level: a quote of level 0 is `"`, of level 1 `\"`, of level 2
 *  `\\\"`: n = 2^L - 1 backslashes. Inside a string of level n, a quote
 *  with r backslashes before it closes the string when r % (2n+2) == n
 *  (the other r are a quote or a backslash of a deeper level), and one
 *  with fewer than n closes the string AROUND it.
 ***************************************************************************/
PRIVATE size_t backslashes_before(const char *lo, const char *p)
{
    size_t r = 0;
    while(p > lo && p[-1] == '\\') {
        p--;
        r++;
    }
    return r;
}

/*
 *  The end of the text of a string of level `n` that begins at `s` (the
 *  backslashes of its closing quote are not its text), and in `next`
 *  where the scan goes on. Never closed: `bound`.
 */
PRIVATE const char *level_string_end(const char *s, const char *bound, size_t n, const char **next)
{
    size_t r = 0;
    for(const char *e = s; e < bound; e++) {
        if(*e == '\\') {
            r++;
            continue;
        }
        if(*e == '"') {
            if(r < n) {
                *next = e - r;      // the string around it ends: so does this one
                return e - r;
            }
            if((r % (2*n + 2)) == n) {
                *next = e + 1;
                return e - n;
            }
        }
        r = 0;
    }
    *next = bound;
    return bound;
}

/*
 *  The end of an object or a list of level `n` that begins at `v`
 */
PRIVATE const char *level_container_end(const char *v, const char *bound, size_t n)
{
    int depth = 0;
    BOOL in_string = FALSE;
    size_t r = 0;
    for(const char *e = v; e < bound; e++) {
        char c = *e;
        if(c == '\\') {
            r++;
            continue;
        }
        if(c == '"') {
            if(r < n) {
                return e - r;       // the string around it ends
            }
            if((r % (2*n + 2)) == n) {
                in_string = in_string? FALSE: TRUE;
            }
            r = 0;
            continue;
        }
        r = 0;
        if(in_string) {
            continue;
        }
        if(c == '{' || c == '[') {
            depth++;
        } else if(c == '}' || c == ']') {
            depth--;
            if(depth == 0) {
                return e + 1;
            }
        }
    }
    return bound;
}

/*
 *  Redact the value of a json member at `v` (after its ':'), whose key's
 *  quotes are of level `n`: a string (the quotes stay), an object or a
 *  list (up to the bracket that closes it), or a scalar. Return where the
 *  scan goes on.
 */
#define MAX_QUOTE_BACKSLASHES   63

PRIVATE const char *redact_json_value(
    redact_scan_t *sc,
    key_kind_t kind,
    const char *v,
    const char *bound,
    size_t n
)
{
    while(v < bound && isspace((unsigned char)*v)) {
        v++;
    }
    if(v >= bound) {
        return bound;
    }

    size_t r = 0;
    while(v + r < bound && v[r] == '\\') {
        r++;
    }
    if(v + r < bound && v[r] == '"') {
        /*
         *  Of level r: an escaped key whose value is not escaped (a run cut
         *  where the escaped json is not whole) is still a value
         */
        const char *next;
        const char *e = level_string_end(v + r + 1, bound, r, &next);
        replace_value(sc, kind, v + r + 1, e);
        return next;
    }

    const char *e = v;
    if(r == 0 && (*v == '{' || *v == '[')) {
        e = level_container_end(v, bound, n);
    } else {
        const char *ends = n? ",}] \t\r\n\\\"": ",}] \t\r\n";
        while(e < bound && !strchr(ends, *e)) {
            e++;
        }
        if(e == v) {
            return v;
        }
    }

    /*
     *  A string in place of it, with the quotes of its level
     */
    char replacement[2*MAX_QUOTE_BACKSLASHES + sizeof(REDACTED) + 3];
    size_t len = 0;
    size_t nq = (n <= MAX_QUOTE_BACKSLASHES)? n: 0;
    for(int side=0; side<2; side++) {
        for(size_t i=0; i<nq; i++) {
            replacement[len++] = '\\';
        }
        replacement[len++] = '"';
        if(side == 0) {
            memcpy(replacement + len, REDACTED, strlen(REDACTED));
            len += strlen(REDACTED);
        }
    }
    replacement[len] = 0;
    out_replace(sc, v, e, replacement);
    return e;
}

/*
 *  A byte of the name of a json key read without its quotes paired (the
 *  backslash: its \u escapes)
 */
PRIVATE BOOL is_key_byte(char c)
{
    return (isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.' || c == '\\')? TRUE: FALSE;
}

/***************************************************************************
 *  The closing quote of the quoted value at `v` ('...' or "..."), NULL if
 *  there is none before `bound`. Inside "..." a quote with an odd run of
 *  '\' before it is escaped (password="a\" S"); inside '...' nothing is.
 ***************************************************************************/
PRIVATE const char *closing_quote(const char *v, const char *bound)
{
    char quote = *v;
    size_t r = 0;
    for(const char *e = v + 1; e < bound; e++) {
        if(*e == quote && (quote == '\'' || (r % 2) == 0)) {
            return e;
        }
        r = (*e == '\\')? r + 1: 0;
    }
    return NULL;
}

/*
 *  The end of the word that goes on at `p`, up to a blank, the shell's
 *  way: a quoted piece is taken whole (blanks included), and a '\' takes
 *  the byte after it. So the shell's quote inside a quoted value,
 *  password='it'\''s S', is ONE word. In `last_quote` the closing quote of
 *  its last piece when the word ends with it, else NULL. A piece never
 *  closed goes to `bound`.
 */
PRIVATE const char *word_end(const char *p, const char *bound, const char **last_quote)
{
    *last_quote = NULL;
    while(p < bound && !is_blank(*p)) {
        *last_quote = NULL;
        if(*p == '\\') {
            p += (p + 1 < bound)? 2: 1;
        } else if(*p == '\'' || *p == '"') {
            const char *q = closing_quote(p, bound);
            if(!q) {
                return bound;
            }
            *last_quote = q;
            p = q + 1;
        } else {
            p++;
        }
    }
    return p;
}

/*
 *  A word that ends at the quote closing its region goes on beyond it
 *  when a byte of a word follows that quote: the quote is in the word
 *  (x="a password='a'"b S"'c T'"). Before a blank, a json separator or the
 *  end, it closes the region and the word (command="... password='S'" n=1).
 */
PRIVATE BOOL word_goes_on(const char *quote, const char *end)
{
    return (quote + 1 < end && !is_blank(quote[1]) && !strchr(",:}]", quote[1]))? TRUE: FALSE;
}

PRIVATE const char *word_beyond_region(scan_state_t *st, const char *w, const char **last_quote)
{
    if(st->depth > 0 && w == st->hi && w < st->end && word_goes_on(w, st->end)) {
        return word_end(w, st->end, last_quote);
    }
    return w;
}

/***************************************************************************
 *  The value at `v` of a secret or a content64 parameter (see
 *  scan_param()): its whole word, the shell's way (see word_end()), not
 *  what the parser would take. Up to review 19 a quoted value ended at the
 *  first quote of its kind, escaped or not, and a quote inside a secret
 *  left the rest of it in the record (password="a\" S",
 *  password='it'\''s S'). In a json text decoded from a quoted run
 *  (escape_level > 0), a value that reaches the end of the text can go on
 *  after the run's closing quote: the run is the text between two
 *  quotes, not always a string (x" token="S T"). Nothing of it here: it
 *  is left to the level above, which sees the key too (ctx->cut). Some of
 *  it here: redacted, and the level above redacts the rest. A quoted piece
 *  that closes at the very end is whole here, but its word can go on
 *  after the run all the same (a stray quote began the run, and the quote
 *  that ended it is in the word: x" \ token='a'"b S"): the level above
 *  goes on after the run's quote (see CUT_PARAM_WORD in
 *  scan_escaped_run()).
 *  Return where the scan goes on.
 ***************************************************************************/
PRIVATE const char *redact_param_value(
    redact_scan_t *sc,
    scan_state_t *st,
    key_kind_t kind,
    const char *v
)
{
    if(v >= st->end && sc->ctx->escape_level > 0) {
        sc->ctx->cut = CUT_PARAM_EMPTY;
        sc->ctx->cut_kind = kind;
        forget_quotes(st);
        return st->end;
    }

    const char *hi = st->hi;
    if(v == st->hi && st->depth > 0 && v < st->end && (*v == '\'' || *v == '"')) {
        hi = st->end;   // the quote that closes the region opens the value
    }
    size_t r = 0;
    while(v + r < hi && v[r] == '\\') {
        r++;
    }

    const char *v_begin;
    const char *v_end;
    const char *next;
    cut_t cut = CUT_NONE;
    if(v < hi && (*v == '\'' || *v == '"')) {
        /*
         *  Up to its own quote even beyond the region (see scan_param()),
         *  and on to the end of its word: in the region when that quote is
         *  in it, else in the text
         */
        const char *q = closing_quote(v, st->end);
        v_begin = v + 1;
        if(!q) {
            v_end = st->end;
            next = st->end;
            cut = CUT_PARAM_TAIL;
        } else {
            const char *last_quote;
            const char *w = word_end(q + 1, (q < st->hi)? st->hi: st->end, &last_quote);
            w = word_beyond_region(st, w, &last_quote);
            if(w == q + 1) {
                v_end = q;
                last_quote = q;
            } else {
                v_end = last_quote? last_quote: w;
            }
            next = w;
            if(w >= st->end) {
                cut = last_quote? CUT_PARAM_WORD: CUT_PARAM_TAIL;
            }
        }
    } else if(r % 2 == 1 && v + r < st->end && v[r] == '"') {
        v_begin = v + r + 1;
        v_end = level_string_end(v_begin, st->end, r, &next);
        if(next <= v_end) {
            cut = CUT_PARAM_TAIL;
        }
    } else {
        const char *last_quote;
        v_begin = v;
        v_end = word_end(v, st->hi, &last_quote);
        v_end = word_beyond_region(st, v_end, &last_quote);
        next = v_end;
        if(v_end >= st->end) {
            cut = last_quote? CUT_PARAM_WORD: CUT_PARAM_TAIL;
        }
    }

    replace_value(sc, kind, v_begin, v_end);
    if(cut != CUT_NONE && sc->ctx->escape_level > 0) {
        sc->ctx->cut = cut;
        sc->ctx->cut_kind = kind;
    }
    forget_quotes(st);
    return next;
}

/***************************************************************************
 *  `key=value` at `eq`, the parser's way: the key is the last word before
 *  the '=' (blanks allowed around it, or a quoted word); the value is
 *  quoted ('' or "", up to the same quote, or to the end of the region if
 *  it never closes) or runs to a blank. A plain value is not skipped: the
 *  scan goes on inside it (a command or a json inside the value), and a
 *  quoted one becomes a region.
 *
 *  The value of a secret or a content64 is taken WHOLE, whatever the
 *  parser would take: a quoted one up to its own quote even beyond the
 *  region (the quote that closes a region can be the one that opens the
 *  value: x="a password="S T"), and \"two words\" up to its escaped
 *  quote. And the "..." value of a plain key is first looked at as a
 *  quoted run (scan_escaped_run()): the parser ends it at the first '"',
 *  escaped or not, and a json text inside it ("{\"password\":...}") is
 *  the run up to the first '"' NOT escaped. Return where the scan goes on.
 ***************************************************************************/
PRIVATE const char *scan_param(redact_scan_t *sc, scan_state_t *st, const char *eq)
{
    const char *k_end = eq;
    while(k_end > st->lo && is_blank(k_end[-1])) {
        k_end--;
    }
    const char *k = k_end;
    if(k_end > st->lo && (k_end[-1] == '\'' || k_end[-1] == '"')) {
        /*
         *  A quoted key ('password'=x): the parser keeps the quotes in the
         *  key, the audit judges the word between them
         */
        char quote = k_end[-1];
        k_end--;
        k = k_end;
        while(k > st->lo && k[-1] != quote) {
            k--;
        }
    } else {
        while(k > st->lo && !strchr(" \t'\"=", k[-1])) {
            k--;
        }
    }
    key_kind_t kind = key_kind(k, (size_t)(k_end - k), sc->ctx);

    const char *v = eq + 1;
    if(kind != KEY_PLAIN) {
        while(v < st->hi && is_blank(*v)) {
            v++;    // the value goes to the next word, better said than written
        }
    }

    if(kind == KEY_PLAIN) {
        if(v >= st->hi || (*v != '\'' && *v != '"')) {
            return eq + 1;
        }
        if(*v == '"' && (!st->run_end || v >= st->run_end)) {
            const char *next = scan_escaped_run(sc, st, v, TRUE);
            if(next) {
                return next;
            }
        }
        const char *q = memchr(v + 1, *v, (size_t)(st->hi - (v + 1)));
        if(q && st->depth < MAX_REGIONS) {
            st->region_begin[st->depth] = v + 1;
            st->region_end[st->depth] = q;
            st->depth++;
            st->lo = v + 1;
            st->hi = q;
        }
        forget_quotes(st);
        return v + 1;
    }

    return redact_param_value(sc, st, kind, v);
}

/***************************************************************************
 *  `"key": value` at `colon` (a json inside a string). The key is the
 *  last string before the ':' (only blanks between). The value is a
 *  string (with its escapes), an object or a list (up to the bracket that
 *  closes it), or a scalar.
 *
 *  A key whose quotes the pass did not pair is read by its shape: a '"'
 *  (with the backslashes of its level) and a ':'. That is an escaped key
 *  (\"password\":) of a json text that no quoted run holds whole -- the
 *  parser's quote cut it (x="{\"password\":...}"), or it has no quote of
 *  its own ('{\"password\":...}') -- or a key after a value the scan
 *  jumped over. An escaped key is the WHOLE string from the escaped quote
 *  before it with the same backslashes (\"secret key\", \"password/db\"),
 *  judged as a paired key is; up to review 19 it was the last word only,
 *  so "secret key" was read as "key". A key whose opening quote is not
 *  the escaped quote just before (a deeper quote inside it), or a key of
 *  plain quotes, is the word before the quote. Its value is then taken
 *  with the quotes of that level, up to the end of the text.
 *
 *  A string value of a secret goes to its closing quote even beyond the
 *  region: a region ends at the first quote of its kind, and a json string
 *  inside a '...' value can hold a "'" ('{"password":"it's S"}').
 *  Return where the scan goes on.
 ***************************************************************************/
PRIVATE const char *scan_json_member(redact_scan_t *sc, scan_state_t *st, const char *colon)
{
    const char *k_end = colon;
    while(k_end > st->lo && isspace((unsigned char)k_end[-1])) {
        k_end--;
    }

    key_kind_t kind;
    size_t n = 0;
    const char *bound = st->hi;
    if(st->q_last && st->q_prev && k_end - 1 == st->q_last && st->q_prev >= st->lo) {
        const char *k = st->q_prev + 1;
        kind = json_key_kind(k, (size_t)(st->q_last - k), sc->ctx);
    } else if(k_end > st->lo && k_end[-1] == '"') {
        size_t r = backslashes_before(st->lo, k_end - 1);
        const char *w_end = k_end - 1;
        if(r % 2 == 1) {
            n = r;
            w_end -= r;
        }
        const char *w = w_end;
        if(r % 2 == 1 && st->eq_last == k_end - 1 && st->eq_prev &&
                st->eq_prev_r == r && st->eq_prev + 1 >= st->lo && st->eq_prev + 1 <= w_end) {
            w = st->eq_prev + 1;
        } else {
            while(w > st->lo && is_key_byte(w[-1])) {
                w--;
            }
        }
        if(w == w_end) {
            return colon + 1;
        }
        kind = json_key_kind(w, (size_t)(w_end - w), sc->ctx);
        bound = st->end;
    } else {
        return colon + 1;
    }
    if(kind == KEY_PLAIN) {
        return colon + 1;
    }

    const char *v = colon + 1;
    while(v < st->end && isspace((unsigned char)*v)) {
        v++;
    }
    if(v >= st->end && sc->ctx->escape_level > 0) {
        sc->ctx->cut = CUT_JSON_EMPTY;  // the value is after the run (see redact_param_value())
        sc->ctx->cut_kind = kind;
        forget_quotes(st);
        return st->end;
    }
    size_t rv = 0;
    while(v + rv < st->end && v[rv] == '\\') {
        rv++;
    }
    if(v + rv < st->end && v[rv] == '"') {
        bound = st->end;
    }

    const char *next = redact_json_value(sc, kind, colon + 1, bound, n);
    forget_quotes(st);
    return next;
}

/***************************************************************************
 *  "Bearer <token>" at `p` (any case, a word of its own): the token is
 *  redacted. Return where the scan goes on, NULL if it is not one.
 ***************************************************************************/
#define BEARER_WORD     "bearer"

PRIVATE const char *scan_bearer(redact_scan_t *sc, scan_state_t *st, const char *p)
{
    size_t word_len = strlen(BEARER_WORD);
    if((size_t)(st->hi - p) <= word_len || strncasecmp(p, BEARER_WORD, word_len) != 0) {
        return NULL;
    }
    if(p > st->lo && isalnum((unsigned char)p[-1])) {
        return NULL;
    }
    const char *t = p + word_len;
    if(!is_blank(*t)) {
        return NULL;
    }
    while(t < st->hi && is_blank(*t)) {
        t++;
    }
    const char *e = t;
    while(e < st->hi && !is_blank(*e) && !strchr("'\",;}]", *e)) {
        e++;
    }
    if(e == t) {
        return NULL;
    }
    out_replace(sc, t, e, REDACTED);
    forget_quotes(st);
    return e;
}

/***************************************************************************
 *  "Basic <credentials>" at `p` (any case, a word of its own), the
 *  credentials of an HTTP Authorization header: redacted when they have
 *  their shape, base64 of "user:password" (a "basic" in a text is most
 *  often the word). Return where the scan goes on, NULL if it is not one.
 ***************************************************************************/
#define BASIC_WORD      "basic"

PRIVATE const char *scan_basic(redact_scan_t *sc, scan_state_t *st, const char *p)
{
    size_t word_len = strlen(BASIC_WORD);
    if((size_t)(st->hi - p) <= word_len || strncasecmp(p, BASIC_WORD, word_len) != 0) {
        return NULL;
    }
    if(p > st->lo && isalnum((unsigned char)p[-1])) {
        return NULL;
    }
    const char *t = p + word_len;
    if(!is_blank(*t)) {
        return NULL;
    }
    while(t < st->hi && is_blank(*t)) {
        t++;
    }
    const char *e = t;
    while(e < st->hi && b64_value(*e) >= 0) {
        e++;
    }
    while(e < st->hi && *e == '=') {
        e++;
    }
    if(e - t < 4) {
        return NULL;
    }
    size_t n = 0;
    uint8_t *credentials = base64_decode(t, (size_t)(e - t), &n);
    BOOL is_credentials = (credentials && memchr(credentials, ':', n))? TRUE: FALSE;
    GBMEM_FREE(credentials);
    if(!is_credentials) {
        return NULL;
    }
    out_replace(sc, t, e, REDACTED);
    forget_quotes(st);
    return e;
}

/***************************************************************************
 *  A JWT at `p` ("eyJ" = base64url of '{"', three parts joined by '.'):
 *  redacted. The dots at the end of the run are not its own: a JWT at the
 *  end of a sentence ("... eyJ.eyJ.sig.") is one, and its dot stays (up to
 *  review 19 it was taken as a fourth part, and written). Return where the
 *  scan goes on (the end of the run of base64url bytes, a JWT or not: it
 *  holds no '=', ':' or quote).
 ***************************************************************************/
PRIVATE BOOL is_b64url(char c)
{
    return (isalnum((unsigned char)c) || c == '-' || c == '_')? TRUE: FALSE;
}

PRIVATE const char *scan_jwt(redact_scan_t *sc, scan_state_t *st, const char *p)
{
    if(st->hi - p < 3 || p[0] != 'e' || p[1] != 'y' || p[2] != 'J') {
        return NULL;
    }
    if(p > st->lo && (is_b64url(p[-1]) || p[-1] == '.')) {
        return NULL;
    }
    const char *e = p;
    int dots = 0;
    while(e < st->hi && (is_b64url(*e) || *e == '.')) {
        if(*e == '.') {
            dots++;
        }
        e++;
    }
    const char *t = e;
    while(t[-1] == '.') {
        t--;    // never beyond p: p[0] is 'e'
        dots--;
    }
    if(dots == 2) {
        out_replace(sc, p, t, REDACTED);
        forget_quotes(st);
    }
    return e;
}

/***************************************************************************
 *  A quoted run at `q` (a '"' not escaped) that holds a backslash is
 *  scanned again with its escapes decoded: a json text inside a json text
 *  has its quotes escaped (\"password\":\"x\"), and the scan of this
 *  level cannot tell a key there. The decoded text is redacted like any
 *  string (redact_text(), within the budget), so a json text two or more
 *  levels deep is decoded one level at a time. When something is
 *  replaced, the run is written back as the redacted text with its
 *  escapes; else the scan goes on inside the run, as before.
 *
 *  The run ends at the next '"' not escaped, in the region (never
 *  closed: at its end); in a '...' region, in the text (see below). EVERY quote not escaped begins a run, the closing
 *  quote of the last one included: a run is the text between two quotes
 *  in a row, so a json string is one whatever quotes came before it (a
 *  stray quote, the closing quote of a "..." parameter). Pairing them
 *  two by two, as a first version did, let one quote earlier shift the
 *  pairs, and the escaped json was never decoded. A run between two json
 *  strings is decoded too: harmless, it only ever redacts more. Each
 *  run is looked at once (st->run_end), and runs share only their
 *  quotes: linear time at each level.
 *
 *  The levels are bounded (MAX_ESCAPE_LEVELS): a backslash at level N
 *  costs 2^N bytes when written as "\\", but only N+1 more bytes each
 *  with "\u005c", so the text alone does not bound them. Deeper than
 *  that, a run with a backslash is not written (its size and sha256).
 *  Return where the scan goes on, NULL to go on at `q` as before.
 ***************************************************************************/
PRIVATE void out_append_json_escaped(redact_scan_t *sc, const char *s, size_t len)
{
    const char *from = s;
    const char *end = s + len;
    for(const char *p = s; p < end; p++) {
        unsigned char c = (unsigned char)*p;
        if(c != '"' && c != '\\' && c >= 0x20) {
            continue;
        }
        out_append(sc, from, (size_t)(p - from));
        char esc[8];
        switch(c) {
            case '"':
                snprintf(esc, sizeof(esc), "\\\"");
                break;
            case '\\':
                snprintf(esc, sizeof(esc), "\\\\");
                break;
            case '\n':
                snprintf(esc, sizeof(esc), "\\n");
                break;
            case '\r':
                snprintf(esc, sizeof(esc), "\\r");
                break;
            case '\t':
                snprintf(esc, sizeof(esc), "\\t");
                break;
            default:
                snprintf(esc, sizeof(esc), "\\u%04x", c);
                break;
        }
        out_append(sc, esc, strlen(esc));
        from = p + 1;
    }
    out_append(sc, from, (size_t)(end - from));
}

PRIVATE const char *scan_escaped_run(redact_scan_t *sc, scan_state_t *st, const char *q, BOOL value_run)
{
    /*
     *  In a '...' region the run goes on to its quote beyond the region: the
     *  region ends at the first "'", and a json string can hold one
     *  (cfg='{"a":"{\"password\":\"it's S\"}"}': up to review 19 the run
     *  ended at "it", and the rest of the secret was written)
     */
    const char *hi = (st->depth > 0 && st->lo[-1] == '\'')? st->end: st->hi;
    BOOL has_backslash = FALSE;
    const char *e = q + 1;
    while(e < hi && *e != '"') {
        if(*e == '\\' && e + 1 < hi) {
            has_backslash = TRUE;
            e++;
        } else if(*e == '\\') {
            has_backslash = TRUE;
        }
        e++;
    }
    st->run_end = e;    // `hi` if never closed
    if(!has_backslash) {
        return NULL;
    }

    const char *v = q + 1;
    size_t len = (size_t)(e - v);
    char *redacted = NULL;
    cut_t cut = CUT_NONE;
    key_kind_t cut_kind = KEY_PLAIN;
    if(sc->ctx->escape_level >= MAX_ESCAPE_LEVELS) {
        redacted = not_scanned_text(v, len);
    } else {
        char *decoded = gbmem_malloc(len + 1);
        if(!decoded) {
            // Error already logged. Never write what cannot be judged
            redacted = gbmem_strdup("<not written: no memory to redact it>");
        } else {
            size_t n = json_unescape(v, len, decoded);
            BOOL value_is_secret = sc->ctx->value_is_secret;
            if(!value_is_secret && names_secret_attribute(decoded, n, NULL)) {
                sc->ctx->value_is_secret = TRUE;
            }
            sc->ctx->escape_level++;
            sc->ctx->cut = CUT_NONE;
            redacted = redact_text(decoded, n, sc->ctx);
            cut = sc->ctx->cut;
            cut_kind = sc->ctx->cut_kind;
            sc->ctx->cut = CUT_NONE;
            sc->ctx->escape_level--;
            sc->ctx->value_is_secret = value_is_secret;
            gbmem_free(decoded);
        }
    }
    if(!redacted) {
        /*
         *  Nothing to redact inside: the scan goes on in the run (and sees
         *  the key of a value cut by its end: it takes the value itself)
         */
        return NULL;
    }

    if(out_copy_upto(sc, v)) {
        out_append_json_escaped(sc, redacted, strlen(redacted));
        sc->from = e;
    }
    gbmem_free(redacted);
    forget_quotes(st);

    /*
     *  The run is written back redacted: the value that its end cut is
     *  taken here, from its closing quote
     */
    if(e >= st->end && sc->ctx->escape_level > 0 &&
            (cut == CUT_PARAM_WORD || cut == CUT_JSON_EMPTY)) {
        /*
         *  A run never closed: its end is the end of this text too, the
         *  level above goes on
         */
        sc->ctx->cut = cut;
        sc->ctx->cut_kind = cut_kind;
        return e;
    }
    switch(cut) {
        case CUT_PARAM_EMPTY:
            return redact_param_value(sc, st, cut_kind, e);
        case CUT_PARAM_WORD:
            /*
             *  The quote that ends the run of a "..." value ends the value
             *  when a blank follows it (command="... password='S'" n=1);
             *  a run begun by any other quote (a stray one) ends inside
             *  the word, blank or not (x" \ password='a'" S"), unless json
             *  goes on after it (the end of a json string)
             */
            if(value_run? !word_goes_on(e, st->end):
                    (e + 1 >= st->end || strchr(",:}]", e[1]))) {
                break;  // the run's quote ends the word too
            }
            /* Falls through */
        case CUT_PARAM_TAIL:
            {
                const char *last_quote;
                const char *t = word_end(e, st->end, &last_quote);
                out_replace(sc, e, t, REDACTED);
                if(t >= st->end && sc->ctx->escape_level > 0) {
                    sc->ctx->cut = CUT_PARAM_TAIL;
                    sc->ctx->cut_kind = cut_kind;
                }
                return t;
            }
        case CUT_JSON_EMPTY:
            return redact_json_value(sc, cut_kind, e, st->end, 0);
        case CUT_NONE:
        default:
            break;
    }
    return e;   // its closing quote begins the next run
}

/***************************************************************************
 *  Scan the bytes [begin, end) of the text: one pass (and one more over a
 *  quoted run with escapes, see scan_escaped_run())
 ***************************************************************************/
PRIVATE void scan_text(redact_scan_t *sc, const char *begin, const char *end)
{
    scan_state_t st;    // the regions are written before they are read: no memset
    st.end = end;
    st.lo = begin;
    st.hi = end;
    st.depth = 0;
    forget_quotes(&st);
    st.run_end = NULL;

    const char *p = begin;
    while(p < end) {
        while(st.depth > 0 && p >= st.hi) {
            /*
             *  The end of a quoted value (its closing quote): back to the
             *  region around it
             */
            st.depth--;
            st.lo = st.depth? st.region_begin[st.depth-1]: begin;
            st.hi = st.depth? st.region_end[st.depth-1]: end;
        }

        char c = *p;
        const char *next = NULL;
        if(c == '=') {
            next = scan_param(sc, &st, p);
        } else if(c == ':') {
            next = scan_json_member(sc, &st, p);
        } else if((c == 'b' || c == 'B') && st.hi - p > 1 && ascii_lower(p[1]) == 'e') {
            next = scan_bearer(sc, &st, p);
        } else if((c == 'b' || c == 'B') && st.hi - p > 1 && ascii_lower(p[1]) == 'a') {
            next = scan_basic(sc, &st, p);
        } else if(c == 'e' && st.hi - p > 2 && p[1] == 'y' && p[2] == 'J') {
            next = scan_jwt(sc, &st, p);
        } else if(c == '"' && (st.backslashes % 2) == 0 && (!st.run_end || p >= st.run_end)) {
            next = scan_escaped_run(sc, &st, p, FALSE);
        }
        if(next) {
            p = next;
            continue;
        }

        if(c == '"' && (st.backslashes % 2) == 0) {
            st.q_prev = st.q_last;
            st.q_last = p;
        } else if(c == '"') {
            st.eq_prev = st.eq_last;
            st.eq_prev_r = st.eq_last_r;
            st.eq_last = p;
            st.eq_last_r = st.backslashes;
        }
        st.backslashes = (c == '\\')? st.backslashes + 1: 0;
        p++;
    }
}

/***************************************************************************
 *  A copy of `text` with every content64 and secret replaced (see the
 *  header). NULL if there is nothing to replace (use the text as it is).
 *  Free the result with gbmem_free().
 ***************************************************************************/
PRIVATE char *not_scanned_text(const char *text, size_t len)
{
    char hex[SHA256_HEX_LEN + 1];
    if(sha256_hex(text, len, hex, sizeof(hex)) < 0) {
        snprintf(hex, sizeof(hex), "?");    // Error already logged
    }
    char bf[SHA256_HEX_LEN + 64];
    snprintf(bf, sizeof(bf), "<%zu bytes, not scanned, sha256:%s>", len, hex);
    return gbmem_strdup(bf);
}

PRIVATE char *redact_text(const char *text, size_t len, redact_ctx_t *ctx)
{
    if(len > ctx->budget) {
        ctx->budget = 0;
        return not_scanned_text(text, len);
    }
    ctx->budget -= len;

    redact_scan_t sc = {
        .text = text,
        .from = text,
        .ctx = ctx
    };
    scan_text(&sc, text, text + len);

    if(sc.broken) {
        gbmem_free(sc.out);
        // Error already logged. Never the text: it holds what is redacted
        return gbmem_strdup("<not written: the audit scan broke>");
    }
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
PRIVATE json_t *redacted_member(const char *key, json_t *value, redact_ctx_t *ctx)
{
    key_kind_t kind = key_kind(key, strlen(key), ctx);
    if(kind == KEY_SECRET) {
        return json_string(REDACTED);
    }
    if(kind == KEY_CONTENT64 && json_is_string(value)) {
        size_t len = json_string_length(value);
        char summary[SHA256_HEX_LEN + 128];
        if(len > ctx->budget) {
            ctx->budget = 0;
            char *s = redact_text(json_string_value(value), len, ctx);  // not scanned: its sha256
            json_t *jn = json_string(s? s: "");
            GBMEM_FREE(s);
            return jn;
        }
        ctx->budget -= len;
        content64_summary(json_string_value(value), len, ctx->tty, summary, sizeof(summary));
        return json_string(summary);
    }
    return redacted_copy(value, ctx);
}

PRIVATE json_t *redacted_copy(json_t *jn, redact_ctx_t *ctx)
{
    if(json_is_object(jn)) {
        /*
         *  A write-attr given as a json object ({"attribute": "api_key",
         *  "value": ...}): its `value` is a secret, at this level and below
         */
        BOOL value_is_secret = ctx->value_is_secret;
        if(!value_is_secret && kw_names_secret_attribute(jn)) {
            ctx->value_is_secret = TRUE;
        }
        json_t *jn_copy = json_object();
        const char *key;
        json_t *value;
        json_object_foreach(jn, key, value) {
            json_object_set_new(jn_copy, key, redacted_member(key, value, ctx));
        }
        ctx->value_is_secret = value_is_secret;
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
 *  The command text of the record: redacted, or beyond the budget its
 *  first word, size and sha256 only. NULL if the text is written as it
 *  is. Free with gbmem_free().
 ***************************************************************************/
PRIVATE char *record_command_text(const char *command, const char *verb, redact_ctx_t *ctx)
{
    size_t len = strlen(command);
    if(len > ctx->budget) {
        ctx->budget = 0;
        char hex[SHA256_HEX_LEN + 1];
        if(sha256_hex(command, len, hex, sizeof(hex)) < 0) {
            snprintf(hex, sizeof(hex), "?");    // Error already logged
        }
        char bf[NAME_MAX + SHA256_HEX_LEN + 64];
        snprintf(bf, sizeof(bf), "%s <%zu bytes, not scanned, sha256:%s>", verb, len, hex);
        return gbmem_strdup(bf);
    }
    return redact_text(command, len, ctx);
}

/***************************************************************************
 *  Build the audit record of a command whose word is `verb`
 ***************************************************************************/
PRIVATE json_t *record_build(
    const char *command,
    json_t *kw,
    const char *date,
    const sdata_desc_t *command_table,
    const char *verb
)
{
    char inner[NAME_MAX] = "";
    char inner_other_case[NAME_MAX] = "";
    BOOL inner_in_text = FALSE;
    BOOL has_other_case = FALSE;
    BOOL wrapper = is_wrapper(verb);
    size_t command_len = strlen(command);
    if(wrapper && command_len <= AUDIT_SCAN_BUDGET) {
        inner_command(
            command, kw, verb, command_table,
            inner, sizeof(inner), &inner_in_text,
            inner_other_case, sizeof(inner_other_case), &has_other_case
        );
    }

    redact_ctx_t ctx = {
        .tty = (strcmp(verb, TTY_WRITE) == 0 || strcmp(inner, TTY_WRITE) == 0 ||
            strcmp(inner_other_case, TTY_WRITE) == 0)? TRUE: FALSE,
        .value_is_secret = FALSE,
        .budget = AUDIT_SCAN_BUDGET
    };
    if(command_len <= AUDIT_SCAN_BUDGET) {
        ctx.value_is_secret = names_secret_attribute(command, command_len, kw);
    } else {
        ctx.value_is_secret = kw_names_secret_attribute(kw);
    }
    if(!ctx.value_is_secret && wrapper && json_is_object(kw)) {
        /*
         *  kw.command, and a key `command` in another case too (it does not
         *  run, but it is written: the redaction is never looser)
         */
        const char *key;
        json_t *jn_inner;
        json_object_foreach(kw, key, jn_inner) {
            if(strcasecmp(key, "command") != 0 || !json_is_string(jn_inner) ||
                    json_string_length(jn_inner) > AUDIT_SCAN_BUDGET) {
                continue;
            }
            if(names_secret_attribute(json_string_value(jn_inner), json_string_length(jn_inner), NULL)) {
                ctx.value_is_secret = TRUE;
                break;
            }
        }
    }

    char *command_redacted = record_command_text(command, verb, &ctx);
    const char *command_text = command_redacted? command_redacted: command;
    json_t *jn_record = NULL;

    if(command_is_read_only(command, kw, verb, wrapper, inner, has_other_case)) {
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

    GBMEM_FREE(command_redacted);
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
 *  Build the audit record of a command, see audit_record.h
 ***************************************************************************/
PUBLIC json_t *audit_record_build(
    const char *command,
    json_t *kw,         // not owned
    const char *date,
    const sdata_desc_t *command_table
)
{
    if(!command) {
        command = "";
    }
    if(!date) {
        date = "";
    }
    char verb[NAME_MAX];
    command_word(command, command_table, verb, sizeof(verb));
    return record_build(command, kw, date, command_table, verb);
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
 *  Console writes of a command whose word is `verb`: TRUE if it is a
 *  write-tty (its records are made here)
 ***************************************************************************/
PRIVATE BOOL tty_command(
    json_t *jn_bursts,
    const char *command,
    json_t *kw,
    const char *date,
    unsigned burst_seconds,
    const char *verb,
    json_t *jn_records
)
{
    tty_close_bursts(jn_bursts, FALSE, jn_records);

    if(strcmp(verb, CLOSE_CONSOLE) == 0) {
        tty_close_bursts(jn_bursts, TRUE, jn_records);
        return FALSE;   // close-console has its own record
    }
    if(strcmp(verb, TTY_WRITE) != 0) {
        return FALSE;
    }

    const char *text = (strlen(command) <= AUDIT_SCAN_BUDGET)? command: "";    // beyond: the kw only
    char *console = param_value(text, kw, "name");
    char *content64 = param_value(text, kw, CONTENT64_KEY);
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

/***************************************************************************
 *  Parameters of the public functions
 ***************************************************************************/
PRIVATE BOOL bursts_and_records_ok(json_t *jn_bursts, json_t *jn_records, const char *fn)
{
    if(!json_is_object(jn_bursts) || !json_is_array(jn_records)) {
        gobj_log_error(0, 0,
            "function",     "%s", fn,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "jn_bursts must be a dict and jn_records a list",
            NULL
        );
        return FALSE;
    }
    return TRUE;
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
    const sdata_desc_t *command_table,
    json_t *jn_records
)
{
    if(!bursts_and_records_ok(jn_bursts, jn_records, __FUNCTION__)) {
        return FALSE;
    }
    if(!command) {
        command = "";
    }
    if(!date) {
        date = "";
    }
    char verb[NAME_MAX];
    command_word(command, command_table, verb, sizeof(verb));
    return tty_command(jn_bursts, command, kw, date, burst_seconds, verb, jn_records);
}

/***************************************************************************
 *  The records of one command, see audit_record.h
 ***************************************************************************/
PUBLIC int audit_command_records(
    json_t *jn_bursts,
    const char *command,
    json_t *kw,
    const char *date,
    unsigned burst_seconds,
    const sdata_desc_t *command_table,
    json_t *jn_records
)
{
    if(!bursts_and_records_ok(jn_bursts, jn_records, __FUNCTION__)) {
        return -1;
    }
    if(!command) {
        command = "";
    }
    if(!date) {
        date = "";
    }
    char verb[NAME_MAX];
    command_word(command, command_table, verb, sizeof(verb));   // once for both
    if(tty_command(jn_bursts, command, kw, date, burst_seconds, verb, jn_records)) {
        return 0;
    }
    json_t *jn_record = record_build(command, kw, date, command_table, verb);
    if(!jn_record) {
        return -1;  // Error already logged
    }
    json_array_append_new(jn_records, jn_record);
    return 0;
}
