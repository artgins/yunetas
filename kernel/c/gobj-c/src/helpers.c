/****************************************************************************
 *              helpers.c
 *
 *              Several helpers
 *
 *              Copyright (c) 2014,2023 Niyamaka.
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <locale.h>
#include <sys/file.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <regex.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>

#include <yuneta_config.h>  /* don't remove */

#ifdef ESP_PLATFORM
#include "esp_system.h" // For esp_fill_random()
#elif __linux__
#include <sys/random.h> // For getrandom()
#include <syslog.h>
#include <backtrace.h>
#endif

#include <arpa/inet.h>   // For htonl and htons
#include <endian.h>      // For __BYTE_ORDER, __LITTLE_ENDIAN, etc.
// Fallback definitions if <endian.h> is not available
#ifndef __BYTE_ORDER
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
#define __BYTE_ORDER __BYTE_ORDER__
#define __LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define __BIG_ENDIAN __ORDER_BIG_ENDIAN__
#else
#error "Byte order macros are not defined"
#endif
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
  #include <copyfile.h>
#elif defined(__linux__)
  #include <sys/sendfile.h>
#endif

#ifdef ESP_PLATFORM
    #include <esp_log.h>
    #define fstat64 fstat
    #define stat64 stat
    #define syslog(priority, format, ... ) ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, "yuneta", format, ##__VA_ARGS__)
#endif

#include "kwid.h"
#include "helpers.h"

extern void jsonp_free(void *ptr);

/*****************************************************************
 *     Constants
 *****************************************************************/
#define _

#define PATH_UUID "/yuneta/store/agentdb/uuid"

/*****************************************************************
 *     Prototypes
 *****************************************************************/
PRIVATE int _walk_tree(
    hgobj gobj,
    const char *root_dir,
    regex_t *reg,
    void *user_data,
    wd_option opt,
    int level,
    walkdir_cb cb
);

/*****************************************************************
 *     Data
 *****************************************************************/
PRIVATE BOOL umask_cleared = FALSE;
PRIVATE char node_uuid_[64] = {0}; // uuid of the node




                    /*------------------------------------*
                     *  ### File System
                     *------------------------------------*/




/***************************************************************************
 *  Create a new directory
 *  The use of this functions implies the use of 00_security.h's permission system:
 *  umask will be set to 0 and we control all permission mode.
 ***************************************************************************/
PUBLIC int newdir(const char *path, int xpermission)
{
    if(!umask_cleared) {
        umask(0);
        umask_cleared = TRUE;
    }
    return mkdir(path, xpermission);
}

/***************************************************************************
 *  Create a new file for wr/rd (WARNING before was only to wr)
 *  The use of this functions implies the use of 00_security.h's permission system:
 *  umask will be set to 0 and we control all permission mode.
 ***************************************************************************/
PUBLIC int newfile(const char *path, int rpermission, BOOL overwrite)
{
    int flags = O_CREAT|O_RDWR|O_LARGEFILE;

    if(!umask_cleared) {
        umask(0);
        umask_cleared = TRUE;
    }

    if(overwrite)
        flags |= O_TRUNC;
    else
        flags |= O_EXCL;

    return open(path, flags, rpermission);
}

/***************************************************************************
 *  Open a file as exclusive
 ***************************************************************************/
PUBLIC int open_exclusive(const char *path, int flags, int rpermission)
{
    if(!flags) {
        flags = O_RDWR|O_NOFOLLOW;
    }

    int fp = open(path, flags, rpermission);
    if(flock(fp, LOCK_EX|LOCK_NB)<0) {
        close(fp);
        return -1;
    }
    return fp;
}

/***************************************************************************
 *  Get size of file
 ***************************************************************************/
PUBLIC off_t filesize(const char *path)
{
    struct stat st;
    int ret = stat(path, &st);
    if(ret < 0) {
        return 0;
    }
    off_t size = st.st_size;
    return size;
}

/***************************************************************************
 *  Get size of file
 ***************************************************************************/
PUBLIC off_t filesize2(int fd)
{
    struct stat st;
    int ret = fstat(fd, &st);
    if(ret < 0) {
        return 0;
    }
    off_t size = st.st_size;
    return size;
}

/*****************************************************************
 *        lock file
 *****************************************************************/
PUBLIC int lock_file(int fd)
{
    struct flock fl;
    if(fd <= 0) {
        return -1;
    }
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    return fcntl(fd, F_SETLKW, &fl);
}

/*****************************************************************
 *        unlock file
 *****************************************************************/
PUBLIC int unlock_file(int fd)
{
    struct flock fl;
    if(fd <= 0) {
        return -1;
    }
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    return fcntl(fd, F_SETLKW, &fl);
}

/***************************************************************************
 *  Tell if path is a regular file
 ***************************************************************************/
PUBLIC BOOL is_regular_file(const char *path)
{
    struct stat buf;
    int ret = stat(path, &buf);
    if(ret < 0) {
        return FALSE;
    }
    return S_ISREG(buf.st_mode)?TRUE:FALSE;
}

/***************************************************************************
 *  Tell if path is a directory
 ***************************************************************************/
