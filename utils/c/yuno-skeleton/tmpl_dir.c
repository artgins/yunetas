/****************************************************************************
 *          MAKE_SKELETON.C
 *          Make a skeleton
 *
 *          Copyright (c) 2015 Niyamaka.
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
#include <errno.h>
#include <fcntl.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
  #include <copyfile.h>
#elif defined(__linux__)
  #include <sys/sendfile.h>
#endif
#include <dirent.h>
#include <jansson.h>
#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 8
#include <helpers.h>
#include <pcre2.h>
#include <helpers.h>
#include "tmpl_dir.h"

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
 *  Busca en str las {{clave}} y sustituye la clave con el valor
 *  de dicha clave en el dict jn_values
 ***************************************************************************/
// version with pcre, changed to pcre2
// int render_string(char *rendered_str, int rendered_str_size, char *str, json_t *jn_values)
// {
//     pcre *re;
//     const char *error;
//     int erroffset;
//     int ovector[100];
//
//     re = pcre_compile(
//              "(\\{\\{.+?\\}\\})",   /* the pattern */
//              0,                     /* default options */
//              &error,                /* for error message */
//              &erroffset,            /* for error offset */
//              0                      /* use default character tables */
//     );
//     if(!re) {
//         fprintf(stderr, "pcre_compile failed (offset: %d), %s\n", erroffset, error);
//         exit(-1);
//     }
//
//     snprintf(rendered_str, rendered_str_size, "%s", str);
//
//     int rc;
//     unsigned int offset = 0;
//     unsigned int len = strlen(str);
//     while (offset < len && (rc = pcre_exec(re, 0, str, len, offset, 0, ovector, sizeof(ovector))) >= 0)
//     {
//         for(int i = 0; i < rc; ++i)
//         {
//             int macro_len = ovector[2*i+1] - ovector[2*i];
//             //printf("%2d: %.*s\n", i, macro_len, str + ovector[2*i]);
//             char macro[256]; // enough of course
//             char rendered[256];
//             snprintf(macro, sizeof(macro), "%.*s", macro_len, str + ovector[2*i]);
//             char key[256];
//             snprintf(key, sizeof(key), "%.*s", macro_len-4, str + ovector[2*i] + 2);
//
//             const char *value = json_string_value(json_object_get(jn_values, key));
//             if(!value)
//                 value = "";
//             snprintf(rendered, sizeof(rendered), "%s", value);
//
//             char * new_value = replace_string(rendered_str, macro, rendered);
//             snprintf(rendered_str, rendered_str_size, "%s", new_value);
//             free(new_value);
//         }
//         offset = ovector[1];
//     }
//     free(re);
//
//     return 0;
// }

int render_string(char *rendered_str, int rendered_str_size, char *str, json_t *jn_values)
{
    pcre2_code *re;
    PCRE2_SPTR pattern = (PCRE2_SPTR)"(\\{\\{.+?\\}\\})";
    int errornumber;
    PCRE2_SIZE erroroffset;

    re = pcre2_compile(
        pattern,                  /* the pattern */
        PCRE2_ZERO_TERMINATED,    /* pattern is zero-terminated */
        0,                        /* default options */
        &errornumber,             /* for error code */
        &erroroffset,             /* for error offset */
        NULL                      /* use default compile context */
    );
    if(!re) {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
        fprintf(stderr, "pcre2_compile failed at offset %d: %s\n", (int)erroroffset, buffer);
        exit(-1);
    }

    snprintf(rendered_str, rendered_str_size, "%s", str);

    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);
    size_t offset = 0;
    size_t len = strlen(str);

    while(offset < len) {
        int rc = pcre2_match(
            re,
            (PCRE2_SPTR)str,
            len,
            offset,
            0,
            match_data,
            NULL
        );

        if(rc <= 0) {
            break;
        }

        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);

        for(int i = 0; i < rc; ++i) {
            int macro_len = (int)(ovector[2*i+1] - ovector[2*i]);
            char macro[256];
            char rendered[256];
            snprintf(macro, sizeof(macro), "%.*s", macro_len, str + ovector[2*i]);

            char key[256];
            snprintf(key, sizeof(key), "%.*s", macro_len - 4, str + ovector[2*i] + 2);

            const char *value = json_string_value(json_object_get(jn_values, key));
            if(!value) {
                value = "";
            }
            snprintf(rendered, sizeof(rendered), "%s", value);

            char *new_value = replace_string(rendered_str, macro, rendered);
            snprintf(rendered_str, rendered_str_size, "%s", new_value);
            free(new_value);
        }
        offset = ovector[1];  /* move past the last match */
    }

    pcre2_match_data_free(match_data);
    pcre2_code_free(re);

    return 0;
}

/***************************************************************************
 *  Busca en str las +clave+ y sustituye la clave con el valor
 *  de dicha clave en el dict jn_values
 *  Busca tb "_tmpl$" y elimínalo.
 ***************************************************************************/
