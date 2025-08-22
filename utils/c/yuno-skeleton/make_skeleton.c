/****************************************************************************
 *          MAKE_SKELETON.C
 *          Make a skeleton
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <argp-standalone.h>
#include <regex.h>
#include <jansson.h>
#include <helpers.h>
#include "make_skeleton.h"
#include "tmpl_dir.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/
json_t *find_skeleton(
    char *fullname,
    int fullname_size,
    const char* base,
    const char *file,
    const char *skeleton);

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
int indent = 0;


/****************************************************************************
 *
 ****************************************************************************/
PRIVATE int _walk_tree(
    const char *root_dir,
    regex_t *reg,
    void *user_data,
    wd_option opt,
    int level,
    walkdir_cb cb)
{
    struct dirent *dent;
    DIR *dir;
    struct stat st;
    wd_found_type type;
    level++;
    int index=0;

    if (!(dir = opendir(root_dir))) {
        printf("Cannot open '%s' directory, error '%s'\n", root_dir, strerror(errno));
        exit(-1);
    }

    while ((dent = readdir(dir))) {
        char *dname = dent->d_name;
        if (!strcmp(dname, ".") || !strcmp(dname, ".."))
            continue;
        if (!(opt & WD_HIDDENFILES) && dname[0] == '.')
            continue;

        int len = strlen(root_dir) + 2 + strlen(dname);
        char *path = malloc(len);
        if(!path) {
            printf("No memory, %s\n", strerror(errno));
            exit(-1);
        }
        memset(path, 0, len);
        strncpy(path, root_dir, len-1);
        strcat(path, "/");
        strcat(path, dname);

        if(stat(path, &st) == -1) {
            printf("Stat('%s') failed: %s\n", path, strerror(errno));
            free(path);
            continue;
        }

        type = 0;
        if(S_ISDIR(st.st_mode)) {
            /* recursively follow dirs */
            if((opt & WD_RECURSIVE)) {
                _walk_tree(path, reg, user_data, opt, level, cb);
            }
            if ((opt & WD_MATCH_DIRECTORY)) {
                type = WD_TYPE_DIRECTORY;
            }
        } else if(S_ISREG(st.st_mode)) {
            if((opt & WD_MATCH_REGULAR_FILE)) {
                type = WD_TYPE_REGULAR_FILE;
            }

        } else if(S_ISFIFO(st.st_mode)) {
            if((opt & WD_MATCH_PIPE)) {
                type = WD_TYPE_PIPE;
            }
        } else if(S_ISLNK(st.st_mode)) {
            if((opt & WD_MATCH_SYMBOLIC_LINK)) {
                type = WD_TYPE_SYMBOLIC_LINK;
            }
        } else if(S_ISSOCK(st.st_mode)) {
            if((opt & WD_MATCH_SOCKET)) {
                type = WD_TYPE_SOCKET;
            }
        } else {
            // type not implemented
            type = 0;
        }

        if(type) {
            if (regexec(reg, dname, 0, 0, 0)==0) {
                if(!(cb)(0, user_data, type, path, root_dir, dname, level, index)) {
                    // returning FALSE: don't want continue traverse
                    break;
                }
                index++;
            }
        }
        free(path);
    }
    closedir(dir);
    return 0;
}

/****************************************************************************
 *  Walk directory tree
 *  Se matchea un único pattern a todo lo encontrado.
 ****************************************************************************/
PRIVATE int mywalk_dir_tree(
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    walkdir_cb cb,
    void *user_data)
{
    regex_t r;

    if(regcomp(&r, pattern, REG_EXTENDED | REG_NOSUB)!=0) {
        printf("regcomp('%s') failed\n", pattern);
        return -1;
    }

    int ret = _walk_tree(root_dir, &r, user_data, opt, 0, cb);
    regfree(&r);
    return ret;
}

/***************************************************************************
 *  Input values
 ***************************************************************************/
