/****************************************************************************
 *          COPY_DIR.C
 *          Copy a directory
 *
 *          Copyright (c) 2015-2022 Niyamaka.
 *          All Rights Reserved.
 *
 *
 *  sudo apt-get install libpcre3-dev
 *
 ****************************************************************************/
#define _POSIX_SOURCE
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <pcre.h>
#include <errno.h>
#include <12_walkdir.h>
#include <fcntl.h>
#include <dirent.h>
#include <jansson.h>
#include <00_replace_string.h>
#include "clone_tree_dir.h"

#define Color_Off "\033[0m"       // Text Reset

// Regular Colors
#define Black "\033[0;30m"        // Black
#define Red "\033[0;31m"          // Red
#define Green "\033[0;32m"        // Green
#define Yellow "\033[0;33m"       // Yellow
#define Blue "\033[0;34m"         // Blue
#define Purple "\033[0;35m"       // Purple
#define Cyan "\033[0;36m"         // Cyan
#define White "\033[0;37m"        // White

// Bold
#define BBlack "\033[1;30m"       // Black
#define BRed "\033[1;31m"         // Red
#define BGreen "\033[1;32m"       // Green
#define BYellow "\033[1;33m"      // Yellow
#define BBlue "\033[1;34m"        // Blue
#define BPurple "\033[1;35m"      // Purple
#define BCyan "\033[1;36m"        // Cyan
#define BWhite "\033[1;37m"       // White

// Background
#define On_Black "\033[40m"       // Black
#define On_Red "\033[41m"         // Red
#define On_Green "\033[42m"       // Green
#define On_Yellow "\033[43m"      // Yellow
#define On_Blue "\033[44m"        // Blue
#define On_Purple "\033[45m"      // Purple
#define On_Cyan "\033[46m"        // Cyan
#define On_White "\033[47m"       // White

/***************************************************************************
 *      Constants
 ***************************************************************************/

/***************************************************************************
 *  Busca en str las +clave+ y sustituye la clave con el valor
 *  de dicha clave en el dict jn_values
 *  Busca tb "_tmpl$" y elimínalo.
 ***************************************************************************/
int render_string(char *rendered_str, int rendered_str_size, char *str, json_t *jn_values)
{
    pcre *re;
    const char *error;
    int erroffset;
    int ovector[100];

    snprintf(rendered_str, rendered_str_size, "%s", str);

    const char *old_name; json_t *jn_value;
    json_object_foreach(jn_values, old_name, jn_value) {
        const char *new_name = json_string_value(jn_value);
        if(empty_string(new_name)) {
            continue;
        }
        char pattern[NAME_MAX];
        snprintf(pattern, sizeof(pattern), "(%s+?)", old_name);
        re = pcre_compile(
            pattern,        /* the pattern */
            0,              /* default options */
            &error,         /* for error message */
            &erroffset,     /* for error offset */
            0               /* use default character tables */
        );
        if(!re) {
            fprintf(stderr, "pcre_compile failed (offset: %d), %s\n", erroffset, error);
            exit(-1);
        }

        int rc;
        int offset = 0;
        int len = (int)strlen(str);
        while (offset < len && (rc = pcre_exec(re, 0, str, len, offset, 0, ovector, sizeof(ovector))) >= 0)
        {
            for(int i = 0; i < rc; ++i)
            {
                int macro_len = ovector[2*i+1] - ovector[2*i];
                //printf("%2d: %.*s\n", i, macro_len, str + ovector[2*i]);
                char macro[NAME_MAX]; // enough of course
                snprintf(macro, sizeof(macro), "%.*s", macro_len, str + ovector[2*i]);
                const char *value = json_string_value(json_object_get(jn_values, macro));
                char * new_value = replace_string(rendered_str, macro, value);
                snprintf(rendered_str, rendered_str_size, "%s", new_value);
                free(new_value);
            }
            offset = ovector[1];
        }
        free(re);
    }

    return 0;
}

/***************************************************************************
 *  Lee el fichero src_path línea a línea, render la línea,
 *  y sálvala en dst_path
 ***************************************************************************/
char old_line[64*1024];
char new_line[64*1024];

