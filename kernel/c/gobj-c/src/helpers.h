/****************************************************************************
 *              helpers.h
 *
 *              Several helpers
 *
 *              Copyright (c) 2014,2023 Niyamaka.
 *              Copyright (c) 2024-2026, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <sys/types.h>
#include <regex.h>
#include <inttypes.h>
#include <time.h>
#include <sys/socket.h>

/*
 * Do not remove, include the list of dependencies
 */
#include "llhttp.h"
#include "00_security.h"
#include "replace_string.h"
#include "ansi_escape_codes.h"
#include "gtypes.h"
#include "dl_list.h"
#include "gbmem.h"
#include "gbuffer.h"
#include "glogger.h"


#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define INLINE_COMMENT "#^^"

/***********************************************************************
 *  A switch on a string: each case compares it, the first one that
 *  matches runs, and a case with no `break` (or `return`) goes on into
 *  the next one, as in a C switch. DEFAULTS runs when no case before it
 *  matched, or when one falls into it.

    SWITCHS(command) {
        CASES("start")
        CASES("run")
            gobj_start(gobj);           // "start" or "run", exactly
            break;

        ICASES("stop")
            gobj_stop(gobj);            // "stop", "STOP", "Stop"...
            break;

        CASES_RE("^list-", 0)
            list_things(command);       // anything starting with "list-"
            break;

        CASES_RE("^help$", REG_ICASE)
            print_help();               // regcomp(3) flags
            break;

        DEFAULTS
            unknown_command(command);
            break;
    } SWITCHS_END

 *  NOTHING IS ALLOCATED BY A SWITCHS, so leaving it from inside a case --
 *  a `return`, a `goto` -- is safe.
 *
 *  Up to 7.25.6 SWITCHS compiled a regex (".*") on ENTRY and only SWITCHS_END
 *  freed it, so every `return` inside a case lost glibc's compiled automaton,
 *  ~1 KB, outside gbmem (invisible to its audit and to cur_system_memory).
 *  29 of the 63 SWITCHS of the tree return from a case, several per message:
 *  C_MQIOGATE's send path lost ~1.1 KB per message and a gate_central grew to
 *  1 GB in 25 minutes. Now only CASES_RE touches a regex, and it frees it
 *  before the case body runs.
 *
 *  How it is built: the `do { ... } while(0)` is what a `break` leaves, and
 *  each case closes the block of the case before it and opens its own.
 *  switchs_taken_ says that a case was entered: from then on every case
 *  opens without comparing, which is the fall-through. A case that was
 *  entered and left with `break` never reaches the next one, so DEFAULTS
 *  needs no test: getting to it means nothing before it matched, or a
 *  case fell into it.
 ***********************************************************************/
#define SWITCHS(x) \
    { const char *switchs_text_ = (x); BOOL switchs_taken_ = FALSE; do {

#define CASES(x) \
    } if(switchs_taken_ || strcmp(switchs_text_, (x)) == 0) { switchs_taken_ = TRUE;

#define ICASES(x) \
    } if(switchs_taken_ || strcasecmp(switchs_text_, (x)) == 0) { switchs_taken_ = TRUE;

#define CASES_RE(x, flags) \
    } if(switchs_taken_ || str_match_regex(switchs_text_, (x), (flags))) { switchs_taken_ = TRUE;

#define DEFAULTS \
    } {

#define SWITCHS_END \
    } while(0); }

/*****************************************************************
 *     Structures
 *****************************************************************/
typedef struct {
    time_t start;
    time_t end;
} time_range_t;

/***************************************************************
 *  inline functions
 ***************************************************************/
static inline BOOL empty_string(const char *str)
{
    return (str && *str)?0:1;
}

/*
 *  How much content a json carries: elements of a container, 1 for a
 *  non-empty string, 0 for everything else (NULL, json null, scalars).
 *  It is the size half of empty_json(); use that one to ask a question.
 */
static inline size_t json_size(const json_t *jn)
{
    if(!jn) {
        return 0;
    }
    switch(json_typeof(jn)) {
    case JSON_ARRAY:
        return json_array_size(jn);
    case JSON_OBJECT:
        return json_object_size(jn);
    case JSON_STRING:
        return strlen(json_string_value(jn))?1:0;
    default:
        return 0;
    }
}

/*
 *  "There is nothing usable here": C NULL, json null, {} , [] , "" and any
 *  scalar. THIS is the guard to write over a json, never `if(!jn)` -- a
 *  DTP_JSON attr with a null default is json_null(), a valid pointer, so
 *  `if(!jn)` is dead code. See CLAUDE.md, "C API footguns".
 */
static inline BOOL empty_json(const json_t *jn)
{
    return json_size(jn)==0;
}

/*
 *  "There is no value here": C NULL or an explicit json null, and nothing
 *  else. Use it only where null and empty mean different things (a field
 *  written null on purpose vs a field never written); otherwise the
 *  question is empty_json().
 */
static inline BOOL json_absent(const json_t *jn)
{
    return (!jn || json_is_null(jn))?TRUE:FALSE;
}

/*****************************************************************
 *     Prototypes
 *****************************************************************/
/*------------------------------------*
 *  ### File System
 *------------------------------------*/
PUBLIC int newdir(const char *path, int xpermission);
PUBLIC int newfile(const char *path, int rpermission, BOOL overwrite);
PUBLIC int open_exclusive(const char *path, int flags, int rpermission);  // open exclusive

PUBLIC off_t filesize(const char *path);
PUBLIC off_t filesize2(int fd);

PUBLIC int lock_file(int fd);
PUBLIC int unlock_file(int fd);

PUBLIC BOOL is_regular_file(const char *path);
PUBLIC BOOL is_directory(const char *path);
PUBLIC off_t file_size(const char *path);
PUBLIC mode_t file_permission(const char *path);
PUBLIC BOOL file_exists(const char *directory, const char *filename);
PUBLIC BOOL subdir_exists(const char *directory, const char *subdir);
PUBLIC int file_remove(const char *directory, const char *filename);

PUBLIC int mkrdir(const char *path, int xpermission); // -1 logged, errno the cause
PUBLIC int rmrdir(const char *path);
PUBLIC int rmrcontentdir(const char *root_dir);

/*------------------------------------*
 *  ### Strings
 *------------------------------------*/
PUBLIC char *delete_right_char(char *s, char x);
PUBLIC char *delete_left_char(char *s, char x);
PUBLIC char *build_path(char *bf, size_t bfsize, ...);
PUBLIC char *get_last_segment(char *path);
PUBLIC char *pop_last_segment(char *path); // WARNING path modified

/**rst**
   Change ' by ".
   Useful for easier json representation in C strings,
   **BUT** you cannot use true '
**rst**/
PUBLIC char *helper_quote2doublequote(char *str);

