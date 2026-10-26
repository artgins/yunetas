/****************************************************************************
 *          RENAME_GCLASS.C
 *          Clone a gclass (c_<old>.c/.h) with a new name
 *
 *          Copyright (c) 2015 Niyamaka.
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <jansson.h>
#include <helpers.h>
#include "clone_tree_dir.h"
#include "rename_gclass.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/

/***************************************************************************
 *      Structures
 ***************************************************************************/

/***************************************************************************
 *      Data
 ***************************************************************************/

/***************************************************************************
 *  Rename a gclass
 ***************************************************************************/
PUBLIC int rename_gclass(char *old_gclass_name, char *new_gclass_name)
{
    char old_puntoc[PATH_MAX];
    char old_puntoh[PATH_MAX];

    char new_puntoc[PATH_MAX];
    char new_puntoh[PATH_MAX];

    lower(old_gclass_name);
    snprintf(old_puntoc, sizeof(old_puntoc), "c_%s.c", old_gclass_name);
    snprintf(old_puntoh, sizeof(old_puntoh), "c_%s.h", old_gclass_name);

    if(!is_regular_file(old_puntoc)) {
        printf("File NOT exist: %s. Remember don't include the 'c_' prefix, neither '.c' suffix.\n", old_puntoc);
        return -1;
    }
    if(!is_regular_file(old_puntoh)) {
        printf("File NOT exist: %s. Remember don't include the 'c_' prefix, neither '.c' suffix.\n", old_puntoh);
        return -1;
    }

    lower(new_gclass_name);
    snprintf(new_puntoc, sizeof(new_puntoc), "c_%s.c", new_gclass_name);
    snprintf(new_puntoh, sizeof(new_puntoh), "c_%s.h", new_gclass_name);

    if(access(new_puntoc, F_OK) == 0) {
        printf("File ALREADY exist: %s\n", new_puntoc);
        return -1;
    }
    if(access(new_puntoh, F_OK) == 0) {
        printf("File ALREADY exist: %s\n", new_puntoh);
        return -1;
    }

    /*
     *  The name in lower, Capitalized and UPPER case
     */
    json_t *jn_values = json_object();

    lower(old_gclass_name);
    lower(new_gclass_name);
    json_object_set_new(jn_values, old_gclass_name, json_string(new_gclass_name));

    capitalize(old_gclass_name);
    capitalize(new_gclass_name);
    json_object_set_new(jn_values, old_gclass_name, json_string(new_gclass_name));

    upper(old_gclass_name);
    upper(new_gclass_name);
    json_object_set_new(jn_values, old_gclass_name, json_string(new_gclass_name));

    clone_spec_t spec = {
        .content_values = jn_values,
        .name_values = jn_values,
        .text_suffix = NULL,
        .untouched = json_object()
    };

    int ret = clone_file(new_puntoh, old_puntoh, &spec);
    if(ret == 0) {
        ret = clone_file(new_puntoc, old_puntoc, &spec);
        if(ret < 0) {
            // Error already logged; not half a gclass left behind
            unlink(new_puntoh);
        }
    }

    if(ret == 0) {
        print_untouched_words(&spec);
        printf("\nRemember to change file headers (copyright, year, description)\n");
    }

    json_decref(spec.untouched);
    json_decref(jn_values);

    return ret;
}
