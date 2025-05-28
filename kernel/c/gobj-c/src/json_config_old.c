/***********************************************************************
 *          JSON_CONFIG.C
 *
 *          Json configuration.
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <jansson.h>
#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "helpers.h"

// From jannson_private.h
//extern void* jsonp_malloc(size_t size);
extern void jsonp_free(void *ptr);
extern char *jsonp_strdup(const char *str);
//extern char *jsonp_strndup(const char *str, size_t len);


/***************************************************************************
 *              Constants
 ***************************************************************************/
#define INLINE_COMMENT "#^^"
#define PRINT_JSON(comment, jn) printf("%s===>\n%s\n", (comment), json_dumps((jn), JSON_INDENT(4)))

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef enum {
    OP_FILE = 1,
    OP_STR,
} OP;

typedef struct op_s {
    DL_ITEM_FIELDS
    OP op;
    char *lines;
    FILE *file;
} op_t;


/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int load_files(
    dl_list_t *dl_op,
    json_t *jn_variable_config,
    const char *json_file,
    const char *json_file_type,
    int print_verbose_config,
    int print_final_config,
    pe_flag_t quit
);
PRIVATE json_t *x_legalstring2json(dl_list_t *dl_op, char *reference, const char *bf, pe_flag_t quit);
PRIVATE json_t *load_json_file(dl_list_t *dl_op, const char *path, pe_flag_t quit);
PRIVATE int expand_dict(json_t *jn_dict, pe_flag_t quit);
PRIVATE int expand_list(json_t *jn_list, pe_flag_t quit);
PRIVATE char *replace_vars(json_t *jn_dict, json_t *jn_vars, pe_flag_t quit);
PRIVATE json_t *nonx_legalstring2json(const char *reference, const char *bf, pe_flag_t quit);
PRIVATE int json_dict_recursive_update(json_t *object, json_t *other, BOOL overwrite, pe_flag_t quit);
PRIVATE int json_list_find(json_t *list, json_t *value);
PRIVATE int json_list_update(json_t* list, json_t* other, BOOL as_set, pe_flag_t quit);
PRIVATE json_t *json_listsrange2set(json_t *listsrange, pe_flag_t quit);
PRIVATE BOOL json_is_range(json_t *list);
PRIVATE json_t *json_range_list(json_t *list);


/***************************************************************************
 *  Load json config from a file '' or from files ['','']
 ***************************************************************************/
PRIVATE int load_files(
    dl_list_t *dl_op,
    json_t *jn_variable_config,
    const char *json_file,
    const char *json_file_type,
    int print_verbose_config,
    int print_final_config,
    pe_flag_t quit)
{
    char _json_file[1024];
    if(*json_file != '"' && *json_file != '[') {
        /*
         *  Consider a normal argp parameter as string
         */
        snprintf(_json_file, sizeof(_json_file), "\"%s\"", json_file);
        json_file = _json_file;
    }

    size_t flags = JSON_INDENT(4);//|JSON_SORT_KEYS;
    json_t * jn_files = nonx_legalstring2json(
        json_file_type,
        json_file,
        quit
    );
    if(json_is_string(jn_files)) {
        /*
            *  Only one file
            */
        json_t *jn_file = load_json_file(
            dl_op,
            json_string_value(jn_files),
            quit
        );
        if(print_verbose_config) {
            printf("%s '%s' ===>\n", json_file_type, json_string_value(jn_files));
            json_dumpf(jn_file, stdout, flags);
            printf("\n");
        }
        json_dict_recursive_update(jn_variable_config, jn_file, true, quit);
        json_decref(jn_file);

    } else if(json_is_array(jn_files)) {
        /*
            *  List of files
            */
        size_t index;
        json_t *value;
        json_array_foreach(jn_files, index, value) {
            if(json_is_string(value)) {
                json_t *jn_file = load_json_file(
                    dl_op,
                    json_string_value(value),
                    quit
                );
                if(print_verbose_config) {
                    printf("%s '%s' ===>\n", json_file_type, json_string_value(value));
                    json_dumpf(jn_file, stdout, flags);
                    printf("\n");
                }
                json_dict_recursive_update(jn_variable_config, jn_file, true, quit);
                json_decref(jn_file);
            }
        }
    } else {
        /*
         *  Merde
         */
        print_error(
            quit,
            "json_file must be a path 'string' or an ['string's]\n"
            "json_file: '%s'\n",
            json_file
        );
    }
    json_decref(jn_files);

    return 0;
}