/**rst**
   Change " by '.
   Useful for easier json representation in C strings,
   **BUT** you cannot use true "
**rst**/
PUBLIC char *helper_doublequote2quote(char *str);

/**rst**
    Return TRUE if all characters (not empty) are numbers
**rst**/
PUBLIC BOOL all_numbers(const char* s);
PUBLIC void nice_size(char* bf, size_t bfsize, uint64_t bytes, BOOL b1024);
PUBLIC void delete_right_blanks(char *s);
PUBLIC void delete_left_blanks(char *s);
PUBLIC void left_justify(char *s);
PUBLIC char *strntoupper(char* s, size_t n);
PUBLIC char *strntolower(char* s, size_t n);

/**rst**
    The bytes of `s` that fit in `max_bytes` WITHOUT cutting a UTF-8
    character: the precision to cap a string with, as `"%.*s"`. A cap by
    bytes (`"%.64s"`) can end in the middle of a character, and the json
    built with it is refused (json_sprintf() checks UTF-8) or logged broken.
**rst**/
PUBLIC int utf8_prefix_len(const char *s, int max_bytes);

/**rst**
    The COLUMNS a string takes on a terminal: one per UTF-8 character, not
    one per byte. `utf8_columns()` measures the whole string;
    `utf8_prefix_columns()` returns the bytes of the longest prefix that
    fits in `max_columns` without cutting a character, and writes in
    `columns` (if not NULL) how many columns those bytes take. A table cell
    is padded with the two answers:

        int cols;
        int bytes = utf8_prefix_columns(text, width, &cols);
        printf("%.*s%*s", bytes, text, width - cols, "");

    `"%-*.*s"` counts BYTES: each non-ASCII character of a cell ("·", "ñ")
    pulls every column after it one place to the left, and its cut can split
    a character. One column per character is right for the Latin text these
    tables carry; wide (CJK) and combining characters are not measured.
**rst**/
PUBLIC int utf8_columns(const char *s);
PUBLIC int utf8_prefix_columns(const char *s, int max_columns, int *columns);
PUBLIC char *translate_string(
    char *to,
    int tolen,
    const char *from,
    const char *mk_to,
    const char *mk_from
);
PUBLIC int change_char(char *s, char old_c, char new_c);

/**rst**
   Extract parameter: delimited by blanks (\b\t) or quotes ('' "")
   The string is modified (nulls inserted)!
**rst**/
PUBLIC char *get_parameter(char *s, char **save_ptr);

/**rst**
 *  Extract key=value or key='this value' parameter
 *  Return the value, the key in `key`
 *  The string is modified (nulls inserted)!
**rst**/
PUBLIC char *get_key_value_parameter(char *s, char **key, char **save_ptr);

/**rst**
 *  TRUE if `name` (of `len` bytes, not NUL-terminated) is the name of a
 *  secret: a password, a token, a key... by NAME, for what cannot be
 *  known by its declaration (SDF_SECRET): the free keys of a command that
 *  is forwarded (SDF_WILD_CMD), a config variable, an audit record.
 *  Any case. A name holds one of the parts "passw", "pwd", "passphrase",
 *  "secret", "token", "jwt", "bearer", "authorization", "cookie",
 *  "credential", "salt"; or one of "apikey", "sessionid", "sessionkey",
 *  "authdata" once '_', '-', '.' and blanks are taken out; or one of
 *  "pass", "pin", "otp" as a WHOLE segment, a run of letters and digits
 *  (db_pass, user-pin, sim_pin; not passive, bypass, ping, spinner, otpx);
 *  or "priv" and "key" both (private_key). But not a name with a segment
 *  (split by '_', '-', '.' and blanks) that names something ABOUT a
 *  credential (endpoint, url, domain, path, file, public, count, type,
 *  name, ...: token_endpoint, cookie_domain, jwt_public_keys, pin_count).
 *  "key" and "cert" alone are NOT secrets' names: a key is the key of a
 *  record of timeranger as often as a credential, and a certificate is
 *  public (ssl_certificate_key is a path). An attribute flagged SDF_SECRET
 *  must have a name it takes: gclass_create() refuses the gclass otherwise
 *  (not on the ESP32 port), and so does a command table whose required
 *  secret parameter is followed by another required one. A known effect,
 *  accepted: an ordinary name with a segment pass, pin or otp is a secret's
 *  too (gpio_pin, a `pass` that counts passes, first_pass, sum_sla_pass).
 *  Compared in ASCII, whatever the locale. The one list of the SDK; gobj-js
 *  has the same (its is_secret_name()).
**rst**/
PUBLIC BOOL is_secret_name(const char *name, size_t len);

/**rst**
 *  TRUE if `name` holds a part of a secret's name (the parts of
 *  is_secret_name()), WHATEVER other segment it has: token_endpoint,
 *  api_key_max and token_mode are TRUE here and FALSE for
 *  is_secret_name(). For a record that is kept and must never hold a
 *  secret (the agent's audit log): there a name about a credential
 *  redacted costs nothing, and one that holds a secret in clear does.
**rst**/
PUBLIC BOOL is_secret_name_any(const char *name, size_t len);

/**rst**
 *  A text (a command line) with the value of every "name=value" and every
 *  "name: value" (its name quoted or not: "password": x, bearer: tok)
 *  whose name is a secret's written as "********" (quoted or not; the
 *  "value" of a write-attr whose attribute names a secret too; a scheme and
 *  its credential whole: token=Bearer abc). The name of a "name=value" is
 *  the word before the '=', and also the run of name characters before it,
 *  as in a query string ("https://my-file-server.com/get?token=x&y=1": its
 *  word holds "file", a segment about a credential), and the name as a form
 *  writes it, a part of it in brackets or quotes, %XX decoded
 *  (user[password], user%5Bpassword%5D, t%6Fken); in the word, pass, pin
 *  and otp count only after its last '&', '?' or ';'. The password of the
 *  userinfo of a URL (scheme://user:password@host). A URL or a query
 *  %-encoded inside a value (next=%2Flogin%3Fpassword%3Dx) is read decoded
 *  too, twice at most. And, under any name
 *  or none, a JWT or a JWE ("eyJ...", base64url and a '.' of its own: a JWT
 *  cut by a cap too; "eyJhIjoxfQ." is a header, no JWT) and the credential
 *  after "Bearer " or "Basic " (any case) when it can be one:
 *  quoted, after the scheme doubled, or not a word of prose -- a digit, a
 *  symbol of its own, a byte that is no ASCII, its case mixed, 24
 *  characters or more; for Bearer 8 at least, for Basic a run of base64
 *  that decodes to a "user:password". "the bearer of this letter" is left
 *  as it is. Each rule judges the text as it came, and a run of bytes to
 *  mask is ONE "********". The rest as it is.
 *  A gbmem string to GBMEM_FREE, or NULL when there was nothing to mask.
**rst**/
PUBLIC char *mask_secrets_inline(const char *str);

