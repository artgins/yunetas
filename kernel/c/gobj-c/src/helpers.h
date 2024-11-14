/****************************************************************************
 *              helpers.h
 *
 *              Several helpers
 *
 *              Copyright (c) 2014,2023 Niyamaka.
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <sys/stat.h>
#include "gobj.h"

/*
 *  Dependencies
 */


#ifdef __cplusplus
extern "C"{
#endif

/*****************************************************************
 *     Prototypes
 *****************************************************************/
/*------------------------------------*
 *      Directory/Files
 *------------------------------------*/
PUBLIC int newdir(const char *path, int permission);
PUBLIC int newfile(const char *path, int permission, BOOL overwrite);
PUBLIC int open_exclusive(const char *path, int flags, int permission);  // open exclusive

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

PUBLIC int mkrdir(const char *path, int permission);
PUBLIC int rmrdir(const char *root_dir);
PUBLIC int rmrcontentdir(const char *root_dir);

/*------------------------------------*
 *          Strings
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
PUBLIC void nice_size(char* bf, size_t bfsize, uint64_t bytes);
PUBLIC void nice_size2(char *bf, size_t bfsize, size_t bytes, BOOL si); // si ? 1000 : 1024
PUBLIC void delete_right_blanks(char *s);
PUBLIC void delete_left_blanks(char *s);
PUBLIC void left_justify(char *s);
PUBLIC char *strntoupper(char* s, size_t n);
PUBLIC char *strntolower(char* s, size_t n);
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
    Split a string by delim returning the list of strings.
    Return filling `list_size` if not null with items size,
    WARNING Remember free with split_free2().
    HACK: No, It does NOT include the empty strings!
**rst**/
PUBLIC const char ** split2(const char *str, const char *delim, int *list_size);
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
PUBLIC char * str_concat(const char *str1, const char *str2);
PUBLIC char * str_concat3(const char *str1, const char *str2, const char *str3);
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

/*------------------------------------*
 *          Json
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
 *  str|string, int|integer, real, bool|boolean, null, dict|object, list|array
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
    const char *type;   // type can be: "str", "int", "real", "bool", "null", "dict", "list"
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
    json_record
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
PUBLIC json_t *json_record_to_schema(const json_desc_t *json_desc);

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

PUBLIC json_t * anystring2json(const char *bf, size_t len, BOOL verbose);
PUBLIC json_t * string2json(const char* str, BOOL verbose); /* only [] or {}, old legalstring2json()*/
#define legalstring2json string2json
#define str2json string2json

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

/*---------------------------------*
 *      Walkdir functions
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
 *  Walk directory tree calling callback witch each file found.
 *  If the callback returns FALSE, then stop traversing the tree.
 *  Return standard unix: 0 success, -1 fail
 */
PUBLIC int walk_dir_tree(
    hgobj gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    walkdir_cb cb,
    void *user_data
);

PUBLIC int get_number_of_files(
    hgobj gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt
);

/*
 * Return the ordered full tree filenames of root_dir
 * WARNING free the returned value (char **) with free_ordered_filename_array()
 * NOTICE: Sometimes I reinvent the wheel: use glob() please.
 */
PUBLIC char **get_ordered_filename_array(
    hgobj gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    int *size
);
PUBLIC void free_ordered_filename_array(char **array, int size);

/*---------------------------------*
 *      Time functions
 *---------------------------------*/
typedef uintmax_t timestamp_t;
#define PRItime PRIuMAX
#define parse_timestamp strtoumax
#define TIME_MAX UINTMAX_MAX

/*
 * ARRAY_SIZE - get the number of elements in a visible array
 *  <at> x: the array whose size you want.
 *
 * This does not work on pointers, or arrays declared as [], or
 * function parameters.  With correct compiler support, such usage
 * will cause a build error (see the build_assert_or_zero macro).
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

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

time_t tm_to_time_t(const struct tm *tm);

/*
 * Convenience helper for passing a constant type, like:
 *
 *   show_date(t, tz, DATE_MODE(NORMAL));
 */
#define DATE_MODE(t) date_mode_from_type(DATE_##t)
struct date_mode *date_mode_from_type(enum date_mode_type type);

const char *show_date(timestamp_t time, int timezone, const struct date_mode *mode);
void show_date_relative(
    timestamp_t time,
    char *timebuf,
    int timebufsize
);
int parse_date(
    const char *date,
    char *out,
    int outsize
);
int parse_date_basic(const char *date, timestamp_t *timestamp, int *offset);
int parse_expiry_date(const char *date, timestamp_t *timestamp);
void datestamp(
    char *out,
    int outsize
);

#define approxidate(s) approxidate_careful((s), NULL)
timestamp_t approxidate_careful(const char *, int *);
timestamp_t approxidate_relative(const char *date);
void parse_date_format(const char *format, struct date_mode *mode);
int date_overflows(timestamp_t date);

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
 *      Utilities functions
 *---------------------------------*/
typedef int (*view_fn_t)(const char *format, ...)JANSSON_ATTRS((format(printf, 1, 2)));
PUBLIC void tdump(const char *prefix, const uint8_t *s, size_t len, view_fn_t view, int nivel);
PUBLIC json_t *tdump2json(const uint8_t *s, size_t len);
PUBLIC int print_json2(const char *label, json_t *jn);
PUBLIC int debug_json(const char *label, json_t *jn, BOOL verbose);
PUBLIC char *current_timestamp(char *bf, size_t bfsize); // `bf` must be 90 bytes minimum
PUBLIC char *tm2timestamp(char *bf, int bfsize, struct tm *tm);
PUBLIC char *t2timestamp(char *bf, int bfsize, time_t t, BOOL local);
PUBLIC time_t start_sectimer(time_t seconds);   /* value <=0 will disable the timer */
PUBLIC BOOL   test_sectimer(time_t value);      /* Return TRUE if timer has finish */
PUBLIC uint64_t start_msectimer(uint64_t miliseconds);   /* value <=0 will disable the timer */
PUBLIC BOOL   test_msectimer(uint64_t value);           /* Return TRUE if timer has finish */
PUBLIC uint64_t time_in_miliseconds(void);   // Return current time in miliseconds
PUBLIC uint64_t time_in_seconds(void);       // Return current time in seconds (standart time(&t))
PUBLIC uint64_t htonll(uint64_t value); /* Convert a 64-bit integer to network byte order*/
PUBLIC uint64_t ntohll(uint64_t value); /* Convert a 64-bit integer to host byte order */
PUBLIC void list_open_files(void);

PUBLIC char *formatdate(time_t t, char *bf, int bfsize, const char *format);

/**rst**
    Check all refcounts
**rst**/
PUBLIC int json_check_refcounts(
    json_t *kw, // not owned
    int max_refcount,
    int *result // firstly initalize to 0
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

/**rst**
   Cuenta cuantos caracteres de 'c' hay en 's'
**rst**/
PUBLIC int count_char(const char *s, char c);

PUBLIC int create_uuid(char *bf, int bfsize);

#ifdef __cplusplus
}
#endif