/***************************************************************************
 *  Convert any json string to json binary.
 ***************************************************************************/
PRIVATE json_t * nonx_legalstring2json(const char *reference, const char *bf, pe_flag_t quit)
{
    size_t flags = JSON_DECODE_ANY;
    json_error_t error;

    json_t *jn_msg = json_loads(bf, flags, &error);
    if(!jn_msg) {
        print_error(
            quit,
            "Cannot convert non-legal json string to json binary.\n"
            "Reference: '%s'\n"
            "Json string:\n'%s'\n"
            "Error: '%s'\n in line %d, column %d, position %d.\n",
            reference,
            bf,
            error.text,
            error.line,
            error.column,
            error.position
        );
    }
    return jn_msg;
}

/***************************************************************************
 *  Read file line removing comments
 *  Return 0 end of file
 *         < 0 error
 *         > 0 process json string
 ***************************************************************************/
PRIVATE size_t on_load_op_callback(void *bf_, size_t bfsize, void *data)
{
    dl_list_t *dl_op = data;
    char *bf = bf_;

    op_t *op = dl_first(dl_op);
    if(op->op == OP_STR) {
        /*-----------------------------------*
         *      Operation in string
         *-----------------------------------*/
        // TODO WARNING algoritmo mal: bf_ es 1024, en lineas mas grandes con comment fallaria
        char *p = op->lines;
        char *begin = p;
        int maxlen = bfsize;
        while(p && *p && maxlen > 0) {
            if(*p == '\n') {
                break;
            }
            p++;
            maxlen--;
        }
        int len = p-begin;

        if(!len) {
            /*
             */
            return 0;
        }
        if(*p == '\n') {
            op->lines = p+1;
        } else {
            op->lines = p;
        }
        memmove(bf, begin, len);
        p = strstr(bf, INLINE_COMMENT);
        if(p) {
            /*
             *  Remove comments
             */
            *(p+0) = '\n';
            *(p+1) = 0;
            len = strlen(bf);
        }
        return len;

    } else {
        /*-----------------------------------*
         *      Operation in file
         *-----------------------------------*/
        if(!fgets(bf, bfsize, op->file)) {
            /*
             */
            return 0; /* end of file */
        }

        // TODO WARNING algoritmo mal: bf_ es 1024, en lineas mas grandes con comment fallaria
        register char *p;
        p = strstr(bf, INLINE_COMMENT);
        if(p) {
            /*
             *  Remove comments
             */
            *(p+0) = '\n';
            *(p+1) = 0;
        }
        return strlen(bf);
    }
}

/***************************************************************************
 *  Convert a legal json string to json binary.
 *  legal json string: MUST BE an array [] or object {}
 ***************************************************************************/
PRIVATE json_t * x_legalstring2json(dl_list_t* dl_op, char* reference, const char* bf, pe_flag_t quit)
{
    size_t flags = 0;
    json_error_t error;

    char *lines = strdup(bf);
    op_t *op = malloc(sizeof(op_t));
    if(!op) {
        print_error(
            quit,
            "No memory for %d bytes.\n",
            (int)sizeof(op_t)
        );
        free(lines);
        return 0;
    }
    memset(op, 0, sizeof(op_t));
    op->op = OP_STR;
    op->lines = lines;
    dl_insert(dl_op, op);
    json_t *jn_msg = json_load_callback(on_load_op_callback, dl_op, flags, &error);
    if(!jn_msg) {
        print_error(
            quit,
            "Cannot convert legal json string to json binary.\n"
            "Reference: '%s'\n"
            "Error: '%s'\n in line %d, column %d, position %d.\n"
            "Json string: \n%s\n",
            reference,
            error.text,
            error.line,
            error.column,
            error.position,
            bf
        );
    }
    free(lines);
    dl_delete(dl_op, op, free);
    return jn_msg;
}

/***************************************************************************
 *  Load a extended json file
 ***************************************************************************/