/**rst**
 *  The bytes of `text` (its `len` bytes, not NUL-terminated) that
 *  mask_secrets_inline() masks, MARKED in `marks`: a map of (len + 7) / 8
 *  bytes, zeroed by the caller, bit i % 8 of byte i / 8 set for byte i.
 *  Marks are only ever set, so a caller can add its own. For a writer of
 *  its own (the agent's audit record) that must hide at least what
 *  mask_secrets_inline() hides, with the same rules. Return the bytes
 *  marked.
**rst**/
PUBLIC size_t mark_secrets_inline(const char *text, size_t len, uint8_t *marks);

/**rst**
 *  `text` (its first `len` bytes) as a log line shows it, CAPPED: masked as
 *  mask_secrets_inline() masks it, and THEN cut to bfsize - 1 bytes (a
 *  whole UTF-8 character) into `bf`. Capped first, a credential that the
 *  cap cut lost its shape and showed ("Bearer abcdefg" of "Bearer
 *  abcdefghijk"). The bytes judged are the cap and 256 more, a credential
 *  that reaches their end masked whatever its shape (a URL authority from
 *  its first ':', '@' or not); the "value" of a write-attr is judged by the
 *  whole text. Return strlen(bf).
**rst**/
PUBLIC size_t mask_secrets_inline_capped(char *bf, size_t bfsize, const char *text, size_t len);

/**rst**
 *  A json as a log or a trace may show it: at any depth, the value of a
 *  key with a secret's name is "********" (whatever its json type; not an
 *  absent one, a null or an empty string), so is the "value" of a dict
 *  whose "attribute" names a secret, and every string, under any key, is
 *  masked as mask_secrets_inline() (a JWT, a "Bearer x", a password=x or a
 *  "token": x in it). A key is judged and copied by all its bytes, a NUL
 *  among them. A key is judged as a NAME, never as a value: a key that is
 *  itself a credential (a dict keyed by tokens, {"eyJ...": 1}) is shown
 *  as it is. Return a NEW reference: a masked copy, or jn itself when
 *  there was nothing to mask. NULL for NULL.
**rst**/
PUBLIC json_t *json_mask_secrets(json_t *jn);

/**rst**
 *  json_mask_secrets() on a budget of `max_bytes` units (json_mask_secrets()
 *  has 4 MB): every node walked costs one unit, a key or a string its bytes
 *  too. Once the budget is spent the walk STOPS: one placeholder,
 *  "<not shown: too large to mask>", stands for the rest of each container
 *  being walked (a "<more>" key in a dict, a last item in a list), never in
 *  clear. A string is walked whole or not shown at all. For a dump of a
 *  few bytes of a kw from a peer: the work is bounded by the cap, not by
 *  what was sent.
**rst**/
PUBLIC json_t *json_mask_secrets_capped(json_t *jn, size_t max_bytes);

/**rst**
 *  The bytes of a traffic dump with the credentials that can be told
 *  soundly written as '*', IN PLACE, the length kept: the value of an HTTP
 *  Cookie, Set-Cookie, Authorization or Proxy-Authorization header at the
 *  start of a line (after the scheme for the last two), of a "name=value",
 *  a json "name": value (or 'name': value) and a name: value whose name is
 *  a secret's (is_secret_name(); a scheme and its credential whole; the
 *  name as a form writes it too: user%5Bpassword%5D=x, 'password'=x), the
 *  password of the userinfo of a URL (scheme://user:password@host) -- in a
 *  URL or a query %-encoded inside a value too, read decoded, twice at
 *  most --, and a JWT or the credential after "Bearer " or "Basic "
 *  wherever they are, as mask_secrets_inline() tells them -- at the END of
 *  the bytes whatever its shape, and a URL authority that reaches it from
 *  its first ':' ('@' or not, a port too): a dump is the first bytes of a
 *  frame, and its end may cut one.
 *  Each rule judges the bytes as they came.
 *  Work on a COPY of what you dump. Return the bytes masked.
**rst**/
PUBLIC size_t mask_secrets_in_text(char *bf, size_t len);

/**rst**
    Split a string by delim returning the list of strings.
    Return filling `list_size` if not null with items size,
    WARNING Remember free with split_free2().
    HACK: No, It does NOT include the empty strings!
**rst**/
PUBLIC const char **split2(const char *str, const char *delim, int *list_size);

/**rst**
    Compare two dotted versions, SEGMENT BY SEGMENT. Returns <0, 0 or >0,
    like strcmp.

    Both '.' and '-' separate, so a release and its revision order together
    ("1.9.0.0-2" is five segments), and a missing segment counts as 0, so
    "7.11" and "7.11.0" are the same version.

    WARNING do not "simplify" this into a single number. The agent used to
    weigh each segment by 1000 and accumulate: "1.9.0.0-2" needs 10^12, which
    overflowed an int and came out NEGATIVE, so the older release won every
    comparison and the agent promoted it back on every restart for eleven
    days on a client node. A wider accumulator only moves the ceiling -- and
    it still assumes every segment stays under 1000, so "1.2000.0" would
    outrank "2.0.0". Comparing segment by segment assumes neither, which is
    what the JS and Python sides of this project already do.
**rst**/
PUBLIC int version_cmp(const char *version1, const char *version2);
PUBLIC void split_free2(const char **list);

/**rst**
    Split string `str` by `delim` chars returning the list of strings.
    Return filling `list_size` if not null with items size,
    WARNING Remember free with split_free3().
    HACK: Yes, It does include the empty strings!
**rst**/
PUBLIC const char **split3(const char *str, const char *delim, int *plist_size);
PUBLIC void split_free3(const char **list);

/**rst**
    Concat two strings or three strings
    WARNING Remember free with str_concat_free().
**rst**/
PUBLIC char *str_concat(const char *str1, const char *str2);
PUBLIC char *str_concat3(const char *str1, const char *str2, const char *str3);
PUBLIC void str_concat_free(char *s);

/**rst**
    Return idx of str in string list.
    Return -1 if not exist
**rst**/
PUBLIC int idx_in_list(const char **list, const char *str, BOOL ignore_case);

/**rst**
    Return TRUE if str is in string list.
**rst**/
PUBLIC BOOL str_in_list(const char **list, const char *str, BOOL ignore_case);
/*
 *  Does `str` match the regular expression `pattern` (regcomp(3) `cflags`)?
 *  Compiles, matches and frees on each call: nothing is kept. A pattern that
 *  does not compile is logged and answers FALSE. It is what CASES_RE uses.
 */
