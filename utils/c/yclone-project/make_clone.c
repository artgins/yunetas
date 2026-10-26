/****************************************************************************
 *          MAKE_CLONE.C
 *          Clone a project with a new name
 *
 *          Copyright (c) 2015 Niyamaka.
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <jansson.h>
#include <helpers.h>
#include "make_clone.h"
#include "clone_tree_dir.h"

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
 *  Values to replace: the keyword in lower, Capitalized and UPPER case
 ***************************************************************************/
PRIVATE json_t *values2replace(char *old_name, char *new_name)
{
    json_t *jn_values = json_object();

    lower(new_name);
    lower(old_name);
    json_object_set_new(jn_values, old_name, json_string(new_name));

    capitalize(new_name);
    capitalize(old_name);
    json_object_set_new(jn_values, old_name, json_string(new_name));

    upper(new_name);
    upper(old_name);
    json_object_set_new(jn_values, old_name, json_string(new_name));

    lower(new_name);
    lower(old_name);

    return jn_values;
}

/***************************************************************************
 *  Clone a project
 ***************************************************************************/
int make_clone(char *source_path, char *destination_path, char *source_keyword, char *destination_keyword)
{
    char dir_bf[PATH_MAX];
    snprintf(dir_bf, sizeof(dir_bf), "%s", destination_path);
    char *destination_dir = dirname(dir_bf);

    char name_bf[PATH_MAX];
    snprintf(name_bf, sizeof(name_bf), "%s", destination_path);
    char *project = basename(name_bf);

    if(empty_string(destination_keyword)) {
        destination_keyword = project;
    }

    if(access(destination_dir, F_OK) != 0) {
        printf("Creating directory: %s\n", destination_dir);
        if(mkrdir(destination_dir, 02775) < 0) {
            fprintf(stderr, "ERROR: cannot create the destination's directory '%s': %s\n",
                destination_dir, strerror(errno));
            return -1;
        }
    } else if(!is_directory(destination_dir)) {
        fprintf(stderr, "ERROR: '%s' is not a directory\n", destination_dir);
        return -1;
    }

    json_t *toreplace = values2replace(source_keyword, destination_keyword);
    clone_spec_t spec = {
        .content_values = toreplace,
        .name_values = toreplace,
        .text_suffix = NULL,
        .untouched = json_object()
    };

    printf("Creating project: %s\n", destination_path);
    int ret = clone_tree_dir(destination_path, source_path, &spec);
    if(ret == 0) {
        print_untouched_words(&spec);
    } else if(access(destination_path, F_OK) == 0) {
        fprintf(stderr, "ERROR: the clone stopped at the first error, '%s' is left as far as it got\n",
            destination_path);
    }

    json_decref(spec.untouched);
    json_decref(toreplace);

    return ret;
}