PRIVATE json_t *load_json_file(dl_list_t* dl_op, const char* path, pe_flag_t quit)
{
    if(access(path, 0)!=0) {
        print_error(
            quit,
            "File '%s' not found.\n",
            path
        );
    }
    FILE *file = fopen(path, "r");
    if(!file) {
        print_error(
            quit,
            "Cannot open '%s' file.\n",
            path
        );
        return 0;
    }

    size_t flags=0;
    json_error_t error;

    op_t *op = malloc(sizeof(op_t));
    if(!op) {
        print_error(
            quit,
            "No memory for %d bytes.\n",
            (int)sizeof(op_t)
        );
        fclose(file);
        return 0;
    }
    memset(op, 0, sizeof(op_t));
    op->op = OP_FILE;
    op->file = file;
    dl_insert(dl_op, op);
    json_t *jn_msg = json_load_callback(on_load_op_callback, dl_op, flags, &error);
    if(!jn_msg) {
        print_error(
            quit,
            "json_load_file_callback() failed.\n"
            "File: '%s'\n"
            "Error: '%s'\n in line %d, column %d, position %d.\n",
            path,
            error.text,
            error.line,
            error.column,
            error.position
        );
    }
    fclose(file);
    dl_delete(dl_op, op, free);
    return jn_msg;
}

/***************************************************************************
 *  Update keys and values, recursive through all objects
 *  If overwrite is false then not update existing keys (protected write)
 ***************************************************************************/
PRIVATE int json_dict_recursive_update(json_t *object, json_t *other, BOOL overwrite, pe_flag_t quit)
{
    const char *key;
    json_t *value;

    if(!json_is_object(object) || !json_is_object(other)) {
        print_error(
            quit,
            "json_dict_recursive_update(): parameters must be objects\n"
        );
        return -1;
    }
    json_object_foreach(other, key, value) {
        json_t *dvalue = json_object_get(object, key);
        if(json_is_object(dvalue) && json_is_object(value)) {
            json_dict_recursive_update(dvalue, value, overwrite, quit);
        } else if(json_is_array(dvalue) && json_is_array(value)) {
            if(overwrite) {
                /*
                 *  WARNING
                 *  In configuration consider the lists as set (no repeated items).
                 */
                json_list_update(dvalue, value, true, quit);
            }
        } else {
            if(overwrite) {
                json_object_set_nocheck(object, key, value);
            } else {
                if(!json_object_get(object, key)) {
                    json_object_set_nocheck(object, key, value);
                }
            }
        }
    }
    return 0;
}

/***************************************************************************
 *  Find a json value in the list.
 *  Return index or -1 if not found or the index relative to 0.
 ***************************************************************************/
PRIVATE int json_list_find(json_t *list, json_t *value)
{
    size_t idx_found = -1;
    size_t flags = JSON_COMPACT|JSON_ENCODE_ANY;//|JSON_SORT_KEYS;
    size_t index;
    json_t *_value;
    char *s_found_value = json_dumps(value, flags);
    if(s_found_value) {
        json_array_foreach(list, index, _value) {
            char *s_value = json_dumps(_value, flags);
            if(s_value) {
                if(strcmp(s_value, s_found_value)==0) {
                    idx_found = index;
                    jsonp_free(s_value);
                    break;
                } else {
                    jsonp_free(s_value);
                }
            }
        }
        jsonp_free(s_found_value);
    }
    return idx_found;
}

/***************************************************************************
 *  Extend array values.
 *  If as_set is true then not repeated values
 ***************************************************************************/
PRIVATE int json_list_update(json_t *list, json_t *other, BOOL as_set, pe_flag_t quit)
{
    if(!json_is_array(list) || !json_is_array(other)) {
        print_error(
            quit,
            "json_list_update(): parameters must be lists\n"
        );
        return -1;
    }
    size_t index;
    json_t *value;
    json_array_foreach(other, index, value) {
        if(as_set) {
            int idx = json_list_find(list, value);
            if(idx < 0) {
                json_array_append(list, value);
            }
        } else {
            json_array_append(list, value);
        }
    }
    return 0;
}

/***************************************************************************
 *  Build a list (set) with lists of integer ranges.
 *  [[#from, #to], [#from, #to], #integer, #integer, ...] -> list
 *  WARNING: Arrays of two integers are considered range of integers.
 *  Arrays of one or more of two integers are considered individual integers.
 *
 *  Return the json list
 ***************************************************************************/