PUBLIC BOOL str_match_regex(const char *str, const char *pattern, int cflags);

/*------------------------------------*
 *  json_config
 *------------------------------------*/
/**rst**

    PUBLIC char *json_config(
        BOOL print_verbose_config,
        BOOL print_final_config,
        const char *fixed_config,
        const char *variable_config,
        const char *config_json_file,
        const char *parameter_config,
        pe_flag_t quit
    );

   Return a malloc'ed string with the final configuration
   of joining the json format input string parameters.

   If there is an error in json format, the function will exit according
   to quit parameter and printing the error

   If ``print_verbose_config`` or ``print_final_config`` is TRUE then
   the function will print the result and will exit(0).

   The json string can contain one-line comments with combination: #^^

   The config is load in next order:

    - fixed_config          string, this config is NOT writable, usually yuno info in main.c
    - variable_config:      string, this config is writable, usually config in main.c
    - config_json_file:     file of file's list, overwriting variable_config, external to main.c
    - parameter_config:     string, overwriting variable_config, usually from command line

    The json string can contain one-line comments with combination: ##^

**rst**/
PUBLIC json_t *json_config(
    BOOL print_verbose_config,      // WARNING, if true will exit(0)
    BOOL print_final_config,        // WARNING, if true will exit(0)
    const char *fixed_config,
    const char *variable_config,
    const char *config_json_file,
    const char *parameter_config,
    pe_flag_t quit                  // What to do in case of error
);

/*------------------------------------*
 *  json_replace_vars
 *------------------------------------*/

/**rst**
    Main API: Replace (^^var^^) or
    custom-delimited variables in jn_dict using jn_vars
**rst**/
PUBLIC json_t *json_replace_var_custom(
    json_t *jn_dict, // owned
    json_t *jn_vars, // owned
    const char *open,
    const char *close
);

/**rst**
    Default version: uses (^^var^^)
**rst**/
PUBLIC json_t *json_replace_var(
    json_t *jn_dict, // owned
    json_t *jn_vars  // owned
);

/*------------------------------------*
 *  ### Json
 *------------------------------------*/

/*
 *  If exclusive then let file opened and return the fd, else close the file
 */
PUBLIC json_t *load_persistent_json(
    hgobj gobj,
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error,
    int *pfd,
    BOOL exclusive,
    BOOL silence  // HACK to silence TRUE you MUST set on_critical_error=LOG_NONE
);

PUBLIC json_t *load_json_from_file(
    hgobj gobj,
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error
);

PUBLIC int save_json_to_file(
    hgobj gobj,
    const char *directory,
    const char *filename,
    int xpermission,
    int rpermission,
    log_opt_t on_critical_error,
    BOOL create,        // Create file if not exists or overwrite.
    BOOL only_read,
    json_t *jn_data     // owned
);

/***************************************************************************
 *
 *  type can be:
 *      str|string,
 *      int|integer,
 *      real,
 *      bool|boolean,
 *      null,
 *      dict|object,
 *      list|array,
 *      time            if default is "now" set the current time
 *
 *  Example:

static json_desc_t jn_xxx_desc[] = {
    // Name         Type        Default
    {"string",      "str",      "",         "10"},  // First item is the pkey
    {"string2",     "str",      "Pepe",     "20"},
    {"integer",     "int",      "0660",     "8"},   // beginning with "0":octal,"x":hexa, others: integer
    {"boolean",     "bool",     "false",    "8"},
    {0}   // HACK important, final null
};
 ***************************************************************************/
typedef struct {
    const char *name;
    const char *type;   // type can be: "str", "int", "real", "bool", "null", "dict", "list", "time"
    const char *defaults;
    const char *fillspace;
} json_desc_t;

PUBLIC json_t *create_json_record(
    hgobj gobj,
    const json_desc_t *json_desc
);

/***************************************************************************
 *
 *  Convert a json record desc to a topic schema
 *
    json_desc
    {
        name: string
        type: string
        defaults: string
        fillspace: string
    }

    schema
    {
        id: string
        header: string
        type: string
        fillspace: integer
    }

 ***************************************************************************/
PUBLIC json_t *json_desc_to_schema(const json_desc_t *json_desc);

/***
 *  Utilities to manage strict ascendant string tables representing bit values of maximum 64 bits.
 *  Convert to json list of strings or a gbuffer with string1|string2|...
 *  The table must be end by NULL
 *  Example:
 *
    typedef enum { // HACK strict ascendant value!, strings in sample_flag_names
        case1       = 0x0001,
        case2       = 0x0002,
        case3       = 0x0004,
    } sample_flag_t;

    const char *sample_flag_names[] = { // Strings of sample_flag_t type
        "case1",
        "case2",
        "case3",
        0
    };

 */
PUBLIC json_t *bits2jn_strlist(
    const char **strings_table,
    uint64_t bits
);
PUBLIC gbuffer_t *bits2gbuffer(
    const char **strings_table,
    uint64_t bits
);

/**
 *  Convert strings
 *      by default separators are "|, "
 *          "s|s|s" or "s s s" or "s,s,s" or any combinations of them
 *  into bits according the string table
 *  The strings table must be end by NULL
*/
PUBLIC uint64_t strings2bits(
    const char **strings_table,
    const char *str,
    const char *separators
);

/**rst**
    Get a the idx of string value in a strings json list.
    Return -1 if not exist
**rst**/
PUBLIC int json_list_str_index(json_t *jn_list, const char *str, BOOL ignore_case);

/**rst**
    Get a integer value from an integers json list search by idx
**rst**/
PUBLIC json_int_t json_list_int(json_t *jn_list, size_t idx);

/**rst**
    Get the idx of integer value in a integers json list.
    Return -1 if not exist
**rst**/
PUBLIC int json_list_int_index(json_t *jn_list, json_int_t value);

/**rst**
    Find a json value in the list.
    Return index or -1 if not found or the index relative to 0.
    WARNING slow function
**rst**/
PUBLIC int json_list_find(json_t *list, json_t *value); // WARNING slow function

/**rst**
 *  Extend array values.
 *  If as_set_type is TRUE then not repeated values (WARNING slow function)
**rst**/
PUBLIC int json_list_update(json_t *list, json_t *other, BOOL as_set_type); // WARNING slow function

/**rst**
 *  Check if a list is a integer range:
 *      - must be a list of two integers (first <= second)
 *  Set first and second values in pfirst or psecond are not null
**rst**/
PUBLIC BOOL json_is_range(json_t *list, json_int_t *pfirst, json_int_t *psecond);

/**rst**
 *  Return a expanded integer range
 *  WARNING slow function, don't use in large ranges
**rst**/
PUBLIC json_t *json_range_list(json_t *list); // WARNING slow function, don't use in large ranges

