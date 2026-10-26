/****************************************************************************
 *          MAKE_SKELETON.C
 *          Make a yuno-skeleton template from a project
 *
 *          Copyright (c) 2015 Niyamaka.
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <sys/stat.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>
#include <yunetas.h>
#include "make_skeleton.h"
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
 *  One case form of a keyword, as a template marker of the content
 *  ({{var}}) and of the names (+var+). The first keyword to claim a form
 *  keeps it (yunorole before rootname)
 ***************************************************************************/
PRIVATE void add_form(json_t *content_values, json_t *name_values, const char *old, const char *var)
{
    // A keyword is a KEY of the dicts, never a kw path
    if(json_object_get(content_values, old)) {
        return;
    }
    char marker[NAME_MAX + 1];
    snprintf(marker, sizeof(marker), "{{%s}}", var);
    json_object_set_new(content_values, old, json_string(marker));
    snprintf(marker, sizeof(marker), "+%s+", var);
    json_object_set_new(name_values, old, json_string(marker));
}

/***************************************************************************
 *  The keyword in lower, Capitalized and UPPER case
 ***************************************************************************/
PRIVATE void add_value(json_t *content_values, json_t *name_values, char *old_name, const char *var_)
{
    char var[NAME_MAX + 1];
    snprintf(var, sizeof(var), "%s", var_);

    lower(old_name);
    lower(var);
    add_form(content_values, name_values, old_name, var);

    capitalize(old_name);
    capitalize(var);
    add_form(content_values, name_values, old_name, var);

    upper(old_name);
    upper(var);
    add_form(content_values, name_values, old_name, var);

    lower(old_name);
}

/***************************************************************************
 *  Make a skeleton
 ***************************************************************************/
int make_skeleton(char *source_path, char *destination_path, char *yunorole, char *rootname)
{
    char dir_bf[PATH_MAX];
    snprintf(dir_bf, sizeof(dir_bf), "%s", destination_path);
    char *destination_dir = dirname(dir_bf);

    json_t *content_values = json_object();
    json_t *name_values = json_object();

    add_value(content_values, name_values, yunorole, "yunorole");

    int list_size;
    const char **names = split2(rootname, "|, ", &list_size);
    for(int i=0; i<list_size; i++) {
        add_value(content_values, name_values, (char *)names[i], "rootname");
    }
    split_free2(names);

    print_json("Content markers", content_values);

    int ret = 0;
    if(access(destination_dir, F_OK) != 0) {
        printf("Creating directory: %s\n", destination_dir);
        if(mkrdir(destination_dir, 02775) < 0) {
            fprintf(stderr, "ERROR: cannot create the destination's directory '%s': %s\n",
                destination_dir, strerror(errno));
            ret = -1;
        }
    } else if(!is_directory(destination_dir)) {
        fprintf(stderr, "ERROR: '%s' is not a directory\n", destination_dir);
        ret = -1;
    }

    clone_spec_t spec = {
        .content_values = content_values,
        .name_values = name_values,
        .text_suffix = "_tmpl",
        .untouched = json_object()
    };

    if(ret == 0) {
        printf("Creating skeleton: %s\n", destination_path);
        ret = clone_tree_dir(destination_path, source_path, &spec);
        if(ret == 0) {
            print_untouched_words(&spec);
            printf("\nTo use it, add an entry for '%s' in the __skeletons__.json of its directory\n",
                destination_path);
        } else if(access(destination_path, F_OK) == 0) {
            fprintf(stderr, "ERROR: the clone stopped at the first error, '%s' is left as far as it got\n",
                destination_path);
        }
    }

    json_decref(spec.untouched);
    json_decref(name_values);
    json_decref(content_values);

    return ret;
}