PRIVATE json_t *json_listsrange2set(json_t *listsrange, pe_flag_t quit)
{
    if(!json_is_array(listsrange)) {
        print_error(
            quit,
            "json_listsrange2set(): parameters must be lists\n"
        );
        return 0;
    }
    json_t *ln_list = json_array();

    size_t index;
    json_t *value;
    json_array_foreach(listsrange, index, value) {
        if(json_is_integer(value)) {
            // add new integer item
            if(json_list_find(ln_list, value)<0) {
                json_array_append(ln_list, value);
            }
        } else if(json_is_array(value)) {
            // add new integer list or integer range
            if(json_is_range(value)) {
                json_t *range = json_range_list(value);
                if(range) {
                    json_list_update(ln_list, range, true, quit);
                    json_decref(range);
                }
            } else {
                json_list_update(ln_list, value, true, quit);
            }
        } else {
            // ignore the rest
            continue;
        }
    }

    return ln_list;
}

/***************************************************************************
 *  Check if a list is a integer range:
 *      - must be a list of two integers (first < second)
 ***************************************************************************/
PRIVATE BOOL json_is_range(json_t *list)
{
    if(json_array_size(list) != 2)
        return false;

    json_int_t first = json_integer_value(json_array_get(list, 0));
    json_int_t second = json_integer_value(json_array_get(list, 1));
    if(first <= second)
        return true;
    else
        return false;
}

/***************************************************************************
 *  Return a expanded integer range
 ***************************************************************************/
PRIVATE json_t *json_range_list(json_t *list)
{
    if(!json_is_range(list))
        return 0;
    json_int_t first = json_integer_value(json_array_get(list, 0));
    json_int_t second = json_integer_value(json_array_get(list, 1));
    json_t *range = json_array();
    for(int i=first; i<=second; i++) {
        json_t *jn_int = json_integer(i);
        json_array_append_new(range, jn_int);
    }
    return range;

}

/***************************************************************************
 *  Expand a dict group
 ***************************************************************************/
PRIVATE json_t *expand_matched_dict_group_key(json_t *kw, pe_flag_t quit)
{
    json_t * __range__ = json_object_get(kw, "__range__");
    json_t * __vars__ = json_object_get(kw, "__vars__");
    json_t * __content__ = json_object_get(kw, "__content__");
    if(!__range__ || !__vars__ || !__content__) {
        print_error(
            quit,
            "Expand a dict needs of __range__, __vars__ and __content__ keys\n"
        );
        return 0;
    }
    json_t *jn_expanded = json_object();
    json_t * jn_set = json_listsrange2set( __range__, quit);

    //PRINT_JSON("set", jn_set);
    json_t *value;
    int index;
    json_array_foreach(jn_set, index, value) {
        char temp[32];
        json_int_t range = json_integer_value(value);
        snprintf(temp, sizeof(temp), "%" JSON_INTEGER_FORMAT , range);
        json_object_set_new(__vars__, "__range__", json_string(temp));
        json_t *jn_new_content = json_deep_copy(__content__);
        char *replaced = replace_vars(jn_new_content, __vars__, quit);
        if(replaced) {
            json_t *jn_replaced = nonx_legalstring2json("", replaced, quit);
            json_object_update(jn_expanded, jn_replaced);
            json_decref(jn_replaced);
            jsonp_free(replaced);
        }
        json_decref(jn_new_content);
        //PRINT_JSON("expanded", jn_expanded);
    }
    json_decref(jn_set);
    return jn_expanded;
}

/***************************************************************************
 *  Find a regex key group in a dict (with recursivity)
 ***************************************************************************/