/**rst**
 *  Build a list (set) with lists of integer ranges.
 *  [[#from, #to], [#from, #to], #integer, #integer, ...] -> list
 *  WARNING: Arrays of two integers are considered range of integers.
 *  Arrays of one or more of two integers are considered individual integers.
 *
 *  Return the json list
**rst**/
PUBLIC json_t *json_listsrange2set( // WARNING function TOO SLOW, use for short ranges
    json_t *listsrange // not owned
);

/**rst**
 *  Update keys and values, recursive through all objects
 *  If overwrite is FALSE then not update existing keys (protected write)
**rst**/
PUBLIC int json_dict_recursive_update(json_t *object, json_t *other, BOOL overwrite);

/**rst**
    Simple json to real
**rst**/
PUBLIC double jn2real(
    json_t *jn_var
);

/**rst**
    Simple json to int
**rst**/
PUBLIC json_int_t jn2integer(
    json_t *jn_var
);

/**rst**
    Simple json to string, WARNING free return with gbmem_free
**rst**/
PUBLIC char *jn2string(
    json_t *jn_var
);

/**rst**
    Simple json to boolean
**rst**/
PUBLIC BOOL jn2bool(
    json_t *jn_var
);

/**rst**
    Only compare str/int/real/bool items
    Complex types are done as matched
    Return lower, iqual, higher (-1, 0, 1), like strcmp
**rst**/
PUBLIC int cmp_two_simple_json(
    json_t *jn_var1,    // NOT owned
    json_t *jn_var2     // NOT owned
);
/**rst**
    Compare two json and return TRUE if they are identical.
**rst**/
PUBLIC BOOL json_is_identical(
    json_t *kw1,    // NOT owned
    json_t *kw2     // NOT owned
);

PUBLIC json_t *anystring2json(const char *bf, size_t len, BOOL verbose);
PUBLIC json_t *anyfile2json(const char *path, BOOL verbose);
PUBLIC json_t *string2json(const char *str, BOOL verbose); /* only [] or {}, old legalstring2json()*/
#define legalstring2json string2json
#define str2json string2json
#define nonlegalstring2json anystring2json

/*
 *  Convert a legal json string to json binary removing comments.
 */
PUBLIC json_t *json_config_string2json(const char *bf, BOOL verbose);

/**rst**
    A member of a json object as json_scan_object_members() finds it in
    the TEXT of the object: offsets into that text, nothing copied.
**rst**/
typedef struct {
    const char *key;        // in: the key of the member to find (keys must differ)
    int count;              // out: times the key is in the object, 0 if absent
    json_type type;         // out: type of its value (when count > 0)
    size_t value_offset;    // out: its value is text + value_offset, value_length bytes
    size_t value_length;
    size_t cut_offset;      // out: remove these bytes and the object is valid without it
    size_t cut_length;      //      (only when count is 1)
} json_member_span_t;

/**rst**
    Scan `text` (`len` bytes, no NUL needed): 0 if it is ONE json object
    that json_loadb(text, len, flags) accepts, -1 if not. `flags` is 0 or
    JSON_ALLOW_NUL. It builds nothing and allocates nothing.
    For each of `members` it says whether the object has that key at its
    top level, and where: of a repeated key, the LAST one, as jansson
    keeps it. With -1 the members are not to be read.
    Logs only a bad parameter: a text that is not json is an answer.
**rst**/
PUBLIC int json_scan_object_members(
    const char *text,
    size_t len,
    size_t flags,
    json_member_span_t *members,    // in/out, NULL when n_members is 0
    int n_members
);


/**rst**
    Set real precision (use in conversion of json to string functions)
    Return the previous precision
**rst**/
PUBLIC int set_real_precision(int precision);
PUBLIC int get_real_precision(void);


/**rst**
    Any json to indented string
    Remember gbmem_free the returned string
**rst**/
PUBLIC char *json2str(const json_t *jn); // jn not owned

/**rst**
    Any json to ugly (non-tabular) string
    Remember gbmem_free the returned string
**rst**/
PUBLIC char *json2uglystr(const json_t *jn); // jn not owned

/**rst**
    Check all refcounts
**rst**/
PUBLIC int json_check_refcounts(
    json_t *kw, // not owned
    int max_refcount,
    int *result // firstly initalize to 0
);

/**rst**
    What the refcounts of a json say, as a REPORT, logging nothing: the jsons
    held more than `max_refcount` times (0 or less: not asked) and the
    corrupted ones, with the kw path of the first `max_shown`. Corrupted: a
    type jansson does not have, a refcount of 0, or a refcount BELOW the
    truth -- a json reached by the walk more times than its refcount says it
    is held (a cycle whose refcounts are corrupted, a json set with
    json_object_set_new() in two places); its path is where it was reached
    once too many. The root is reached once by the caller, who holds it.
    Each json is walked ONCE, in a loop: a json held in several places (a
    node of a treedb is in its indexes and in the hooks of its parents) is
    reported once, and a cycle (two nodes in each other's hooks) does not
    loop the walk. Its memory: a set of the jsons met, 24 to 48 bytes per
    json, up to 72 while the set doubles, and a frame of 32 bytes per dict or
    list (a scalar takes none); in chunks, so no block grows with the json
    and MAX_BLOCK bounds none. Measured at its peak: 38 bytes per json for a
    list of a million integers, 45 for a dict of 125 000 records, 59 to 72
    at ten thousand jsons (the set just doubled). `truncated` is TRUE when
    the walk stopped short, with no memory left for what it remembers (the
    gbmem ERROR says why): the totals are then of what was walked. A NEW
    json, NULL if `jn` is NULL:

        {"max_refcount": 1, "jsons": 431, "corrupted": 0, "over_max": 27,
         "truncated": false,
         "shown": [{"path": "topics`users`...", "refcount": 2,
                    "fault": "over_max"}, ...]}
**rst**/
PUBLIC json_t *json_refcounts_report(
    json_t *jn, // not owned
    json_int_t max_refcount,
    int max_shown
);

/**rst**
    What is wrong with a json that C reads by its strings, as a short
    phrase, or NULL when nothing is: "a string with a NUL in it" (a json
    decoded with JSON_ALLOW_NUL, as an ievent frame is: C reads a string up
    to its first NUL, so "dx\u0000evil" is "dx"), "a key with a NUL in it",
    or "a json nested deeper than a parser builds" (JSON_PARSER_MAX_DEPTH
    levels: a cycle built with json_object_set() is one). At any depth;
    the json of a peer, which a parser built, is walked once.
**rst**/
PUBLIC const char *json_nul_fault(
    json_t *jn  // not owned
);