PUBLIC BOOL is_directory(const char *path)
{
    struct stat buf;
    int ret = stat(path, &buf);
    if(ret < 0) {
        return FALSE;
    }
    return S_ISDIR(buf.st_mode)?TRUE:FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC off_t file_size(const char *path)
{
    struct stat st;
    if(stat(path, &st)<0) {
        return 0;
    }
    off_t size = st.st_size;
    if(size < 0)
        size = 0;
    return size;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC mode_t file_permission(const char *path)
{
    struct stat st;
    if(stat(path, &st)<0) {
        return 0;
    }
    return st.st_mode & 0777;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL file_exists(const char *directory, const char *filename)
{
    char full_path[PATH_MAX];
    build_path(full_path, sizeof(full_path), directory, filename, NULL);

    if(is_regular_file(full_path)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL subdir_exists(const char *directory, const char *subdir)
{
    char full_path[PATH_MAX];
    build_path(full_path, sizeof(full_path), directory, subdir, NULL);

    if(is_directory(full_path)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int file_remove(const char *directory, const char *filename)
{
    char full_path[PATH_MAX];
    build_path(full_path, sizeof(full_path), directory, filename, NULL);

    if(!is_regular_file(full_path)) {
        return -1;
    }
    return unlink(full_path);
}

/***************************************************************************
 *  Function to create directories recursively like "mkdir -p path"
 ***************************************************************************/
PUBLIC int mkrdir(const char *path, int xpermission)
{
    struct stat st;
    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;

    // Copy the path to a temporary buffer
    snprintf(tmp, sizeof(tmp),"%s", path);
    len = strlen(tmp);

    if(tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    // Iterate over the path and create directories as needed
    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            // Check if the directory exists
            if(access(tmp, F_OK) != 0) {
                // If the directory doesn't exist, create it
                if(newdir(tmp, xpermission)<0) {
                    if(errno != EEXIST) {
                        gobj_log_error(0, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                            "msg",          "%s", "newdir() FAILED",
                            "path",         "%s", tmp,
                            "errno",        "%d", errno,
                            "serrno",       "%s", strerror(errno),
                            NULL
                        );
                        return -1;
                    }
                }
            } else if(stat(tmp, &st) != 0 && !S_ISDIR(st.st_mode)) {
                // If it's not a directory, return an error
                gobj_log_error(0, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "Not a directory",
                    "path",         "%s", tmp,
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
                return -1;
            }
            *p = '/';
        }
    }

    // Create the final directory component
    if(access(tmp, F_OK) != 0) {
        if(newdir(tmp, xpermission)<0) {
            if(errno != EEXIST) {
                gobj_log_error(0, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "newdir() FAILED",
                    "path",         "%s", tmp,
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
                return -1;
            }
        }
    } else if(stat(tmp, &st) != 0 && !S_ISDIR(st.st_mode)) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Not a directory",
            "path",         "%s", tmp,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    return 0;
}

/****************************************************************************
 *  Function to recursively remove a directory and its contents
 ****************************************************************************/
PUBLIC int rmrdir(const char *path)
{
    struct stat statbuf;
    struct dirent *dir_entry;
    DIR *dir;
    char full_path[PATH_MAX];

    // Check if the path exists
    if (stat(path, &statbuf) != 0) {
        return -1;
    }

    // If it's a regular file or symbolic link, remove it
    if (S_ISREG(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) {
        if (remove(path) != 0) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "remove() FAILED",
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            return -1;
        }
    }
        // If it's a directory, remove its contents recursively
    else if (S_ISDIR(statbuf.st_mode)) {
        dir = opendir(path);
        if (dir == NULL) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot open directory",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        while ((dir_entry = readdir(dir)) != NULL) {
            // Skip the special entries "." and ".."
            if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
                continue;
            }

            // Build the full path for the entry
            snprintf(full_path, sizeof(full_path), "%s/%s", path, dir_entry->d_name);

            // Recursively remove the entry
            if (rmrdir(full_path) != 0) {
                closedir(dir);
                return -1;
            }
        }

        closedir(dir);

        // Finally, remove the directory itself
        if (rmdir(path) != 0) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "rmdir() FAILED",
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            return -1;
        }
    }

    return 0;
}

/****************************************************************************
 *  Recursively remove the content of a directory
 ****************************************************************************/
PUBLIC int rmrcontentdir(const char *root_dir)
{
    struct dirent *dent;
    DIR *dir;
    struct stat st;

    if (!(dir = opendir(root_dir))) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot open directory",
            "path",         "%s", root_dir,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    while ((dent = readdir(dir))) {
        char *dname = dent->d_name;
        if (!strcmp(dname, ".") || !strcmp(dname, ".."))
            continue;
        char path[PATH_MAX];
        build_path(path, sizeof(path), root_dir, dname);

        if(stat(path, &st) == -1) {
            closedir(dir);
            return -1;
        }

        if(S_ISDIR(st.st_mode)) {
            /* recursively follow dirs */
            if(rmrdir(path)<0) {
                closedir(dir);
                return -1;
            }
        } else {
            if(unlink(path) < 0) {
                closedir(dir);
                return -1;
            }
        }
    }
    closedir(dir);
    return 0;
}




                    /*------------------------------------*
                     *  ### Strings
                     *------------------------------------*/




/***************************************************************************
 *    Delete char 'x' at end of string
 ***************************************************************************/
PUBLIC char *delete_right_char(char *s, char x)
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
 *   Delete char 'x' at begin of string
 ***************************************************************************/
PUBLIC char *delete_left_char(char *s, char x)
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
 *  WARNING function a bit slower that snprintf
 ***************************************************************************/
PUBLIC char *build_path(char *bf, size_t bfsize, ...)
{
    const char *segment;
    char *segm = 0;
    va_list ap;
    va_start(ap, bfsize);

    int writted = 0;
    int i = 0;
    while ((segment = (char *)va_arg (ap, char *)) != NULL) {
        if(!empty_string(segment)) {
            segm = strdup(segment);
            if(!segm) {
                syslog(LOG_CRIT, "YUNETA: " "No memory");
                break;
            }
            if(i==0) {
                // The first segment can be absolute (begin with /) or relative (not begin with /)
                delete_right_char(segm, '/');
            } else {
                delete_left_char(segm, '/');
                delete_right_char(segm, '/');
            }

            writted = snprintf(bf, bfsize, "%s%s", i>0?"/":"", segm);
            if(writted < 0) {
                syslog(LOG_CRIT, "YUNETA: " "snprintf() FAILED");
                break;
            }
            bf += writted;
            bfsize -= writted;

            EXEC_AND_RESET(free, segm)

            if(bfsize <= 0) {
                syslog(LOG_CRIT, "YUNETA: " "No space to snprintf in build_path()");
                break;
            }
            i++;
        }
    }

    EXEC_AND_RESET(free, segm)

    va_end(ap);

    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *get_last_segment(char *path)
{
    char *p = strrchr(path, '/');
    if(!p) {
        return path;
    }
    return p+1;
}

/***************************************************************************
 *
 ***************************************************************************/
// PUBLIC char *pop_last_segment(char *path) // WARNING path modified
// {
//     char *p = strrchr(path, '/');
//     if(!p) {
//         return path;
//     }
//     *p = 0;
//     return p+1;
// }

PUBLIC char *pop_last_segment(char *path) // WARNING path modified, optimized version
{
    if(!path || !*path) {
        return path;
    }

    char *end = path + strlen(path);
    while(--end >= path) {
        if(*end == '/') {
            *end = 0;
            return end + 1;
        }
    }
    return path;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *helper_quote2doublequote(char *str)
{
    register size_t len = strlen(str);
    register char *p = str;

    for(size_t i=0; i<len; i++, p++) {
        if(*p== '\'')
            *p = '"';
    }
    return str;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *helper_doublequote2quote(char *str)
{
    register size_t len = strlen(str);
    register char *p = str;

    for(size_t i=0; i<len; i++, p++) {
        if(*p== '"')
            *p = '\'';
    }
    return str;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL all_numbers(const char* s)
{
    if(empty_string(s))
        return 0;

    const unsigned char* p = (unsigned char *)s;
    while(*p != '\0') {
        if(!isdigit(*p)) {
            return FALSE;
        }
        p++;
    }
    return TRUE;
}

/***************************************************************************
 *  Prints to the provided buffer a nice number of bytes (KB, MB, GB, etc)
 *  https://www.mbeckler.org/blog/?p=114
 ***************************************************************************/
PUBLIC void nice_size(char *bf, size_t bfsize, uint64_t bytes, BOOL b1024)
{
    double unit = b1024 ? 1000 : 1024;

    const char *suffixes[7];
    suffixes[0] = "B";
    suffixes[1] = "Thousands";
    suffixes[2] = "Millions";
    suffixes[3] = "GB";
    suffixes[4] = "TB";
    suffixes[5] = "PB";
    suffixes[6] = "EB";
    unsigned int s = 0; // which suffix to use
    double count = (double)bytes;

    while (count >= unit && s < 7)
    {
        s++;
        count /= unit;
    }

    // Check if count is an integer by comparing to its cast to int
    if (count == (int)count)
        snprintf(bf, bfsize, "%d %s", (int)count, suffixes[s]);
    else
        snprintf(bf, bfsize, "%.1f %s", count, suffixes[s]);
}

/***************************************************************************
 *    Elimina blancos a la derecha. (Espacios, tabuladores, CR's  o LF's)
 ***************************************************************************/
PUBLIC void delete_right_blanks(char *s)
{
    int l;
    char c;

    /*---------------------------------*
     *  Elimina blancos a la derecha
     *---------------------------------*/
    l = (int)strlen(s);
    if(l==0)
        return;
    while(--l>=0) {
        c= *(s+l);
        if(c==' ' || c=='\t' || c=='\n' || c=='\r')
            *(s+l)='\0';
        else
            break;
    }
}

/***************************************************************************
 *    Elimina blancos a la izquierda. (Espacios, tabuladores, CR's  o LF's)
 ***************************************************************************/
PUBLIC void delete_left_blanks(char *s)
{
    unsigned l;
    char c;

    /*----------------------------*
     *  Busca el primer no blanco
     *----------------------------*/
    l=0;
    while(1) {
        c= *(s+l);
        if(c=='\0')
            break;
        if(c==' ' || c=='\t' || c=='\n' || c=='\r')
            l++;
        else
            break;
    }
    if(l>0) {
        memmove(s,s+l,(unsigned long)strlen(s) -l + 1);
    }
}

/***************************************************************************
 *    Justifica a la izquierda eliminado blancos a la derecha
 ***************************************************************************/
PUBLIC void left_justify(char *s)
{
    if(s) {
        /*---------------------------------*
         *  Elimina blancos a la derecha
         *---------------------------------*/
        delete_right_blanks(s);

        /*-----------------------------------*
         *  Quita los blancos a la izquierda
         *-----------------------------------*/
        delete_left_blanks(s);
    }
}

/***************************************************************************
 *  Convert n bytes of string to upper case
 ***************************************************************************/
PUBLIC char *strntoupper(char* s, size_t n)
{
    if(!s || n == 0)
        return 0;

    char *p = s;
    while (n > 0 && *p != '\0') {
        int c = (int)*p;
        *p = (char)toupper(c);
        p++;
        n--;
    }

    return s;
}

/***************************************************************************
 *  Convert n bytes of string to lower case
 ***************************************************************************/
PUBLIC char *strntolower(char* s, size_t n)
{
    if(!s || n == 0)
        return 0;

    char *p = s;
    while (n > 0 && *p != '\0') {
        int c = (int)*p;
        *p = (char)tolower(c);
        p++;
        n--;
    }

    return s;
}

/****************************************************************************
 *  Translate characters from the string 'from' to the string 'to'.
 *  Usage:
 *        char * translate_string(char *to,
 *                                char *from,
 *                                char *mk_to,
 *                                char *mk_from);
 *  Description:
 *        Move characters from 'from' to 'to' using the masks
 *  Return:
 *        A pointer to string 'to'.
 ****************************************************************************/
PUBLIC char * translate_string(
    char *to,
    int tolen,
    const char *from,
    const char *mk_to,
    const char *mk_from
) {
    int pos;
    char chr;
    char *chr_pos;

    tolen--;
    if(tolen<1)
        return to;
    strncpy(to, mk_to, tolen);
    to[tolen] = 0;

    while((chr = *mk_from++) != 0) {
        if((chr_pos = strchr(mk_to,chr)) == NULL) {
            from++;
            continue ;
        }
        pos = (int) (chr_pos - mk_to);
        if(pos > tolen) {
            break;
        }
        to[pos++] = *from++ ;

        while(chr == *mk_from) {
            if(chr == mk_to[pos]) {
                if(pos > tolen) {
                    break;
                }
                to[pos++] = *from;
            }
            mk_from++ ;
            from++;
        }
    }
    return to;
}

/***************************************************************************
 *    cambia el character old_d por new_c. Retorna los caracteres cambiados
 ***************************************************************************/
PUBLIC int change_char(char *s, char old_c, char new_c)
{
    int count = 0;

    while(*s) {
        if(*s == old_c) {
            *s = new_c;
            count++;
        }
        s++;
    }
    return count;
}

/***************************************************************************
 *  Extract parameter: delimited by blanks (\b\t) or quotes ('' "")
 *  The string is modified (nulls inserted)!
 ***************************************************************************/
PUBLIC char *get_parameter(char *s, char **save_ptr)
{
    char c;
    char *p;

    if(!s) {
        if(save_ptr) {
            *save_ptr = 0;
        }
        return 0;
    }
    /*
     *  Find first no-blank
     */
    while(1) {
        c = *s;
        if(c==0)
            return 0;
        if(!(c==' ' || c=='\t'))
            break;
        s++;
    }

    /*
     *  Check quotes
     */
    if(c=='\'' || c=='"') {
        p = strchr(s+1, c);
        if(p) {
            *p = 0;
            if(save_ptr) {
                *save_ptr = p+1;
            }
            return s+1;
        }
    }

    /*
     *  Find first blank
     */
    p = s;
    while(1) {
        c = *s;
        if(c==0) {
            if(save_ptr) {
                *save_ptr = 0;
            }
            return p;
        }
        if((c==' ' || c=='\t')) {
            *s = 0;
            if(save_ptr) {
                *save_ptr = s+1;
            }
            return p;
        }
        s++;
    }
}

/***************************************************************************
 *  Extract key=value or key='this value' parameter
 *  Return the value, the key in `key`
 *  The string is modified (nulls inserted)!
 ***************************************************************************/
PUBLIC char *get_key_value_parameter(char *s, char **key, char **save_ptr)
{
    char c;
    char *p;

    if(!s) {
        if(save_ptr) {
            *save_ptr = 0;
        }
        return 0;
    }
    /*
     *  Find first no-blank
     */
    while(1) {
        c = *s;
        if(c==0)
            return 0;
        if(!(c==' ' || c=='\t'))
            break;
        s++;
    }

    char *value = strchr(s, '=');
    if(!value) {
        if(key) {
            *key = 0;
        }
        if(save_ptr) {
            *save_ptr = s;
        }
        return 0;
    }
    *value = 0; // delete '='
    value++;
    left_justify(s);
    if(key) {
        *key = s;
    }
    s = value;
    c = *s;

    /*
     *  Check quotes
     */
    if(c=='\'' || c=='"') {
        p = strchr(s+1, c);
        if(p) {
            *p = 0;
            if(save_ptr) {
                *save_ptr = p+1;
            }
            return s+1;
        } else {
            if(save_ptr) {
                *save_ptr = 0;
            }
            return 0;
        }
    }

    /*
     *  Find first blank
     */
    p = s;
    while(1) {
        c = *s;
        if(c==0) {
            if(save_ptr) {
                *save_ptr = 0;
            }
            return p;
        }
        if((c==' ' || c=='\t')) {
            *s = 0;
            if(save_ptr) {
                *save_ptr = s+1;
            }
            return p;
        }
        s++;
    }
}

/***************************************************************************
    Split a string by delim returning the list of strings.
    Return filling `list_size` if not null with items size,
        It MUST be initialized to 0 (no limit) or to maximum items wanted.
    WARNING Remember free with split_free2().
    HACK: No, It does NOT include the empty strings!
 ***************************************************************************/
PUBLIC const char ** split2(const char *str, const char *delim, int *plist_size)
{
    char *ptr;

    if(plist_size) {
        *plist_size = 0; // error case
    }
    char *buffer = gbmem_strdup(str);
    if(!buffer) {
        return 0;
    }

    // Get list size
    int list_size = 0;
    for (ptr = strtok(buffer, delim); ptr != NULL; ptr = strtok(NULL, delim)) {
        list_size++;
    }
    GBMEM_FREE(buffer)

    buffer = gbmem_strdup(str);   // Prev buffer is destroyed!
    if(!buffer) {
        return 0;
    }

    // Alloc list
    int size = sizeof(char *) * (list_size + 1);
    const char **list = GBMEM_MALLOC(size);

    // Fill list
    int i = 0;
    for (ptr = strtok(buffer, delim); ptr != NULL; ptr = strtok(NULL, delim)) {
        if (i < list_size) {
            list[i++] = gbmem_strdup(ptr);
        } else {
            break;
        }
    }
    GBMEM_FREE(buffer)

    if(plist_size) {
        *plist_size = i;
    }
    return list;
}

/***************************************************************************
 *  Free split list content
 ***************************************************************************/
PUBLIC void split_free2(const char **list)
{
    if(list) {
        char **p = (char **)list;
        while(*p) {
            GBMEM_FREE(*p)
            *p = 0;
            p++;
        }
        GBMEM_FREE(list)
    }
}

/***************************************************************************
    Split string `str` by `delim` chars returning the list of strings.
    Return filling `list_size` if not null with items size,
        It MUST be initialized to 0 (no limit) or to maximum items wanted.
    WARNING Remember free with split_free3().
    HACK: Yes, It does include the empty strings!
 ***************************************************************************/
PUBLIC const char **split3(const char *str, const char *delim, int *plist_size)
{
    char *ptr, *p;

    if(plist_size) {
        *plist_size = 0; // error case
    }
    char *buffer = gbmem_strdup(str);
    if(!buffer) {
        return 0;
    }

    // Get list size
    int list_size = 0;

    p = buffer;
    while ((ptr = strsep(&p, delim)) != NULL) {
        list_size++;
    }
    GBMEM_FREE(buffer)

    buffer = gbmem_strdup(str);   // Prev buffer is destroyed!
    if(!buffer) {
        return 0;
    }

    // Alloc list
    size_t size = sizeof(char *) * (list_size + 1);
    const char **list = GBMEM_MALLOC(size);

    // Fill list
    int i = 0;
    p = buffer;
    while ((ptr = strsep(&p, delim)) != NULL) {
        if (i < list_size) {
            list[i] = gbmem_strdup(ptr);
            i++;
        } else {
            break;
        }
    }
    GBMEM_FREE(buffer)

    if(plist_size) {
        *plist_size = list_size;
    }
    return list;
}

/***************************************************************************
 *  Free split list content
 ***************************************************************************/
PUBLIC void split_free3(const char **list)
{
    if(list) {
        char **p = (char **)list;
        while(*p) {
            GBMEM_FREE(*p)
            *p = 0;
            p++;
        }
        GBMEM_FREE(list)
    }
}

/***************************************************************************
    Concat two strings
    WARNING Remember free with str_concat_free().
 ***************************************************************************/
PUBLIC char * str_concat(const char *str1, const char *str2)
{
    size_t len = 0;

    if(str1) {
        len += strlen(str1);
    }
    if(str2) {
        len += strlen(str2);
    }

    char *s = GBMEM_MALLOC(len+1);
    if(!s) {
        return NULL;
    }
    if(str1) {
        strcat(s, str1);
    }
    if(str2) {
        strcat(s, str2);
    }

    return s;
}

/***************************************************************************
    Concat three strings
    WARNING Remember free with str_concat_free().
 ***************************************************************************/
PUBLIC char * str_concat3(const char *str1, const char *str2, const char *str3)
{
    size_t len = 0;

    if(str1) {
        len += strlen(str1);
    }
    if(str2) {
        len += strlen(str2);
    }
    if(str3) {
        len += strlen(str3);
    }

    char *s = GBMEM_MALLOC(len+1);
    if(!s) {
        return NULL;
    }
    if(str1) {
        strcat(s, str1);
    }
    if(str2) {
        strcat(s, str2);
    }
    if(str3) {
        strcat(s, str3);
    }

    return s;
}

/***************************************************************************
 *  Free concat
 ***************************************************************************/
PUBLIC void str_concat_free(char *s)
{
    GBMEM_FREE(s)
}

/***************************************************************************
 *  Return idx of str in string list.
    Return -1 if not exist
 ***************************************************************************/
PUBLIC int idx_in_list(const char **list, const char *str, BOOL ignore_case)
{
    int (*cmp_fn)(const char *s1, const char *s2)=0;
    if(ignore_case) {
        cmp_fn = strcasecmp;
    } else {
        cmp_fn = strcmp;
    }

    int i = 0;
    while(*list) {
        if(cmp_fn(str, *list)==0) {
            return i;
        }
        i++;
        list++;
    }
    return -1;
}

/***************************************************************************
 *  Return TRUE if str is in string list.
 ***************************************************************************/
PUBLIC BOOL str_in_list(const char **list, const char *str, BOOL ignore_case)
{
    int (*cmp_fn)(const char *s1, const char *s2)=0;
    if(ignore_case) {
        cmp_fn = strcasecmp;
    } else {
        cmp_fn = strcmp;
    }

    while(*list) {
        if(cmp_fn(str, *list)==0) {
            return TRUE;
        }
        list++;
    }
    return FALSE;
}




                    /*------------------------------------*
                     *  ### Json
                     *------------------------------------*/




/***************************************************************************
 *  If exclusive then let file opened and return the fd, else close the file
 ***************************************************************************/
PUBLIC json_t *load_persistent_json(
    hgobj gobj,
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error,
    int *pfd,
    BOOL exclusive,
    BOOL silence  // HACK to silence TRUE you MUST set on_critical_error=LOG_NONE
)
{
    if(pfd) {
        *pfd = -1;
    }

    /*
     *  Full path
     */
    char full_path[PATH_MAX];
    build_path(full_path, sizeof(full_path), directory, filename, NULL);

    if(!is_regular_file(full_path)) {
        if(!(silence && on_critical_error == LOG_NONE)) {
            gobj_log_critical(gobj, on_critical_error|LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "Cannot load json, file not exist",
                "path",         "%s", full_path,
                NULL
            );
        }
        return 0;
    }

    int fd;
    if(exclusive) {
        fd = open_exclusive(full_path, O_RDONLY|O_NOFOLLOW, 0);
#ifdef __linux__
        if(fd>0) {
            fcntl(fd, F_SETFD, FD_CLOEXEC); // Que no vaya a los child
        }
#endif
    } else {
        fd = open(full_path, O_RDONLY|O_NOFOLLOW);
    }
    if(fd<0) {
        if(!(silence && on_critical_error == LOG_NONE)) {
            gobj_log_critical(gobj, on_critical_error,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", exclusive? "Cannot open an exclusive json file":"Cannot open a json file",
                "msg2",         "%s", exclusive? "Check if someone has opened the file":"Check permissions",
                "exclusive",    "%d", exclusive,
                "path",         "%s", full_path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
        return 0;
    }

    json_t *jn = json_loadfd(fd, 0, 0);
    if(!jn) {
        gobj_log_critical(gobj, on_critical_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "Cannot load json file, bad json",
            NULL
        );
        close(fd);
        return 0;
    }
    if(!exclusive) {
        close(fd);
    } else {
        if(pfd) {
            *pfd = fd;
        }
    }
    return jn;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *load_json_from_file(
    hgobj gobj,
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error
)
{
    /*
     *  Full path
     */
    char full_path[PATH_MAX];
    build_path(full_path, sizeof(full_path), directory, filename, NULL);

    if(access(full_path, 0)!=0) {
        // Silence, please. Caller must check the return.
        return 0;
    }

    int fd = open(full_path, O_RDONLY|O_NOFOLLOW);
    if(fd<0) {
        gobj_log_critical(gobj, on_critical_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot open json file",
            "path",         "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return 0;
    }

    json_t *jn = json_loadfd(fd, 0, 0);
    if(!jn) {
        gobj_log_critical(gobj, on_critical_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "Cannot load json file, bad json",
            NULL
        );
    }
    close(fd);
    return jn;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int save_json_to_file(
    hgobj gobj,
    const char *directory,
    const char *filename,
    int xpermission,
    int rpermission,
    log_opt_t on_critical_error,
    BOOL create,        // Create file if not exists or overwrite.
    BOOL only_read,
    json_t *jn_data     // owned
)
{
    /*-----------------------------------*
     *  Check parameters
     *-----------------------------------*/
    if(!directory || !filename) {
        gobj_log_critical(gobj, on_critical_error|LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Parameter 'directory' or 'filename' NULL",
            NULL
        );
        JSON_DECREF(jn_data)
        return -1;
    }

    /*-----------------------------------*
     *  Create directory if not exists
     *-----------------------------------*/
    if(!is_directory(directory)) {
        if(!create) {
            JSON_DECREF(jn_data)
            return -1;
        }
        if(mkrdir(directory, xpermission)<0) {
            gobj_log_critical(gobj, on_critical_error,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create directory",
                "directory",    "%s", directory,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            JSON_DECREF(jn_data)
            return -1;
        }
    }

    /*
     *  Full path
     */
    char full_path[PATH_MAX];
    if(empty_string(filename)) {
        snprintf(full_path, sizeof(full_path), "%s", directory);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, filename);
    }

    /*
     *  Create file
     */
    int fp = newfile(full_path, rpermission, create);
    if(fp < 0) {
        gobj_log_critical(gobj, on_critical_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot create json file",
            "filename",     "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        JSON_DECREF(jn_data)
        return -1;
    }

    if(json_dumpfd(jn_data, fp, JSON_INDENT(2))<0) {
        gobj_log_critical(gobj, on_critical_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "Cannot write in json file",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        JSON_DECREF(jn_data)
        return -1;
    }
    close(fp);
    if(only_read) {
        chmod(full_path, 0440);
    }
    JSON_DECREF(jn_data)

    return 0;
}

/*******************************************************************************
 *  fields: DESC str array with: key, type, defaults
 *  type can be:
 *      str|string,
 *      int|integer,
 *      real,
 *      bool|boolean,
 *      null,
 *      dict|object,
 *      list|array,
 *      time
 *******************************************************************************/
PUBLIC json_t *create_json_record(
    hgobj gobj,
    const json_desc_t *json_desc
)
{
    if(!json_desc) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "DESC null",
            NULL
        );
        return 0;
    }
    json_t *jn = json_object();

    while(json_desc->name) {
        if(empty_string(json_desc->name)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "DESC without key field",
                NULL
            );
            break;
        }
        if(empty_string(json_desc->type)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "DESC without type field",
                NULL
            );
            break;
        }
        const char *name = json_desc->name;
        const char *defaults = json_desc->defaults;

        SWITCHS(json_desc->type) {
            CASES("str")
            CASES("string")
                json_object_set_new(jn, name, json_string(defaults));
                break;
            CASES("time")
            CASES("int")
            CASES("integer")
                unsigned long v=0;
                if(*defaults == '0') {
                    v = strtoul(defaults, 0, 8);
                } else if(*defaults == 'x') {
                    v = strtoul(defaults, 0, 16);
                } else {
                    v = strtoul(defaults, 0, 10);
                }
                json_object_set_new(jn, name, json_integer(v));
                break;
            CASES("real")
                json_object_set_new(jn, name, json_real(strtod(defaults, NULL)));
                break;
            CASES("bool")
            CASES("boolean")
                if(strcasecmp(defaults, "TRUE")==0) {
                    json_object_set_new(jn, name, json_true());
                } else if(strcasecmp(defaults, "FALSE")==0) {
                    json_object_set_new(jn, name, json_false());
                } else {
                    json_object_set_new(jn, name, atoi(defaults)?json_true():json_false());
                }
                break;
            CASES("null")
                json_object_set_new(jn, name, json_null());
                break;
            CASES("object")
            CASES("dict")
                char desc_name[80+1];
                if(sscanf(defaults, "{%80s}", desc_name)==1) {
                    //get_fields(db_tranger_desc, desc_name, TRUE); // only to test fields
                } else if(!empty_string(defaults)) {
                    //get_fields(db_tranger_desc, defaults, TRUE); // only to test fields
                }
                json_object_set_new(jn, name, json_object());
                break;
            CASES("array")
            CASES("list")
                char desc_name[80+1];
                if(sscanf(defaults, "{%80s}", desc_name)==1) {
                    //get_fields(db_tranger_desc, desc_name, TRUE); // only to test fields
                }
                json_object_set_new(jn, name, json_array());
                break;
            DEFAULTS
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Type UNKNOWN",
                    "type",         "%s", json_desc->type,
                    NULL
                );
                break;
        } SWITCHS_END;

        json_desc++;
    }

    return jn;
}

/***************************************************************************
 *
 *  Convert a json record desc to a topic schema
 *
    json_record
    {
        name: string
        type: string
        defaults: string
        fillspace: string
    }

    schema
    {
        id: string
        header: string
        type: string
        fillspace: integer
    }

 ***************************************************************************/
PUBLIC json_t *json_desc_to_schema(const json_desc_t *json_desc)
{
    json_t *jn_schema = json_array();

    const json_desc_t *p1 = json_desc;
    while(p1 && p1->name) {
        json_t *schema_item = json_object();
        json_object_set_new(schema_item, "id", json_string(p1->name));
        json_object_set_new(schema_item, "header", json_string(p1->name));
        const char *type = p1->type;
        if(strcmp(type, "int")==0) {
            type = "integer";
        }
        if(strcmp(type, "str")==0) {
            type = "string";
        }
        if(strcmp(type, "bool")==0) {
            type = "boolean";
        }
        json_object_set_new(schema_item, "type", json_string(type));
        int fillspace = empty_string(p1->fillspace)?20:atoi(p1->fillspace);
        json_object_set_new(schema_item, "fillspace", json_integer(fillspace));
        json_array_append_new(jn_schema, schema_item);
        p1++;
    }

    return jn_schema;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *bits2jn_strlist(
    const char **strings_table,
    uint64_t bits
) {
    json_t *jn_list = json_array();

    for(uint64_t i=0; strings_table[i]!=NULL && i<sizeof(i); i++) {
        uint64_t bitmask = 1 << i;
        if (bits & bitmask) {
            json_array_append_new(jn_list, json_string(strings_table[i]));
        }
    }

    return jn_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC gbuffer_t *bits2gbuffer(
    const char **strings_table,
    uint64_t bits
) {
    gbuffer_t *gbuf = gbuffer_create(256, 1024);

    if(!gbuf) {
        return 0;
    }
    BOOL add_sep = FALSE;

    for(uint64_t i=0; strings_table[i]!=NULL && i<sizeof(i); i++) {
        uint64_t bitmask = 1 << i;
        if (bits & bitmask) {
            if(add_sep) {
                gbuffer_append(gbuf, "|", 1);
            }
            gbuffer_append_string(gbuf, strings_table[i]);
            add_sep = TRUE;
        }
    }

    return gbuf;
}

/***************************************************************************
 *  Convert strings
 *      by default separators are "|, "
 *          "s|s|s" or "s s s" or "s,s,s" or any combinations of them
 *  into bits according the string table
 *  The strings table must be end by NULL
 ***************************************************************************/
PUBLIC uint64_t strings2bits(
    const char **strings_table,
    const char *str,
    const char *separators
) {
    uint64_t bitmask = 0;
    if(empty_string(separators)) {
        separators = "|, ";
    }
    int list_size;
    const char **names = split2(str, separators, &list_size);

    for(int i=0; i<list_size; i++) {
        int idx = idx_in_list(strings_table, *(names +i), TRUE);
        if(idx >= 0 && idx < (int)sizeof(uint64_t)) {
            bitmask |= 1 << (idx);
        }
    }

    split_free2(names);

    return bitmask;
}

/***************************************************************************
    Get a the idx of string value in a strings json list.
 ***************************************************************************/
PUBLIC int json_list_str_index(json_t *jn_list, const char *str, BOOL ignore_case)
{
    if(!json_is_array(jn_list)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "list MUST BE a json array",
            NULL
        );
        gobj_trace_json(0, jn_list, "list MUST BE a json array");
        return 0;
    }

    size_t idx;
    json_t *jn_str;

    json_array_foreach(jn_list, idx, jn_str) {
        if(!json_is_string(jn_str)) {
            continue;
        }
        const char *_str = json_string_value(jn_str);
        if(ignore_case) {
            if(strcasecmp(_str, str)==0)
                return (int)idx;
        } else {
            if(strcmp(_str, str)==0)
                return (int)idx;
        }
    }

    return -1;
}

/***************************************************************************
    Get a integer value from an json list search by idx
 ***************************************************************************/
PUBLIC json_int_t json_list_int(json_t *jn_list, size_t idx)
{
    json_t *jn_int;
    json_int_t i;

    if(!json_is_array(jn_list)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "list MUST BE a json array",
            NULL
        );
        return 0;
    }

    jn_int = json_array_get(jn_list, idx);
    if(jn_int && !json_is_integer(jn_int)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "value MUST BE a json integer",
            NULL);
        return 0;
    }

    i = json_integer_value(jn_int);
    return i;
}

/***************************************************************************
    Get the idx of integer value in a integers json list.
 ***************************************************************************/
PUBLIC int json_list_int_index(json_t *jn_list, json_int_t value)
{
    if(!json_is_array(jn_list)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "list MUST BE a json array",
            NULL
        );
        return -1;
    }

    size_t idx = 0;
    json_t *jn_value;
    json_array_foreach(jn_list, idx, jn_value) {
        if(json_is_integer(jn_value)) {
            json_int_t value_ = json_integer_value(jn_value);
            if(value_ == value) {
                return (int)idx;
            }
        }
    }

    return -1;
}

/***************************************************************************
 *  Find a json value in the list.
 *  Return index or -1 if not found or the index relative to 0.
 ***************************************************************************/
PUBLIC int json_list_find(json_t *list, json_t *value) // WARNING slow function
{
    int idx_found = -1;
    size_t flags = JSON_COMPACT|JSON_ENCODE_ANY;
    int index;
    json_t *_value;
    char *s_found_value = json_dumps(value, flags);
    if(s_found_value) {
        json_array_foreach(list, index, _value) {
            char *s_value = json_dumps(_value, flags);
            if(s_value) {
                if(strcmp(s_value, s_found_value)==0) {
                    idx_found = index;
                    jsonp_free(s_value);
                    break;
                } else {
                    jsonp_free(s_value);
                }
            }
        }
        jsonp_free(s_found_value);
    }
    return idx_found;
}

/***************************************************************************
 *  Extend array values.
 *  If as_set_type is TRUE then not repeated values (WARNING slow function)
 ***************************************************************************/
PUBLIC int json_list_update(json_t *list, json_t *other, BOOL as_set_type) // WARNING slow function
{
    if(!json_is_array(list) || !json_is_array(other)) {
        return -1;
    }
    size_t index;
    json_t *value;
    json_array_foreach(other, index, value) {
        if(as_set_type) {
            int idx = json_list_find(list, value);
            if(idx < 0) {
                json_array_append(list, value);
            }
        } else {
            json_array_append(list, value);
        }
    }
    return 0;
}

/***************************************************************************
 *  Check if a list is a integer range:
 *      - must be a list of two integers (first <= second)
 *  Set first and second values in pfirst or psecond are not null
 ***************************************************************************/
PUBLIC BOOL json_is_range(json_t *list, json_int_t *pfirst, json_int_t *psecond)
{
    if(*pfirst) {
        *pfirst = 0;
    }
    if(*psecond) {
        *psecond = 0;
    }
    if(json_array_size(list) != 2) {
        return FALSE;
    }

    json_int_t first = json_integer_value(json_array_get(list, 0));
    json_int_t second = json_integer_value(json_array_get(list, 1));
    if(first <= second) {
        if(*pfirst) {
            *pfirst = first;
        }
        if(*psecond) {
            *psecond = second;
        }
        return TRUE;
    } else {
        return FALSE;
    }
}

/***************************************************************************
 *  Return a expanded integer range
 *  WARNING slow function, don't use in large ranges
 ***************************************************************************/
PUBLIC json_t *json_range_list(json_t *list) // WARNING slow function, don't use in large ranges
{
    json_int_t first;
    json_int_t second;
    if(!json_is_range(list, &first, &second)) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Not a range",
            NULL
        );
        return 0;
    }
    json_t *range = json_array();
    for(json_int_t i=first; i<=second; i++) {
        json_t *jn_int = json_integer(i);
        json_array_append_new(range, jn_int);
    }
    return range;
}

/***************************************************************************
 *  Build a list (set) with lists of integer ranges.
 *  [[#from, #to], [#from, #to], #integer, #integer, ...] -> list
 *  WARNING: Arrays of two integers are considered range of integers.
 *  Arrays of one or more of two integers are considered individual integers.
 *
 *  Return the json list
 ***************************************************************************/
PUBLIC json_t *json_listsrange2set(json_t *listsrange) // WARNING function TOO SLOW, use for short ranges
{
    if(!json_is_array(listsrange)) {
        return 0;
    }
    json_t *ln_list = json_array();

    size_t index;
    json_t *value;
    json_array_foreach(listsrange, index, value) {
        if(json_is_integer(value)) {
            // add new integer item
            if(json_list_find(ln_list, value)<0) {
                json_array_append(ln_list, value);
            }
        } else if(json_is_array(value)) {
            // add new integer list or integer range
            if(json_is_range(value, NULL, NULL)) {
                json_t *range = json_range_list(value);
                if(range) {
                    json_list_update(ln_list, range, TRUE);
                    json_decref(range);
                }
            } else {
                json_list_update(ln_list, value, TRUE);
            }
        } else {
            // ignore the rest
            continue;
        }
    }

    return ln_list;
}

/***************************************************************************
 *  Update keys and values, recursive through all objects
 *  If overwrite is FALSE then not update existing keys (protected write)
 ***************************************************************************/
PUBLIC int json_dict_recursive_update(
    json_t *object,
    json_t *other,
    BOOL overwrite
)
{
    const char *key;
    json_t *value;

    if(!json_is_object(object) || !json_is_object(other)) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json_dict_recursive_update(): parameters must be objects",
            NULL
        );
        return -1;
    }
    json_object_foreach(other, key, value) {
        json_t *dvalue = json_object_get(object, key);
        if(json_is_object(dvalue) && json_is_object(value)) {
            json_dict_recursive_update(dvalue, value, overwrite);
        } else if(json_is_array(dvalue) && json_is_array(value)) {
            if(overwrite) {
                /*
                 *  WARNING
                 *  In configuration consider the lists as set (no repeated items).
                 */
                json_list_update(dvalue, value, TRUE);
            }
        } else {
            if(overwrite) {
                json_object_set_nocheck(object, key, value);
            } else {
                if(!json_object_get(object, key)) {
                    json_object_set_nocheck(object, key, value);
                }
            }
        }
    }
    return 0;
}

/***************************************************************************
 *  Simple json to real
 ***************************************************************************/
PUBLIC double jn2real(json_t *jn_var)
{
    double val = 0.0;
    if(json_is_real(jn_var)) {
        val = json_real_value(jn_var);
    } else if(json_is_integer(jn_var)) {
        val = (double)json_integer_value(jn_var);
    } else if(json_is_string(jn_var)) {
        const char *s = json_string_value(jn_var);
        val = atof(s);
    } else if(json_is_true(jn_var)) {
        val = 1.0;
    } else if(json_is_false(jn_var)) {
        val = 0.0;
    } else if(json_is_null(jn_var)) {
        val = 0.0;
    }
    return val;
}

/***************************************************************************
 *  Simple json to int
 ***************************************************************************/
PUBLIC json_int_t jn2integer(json_t *jn_var)
{
    json_int_t val = 0;
    if(json_is_real(jn_var)) {
        val = (json_int_t)json_real_value(jn_var);
    } else if(json_is_integer(jn_var)) {
        val = json_integer_value(jn_var);
    } else if(json_is_string(jn_var)) {
        const char *v = json_string_value(jn_var);
        if(*v == '0') {
            val = strtoll(v, 0, 8);
        } else if(*v == 'x' || *v == 'X') {
            val = strtoll(v, 0, 16);
        } else {
            val = strtoll(v, 0, 10);
        }
    } else if(json_is_true(jn_var)) {
        val = 1;
    } else if(json_is_false(jn_var)) {
        val = 0;
    } else if(json_is_null(jn_var)) {
        val = 0;
    }
    return val;
}

/***************************************************************************
 *  Simple json to string, WARNING free return with gbmem_free
 ***************************************************************************/
PUBLIC char *jn2string(json_t *jn_var)
{
    char temp[PATH_MAX];
    char *s="";

    if(json_is_string(jn_var)) {
        s = (char *)json_string_value(jn_var);
    } else if(json_is_integer(jn_var)) {
        json_int_t v = json_integer_value(jn_var);
        snprintf(temp, sizeof(temp), "%"JSON_INTEGER_FORMAT, v);
        s = temp;
    } else if(json_is_real(jn_var)) {
        double v = json_real_value(jn_var);
        snprintf(temp, sizeof(temp), "%.17f", v);
        s = temp;
    } else if(json_is_boolean(jn_var)) {
        s = json_is_true(jn_var)?"1":"0";
    }

    return gbmem_strdup(s);
}

/***************************************************************************
 *  Simple json to bool
 ***************************************************************************/
PUBLIC BOOL jn2bool(json_t *jn_var)
{
    BOOL val = 0;
    if(json_is_real(jn_var)) {
        val = json_real_value(jn_var)?1:0;
    } else if(json_is_integer(jn_var)) {
        val = json_integer_value(jn_var)?1:0;
    } else if(json_is_string(jn_var)) {
        const char *v = json_string_value(jn_var);
        val = empty_string(v)?0:1;
    } else if(json_is_true(jn_var)) {
        val = 1;
    } else if(json_is_false(jn_var)) {
        val = 0;
    } else if(json_is_null(jn_var)) {
        val = 0;
    }
    return val;
}

/***************************************************************************
    Only compare str/int/real/bool items
    Complex types are done as matched
    Return lower, iqual, higher (-1, 0, 1), like strcmp
 ***************************************************************************/
PUBLIC int cmp_two_simple_json(
    json_t *jn_var1,    // not owned
    json_t *jn_var2     // not owned
)
{
    /*
     *  Discard complex types, done as NOT matched (DANGER change 17-Mar-2025)
     */
    if(json_is_object(jn_var1) ||
            json_is_object(jn_var2) ||
            json_is_array(jn_var1) ||
            json_is_array(jn_var2)) {
        return -1;
    }

    /*
     *  First try real
     */
    if(json_is_real(jn_var1) || json_is_real(jn_var2)) {
        double val1 = jn2real(jn_var1);
        double val2 = jn2real(jn_var2);
        if(val1 > val2) {
            return 1;
        } else if(val1 < val2) {
            return -1;
        } else {
            return 0;
        }
    }

    /*
     *  Try integer
     */
    if(json_is_integer(jn_var1) || json_is_integer(jn_var2)) {
        json_int_t val1 = jn2integer(jn_var1);
        json_int_t val2 = jn2integer(jn_var2);
        if(val1 > val2) {
            return 1;
        } else if(val1 < val2) {
            return -1;
        } else {
            return 0;
        }
    }

    /*
     *  Try boolean
     */
    if(json_is_boolean(jn_var1) || json_is_boolean(jn_var2)) {
        json_int_t val1 = jn2integer(jn_var1);
        json_int_t val2 = jn2integer(jn_var2);
        if(val1 > val2) {
            return 1;
        } else if(val1 < val2) {
            return -1;
        } else {
            return 0;
        }
    }

    /*
     *  Try string
     */
    char *val1 = jn2string(jn_var1);
    char *val2 = jn2string(jn_var2);
    int ret = strcmp(val1, val2);
    GBMEM_FREE(val1)
    GBMEM_FREE(val2)
    return ret;
}

/***************************************************************************
    Compare two json and return TRUE if they are identical.
 ***************************************************************************/
PUBLIC BOOL json_is_identical(
    json_t *kw1,    // not owned
    json_t *kw2     // not owned
)
{
    return json_equal(kw1, kw2);
    // if(!kw1 || !kw2) {
    //     return FALSE;
    // }
    // char *kw1_ = json2uglystr(kw1);
    // char *kw2_ = json2uglystr(kw2);
    // int ret = strcmp(kw1_, kw2_);
    // GBMEM_FREE(kw1_)
    // GBMEM_FREE(kw2_)
    // return ret==0?TRUE:FALSE;
}

/***************************************************************************
 *  Convert any json string to json binary.
 ***************************************************************************/
PUBLIC json_t *anystring2json(const char *bf, size_t len, BOOL verbose)
{
    if(empty_string(bf)) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "bf EMPTY",
                NULL
            );
        }
        return 0;
    }
    size_t flags = JSON_DECODE_ANY;
    json_error_t error;
    json_t *jn = json_loadb(bf, len, flags, &error);
    if(!jn) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_JSON_ERROR,
                "msg",          "%s", "json_loads() FAILED",
                "bf",          "%s", bf,
                "error",        "%s", error.text,
                "line",         "%d", error.line,
                "column",       "%d", error.column,
                "position",     "%d", error.position,
                NULL
            );
            gobj_trace_dump(0, bf, strlen(bf), "json_loads() FAILED");
        }
    }
    return jn;
}

