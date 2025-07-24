/****************************************************************************
 *          MAKE_CLONE.C
 *          Make a skeleton
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <argp-standalone.h>
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
struct find_sk_s {
    char *fullname;
    int fullname_size;
    const char* base;
    const char *file;
    const char *skeleton;
    json_t *jn_config;
};

/***************************************************************************
 *      Data
 ***************************************************************************/

/****************************************************************************
 *
 ****************************************************************************/
/***************************************************************************
 *  Upper
 ***************************************************************************/
char *upper(char *s)
{
    char *p = s;
    while(*p) {
        *p = toupper(*p);
        p++;
    }
    return s;
}

/***************************************************************************
 *  lower
 ***************************************************************************/
char *lower(char *s)
{
    char *p = s;
    while(*p) {
        *p = tolower(*p);
        p++;
    }
    return s;
}

/***************************************************************************
 *  capitalize
 ***************************************************************************/
char *capitalize(char *s)
{
    lower(s);
    if(*s)
        *s = toupper(*s);
    return s;
}

/***************************************************************************
 *  Values to replace
 ***************************************************************************/
PRIVATE int add_value(json_t *jn_values, char *old_name, char *new_name)
{
    lower(new_name);
    lower(old_name);
    if(!kw_has_key(jn_values, old_name)) {
        json_object_set_new(jn_values, old_name, json_string(new_name));
    }

    capitalize(new_name);
    capitalize(old_name);
    if(!kw_has_key(jn_values, old_name)) {
        json_object_set_new(jn_values, old_name, json_string(new_name));
    }

    upper(new_name);
    upper(old_name);
    if(!kw_has_key(jn_values, old_name)) {
        json_object_set_new(jn_values, old_name, json_string(new_name));
    }

    lower(new_name);
    lower(old_name);

    return 0;
}

/***************************************************************************
 *  Values to replace
 ***************************************************************************/
json_t *values2replace(char *yunorole, char *rootname)
{
    json_t *jn_values = json_object();
    char yunorole_[256] = "yunorole";
    char rootname_[256] = "rootname";

    add_value(jn_values, yunorole, yunorole_);

    int list_size;
    const char **names = split2(rootname, "|, ", &list_size);

    for(int i=0; i<list_size; i++) {
        add_value(jn_values, (char *)(*(names +i)), rootname_);
    }

    split_free2(names);

    return jn_values;
}

/***************************************************************************
 *  Make a skeleton
 ***************************************************************************/
int make_skeleton(char *source_path, char *destination_path_, char *yunorole, char *rootname)
{
    char destination_path[PATH_MAX];
    snprintf(destination_path, sizeof(destination_path), "%s", destination_path_);
    char *destination_dir = dirname(destination_path);

    json_t *toreplace = values2replace(yunorole, rootname);
    print_json2("", toreplace);

    if(access(destination_dir,0)!=0) {
        printf("Creating directory: %s\n", destination_dir);
        if(mkdir(destination_dir, S_ISUID|S_ISGID | S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH)<0) {
            printf("ERROR: cannot create destination ('%s'), %s\n", destination_dir, strerror(errno));
            exit(-1);
        }
    } else if(!is_directory(destination_dir)) {
        printf("ERROR: destination ('%s') is not a directory\n", destination_dir);
        exit(-1);
    }

    printf("Creating project: %s\n", destination_path_);
    if(mkdir(destination_path_, S_ISUID|S_ISGID | S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH)<0) {
        printf("ERROR: cannot create project ('%s'), %s\n", destination_path_, strerror(errno));
        exit(-1);
    }

    /*
     *  Source path and destination path already created
     */
    clone_tree_dir(destination_path_, source_path, toreplace);
    json_decref(toreplace);

    return 0;
}
