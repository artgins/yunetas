/****************************************************************************
 *          RENAME_GCLASS.C
 *          Rename gclass
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <dirent.h>
#include <sys/stat.h>
#include <argp.h>
#include <jansson.h>
#include <12_walkdir.h>
#include "render_file.h"

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
 *
 ***************************************************************************/
int is_regular_file(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

/***************************************************************************
 *
 ***************************************************************************/
int is_directory(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

/***************************************************************************
 *  Rename a gclass
 ***************************************************************************/
int rename_gclass(char *old_gclass_name, char *new_gclass_name)
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

    if(is_regular_file(new_puntoc)) {
        printf("File ALREADY exist: %s\n", new_puntoc);
        return -1;
    }
    if(is_regular_file(new_puntoh)) {
        printf("File ALREADY exist: %s\n", new_puntoh);
        return -1;
    }

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


    render_file(new_puntoh, old_puntoh, jn_values);
    render_file(new_puntoc, old_puntoc, jn_values);

    json_decref(jn_values);

    printf("Remember to change file headers (copyright, year, description)\n");

    return 0;
}