PRIVATE int expand_dict_matched_group_key(
    json_t *jn_dict,
    pcre2_code_8 *re,
    pcre2_match_data_8 *match_data,
    pe_flag_t quit)
{
    json_t *jn_to_expand = json_object();
    /*
     *  Search at first level
     */
    json_t *jn_value;
    const char *key;
    json_object_foreach(jn_dict, key, jn_value) {
        int rc = pcre2_match_8(
            re,                 /* the compiled pattern */
            (PCRE2_SPTR8)key,   /* the subject string */
            strlen(key),        /* the length of the subject */
            0,                  /* start at offset 0 in the subject */
            0,                  /* default options */
            match_data,         /* block for storing the result */
            NULL                /* use default match context */
        );
        if(rc > 1) {
            char group_key[256];
            size_t sz = sizeof(group_key);
            pcre2_substring_copy_bynumber_8(
                match_data,
                1,
                (PCRE2_UCHAR8 *)group_key,
                &sz
            );
            json_object_set_new(jn_to_expand, key, json_string(group_key));
        } else if(json_is_array(jn_value)) {
            json_t *jn_v2;
            int index;
            json_array_foreach(jn_value, index, jn_v2) {
                if(json_is_object(jn_v2)) {
                    expand_dict(jn_v2, quit);
                    expand_list(jn_v2, quit);
                }
            }
        } else if(json_is_object(jn_value)) {
            expand_dict(jn_value, quit);
            expand_list(jn_value, quit);
        }
    }

    /*
     *  Expand first level
     */
    json_object_foreach(jn_to_expand, key, jn_value) {
        const char *group_key = json_string_value(jn_value);
        json_t *jn_matched_dict = json_object_get(jn_dict, key);
        json_incref(jn_matched_dict);
        json_object_del(jn_dict, key);
        json_t *jn_new_dict = expand_matched_dict_group_key(jn_matched_dict, quit);
        if(jn_new_dict) {
            //PRINT_JSON("new_dict", jn_new_dict);
            json_object_set_new(jn_dict, group_key, jn_new_dict);
        }
        json_decref(jn_matched_dict);
    }
    json_decref(jn_to_expand);

    return false;
}

/***************************************************************************
 *  Expand a dict of json data in a range, defined by {^^ ^^}
 ***************************************************************************/
PRIVATE int expand_dict(json_t *jn_dict, pe_flag_t quit)
{
    int errornumber;
    PCRE2_SIZE erroroffset;
    const char *re_key = "\\{\\^\\^(.+?)\\^\\^\\}";
    pcre2_code_8 *re = pcre2_compile_8(
        (PCRE2_SPTR8)re_key,     /* the pattern */
        PCRE2_ZERO_TERMINATED,  /* indicates pattern is zero-terminated */
        0,                      /* default options */
        &errornumber,           /* for error number */
        &erroroffset,           /* for error offset */
        NULL                    /* use default compile context */
    );
    if(!re) {
        PCRE2_UCHAR8 buffer[256];
        pcre2_get_error_message_8(errornumber, buffer, sizeof(buffer));
        print_error(
            quit,
            "PCRE2 compilation failed at offset %d: %s\n",
            (int)erroroffset,
            buffer
        );
        return -1;
    }
    /*
     *  Using this function ensures that the block is exactly the right size for
     *  the number of capturing parentheses in the pattern.
     */
    pcre2_match_data_8 *match_data = pcre2_match_data_create_from_pattern_8(re, NULL);

    expand_dict_matched_group_key(jn_dict, re, match_data, quit);

    pcre2_match_data_free_8(match_data);
    pcre2_code_free_8(re);

    return 0;
}

/***************************************************************************
 *  Expand a dict group
 ***************************************************************************/
PRIVATE json_t *expand_matched_list_group_key(json_t *kw, pe_flag_t quit)
{
    json_t * __range__ = json_object_get(kw, "__range__");
    json_t * __vars__ = json_object_get(kw, "__vars__");
    json_t * __content__ = json_object_get(kw, "__content__");
    if(!__range__ || !__vars__ || !__content__) {
        print_error(
            quit,
            "Expand a dict needs of __range__, __vars__ and __content__ keys\n"
        );
        return 0;
    }
    json_t *jn_expanded = json_array();
    json_t *jn_set = json_listsrange2set(__range__, quit);

    //PRINT_JSON("set", jn_set);
    json_t *value;
    int index;
    json_array_foreach(jn_set, index, value) {
        char temp[32];
        json_int_t range = json_integer_value(value);
        snprintf(temp, sizeof(temp), "%" JSON_INTEGER_FORMAT , range);
        json_object_set_new(__vars__, "__range__", json_string(temp));
        json_t *jn_new_content = json_deep_copy(__content__);
        char *replaced = replace_vars(jn_new_content, __vars__, quit);
        if(replaced) {
            json_t *jn_replaced = nonx_legalstring2json("", replaced, quit);
            json_array_append_new(jn_expanded, jn_replaced);
            jsonp_free(replaced);
        }
        json_decref(jn_new_content);
        //PRINT_JSON("expanded", jn_expanded);
    }
    json_decref(jn_set);
    return jn_expanded;
}

