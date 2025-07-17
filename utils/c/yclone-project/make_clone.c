/****************************************************************************
 *          MAKE_CLONE.C
 *          Make a skeleton
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <argp.h>
#include <regex.h>
#include <jansson.h>
#include <12_walkdir.h>
#include "make_clone.h"
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
//PRIVATE int walk_tree(
//    const char *root_dir,
//    regex_t *reg,
//    void *user_data,
//    wd_option opt,
//    int level,
//    walkdir_cb cb)
//{
//    struct dirent *dent;
//    DIR *dir;
//    struct stat st;
//    wd_found_type type;
//    level++;
//    int index=0;
//
//    if (!(dir = opendir(root_dir))) {
//        printf("Cannot open '%s' directory, error '%s'\n", root_dir, strerror(errno));
//        exit(-1);
//    }
//
//    while ((dent = readdir(dir))) {
//        char *dname = dent->d_name;
//        if (!strcmp(dname, ".") || !strcmp(dname, ".."))
//            continue;
//        if (!(opt & WD_HIDDENFILES) && dname[0] == '.')
//            continue;
//
//        int len = strlen(root_dir) + 2 + strlen(dname);
//        char *path = malloc(len);
//        if(!path) {
//            printf("No memory, %s\n", strerror(errno));
//            exit(-1);
//        }
//        memset(path, 0, len);
//        strncpy(path, root_dir, len-1);
//        strcat(path, "/");
//        strcat(path, dname);
//
//        if(stat(path, &st) == -1) {
//            printf("Stat('%s') failed: %s\n", path, strerror(errno));
//            free(path);
//            continue;
//        }
//
//        type = 0;
//        if(S_ISDIR(st.st_mode)) {
//            /* recursively follow dirs */
//            if((opt & WD_RECURSIVE)) {
//                walk_tree(path, reg, user_data, opt, level, cb);
//            }
//            if ((opt & WD_MATCH_DIRECTORY)) {
//                type = WD_TYPE_DIRECTORY;
//            }
//        } else if(S_ISREG(st.st_mode)) {
//            if((opt & WD_MATCH_REGULAR_FILE)) {
//                type = WD_TYPE_REGULAR_FILE;
//            }
//
//        } else if(S_ISFIFO(st.st_mode)) {
//            if((opt & WD_MATCH_PIPE)) {
//                type = WD_TYPE_PIPE;
//            }
//        } else if(S_ISLNK(st.st_mode)) {
//            if((opt & WD_MATCH_SYMBOLIC_LINK)) {
//                type = WD_TYPE_SYMBOLIC_LINK;
//            }
//        } else if(S_ISSOCK(st.st_mode)) {
//            if((opt & WD_MATCH_SOCKET)) {
//                type = WD_TYPE_SOCKET;
//            }
//        } else {
//            // type not implemented
//            type = 0;
//        }
//
//        if(type) {
//            if (regexec(reg, dname, 0, 0, 0)==0) {
//                if(!(cb)(user_data, type, path, root_dir, dname, level, index)) {
//                    // returning FALSE: don't want continue traverse
//                    break;
//                }
//                index++;
//            }
//        }
//        free(path);
//    }
//    closedir(dir);
//    return 0;
//}

///****************************************************************************
// *  Walk directory tree
// *  match only one pattern a todo lo encontrado.
// ****************************************************************************/
//PRIVATE int mywalk_dir_tree(
//    const char *root_dir,
//    const char *pattern,
//    wd_option opt,
//    walkdir_cb cb,
//    void *user_data)
//{
//    regex_t r;
//
//    if(regcomp(&r, pattern, REG_EXTENDED | REG_NOSUB)!=0) {
//        printf("regcomp('%s') failed\n", pattern);
//        return -1;
//    }
//
//    int ret = walk_tree(root_dir, &r, user_data, opt, 0, cb);
//    regfree(&r);
//    return ret;
//}
//
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
json_t *values2replace(char *old_name, char *new_name)
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
 *  Make a skeleton
 ***************************************************************************/
int make_clone(char *source_path, char *destination_path_, char *source_keyword, char *destination_keyword)
{
    char destination_path[PATH_MAX];
    snprintf(destination_path, sizeof(destination_path), "%s", destination_path_);
    char *destination_dir = dirname(destination_path);
    char *project = basename(destination_path);

    if(empty_string(destination_keyword)) {
        destination_keyword = project;
    }

    json_t *toreplace = values2replace(source_keyword, destination_keyword);

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