/**rst**
    Check deeply the refcount of kw
**rst**/
PUBLIC int json_print_refcounts(
    json_t *jn, // not owned
    int level
);

/**rst**
    Check if a string item are in `list` array:
**rst**/
PUBLIC BOOL json_str_in_list(hgobj gobj, json_t *jn_list, const char *str, BOOL ignore_case);


/*---------------------------------*
 *  ### Walkdir functions
 *---------------------------------*/
typedef enum {
    WD_RECURSIVE            = 0x0001,   /* traverse all tree */
    WD_HIDDENFILES          = 0x0002,   /* show hidden files too */
    WD_ONLY_NAMES           = 0x0004,   /* save only names (without full path) */
    WD_MATCH_DIRECTORY      = 0x0010,   /* if pattern is used on dir names */
    WD_MATCH_REGULAR_FILE   = 0x0020,   /* if pattern is used on regular files */
    WD_MATCH_PIPE           = 0x0040,   /* if pattern is used on pipes */
    WD_MATCH_SYMBOLIC_LINK  = 0x0080,   /* if pattern is used on symbolic links */
    WD_MATCH_SOCKET         = 0x0100,   /* if pattern is used on sockets */
} wd_option;

typedef enum {
    WD_TYPE_DIRECTORY = 1,
    WD_TYPE_REGULAR_FILE,
    WD_TYPE_PIPE,
    WD_TYPE_SYMBOLIC_LINK,
    WD_TYPE_SOCKET,
} wd_found_type;

typedef BOOL (*walkdir_cb)(
    hgobj gobj,
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[256]  filename
    int level,              // level of tree where the file is found
    wd_option opt           // option parameter
);
/*
 *  Walk directory tree calling callback with each file found.
 *  If the callback returns FALSE, the WHOLE walk stops, every level of it
 *  (0 is returned). `pattern` NULL matches every name.
 *  Return standard unix: 0 success, -1 fail (logged): the root cannot be
 *  opened, a directory of the tree cannot be read (readdir() fails), a
 *  SUBdirectory cannot be opened for a transient cause (EMFILE, ENFILE,
 *  ENOMEM, EIO), an entry cannot be stat'ed (other than EACCES, ENOENT),
 *  a path does not fit in PATH_MAX, or the tree is deeper than 1024 levels
 *  (16 on ESP32).
 *  A SUBdirectory that cannot be opened because of EACCES, ENOENT, ENOTDIR
 *  or ELOOP is skipped, with a warning.
 */
PUBLIC int walk_dir_tree(
    hgobj gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    walkdir_cb cb,
    void *user_data
);

/*
 *  find_files_with_suffix_array
Example of use:

    dir_array_t da;

    find_files_with_suffix_array(
        gobj,
        "/yuneta/store",
        ".json",
        &da
    );
    printf("Found %d files\n", da.count);

    dir_array_sort(&da);

    // Use the array
    for(int i=0; i<da.count; i++) {
        char *filename = da.items[i];
        //...
    }

    dir_array_free(&da);

 */
typedef struct dir_array_s {
    char    **items;
    json_int_t count;
    json_int_t capacity;
} dir_array_t;

/*
 *  find_files_with_suffix_array(), walk_dir_array(), get_ordered_filename_array():
 *  0 on success; -1 (logged) when the directory cannot be opened, cannot be
 *  read (readdir() fails: EIO, ESTALE), an entry cannot be kept (no
 *  memory), a path does not fit in PATH_MAX, or (a walk) a SUBdirectory
 *  cannot be opened for a transient cause (EMFILE, ENFILE, ENOMEM, EIO) or
 *  the tree is deeper than 1024 levels; and then 'da' is EMPTY: a listing
 *  that lost an entry is not the listing of the directory. A walk skips,
 *  with a warning, a SUBdirectory it cannot open because of EACCES, ENOENT,
 *  ENOTDIR or ELOOP. `re` NULL (walk_dir_array, get_ordered_filename_array)
 *  matches every name.
 */
PUBLIC int find_files_with_suffix_array( // Remember to free 'da' with dir_array_free()
    hgobj gobj,
    const char *directory,
    const char *suffix,
    dir_array_t *da
);

PUBLIC void dir_array_sort(
    dir_array_t *da
);

PUBLIC void dir_array_free(
    dir_array_t *da
);

PUBLIC int walk_dir_array( // Remember to free 'da' with dir_array_free()
    hgobj gobj,
    const char *root_dir,
    const char *re, // regular expression
    wd_option opt,
    dir_array_t *da
);

/*
 *  Get ordered full tree filenames of root_dir
Example of use:

    dir_array_t da;

    get_ordered_filename_array(
        gobj,
        "/yuneta/store",
        ".*\\.json",
        WD_MATCH_REGULAR_FILE|WD_ONLY_NAMES,
        &da
    );
    printf("Found %d files\n", da.count);

    // Use the array
    for(int i=0; i<da.count; i++) {
        char *filename = da.items[i];
        //...
    }

    dir_array_free(&da);

 */
PUBLIC int get_ordered_filename_array( // Remember to free 'da' with dir_array_free()
    hgobj gobj,
    const char *root_dir,
    const char *re, // regular expression
    wd_option opt,
    dir_array_t *da
);

/*---------------------------------*
 *  ### Time functions
 *---------------------------------*/
typedef uintmax_t timestamp_t;
#define PRItime PRIuMAX
#define parse_timestamp strtoumax
#define TIME_MAX UINTMAX_MAX

#define bitsizeof(x)  (CHAR_BIT * sizeof(x))

#define maximum_signed_value_of_type(a) \
    (INTMAX_MAX >> (bitsizeof(intmax_t) - bitsizeof(a)))

#define maximum_unsigned_value_of_type(a) \
    (UINTMAX_MAX >> (bitsizeof(uintmax_t) - bitsizeof(a)))

/*
 * Signed integer overflow is undefined in C, so here's a helper macro
 * to detect if the sum of two integers will overflow.
 *
 * Requires: a >= 0, typeof(a) equals typeof(b)
 */
#define signed_add_overflows(a, b) \
    ((b) > maximum_signed_value_of_type(a) - (a))

#define unsigned_add_overflows(a, b) \
    ((b) > maximum_unsigned_value_of_type(a) - (a))

/*
 * If the string "str" begins with the string found in "prefix", return 1.
 * The "out" parameter is set to "str + strlen(prefix)" (i.e., to the point in
 * the string right after the prefix).
 *
 * Otherwise, return 0 and leave "out" untouched.
 *
 * Examples:
 *
 *   [extract branch name, fail if not a branch]
 *   if (!skip_prefix(ref, "refs/heads/", &branch)
 *    return -1;
 *
 *   [skip prefix if present, otherwise use whole string]
 *   skip_prefix(name, "refs/heads/", &name);
 */