/***************************************************************************
 *  Convert a legal json string to json binary.
 *  legal json string: MUST BE an array [] or object {}
 *  Old legalstring2json()
 ***************************************************************************/
PUBLIC json_t * string2json(const char* str, BOOL verbose)
{
    size_t flags = 0;
    json_error_t error;

    json_t *jn = json_loads(str, flags, &error);
    if(!jn) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "json_loads(3) FAILED",
                "str",          "%s", str,
                "error",        "%s", error.text,
                "line",         "%d", error.line,
                "column",       "%d", error.column,
                "position",     "%d", error.position,
                NULL
            );
            fprintf(stderr, "\n%s\n", str);     // WARNING output to console
        }
    }
    return jn;
}

/***************************************************************************
 *  Set real precision
 *  Return the previous precision
 ***************************************************************************/
PRIVATE int __real_precision__ = 12;
PUBLIC int set_real_precision(int precision)
{
    int prev = __real_precision__;
    __real_precision__ = precision;
    return prev;
}
PUBLIC int get_real_precision(void)
{
    return __real_precision__;
}

/***************************************************************************
 *  Any json to string
 *  Remember gbmem_free the returned string
 ***************************************************************************/
PUBLIC char *json2str(const json_t *jn) // jn not owned
{
    if(!jn) {
        return 0;
    }
    size_t flags = JSON_ENCODE_ANY | JSON_INDENT(4) | JSON_REAL_PRECISION(get_real_precision());
    return json_dumps(jn, flags);
}

/***************************************************************************
 *  Any json to ugly (non-tabular) string
 *  Remember gbmem_free the returned string
 ***************************************************************************/
PUBLIC char *json2uglystr(const json_t *jn) // jn not owned
{
    if(!jn) {
        return 0;
    }
    size_t flags = JSON_ENCODE_ANY | JSON_COMPACT | JSON_REAL_PRECISION(get_real_precision());
    return json_dumps(jn, flags);
}

/***************************************************************************
 *  String element = 1 if not empty string
 ***************************************************************************/
PUBLIC size_t json_size(json_t *value)
{
    if(!value) {
        return 0;
    }
    switch(json_typeof(value)) {
    case JSON_ARRAY:
        return json_array_size(value);
    case JSON_OBJECT:
        return json_object_size(value);
    case JSON_STRING:
        {
            const char *s = json_string_value(value);
            return strlen(s)?1:0;
        }
    default:
        return 0;
    }
}

/***************************************************************************
 *  Check deeply the refcount of kw
 *  TODO use debug_json()
 ***************************************************************************/
PUBLIC int json_check_refcounts(
    json_t *jn, // not owned
    int max_refcount,
    int *result // firstly, initialize to 0
)
{
    if(!jn) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "jn NULL",
            NULL
        );
        if(result) {
            (*result)--;
        }
    }

    switch(json_typeof(jn)) {
    case JSON_ARRAY:
        {
            if(jn->refcount <= 0) {
                gobj_log_error(0,0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "refcount <= 0",
                    "refcount",     "%d", (int)jn->refcount,
                    NULL
                );
                if(result) {
                    (*result)--;
                }
            }

            if(max_refcount > 0 && jn->refcount > max_refcount) {
                gobj_log_error(0,0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "refcount > max_refcount",
                    "refcount",     "%d", (int)jn->refcount,
                    "max_refcount", "%d", (int)max_refcount,
                    NULL
                );
                gobj_trace_json(0, jn, "refcount > max_refcount");
                if(result) {
                    (*result)--;
                }
            }

            int idx; json_t *jn_value;
            json_array_foreach(jn, idx, jn_value) {
                json_check_refcounts(jn_value, max_refcount, result);
            }
        }
        break;
    case JSON_OBJECT:
        {
            if(jn->refcount <= 0) {
                gobj_log_error(0,0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "refcount <= 0",
                    "refcount",     "%d", (int)jn->refcount,
                    NULL
                );
                if(result) {
                    (*result)--;
                }
            }

            if(max_refcount > 0 && jn->refcount > max_refcount) {
                gobj_log_error(0,0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "refcount > max_refcount",
                    "refcount",     "%d", (int)jn->refcount,
                    "max_refcount", "%d", (int)max_refcount,
                    NULL
                );
                gobj_trace_json(0, jn, "refcount > max_refcount");
                if(result) {
                    (*result)--;
                }
            }

            const char *key; json_t *jn_value;
            json_object_foreach(jn, key, jn_value) {
                json_check_refcounts(jn_value, max_refcount, result);
            }
        }
        break;

    case JSON_INTEGER:
    case JSON_REAL:
    case JSON_STRING:
        if(jn->refcount <= 0) {
            gobj_log_error(0,0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "refcount <= 0",
                "refcount",     "%d", (int)jn->refcount,
                NULL
            );
            if(result) {
                (*result)--;
            }
        }
        if(max_refcount > 0 && jn->refcount > max_refcount) {
            gobj_log_error(0,0,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "refcount > max_refcount",
                "refcount",     "%d", (int)jn->refcount,
                "max_refcount", "%d", (int)max_refcount,
                NULL
            );
            if(result) {
                (*result)--;
            }
        }
        break;
    case JSON_TRUE:
    case JSON_FALSE:
    case JSON_NULL:
        // These have -1 refcount
        break;
    default:
        gobj_log_error(0,0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "json corrupted",
            "refcount",     "%d", (int)jn->refcount,
            "max_refcount", "%d", (int)max_refcount,
            NULL
        );
        if(result) {
            (*result)--;
        }
    }

    if(result) {
        return *result;
    } else {
        return 0;
    }
}