static int input_value(char *bf, int bfsize, const char *default_value, int testing)
{
    bf[0] = 0;
    if(!testing) {
        char *x = fgets(bf, bfsize, stdin);
        if(x) {} // avoid warning
    } else{
        strncpy(bf, "aa", bfsize-1);
    }
    int len = (int)strlen(bf);
    if(len > 0 && bf[len-1]=='\n') {
        bf[len-1] = 0;
        len--;
    }
    if(!len && default_value) {
        strncpy(bf, default_value, bfsize-1);
    }
    return 0;
}

/***************************************************************************
 *  Input values for skeleton
 ***************************************************************************/

/*
 *
En yunotemplate es:

Enter yuno role (maximum 15 characters): sdfs
Enter yuno name ['']:
Enter root gobj name ['root']:
Enter version (Version) ['1.0.0']:
Enter description (One-line description of the package) ['']:
Enter author (Author name) ['']:
Enter author_email (Author email) ['']:
Enter license_name (License name) ['']:


Enter gclass name: dd
Enter description (One-line description of the package) ['']: dd
Enter author (Author name) ['']: dd
*/
static json_t *input_vars_values(const char *type, json_t *jn_vars, int testing)
{
    json_t *jn_values = json_object();
    char bf[120];
    char root_name[120];
    size_t index;
    json_t *jn_var;

    time_t timeval;
    struct tm *tp;
    time (&timeval);
    tp = gmtime(&timeval);
    char year[23];
    snprintf(year, sizeof(year), "%d", tp->tm_year + 1900);
    json_object_set_new(jn_values, "__year__", json_string(year));

    int len = 0;
    if(strcasecmp(type, "yuno")==0) {
        do {
            printf("Enter yuno role (maximum 15 characters): ");
            input_value(bf, sizeof(bf), "", testing);
            if(testing) {
                break;
            }
            len = (int)strlen(bf);
        } while(len < 1 || len > 15);
        lower(bf);
        snprintf(root_name, sizeof(root_name), "%s", bf);
        json_object_set_new(jn_values, "yunorole", json_string(bf));
        capitalize(bf);
        json_object_set_new(jn_values, "Yunorole", json_string(bf));
        upper(bf);
        json_object_set_new(jn_values, "YUNOROLE", json_string(bf));

        printf("Enter root gobj name ['%s']: ", root_name);
        input_value(bf, sizeof(bf), bf, testing);
        len = (int)strlen(bf);
        if(len == 0) {
            snprintf(bf, sizeof(bf), "%s", root_name);
        }

        lower(bf);
        json_object_set_new(jn_values, "rootname", json_string(bf));
        capitalize(bf);
        json_object_set_new(jn_values, "Rootname", json_string(bf));
        upper(bf);
        json_object_set_new(jn_values, "ROOTNAME", json_string(bf));

    } else if(strcasecmp(type, "utility")==0) {
        do {
            printf("Enter utility name: ");
            input_value(bf, sizeof(bf), "", testing);
            if(testing) {
                break;
            }
            len = (int)strlen(bf);
        } while(len < 1 || len > 15);
        lower(bf);
        json_object_set_new(jn_values, "utility", json_string(bf));
        capitalize(bf);
        json_object_set_new(jn_values, "Utility", json_string(bf));
        upper(bf);
        json_object_set_new(jn_values, "UTILITY", json_string(bf));

    } else {
        do {
            printf("Enter gclass name: ");
            input_value(bf, sizeof(bf), "", testing);
            len = (int)strlen(bf);
        } while(len < 1);
        lower(bf);
        json_object_set_new(jn_values, "rootname", json_string(bf));
        capitalize(bf);
        json_object_set_new(jn_values, "Rootname", json_string(bf));
        upper(bf);
        json_object_set_new(jn_values, "ROOTNAME", json_string(bf));
    }

    json_array_foreach(jn_vars, index, jn_var) {
        if(!json_is_object(jn_var)) {
            continue;
        }
        const char *var = json_string_value(json_object_get(jn_var, "var"));
        const char *default_value = json_string_value(json_object_get(jn_var, "default"));
        const char *message = json_string_value(json_object_get(jn_var, "message"));

        printf("%s ['%s']: ", message, default_value);
        input_value(bf, sizeof(bf), default_value, testing);
        json_object_set_new(jn_values, var, json_string(bf));

    }
    return jn_values;
}