static inline int skip_prefix(const char *str, const char *prefix,
                              const char **out)
{
    do {
        if (!*prefix) {
            *out = str;
            return 1;
        }
    } while (*str++ == *prefix++);
    return 0;
}

struct date_mode {
    enum date_mode_type {
        DATE_NORMAL = 0,
        DATE_RELATIVE,
        DATE_SHORT,
        DATE_ISO8601,
        DATE_ISO8601_STRICT,
        DATE_RFC2822,
        DATE_STRFTIME,
        DATE_RAW,
        DATE_UNIX
    } type;
    char strftime_fmt[256];
    int local;
};

PUBLIC time_t tm_to_time_t(const struct tm *tm);

/*
 * Convenience helper for passing a constant type, like:
 *
 *   show_date(t, tz, DATE_MODE(NORMAL));
 */
#define DATE_MODE(t) date_mode_from_type(DATE_##t)
PUBLIC struct date_mode *date_mode_from_type(enum date_mode_type type);

PUBLIC const char *show_date(timestamp_t time, int timezone, const struct date_mode *mode);
PUBLIC void show_date_relative(
    timestamp_t time,
    char *timebuf,
    int timebufsize
);
PUBLIC int parse_date(
    const char *date,
    char *out,
    int outsize
);
PUBLIC int parse_date_basic(const char *date, timestamp_t *timestamp, int *offset);
PUBLIC int parse_expiry_date(const char *date, timestamp_t *timestamp);
PUBLIC void datestamp(
    char *out,
    int outsize
);

#define approxidate(s) approxidate_careful((s), NULL)
timestamp_t approxidate_careful(const char *, int *);
timestamp_t approxidate_relative(const char *date);
PUBLIC void parse_date_format(const char *format, struct date_mode *mode);
PUBLIC int date_overflows(timestamp_t date);

/*
Here’s a breakdown of the kinds of formats approxidate_careful() can interpret:
1. Absolute Dates (Standard formats):

    ISO 8601 format:
        YYYY-MM-DD (e.g., 2024-09-17)
        YYYY/MM/DD (e.g., 2024/09/17)
    Basic English date formats:
        Sep 17 2024
        17 Sep 2024
        September 17, 2024
    US-centric formats:
        09/17/2024
        9/17/2024
    Year-only:
        2024
    Month and year:
        Sep 2024

2. Relative Dates:

The function can interpret relative dates, which are based on the current date (or another reference point) and include phrases like:

    Days ago/forward:
        3 days ago
        2 days from now
    Weeks ago/forward:
        2 weeks ago
        1 week from now
    Months ago/forward:
        1 month ago
        in 3 months
    Years ago/forward:
        2 years ago
        next year
        in 5 years
    Human-readable terms:
        yesterday
        today
        tomorrow
    Named days of the week:
        Monday
        last Monday
        next Friday
        this Tuesday

3. Special Time Keywords:

It can handle specific keywords associated with time, such as:

    Noon:
        noon today
    Midnight:
        midnight
    Time of day:
        3:45 PM
        10:30 AM

4. Combinations of Date and Time:

    yesterday 3:00 PM
    next Friday at 12:30
    2024-09-17 10:45

5. Time-relative to specific date:

Approxidate can also parse relative times or dates anchored to a specific time:

    3 days after 2024-09-17
    2 weeks before next Monday

6. Shorthand Dates:

    Today’s shorthand:
        0 (interpreted as today)
    Days ago shorthand:
        -1 (interpreted as yesterday)
        +2 (interpreted as two days from today)

7. Special Cases and Contextual Phrases:

It might also interpret more casual terms such as:

    the day before yesterday
    the day after tomorrow

Important Considerations:

    Contextual Parsing: approxidate_careful() will interpret relative terms (like tomorrow or next week) based on the current date or a reference date.
    Locale-specific parsing: It is likely designed to handle common date formats used in different regions, including differences between US-centric formats (MM/DD/YYYY) and European formats (DD/MM/YYYY).

Error Handling:

    Ambiguities: When presented with ambiguous formats or incomplete information, approxidate_careful() tries to interpret the most likely date based on context, hence the "careful" part of its functionality.
 */

/*---------------------------------*
 *  ### Utilities functions
 *---------------------------------*/
typedef int (*view_fn_t)(const char *format, ...)JANSSON_ATTRS((format(printf, 1, 2)));
PUBLIC char *hex2bin(char *bf, int bfsize, const char *hex, size_t hex_len, size_t *out_len); // return bf
PUBLIC char *bin2hex(char *bf, int bfsize, const uint8_t *bin, size_t bin_len); // return bf
PUBLIC void tdump(const char *prefix, const uint8_t *s, size_t len, view_fn_t view, int nivel);
PUBLIC json_t *tdump2json(const uint8_t *s, size_t len);
PUBLIC int print_json(const char *label, json_t *jn);
PUBLIC int debug_json(const char *label, json_t *jn, BOOL verbose);
PUBLIC int debug_json2(json_t *jn, const char *format, ...)JANSSON_ATTRS((format(printf, 2, 3)));
PUBLIC char *current_timestamp(char *bf, size_t bfsize); // `bf` must be 90 bytes minimum
PUBLIC char *tm2timestamp(char *bf, int bfsize, struct tm *tm);
PUBLIC char *t2timestamp(char *bf, int bfsize, time_t t, BOOL local);
PUBLIC time_t start_sectimer(time_t seconds);   /* value <=0 will disable the timer */
PUBLIC BOOL   test_sectimer(time_t value);      /* Return TRUE if timer has finish */
PUBLIC uint64_t start_msectimer(uint64_t milliseconds);   /* value <=0 will disable the timer */
PUBLIC BOOL   test_msectimer(uint64_t value);           /* Return TRUE if timer has finish */
PUBLIC uint64_t time_in_milliseconds_monotonic(void); // Return MONOTONIC time in milliseconds
PUBLIC uint64_t time_in_milliseconds(void);   // Return current **real** time in milliseconds
PUBLIC time_t time_in_seconds(void);       // Return current time in seconds (standard time(&t))

PUBLIC uint64_t cpu_usage(void);
PUBLIC double cpu_usage_percent(
    uint64_t *last_cpu_ticks,
    uint64_t *last_ms
);

PUBLIC uint64_t htonll(uint64_t value); /* Convert a 64-bit integer to network byte order*/
PUBLIC uint64_t ntohll(uint64_t value); /* Convert a 64-bit integer to host byte order */
PUBLIC void list_open_files(void);

PUBLIC time_t gmtime2timezone(time_t t, const char *tz, struct tm *ltm, time_t *offset);
PUBLIC char *formatdate(time_t t, char *bf, int bfsize, const char *format);