/***************************************************************************
 *  Print deeply the refcount of kw
 ***************************************************************************/
PUBLIC int json_print_refcounts(
    json_t *jn, // not owned
    int level
)
{
    size_t flags = JSON_COMPACT|JSON_ENCODE_ANY;

    if(!jn) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "jn NULL",
            NULL
        );
    }

    int i = level*4;
    while(i>0) {
        fprintf(stdout, " ");
        i--;
    }

    switch(json_typeof(jn)) {
    case JSON_ARRAY:
        {
            fprintf(stdout, "list: %d ==>", (int)jn->refcount);
            json_dumpf(jn, stdout, flags);
            fprintf(stdout, "\n");

            level++;

            int idx; json_t *jn_value;
            json_array_foreach(jn, idx, jn_value) {
                json_print_refcounts(jn_value, level);
            }
            level--;
        }
        break;
    case JSON_OBJECT:
        {
            fprintf(stdout, "dict: %d ==>", (int)jn->refcount);
            json_dumpf(jn, stdout, flags);
            fprintf(stdout, "\n");

            level++;

            const char *key; json_t *jn_value;
            json_object_foreach(jn, key, jn_value) {
                json_print_refcounts(jn_value, level);
            }
            level--;
        }
        break;

    case JSON_INTEGER:
        fprintf(stdout, "   i: %d ==>", (int)jn->refcount);
        json_dumpf(jn, stdout, flags);
        fprintf(stdout, "\n");
        break;
    case JSON_REAL:
        fprintf(stdout, "   r: %d ==>", (int)jn->refcount);
        json_dumpf(jn, stdout, flags);
        fprintf(stdout, "\n");
        break;
    case JSON_STRING:
        fprintf(stdout, "   s: %d ==>", (int)jn->refcount);
        json_dumpf(jn, stdout, flags);
        fprintf(stdout, "\n");
        break;

    case JSON_TRUE:
    case JSON_FALSE:
    case JSON_NULL:
        // These have -1 refcount
        break;
    default:
        gobj_log_error(0,0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "json corrupted",
            NULL
        );
    }

    fprintf(stdout, "\n");

    return 0;
}

/***************************************************************************
 *  Check if a item are in `list` array:
 ***************************************************************************/
PUBLIC BOOL json_str_in_list(hgobj gobj, json_t *jn_list, const char *str, BOOL ignore_case)
{
    size_t idx;
    json_t *jn_str;

    if(!json_is_array(jn_list)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "list MUST BE a json array",
            NULL
        );
        gobj_trace_json(gobj, jn_list, "list MUST BE a json array");
        return FALSE;
    }
    if(!str) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "str NULL",
            NULL
        );
        return FALSE;
    }

    json_array_foreach(jn_list, idx, jn_str) {
        if(!json_is_string(jn_str)) {
            continue;
        }
        const char *_str = json_string_value(jn_str);
        if(ignore_case) {
            if(strcasecmp(_str, str)==0)
                return TRUE;
        } else {
            if(strcmp(_str, str)==0)
                return TRUE;
        }
    }

    return FALSE;
}




                    /*---------------------------------*
                     *  ### Walkdir functions
                     *---------------------------------*/




/****************************************************************************
 *
 ****************************************************************************/
PRIVATE int _walk_tree(
    hgobj gobj,
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

    if (!(dir = opendir(root_dir))) {
        // DO NOT take trace of:
        // EACCES Permission denied (when it is a file opened by another, for example)
        // ENOENT No such file or directory (Broken links, for example)
        if(!(errno==EACCES ||errno==ENOENT)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Cannot open directory",
                "path",         "%s", root_dir,
                "error",        "%d", errno,
                "serror",       "%s", strerror(errno),
                NULL
            );
        }
        return -1;
    }

    while ((dent = readdir(dir))) {
        char *dname = dent->d_name;
        if(!strcmp(dname, ".") || !strcmp(dname, "..")) {
            continue;
        }
        if(!(opt & WD_HIDDENFILES) && dname[0] == '.') {
            continue;
        }

        char path[PATH_MAX];
        build_path(path, sizeof(path), root_dir, dname, NULL);

        if(lstat(path, &st) == -1) {
            // DO NOT take trace of:
            // EACCES Permission denied (when it is a file opened by another, for example)
            // ENOENT No such file or directory (Broken links, for example)
            if(!(errno==EACCES ||errno==ENOENT)) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "stat() FAILED",
                    "path",         "%s", path,
                    "error",        "%d", errno,
                    "serror",       "%s", strerror(errno),
                    NULL
                );
            }
            continue;
        }

        type = 0;
        if(S_ISDIR(st.st_mode)) {
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
                if(!(cb)(gobj, user_data, type, path, root_dir, dname, level, opt)) {
                    // returning FALSE: don't want to continue traversing
                    break;
                }
            }
        }

        /* recursively follow dirs */
        if(S_ISDIR(st.st_mode)) {
            if ((opt & WD_RECURSIVE)) {
                _walk_tree(gobj, path, reg, user_data, opt, level, cb);
            }
        }
    }
    closedir(dir);
    return 0;
}

/****************************************************************************
 *  Walk directory tree
 *  Only one pattern is use for all found
 ****************************************************************************/
PUBLIC int walk_dir_tree(
    hgobj gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    walkdir_cb cb,
    void *user_data)
{
    regex_t r;

    if(!is_directory(root_dir)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot open directory",
            "path",         "%s", root_dir,
           NULL
        );
        return -1;
    }
    if(!pattern) {
        pattern = ".*";
    }
    int ret = regcomp(&r, pattern, REG_EXTENDED | REG_NOSUB);
    if(ret!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "regcomp() FAILED",
            "error",        "%d", ret,
           NULL
        );
        return -1;
    }

    ret = _walk_tree(gobj, root_dir, &r, user_data, opt, 0, cb);
    regfree(&r);
    return ret;
}

/***************************************************************************
 *  find_files_with_suffix_array
 ***************************************************************************/
PRIVATE void dir_array_init(
    dir_array_t         *da
)
{
    da->items = NULL;
    da->count = 0;
    da->capacity = 0;
}

PUBLIC void dir_array_free(
    dir_array_t         *da
)
{
    if(da->items) {
        for(size_t i = 0; i < da->count; i++) {
            GBMEM_FREE(da->items[i]);
        }
        GBMEM_FREE(da->items);
    }
    da->items = NULL;
    da->count = 0;
    da->capacity = 0;
}

PRIVATE int dir_array_add(
    dir_array_t         *da,
    const char          *name
)
{
    if(da->count >= da->capacity) {
        size_t new_capacity = (da->capacity == 0) ? 1024 : da->capacity * 2;
        char **new_items = GBMEM_REALLOC(da->items, new_capacity * sizeof(char *));
        if(!new_items) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "No memory to dir_array",
                "sizeof",       "%d", new_capacity * sizeof(char *),
                NULL
            );
            return -1;
        }
        da->items = new_items;
        da->capacity = new_capacity;
    }

    da->items[da->count] = gbmem_strdup(name);
    da->count++;
    return 0;
}

PUBLIC int find_files_with_suffix_array(
    hgobj gobj,
    const char *directory,
    const char *suffix,
    dir_array_t *da
)
{
    struct dirent *entry;

    dir_array_init(da);

    DIR *dir = opendir(directory);
    if(!dir) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot open directory",
            "path",         "%s", directory,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    while((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.' &&
          (entry->d_name[1] == '\0' ||
           (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        int is_file = 0;

        #ifdef DT_REG
        if(entry->d_type == DT_REG) {
            is_file = 1;
        } else if(entry->d_type == DT_UNKNOWN) {
            struct stat st;
            char path[PATH_MAX];

            snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
            if(stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                is_file = 1;
            }
        }
        #else
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if(stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            is_file = 1;
        }
        #endif

        if(!is_file) {
            continue;
        }

        if(suffix && *suffix) {
            size_t name_len = strlen(entry->d_name);
            size_t suffix_len = strlen(suffix);
            if(name_len < suffix_len) {
                continue;
            }
            if(strcmp(entry->d_name + name_len - suffix_len, suffix) != 0) {
                continue;
            }
        }

        dir_array_add(da, entry->d_name);
    }

    closedir(dir);
    return 0;
}

PRIVATE int compare_strings(
    const void *a,
    const void *b
)
{
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sa, sb);
}

PUBLIC void dir_array_sort(
    dir_array_t *da
)
{
    if(da->count > 1) {
        qsort(da->items, da->count, sizeof(char *), compare_strings);
    }
}

/****************************************************************************
 *
 ****************************************************************************/
PRIVATE BOOL fill_array_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *filename, // dname[256]
    int level,
    wd_option opt
) {
    dir_array_t *da = user_data;

    if(opt & WD_ONLY_NAMES) {
        dir_array_add(da, filename);

    } else {
        dir_array_add(da, fullpath);
    }

    return TRUE; // continue traversing tree
}

/****************************************************************************
 *  Get ordered full tree filenames of root_dir
 *  WARNING free return array with dir_array_free()
 ****************************************************************************/
PUBLIC int walk_dir_array(
    hgobj gobj,
    const char *root_dir,
    const char *re, // regular expression
    wd_option opt,
    dir_array_t *da
) {
    regex_t r;

    dir_array_init(da);

    if(!is_directory(root_dir)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot open directory",
            "path",         "%s", root_dir,
           NULL
        );
        return -1;
    }

    int ret = regcomp(&r, re, REG_EXTENDED | REG_NOSUB);
    if(ret!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "regcomp() FAILED",
            "error",        "%d", ret,
           NULL
        );
        return -1;
    }

    /*
     *  Fill the array
     */
    _walk_tree(gobj, root_dir, &r, &da, opt, 0, fill_array_cb);
    regfree(&r);

    return 0;
}

/****************************************************************************
 *  Get ordered full tree filenames of root_dir
 *  WARNING free return array with dir_array_free()
 ****************************************************************************/
PUBLIC int get_ordered_filename_array( // WARNING too slow for thousands of files
    hgobj gobj,
    const char *root_dir,
    const char *re, // regular expression
    wd_option opt,
    dir_array_t *da
) {

    int ret = walk_dir_array(
        gobj,
        root_dir,
        re,
        opt,
        da
    );

    /*
     *  Order the array
     */
    dir_array_sort(da);

    return ret;
}




                    /*---------------------------------*
                     *  ### Time functions
                     *---------------------------------*/




/****************************************************************************
 *              TIME_HELPER2.H
 *              Code from git/date.c
 *
 *
 check_approxidate 'noon today' '2009-08-30 12:00:00'
 check_approxidate 'noon yesterday' '2009-08-29 12:00:00'
 check_approxidate 'January 5th noon pm' '2009-01-05 12:00:00'
+check_approxidate '10am noon' '2009-08-29 12:00:00'

 check_approxidate 'last tuesday' '2009-08-25 19:20:00'
 check_approxidate 'July 5th' '2009-07-05 19:20:00'

 *              Copyright (C) Linus Torvalds, 2005
 *              Copyright (c) Niyamaka, 2018.
 ****************************************************************************/
/*
 * This is like mktime, but without normalization of tm_wday and tm_yday.
 */