int render_file(char *dst_path, char *src_path, json_t *jn_values)
{
    FILE *f = fopen(src_path, "r");
    if(!f) {
        fprintf(stderr, "ERROR Cannot open file %s\n", src_path);
        exit(-1);
    }

    if(access(dst_path, 0)==0) {
        fprintf(stderr, "ERROR File %s ALREADY EXISTS\n", dst_path);
        exit(-1);
    }

    FILE *fout = fopen(dst_path, "w");
    if(!fout) {
        printf("ERROR: cannot create '%s', %s\n", dst_path, strerror(errno));
        exit(-1);
    }
    printf("Creating filename: %s\n", dst_path);
    while(fgets(old_line, sizeof(old_line), f)) {
        render_string(new_line, sizeof(new_line), old_line, jn_values);
        fputs(new_line, fout);
    }
    fclose(f);
    fclose(fout);
    return 0;
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
 *
 ***************************************************************************/
int is_link(const char *path)
{
    struct stat path_stat;
    lstat(path, &path_stat);
    return S_ISLNK(path_stat.st_mode);
}

/***************************************************************************
 *  Create a new file (only to write)
 *  The use of this functions implies the use of 00_security.h's permission system:
 *  umask will be set to 0 and we control all permission mode.
 ***************************************************************************/
static int umask_cleared = 0;
int newfile(const char *path, int permission, int overwrite)
{
    int flags = O_CREAT|O_WRONLY|O_LARGEFILE;

    if(!umask_cleared) {
        umask(0);
        umask_cleared = 1;
    }

    if(overwrite)
        flags |= O_TRUNC;
    else
        flags |= O_EXCL;
    return open(path, flags, permission);
}

/***************************************************************************
 *
 ***************************************************************************/
int copy_link(
    const char* source,
    const char* destination
)
{
    char bf[PATH_MAX] = {0};
    if(readlink(source, bf, sizeof(bf))<0) {
        printf("%sERROR reading %s link%s\n\n", On_Red BWhite, source, Color_Off);
        return -1;
    }
    if(symlink(bf, destination)<0) {
        printf("%sERROR %d Cannot create symlink %s -> %s%s\n\n",
            On_Red BWhite, errno, destination, bf, Color_Off);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *    Elimina el caracter 'x' a la derecha.
 ***************************************************************************/
char *delete_right_char(char *s, char x)
{
    int l;

    l = strlen(s);
    if(l==0) {
        return s;
    }
    while(--l>=0) {
        if(*(s+l)==x) {
            *(s+l)='\0';
        } else {
            break;
        }
    }
    return s;
}

/***************************************************************************
 *    Elimina el caracter 'x' a la izquierda.
 ***************************************************************************/
char *delete_left_char(char *s, char x)
{
    int l;
    char c;

    if(strlen(s)==0) {
        return s;
    }

    l=0;
    while(1) {
        c= *(s+l);
        if(c=='\0'){
            break;
        }
        if(c==x) {
            l++;
        } else {
            break;
        }
    }
    if(l>0) {
        memmove(s,s+l,strlen(s)-l+1);
    }
    return s;
}

/***************************************************************************
 *
 ***************************************************************************/
char *build_path2(
    char *path,
    int pathsize,
    const char *dir1,
    const char *dir2
)
{
    snprintf(path, pathsize, "%s", dir1);
    delete_right_char(path, '/');

    if(dir2 && strlen(dir2)) {
        int l = strlen(path);
        snprintf(path+l, pathsize-l, "/");
        l = strlen(path);
        snprintf(path+l, pathsize-l, "%s", dir2);
        delete_left_char(path+l, '/');
        delete_right_char(path, '/');
    }
    return path;
}

/***************************************************************************
 *  Copy recursively the directory src to dst directory,
 *  rendering {{ }} of names of files and directories, and the content of the files,
 *  substituting by the value of jn_values
 ***************************************************************************/
int clone_tree_dir(
    const char *dst, // must exists
    const char *src, // must exists
    json_t *jn_values
)
{
    /*
     *  src must be a directory
     *  Get file of src directory, render the filename and his content, and copy to dst directory.
     *  When found a directory, render the name and make a new directory in dst directory
     *  and call recursively with these two new directories.
     */

    DIR *src_dir;
    DIR *dst_dir;
    struct dirent *entry;

    if (!(src_dir = opendir(src))) {
        printf("ERROR: cannot opendir source ('%s'), %s\n", src, strerror(errno));
        exit(-1);
    }
    if (!(entry = readdir(src_dir))) {
        printf("ERROR: cannot readdir source ('%s'), %s\n", src, strerror(errno));
        exit(-1);
    }

    if (!(dst_dir = opendir(dst))) {
        /*
         *  Create destination if not exist
         */
        printf("Creating directory: %s\n", dst);
        mkdir(dst, S_ISUID|S_ISGID | S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
        if (!(dst_dir = opendir(dst))) {
            printf("ERROR: cannot opendir destination ('%s'), %s\n", dst, strerror(errno));
            exit(-1);
        }
    }

    char dst_path[PATH_MAX];
    char src_path[PATH_MAX];
    char rendered_str[NAME_MAX];
    do {
        build_path2(src_path, sizeof(src_path), src, entry->d_name);

        render_string(rendered_str, sizeof(rendered_str), entry->d_name, jn_values);
        build_path2(dst_path, sizeof(dst_path), dst, rendered_str);

        if (is_link(src_path)) {
            copy_link(src_path, dst_path);

        } else if (is_directory(src_path)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            clone_tree_dir(dst_path, src_path, jn_values);

        } else if (is_regular_file(src_path)) {
            render_file(dst_path, src_path, jn_values);
        }

    } while ((entry = readdir(src_dir)));

    closedir(src_dir);
    closedir(dst_dir);

    return 0;
}