/***************************************************************************
 *  Find a regex key group in a list (with recursivity)
 ***************************************************************************/
PRIVATE int expand_list_matched_group_key(
    json_t *jn_dict,
    pcre2_code_8 *re,
    pcre2_match_data_8 *match_data,
    pe_flag_t quit)
{
    json_t *jn_to_expand = json_object();
    /*
     *  Search at first level
     */
    json_t *jn_value;
    const char *key;
    json_object_foreach(jn_dict, key, jn_value) {
        int rc = pcre2_match_8(
            re,                 /* the compiled pattern */
            (PCRE2_SPTR8)key,   /* the subject string */
            strlen(key),        /* the length of the subject */
            0,                  /* start at offset 0 in the subject */
            0,                  /* default options */
            match_data,         /* block for storing the result */
            NULL                /* use default match context */
        );
        if(rc > 1) {
            char group_key[256];
            size_t sz = sizeof(group_key);
            pcre2_substring_copy_bynumber_8(
                match_data,
                1,
                (PCRE2_UCHAR8 *)group_key,
                &sz
            );
            json_object_set_new(jn_to_expand, key, json_string(group_key));
        }
    }

    /*
     *  Expand first level
     */
    json_object_foreach(jn_to_expand, key, jn_value) {
        const char *group_key = json_string_value(jn_value);
        json_t *jn_matched_dict = json_object_get(jn_dict, key);
        json_incref(jn_matched_dict);
        json_object_del(jn_dict, key);
        json_t *jn_new_list = expand_matched_list_group_key(jn_matched_dict, quit);
        if(jn_new_list) {
            //PRINT_JSON("new_dict", jn_new_dict);
            json_t *cur_list = json_object_get(jn_dict, group_key);
            if(!cur_list) {
                json_object_set_new(jn_dict, group_key, jn_new_list);

            } else {
                json_array_extend(cur_list, jn_new_list);
                json_decref(jn_new_list);
            }
        }
        json_decref(jn_matched_dict);
    }
    json_decref(jn_to_expand);

    return false;
}

/***************************************************************************
 *  Expand a list of json data in a range, defined by [^^ ^^]
 ***************************************************************************/
PRIVATE int expand_list(json_t *jn_dict, pe_flag_t quit)
{
    int errornumber;
    PCRE2_SIZE erroroffset;
    const char *re_key = "\\[\\^\\^(.+?)\\^\\^\\]";
    pcre2_code_8 *re = pcre2_compile_8(
        (PCRE2_SPTR8)re_key,     /* the pattern */
        PCRE2_ZERO_TERMINATED,  /* indicates pattern is zero-terminated */
        0,                      /* default options */
        &errornumber,           /* for error number */
        &erroroffset,           /* for error offset */
        NULL                    /* use default compile context */
    );
    if(!re) {
        PCRE2_UCHAR8 buffer[256];
        pcre2_get_error_message_8(errornumber, buffer, sizeof(buffer));
        print_error(
            quit,
            "PCRE2 compilation failed at offset %d: %s\n",
            (int)erroroffset,
            buffer
        );
        return -1;
    }
    /*
     *  Using this function ensures that the block is exactly the right size for
     *  the number of capturing parentheses in the pattern.
     */
    pcre2_match_data_8 *match_data = pcre2_match_data_create_from_pattern_8(re, NULL);

    expand_list_matched_group_key(jn_dict, re, match_data, quit);

    pcre2_match_data_free_8(match_data);
    pcre2_code_free_8(re);

    return 0;
}

/***************************************************************************
 *  In jn_dict replace (^^ ^^) by values found in jn_vars.
 *  Remember **free** the return string with jsonp_free().
 ***************************************************************************/