PUBLIC time_t tm_to_time_t(const struct tm *tm)
{
    static const int mdays[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    int year = tm->tm_year - 70;
    int month = tm->tm_mon;
    int day = tm->tm_mday;

    if (year < 0 || year > 129) /* algo only works for 1970-2099 */
        return -1;
    if (month < 0 || month > 11) /* array bounds */
        return -1;
    if (month < 2 || (year + 2) % 4)
        day--;
    if (tm->tm_hour < 0 || tm->tm_min < 0 || tm->tm_sec < 0)
        return -1;
    return (year * 365 + (year + 1) / 4 + mdays[month] + day) * 24*60*60UL +
        tm->tm_hour * 60*60 + tm->tm_min * 60 + tm->tm_sec;
}

static const char *month_names[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static const char *weekday_names[] = {
    "Sundays", "Mondays", "Tuesdays", "Wednesdays", "Thursdays", "Fridays", "Saturdays"
};

static time_t gm_time_t(timestamp_t time, int tz)
{
    int minutes;

    minutes = tz < 0 ? -tz : tz;
    minutes = (minutes / 100)*60 + (minutes % 100);
    minutes = tz < 0 ? -minutes : minutes;

    if (minutes > 0) {
        if (unsigned_add_overflows(time, minutes * 60))
            gobj_trace_msg(0, "Timestamp+tz too large: %"PRItime" +%04d",
                time, tz);
    } else if (time < -minutes * 60) {
        gobj_trace_msg(0, "Timestamp before Unix epoch: %"PRItime" %04d", time, tz);
    }
    time += minutes * 60;
    if (date_overflows(time))
        gobj_trace_msg(0, "Timestamp too large for this system: %"PRItime, time);
    return (time_t)time;
}

/*
 * The "tz" thing is passed in as this strange "decimal parse of tz"
 * thing, which means that tz -0100 is passed in as the integer -100,
 * even though it means "sixty minutes off"
 */
static struct tm *time_to_tm(timestamp_t time, int tz, struct tm *tm)
{
    time_t t = gm_time_t(time, tz);
#ifdef WIN32
    gmtime_s(tm, &t);
    return tm;
#else
    return gmtime_r(&t, tm);
#endif
}

static struct tm *time_to_tm_local(timestamp_t time, struct tm *tm)
{
    time_t t = (time_t)time;
#ifdef WIN32
    localtime_s(tm, &t);
    return tm;
#else
    return localtime_r(&t, tm);
#endif
}

/*
 * Fill in the localtime 'struct tm' for the supplied time,
 * and return the local tz.
 */
static time_t local_time_tzoffset(time_t t, struct tm *tm)
{
    time_t t_local;
    time_t offset, eastwest;

#ifdef WIN32
    localtime_s(tm, &t);
#else
    localtime_r(&t, tm);
#endif
    t_local = tm_to_time_t(tm);
    if (t_local == -1)
        return 0; /* error; just use +0000 */
    if (t_local < t) {
        eastwest = -1;
        offset = t - t_local;
    } else {
        eastwest = 1;
        offset = t_local - t;
    }
    offset /= 60; /* in minutes */
    offset = (offset % 60) + ((offset / 60) * 100);
    return offset * eastwest;
}

/*
 * What value of "tz" was in effect back then at "time" in the
 * local timezone?
 */
static int local_tzoffset(timestamp_t time)
{
    struct tm tm;

    if (date_overflows(time))
        gobj_trace_msg(0, "Timestamp too large for this system: %"PRItime, time);

    return local_time_tzoffset((time_t)time, &tm);
}

/***********************************************************************
 *   Get a string with some now! date or time formatted
 ***********************************************************************/
PUBLIC void show_date_relative(
    timestamp_t tim,
    char *timebuf,
    int timebufsize)
{
    time_t tv_sec;
    timestamp_t diff;

    time(&tv_sec);
    if (tv_sec < tim) {
        snprintf(timebuf, timebufsize, _("in the future"));
        return;
    }
    diff = tv_sec - tim;
    if (diff < 90) {
        snprintf(timebuf, timebufsize,
             _("%"PRItime" seconds ago"), diff);
        return;
    }
    /* Turn it into minutes */
    diff = (diff + 30) / 60;
    if (diff < 90) {
        snprintf(timebuf, timebufsize,
             _("%"PRItime" minutes ago"), diff);
        return;
    }
    /* Turn it into hours */
    diff = (diff + 30) / 60;
    if (diff < 36) {
        snprintf(timebuf, timebufsize,
             _("%"PRItime" hours ago"), diff);
        return;
    }
    /* We deal with number of days from here on */
    diff = (diff + 12) / 24;
    if (diff < 14) {
        snprintf(timebuf, timebufsize,
             _("%"PRItime" days ago"), diff);
        return;
    }
    /* Say weeks for the past 10 weeks or so */
    if (diff < 70) {
        snprintf(timebuf, timebufsize,
             _("%"PRItime" weeks ago"),
             (diff + 3) / 7);
        return;
    }
    /* Say months for the past 12 months or so */
    if (diff < 365) {
        snprintf(timebuf, timebufsize,
             _("%"PRItime" months ago"),
             (diff + 15) / 30);
        return;
    }
    /* Give years and months for 5 years or so */
    if (diff < 1825) {
        timestamp_t totalmonths = (diff * 12 * 2 + 365) / (365 * 2);
        timestamp_t years = totalmonths / 12;
        timestamp_t months = totalmonths % 12;
        if (months) {
            char sb[120];
            snprintf(sb, sizeof(sb), _("%"PRItime" years"), years);
            snprintf(timebuf, timebufsize,
                 /* TRANSLATORS: "%s" is "<n> years" */
                 _("%s, %"PRItime" months ago"),
                 sb, months);
        } else
            snprintf(timebuf, timebufsize,
                 _("%"PRItime" years ago"), years);
        return;
    }
    /* Otherwise, just years. Centuries is probably overkill. */
    snprintf(timebuf, timebufsize,
         _("%"PRItime" years ago"),
         (diff + 183) / 365);
}

PUBLIC struct date_mode *date_mode_from_type(enum date_mode_type type)
{
    static struct date_mode mode;
    if (type == DATE_STRFTIME)
        gobj_trace_msg(0, "cannot create anonymous strftime date_mode struct");
    mode.type = type;
    return &mode;
}

PUBLIC const char *show_date(timestamp_t tim, int tz, const struct date_mode *mode)
{
    struct tm *tm;
    static char timebuf[1024];
    struct tm tmbuf = { 0 };

    if (mode->type == DATE_UNIX) {
        snprintf(timebuf, sizeof(timebuf), "%"PRItime, tim);
        return timebuf;
    }

    if (mode->local)
        tz = local_tzoffset(tim);

    if (mode->type == DATE_RAW) {
        snprintf(timebuf, sizeof(timebuf), "%"PRItime" %+05d", tim, tz);
        return timebuf;
    }

    if (mode->type == DATE_RELATIVE) {
        show_date_relative(tim, timebuf, sizeof(timebuf));
        return timebuf;
    }

    if (mode->local)
        tm = time_to_tm_local(tim, &tmbuf);
    else
        tm = time_to_tm(tim, tz, &tmbuf);
    if (!tm) {
        tm = time_to_tm(0, 0, &tmbuf);
        tz = 0;
    }

    if (mode->type == DATE_SHORT)
        snprintf(timebuf, sizeof(timebuf), "%04d-%02d-%02d", tm->tm_year + 1900,
            tm->tm_mon + 1, tm->tm_mday);
    else if (mode->type == DATE_ISO8601)
        snprintf(timebuf, sizeof(timebuf), "%04d-%02d-%02d %02d:%02d:%02d %+05d",
            tm->tm_year + 1900,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            tz);
    else if (mode->type == DATE_ISO8601_STRICT) {
        char sign = (tz >= 0) ? '+' : '-';
        tz = abs(tz);
        snprintf(timebuf, sizeof(timebuf), "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
            tm->tm_year + 1900,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            sign, tz / 100, tz % 100);
    } else if (mode->type == DATE_RFC2822)
        snprintf(timebuf, sizeof(timebuf), "%.3s, %d %.3s %d %02d:%02d:%02d %+05d",
            weekday_names[tm->tm_wday], tm->tm_mday,
            month_names[tm->tm_mon], tm->tm_year + 1900,
            tm->tm_hour, tm->tm_min, tm->tm_sec, tz);
    else if (mode->type == DATE_STRFTIME)
        snprintf(timebuf, sizeof(timebuf), mode->strftime_fmt, tm, tz,
            !mode->local);
    else
        snprintf(timebuf, sizeof(timebuf), "%.3s %.3s %d %02d:%02d:%02d %d%c%+05d",
            weekday_names[tm->tm_wday],
            month_names[tm->tm_mon],
            tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            tm->tm_year + 1900,
            mode->local ? 0 : ' ',
            tz);
    return timebuf;
}

/*
 * Check these. And note how it doesn't do the summer-time conversion.
 *
 * In my world, it's always summer, and things are probably a bit off
 * in other ways too.
 */
static const struct {
    const char *name;
    int offset;
    int dst;
} timezone_names[] = {
    { "IDLW", -12, 0, },    /* International Date Line West */
    { "NT",   -11, 0, },    /* Nome */
    { "CAT",  -10, 0, },    /* Central Alaska */
    { "HST",  -10, 0, },    /* Hawaii Standard */
    { "HDT",  -10, 1, },    /* Hawaii Daylight */
    { "YST",   -9, 0, },    /* Yukon Standard */
    { "YDT",   -9, 1, },    /* Yukon Daylight */
    { "PST",   -8, 0, },    /* Pacific Standard */
    { "PDT",   -8, 1, },    /* Pacific Daylight */
    { "MST",   -7, 0, },    /* Mountain Standard */
    { "MDT",   -7, 1, },    /* Mountain Daylight */
    { "CST",   -6, 0, },    /* Central Standard */
    { "CDT",   -6, 1, },    /* Central Daylight */
    { "EST",   -5, 0, },    /* Eastern Standard */
    { "EDT",   -5, 1, },    /* Eastern Daylight */
    { "AST",   -3, 0, },    /* Atlantic Standard */
    { "ADT",   -3, 1, },    /* Atlantic Daylight */
    { "WAT",   -1, 0, },    /* West Africa */

    { "GMT",    0, 0, },    /* Greenwich Mean */
    { "UTC",    0, 0, },    /* Universal (Coordinated) */
    { "Z",      0, 0, },    /* Zulu, alias for UTC */

    { "WET",    0, 0, },    /* Western European */
    { "BST",    0, 1, },    /* British Summer */
    { "CET",   +1, 0, },    /* Central European */
    { "MET",   +1, 0, },    /* Middle European */
    { "MEWT",  +1, 0, },    /* Middle European Winter */
    { "MEST",  +1, 1, },    /* Middle European Summer */
    { "CEST",  +1, 1, },    /* Central European Summer */
    { "MESZ",  +1, 1, },    /* Middle European Summer */
    { "FWT",   +1, 0, },    /* French Winter */
    { "FST",   +1, 1, },    /* French Summer */
    { "EET",   +2, 0, },    /* Eastern Europe, USSR Zone 1 */
    { "EEST",  +2, 1, },    /* Eastern European Daylight */
    { "WAST",  +7, 0, },    /* West Australian Standard */
    { "WADT",  +7, 1, },    /* West Australian Daylight */
    { "CCT",   +8, 0, },    /* China Coast, USSR Zone 7 */
    { "JST",   +9, 0, },    /* Japan Standard, USSR Zone 8 */
    { "EAST", +10, 0, },    /* Eastern Australian Standard */
    { "EADT", +10, 1, },    /* Eastern Australian Daylight */
    { "GST",  +10, 0, },    /* Guam Standard, USSR Zone 9 */
    { "NZT",  +12, 0, },    /* New Zealand */
    { "NZST", +12, 0, },    /* New Zealand Standard */
    { "NZDT", +12, 1, },    /* New Zealand Daylight */
    { "IDLE", +12, 0, },    /* International Date Line East */
};

static int match_string(const char *date, const char *str)
{
    int i;

    for (i = 0; *date; date++, str++, i++) {
        if (*date == *str)
            continue;
        if (toupper(*date) == toupper(*str))
            continue;
        if (!isalnum(*((unsigned char *)date)))
            break;
        return 0;
    }
    return i;
}

static int skip_alpha(const char *date)
{
    int i = 0;
    do {
        i++;
    } while (isalpha(((unsigned char *)date)[i]));
    return i;
}

/*
* Parse month, weekday, or timezone name
*/
static int match_alpha(const char *date, struct tm *tm, int *offset)
{
    int i;

    for (i = 0; i < 12; i++) {
        int match = match_string(date, month_names[i]);
        if (match >= 3) {
            tm->tm_mon = i;
            return match;
        }
    }

    for (i = 0; i < 7; i++) {
        int match = match_string(date, weekday_names[i]);
        if (match >= 3) {
            tm->tm_wday = i;
            return match;
        }
    }

    for (i = 0; i < ARRAY_SIZE(timezone_names); i++) {
        int match = match_string(date, timezone_names[i].name);
        if (match >= 3 || match == strlen(timezone_names[i].name)) {
            int off = timezone_names[i].offset;

            /* This is bogus, but we like summer */
            off += timezone_names[i].dst;

            /* Only use the tz name offset if we don't have anything better */
            if (*offset == -1)
                *offset = 60*off;

            return match;
        }
    }

    if (match_string(date, "PM") == 2) {
        tm->tm_hour = (tm->tm_hour % 12) + 12;
        return 2;
    }

    if (match_string(date, "AM") == 2) {
        tm->tm_hour = (tm->tm_hour % 12) + 0;
        return 2;
    }

    /* BAD CRAP */
    return skip_alpha(date);
}

static int set_date(int year, int month, int day, struct tm *now_tm, time_t now, struct tm *tm)
{
    if (month > 0 && month < 13 && day > 0 && day < 32) {
        struct tm check = *tm;
        struct tm *r = (now_tm ? &check : tm);
        time_t specified;

        r->tm_mon = month - 1;
        r->tm_mday = day;
        if (year == -1) {
            if (!now_tm)
                return 1;
            r->tm_year = now_tm->tm_year;
        }
        else if (year >= 1970 && year < 2100)
            r->tm_year = year - 1900;
        else if (year > 70 && year < 100)
            r->tm_year = year;
        else if (year < 38)
            r->tm_year = year + 100;
        else
            return -1;
        if (!now_tm)
            return 0;

        specified = tm_to_time_t(r);

        /* Be it commit time or author time, it does not make
         * sense to specify timestamp way into the future.  Make
         * sure it is not later than ten days from now...
         */
        if ((specified != -1) && (now + 10*24*3600 < specified))
            return -1;
        tm->tm_mon = r->tm_mon;
        tm->tm_mday = r->tm_mday;
        if (year != -1)
            tm->tm_year = r->tm_year;
        return 0;
    }
    return -1;
}

static int set_time(long hour, long minute, long second, struct tm *tm)
{
    /* We accept 61st second because of leap second */
    if (0 <= hour && hour <= 24 &&
        0 <= minute && minute < 60 &&
        0 <= second && second <= 60) {
        tm->tm_hour = hour;
        tm->tm_min = minute;
        tm->tm_sec = second;
        return 0;
    }
    return -1;
}

static int is_date_known(struct tm *tm)
{
    return tm->tm_year != -1 && tm->tm_mon != -1 && tm->tm_mday != -1;
}

static int match_multi_number(timestamp_t num, char c, const char *date,
                  char *end, struct tm *tm, time_t now)
{
    struct tm now_tm;
    struct tm *refuse_future;
    long num2, num3;

    num2 = strtol(end+1, &end, 10);
    num3 = -1;
    if (*end == c && isdigit(((unsigned char *)end)[1]))
        num3 = strtol(end+1, &end, 10);

    /* Time? Date? */
    switch (c) {
    case ':':
        if (num3 < 0)
            num3 = 0;
        if (set_time(num, num2, num3, tm) == 0) {
            /*
             * If %H:%M:%S was just parsed followed by: .<num4>
             * Consider (& discard) it as fractional second
             * if %Y%m%d is parsed before.
             */
            if (*end == '.' && isdigit(((unsigned char *)end)[1]) && is_date_known(tm))
                strtol(end + 1, &end, 10);
            break;
        }
        return 0;

    case '-':
    case '/':
    case '.':
        if (!now)
            now = time(NULL);
        refuse_future = NULL;

#ifdef WIN32
            if (gmtime_s(&now_tm, &now))
#else
            if (gmtime_r(&now, &now_tm))
#endif
            refuse_future = &now_tm;

        if (num > 70) {
            /* yyyy-mm-dd? */
            if (set_date(num, num2, num3, NULL, now, tm) == 0)
                break;
            /* yyyy-dd-mm? */
            if (set_date(num, num3, num2, NULL, now, tm) == 0)
                break;
        }
        /* Our eastern European friends say dd.mm.yy[yy]
         * is the norm there, so giving precedence to
         * mm/dd/yy[yy] form only when separator is not '.'
         */
        if (c != '.' &&
            set_date(num3, num, num2, refuse_future, now, tm) == 0)
            break;
        /* European dd.mm.yy[yy] or funny US dd/mm/yy[yy] */
        if (set_date(num3, num2, num, refuse_future, now, tm) == 0)
            break;
        /* Funny European mm.dd.yy */
        if (c == '.' &&
            set_date(num3, num, num2, refuse_future, now, tm) == 0)
            break;
        return 0;
    }
    return end - date;
}

/*
 * Have we filled in any part of the time/date yet?
 * We just do a binary 'and' to see if the sign bit
 * is set in all the values.
 */
static inline int nodate(struct tm *tm)
{
    return (tm->tm_year &
        tm->tm_mon &
        tm->tm_mday &
        tm->tm_hour &
        tm->tm_min &
        tm->tm_sec) < 0;
}

/*
 * We've seen a digit. Time? Year? Date?
 */
static int match_digit(const char *date, struct tm *tm, int *offset, int *tm_gmt)
{
    int n;
    char *end;
    timestamp_t num;

    num = parse_timestamp(date, &end, 10);

    /*
     * Seconds since 1970? We trigger on that for any numbers with
     * more than 8 digits. This is because we don't want to rule out
     * numbers like 20070606 as a YYYYMMDD date.
     */
    if (num >= 100000000 && nodate(tm)) {
        time_t time = num;

#ifdef WIN32
        if (gmtime_s(tm, &time)) {
#else
        if (gmtime_r(&time, tm)) {
#endif
            *tm_gmt = 1;
            return end - date;
        }
    }

    /*
     * Check for special formats: num[-.:/]num[same]num
     */
    switch (*end) {
    case ':':
    case '.':
    case '/':
    case '-':
        if (isdigit(((unsigned char *)end)[1])) {
            int match = match_multi_number(num, *end, date, end, tm, 0);
            if (match)
                return match;
        }
    }

    /*
     * None of the special formats? Try to guess what
     * the number meant. We use the number of digits
     * to make a more educated guess..
     */
    n = 0;
    do {
        n++;
    } while (isdigit(((unsigned char *)date)[n]));

    /* 8 digits, compact style of ISO-8601's date: YYYYmmDD */
    /* 6 digits, compact style of ISO-8601's time: HHMMSS */
    if (n == 8 || n == 6) {
        unsigned int num1 = num / 10000;
        unsigned int num2 = (num % 10000) / 100;
        unsigned int num3 = num % 100;
        if (n == 8)
            set_date(num1, num2, num3, NULL, time(NULL), tm);
        else if (n == 6 && set_time(num1, num2, num3, tm) == 0 &&
             *end == '.' && isdigit(((unsigned char *)end)[1]))
            strtoul(end + 1, &end, 10);
        return end - date;
    }

    /* Four-digit year or a timezone? */
    if (n == 4) {
        if (num <= 1400 && *offset == -1) {
            unsigned int minutes = num % 100;
            unsigned int hours = num / 100;
            *offset = hours*60 + minutes;
        } else if (num > 1900 && num < 2100)
            tm->tm_year = num - 1900;
        return n;
    }

    /*
     * Ignore lots of numerals. We took care of 4-digit years above.
     * Days or months must be one or two digits.
     */
    if (n > 2)
        return n;

    /*
     * NOTE! We will give precedence to day-of-month over month or
     * year numbers in the 1-12 range. So 05 is always "mday 5",
     * unless we already have a mday..
     *
     * IOW, 01 Apr 05 parses as "April 1st, 2005".
     */
    if (num > 0 && num < 32 && tm->tm_mday < 0) {
        tm->tm_mday = num;
        return n;
    }

    /* Two-digit year? */
    if (n == 2 && tm->tm_year < 0) {
        if (num < 10 && tm->tm_mday >= 0) {
            tm->tm_year = num + 100;
            return n;
        }
        if (num >= 70) {
            tm->tm_year = num;
            return n;
        }
    }

    if (num > 0 && num < 13 && tm->tm_mon < 0)
        tm->tm_mon = num-1;

    return n;
}

static int match_tz(const char *date, int *offp)
{
    char *end;
    int hour = strtoul(date + 1, &end, 10);
    int n = end - (date + 1);
    int min = 0;

    if (n == 4) {
        /* hhmm */
        min = hour % 100;
        hour = hour / 100;
    } else if (n != 2) {
        min = 99; /* random crap */
    } else if (*end == ':') {
        /* hh:mm? */
        min = strtoul(end + 1, &end, 10);
        if (end - (date + 1) != 5)
            min = 99; /* random crap */
    } /* otherwise we parsed "hh" */

    /*
     * Don't accept any random crap. Even though some places have
     * offset larger than 12 hours (e.g. Pacific/Kiritimati is at
     * UTC+14), there is something wrong if hour part is much
     * larger than that. We might also want to check that the
     * minutes are divisible by 15 or something too. (Offset of
     * Kathmandu, Nepal is UTC+5:45)
     */
    if (min < 60 && hour < 24) {
        int offset = hour * 60 + min;
        if (*date == '-')
            offset = -offset;
        *offp = offset;
    }
    return end - date;
}

static void date_string(timestamp_t date, int offset, char *buf, int bufsize)
{
    int sign = '+';

    if (offset < 0) {
        offset = -offset;
        sign = '-';
    }
    snprintf(buf, bufsize, "%"PRItime" %c%02d%02d", date, sign, offset / 60, offset % 60);
}

/*
 * Parse a string like "0 +0000" as ancient timestamp near epoch, but
 * only when it appears not as part of any other string.
 */
static int match_object_header_date(const char *date, timestamp_t *timestamp, int *offset)
{
    char *end;
    timestamp_t stamp;
    int ofs;

    if (*date < '0' || '9' < *date)
        return -1;
    stamp = parse_timestamp(date, &end, 10);
    if (*end != ' ' || stamp == TIME_MAX || (end[1] != '+' && end[1] != '-'))
        return -1;
    date = end + 2;
    ofs = strtol(date, &end, 10);
    if ((*end != '\0' && (*end != '\n')) || end != date + 4)
        return -1;
    ofs = (ofs / 100) * 60 + (ofs % 100);
    if (date[-1] == '-')
        ofs = -ofs;
    *timestamp = stamp;
    *offset = ofs;
    return 0;
}

/* Gr. strptime is crap for this; it doesn't have a way to require RFC2822
   (i.e. English) day/month names, and it doesn't work correctly with %z. */
PUBLIC int parse_date_basic(const char *date, timestamp_t *timestamp, int *offset)
{
    struct tm tm;
    int tm_gmt;
    timestamp_t dummy_timestamp;
    int dummy_offset;

    if (!timestamp)
        timestamp = &dummy_timestamp;
    if (!offset)
        offset = &dummy_offset;

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = -1;
    tm.tm_mon = -1;
    tm.tm_mday = -1;
    tm.tm_isdst = -1;
    tm.tm_hour = -1;
    tm.tm_min = -1;
    tm.tm_sec = -1;
    *offset = -1;
    tm_gmt = 0;

    if (*date == '@' &&
        !match_object_header_date(date + 1, timestamp, offset))
        return 0; /* success */
    for (;;) {
        int match = 0;
        unsigned char c = *date;

        /* Stop at end of string or newline */
        if (!c || c == '\n')
            break;

        if (isalpha(c))
            match = match_alpha(date, &tm, offset);
        else if (isdigit(c))
            match = match_digit(date, &tm, offset, &tm_gmt);
        else if ((c == '-' || c == '+') && isdigit(((unsigned char *)date)[1]))
            match = match_tz(date, offset);

        if (!match) {
            /* BAD CRAP */
            match = 1;
        }

        date += match;
    }

    /* do not use mktime(), which uses local timezone, here */
    *timestamp = tm_to_time_t(&tm);
    if (*timestamp == -1)
        return -1;

    if (*offset == -1) {
        time_t temp_time;

        /* gmtime_r() in match_digit() may have clobbered it */
        tm.tm_isdst = -1;
        temp_time = mktime(&tm);
        if ((time_t)*timestamp > temp_time) {
            *offset = ((time_t)*timestamp - temp_time) / 60;
        } else {
            *offset = -(int)((temp_time - (time_t)*timestamp) / 60);
        }
    }

    if (!tm_gmt)
        *timestamp -= *offset * 60;
    return 0; /* success */
}

PUBLIC int parse_expiry_date(const char *date, timestamp_t *timestamp)
{
    int errors = 0;

    if (!strcmp(date, "never") || !strcmp(date, "FALSE"))
        *timestamp = 0;
    else if (!strcmp(date, "all") || !strcmp(date, "now"))
        /*
         * We take over "now" here, which usually translates
         * to the current timestamp.  This is because the user
         * really means to expire everything that was done in
         * the past, and by definition reflogs are the record
         * of the past, and there is nothing from the future
         * to be kept.
         */
        *timestamp = TIME_MAX;
    else
        *timestamp = approxidate_careful(date, &errors);

    return errors;
}

PUBLIC int parse_date(const char *date, char *result, int resultsize)
{
    timestamp_t timestamp;
    int offset;
    if (parse_date_basic(date, &timestamp, &offset))
        return -1;
    date_string(timestamp, offset, result, resultsize);
    return 0;
}

static enum date_mode_type parse_date_type(const char *format, const char **end)
{
    if (skip_prefix(format, "relative", end))
        return DATE_RELATIVE;
    if (skip_prefix(format, "iso8601-strict", end) ||
        skip_prefix(format, "iso-strict", end))
        return DATE_ISO8601_STRICT;
    if (skip_prefix(format, "iso8601", end) ||
        skip_prefix(format, "iso", end))
        return DATE_ISO8601;
    if (skip_prefix(format, "rfc2822", end) ||
        skip_prefix(format, "rfc", end))
        return DATE_RFC2822;
    if (skip_prefix(format, "short", end))
        return DATE_SHORT;
    if (skip_prefix(format, "default", end))
        return DATE_NORMAL;
    if (skip_prefix(format, "raw", end))
        return DATE_RAW;
    if (skip_prefix(format, "unix", end))
        return DATE_UNIX;
    if (skip_prefix(format, "format", end))
        return DATE_STRFTIME;

    gobj_trace_msg(0, "unknown date format %s", format);
    return DATE_NORMAL;
}

PUBLIC void parse_date_format(const char *format, struct date_mode *mode)
{
    const char *p=NULL;

    /* "auto:foo" is "if tty/pager, then foo, otherwise normal" */
    if (skip_prefix(format, "auto:", &p)) {
        if (isatty(1))
            format = p;
        else
            format = "default";
    }

    /* historical alias */
    if (!strcmp(format, "local"))
        format = "default-local";

    mode->type = parse_date_type(format, &p);
    mode->local = 0;

    if (skip_prefix(p, "-local", &p))
        mode->local = 1;

    if (mode->type == DATE_STRFTIME) {
        if (!skip_prefix(p, ":", &p))
            gobj_trace_msg(0, "date format missing colon separator: %s", format);
        snprintf(mode->strftime_fmt, sizeof(mode->strftime_fmt), "%s", p);
    } else if (*p)
        gobj_trace_msg(0, "unknown date format %s", format);
}

PUBLIC void datestamp(char *out, int outsize)
{
    time_t now;
    int offset;
    struct tm tm = { 0 };

    time(&now);

#ifdef WIN32
    localtime_s(&tm, &now);
    offset = tm_to_time_t(&tm) - now;
#else
    offset = tm_to_time_t(localtime_r(&now, &tm)) - now;
#endif
    offset /= 60;

    date_string(now, offset, out, outsize);
}

/*
 * Relative time update (eg "2 days ago").  If we haven't set the time
 * yet, we need to set it from current time.
 */
static time_t update_tm(struct tm *tm, struct tm *now, time_t sec)
{
    time_t n;

    if (tm->tm_mday < 0)
        tm->tm_mday = now->tm_mday;
    if (tm->tm_mon < 0)
        tm->tm_mon = now->tm_mon;
    if (tm->tm_year < 0) {
        tm->tm_year = now->tm_year;
        if (tm->tm_mon > now->tm_mon)
            tm->tm_year--;
    }
    n = mktime(tm) - sec;
#ifdef WIN32
    localtime_s(tm, &n);
#else
    localtime_r(&n, tm);
#endif
    return n;
}

/*
 * Do we have a pending number at the end, or when
 * we see a new one? Let's assume it's a month day,
 * as in "Dec 6, 1992"
 */
static void pending_number(struct tm *tm, int *num)
{
    int number = *num;

    if (number) {
        *num = 0;
        if (tm->tm_mday < 0 && number < 32)
            tm->tm_mday = number;
        else if (tm->tm_mon < 0 && number < 13)
            tm->tm_mon = number-1;
        else if (tm->tm_year < 0) {
            if (number > 1969 && number < 2100)
                tm->tm_year = number - 1900;
            else if (number > 69 && number < 100)
                tm->tm_year = number;
            else if (number < 38)
                tm->tm_year = 100 + number;
            /* We screw up for number = 00 ? */
        }
    }
}

static void date_now(struct tm *tm, struct tm *now, int *num)
{
    *num = 0;
    update_tm(tm, now, 0);
}

static void date_yesterday(struct tm *tm, struct tm *now, int *num)
{
    *num = 0;
    update_tm(tm, now, 24*60*60);
}

static void date_time(struct tm *tm, struct tm *now, int hour)
{
    if (tm->tm_hour < hour)
        update_tm(tm, now, 24*60*60);
    tm->tm_hour = hour;
    tm->tm_min = 0;
    tm->tm_sec = 0;
}

static void date_midnight(struct tm *tm, struct tm *now, int *num)
{
    pending_number(tm, num);
    date_time(tm, now, 0);
}

static void date_noon(struct tm *tm, struct tm *now, int *num)
{
    pending_number(tm, num);
    date_time(tm, now, 12);
}

static void date_tea(struct tm *tm, struct tm *now, int *num)
{
    pending_number(tm, num);
    date_time(tm, now, 17);
}

static void date_pm(struct tm *tm, struct tm *now, int *num)
{
    int hour, n = *num;
    *num = 0;

    hour = tm->tm_hour;
    if (n) {
        hour = n;
        tm->tm_min = 0;
        tm->tm_sec = 0;
    }
    tm->tm_hour = (hour % 12) + 12;
}

static void date_am(struct tm *tm, struct tm *now, int *num)
{
    int hour, n = *num;
    *num = 0;

    hour = tm->tm_hour;
    if (n) {
        hour = n;
        tm->tm_min = 0;
        tm->tm_sec = 0;
    }
    tm->tm_hour = (hour % 12);
}

static void date_never(struct tm *tm, struct tm *now, int *num)
{
    time_t n = 0;
#ifdef WIN32
    localtime_s(tm, &n);
#else
    localtime_r(&n, tm);
#endif
    *num = 0;
}

static const struct special {
    const char *name;
    void (*fn)(struct tm *, struct tm *, int *);
} special[] = {
    { "yesterday", date_yesterday },
    { "noon", date_noon },
    { "midnight", date_midnight },
    { "tea", date_tea },
    { "PM", date_pm },
    { "AM", date_am },
    { "never", date_never },
    { "now", date_now },
    { NULL, NULL }
};

static const char *number_name[] = {
    "zero", "one", "two", "three", "four",
    "five", "six", "seven", "eight", "nine", "ten",
};

static const struct typelen {
    const char *type;
    int length;
} typelen[] = {
    { "seconds", 1 },
    { "minutes", 60 },
    { "hours", 60*60 },
    { "days", 24*60*60 },
    { "weeks", 7*24*60*60 },
    { NULL, 0 }
};

static const char *approxidate_alpha(const char *date, struct tm *tm, struct tm *now, int *num, int *touched)
{
    const struct typelen *tl;
    const struct special *s;
    const char *end = date;
    int i;

    while (isalpha(*((unsigned char *)(++end))))
        ;

    for (i = 0; i < 12; i++) {
        int match = match_string(date, month_names[i]);
        if (match >= 3) {
            tm->tm_mon = i;
            *touched = 1;
            return end;
        }
    }

    for (s = special; s->name; s++) {
        int len = strlen(s->name);
        if (match_string(date, s->name) == len) {
            s->fn(tm, now, num);
            *touched = 1;
            return end;
        }
    }

    if (!*num) {
        for (i = 1; i < 11; i++) {
            int len = strlen(number_name[i]);
            if (match_string(date, number_name[i]) == len) {
                *num = i;
                *touched = 1;
                return end;
            }
        }
        if (match_string(date, "last") == 4) {
            *num = 1;
            *touched = 1;
        }
        return end;
    }

    tl = typelen;
    while (tl->type) {
        int len = strlen(tl->type);
        if (match_string(date, tl->type) >= len-1) {
            update_tm(tm, now, tl->length * *num);
            *num = 0;
            *touched = 1;
            return end;
        }
        tl++;
    }

    for (i = 0; i < 7; i++) {
        int match = match_string(date, weekday_names[i]);
        if (match >= 3) {
            int diff, n = *num -1;
            *num = 0;

            diff = tm->tm_wday - i;
            if (diff <= 0)
                n++;
            diff += 7*n;

            update_tm(tm, now, diff * 24 * 60 * 60);
            *touched = 1;
            return end;
        }
    }

    if (match_string(date, "months") >= 5) {
        int n;
        update_tm(tm, now, 0); /* fill in date fields if needed */
        n = tm->tm_mon - *num;
        *num = 0;
        while (n < 0) {
            n += 12;
            tm->tm_year--;
        }
        tm->tm_mon = n;
        *touched = 1;
        return end;
    }

    if (match_string(date, "years") >= 4) {
        update_tm(tm, now, 0); /* fill in date fields if needed */
        tm->tm_year -= *num;
        *num = 0;
        *touched = 1;
        return end;
    }

    return end;
}

static const char *approxidate_digit(const char *date, struct tm *tm, int *num,
                     time_t now)
{
    char *end;
    timestamp_t number = parse_timestamp(date, &end, 10);

    switch (*end) {
    case ':':
    case '.':
    case '/':
    case '-':
        if (isdigit(((unsigned char *)end)[1])) {
            int match = match_multi_number(number, *end, date, end,
                               tm, now);
            if (match)
                return date + match;
        }
    }

    /* Accept zero-padding only for small numbers ("Dec 02", never "Dec 0002") */
    if (date[0] != '0' || end - date <= 2)
        *num = number;
    return end;
}

static timestamp_t approxidate_str(
    const char *date,
    time_t time_sec,
    int *error_ret)
{
    int number = 0;
    int touched = 0;
    struct tm tm, now;

#ifdef WIN32
    localtime_s(&tm, &time_sec);
#else
    localtime_r(&time_sec, &tm);
#endif



    now = tm;

    tm.tm_year = -1;
    tm.tm_mon = -1;
    tm.tm_mday = -1;

    for (;;) {
        unsigned char c = *date;
        if (!c)
            break;
        date++;
        if (isdigit(c)) {
            pending_number(&tm, &number);
            date = approxidate_digit(date-1, &tm, &number, time_sec);
            touched = 1;
            continue;
        }
        if (isalpha(c))
            date = approxidate_alpha(date-1, &tm, &now, &number, &touched);
    }
    pending_number(&tm, &number);
    if (!touched)
        *error_ret = 1;
    return (timestamp_t)update_tm(&tm, &now, 0);
}

timestamp_t approxidate_relative(const char *date)
{
    time_t tv;
    timestamp_t timestamp;
    int offset;
    int errors = 0;

    if (!parse_date_basic(date, &timestamp, &offset))
        return timestamp;

    time(&tv);
    return approxidate_str(date, tv, &errors);
}

timestamp_t approxidate_careful(const char *date, int *error_ret)
{
    time_t tv;
    timestamp_t timestamp;
    int offset;
    int dummy = 0;
    if (!error_ret)
        error_ret = &dummy;

    if (!parse_date_basic(date, &timestamp, &offset)) {
        *error_ret = 0;
        return timestamp;
    }

    time(&tv);
    return approxidate_str(date, tv, error_ret);
}

PUBLIC int date_overflows(timestamp_t t)
{
    time_t sys;

    /* If we overflowed our timestamp data type, that's bad... */
    if ((uintmax_t)t >= TIME_MAX)
        return 1;

    /*
     * ...but we also are going to feed the result to system
     * functions that expect time_t, which is often "signed long".
     * Make sure that we fit into time_t, as well.
     */
    sys = t;
    return t != sys || (t < 1) != (sys < 1);
}




                    /*---------------------------------*
                     *  ### Utilities functions
                     *---------------------------------*/




/***********************************************************************
 *  byte_to_strhex - sprintf %02X
 *  Usage:
 *        char *byte_to_strhex(char *strh, uint8_t w)
 *  Description:
 *        Print in 'strh' the Hexadecimal ASCII representation of 'w'
 *        Equivalent to sprintf %02X SIN EL NULO
 *        OJO que hay f() que usan esta caracteristica
 *  Return:
 *        Pointer to endp
 ***********************************************************************/
PRIVATE const char tbhexa[]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
PRIVATE inline char * byte_to_strhex(char *s, char w)
{
    *s = tbhexa[ ((w >> 4) & 0x0f) ];
    s++;
    *s = tbhexa[ ((w) & 0x0f) ];
    s++;
    return s;
}

/***********************************************************************
 *  hex2bin
 *  return bf
 ***********************************************************************/
PRIVATE char base16_decoding_table1[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};
PUBLIC char *hex2bin(char *bf, int bfsize, const char *hex, size_t hex_len, size_t *out_len)
{
    size_t i;
    char cur;
    char val;
    for (i = 0; i < hex_len; i++) {
        cur = hex[i];
        val = base16_decoding_table1[(int)cur];
        /* even characters are the first half, odd characters the second half
         * of the current output byte */
        if(val < 0) {
            break; // Found non-hex, break
        }
        if(i/2 >= bfsize) {
            break;
        }
        if (i%2 == 0) {
            bf[i/2] = val << 4;
        } else {
            bf[i/2] |= val;
        }
    }
    if(out_len) {
        *out_len = i/2;
    }
    return bf;
}

/***********************************************************************
 *  bin2hex
 *  return bf
 ***********************************************************************/
PUBLIC char *bin2hex(char *bf, int bfsize, const uint8_t *bin, size_t bin_len)
{
    char *p;
    size_t i, j;

    p = bf;
    for(i=j=0; i<bin_len && j < (bfsize-2); i++, j+=2) {
        p = byte_to_strhex(p, *bin++);
    }
    *p = 0; // final null
    return bf;
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *  nivel 1 -> solo en hexa
 *  nivel 2 -> en hexa y en asci
 *  nivel 3 -> en hexa y en asci y con contador (indice)
 ***********************************************************************/
PUBLIC void tdump(const char *prefix, const uint8_t *s, size_t len, view_fn_t view, int nivel)
{
    static char bf[80+1];
    static char asci[40+1];
    char *p;
    size_t i, j;

    if(!nivel) {
        nivel = 3;
    }
    if(!view) {
        view = printf;
    }
    if(!prefix) {
        prefix = (char *) "";
    }

    p = bf;
    for(i=j=0; i<len; i++) {
        asci[j] = (*s<' ' || *s>0x7f)? '.': *s;
        if(asci[j] == '%')
            asci[j] = '.';
        j++;
        p = byte_to_strhex(p, *s++);
        *p++ = ' ';
        if(j == 16) {
            *p++ = '\0';
            asci[j] = '\0';

            if(nivel==1) {
                view("%s%s\n", prefix, bf);

            } else if(nivel==2) {
                view("%s%s  %s\n", prefix, bf, asci);

            } else {
                view("%s%04X: %s  %s\n", prefix, (int)(i-15), bf, asci);
            }

            p = bf;
            j = 0;
        }
    }
    if(j) {
        len = 16 - j;
        while(len-- >0) {
            *p++ = ' ';
            *p++ = ' ';
            *p++ = ' ';
        }
        *p++ = '\0';
        asci[j] = '\0';

        if(nivel==1) {
           view("%s%s\n", prefix, bf);

        } else if(nivel==2) {
            view("%s%s  %s\n", prefix, bf, asci);

        } else {
            view("%s%04X: %s  %s\n", prefix, (int)(i - j), bf, asci);
        }
    }
}

/***********************************************************************
 *  Vuelca en formato tdump un array de longitud 'len'
 *    Contador, hexa, ascii
 ***********************************************************************/
PUBLIC json_t *tdump2json(const uint8_t *s, size_t len)
{
    char hexa[80+1];
    char asci[40+1];
    char addr[16+1];
    char *p;
    size_t i, j;

    json_t *jn_dump = json_object();

    p = hexa;
    for(i=j=0; i<len; i++) {
        asci[j] = (*s<' ' || *s>0x7f)? '.': *s;
        if(asci[j] == '%')
            asci[j] = '.';
        j++;
        p = byte_to_strhex(p, *s++);
        *p++ = ' ';
        if(j == 16) {
            *p++ = '\0';
            asci[j] = '\0';

            snprintf(addr, sizeof(addr), "%04X", (unsigned int)(i-15));
            json_t *jn_hexa = json_sprintf("%s  %s",  hexa, asci);
            json_object_set_new(jn_dump, addr, jn_hexa);

            p = hexa;
            j = 0;
        }
    }
    if(j) {
        len = 16 - j;
        while(len-- >0) {
            *p++ = ' ';
            *p++ = ' ';
            *p++ = ' ';
        }
        *p++ = '\0';
        asci[j] = '\0';

        snprintf(addr, sizeof(addr), "%04X", (unsigned int)(i-j));
        json_t *jn_hexa = json_sprintf("%s  %s",  hexa, asci);
        json_object_set_new(jn_dump, addr, jn_hexa);
    }

    return jn_dump;
}

/***************************************************************************
 *  Print json to stdout
 ***************************************************************************/
PUBLIC int print_json2(const char *label, json_t *jn)
{
    if(!label) {
        label = "";
    }
    if(!jn || jn->refcount <= 0) {
        fprintf(stdout, "%s (%p) ERROR print_json2(): json %s %d, type %d\n",
            label,
            jn,
            jn?"NULL":"or refcount is",
            jn?(int)(jn->refcount):0,
            jn?(int)(jn->type):0
        );
        return -1;
    }

    size_t flags = JSON_INDENT(4)|JSON_ENCODE_ANY;
    fprintf(stdout, "%s (%p) (refcount: %d, type %d) ==>\n",
        label,
        jn,
        (int)(jn->refcount),
        (int)(jn->type)
    );
    json_dumpf(jn, stdout, flags);
    fprintf(stdout, "\n");
    return 0;
}

/****************************************************************************
 *  Indent, return spaces multiple of depth level gobj.
 *  With this, we can see the trace messages indenting according
 *  to depth level.
 ****************************************************************************/
PRIVATE char *_tab(char *bf, int bflen, int deep)
{
    int i;

    for(i=0; i<deep*4 && i<bflen-1; i++) {
        bf[i] = ' ';
    }
    bf[i] = '\0';
    return bf;
}

/****************************************************************************
 *  Trace machine function
 ****************************************************************************/
PRIVATE void _trace_json(int deep, BOOL verbose, const char *fmt, ...) JANSSON_ATTRS((format(printf, 3, 4)));

PRIVATE void _trace_json(int deep, BOOL verbose, const char *fmt, ...)
{
    if(verbose) {
        va_list ap;
        char bf[2 * 1024];
        _tab(bf, sizeof(bf), deep);

        va_start(ap, fmt);
        int len = (int) strlen(bf);
        vsnprintf(bf + len, sizeof(bf) - len, fmt, ap);
        va_end(ap);

        fprintf(stdout, "%s", bf);
    }
}

/***************************************************************************
 *  Print json with refcounts
 ***************************************************************************/
PRIVATE int _debug_json(int deep, json_t *jn, BOOL inside_list, BOOL inside_dict, BOOL verbose)
{
    if(!jn) {
        fprintf(stdout, "%sERROR _debug_json()%s: json NULL\n", On_Red BWhite, Color_Off);
        return -1;
    }
    if(jn->refcount <= 0) {
        fprintf(stdout, "%sERROR _debug_json()%s: refcount is 0\n", On_Red BWhite, Color_Off);
        return -1;
    }

    int ret = 0;
    if(json_is_array(jn)) {
        size_t idx;
        json_t *jn_value;
        _trace_json(inside_dict?0:deep, verbose, "[ (%d)\n", (int)(jn->refcount));
        deep++;
        json_array_foreach(jn, idx, jn_value) {
            if(idx > 0) {
                _trace_json(0, verbose, ",\n");
            }
            ret += _debug_json(deep, jn_value, 1, 0, verbose);
        }
        deep--;
        _trace_json(0, verbose, "\n");
        _trace_json(deep, verbose, "]");
        if(!inside_dict) {
            _trace_json(deep, verbose, "\n");
        }

    } else if(json_is_object(jn)) {
        const char *key;
        json_t *jn_value;

        _trace_json(inside_dict?0:deep, verbose, "{ (%d)\n", (int)(jn->refcount));
        deep++;
        int idx = 0;
        int max_idx = (int)json_object_size(jn);
        json_object_foreach(jn, key, jn_value) {
            _trace_json(deep, verbose, "\"%s\": ", key);
            ret += _debug_json(deep, jn_value, 0, 1, verbose);
            idx++;
            if(idx < max_idx) {
                _trace_json(0, verbose, ",\n");
            } else {
               _trace_json(0, verbose, "\n");
            }
        }
        deep--;
        _trace_json(deep, verbose, "}");
    } else if(json_is_string(jn)) {
        _trace_json(inside_list?deep:0, verbose, "\"%s\" (%d)", json_string_value(jn), (int)(jn->refcount));
    } else if(json_is_integer(jn)) {
        _trace_json(inside_list?deep:0, verbose, "%"JSON_INTEGER_FORMAT" (%d)", json_integer_value(jn), (int)(jn->refcount));
    } else if(json_is_real(jn)) {
        _trace_json(inside_list?deep:0, verbose, "%.2f (%d)", json_real_value(jn), (int)(jn->refcount));
    } else if(json_is_true(jn))  {
        _trace_json(inside_list?deep:0, verbose, "TRUE");
    } else if(json_is_false(jn)) {
        _trace_json(inside_list?deep:0, verbose, "FALSE");
    } else if(json_is_null(jn)) {
        _trace_json(inside_list?deep:0, verbose, "null");
    } else {
        fprintf(stdout, "%sERROR _debug_json()%s: What???\n", On_Red BWhite, Color_Off);
        _trace_json(inside_list?deep:0, verbose, "What???");
        return -1;
    }

    return ret;
}

/***************************************************************************
 *  Print json with refcounts
 ***************************************************************************/
PUBLIC int debug_json(const char *label, json_t *jn, BOOL verbose)
{
    if(!jn || jn->refcount <= 0) {
        fprintf(stdout, "%sERROR debug_json()%s: json NULL or refcount is 0\n",
            On_Red BWhite, Color_Off);
        return -1;
    }
    if(verbose) {
        fprintf(stdout, "%s\n", label);
    }
    int ret = _debug_json(0, jn, 0, 0, verbose);
    if(verbose) {
        fprintf(stdout, "\n");
        fflush(stdout);
    }
    return ret;
}

/*****************************************************************
 *  timestamp with usec resolution
 *  `bf` must be 90 bytes minimum
 *****************************************************************/
PUBLIC char *current_timestamp(char *bf, size_t bfsize)
{
    struct timespec ts;
    struct tm *tm;
    char stamp[64], zone[16];
    clock_gettime(CLOCK_REALTIME, &ts);
    tm = localtime(&ts.tv_sec);

    strftime(stamp, sizeof (stamp), "%Y-%m-%dT%H:%M:%S", tm);
    strftime(zone, sizeof (zone), "%z", tm);
    snprintf(bf, bfsize, "%s.%09lu%s", stamp, ts.tv_nsec, zone);
    return bf;
}

/*****************************************************************
 *  Get timestamp from tm struct
 *****************************************************************/
PUBLIC char *tm2timestamp(char *bf, int bfsize, struct tm *tm)
{
    strftime(bf, bfsize, "%Y-%m-%dT%H:%M:%S.0%z", tm);
    return bf;
}

/*****************************************************************
 *  Get timestamp from t (zulu or local)
 *****************************************************************/
PUBLIC char *t2timestamp(char *bf, int bfsize, time_t t, BOOL local)
{
    struct tm *tm;

    if(local) {
        tm = localtime(&t);
    } else {
        tm = gmtime(&t);
    }

    strftime(bf, bfsize, "%Y-%m-%dT%H:%M:%S.0%z", tm);
    return bf;
}

/****************************************************************************
 *   Arranca un timer de 'seconds' segundos.
 *   El valor retornado es el que hay que usar en la funcion test_timer()
 *   para ver si el timer ha cumplido.
 ****************************************************************************/
PUBLIC time_t start_sectimer(time_t seconds)
{
    time_t timer;

    time(&timer);
    timer += seconds;
    return timer;
}

/****************************************************************************
 *   Retorna TRUE si ha cumplido el timer 'value', FALSE si no.
 ****************************************************************************/
PUBLIC BOOL test_sectimer(time_t value)
{
    time_t timer_actual;

    if(value <= 0) {
        // No value no test TRUE
        return FALSE;
    }
    time(&timer_actual);
    return (timer_actual>=value)? TRUE:FALSE;
}

/****************************************************************************
 *   Arranca un timer de 'milliseconds' mili-segundos.
 *   El valor retornado es el que hay que usar en la funcion test_msectimer()
 *   para ver si el timer ha cumplido.
 ****************************************************************************/
PUBLIC uint64_t start_msectimer(uint64_t milliseconds)
{
    uint64_t ms = time_in_milliseconds_monotonic();
    ms += milliseconds;
    return ms;
}

/****************************************************************************
 *   Retorna TRUE si ha cumplido el timer 'value', FALSE si no.
 ****************************************************************************/
PUBLIC BOOL test_msectimer(uint64_t value)
{
    if(value == 0) {
        // No value no test TRUE
        return FALSE;
    }

    uint64_t ms = time_in_milliseconds_monotonic();

    return (ms>=value)? TRUE:FALSE;
}

/****************************************************************************
 *  Return CLOCK_MONOTONIC time in milliseconds
 ****************************************************************************/
PUBLIC uint64_t time_in_milliseconds_monotonic(void)
{
    struct timespec spec;

    clock_gettime(CLOCK_MONOTONIC, &spec);

    return ((uint64_t)spec.tv_sec) * 1000 + ((uint64_t)spec.tv_nsec) / 1000000;
}

/****************************************************************************
 *  Current **real** time in milliseconds
 ****************************************************************************/
PUBLIC uint64_t time_in_milliseconds(void)
{
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    // Convert to milliseconds
    return ((uint64_t)spec.tv_sec)*1000 + ((uint64_t)spec.tv_nsec)/1000000;
}

/***************************************************************************
 *  Return current time in seconds (standard time(&t))
 ***************************************************************************/
PUBLIC uint64_t time_in_seconds(void)
{
    time_t __t__;
    time(&__t__);
    return (uint64_t) __t__;
}

/****************************************************************************
 *  Return process CPU time (utime + stime) in jiffies
 ****************************************************************************/
PUBLIC uint64_t cpu_usage(void)
{
    FILE *fp = fopen("/proc/self/stat", "r");
    if(!fp) {
        return 0;
    }

    uint64_t utime = 0;
    uint64_t stime = 0;

    int res = fscanf(fp,
        "%*d %*s %*c %*d %*d %*d %*d %*d "
        "%*u %*u %*u %*u %*u "
        "%lu %lu",
        &utime, &stime
    );
    if(res < 0) {}
    fclose(fp);

    return utime + stime;
}

/****************************************************************************
 *  Return CPU usage percent of current process over interval
 *
 *  Parameters:
 *      last_cpu_ticks:  IN/OUT → previous cpu_ticks (jiffies)
 *      last_ms:         IN/OUT → previous timestamp (milliseconds)
 *
 *  Return:
 *      CPU usage % (float), 0.0 - 100.0
 ****************************************************************************/
PUBLIC double cpu_usage_percent(
    uint64_t *last_cpu_ticks,
    uint64_t *last_ms
)
{
    uint64_t cpu_ticks = cpu_usage();  // utime + stime in jiffies
    uint64_t ms = time_in_milliseconds_monotonic();
    long ticks_per_sec = sysconf(_SC_CLK_TCK);

    if(*last_ms == 0) {
        *last_ms = ms;
        *last_cpu_ticks = cpu_ticks;
        return 0.0;
    }

    uint64_t delta_ms = ms - *last_ms;
    if(delta_ms == 0) {
        return 0.0;
    }

    uint64_t delta_cpu_ticks = cpu_ticks - *last_cpu_ticks;

    // Expected jiffies for this interval
    double expected_jiffies = (double)(delta_ms * ticks_per_sec) / 1000.0;

    // CPU usage % of ONE core
    double cpu_percent = ((double)delta_cpu_ticks / expected_jiffies) * 100.0;

    *last_ms = ms;
    *last_cpu_ticks = cpu_ticks;

    return cpu_percent;
}

/***************************************************************************
 *  Convert a 64-bit integer to network byte order
 ***************************************************************************/
PUBLIC uint64_t htonll(uint64_t value)
{
    // If the system is little-endian, reverse the byte order
    if (__BYTE_ORDER == __LITTLE_ENDIAN) {
        uint32_t high_part = htonl((uint32_t)(value >> 32));
        uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFFLL));
        return (((uint64_t)low_part) << 32) | high_part;
    } else {
        return value;
    }
}

/***************************************************************************
 *  Convert a 64-bit integer to host byte order
 ***************************************************************************/
PUBLIC uint64_t ntohll(uint64_t value)
{
    // If the system is little-endian, reverse the byte order
    if (__BYTE_ORDER == __LITTLE_ENDIAN) {
        uint32_t high_part = ntohl((uint32_t)(value >> 32));
        uint32_t low_part = ntohl((uint32_t)(value & 0xFFFFFFFFLL));
        return (((uint64_t)low_part) << 32) | high_part;
    } else {
        return value;
    }
}

//#include <stdio.h>
//#include <stdlib.h>
//#include <dirent.h>
//#include <unistd.h>
//#include <limits.h>
//#include <string.h>

/***************************************************************************
 *  Convert a 64-bit integer to host byte order
 ***************************************************************************/
PUBLIC void list_open_files(void)
{
    char fd_dir_path[PATH_MAX];
    char file_path[2*PATH_MAX];
    char resolved_path[PATH_MAX];
    struct dirent *entry;
    DIR *dir;

    // Get the directory path for the file descriptors of the current process
    snprintf(fd_dir_path, sizeof(fd_dir_path), "/proc/self/fd");

    // Open the /proc/self/fd directory
    dir = opendir(fd_dir_path);
    if (dir == NULL) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot open directory",
            "path",         "%s", fd_dir_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return;
    }

    // Iterate over each entry in the directory
    while ((entry = readdir(dir)) != NULL) {
        // Skip the '.' and '..' entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct the full path to the file descriptor link
        snprintf(file_path, sizeof(file_path), "%s/%s", fd_dir_path, entry->d_name);

        // Resolve the symbolic link to get the actual file path
        ssize_t len = readlink(file_path, resolved_path, sizeof(resolved_path) - 1);
        if (len != -1) {
            resolved_path[len] = '\0';
            printf("FD %s: %s\n", entry->d_name, resolved_path);
        } else {
            perror("readlink");
        }
    }

    // Close the directory
    closedir(dir);
}

/***********************************************************************
 *   Get a string with some now! date formatted
 *   DEPRECATED use strftime()
 ***********************************************************************/
PUBLIC char *formatdate(time_t t, char *bf, int bfsize, const char *format)
{
    char sfechahora[64];

    struct tm *tm;
    tm = localtime(&t);

    /* Pon en formato DD/MM/CCYY-W-ZZZ */
    snprintf(sfechahora, sizeof(sfechahora), "%02d/%02d/%4d-%d-%03d",
             tm->tm_mday,            // 01-31
             tm->tm_mon+1,           // 01-12
             tm->tm_year + 1900,
             tm->tm_wday+1,          // 1-7
             tm->tm_yday+1           // 001-365
    );
    if(empty_string(format)) {
        format = "DD/MM/CCYY-W-ZZZ";
    }

    translate_string(bf, bfsize, sfechahora, format, "DD/MM/CCYY-W-ZZZ");

    return bf;
}

/***************************************************************************
 *    Cuenta cuantos caracteres de 'c' hay en 's'
 ***************************************************************************/
PUBLIC int count_char(const char *s, char c)
{
    int count = 0;

    while(*s) {
        if(*s == c)
            count++;
        s++;
    }
    return count;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC const char *get_hostname(void)
{
    static char hostname[64 + 1] = {0};

    if(!*hostname) {
#ifdef __linux__
        FILE *file = fopen("/etc/hostname", "r");
        if(file) {
            if(fgets(hostname, sizeof(hostname), file)) {};
            fclose(file);
            left_justify(hostname);
        }
        // Merde dynamic library dependency, gethostname(hostname, sizeof(hostname)-1);
#elif defined(ESP_PLATFORM)
        // TODO improve
        snprintf(hostname, sizeof(hostname), "%s", "esp32");
#else
    #error "What S.O.?"
#endif
    }
    return hostname;
}

/***************************************************************************
 *  Create a cryptographically secure UUID without OpenSSL or mbedTLS
 ***************************************************************************/
PUBLIC int create_random_uuid(char *bf, int bfsize) {
    if (bfsize > 0) {
        *bf = 0;
    }
    if (bfsize < 37) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "buffer TOO small",
            NULL
        );
        return -1;
    }

    unsigned char uuid[16];

#ifdef ESP_PLATFORM
    // Use ESP-IDF's built-in cryptographically secure random function
    esp_fill_random(uuid, sizeof(uuid));
#elif __linux__
    // Use getrandom() for cryptographic random bytes
    ssize_t ret = getrandom(uuid, sizeof(uuid), 0);
    if (ret != sizeof(uuid)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_RUNTIME_ERROR,
            "msg",      "%s", "getrandom failed",
            "errno",    "%d", errno,
            NULL
        );
        return -1;
    }
