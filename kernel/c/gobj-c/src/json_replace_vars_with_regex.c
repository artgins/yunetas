/***********************************************************************
 *          json_replace_vars.c  (POSIX regex version)
 *
 *          Json configuration, replace variables.
 *
 *  Replace placeholders in JSON keys and values with values from a variable dict.
 *  Supports recursive objects and arrays, with customizable delimiters.
 *
 *  Dependencies: Jansson, POSIX regex.h
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <regex.h>
#include <jansson.h>

#include "kwid.h"

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    char        *open_delim;
    char        *close_delim;
    size_t       open_len;
    size_t       close_len;
    regex_t      open_re;   // literal-escaped open delimiter
    regex_t      close_re;  // literal-escaped close delimiter
} subst_config_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *replace_json(json_t *jn, json_t *jn_vars, subst_config_t *cfg);

/***************************************************************************
 *  Helpers: escape text so it matches literally in a POSIX ERE
 ***************************************************************************/
static char *regex_escape_literal(const char *s)
{
    /* Characters that are special in ERE and should be backslashed */
    const char *special = ".^$*+?()[]{}|\\";
    size_t len = strlen(s);
    /* Worst case: every char escaped -> 2x + 1 */
    char *out = (char *)malloc(len * 2 + 1);
    if(!out) {
        return NULL;
    }

    char *w = out;
    for(size_t i = 0; i < len; i++) {
        if(strchr(special, s[i])) {
            *w++ = '\\';
        }
        *w++ = s[i];
    }
    *w = 0;
    return out;
}

/***************************************************************************
 *  Create substitution configuration with compiled regex (POSIX)
 ***************************************************************************/
PRIVATE subst_config_t *create_subst_config(const char *open, const char *close)
{
    subst_config_t *cfg = calloc(1, sizeof(subst_config_t));
    if(!cfg) {
        return NULL;
    }

    cfg->open_delim  = strdup(open);
    cfg->close_delim = strdup(close);
    if(!cfg->open_delim || !cfg->close_delim) {
        free(cfg->open_delim);
        free(cfg->close_delim);
        free(cfg);
        return NULL;
    }
    cfg->open_len  = strlen(open);
    cfg->close_len = strlen(close);

    /* Compile two literal regexes for the delimiters */
    char *open_lit  = regex_escape_literal(open);
    char *close_lit = regex_escape_literal(close);
    if(!open_lit || !close_lit) {
        free(open_lit);
        free(close_lit);
        free(cfg->open_delim);
        free(cfg->close_delim);
        free(cfg);
        return NULL;
    }

    int rc1 = regcomp(&cfg->open_re, open_lit, REG_EXTENDED);
    int rc2 = regcomp(&cfg->close_re, close_lit, REG_EXTENDED);
    free(open_lit);
    free(close_lit);

    if(rc1 != 0 || rc2 != 0) {
        if(rc1 == 0) {
            regfree(&cfg->open_re);
        }
        if(rc2 == 0) {
            regfree(&cfg->close_re);
        }
        free(cfg->open_delim);
        free(cfg->close_delim);
        free(cfg);
        return NULL;
    }

    return cfg;
}

/***************************************************************************
 *  Free substitution configuration
 ***************************************************************************/
PRIVATE void free_subst_config(subst_config_t *cfg)
{
    if(!cfg) {
        return;
    }
    regfree(&cfg->open_re);
    regfree(&cfg->close_re);
    free(cfg->open_delim);
    free(cfg->close_delim);
    free(cfg);
}

/***************************************************************************
 *  Safe append helper with growth
 ***************************************************************************/
static void sb_append(char **buf, size_t *cap, const char *src, size_t n)
{
    if(n == (size_t)-1) {
        n = strlen(src);
    }
    size_t need = strlen(*buf) + n + 1;
    if(need > *cap) {
        *cap = need * 2;
        *buf = (char *)realloc(*buf, *cap);
    }
    strncat(*buf, src, n);
}

/***************************************************************************
 *  Replace placeholders in a string with values from jn_vars (POSIX regex)
 *
 *  Non-greedy behavior is achieved by:
 *   - Find next OPEN via regex on the current cursor
 *   - From the end of that match, find the nearest CLOSE via regex
 ***************************************************************************/
