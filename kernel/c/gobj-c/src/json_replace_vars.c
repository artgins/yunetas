/***********************************************************************
 *          json_replace_vars.c
 *
 *          Json configuration, replace variables.
 *
 *  Replace placeholders in JSON keys and values with values from a variable dict.
 *  Supports recursive objects and arrays, with customizable delimiters.
 *
 *  Dependencies: Jansson, PCRE2
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <jansson.h>

#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "kwid.h"
#include "helpers.h"


/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    PCRE2_SPTR     pattern;
    pcre2_code     *regex;
    const char     *open_delim;
    const char     *close_delim;
    size_t         open_len;
    size_t         close_len;
} subst_config_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *replace_json(json_t *jn, json_t *jn_vars, subst_config_t *cfg);

/***************************************************************************
 *  Create substitution configuration with compiled regex
 ***************************************************************************/
PRIVATE subst_config_t *create_subst_config(const char *open, const char *close)
{
    subst_config_t *cfg = calloc(1, sizeof(subst_config_t));
    cfg->open_delim = open;
    cfg->close_delim = close;
    cfg->open_len = strlen(open);
    cfg->close_len = strlen(close);

    char pattern[512];
    snprintf(pattern, sizeof(pattern), "\\Q%s\\E(.+?)\\Q%s\\E", open, close);
    cfg->pattern = (PCRE2_SPTR)strdup(pattern);

    int errornumber;
    PCRE2_SIZE erroroffset;
    cfg->regex = pcre2_compile(cfg->pattern, PCRE2_ZERO_TERMINATED, 0, &errornumber, &erroroffset, NULL);

    if(!cfg->regex) {
        free((char *)cfg->pattern);
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
    if(cfg) {
        pcre2_code_free(cfg->regex);
        free((char *)cfg->pattern);
        free(cfg);
    }
}

/***************************************************************************
 *  Replace placeholders in a string with values from jn_vars
 ***************************************************************************/
PRIVATE char *replace_vars(const char *input, json_t *jn_vars, subst_config_t *cfg)
{
    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(cfg->regex, NULL);
    const char *cursor = input;
    size_t input_len = strlen(input);
    size_t outcap = input_len + 64;
    char *output = calloc(outcap, 1);
    output[0] = 0;

    while(*cursor) {
        int rc = pcre2_match(cfg->regex, (PCRE2_SPTR)cursor, strlen(cursor), 0, 0, match_data, NULL);
        if(rc < 0) {
            strcat(output, cursor);
            break;
        }

        PCRE2_SIZE *ov = pcre2_get_ovector_pointer(match_data);
        strncat(output, cursor, ov[0]);

        size_t len = ov[3] - ov[2];
        char *varname = strndup(cursor + ov[2], len);

        const char *val = json_string_value(json_object_get(jn_vars, varname));
        if(val) {
            size_t need = strlen(output) + strlen(val) + strlen(cursor + ov[1]) + 1;
            if(need > outcap) {
                outcap = need * 2;
                output = realloc(output, outcap);
            }
            strcat(output, val);
        } else {
            strncat(output, cursor + ov[0], ov[1] - ov[0]);
        }

        free(varname);
        cursor += ov[1];
    }

    pcre2_match_data_free(match_data);
    return output;
}

/***************************************************************************
 *  Replace placeholders in arrays
 ***************************************************************************/
PRIVATE json_t *replace_array(json_t *jn_array, json_t *jn_vars, subst_config_t *cfg)
{
    size_t i;
    json_t *result = json_array();
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
        const char *key;
        json_t *val;

        json_object_foreach(jn, key, val) {
            char *new_key = replace_vars(key, jn_vars, cfg);
            json_t *new_val = replace_json(val, jn_vars, cfg);
            json_object_set_new(result, new_key, new_val);
            free(new_key);
        }
        return result;

    } else if(json_is_array(jn)) {
        return replace_array(jn, jn_vars, cfg);

    } else if(json_is_string(jn)) {
        const char *s = json_string_value(jn);
        char *replaced = replace_vars(s, jn_vars, cfg);
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