#else
    #error "No secure random generator available for this platform."
#endif

    // Set the version to 4 (randomly generated UUID)
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    // Set the variant to DCE 1.1
    uuid[8] = (uuid[8] & 0x3F) | 0x80;

    // Format the UUID
    snprintf(bf, bfsize,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5],
        uuid[6], uuid[7],
        uuid[8], uuid[9],
        uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]
    );

    return 0;
}

/***************************************************************************
 *  Node uuid
 ***************************************************************************/
#ifdef __linux__
PRIVATE void save_node_uuid(void)
{
    char *directory = PATH_UUID;
    json_t *jn_uuid = json_object();
    json_object_set_new(jn_uuid, "uuid", json_string(node_uuid_));

    save_json_to_file(
        NULL,
        directory,
        "uuid.json",
        02770,
        0660,
        0,
        TRUE,   //create
        FALSE,  //only_read
        jn_uuid // owned
    );
}
#endif

/***************************************************************************
 *  Node uuid
 ***************************************************************************/
#ifdef __linux__
PRIVATE int read_node_uuid(void)
{
   json_t *jn_uuid = load_json_from_file(
       NULL,
        PATH_UUID,
        "uuid.json",
        0
    );

    if(jn_uuid) {
        const char *uuid_ = kw_get_str(0, jn_uuid, "uuid", "", KW_REQUIRED);
        snprintf(node_uuid_, sizeof(node_uuid_), "%s", uuid_);
        json_decref(jn_uuid);
        return 0;
    }
    return -1;
}
#endif