PRIVATE char *replace_vars(const char *input, json_t *jn_vars, subst_config_t *cfg)
{
    const char *cursor = input;
    size_t outcap = strlen(input) + 64;
    char *output = (char *)calloc(outcap, 1);
    if(!output) {
        return NULL;
    }
    output[0] = 0;

    while(*cursor) {
        /* Find OPEN from cursor */
        regmatch_t mopen;
        int rc = regexec(&cfg->open_re, cursor, 1, &mopen, 0);
        if(rc != 0 || mopen.rm_so == -1) {
            /* No more OPEN: append rest and break */
            sb_append(&output, &outcap, cursor, (size_t)-1);
            break;
        }

        /* Append text before OPEN */
        if(mopen.rm_so > 0) {
            sb_append(&output, &outcap, cursor, (size_t)mopen.rm_so);
        }

        /* Find CLOSE from after OPEN */
        const char *after_open = cursor + mopen.rm_eo;
        regmatch_t mclose;
        rc = regexec(&cfg->close_re, after_open, 1, &mclose, 0);
        if(rc != 0 || mclose.rm_so == -1) {
            /* No CLOSE found: keep the rest verbatim from OPEN position */
            sb_append(&output, &outcap, cursor + mopen.rm_so, (size_t)-1);
            break;
        }

        /* Extract var name between OPEN and CLOSE */
        size_t varlen = (size_t)mclose.rm_so;
        char *varname = (char *)malloc(varlen + 1);
        if(!varname) {
            /* On OOM, append raw and exit */
            sb_append(&output, &outcap, cursor + mopen.rm_so, (size_t)-1);
            break;
        }
        memcpy(varname, after_open, varlen);
        varname[varlen] = 0;

        /* Lookup */
        const char *val = NULL;
        json_t *jv = json_object_get(jn_vars, varname);
        if(json_is_string(jv)) {
            val = json_string_value(jv);
        }

        if(val) {
            sb_append(&output, &outcap, val, (size_t)-1);
        } else {
            /* Unknown var: keep original including delimiters */
            sb_append(&output, &outcap, cursor + mopen.rm_so, (size_t)(mclose.rm_eo + (after_open - (cursor + mopen.rm_so))));
        }

        free(varname);

        /* Advance cursor after CLOSE */
        cursor = after_open + mclose.rm_eo;
    }

    return output;
}

/***************************************************************************
 *  Replace placeholders in arrays
 ***************************************************************************/
PRIVATE json_t *replace_array(json_t *jn_array, json_t *jn_vars, subst_config_t *cfg)
{
    size_t i;
    json_t *result = json_array();
    if(!result) {
        return NULL;
    }
    json_t *elem;

    json_array_foreach(jn_array, i, elem) {
        json_t *replaced = replace_json(elem, jn_vars, cfg);
        json_array_append_new(result, replaced);
    }

    return result;
}

/***************************************************************************
 *  Recursive replacement in JSON structures
 ***************************************************************************/
PRIVATE json_t *replace_json(json_t *jn, json_t *jn_vars, subst_config_t *cfg)
{
    if(json_is_object(jn)) {
        json_t *result = json_object();
        if(!result) {
            return NULL;
        }
        const char *key;
        json_t *val;

        json_object_foreach(jn, key, val) {
            char *new_key = replace_vars(key, jn_vars, cfg);
            json_t *new_val = replace_json(val, jn_vars, cfg);
            json_object_set_new(result, new_key ? new_key : key, new_val);
            free(new_key);
        }
        return result;

    } else if(json_is_array(jn)) {
        return replace_array(jn, jn_vars, cfg);

    } else if(json_is_string(jn)) {
        const char *s = json_string_value(jn);
        char *replaced = replace_vars(s, jn_vars, cfg);
        if(!replaced) {
            return json_deep_copy(jn);
        }
        json_t *result = json_string(replaced);
        free(replaced);
        return result;

    } else {
        return json_deep_copy(jn);
    }
}

/***************************************************************************
 *  PUBLIC API: replace using custom delimiters
 ***************************************************************************/
PUBLIC json_t *json_replace_var_custom(
    json_t *jn_dict, // owned
    json_t *jn_vars, // owned
    const char *open,
    const char *close
)
{
    if(!json_is_object(jn_dict) || !json_is_object(jn_vars)) {
        JSON_DECREF(jn_dict)
        JSON_DECREF(jn_vars)
        return NULL;
    }

    subst_config_t *cfg = create_subst_config(open, close);
    if(!cfg) {
        JSON_DECREF(jn_dict)
        JSON_DECREF(jn_vars)
        return NULL;
    }

    json_t *result = replace_json(jn_dict, jn_vars, cfg);
    free_subst_config(cfg);

    JSON_DECREF(jn_dict)
    JSON_DECREF(jn_vars)
    return result;
}

/***************************************************************************
 *  PUBLIC API: default delimiters (^^var^^)
 ***************************************************************************/
PUBLIC json_t *json_replace_var(
    json_t *jn_dict, // owned
    json_t *jn_vars  // owned
)
{
    return json_replace_var_custom(jn_dict, jn_vars, "(^^", "^^)");
}