PRIVATE BOOL find_skeletons_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *name,             // dname[255]
    int level,
    wd_option opt)
{
    struct find_sk_s *find_sk = user_data;

    printf("%*.*sScanning '%s'\n",
        2*indent,
        2*indent,
        "                                             ",
        fullpath
    );

    find_sk->jn_config = find_skeleton(
        find_sk->fullname,
        find_sk->fullname_size,
        fullpath,
        find_sk->file,
        find_sk->skeleton
    );
    if(find_sk->jn_config) {
        return FALSE;
    } else {
        return TRUE; // continue traverse tree
    }
}

/***************************************************************************
 *  Find a skeleton
 ***************************************************************************/
json_t *find_skeleton(
    char *fullname,
    int fullname_size,
    const char* base,
    const char *file,
    const char *skeleton)
{
    size_t index;
    json_t *jn_skeleton;

    char path[256]; // json config file
    snprintf(path, sizeof(path), "%s/%s", base, file);
    if(access(path, 0)!=0) {
        if(access(base, 0)== 0) {

            struct find_sk_s find_sk;
            find_sk.fullname = fullname;
            find_sk.fullname_size = fullname_size;
            find_sk.base = base;
            find_sk.file = file;
            find_sk.skeleton = skeleton;
            find_sk.jn_config = 0;
            mywalk_dir_tree(base, ".*", WD_MATCH_DIRECTORY, find_skeletons_cb, (void *)&find_sk);

            return find_sk.jn_config;

        } else {
            fprintf(stderr, "Cannot find %s file\n", path);
            exit(-1);
        }
    }
    json_error_t error;
    size_t flags = 0;

    json_t *jn_skeletons = json_load_file(path, flags, &error);
    if(!jn_skeletons) {
        fprintf(stderr, "ERROR loading json file %s: %s\n", path, error.text);
        exit(-1);
    }

    json_array_foreach(jn_skeletons, index, jn_skeleton) {
        if(!json_is_object(jn_skeleton)) {
            continue;
        }
        const char *name_ = json_string_value(json_object_get(jn_skeleton, "name"));
        const char *type = json_string_value(json_object_get(jn_skeleton, "type"));
        if(strcasecmp(type, "skeleton-dir")==0) {
            snprintf(path, sizeof(path), "%s/%s", base, name_);
            return find_skeleton(fullname, fullname_size, path, file, skeleton);

        } else {
            if(strcmp(name_, skeleton)==0) {
                snprintf(fullname, fullname_size, "%s/%s", base, skeleton);
                if(access(fullname, 0)!=0) {
                    fprintf(stderr, "Cannot find %s directory\n", fullname);
                    exit(-1);
                }
                return jn_skeleton;
            }
        }
    }
    return 0;
}

/***************************************************************************
 *  List skeletons in *base* directory
 ***************************************************************************/
PRIVATE BOOL list_skeletons_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *name,             // dname[255]
    int level,
    wd_option opt)
{
    const char *file = user_data;

//     printf("%*.*sScanning '%s'\n",
//         2*indent,
//         2*indent,
//         "                                             ",
//         fullpath
//     );
    list_skeletons(fullpath, file);

    return TRUE; // continue traverse tree
}

/***************************************************************************
 *  List skeletons in *base* directory
 ***************************************************************************/