/***************************************************************************
 *  Node uuid
 *  Read uuid: get node uuid in node_uuid_
 *  Create if it doesn't exist.
 ***************************************************************************/
#ifdef __linux__
PRIVATE int create_node_uuid(void)
{
    read_node_uuid(); // get node uuid in node_uuid_
    if(!empty_string(node_uuid_)) {
        return 0;
    }

    // TODO improve, I use node_uuid to identify the machine, be more depending of machine instead random
    create_random_uuid(node_uuid_, sizeof(node_uuid_));
    save_node_uuid();
    return 0;
}
#endif

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char *node_uuid(void)
{
    if(!node_uuid_[0]) {
#ifdef ESP_PLATFORM
        uint8_t mac_addr[6] = {0};
        esp_efuse_mac_get_default(mac_addr);
        snprintf(node_uuid_, sizeof(node_uuid_), "ESP32-%02X-%02X-%02X-%02X-%02X-%02X",
            mac_addr[0],
            mac_addr[1],
            mac_addr[2],
            mac_addr[3],
            mac_addr[4],
            mac_addr[5]
        );
        gobj_log_info(0, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Mac address",
            "efuse_mac",    "%s", node_uuid_,
            "uuid",         "%s", node_uuid_,
            NULL
        );
#elif defined(__linux__)
        create_node_uuid();
#else
    #error "What S.O.?"
#endif
    }
    return node_uuid_;
}