PRIVATE char * replace_vars(json_t *jn_dict, json_t *jn_vars, pe_flag_t quit)
{
    size_t flags = JSON_INDENT(4);//|JSON_SORT_KEYS;
    char *rendered_str = json_dumps(jn_dict, flags);
    char *prendered_str = rendered_str;
    //printf("rendered_str ====>\n%s\n", rendered_str);

    int errornumber;
    PCRE2_SIZE erroroffset;
    const char *re_key = "(\\(\\^\\^.+?\\^\\^\\))";
    pcre2_code_8 *re = pcre2_compile_8(
        (PCRE2_SPTR8)re_key,     /* the pattern */
        PCRE2_ZERO_TERMINATED,  /* indicates pattern is zero-terminated */
        0,                      /* default options */
        &errornumber,           /* for error number */
        &erroroffset,           /* for error offset */
        NULL                    /* use default compile context */
    );
    if(!re) {
        PCRE2_UCHAR8 buffer[256];
        pcre2_get_error_message_8(errornumber, buffer, sizeof(buffer));
        print_error(
            quit,
            "PCRE2 compilation failed at offset %d: %s\n",
            (int)erroroffset,
            buffer
        );
        return 0;
    }
    /*
     *  Using this function ensures that the block is exactly the right size for
     *  the number of capturing parentheses in the pattern.
     */
    pcre2_match_data_8 *match_data = pcre2_match_data_create_from_pattern_8(re, NULL);

    BOOL fin = false;
    while(!fin) {
        int rc = pcre2_match_8(
            re,                 /* the compiled pattern */
            (PCRE2_SPTR8)prendered_str, /* the subject string */
            strlen(prendered_str),      /* the length of the subject */
            0,                  /* start at offset 0 in the subject */
            0,                  /* default options */
            match_data,         /* block for storing the result */
            NULL                /* use default match context */
        );
        if(rc < 0 ) {
            break;
        }
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer_8(match_data);

        for (int i = 0; i < 1; i++) { // only once, because rendered_str is changed.
            PCRE2_SPTR8 substring_start = (PCRE2_SPTR8)prendered_str + ovector[2*i];
            size_t substring_length = ovector[2*i+1] - ovector[2*i];
            //printf("%2d: %.*s\n", i, (int)substring_length, (char *)substring_start);

            char macro[256]; // enough of course
            snprintf(macro, sizeof(macro), "%.*s", (int)substring_length, substring_start);
            char key[256];
            snprintf(key, sizeof(key), "%.*s", (int)substring_length-6, substring_start + 3);
            char rendered[4*1024]; // WARNING no puedes poner mas de 4K en el valor de una variable
            json_t *jn_value = json_object_get(jn_vars, key);
            if(!jn_value) {
                prendered_str = (char *)substring_start + substring_length;
                break;

            } else {
                if(json_is_string(jn_value)) {
                    snprintf(rendered, sizeof(rendered),
                        "%s",
                        json_string_value(jn_value)
                    );
                } else if(json_is_integer(jn_value)) {
                    snprintf(rendered, sizeof(rendered),
                        "%" JSON_INTEGER_FORMAT,
                        json_integer_value(jn_value)
                    );
                } else {
                    /*
                    *  This is useless: macros can be only in strings!
                    */
                    char *value = json_dumps(jn_value, JSON_ENCODE_ANY);
                    snprintf(rendered, sizeof(rendered), "%s", value);
                    jsonp_free(value);
                }

//                 printf("macro    ====>%s\n", macro);
//                 printf("rendered ====>%s\n", rendered);
//                 printf("rendered_str ====>\n%s\n", rendered_str);
                char * _new_value = replace_string(rendered_str, macro, rendered); // WARNING use malloc/free
                char *new_value = jsonp_strdup(_new_value);
                free(_new_value);
//                 printf("new_value ====>\n%s\n", new_value);
                jsonp_free(rendered_str);
                prendered_str = rendered_str = new_value;
            }
        }
    }

    pcre2_match_data_free_8(match_data);
    pcre2_code_free_8(re);

//     printf("rendered_str ====>\n%s\n", rendered_str);
    return rendered_str;
}

/***************************************************************************
 *  WARNING This function CANNOT use gbmem, glogger, etc
 ***************************************************************************/