int render_filename(char *rendered_str, int rendered_str_size, char *str, json_t *jn_values)
{
    // TODO
    // pcre *re;
    // const char *error;
    // int erroffset;
    // int ovector[100];
    //
    // re = pcre_compile(
    //          "(\\+.+?\\+)", /* the pattern */
    //          0,             /* default options */
    //          &error,        /* for error message */
    //          &erroffset,    /* for error offset */
    //          0              /* use default character tables */
    // );
    // if(!re) {
    //     fprintf(stderr, "pcre_compile failed (offset: %d), %s\n", erroffset, error);
    //     exit(-1);
    // }
    //
    // snprintf(rendered_str, rendered_str_size, "%s", str);
    //
    // int rc;
    // unsigned int offset = 0;
    // unsigned int len = strlen(str);
    // while (offset < len && (rc = pcre_exec(re, 0, str, len, offset, 0, ovector, sizeof(ovector))) >= 0)
    // {
    //     for(int i = 0; i < rc; ++i)
    //     {
    //         int macro_len = ovector[2*i+1] - ovector[2*i];
    //         //printf("%2d: %.*s\n", i, macro_len, str + ovector[2*i]);
    //         char macro[256]; // enough of course
    //         char rendered[256];
    //         snprintf(macro, sizeof(macro), "%.*s", macro_len, str + ovector[2*i]);
    //         char key[256];
    //         snprintf(key, sizeof(key), "%.*s", macro_len-2, str + ovector[2*i] + 1);
    //
    //         const char *value = json_string_value(json_object_get(jn_values, key));
    //         snprintf(rendered, sizeof(rendered), "%s", value?value:"");
    //
    //         char * new_value = replace_string(rendered_str, macro, rendered);
    //         snprintf(rendered_str, rendered_str_size, "%s", new_value);
    //         free(new_value);
    //     }
    //     offset = ovector[1];
    // }
    // free(re);
    //
    // len = strlen(rendered_str);
    // if(len > 5 && strcmp(rendered_str+len-5, "_tmpl")==0) {
    //     *(rendered_str+len-5) = 0;
    // }

    return 0;
}

/***************************************************************************
 *  Lee el fichero src_path línea a línea, render la línea,
 *  y sálvala en dst_path
 ***************************************************************************/
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
    char line[4*1024];
    char rendered[4*1024];
    while(fgets(line, sizeof(line), f)) {
        render_string(rendered, sizeof(rendered), line, jn_values);
        fputs(rendered, fout);
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
 *  Copy file in kernel mode.
 *  http://stackoverflow.com/questions/2180079/how-can-i-copy-a-file-on-unix-using-c
 ***************************************************************************/
int copyfile(
    const char* source,
    const char* destination,
    int permission,
    int overwrite)
{
    int input, output;
    if ((input = open(source, O_RDONLY)) == -1) {
        return -1;
    }
    if ((output = newfile(destination, permission, overwrite)) == -1) {
        // error already logged
        close(input);
        return -1;
    }

    //Here we use kernel-space copying for performance reasons
#if defined(__APPLE__) || defined(__FreeBSD__)
    //fcopyfile works on FreeBSD and OS X 10.5+
    int result = fcopyfile(input, output, 0, COPYFILE_ALL);
#elif defined(__linux__)
    //sendfile will work with non-socket output (i.e. regular file) on Linux 2.6.33+
    off_t bytesCopied = 0;
    struct stat fileinfo = {0};
    fstat(input, &fileinfo);
    int result = sendfile(output, input, &bytesCopied, fileinfo.st_size);
#else
    ssize_t nread;
    int result = 0;
    int error = 0;
    char buf[4096];

    while (nread = read(input, buf, sizeof buf), nread > 0 && !error) {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(output, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                error = 1;
                result = -1;
                break;
            }
        } while (nread > 0);
    }

#endif

    close(input);
    close(output);

    return result;
}

/***************************************************************************
 *  Copy recursively the directory src to dst directory,
 *  rendering {{ }} of names of files and directories, and the content of the files,
 *  substituting by the value of jn_values
 ***************************************************************************/
int copy_dir(const char *dst, const char *src, json_t *jn_values)
{
    /*
     *  src must be a directory
     *  Get file of src directory, render the filename and his content, and copy to dst directory.
     *  When found a directory, render the name and make a new directory in dst directory
     *  and call recursively with these two new directories.
     */

    printf("Copying '%s' in '%s'\n", src, dst);

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

    int len;
    char dst_path[1024];
    char src_path[1024];
    char rendered_str[80];
    do {
        len = snprintf(src_path, sizeof(src_path)-1, "%s/%s", src, entry->d_name);
        src_path[len] = 0;

        render_filename(rendered_str, sizeof(rendered_str), entry->d_name, jn_values);
        len = snprintf(dst_path, sizeof(dst_path)-1, "%s/%s", dst, rendered_str);
        dst_path[len] = 0;


        if (is_link(src_path)) {
            copy_link(src_path, dst_path);

        } else if (is_directory(src_path)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            copy_dir(dst_path, src_path, jn_values);
        } else if (is_regular_file(src_path)) {
            if(strstr(src_path, "_tmpl")) {
                render_file(dst_path, src_path, jn_values);
            } else {
                struct stat path_stat;
                stat(src_path, &path_stat);

                copyfile(src_path, dst_path, path_stat.st_mode, 1);
            }
        }

    } while ((entry = readdir(src_dir)));

    closedir(src_dir);
    closedir(dst_dir);

    return 0;
}