/***************************************************************************
 *  Metadata key (variable) has a prefix of 2 underscore
 ***************************************************************************/
PUBLIC BOOL is_metadata_key(const char *key)
{
    if(!key) {
        return FALSE;
    }
    int i;
    for(i = 0; i < strlen(key); i++) {
        if (key[i] != '_') {
            break;
        }
        if(i > 2) {
            break;
        }
    }
    return (i == 2)?TRUE:FALSE;
}

/***************************************************************************
 *  Private key (variable) has a prefix of 1 underscore
 ***************************************************************************/
PUBLIC BOOL is_private_key(const char *key)
{
    if(!key) {
        return FALSE;
    }
    int i;
    for(i = 0; i < strlen(key); i++) {
        if (key[i] != '_') {
            break;
        }
        if(i > 2) {
            break;
        }
    }
    return (i == 1)?TRUE:FALSE;
}




                    /*------------------------------------*
                     *  ### Common protocols
                     *------------------------------------*/




typedef struct {
    DL_ITEM_FIELDS

    gclass_name_t gclass_name;
    const char *schema;
} comm_prot_t;

PRIVATE volatile char __comm_prot_initialized__ = FALSE;
PRIVATE dl_list_t dl_comm_prot;

/***************************************************************************
 *  Register a gclass with a communication protocol
 ***************************************************************************/
PUBLIC int comm_prot_register(gclass_name_t gclass_name, const char *schema)
{
    if(!__comm_prot_initialized__) {
        __comm_prot_initialized__ = TRUE;
        dl_init(&dl_comm_prot, 0);
    }

    comm_prot_t *lh = GBMEM_MALLOC(sizeof(comm_prot_t));
    if(!lh) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "No memory to comm_prot_t",
            NULL
        );
        return -1;
    }
    lh->schema = gbmem_strdup(schema);
    lh->gclass_name = gclass_name;

    /*----------------*
     *  Add to list
     *----------------*/
    return dl_add(&dl_comm_prot, lh);
}

/***************************************************************************
 *  Get the gclass name implementing the schema
 ***************************************************************************/
PUBLIC gclass_name_t comm_prot_get_gclass(const char *schema)
{
    comm_prot_t *lh = dl_first(&dl_comm_prot);
    while(lh) {
        comm_prot_t *next = dl_next(lh);
        if(strcmp(lh->schema, schema)==0) {
            return lh->gclass_name;
        }
        /*
         *  Next
         */
        lh = next;
    }

    gobj_log_error(0, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
        "msg",          "%s", "gclass for schema not found",
        "schema",       "%s", schema,
        NULL
    );
    return NULL;
}

/***************************************************************************
 *  Free comm_prot register
 ***************************************************************************/
PUBLIC void comm_prot_free(void)
{
    comm_prot_t *lh;
    while((lh=dl_first(&dl_comm_prot))) {
        dl_delete(&dl_comm_prot, lh, 0);
        GBMEM_FREE(lh->schema)
        GBMEM_FREE(lh)
    }

    __comm_prot_initialized__ = FALSE;
}




                    /*------------------------------------*
                     *  ### Daemons
                     *------------------------------------*/




/*
    Returning the First Child's PID: The first fork PID is returned directly.
    This allows the main program to keep a reference to the process that will become the daemon.

    Parent Process Doesn’t Wait: The parent doesn’t wait for the first child,
    so it continues executing immediately after forking.

    Returning pid: launch_program now returns the PID of the first child,
    which will remain the same after the second fork.

    Managing the Process: The main program can now use the returned PID to monitor
    or control the daemonized process, for example,
    by calling kill(child_pid, SIGTERM) to terminate it.
 */

/***************************************************************************
 *  Function to launch a program as a daemonized process and return its PID
 ***************************************************************************/
pid_t launch_daemon(BOOL redirect_stdio_to_null, const char *program, ...)
{
    #define MAX_ARGS 256 // Maximum number of arguments
#ifdef __linux__
    if (program == NULL) {
        print_error(0, "launch_program(): Program name is NULL.");
        return -1;
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        print_error(0, "launch_program() pipe failed '%s'", program);
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        print_error(0, "launch_program() fork failed '%s'", program);
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    } else if (pid > 0) {
        // Parent process
        close(pipe_fd[1]);  // Close write end of the pipe

        // Set the read end of the pipe to non-blocking
        int flags = fcntl(pipe_fd[0], F_GETFL, 0);
        fcntl(pipe_fd[0], F_SETFL, flags | O_NONBLOCK);

        int execvp_status;
        ssize_t bytes = read(pipe_fd[0], &execvp_status, sizeof(execvp_status));
        close(pipe_fd[0]);

        if (bytes > 0) {
            // If we received data, execvp failed
            print_error(0, "launch_program() Failed to launch program: execvp error %d '%s'", execvp_status, program);
            return -1;
        }

        return pid;  // Return PID of the first child
    }

    // First child process: create another child using double fork technique
    pid_t second_pid = fork();
    if (second_pid < 0) {
        print_error(0, "launch_program() second fork failed '%s'", program);
        exit(EXIT_FAILURE);
    } else if (second_pid > 0) {
        // First child exits immediately to avoid zombie
        exit(0);
    }

    // Second child becomes the daemonized process
    if (setsid() == -1) {
        print_error(0, "launch_program() setsid failed '%s'", program);
        exit(EXIT_FAILURE);
    }

    signal(SIGHUP, SIG_IGN);  // Ignore SIGHUP

    // Redirect standard file descriptors to /dev/null
    if(redirect_stdio_to_null) {
        int fd = open("/dev/null", O_RDWR);
        if (fd == -1) {
            print_error(0, "launch_program() Failed to open /dev/null '%s'", program);
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) {
            close(fd);
        }
    }

    // Close the read end of the pipe in the daemon
    close(pipe_fd[0]);

    // Prepare arguments for execvp
    va_list args;
    va_start(args, program);

    const char *arg;
    char *argv[MAX_ARGS];
    int i = 0;

    argv[i++] = (char *)program;
    while ((arg = va_arg(args, const char *)) != NULL && i < MAX_ARGS - 1) {
        argv[i++] = (char *)arg;
    }
    argv[i] = NULL;  // Null-terminate the argument list

    va_end(args);

    // Execute the program
    if (execvp(program, argv) == -1) {
        int err = errno;  // Capture the execvp error code
        print_error(0, "launch_program() execvp failed '%s', errno %d '%s'", program, err, strerror(err));
        int ret = write(pipe_fd[1], &err, sizeof(err));  // Send error to parent
        if(ret < 0) {}
        exit(EXIT_FAILURE);
    }

    close(pipe_fd[1]);  // Close write end of pipe if execvp succeeds

#endif /* __linux__ */
    return 0;  // This line is never reached
}


#ifdef TESTING

/***************************************************************************
 *  Function to handle signals, specifically cleaning up child processes
 ***************************************************************************/
#include <sys/wait.h>

// Function to handle signals, specifically cleaning up child processes
void handle_signals(int sig) {
    if (sig == SIGCHLD) {
        while (waitpid(-1, NULL, WNOHANG) > 0) {}  // Reap zombie processes
    }
}

PRIVATE void handle_signals(int sig)
{
    if (sig == SIGCHLD) {
        // Reap any zombie child processes
        while (waitpid(-1, NULL, WNOHANG) > 0) {}
    }
}

int main() {
    // Set up a signal handler to clean up child processes
    struct sigaction sa;
    sa.sa_handler = &handle_signals;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        print_error(0, "launch_program() sigaction failed");
        return EXIT_FAILURE;
    }

    printf("Launching program without waiting for response.\n");

    // Launch a program as a detached daemon process
    pid_t child_pid = launch_program("nonexistent_program_name", "arg1", "arg2", NULL);
    if (child_pid < 0) {
        print_error(0, "launch_program()Failed to launch program\n");
        return EXIT_FAILURE;
    }

    printf("Launched program with PID: %d\n", child_pid);

    // Continue with other work in the main program
    printf("Continuing with main program execution.\n");

    return EXIT_SUCCESS;
}
#endif




                    /*------------------------------------*
                     *  ### Parse url
                     *------------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int parse_url(
    hgobj gobj,
    const char *uri,
    char *schema, size_t schema_size,
    char *host, size_t host_size,
    char *port, size_t port_size,
    char *path, size_t path_size,
    char *query, size_t query_size,
    BOOL no_schema // only host:port
)
{
    struct http_parser_url u;
    http_parser_url_init(&u);

    if(host) host[0] = 0;
    if(schema) schema[0] = 0;
    if(port) port[0] = 0;
    if(path) path[0] = 0;
    if(query) query[0] = 0;

    if(empty_string(uri)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_INTERNAL_ERROR,
            "msg",                  "%s", "uri EMPTY",
            "url",                  "%s", uri,
            NULL
        );
        return -1;
    }

    int result = http_parser_parse_url(uri, strlen(uri), no_schema, &u);
    if(result != 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_INTERNAL_ERROR,
            "msg",                  "%s", "http_parser_parse_url() FAILED",
            "url",                  "%s", uri,
            NULL
        );
        return -1;
    }

    size_t ln;

    /*
     *  Schema
     */
    if(schema && schema_size > 0) {
        if(!no_schema) {
            ln = u.field_data[UF_SCHEMA].len;
            if(ln > 0) {
                if(ln >= schema_size) {
                    ln = schema_size - 1;
                }
                memcpy(schema, uri + u.field_data[UF_SCHEMA].off, ln);
                schema[ln] = 0;
            }
        }
    }

    /*
     *  Host
     */
    if(host && host_size > 0) {
        ln = u.field_data[UF_HOST].len;
        if(ln > 0) {
            if(ln >= host_size) {
                ln = host_size - 1;
            }
            memcpy(host, uri + u.field_data[UF_HOST].off, ln);
            host[ln] = 0;
        }
    }

    /*
     *  Port
     */
    if(port && port_size > 0) {
        ln = u.field_data[UF_PORT].len;
        if(ln > 0) {
            if(ln >= port_size) {
                ln = port_size - 1;
            }
            memcpy(port, uri + u.field_data[UF_PORT].off, ln);
            port[ln] = 0;
        }
    }

    /*
     *  Path
     */
    if(path && path_size > 0) {
        ln = u.field_data[UF_PATH].len;
        if(ln > 0) {
            if(ln >= path_size) {
                ln = path_size - 1;
            }
            memcpy(path, uri + u.field_data[UF_PATH].off, ln);
            path[ln] = 0;
        }
    }

    /*
     *  Query
     */
    if(query && query_size > 0) {
        ln = u.field_data[UF_QUERY].len;
        if(ln > 0) {
            if(ln >= query_size) {
                ln = query_size - 1;
            }
            memcpy(query, uri + u.field_data[UF_QUERY].off, ln);
            query[ln] = 0;
        }
    }

    return 0;
}

/***************************************************************************
   Get the schema of an url
 ***************************************************************************/
PUBLIC int get_url_schema(
    hgobj gobj,
    const char *uri,
    char *schema, size_t schema_size
) {
    struct http_parser_url u;
    http_parser_url_init(&u);

    if(schema) schema[0] = 0;

    int result = http_parser_parse_url(uri, strlen(uri), FALSE, &u);
    if (result != 0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "http_parser_parse_url() FAILED",
            "url",          "%s", uri,
            NULL
        );
        return -1;
    }

    size_t ln;

    /*
     *  Schema
     */
    if(schema && schema_size > 0) {
        ln = u.field_data[UF_SCHEMA].len;
        if(ln > 0) {
            if(ln >= schema_size) {
                ln = schema_size - 1;
            }
            memcpy(schema, uri + u.field_data[UF_SCHEMA].off, ln);
            schema[ln] = 0;
            return 0;
        }
    }
    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
        "msg",          "%s", "no schema found",
        "url",          "%s", uri,
        NULL
    );
    return -1;
}




                    /*------------------------------------*
                     *  ### Debug
                     *------------------------------------*/




#if CONFIG_DEBUG_WITH_BACKTRACE
struct pass_data {
    loghandler_fwrite_fn_t fwrite_fn;
    void *h;
};

PRIVATE char backtrace_initialized = FALSE;
PRIVATE char program_name[1024];

PRIVATE struct backtrace_state *state = NULL;

PRIVATE void error_callback(void *data_, const char *msg, int errnum)
{
    struct pass_data *data = data_;
    loghandler_fwrite_fn_t fwrite_fn =  data->fwrite_fn;
    void *h = data->h;

    fwrite_fn(h, LOG_DEBUG, "Error: %s (%d)\n", msg, errnum);
}

PRIVATE int full_callback(
    void *data_,
    uintptr_t pc,
    const char *filename,
    int lineno,
    const char *function
)
{
    struct pass_data *data = data_;
    loghandler_fwrite_fn_t fwrite_fn =  data->fwrite_fn;
    void *h = data->h;

    if (filename && function) {
        fwrite_fn(h, LOG_DEBUG, "%-34s %s:%d (0x%lx) ", function, filename, lineno, (unsigned long)pc);
    } else if (filename) {
        fwrite_fn(h, LOG_DEBUG, "%s:%d (0x%lx)", filename, lineno, (unsigned long)pc);
    } else {
        fwrite_fn(h, LOG_DEBUG, "(unknown) (0x%lx)", (unsigned long)pc);
    }
    return 0;
}
#endif

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void show_backtrace_with_backtrace(loghandler_fwrite_fn_t fwrite_fn, void *h)
{
#if CONFIG_DEBUG_WITH_BACKTRACE
    if(!backtrace_initialized) {
        return;
    }
    struct pass_data {
        loghandler_fwrite_fn_t fwrite_fn;
        void *h;
    } data = {
        .fwrite_fn = fwrite_fn,
        .h = h
    };

    // get symbols and print stack trace
    fwrite_fn(h, LOG_DEBUG, "===============> begin stack trace <==================");

    backtrace_full(state, 0, full_callback, error_callback, &data);
#endif
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int init_backtrace_with_backtrace(const char *program)
{
#if CONFIG_DEBUG_WITH_BACKTRACE
    if(!backtrace_initialized) {
        // Initialize the backtrace state
        state = backtrace_create_state(program, 1, error_callback, NULL);

        if (!state) {
            print_error(
                PEF_CONTINUE,
                "Failed to create backtrace state in %s",
                program
            );
            return -1;
        }

        backtrace_initialized = TRUE;
    }
    snprintf(program_name, sizeof(program_name), "%s", program?program:"");
#endif
    return 0;
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

/***************************************************************
 *  Return the free RAM in kilobytes
 ***************************************************************/
PUBLIC unsigned long free_ram_in_kb(void)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if(fp) {
        char line[256];
        unsigned long memfree = 0;
        unsigned long buffers = 0;
        unsigned long cached = 0;
        unsigned long available = 0;

        while(fgets(line, sizeof(line), fp)) {
            if(strncmp(line, "MemAvailable:", 13) == 0) {
                if(sscanf(line + 13, "%lu", &available) == 1) {
                    fclose(fp);
                    return available;
                }
            } else if(strncmp(line, "MemFree:", 8) == 0) {
                sscanf(line + 8, "%lu", &memfree);
            } else if(strncmp(line, "Buffers:", 8) == 0) {
                sscanf(line + 8, "%lu", &buffers);
            } else if(strncmp(line, "Cached:", 7) == 0) {
                sscanf(line + 7, "%lu", &cached);
            }
        }

        fclose(fp);
        return memfree + buffers + cached;
    }

    // Fallback to sysinfo
    struct sysinfo info;
    if(sysinfo(&info) == 0) {
        return (info.freeram * info.mem_unit) / 1024;
    }

    return 0;
}

/***************************************************************
 *  Return the total RAM in kilobytes
 ***************************************************************/
PUBLIC unsigned long total_ram_in_kb(void)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if(fp) {
        char line[256];
        while(fgets(line, sizeof(line), fp)) {
            if(strncmp(line, "MemTotal:", 9) == 0) {
                unsigned long kb = 0;
                if(sscanf(line + 9, "%lu", &kb) == 1) {
                    fclose(fp);
                    return kb;
                }
                break;
            }
        }
        fclose(fp);
    }

    // Fallback to sysinfo if /proc/meminfo fails
    struct sysinfo info;
    if(sysinfo(&info) == 0) {
        return (info.totalram * info.mem_unit) / 1024;
    }

    return 0;  // Unable to determine total RAM
}

/***************************************************************
 * Read the command line of a process into a caller-provided buffer.
 *
 * @param bf        Buffer to store the command line
 * @param bfsize    Size of the buffer in bytes
 * @param pid       Process ID (use 0 for current process)
 * @return          0 on success, -1 on error (errno set)
 ***************************************************************/
PUBLIC int read_process_cmdline(char *bf, size_t bfsize, pid_t pid)
{
    if(!bf || bfsize == 0) {
        errno = EINVAL;
        return -1;
    }

    bf[0] = 0;

    char path[64];
    if(pid == 0) {
        snprintf(path, sizeof(path), "/proc/self/cmdline");
    } else {
        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    }

    FILE *fp = fopen(path, "rb");
    if(!fp) {
        return -1;
    }

    size_t n = fread(bf, 1, bfsize - 1, fp);
    int err = ferror(fp);  // check error before closing
    fclose(fp);

    if(n == 0 && err) {
        return -1;
    }

    // Replace '\0' separators with spaces
    for(size_t i = 0; i < n; i++) {
        if(bf[i] == '\0') {
            bf[i] = ' ';
        }
    }
    bf[n] = '\0';

    return 0;
}

/***************************************************************************
 *  Copy file in kernel mode.
 *  http://stackoverflow.com/questions/2180079/how-can-i-copy-a-file-on-unix-using-c
 ***************************************************************************/
PUBLIC int copyfile(
    const char* source,
    const char* destination,
    int permission,
    BOOL overwrite
) {
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
    size_t nread;
    int result = 0;
    int error = 0;
    char buf[4096];

    while (nread = read(input, buf, sizeof buf), nread > 0 && !error) {
        char *out_ptr = buf;
        size_t nwritten;

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