int list_skeletons(const char* base, const char *file)
{
    char path[256];

    snprintf(path, sizeof(path), "%s/%s", base, file);
    if(access(path, 0)!=0) {
        if(access(base, 0)== 0) {
            return mywalk_dir_tree(base, ".*", WD_MATCH_DIRECTORY, list_skeletons_cb, (void *)file);
        } else {
            fprintf(stderr, "Cannot find %s file\n", path);
            exit(-1);
        }
    }
    json_error_t error;
    size_t flags = 0;

    json_t *jn_skeletons = json_load_file(path, flags, &error);
    if(!jn_skeletons) {
        fprintf(stderr, "ERROR loading json file %s: %s\n", path, error.text);
        exit(-1);
    }

    size_t index;
    json_t *jn_skeleton;

    indent++;
    printf("\n%*.*sDirectory '%s'\n\n",
        2*indent,
        2*indent,
        "                                             ",
        base
    );
    json_array_foreach(jn_skeletons, index, jn_skeleton) {
        if(!json_is_object(jn_skeleton)) {
            continue;
        }
        const char *name = json_string_value(json_object_get(jn_skeleton, "name"));
        const char *summary = json_string_value(json_object_get(jn_skeleton, "summary"));
        const char *type = json_string_value(json_object_get(jn_skeleton, "type"));
        if(strcasecmp(type, "skeleton-dir")==0) {
            snprintf(path, sizeof(path), "%s/%s", base, name);
            list_skeletons(path, file);

        } else {
            printf("%*.*s%-22s %-12s %s\n",
                2*indent,
                2*indent,
                "                                             ",
                name,
                type,
                summary
            );
        }
    }
    indent--;
    printf("\n");

    json_decref(jn_skeletons);

    return 0;
}

/***************************************************************************
 *  Make a skeleton
 ***************************************************************************/
int make_skeleton(const char* base, const char* file, const char* skeleton)
{
    char fullname[256]; // directory with the skeleton

    json_t *jn_skeleton = find_skeleton(fullname, sizeof(fullname), base, file, skeleton);
    if(!jn_skeleton) {
        fprintf(stderr, "Skeleton '%s' NO FOUND\n", skeleton);
        exit(-1);
    }

    json_t *jn_vars = json_object_get(jn_skeleton, "vars");
    const char *type = json_string_value(json_object_get(jn_skeleton, "type"));
    json_t *jn_values = input_vars_values(type, jn_vars, 0);

    char dst_dir[80] = ".";
    if(strcasecmp(type, "yuno")==0) {
        const char *yunorole = json_string_value(json_object_get(jn_values, "yunorole"));
        if(!yunorole || !*yunorole) {
            printf("\nyunorole NULL\n");
            exit(-1);
        }
        strncpy(dst_dir, yunorole, sizeof(dst_dir)-1);
        if(access(dst_dir, 0)==0) {
            char temp[PATH_MAX]={0};
            char *x = realpath(".", temp);
            if(x) {} // avoid warning
            fprintf(stderr, "Directory of file '%s/%s' already exists\n",
                temp,
                yunorole
            );
            exit(-1);
        }
        mkdir(yunorole, S_ISUID|S_ISGID | S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
        if(access(dst_dir, 0)!=0) {
            fprintf(stderr, "Cannot create directory '%s'\n", yunorole);
            exit(-1);
        }
    } else if(strcasecmp(type, "utility")==0) {
        const char *utility = json_string_value(json_object_get(jn_values, "utility"));
        if(!utility || !*utility) {
            printf("\nutility name NULL\n");
            exit(-1);
        }
        strncpy(dst_dir, utility, sizeof(dst_dir)-1);
        if(access(dst_dir, 0)==0) {
            char temp[PATH_MAX]={0};
            char *x = realpath(".", temp);
            if(x) {} // avoid warning
            fprintf(stderr, "Directory of file '%s/%s' already exists\n",
                temp,
                utility
            );
            exit(-1);
        }
        mkdir(utility, S_ISUID|S_ISGID | S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
        if(access(dst_dir, 0)!=0) {
            fprintf(stderr, "Cannot create directory '%s'\n", utility);
            exit(-1);
        }
    }

    copy_dir(dst_dir, fullname, jn_values);

    json_decref(jn_values);
    json_decref(jn_skeleton);

    return 0;
}
