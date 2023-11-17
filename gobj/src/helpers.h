/****************************************************************************
 *              helpers.h
 *
 *              Several helpers
 *
 *              Copyright (c) 2014,2023 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gobj.h"

/*
 *  Dependencies
 */


#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/

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
PUBLIC BOOL file_exists(const char *directory, const char *filename);
PUBLIC BOOL subdir_exists(const char *directory, const char *subdir);
PUBLIC int file_remove(const char *directory, const char *filename);

PUBLIC int mkrdir(const char *path, int index, int permission);
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
    {"string",      "str",      ""},
    {"string2",     "str",      "Pepe"},
    {"integer",     "int",      "0660"},     // beginning with "0":octal,"x":hexa, others: integer
    {"boolean",     "bool",     "false"},
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
    Return idx of str in string list.
    Return -1 if not exist
**rst**/
PUBLIC int idx_in_list(const char **list, const char *str, BOOL ignore_case);

/*---------------------------------*
 *      Utilities functions
 *---------------------------------*/
typedef int (*view_fn_t)(const char *format, ...);;
PUBLIC void tdump(const char *prefix, const uint8_t *s, size_t len, view_fn_t view, int nivel);
PUBLIC json_t *tdump2json(const uint8_t *s, size_t len);
PUBLIC int print_json2(const char *label, json_t *jn);
PUBLIC char *current_timestamp(char *bf, size_t bfsize);
PUBLIC time_t start_sectimer(time_t seconds);   /* value <=0 will disable the timer */
PUBLIC BOOL   test_sectimer(time_t value);      /* Return TRUE if timer has finish */
PUBLIC uint64_t start_msectimer(uint64_t miliseconds);   /* value <=0 will disable the timer */
PUBLIC BOOL   test_msectimer(uint64_t value);           /* Return TRUE if timer has finish */
PUBLIC uint64_t time_in_miliseconds(void);   // Return current time in miliseconds
PUBLIC uint64_t time_in_seconds(void);       // Return current time in seconds (standart time(&t))

PUBLIC char *helper_quote2doublequote(char *str);
PUBLIC char *helper_doublequote2quote(char *str);
PUBLIC json_t * anystring2json(const char *bf, size_t len, BOOL verbose);
PUBLIC void nice_size(char* bf, size_t bfsize, uint64_t bytes);
PUBLIC void nice_size2(char *bf, size_t bfsize, size_t bytes, BOOL si); // si ? 1000 : 1024
PUBLIC void delete_right_blanks(char *s);
PUBLIC void delete_left_blanks(char *s);
PUBLIC void left_justify(char *s);
PUBLIC char *strntoupper(char* s, size_t n);
PUBLIC char *strntolower(char* s, size_t n);
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
    Split a string by delim returning the list of strings.
    Return filling `list_size` if not null with items size,
        It MUST be initialized to 0 (no limit) or to maximum items wanted.
    WARNING Remember free with split_free2().
    HACK: No, It does NOT include the empty strings!
**rst**/
PUBLIC const char ** split2(const char *str, const char *delim, int *list_size);
PUBLIC void split_free2(const char **list);

/**rst**
    Split string `str` by `delim` chars returning the list of strings.
    Return filling `list_size` if not null with items size,
        It MUST be initialized to 0 (no limit) or to maximum items wanted.
    WARNING Remember free with split_free3().
    HACK: Yes, It does include the empty strings!
**rst**/
PUBLIC const char **split3(const char *str, const char *delim, int *plist_size);
PUBLIC void split_free3(const char **list);

/**rst**
    Get a the idx of string value in a strings json list.
    Return -1 if not exist
**rst**/
PUBLIC int json_list_str_index(json_t *jn_list, const char *str, BOOL ignore_case);

#ifdef __cplusplus
}
#endif
