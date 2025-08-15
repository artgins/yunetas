/***********************************************************************
 *          JSON_CONFIG.C
 *
 *          Json configuration.
 *
 *          Copyright (c) 2015 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <jansson.h>
#include "helpers.h"
#include "kwid.h"

// From jansson_private.h
extern void jsonp_free(void *ptr);

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define INLINE_COMMENT "#^^"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int load_files(
    json_t *jn_variable_config,
    const char *json_file,
    const char *json_file_type,
    int print_verbose_config,
    int print_final_config,
    pe_flag_t quit
);
PRIVATE json_t *x_legalstring2json(const char *reference, const char *bf, pe_flag_t quit);
PRIVATE json_t *load_json_file(const char *path, pe_flag_t quit);
PRIVATE json_t *nonx_legalstring2json(const char *reference, const char *bf, pe_flag_t quit);

/***************************************************************************
 *  Load json config from a file '' or from files ['','']
 ***************************************************************************/
PRIVATE int load_files(
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
            json_string_value(jn_files),
            quit
        );
        if(print_verbose_config) {
            printf("%s '%s' ===>\n", json_file_type, json_string_value(jn_files));
            json_dumpf(jn_file, stdout, flags);
            printf("\n");
        }
        json_dict_recursive_update(jn_variable_config, jn_file, TRUE);
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
                    json_string_value(value),
                    quit
                );
                if(print_verbose_config) {
                    printf("%s '%s' ===>\n", json_file_type, json_string_value(value));
                    json_dumpf(jn_file, stdout, flags);
                    printf("\n");
                }
                json_dict_recursive_update(jn_variable_config, jn_file, TRUE);
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
PRIVATE json_t *nonx_legalstring2json(const char *reference, const char *bf, pe_flag_t quit)
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
 *  Convert a legal json string to json binary.
 *  legal json string: MUST BE an array [] or object {}
 ***************************************************************************/
PRIVATE json_t * x_legalstring2json(const char *reference, const char *bf, pe_flag_t quit)
{
    /*
     *  Memory needed, double for src,dst
     */
    size_t len = strlen(bf);

    /*
     *  Create source and load the string
     */
    gbuffer_t *gbuf_src = gbuffer_create(len, len);
    if(!gbuf_src) {
        print_error(
            quit,
            "No memory for %s: %lu bytes\n",
            reference?reference:"",
            len
        );
        return NULL;
    }
    gbuffer_append_string(gbuf_src, bf);

    /*
     *  Create destination
     */
    gbuffer_t *gbuf_dst = gbuffer_create(len, len);
    if(!gbuf_dst) {
        print_error(
            quit,
            "No memory for %s: %lu bytes\n",
            reference?reference:"",
            len
        );
        gbuffer_decref(gbuf_src);
        return NULL;
    }

    /*
     *  Filter the comments and save to destination
     */
    char *s;
    while((s=gbuffer_getline(gbuf_src, '\n'))) {
        char *f = strstr(s, INLINE_COMMENT);
        if(f) {
            /*
             *  Remove comments
             */
            *f = 0;
        }
        gbuffer_append_string(gbuf_dst, s);
    }

    /*
     *  Convert to jansson
     */
    s = gbuffer_cur_rd_pointer(gbuf_dst);
    json_t *jn_msg = string2json(s, TRUE);

    /*
     *  Extract special __json_config_variables__ if exits
     */
    json_t *__json_config_variables__ = kw_get_dict(0,
        jn_msg,
        "__json_config_variables__",
        json_object(),
        KW_EXTRACT
    );
    json_object_set_new(
        __json_config_variables__,
        "__hostname__",
        json_string(get_hostname())
    );

    /*
     *  Replace attribute vars
     */
    json_t *new_kw = json_replace_var(
        jn_msg,          // owned
        __json_config_variables__   // owned
    );

    /*
     *  Free
     */
    gbuffer_decref(gbuf_src);
    gbuffer_decref(gbuf_dst);

    return new_kw;
}

/***************************************************************************
 *  Load a extended json file
 ***************************************************************************/
PRIVATE json_t *load_json_file(const char *path, pe_flag_t quit)
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
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t len = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if(!buf) {
        print_error(
            quit,
            "No memory for: %lu bytes\n",
            len
        );
        fclose(file);
        return NULL;
    }

    size_t ret = fread(buf, 1, len, file);
    if (ret != len) {
        print_error(
            quit,
            "Cannot read '%s' file.\n",
            path
        );
        free(buf);
        fclose(file);
        return NULL;
    }
    buf[len] = '\0';

    json_t *jn_msg = x_legalstring2json(path, buf, quit);

    free(buf);
    fclose(file);

    return jn_msg;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *json_config(
    BOOL print_verbose_config,      // WARNING, if TRUE will exit(0)
    BOOL print_final_config,        // WARNING, if TRUE will exit(0)
    const char *fixed_config,
    const char *variable_config,
    const char *config_json_file,
    const char *parameter_config,
    pe_flag_t quit                  // What to do in case of error
)
{
    size_t flags = JSON_INDENT(4);
    json_t *jn_config;
    json_t *jn_variable_config=NULL;

    /*--------------------------------------*
     *  Fixed config
     *--------------------------------------*/
    if(fixed_config && *fixed_config) {
        jn_config = x_legalstring2json(
            "fixed_config",
            fixed_config,
            quit
        );
    } else {
        jn_config = json_object();
    }
    if(print_verbose_config) {
        printf("fixed configuration ===>\n");
        json_dumpf(jn_config, stdout, flags);
        printf("\n");
    }

    /*--------------------------------------*
     *  Variable config
     *--------------------------------------*/
    if(variable_config && *variable_config) {
        jn_variable_config = x_legalstring2json(
            "variable_config",
            variable_config,
            quit
        );
    } else {
        jn_variable_config = json_object();
    }
    if(print_verbose_config) {
        printf("variable configuration ===>\n");
        json_dumpf(jn_variable_config, stdout, flags);
        printf("\n");
    }

    /*--------------------------------------*
     *  Config from file or files
     *--------------------------------------*/
    if(config_json_file && *config_json_file) {
        load_files(
            jn_variable_config,
            config_json_file,
            "config from json files",
            print_verbose_config,
            print_final_config,
            quit
        );
    }

    /*--------------------------------------*
     *  Config from command line parameter
     *--------------------------------------*/
    if(parameter_config && *parameter_config) {
        json_t *jn_parameter_config = x_legalstring2json(
            "config from command line parameter",
            parameter_config,
            quit
        );
        if(print_verbose_config) {
            printf("command line parameter configuration ===>\n");
            json_dumpf(jn_parameter_config, stdout, flags);
            printf("\n");
        }
        json_dict_recursive_update(jn_variable_config, jn_parameter_config, TRUE);
        json_decref(jn_parameter_config);
    }

    /*------------------------------*
     *      Merge the config
     *------------------------------*/
    json_dict_recursive_update(jn_config, jn_variable_config, FALSE);
    json_decref(jn_variable_config);
    if(print_verbose_config) {
        printf("Merged configuration ===>\n");
        json_dumpf(jn_config, stdout, flags);
        printf("\n");
    }

    if(print_final_config || print_verbose_config) {
        printf("Final configuration ===>\n");
        json_dumpf(jn_config, stdout, flags);
        exit(0); // nothing more.
    }
    return jn_config;
}