/**rst**
   Cuenta cuantos caracteres de 'c' hay en 's'
**rst**/
PUBLIC int count_char(const char *s, char c);

PUBLIC const char *get_hostname(void);

// Create a new random uuid, bfsize must be >=37 bytes (used by treedb)
PUBLIC int create_random_uuid(char *bf, int bfsize);

PUBLIC const char *node_uuid(void); // Get the uuid of the machine

PUBLIC BOOL is_metadata_key(const char *key); // Metadata key (variable) has a prefix of 2 underscore
PUBLIC BOOL is_private_key(const char *key); // Private key (variable) has a prefix of 1 underscore

/*------------------------------------*
 *  ### Common protocols
 *------------------------------------*/
/**rst**
   Register a gclass with a communication protocol
**rst**/
PUBLIC int comm_prot_register(gclass_name_t gclass_name, const char *schema);

/**rst**
   Get the gclass name implementing the schema
**rst**/
PUBLIC gclass_name_t comm_prot_get_gclass(const char *schema);

/**rst**
   Free comm_prot register
**rst**/
PUBLIC void comm_prot_free(void);

#ifdef __cplusplus
}
#endif

/*------------------------------------*
 *  ### Daemons
 *------------------------------------*/
PUBLIC int launch_daemon( // Return pid of daemon
    BOOL redirect_stdio_to_null,
    const char *program,
    ...
);

/*------------------------------------*
 *  ### Parse url
 *------------------------------------*/
PUBLIC int parse_url(
    hgobj gobj,
    const char *uri,
    char *schema, size_t schema_size,
    char *host, size_t host_size,
    char *port, size_t port_size,
    char *path, size_t path_size,
    char *query, size_t query_size,
    BOOL no_schema // only host:port
);

/**rst**
   Get the schema of an url
**rst**/
PUBLIC int get_url_schema(
    hgobj gobj,
    const char *uri,
    char *schema, size_t schema_size
);

/*------------------------------------*
 *  ### Debug
 *------------------------------------*/
int init_backtrace_with_backtrace(
    const char *program
);
void show_backtrace_with_backtrace(
    loghandler_fwrite_fn_t fwrite_fn,
    void *h
);

PUBLIC unsigned long free_ram_in_kb(void);
PUBLIC unsigned long total_ram_in_kb(void);

/*
 * Read the command line of a process into a caller-provided buffer.
 *
 * @param bf        Buffer to store the command line
 * @param bfsize    Size of the buffer in bytes (maximum cmd line is ARG_MAX)
 * @param pid       Process ID (use 0 for current process)
 * @return          0 on success, -1 on error (errno set)
 */
PUBLIC int read_process_cmdline(char *bf, size_t bfsize, pid_t pid);

PUBLIC int copyfile(
    const char* source,
    const char* destination,
    int permission,
    BOOL overwrite
);

PUBLIC int set_nonblocking(int fd);
PUBLIC int set_cloexec(int fd); // children must not inherit
PUBLIC char *upper(char *s);
PUBLIC char *lower(char *s);
PUBLIC char *capitalize(char *s);

/*
 *  Set TCP_NODELAY, SO_KEEPALIVE and SO_LINGER options to socket
 */
PUBLIC int set_tcp_socket_options(int fd, int delay); // Set internally in tcp sockets (client and clisrv)

PUBLIC BOOL is_tcp_socket(int fd);
PUBLIC BOOL is_udp_socket(int fd);
PUBLIC int print_socket_address(char *buf, size_t buflen, const struct sockaddr *sa);
PUBLIC int get_peername(char *bf, size_t bfsize, int fd);
PUBLIC int get_sockname(char *bf, size_t bfsize, int fd);

PUBLIC int check_open_fds(void); // return # opened fd's
PUBLIC int print_open_fds(const char *fmt, ...);
PUBLIC int get_inotify_self_usage(int *instances, int *watches); // this process; 0 ok, -1 error
PUBLIC int is_yuneta_user(const char *username);

#include <pwd.h>
#include <grp.h>
/*
 * Portable user/group lookup wrappers.
 * In CONFIG_FULLY_STATIC builds these read /etc/passwd and /etc/group
 * directly (no glibc NSS / dlopen). In dynamic builds they delegate to
 * the standard glibc functions.
 */
PUBLIC struct passwd *yuneta_getpwuid(uid_t uid);
PUBLIC struct passwd *yuneta_getpwnam(const char *name);
PUBLIC struct group *yuneta_getgrnam(const char *name);
PUBLIC int yuneta_getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups);

PUBLIC const char *path_basename(const char *path);
PUBLIC const char *get_yunetas_base(void);
PUBLIC gbuffer_t *source2base64_for_yunetas(
    const char *source,
    char *comment,
    int commentlen
);
PUBLIC gbuffer_t *replace_cli_vars(
    const char *command,
    char *comment,
    int commentlen
);

typedef struct { // WARNING the table must end with {0,0}
    int number;
    char *name;
} number_name_table_t;

PUBLIC int get_number_from_nn_table(const number_name_table_t *table, const char *name);
PUBLIC const char *get_name_from_nn_table(const number_name_table_t *table, int number);

/**rst**
    Return in gmt time range in hours of time t, TZ optional
**rst**/
PUBLIC time_range_t get_hours_range(time_t t, int range, const char *TZ);

/**rst**
    Return in gmt time range in days of time t, TZ optional
**rst**/
PUBLIC time_range_t get_days_range(time_t t, int range, const char *TZ);

/**rst**
    Return in gmt time range in weeks of time t, TZ optional
**rst**/
PUBLIC time_range_t get_weeks_range(time_t t, int range, const char *TZ);

/**rst**
    Return in gmt time range in month of time t, TZ optional
**rst**/
PUBLIC time_range_t get_months_range(time_t t, int range, const char *TZ);

/**rst**
    Return in gmt time range in year of time t, TZ optional
**rst**/
PUBLIC time_range_t get_years_range(time_t t, int range, const char *TZ);

/*------------------------------------*
 *  ### Hashing
 *------------------------------------*/
/**rst**
    sha256 of a buffer, standalone (FIPS 180-4), no TLS backend involved.
    `sha256_hex()` writes the 64 lowercase hex chars plus the nul; `bflen`
    must be at least SHA256_HEX_LEN + 1. Return 0 or -1 (error logged).
**rst**/
#define SHA256_DIGEST_LEN   32
#define SHA256_HEX_LEN      64
PUBLIC void sha256_digest(const void *data, size_t len, uint8_t digest[SHA256_DIGEST_LEN]);
PUBLIC int sha256_hex(const void *data, size_t len, char *bf, size_t bflen);