PUBLIC char *json_config(
    BOOL print_verbose_config,
    BOOL print_final_config,
    const char *fixed_config,
    const char *variable_config,
    const char *config_json_file,
    const char *parameter_config,
    pe_flag_t quit
)
{
    dl_list_t dl_op = {0};
    dl_init(&dl_op, 0);

    size_t flags = JSON_INDENT(4);
    /*
     *  HACK don't use JSON_SORT_KEYS nor JSON_PRESERVE_ORDER in high processing, they do internal sorting.
     *  In jannson 2.8 JSON_PRESERVE_ORDER is deprecated, the order is always preserved.
     */
    //flags |= JSON_SORT_KEYS; Not necessary with 2.8 and higher

    json_t *jn_config;
    json_t *jn_variable_config=0;   // variable config from app main.c

    /*--------------------------------------*
     *  Fixed config from app main.c
     *--------------------------------------*/
    if(fixed_config && *fixed_config) {
        jn_config = x_legalstring2json(
            &dl_op,
            "fixed_config",
            fixed_config,
            quit
        );
    } else {
        jn_config = json_object();
    }
    if(print_verbose_config) {
        printf("fixed inside-program configuration ===>\n");
        json_dumpf(jn_config, stdout, flags);
        printf("\n");
    }

    /*--------------------------------------*
     *  Variable config from app main.c
     *--------------------------------------*/
    if(variable_config && *variable_config) {
        jn_variable_config = x_legalstring2json(
            &dl_op,
            "variable_config",
            variable_config,
            quit
        );
    } else {
        jn_variable_config = json_object();
    }
    if(print_verbose_config) {
        printf("variable inside-program configuration ===>\n");
        json_dumpf(jn_variable_config, stdout, flags);
        printf("\n");
    }

    /*--------------------------------------*
     *  Config file or files
     *--------------------------------------*/
    if(config_json_file && *config_json_file) {
        load_files(
            &dl_op,
            jn_variable_config,
            config_json_file,
            "config json file",
            print_verbose_config,
            print_final_config,
            quit
        );
    }

    /*--------------------------------------*
     *  Command line parameter config
     *--------------------------------------*/
    if(parameter_config && *parameter_config) {
        json_t *jn_parameter_config = x_legalstring2json(
            &dl_op,
            "command line parameter config",
            parameter_config,
            quit
        );
        if(print_verbose_config) {
            printf("command line parameter configuration ===>\n");
            json_dumpf(jn_parameter_config, stdout, flags);
            printf("\n");
        }
        json_dict_recursive_update(jn_variable_config, jn_parameter_config, true, quit);
        json_decref(jn_parameter_config);
    }

    /*------------------------------*
     *      Merge the config
     *------------------------------*/
    json_dict_recursive_update(jn_config, jn_variable_config, false, quit);
    json_decref(jn_variable_config);
    if(print_verbose_config) {
        printf("Merged configuration ===>\n");
        json_dumpf(jn_config, stdout, flags);
        printf("\n");
    }

    /*-----------------------------------------*
     *      Apply skeleton rules: {^^ ^^}
     *-----------------------------------------*/
printf("-> expand_dict\n");
    expand_dict(jn_config, quit);
    // PRINT_JSON("after expand dict ", jn_config);

    /*-----------------------------------------*
     *      Apply skeleton rules: [^^ ^^]
     *-----------------------------------------*/
printf("-> expand_list\n");
    expand_list(jn_config, quit);
    // PRINT_JSON("after expand list ", jn_config);

    /*-----------------------------------------*
     *      Apply skeleton rules: (^^ ^^)
     *  with __json_config_variables__ dict
     *-----------------------------------------*/
    json_t *__json_config_variables__ = json_object_get(
        jn_config,
        "__json_config_variables__"
    );
    // PRINT_JSON("variables", __json_config_variables__);
    char *final_sconfig;
    if(__json_config_variables__) {
        json_incref(__json_config_variables__);
        json_object_del(jn_config, "__json_config_variables__");

        char __host_name__[120];
        gethostname(__host_name__, sizeof(__host_name__)-1);

        json_object_set_new(
            __json_config_variables__,
            "__hostname__",
            json_string(__host_name__)
        );

        if(print_verbose_config) {
            printf("__json_config_variables__ ===>\n");
            json_dumpf(__json_config_variables__, stdout, flags);
            printf("\n");
        }

        final_sconfig = replace_vars(jn_config, __json_config_variables__, quit);
        json_decref(__json_config_variables__);
    } else {
        final_sconfig = json_dumps(jn_config, flags);
    }
    json_decref(jn_config);

//     printf("%s\n\n", final_sconfig);

    if(print_final_config || print_verbose_config) {
        printf("%s\n\n", final_sconfig);
        exit(0); // nothing more.
    }
    return final_sconfig;
}
