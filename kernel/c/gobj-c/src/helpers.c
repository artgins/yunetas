/****************************************************************************
 *              helpers.c
 *
 *              Several helpers
 *
 *              Copyright (c) 2014,2023 Niyamaka.
 *              Copyright (c) 2024-2026, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <locale.h>
#include <grp.h>
#include <pwd.h>

#include <sys/file.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <regex.h>

#include <yuneta_config.h>  /* don't remove */

#ifdef ESP_PLATFORM
#include "esp_system.h" // For esp_fill_random()
#include "esp_mac.h"    // For esp_base_mac_addr_set()
#elif __linux__
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/random.h> // For getrandom()
#include <sys/un.h>
#include <syslog.h>
#include <backtrace.h>
#endif

#include <arpa/inet.h>   // For htonl and htons
#include <netinet/tcp.h>
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

#if defined(__linux__)
  #include <sys/sendfile.h>
#endif

#ifdef ESP_PLATFORM
    #include <esp_log.h>
    #define fstat64 fstat
    #define stat64 stat
    #define syslog(priority, format, ... ) ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, "yuneta", format, ##__VA_ARGS__)
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#include "kwid.h"
#include "helpers.h"

extern void jsonp_free(void *ptr);

/*****************************************************************
 *     Constants
 *****************************************************************/
#define _

#define PATH_UUID "/yuneta/store/agent/uuid"

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
PRIVATE json_t *load_json_from_fd(
    hgobj gobj,
    int fd,
    const char *path,
    log_opt_t on_critical_error
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
#ifdef __linux__
        umask(0);
#endif
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
    int flags = O_CREAT|O_RDWR|O_LARGEFILE|O_CLOEXEC;

    if(!umask_cleared) {
#ifdef __linux__
        umask(0);
#endif
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
    flags |= O_CLOEXEC;

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
 *  Make sure `path` is a directory: create it if it does not exist.
 *  A symbolic link to a directory counts as a directory (stat() follows it).
 ***************************************************************************/
PRIVATE int ensure_one_directory(const char *path, int xpermission)
{
    struct stat st;

    if(stat(path, &st) == 0) {
        if(S_ISDIR(st.st_mode)) {
            return 0;
        }
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Not a directory: the path exists and is not a directory",
            "path",         "%s", path,
            "errno",        "%d", ENOTDIR,
            "serrno",       "%s", strerror(ENOTDIR),
            NULL
        );
        errno = ENOTDIR;
        return -1;
    }
    if(errno != ENOENT) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "stat() FAILED",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    if(newdir(path, xpermission) < 0) {
        int err = errno;
        if(err == EEXIST && is_directory(path)) {
            return 0;   // Created by someone else in between
        }
        // EEXIST without a directory: a dangling symbolic link, for example
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "newdir() FAILED",
            "path",         "%s", path,
            "errno",        "%d", err,
            "serrno",       "%s", strerror(err),
            NULL
        );
        errno = err;
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Function to create directories recursively like "mkdir -p path"
 *  A symbolic link to a directory counts as a directory, as in mkdir -p.
 *  A component that exists and is not a directory is an error, logged with
 *  ENOTDIR; so is a dangling symbolic link. Up to 7.25.4 the first check
 *  could never be true and the second was taken as "exists": both answered 0
 *  with no directory there. A path longer than PATH_MAX is refused with a
 *  log (-1).
 *  On -1 errno is the cause (ENAMETOOLONG, EINVAL for an empty path,
 *  ENOTDIR, or the errno of the stat()/mkdir() that failed), for a caller
 *  that logs it too: up to 7.25.4 the logs in between had changed it.
 ***************************************************************************/
PUBLIC int mkrdir(const char *path, int xpermission)
{
    char tmp[PATH_MAX];

    size_t len = path? strlen(path): 0;
    if(len == 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "path EMPTY",
            NULL
        );
        errno = EINVAL;
        return -1;
    }
    if(len >= sizeof(tmp)) {
        /*
         *  Up to 7.25.4 the path was cut to PATH_MAX without a word, the
         *  cut path was created, and the answer was 0
         */
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Path too long, not created",
            "path",         "%.256s", path,
            "path_len",     "%d", (int)len,
            "PATH_MAX",     "%d", (int)PATH_MAX,
            NULL
        );
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(tmp, path, len + 1);
    if(len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for(char *p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            if(ensure_one_directory(tmp, xpermission) < 0) {
                return -1;  // Error already logged
            }
            *p = '/';
        }
    }

    return ensure_one_directory(tmp, xpermission);
}

/****************************************************************************
 *  stat() of the path itself: a symbolic link is not followed.
 ****************************************************************************/
PRIVATE int stat_no_follow(const char *path, struct stat *st)
{
#ifdef ESP_PLATFORM
    return stat(path, st);  // the ESP32 filesystems have no symbolic links
#else
    return lstat(path, st);
#endif
}

/****************************************************************************
 *  Remove one entry that is not a directory (a file, a symbolic link, ...)
 *  remove() of a symbolic link removes the link, never its target.
 *  An entry already gone (removed by another process during a walk) is
 *  what the caller wants: 0, no log.
 ****************************************************************************/
PRIVATE int remove_non_directory(const char *path)
{
    if(remove(path) != 0) {
        if(errno == ENOENT) {
            return 0;   // Already gone
        }
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "remove() FAILED",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    return 0;
}

/****************************************************************************
 *  The deepest tree that rmrdir() / rmrcontentdir() walk. Each level keeps
 *  one directory open and one small stack frame (the path is ONE buffer
 *  for the whole walk), and a path longer than PATH_MAX is refused, so a
 *  deeper tree is refused with a log, never walked into.
 ****************************************************************************/
#ifdef ESP_PLATFORM
#define MAX_TREE_DEPTH  16
#else
#define MAX_TREE_DEPTH  1024
#endif

/****************************************************************************
 *  Append "/name" to the path of a walk (`path` of `path_len` bytes, in a
 *  buffer of PATH_MAX). Refused with a log when it does not fit: up to
 *  now build_path() dropped the name, and the walk went into the SAME
 *  directory again, forever (a crash on a tree deeper than PATH_MAX).
 *  build_path() is not used: the source and the destination are the same
 *  buffer, and the length must be known before, to refuse.
 ****************************************************************************/
PRIVATE int walk_path_append(char *path, size_t path_len, const char *name)
{
    size_t name_len = strlen(name);
    BOOL sep = (path_len > 0 && path[path_len-1] != '/')? TRUE: FALSE;
    if(path_len + (sep? 1: 0) + name_len >= PATH_MAX) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Path too long, the tree is not removed",
            "path",         "%.256s", path,
            "name",         "%s", name,
            "path_len",     "%d", (int)path_len,
            "PATH_MAX",     "%d", (int)PATH_MAX,
            NULL
        );
        errno = ENAMETOOLONG;
        return -1;
    }
    if(sep) {
        path[path_len++] = '/';
    }
    memcpy(path + path_len, name, name_len + 1);
    return 0;
}

/****************************************************************************
 *  Remove an entry of a walk and, if it is a directory, all its content.
 *  `path` is the buffer of the walk (PATH_MAX): the names of the entries
 *  are appended to it and cut again, so a level costs no path of its own.
 *  An entry that is already gone (another process removed it between the
 *  readdir() and here) is not an error: 0, no log. Up to 7.25.4 that
 *  case returned -1 with no log, and every level above answered -1 as
 *  "Error already logged". Nor is, once, an entry another process made
 *  in a directory while it was walked (its rmdir() fails, ENOTEMPTY): the
 *  directory is walked again; up to 7.25.21 that was an ERROR, even when
 *  the caller's own retry then removed it. A symbolic link is removed,
 *  never descended.
 ****************************************************************************/
PRIVATE int remove_tree_walk(char *path, int depth)
{
    struct stat statbuf;
    struct dirent *dir_entry;
    DIR *dir;

    if(stat_no_follow(path, &statbuf) != 0) {
        if(errno == ENOENT) {
            return 0;   // Already gone
        }
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "lstat() FAILED",
            "path",         "%s", path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    if(!S_ISDIR(statbuf.st_mode)) {
        return remove_non_directory(path);
    }

    if(depth > MAX_TREE_DEPTH) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Tree too deep, it is not removed",
            "path",         "%.256s", path,
            "max_depth",    "%d", MAX_TREE_DEPTH,
            NULL
        );
        return -1;
    }

    for(int pass = 0; ; pass++) {
        dir = opendir(path);
        if(dir == NULL) {
            if(errno == ENOENT) {
                return 0;   // Already gone
            }
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot open directory",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        size_t path_len = strlen(path);
        while((errno = 0, dir_entry = readdir(dir)) != NULL) {   // errno tells the end from a failure
            if(strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
                continue;
            }

            if(walk_path_append(path, path_len, dir_entry->d_name) < 0) {
                closedir(dir);
                return -1;  // Error already logged
            }
            int ret = remove_tree_walk(path, depth + 1);
            path[path_len] = 0;
            if(ret != 0) {
                closedir(dir);
                return -1;  // Error already logged
            }
        }
        if(errno != 0) {
            /*
             *  Up to 7.25.4 a failed readdir() was the end, and the rmdir()
             *  below failed with ENOTEMPTY: the log blamed the rmdir()
             */
            int last_errno = errno;
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot remove directory, readdir() FAILED",
                "path",         "%s", path,
                "errno",        "%d", last_errno,
                "serrno",       "%s", strerror(last_errno),
                NULL
            );
            closedir(dir);
            return -1;
        }

        closedir(dir);

        if(rmdir(path) != 0) {
            if(errno == ENOENT) {
                return 0;   // Already gone
            }
            if((errno == ENOTEMPTY || errno == EEXIST) && pass == 0) {
                continue;   // something made in it while it was walked: once more
            }
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "rmdir() FAILED",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            return -1;
        }

        return 0;
    }
}

/****************************************************************************
 *  Copy a path to the buffer of a walk. Refused with a log when it does
 *  not fit.
 ****************************************************************************/
PRIVATE int walk_path_start(char *bf, const char *path)
{
    size_t len = strlen(path);
    if(len == 0 || len >= PATH_MAX) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", len? "Path too long": "path EMPTY",
            "path",         "%.256s", path,
            "path_len",     "%d", (int)len,
            NULL
        );
        errno = len? ENAMETOOLONG: EINVAL;
        return -1;
    }
    memcpy(bf, path, len + 1);
    return 0;
}

/****************************************************************************
 *  Function to recursively remove a directory and its contents
 *
 *  A symbolic link is removed as a link and NEVER descended: up to 7.25.4
 *  this used stat(), which follows the link, so a link to a directory
 *  was walked into and the files of its target were deleted (outside the
 *  tree), and a dangling link made the whole removal fail.
 *  A path that does not exist returns -1 without a log: callers use
 *  rmrdir() to make sure a directory is gone. An entry INSIDE the tree
 *  that disappears during the walk is not an error (see
 *  remove_tree_walk()). A tree whose paths do not fit in PATH_MAX, or
 *  deeper than MAX_TREE_DEPTH, is refused with a log (-1): what was
 *  walked before is removed, the rest stays.
 ****************************************************************************/
PUBLIC int rmrdir(const char *path)
{
    struct stat statbuf;

    if(stat_no_follow(path, &statbuf) != 0 && errno == ENOENT) {
        return -1;  // Nothing to remove, see the header
    }
    char bf[PATH_MAX];
    if(walk_path_start(bf, path) < 0) {
        return -1;  // Error already logged
    }
    return remove_tree_walk(bf, 0);  // on -1 error already logged
}

/****************************************************************************
 *  Recursively remove the content of a directory
 *  A symbolic link is removed as a link and never descended (see rmrdir()).
 *  A readdir() that fails is a removal that fails: -1, logged, and what
 *  was not read yet stays.
 ****************************************************************************/
PUBLIC int rmrcontentdir(const char *root_dir)
{
    struct dirent *dent;
    DIR *dir;
    char bf[PATH_MAX];

    if(walk_path_start(bf, root_dir) < 0) {
        return -1;  // Error already logged
    }

    if (!(dir = opendir(bf))) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open directory",
            "path",         "%s", root_dir,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    size_t root_len = strlen(bf);
    while ((errno = 0, dent = readdir(dir))) {   // errno tells the end from a failure
        char *dname = dent->d_name;
        if (!strcmp(dname, ".") || !strcmp(dname, "..")) {
            continue;
        }
        if(walk_path_append(bf, root_len, dname) < 0) {
            closedir(dir);
            return -1;  // Error already logged
        }
        int ret = remove_tree_walk(bf, 1);
        bf[root_len] = 0;
        if(ret < 0) {
            closedir(dir);
            return -1;  // Error already logged
        }
    }
    if(errno != 0) {
        /*
         *  Up to 7.25.4 a failed readdir() was the end: 0, with the
         *  entries not read yet still there, and nothing logged
         */
        int last_errno = errno;
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot remove the content of directory, readdir() FAILED",
            "path",         "%s", root_dir,
            "errno",        "%d", last_errno,
            "serrno",       "%s", strerror(last_errno),
            NULL
        );
        closedir(dir);
        return -1;
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
 *  Lexically resolve '.' and '..' in the mutable tail of an assembled
 *  path, clamped so that '..' can never rise above `floor` (the end of
 *  the first/root segment passed to build_path()). The first segment is
 *  the trusted root; every subsequent (often caller/attacker-influenced)
 *  segment is confined under it — a trailing "../../etc" can neutralise
 *  components down to the root but never escape it. In-place; the result
 *  is never longer than the input. Collapses duplicate separators too.
 ***************************************************************************/
PRIVATE void clamp_path_tail(char *bf_start, char *floor)
{
    char *rd = floor;
    char *wr = floor;

    while(*rd) {
        if(*rd == '/') {                    // skip separators (incl. duplicates)
            rd++;
            continue;
        }
        char *comp = rd;
        while(*rd && *rd != '/') {
            rd++;
        }
        size_t len = (size_t)(rd - comp);

        if(len == 1 && comp[0] == '.') {
            continue;                       // drop "."
        }
        if(len == 2 && comp[0] == '.' && comp[1] == '.') {
            if(wr > floor) {                // pop the previous tail component
                char *p = wr;
                while(p > floor && *(p - 1) != '/') {
                    p--;
                }
                if(p > floor && *(p - 1) == '/') {
                    p--;                    // also drop the separator before it
                }
                wr = p;
            }
            continue;                       // at the floor: clamp (drop "..")
        }

        // Normal component: ensure exactly one separator before it
        if(wr > bf_start && *(wr - 1) != '/') {
            *wr++ = '/';
        }
        if(wr != comp) {
            memmove(wr, comp, len);
        }
        wr += len;
    }
    *wr = '\0';
}

/***************************************************************************
 *  Build a path/URL from variadic segments, ensuring exactly one '/'
 *  between each part. No heap allocations — uses pointer arithmetic only.
 *  The first segment is treated as the trusted root: '.'/'..' in any
 *  subsequent segment are resolved lexically and can never escape it
 *  (see clamp_path_tail()), neutralising path-traversal via tail input.
 *  Returns the start of the buffer (bf_start), or NULL when the path does
 *  not fit: logged (an ERROR with the stack), errno ENAMETOOLONG, and the
 *  buffer holds the segments that fitted, terminated -- never a path to
 *  use. Up to 7.29.2 the cut path was returned as if whole, and every
 *  `!build_path()` guard of the tree was dead.
 ***************************************************************************/
PUBLIC char *build_path(char *bf, size_t bfsize, ...)
{
    char *bf_start = bf;
    size_t size = bfsize;
    const char *overflow = NULL;    // the segment that did not fit
    const char *segment;
    va_list ap;
    va_start(ap, bfsize);

    if(!bf || bfsize == 0) {
        va_end(ap);
        return bf;
    }
    *bf = '\0';

    size_t root_len = 0;
    int i = 0;
    while((segment = va_arg(ap, const char *)) != NULL) {
        if(empty_string(segment)) {
            continue;
        }

        const char *start = segment;
        const char *end = segment + strlen(segment);

        if(i == 0) {
            // First segment: keep leading '/', strip trailing '/'
            while(end > start && *(end - 1) == '/') {
                end--;
            }
        } else {
            // Subsequent segments: strip leading and trailing '/'
            while(*start == '/') {
                start++;
            }
            while(end > start && *(end - 1) == '/') {
                end--;
            }
        }

        size_t seg_len = (size_t)(end - start);
        if(seg_len == 0) {
            continue;
        }

        // Add separator
        size_t sep_len = (i > 0) ? 1 : 0;
        if(sep_len + seg_len >= bfsize) {
            overflow = segment;
            break;
        }

        if(sep_len) {
            *bf++ = '/';
            bfsize--;
        }
        memcpy(bf, start, seg_len);
        bf += seg_len;
        bfsize -= seg_len;
        *bf = '\0';

        i++;
        if(i == 1) {
            // End of the first segment: it is the trusted root, locked
            // against '..' clamping of every later segment.
            root_len = (size_t)(bf - bf_start);
        }
    }

    va_end(ap);

    clamp_path_tail(bf_start, bf_start + root_len);

    if(overflow) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Path too long, build_path() cannot build it",
            "path",         "%s", bf_start,
            "segment",      "%.255s", overflow,
            "size",         "%d", (int)size,
            NULL
        );
        errno = ENAMETOOLONG;
        return NULL;
    }
    return bf_start;
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
 *  The bytes of `s` that fit in `max_bytes` without cutting a UTF-8
 *  character: a cut before a continuation byte (10xxxxxx) goes back to the
 *  first byte of its character, which is left out.
 ****************************************************************************/
PUBLIC int utf8_prefix_len(const char *s, int max_bytes)
{
    if(!s || max_bytes <= 0) {
        return 0;
    }
    size_t n = strnlen(s, (size_t)max_bytes);
    if(n < (size_t)max_bytes) {
        return (int)n;  // the whole string fits
    }
    while(n > 0 && (((unsigned char)s[n]) & 0xC0) == 0x80) {
        n--;
    }
    return (int)n;
}

/****************************************************************************
 *  The columns of `s` on a terminal: its UTF-8 characters, counted by
 *  their first byte (anything but a continuation byte, 10xxxxxx).
 ****************************************************************************/
PUBLIC int utf8_columns(const char *s)
{
    int columns = 0;
    if(!s) {
        return 0;
    }
    for(const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if((*p & 0xC0) != 0x80) {
            columns++;
        }
    }
    return columns;
}

/****************************************************************************
 *  The bytes of the longest prefix of `s` that fits in `max_columns`
 *  columns, ending on a character boundary; its columns go to `columns`.
 ****************************************************************************/
PUBLIC int utf8_prefix_columns(const char *s, int max_columns, int *columns)
{
    int cols = 0;
    size_t n = 0;
    if(s && max_columns > 0) {
        while(s[n]) {
            if((((unsigned char)s[n]) & 0xC0) != 0x80) {
                if(cols == max_columns) {
                    break;  // the next character does not fit
                }
                cols++;
            }
            n++;
        }
    }
    if(columns) {
        *columns = cols;
    }
    return (int)n;
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
    const char *chr_pos;

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
 *  The names of the secrets (is_secret_name()). A name that holds one of
 *  these (any case) is a secret. Taken from the attributes and parameters
 *  of the SDK and of the projects (a scan of every SDATA / SDATAPM name,
 *  2026-09-24).
 ***************************************************************************/
PRIVATE const char *secret_name_parts[] = {
    "passw",        // password, passwd, user_passw
    "pwd",
    "passphrase",   // the key of a private key file
    "secret",       // secret, client_secret, kc_admin_client_secret, sign_secret
    "token",        // token, access_token, refresh_token
    "jwt",
    "bearer",
    "authorization",// an HTTP Authorization header: "Bearer <token>", "Basic <user:pass>"
    "cookie",       // http_cookie (the session of a browser); not cookie_domain (a segment, below)
    "credential",
    "salt",         // visitor_salt of webstats: with it the visitors can be told again
    0
};

/*
 *  ... or one of these, once '_', '-', '.' and blanks are taken out
 *  (api_key, apikey, x-api-key, ESIOS_API_KEY; session_id, __session_id__)
 */
PRIVATE const char *secret_name_joined[] = {
    "apikey",       // api_key of wattyzer gate_pvpc (write-attr attribute=api_key value=...)
    "sessionid",    // __session_id__ of c_ievent_srv / c_prot_mqtt2
    "sessionkey",
    "authdata",     // auth_data of c_prot_mqtt2 (the MQTT 5 auth data)
    0
};

/*
 *  ... or one of these as a WHOLE segment, a run of letters and digits
 *  (pass, db_pass, user-pin, sim_pin, otp, user[pin]): inside a word they
 *  are other words (passive, bypass, compass, ping, mapping, spinner, otpx).
 *  As a segment they take ordinary names too -- gpio_pin, a `pass` that
 *  counts passes, first_pass, sum_sla_pass --: accepted, the safe side.
 *  Up to 7.26.9 they were no names (secret_names_t).
 */
PRIVATE const char *secret_name_segments[] = {
    "pass",
    "pin",
    "otp",
    0
};

/*
 *  Not secrets: the PATH of a key or a certificate (ssl_certificate_key,
 *  ssl_trusted_certificate), cert_pem (a public certificate), the ids of
 *  treedb (pkey, rkey, tkey), in_session, mqtt_clean_session,
 *  max_sessions, authz, auth_method, ignore_private. `key` and `cert` are
 *  no parts: a key is the key of a record of timeranger as often as a
 *  credential, and a cert is public.
 *
 *  And a name with one of these SEGMENTS (split by '_', '-', '.' and
 *  blanks) is not a secret, whatever part it holds: it names something
 *  ABOUT a credential, not the credential (token_endpoint, cookie_domain,
 *  jwt_public_keys, refresh_token_count, password_file, secret_name).
 */
PRIVATE const char *not_secret_segments[] = {
    "endpoint", "url", "uri", "domain", "path", "file", "public", "pub",
    "count", "counts", "type", "name", "len", "length", "size", "max", "min",
    "ttl", "timeout", "expiry", "expires", "mode",
    0
};

PRIVATE BOOL is_name_joiner(char c)
{
    return (c == '_' || c == '-' || c == '.' || c == ' ' || c == '\t')? TRUE: FALSE;
}

PRIVATE char name_lower(char c)
{
    return (c >= 'A' && c <= 'Z')? (char)(c + ('a' - 'A')): c;
}

/*
 *  TRUE if `part` (lower case) begins at name[i]
 */
PRIVATE BOOL name_part_at(const char *name, size_t len, size_t i, const char *part, BOOL joined)
{
    size_t j = i;
    size_t k = 0;
    while(part[k]) {
        if(j >= len) {
            return FALSE;
        }
        char c = name[j];
        if(joined && k > 0 && is_name_joiner(c)) {
            j++;
            continue;
        }
        if(name_lower(c) != part[k]) {
            return FALSE;
        }
        j++;
        k++;
    }
    return TRUE;
}

/*
 *  The letters that begin a part (made once from the lists): at any other
 *  byte of a name nothing is compared
 */
PRIVATE BOOL secret_first_letters_done = FALSE;
PRIVATE BOOL secret_first_letters[256];

PRIVATE void make_secret_first_letters(void)
{
    for(int p=0; secret_name_parts[p]; p++) {
        secret_first_letters[(unsigned char)secret_name_parts[p][0]] = TRUE;
    }
    for(int p=0; secret_name_joined[p]; p++) {
        secret_first_letters[(unsigned char)secret_name_joined[p][0]] = TRUE;
    }
    secret_first_letters['p'] = TRUE;  // priv
    secret_first_letters['k'] = TRUE;  // key
    secret_first_letters_done = TRUE;
}

/*
 *  Is the run of `seg_len` bytes at `seg` one of `list` (any case, ASCII:
 *  strncasecmp() of a Turkish locale does not take "PIN" for "pin")?
 */
PRIVATE BOOL name_segment_in(const char *seg, size_t seg_len, const char **list)
{
    if(seg_len == 0) {
        return FALSE;
    }
    char c0 = name_lower(seg[0]);
    for(int n=0; list[n]; n++) {
        if(list[n][0] != c0) {
            continue;
        }
        size_t i = 1;
        while(i < seg_len && list[n][i] && name_lower(seg[i]) == list[n][i]) {
            i++;
        }
        if(i == seg_len && list[n][i] == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

PRIVATE BOOL is_ascii_alnum(char c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))? TRUE: FALSE;
}

/***************************************************************************
 *  TRUE if `name` (of `len` bytes) is the name of a secret.
 *  One pass over the name, no copy: at each byte, only the parts that
 *  begin with that letter are compared. Any case; for the joined parts the
 *  bytes '_', '-', '.' and blanks of the name are skipped. With
 *  `about_wins` a segment that names something about a credential makes
 *  the name not a secret's; with `by_segments` pass, pin and otp as whole
 *  segments are names too.
 ***************************************************************************/
PRIVATE BOOL secret_name_check(const char *name, size_t len, BOOL about_wins, BOOL by_segments)
{
    if(!name || len < 3) {
        return FALSE;   // the shortest part is three letters ("pwd", "jwt", "pin")
    }
    if(!secret_first_letters_done) {
        make_secret_first_letters();
    }

    /*
     *  A segment that names something about a credential
     */
    size_t seg = 0;
    for(size_t i=0; about_wins && i<=len; i++) {
        if(i < len && !is_name_joiner(name[i])) {
            continue;
        }
        if(name_segment_in(name + seg, i - seg, not_secret_segments)) {
            return FALSE;
        }
        seg = i + 1;
    }

    /*
     *  A secret's name as a whole segment (a run of letters and digits)
     */
    seg = 0;
    for(size_t i=0; by_segments && i<=len; i++) {
        if(i < len && is_ascii_alnum(name[i])) {
            continue;
        }
        if(name_segment_in(name + seg, i - seg, secret_name_segments)) {
            return TRUE;
        }
        seg = i + 1;
    }

    BOOL has_priv = FALSE;
    BOOL has_key = FALSE;
    for(size_t i=0; i<len; i++) {
        char c = name_lower(name[i]);
        if(!secret_first_letters[(unsigned char)c]) {
            continue;
        }
        for(int p=0; secret_name_parts[p]; p++) {
            if(secret_name_parts[p][0] == c && name_part_at(name, len, i, secret_name_parts[p], FALSE)) {
                return TRUE;
            }
        }
        for(int p=0; secret_name_joined[p]; p++) {
            if(secret_name_joined[p][0] == c && name_part_at(name, len, i, secret_name_joined[p], TRUE)) {
                return TRUE;
            }
        }
        if(c == 'p' && !has_priv) {
            has_priv = name_part_at(name, len, i, "priv", FALSE);
        } else if(c == 'k' && !has_key) {
            has_key = name_part_at(name, len, i, "key", FALSE);
        }
    }
    return (has_priv && has_key)? TRUE: FALSE;
}

/***************************************************************************
 *  See helpers.h
 ***************************************************************************/
PUBLIC BOOL is_secret_name(const char *name, size_t len)
{
    return secret_name_check(name, len, TRUE, TRUE);
}

/***************************************************************************
 *  See helpers.h
 ***************************************************************************/
PUBLIC BOOL is_secret_name_any(const char *name, size_t len)
{
    return secret_name_check(name, len, FALSE, TRUE);
}

/*
 *  The names a reading judges by: the names of now, or the names of 7.26.9
 *  -- by their parts only, pass, pin and otp no names. A reading of 7.26.9
 *  is kept whole (mark_params(), mark_dump_in_place()): with the names of
 *  now, the value of a name of a segment ("db_pass='password'=x") is jumped
 *  over, and 7.26.9 judged the bytes inside it.
 */
typedef enum {
    NAMES_NOW = 0,
    NAMES_7_26_9,
} secret_names_t;

PRIVATE BOOL is_secret_name_by(const char *name, size_t len, secret_names_t names)
{
    return secret_name_check(name, len, TRUE, (names == NAMES_NOW)? TRUE : FALSE);
}

/*
 *  Masking is reachable from a peer before its session (C_IEVENT_SRV dumps
 *  the kw of an event before the identity card): it is linear, and bounded.
 *  A secret's name is short: no more than MASK_MAX_NAME bytes of a name are
 *  ever asked about. A text, or the json of one call, longer than
 *  MASK_MAX_BYTES is not shown at all (it fails safe, never in clear).
 */
#define MASK_MAX_NAME   128
#define MASK_MAX_BYTES  (4*1024*1024)
#define MASK_TOO_LARGE  "<not shown: too large to mask>"

PRIVATE BOOL is_name_char(char c);

/*
 *  Is the name a secret's, by its LAST MASK_MAX_NAME bytes (the part that
 *  says what it is comes last: <130 x>_password)
 */
PRIVATE BOOL is_secret_name_tail(const char *name, size_t len, secret_names_t names)
{
    if(len > MASK_MAX_NAME) {
        name += len - MASK_MAX_NAME;
        len = MASK_MAX_NAME;
    }
    return is_secret_name_by(name, len, names);
}

/*
 *  Is the word before a '=' or a ':' (`name`, its last MASK_MAX_NAME bytes)
 *  a secret's name, as `names` judges it? pass, pin and otp only in its
 *  part after its last '&', '?' or ';': before it they are words of the
 *  value of another parameter ("q=boarding+pass&lang=es" is no secret).
 */
PRIVATE BOOL is_query_separator(char c)
{
    return (c == '&' || c == '?' || c == ';')? TRUE : FALSE;
}

PRIVATE BOOL is_param_name_secret(const char *name, size_t len, secret_names_t names)
{
    size_t tail = len;
    while(names == NAMES_NOW && tail > 0 && !is_query_separator(name[tail - 1])) {
        tail--;
    }
    if(names != NAMES_NOW || tail == 0) {
        return is_secret_name_by(name, len, names);
    }
    if(is_secret_name_by(name, len, NAMES_7_26_9)) {
        return TRUE;
    }
    return (is_secret_name_by(name, len, NAMES_NOW) &&
        secret_name_check(name + tail, len - tail, FALSE, TRUE))? TRUE : FALSE;
}

/*
 *  The name before the '=' at `eq`: the run of name characters just
 *  before it, no longer than MASK_MAX_NAME (for the bytes of a dump)
 */
PRIVATE const char *name_before(const char *start, const char *eq)
{
    const char *n = eq;
    while(n > start && (size_t)(eq - n) < MASK_MAX_NAME && is_name_char(n[-1])) {
        n--;
    }
    return n;
}

/***************************************************************************
 *  Does `str` (a command line, its `len` bytes) carry "attribute=<a
 *  secret's name>"? Then its "value" is that secret (write-attr
 *  attribute=password value=...), judged by the names of `names`.
 ***************************************************************************/
PRIVATE BOOL inline_value_is_secret(const char *str, size_t len, secret_names_t names)
{
    static const char attribute_eq[] = "attribute=";
    const size_t n = sizeof(attribute_eq) - 1;
    const char *end = str + len;
    for(const char *p = str; p + n <= end; p++) {
        if(*p != 'a' || (p > str && p[-1] != ' ' && p[-1] != '\t') ||
                memcmp(p, attribute_eq, n) != 0) {
            continue;
        }
        const char *v = p + n;
        if(v < end && (*v == '"' || *v == '\'')) {
            v++;
        }
        const char *e = v;
        while(e < end && *e && *e != ' ' && *e != '\t' && *e != '"' && *e != '\'') {
            e++;
        }
        if(is_secret_name_tail(v, (size_t)(e - v), names)) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Where an unquoted secret value that begins at `p` ends: at `limit`, at
 *  a NUL, at the quote that closes an outer quoted value (`outer`), or
 *  before the next "word=" -- a plain word after a blank is still part of
 *  it, as a password written with blanks is ("password=correct horse
 *  battery", "password= hunter2"): the command parser leaves those words
 *  unread, and they are the secret.
 ***************************************************************************/
PRIVATE BOOL closes_outer(const char *c, const char *limit, char outer)
{
    return (outer && *c == outer && (c + 1 >= limit || c[1] == 0 || c[1] == ' ' || c[1] == '\t'))?
        TRUE : FALSE;
}

PRIVATE const char *inline_secret_value_end(const char *p, const char *limit, char outer)
{
    const char *end = p;
    while(1) {
        while(end < limit && *end && *end != ' ' && *end != '\t') {
            if(closes_outer(end, limit, outer)) {
                return end;
            }
            end++;
        }
        if(end >= limit || !*end) {
            return end;
        }
        const char *w = end;    // a blank: what follows is a plain word, or a name=value
        while(w < limit && (*w == ' ' || *w == '\t')) {
            w++;
        }
        if(w >= limit || !*w || closes_outer(w, limit, outer)) {
            return end;
        }
        for(const char *e = w; e < limit && *e && *e != ' ' && *e != '\t' &&
                !closes_outer(e, limit, outer); e++) {
            if(*e == '=' && e > w) {
                return end;     // the next parameter
            }
        }
        end = w;                // a plain word: part of this value
    }
}

/***************************************************************************
 *  A credential by its SHAPE, whatever names it -- the shapes that the
 *  agent's audit record redacts too (audit_record.c, through
 *  mark_secrets_inline()), and the schemes of an HTTP Authorization:
 *    - a JWT or a JWE: "eyJ" (the base64url of '{"') at the start of a
 *      word, base64url, and a '.' of its own after it -- three parts joined
 *      by '.' (a JWS), five (a JWE), or fewer when a cap cut it (a dump of
 *      256 bytes keeps "eyJhdr.eyJpay"). The dots at the end of the run are
 *      not its own (a JWT ends a sentence: "... eyJ.eyJ.sig."), so a run
 *      whose only dots end it is a header and no JWT ("eyJhIjoxfQ."). Where
 *      the text may have been cut (`end_is_cut`: a dump, a window), a run
 *      that reaches its end is one without its '.' too: the cut may have
 *      taken it;
 *    - the credential after the scheme word "Bearer" or "Basic" (any case,
 *      a word of its own): scheme_credential().
 *  On the bytes from `start` to `end`.
 ***************************************************************************/
#define MASK_TOKEN_MIN  8      // a Bearer token is this long at least
#define MASK_PROSE_MAX  24     // a word of prose is shorter

PRIVATE BOOL is_base64url_char(char c)
{
    return (is_ascii_alnum(c) || c == '-' || c == '_')? TRUE : FALSE;
}

PRIVATE BOOL is_ascii_letter(char c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))? TRUE : FALSE;
}

PRIVATE BOOL is_ascii_upper(char c)
{
    return (c >= 'A' && c <= 'Z')? TRUE : FALSE;
}

/*
 *  The `n` bytes at `p` are `word` (lower case) in any case: ASCII only,
 *  whatever the locale (strncasecmp() of a Turkish locale folds 'I' to a
 *  dotless i)
 */
PRIVATE BOOL ascii_case_equal(const char *p, const char *word, size_t n)
{
    for(size_t i = 0; i < n; i++) {
        if(name_lower(p[i]) != word[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

/*
 *  The bytes of the JWT that begins at `p`, 0 if none begins there
 */
PRIVATE size_t jwt_len(const char *p, const char *start, const char *end, BOOL end_is_cut)
{
    if(end - p < 3 || p[0] != 'e' || p[1] != 'y' || p[2] != 'J') {
        return 0;
    }
    if(p > start && (is_base64url_char(p[-1]) || p[-1] == '.')) {
        return 0;   // inside a word, or a part of a dotted run
    }
    const char *e = p;
    BOOL dotted = FALSE;    // a '.' with a part after it: its own
    while(e < end && (is_base64url_char(*e) || *e == '.')) {
        if(e > p && *e != '.' && e[-1] == '.') {
            dotted = TRUE;
        }
        e++;
    }
    if(!dotted && !(end_is_cut && e >= end)) {
        return 0;   // base64url of a json, and no part after it
    }
    while(e[-1] == '.') {
        e--;        // never before p: p[0] is 'e'
    }
    return (size_t)(e - p);
}

/*
 *  The scheme word that begins at `p` -- "bearer" or "basic", any case, a
 *  word of its own and a blank after it -- as its bytes, 0 if none
 */
PRIVATE size_t scheme_word_len(const char *p, const char *start, const char *end)
{
    static const char *schemes[] = {"bearer", "basic", 0};
    if(p > start && is_ascii_alnum(p[-1])) {
        return 0;   // inside a word
    }
    for(int i=0; schemes[i]; i++) {
        size_t n = strlen(schemes[i]);
        if((size_t)(end - p) > n && ascii_case_equal(p, schemes[i], n) &&
                (p[n] == ' ' || p[n] == '\t')) {
            return n;
        }
    }
    return 0;
}

/*
 *  The characters of the `n` bytes at `w` (UTF-8: the bytes that begin
 *  one), as gobj-js counts them (its code points)
 */
PRIVATE size_t utf8_chars(const char *w, size_t n)
{
    size_t chars = 0;
    for(size_t i = 0; i < n; i++) {
        if(((unsigned char)w[i] & 0xC0) != 0x80) {
            chars++;
        }
    }
    return chars;
}

/*
 *  Is the word of `n` bytes at `w` a word of prose? ASCII letters only, in
 *  runs joined by one '-', '.' or '/' (self-service, e.g, and/or), each
 *  run in lower case, in upper case or capitalized (bond, Bond, BOND), and
 *  shorter than MASK_PROSE_MAX. A credential is not: it holds a digit, a
 *  symbol of its own (_ ~ + = % ...) or a byte that is no ASCII, mixes its
 *  case (aBc, abC), or is long.
 */
PRIVATE BOOL is_prose_word(const char *w, size_t n)
{
    if(n == 0 || n >= MASK_PROSE_MAX) {
        return FALSE;
    }
    size_t i = 0;
    while(1) {
        size_t r = i;
        BOOL upper_rest = TRUE;
        BOOL lower_rest = TRUE;
        while(r < n && is_ascii_letter(w[r])) {
            if(r > i) {
                if(is_ascii_upper(w[r])) {
                    lower_rest = FALSE;
                } else {
                    upper_rest = FALSE;
                }
            }
            r++;
        }
        if(r == i) {
            return FALSE;   // no letter where a run begins
        }
        if(!lower_rest && !(upper_rest && is_ascii_upper(w[i]))) {
            return FALSE;   // its case mixed
        }
        if(r == n) {
            return TRUE;
        }
        if((w[r] != '-' && w[r] != '.' && w[r] != '/') || r + 1 == n) {
            return FALSE;
        }
        i = r + 1;
    }
}

/*
 *  The value of a base64 digit, -1 for a byte that is none
 */
PRIVATE int base64_digit(char c)
{
    if(c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if(c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if(c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if(c == '+') {
        return 62;
    }
    if(c == '/') {
        return 63;
    }
    return -1;
}

/*
 *  Does the word of `n` bytes at `w` begin with the base64 of
 *  "user:password", the credentials of a Basic Authorization (RFC 7617)?
 *  Its run of base64 digits (the '=' of its padding after them) decodes to
 *  bytes with a ':' -- the ones a cap left too, and whatever follows the
 *  run in the word ("(Basic dXNlcjpwYXNz)" is one: the audit record's rule
 *  up to 7.26.9).
 */
PRIVATE BOOL is_basic_credential(const char *w, size_t n)
{
    uint32_t bits = 0;
    int n_bits = 0;
    for(size_t i = 0; i < n; i++) {
        int v = base64_digit(w[i]);
        if(v < 0) {
            return FALSE;
        }
        bits = ((bits << 6) | (uint32_t)v) & 0x3FFF;
        n_bits += 6;
        if(n_bits >= 8) {
            n_bits -= 8;
            if(((bits >> n_bits) & 0xFF) == ':') {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/*
 *  Where the word after a scheme ends: a blank, an end of line, a quote,
 *  one of ,;}] or a NUL -- where the token after "Bearer" ended in the
 *  audit record of 7.26.9 and in gobj-js 7.26.20
 */
PRIVATE BOOL is_scheme_word_stop(char c)
{
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '"' || c == '\'' ||
        c == ',' || c == ';' || c == '}' || c == ']' || c == 0)? TRUE : FALSE;
}

/*
 *  The credential after a scheme word that begins at `p`
 *  (scheme_word_len()): its offset from `p` in *offset, and its bytes as
 *  the return; 0 when what follows the scheme is no credential. It is the
 *  word after the scheme's blanks, to a blank, an end of line, a quote or
 *  one of ,;}] (is_scheme_word_stop()), masked whole:
 *    - in quotes ("Bearer \"tok\""), the quotes too: whatever its shape, a
 *      quote makes it data;
 *    - after the same scheme word again ("Bearer Bearer tok", a header
 *      prefixed twice): whatever its shape;
 *    - where the text may have been cut (`end_is_cut`), a word that
 *      reaches the end: whatever its shape, the cut may have taken the
 *      rest of it ("Bearer ab" of "Bearer abcdefgh");
 *    - else when it can be a credential, judged without the dots that end
 *      it (the end of a sentence): no word of prose (is_prose_word()), and
 *      for "Bearer" MASK_TOKEN_MIN characters at least, for "Basic" a run
 *      of base64 that decodes to a "user:password" (is_basic_credential()).
 *  So "the bearer of this letter", "the bearer bond", "basic plan" or
 *  "Basic Operations" are left as they are (gobj-js 7.26.20 masked the
 *  word after "bearer" whatever it was).
 */
PRIVATE size_t scheme_credential(
    const char *p,
    const char *start,
    const char *end,
    BOOL end_is_cut,
    size_t *offset
)
{
    size_t n = scheme_word_len(p, start, end);
    if(!n) {
        return 0;
    }
    const char *scheme = (n == 5)? "basic" : "bearer";
    BOOL context = FALSE;
    const char *t = p + n;
    while(t < end && (*t == ' ' || *t == '\t')) {
        t++;
    }
    // the scheme doubled, once: a loop over "Bearer Bearer ..." would be quadratic
    if(t < end && scheme_word_len(t, start, end) == n && ascii_case_equal(t, scheme, n)) {
        t += n;
        context = TRUE;
        while(t < end && (*t == ' ' || *t == '\t')) {
            t++;
        }
    }
    if(t >= end) {
        return 0;
    }

    if(*t == '"' || *t == '\'') {
        const char *q = t + 1;
        while(q < end && *q != *t && *q != ' ' && *q != '\t' && *q != '\r' && *q != '\n' && *q) {
            q++;
        }
        if(q < end && *q == *t && q > t + 1) {
            *offset = (size_t)(t - p);
            return (size_t)(q + 1 - t);
        }
        if(end_is_cut && q >= end && q > t + 1) {
            *offset = (size_t)(t - p);
            return (size_t)(q - t);
        }
        return 0;
    }

    const char *e = t;
    while(e < end && !is_scheme_word_stop(*e)) {
        e++;
    }
    if(e == t) {
        return 0;
    }
    if(!context && !(end_is_cut && e >= end)) {
        const char *w_end = e;
        while(w_end > t && w_end[-1] == '.') {
            w_end--;
        }
        size_t len = (size_t)(w_end - t);
        if(len == 0 || is_prose_word(t, len)) {
            return 0;
        }
        if(n == 5) {
            if(!is_basic_credential(t, len)) {
                return 0;
            }
        } else if(utf8_chars(t, len) < MASK_TOKEN_MIN) {
            return 0;
        }
    }
    *offset = (size_t)(t - p);
    return (size_t)(e - t);
}

/*
 *  A credential by its shape that begins at `p`: a JWT (jwt_len()), or the
 *  credential after a scheme word (scheme_credential()). Its bytes, 0 if
 *  none; *offset is where it begins, from `p`.
 */
PRIVATE size_t shape_len(const char *p, const char *start, const char *end, BOOL end_is_cut, size_t *offset)
{
    *offset = 0;
    if(p >= end) {
        return 0;
    }
    if(*p == 'e') {
        return jwt_len(p, start, end, end_is_cut);
    }
    if(*p == 'b' || *p == 'B') {
        return scheme_credential(p, start, end, end_is_cut, offset);
    }
    return 0;
}

/*
 *  Where a JWT, or a scheme word, may begin at `p` (a NUL-terminated
 *  text): what makes a text worth looking at
 */
PRIVATE BOOL may_begin_shape(const char *p)
{
    if(p[0] == 'e') {
        return (p[1] == 'y' && p[2] == 'J')? TRUE : FALSE;
    }
    if(p[0] == 'b' || p[0] == 'B') {
        size_t n = 0;
        while(n < 6 && p[n]) {
            n++;
        }
        return ((n >= 6 && ascii_case_equal(p, "bearer", 6)) ||
            (n >= 5 && ascii_case_equal(p, "basic", 5)))? TRUE : FALSE;
    }
    return FALSE;
}

/*
 *  Where the value of a "name: value" ends, the value beginning at `v` (its
 *  blanks skipped): a quoted text to its closing quote, a json object or
 *  list to its closing bracket (the texts in it skipped), the text of a
 *  json inside a json, its quote escaped (\"hunter2\"), to the quote
 *  escaped the same way, a scheme and the word after it ("Bearer abc": the
 *  WHOLE value), or one word, to a blank, an end of line or one of
 *  ,;&)]}<>"'
 */
PRIVATE BOOL is_colon_value_stop(char c)
{
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == ';' ||
        c == '&' || c == ')' || c == ']' || c == '}' || c == '<' || c == '>' ||
        c == '"' || c == '\'' || c == 0)? TRUE : FALSE;
}

PRIVATE const char *colon_value_end(const char *v, const char *start, const char *end)
{
    if(v >= end) {
        return v;
    }
    if(*v == '"' || *v == '\'') {
        const char *e = v + 1;
        while(e < end && *e != *v) {
            if(*e == '\\' && e + 1 < end) {
                e++;
            }
            e++;
        }
        return (e < end)? e + 1 : end;
    }
    if(*v == '{' || *v == '[') {
        int depth = 0;
        BOOL in_string = FALSE;
        for(const char *e = v; e < end; e++) {
            if(in_string) {
                if(*e == '\\' && e + 1 < end) {
                    e++;
                } else if(*e == '"') {
                    in_string = FALSE;
                }
            } else if(*e == '"') {
                in_string = TRUE;
            } else if(*e == '{' || *e == '[') {
                depth++;
            } else if(*e == '}' || *e == ']') {
                depth--;
                if(depth == 0) {
                    return e + 1;
                }
            }
        }
        return end;
    }
    if(*v == '\\') {
        const char *q = v;
        while(q < end && *q == '\\') {
            q++;
        }
        if(q < end && (*q == '"' || *q == '\'')) {
            size_t level = (size_t)(q - v);
            size_t run = 0;     // the backslashes before the byte looked at
            for(const char *e = q + 1; e < end; e++) {
                if(*e == '\\') {
                    run++;
                    continue;
                }
                if(*e == *q && run == level) {
                    return e + 1;
                }
                run = 0;
            }
            return end;
        }
    }
    const char *e = v;
    size_t n = scheme_word_len(v, start, end);
    if(n) {
        e = v + n;
        while(e < end && (*e == ' ' || *e == '\t')) {
            e++;
        }
        if(e < end && (*e == '"' || *e == '\'')) {
            return colon_value_end(e, start, end);
        }
    }
    while(e < end && !is_colon_value_stop(*e)) {
        e++;
    }
    return e;
}

/***************************************************************************
 *  The bytes to mask are MARKED, one bit for each byte of the text, and
 *  every rule judges the text as it came: what one rule masks hides from
 *  no other rule the bytes it judges by, and what is masked is what any
 *  of them masks. A rule that masked in place, or jumped over its value,
 *  hid from the next rule a name=value inside that value
 *  ("x-auth-token: password=a,b c" showed ",b c").
 ***************************************************************************/
PRIVATE void mark_range(uint8_t *marks, size_t from, size_t to)
{
    for(size_t i = from; i < to; i++) {
        marks[i >> 3] |= (uint8_t)(1u << (i & 7));
    }
}

PRIVATE BOOL is_marked(const uint8_t *marks, size_t i)
{
    return (marks[i >> 3] & (1u << (i & 7)))? TRUE : FALSE;
}

/*
 *  The shape at `p` as gobj-js 7.26.20 told it (the C of that round was
 *  never released), the one its reading of the parameters jumps over: a
 *  JWT of exactly three parts (its dots at the end not its own), or the
 *  word after a "bearer" word, whatever it is, to a blank, an end of line,
 *  a quote or one of ,;}]. Its bytes, 0 if none; *offset is where it
 *  begins, from `p`. Whether a shape is masked is mark_shapes()'s to say.
 */
PRIVATE size_t shape_len_js_7_26_20(const char *p, const char *start, const char *end, size_t *offset)
{
    *offset = 0;
    if(*p == 'e') {
        if(end - p < 3 || p[1] != 'y' || p[2] != 'J') {
            return 0;
        }
        if(p > start && (is_base64url_char(p[-1]) || p[-1] == '.')) {
            return 0;
        }
        const char *e = p;
        int dots = 0;
        while(e < end && (is_base64url_char(*e) || *e == '.')) {
            if(*e == '.') {
                dots++;
            }
            e++;
        }
        while(e[-1] == '.') {
            e--;    // never before p: p[0] is 'e'
            dots--;
        }
        return (dots == 2)? (size_t)(e - p) : 0;
    }
    if(*p == 'b' || *p == 'B') {
        if((size_t)(end - p) <= 6 || !ascii_case_equal(p, "bearer", 6)) {
            return 0;
        }
        if(p > start && is_ascii_alnum(p[-1])) {
            return 0;
        }
        const char *t = p + 6;
        if(*t != ' ' && *t != '\t') {
            return 0;
        }
        while(t < end && (*t == ' ' || *t == '\t')) {
            t++;
        }
        const char *e = t;
        while(e < end && !strchr(" \t\r\n'\",;}]", *e)) {
            e++;
        }
        *offset = (size_t)(t - p);
        return (size_t)(e - t);
    }
    return 0;
}

/*
 *  The readings of the name=value parameters. Each one is kept whole, so
 *  that nothing a released version masked shows:
 *    - READ_AS_7_26_9: as 7.26.9 read them, with its names (NAMES_7_26_9)
 *      and no shape;
 *    - READ_AS_GOBJ_JS_7_26_20: as gobj-js 7.26.20 read them, with the
 *      names of now, jumping over its shapes (shape_len_js_7_26_20(): a
 *      word begins after one);
 *    - READ_NOW: with the names of now, jumping over no shape, so a
 *      name=value inside the word after "bearer" is judged even when that
 *      word is no credential ("Bearer pwd=x").
 */
typedef enum {
    READ_NOW = 0,
    READ_AS_GOBJ_JS_7_26_20,
    READ_AS_7_26_9,
} param_reading_t;

/*
 *  The name=value secrets of the `len` bytes of `str`, marked as one
 *  reading (param_reading_t) reads them. A name=value is judged by the
 *  word before its '=' (since the last blank or '=', its last
 *  MASK_MAX_NAME bytes): 'password'=x and user[password]=x are names too,
 *  and so is the `value` of a write-attr of a secret attribute
 *  (`value_is_secret`). Its value, quoted or not, runs to the next "word="
 *  (inline_secret_value_end()). One pass: a value marked is jumped over.
 *  Return TRUE if a name it took for a secret's is none of 7.26.9's: up
 *  to the first one, READ_AS_7_26_9 reads as READ_NOW does.
 */
PRIVATE BOOL mark_params(
    const char *str,
    size_t len,
    BOOL value_is_secret,
    param_reading_t reading,
    uint8_t *marks
)
{
    secret_names_t names = (reading == READ_AS_7_26_9)? NAMES_7_26_9 : NAMES_NOW;
    BOOL not_7_26_9 = FALSE;
    const char *str_end = str + len;
    char outer = 0;             // the quote of an outer value open (command='...')
    const char *word = str;     // where the current word begins
    const char *p = str;
    while(p < str_end) {
        if(*p == ' ' || *p == '\t' || *p == 0) {
            p++;
            word = p;
            continue;
        }
        if(*p == '"' || *p == '\'') {
            if(!outer && (p == str || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '=')) {
                outer = *p;     // opens a value
            } else if(*p == outer && (p + 1 >= str_end || p[1] == 0 || p[1] == ' ' || p[1] == '\t')) {
                outer = 0;      // closes it
            }
        }
        if(reading == READ_AS_GOBJ_JS_7_26_20) {
            size_t offset = 0;
            size_t n = shape_len_js_7_26_20(p, str, str_end, &offset);
            if(n) {
                p += offset + n;
                word = p;
                continue;
            }
        }
        if(*p != '=') {
            p++;
            continue;
        }

        // a bounded look back: "a=a=a=..." would ask about the whole word at each '='
        const char *name = ((size_t)(p - word) > MASK_MAX_NAME)? p - MASK_MAX_NAME : word;
        size_t name_len = (size_t)(p - name);
        word = p + 1;
        BOOL secret = is_param_name_secret(name, name_len, names);
        if(secret && names == NAMES_NOW && !not_7_26_9 && !is_secret_name_by(name, name_len, NAMES_7_26_9)) {
            not_7_26_9 = TRUE;
        }
        if(!(secret || (value_is_secret && name_len == 5 && ascii_case_equal(name, "value", 5)))) {
            p++;
            continue;
        }

        p++;    // the '='
        const char *end;
        char quote = (p < str_end && (*p == '"' || *p == '\''))? *p : 0;
        if(quote) {
            end = p + 1;
            while(end < str_end && *end && *end != quote) {
                end++;
            }
            if(end < str_end && *end == quote) {
                end++;
            }
        } else {
            end = inline_secret_value_end(p, str_end, outer);
        }
        mark_range(marks, (size_t)(p - str), (size_t)(end - str));
        p = end;
        word = p;
    }
    return not_7_26_9;
}

/*
 *  Does a shape of gobj-js 7.26.20 begin in the `len` bytes of `str`?
 *  Where none does, READ_AS_GOBJ_JS_7_26_20 reads as READ_NOW does.
 */
PRIVATE BOOL has_shape_js_7_26_20(const char *str, size_t len)
{
    const char *end = str + len;
    for(const char *p = str; p < end; p++) {
        size_t offset = 0;
        if((*p == 'e' || *p == 'b' || *p == 'B') && shape_len_js_7_26_20(p, str, end, &offset)) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 *  The stops of a value of a dump, and of a query string
 */
#define PARAM_VALUE_STOPS   "&; \t,\"'<>"       // name=value, as 7.26.9's dump
#define JSON_VALUE_STOPS    ",}] \t"            // "name": value, as 7.26.9's dump
#define LABEL_VALUE_STOPS   ",;&)]}<>\"' \t"    // name: value

/*
 *  Mark from `p` up to a byte of `stops`, a CR/LF, a NUL or `end`: return
 *  where it stopped
 */
PRIVATE const char *mark_run(const char *bf, const char *p, const char *end, const char *stops, uint8_t *marks)
{
    const char *e = p;
    while(e < end && *e != '\r' && *e != '\n' && *e && !strchr(stops, *e)) {
        e++;
    }
    mark_range(marks, (size_t)(p - bf), (size_t)(e - bf));
    return e;
}

/*
 *  The value at `v` is a scheme and its credential ("Bearer abc", under a
 *  secret's name): its WHOLE marked, the scheme, its blanks and the word
 *  after them (to a byte of `stops`; a quoted one to its quote) -- the
 *  scheme alone masked would leave the token to a rule that sees it
 *  masked. Return where it ends, NULL when `v` begins no scheme.
 */
PRIVATE const char *mark_scheme_value(const char *bf, const char *v, const char *end, const char *stops, uint8_t *marks)
{
    size_t n = scheme_word_len(v, bf, end);
    if(!n) {
        return NULL;
    }
    const char *w = v + n;
    while(w < end && (*w == ' ' || *w == '\t')) {
        w++;
    }
    mark_range(marks, (size_t)(v - bf), (size_t)(w - bf));
    if(w < end && (*w == '"' || *w == '\'')) {
        const char *e = colon_value_end(w, bf, end);
        mark_range(marks, (size_t)(w - bf), (size_t)(e - bf));
        return e;
    }
    return mark_run(bf, w, end, stops, marks);
}

/*
 *  How the name before a '=' (or a ':') is read: the run of name bytes
 *  before it (name_before()), or the name as a form or a query writes it
 *  (written_name_is_secret())
 */
typedef enum {
    NAME_PLAIN = 0,
    NAME_WRITTEN,
} name_reading_t;

PRIVATE int hex_digit_value(char c)
{
    if(c >= '0' && c <= '9') {
        return c - '0';
    }
    if(c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if(c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

PRIVATE BOOL is_written_name_byte(char c)
{
    return (is_name_char(c) || c == '[' || c == ']' || c == '%' || c == '\'' || c == '"')? TRUE : FALSE;
}

PRIVATE BOOL is_written_name_bracket(char c)
{
    return (c == '[' || c == ']' || c == '\'' || c == '"')? TRUE : FALSE;
}

/*
 *  The name before the '=' or ':' at `eq` as a form or a query writes it:
 *  the run of name bytes, brackets, quotes and '%' before it (its last
 *  MASK_MAX_NAME), its %XX decoded -- user[password], user%5Bpassword%5D,
 *  t%6Fken, 'password'. Is one of its parts, between the brackets and the
 *  quotes, a secret's name? Not its last part when that is a run of name
 *  bytes after a bracket or a quote (the "pin" of x"pin): that one is
 *  name_before()'s, and so is a name of name bytes only.
 */
PRIVATE BOOL written_name_is_secret(const char *start, const char *eq)
{
    const char *n = eq;
    const char *plain = NULL;   // where the run of name bytes before `eq` begins
    while(n > start && (size_t)(eq - n) < MASK_MAX_NAME && is_written_name_byte(n[-1])) {
        n--;
        if(!plain && !is_name_char(*n)) {
            plain = n + 1;
        }
    }
    if(!plain) {
        return FALSE;
    }
    const char *to = (plain[-1] == '%')? eq : plain - 1;

    char name[MASK_MAX_NAME];
    size_t len = 0;
    for(const char *s = n; s < to; s++) {
        int hi = (*s == '%' && to - s > 2)? hex_digit_value(s[1]) : -1;
        int lo = (hi >= 0)? hex_digit_value(s[2]) : -1;
        if(lo >= 0) {
            name[len++] = (char)(hi*16 + lo);
            s += 2;
        } else {
            name[len++] = *s;
        }
    }

    size_t seg = 0;
    for(size_t i = 0; i <= len; i++) {
        if(i < len && !is_written_name_bracket(name[i])) {
            continue;
        }
        if(is_secret_name(name + seg, i - seg)) {
            return TRUE;
        }
        seg = i + 1;
    }
    return FALSE;
}

/*
 *  Does a byte of a written name that is no name byte ("[]%'\"") lie
 *  between `bf` and `end`? Without one, NAME_WRITTEN finds nothing.
 */
PRIVATE BOOL may_hold_written_name(const char *bf, const char *end)
{
    for(const char *p = bf; p < end; p++) {
        if(*p == '[' || *p == ']' || *p == '%' || *p == '\'' || *p == '"') {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 *  Is the name before the '=' or ':' at `p` a secret's, as `reading` reads it?
 */
PRIVATE BOOL name_at_is_secret(const char *bf, const char *p, name_reading_t reading)
{
    if(reading == NAME_WRITTEN) {
        return written_name_is_secret(bf, p);
    }
    const char *n = name_before(bf, p);
    return (n < p && is_secret_name(n, (size_t)(p - n)))? TRUE : FALSE;
}

/*
 *  The value of a name=value whose name -- the run of name bytes before
 *  the '=' (name_before()), or, with NAME_WRITTEN, the name as a form
 *  writes it (written_name_is_secret()) -- is a secret's, as in a query
 *  string, a form body or a command line, marked: inside its quotes, or
 *  to a byte of PARAM_VALUE_STOPS. The word before the '=' (mark_params())
 *  can hold more than the name: in "GET https://my-file-server.com/get?token=x"
 *  it holds a segment about a credential ("file"), and the token showed.
 *  Each reading is a pass of its own.
 */
PRIVATE void mark_query_params(const char *bf, const char *end, name_reading_t reading, uint8_t *marks)
{
    if(reading == NAME_WRITTEN && !may_hold_written_name(bf, end)) {
        return;
    }
    const char *frontier = bf;
    for(const char *p = bf + 1; p < end; p++) {
        if(*p != '=' || p < frontier) {
            continue;
        }
        if(!name_at_is_secret(bf, p, reading)) {
            continue;
        }
        const char *v = p + 1;
        if(v < end && (*v == '"' || *v == '\'')) {
            char q[2] = {*v, 0};
            frontier = mark_run(bf, v + 1, end, q, marks);
        } else {
            mark_scheme_value(bf, v, end, PARAM_VALUE_STOPS, marks);
            frontier = mark_run(bf, v, end, PARAM_VALUE_STOPS, marks);  // where 7.26.9's dump went on
        }
    }
}

/*
 *  The password of the userinfo of a URL (scheme://user:password@host),
 *  marked: from the first ':' of the authority to its last '@' (the
 *  userinfo ends at the last one, as a browser reads it). The authority
 *  runs to a byte of URL_AUTHORITY_STOPS, an end of line or a NUL, so the
 *  scans from two "://" never overlap. Where the text may have been cut
 *  (`end_is_cut`: a dump, a window), an authority that reaches its end is
 *  marked from its first ':' to the end, '@' or not: the userinfo ends at
 *  the LAST '@', and the one that ends the password may be past the cut
 *  (a password written with a raw '@', "p@ss", or a user that is an email
 *  address). A port there, and a host after a whole userinfo, are masked
 *  too: the bytes do not say they are not a password's. The colons of an
 *  IP literal followed by a port ("[2001:db8::1]:8080") are no password's
 *  there: it is marked from the ':' after the literal -- in a decoded view
 *  (`written` not NULL) only when its '[' was written raw and its bytes as
 *  written, to the cut, fit (written_literal_fits()); the rest of it may
 *  have been written encoded ("[2001%3Adb8%3A%3A1]%3A80").
 */
#define URL_AUTHORITY_STOPS "/?#\"'<> \t"

/*
 *  The end of the dotted IPv4 quad written at `s`, or NULL
 */
PRIVATE const char *ipv4_text_end(const char *s, const char *e)
{
    const char *q = s;
    for(int octet = 0; octet < 4; octet++) {
        if(octet > 0) {
            if(q == e || *q != '.') {
                return NULL;
            }
            q++;
        }
        const char *d = q;
        int value = 0;
        while(q < e && q - d < 3 && *q >= '0' && *q <= '9') {
            value = value*10 + (*q - '0');
            q++;
        }
        if(q == d || value > 255 || (*d == '0' && q - d > 1)) {
            return NULL;    // RFC 3986 3.2.2: no leading zero ("01")
        }
    }
    return q;
}

/*
 *  The end of the IPv6 address written at `s` (RFC 4291 2.2: eight hex
 *  groups of four digits at most, one "::" in place of one group or more,
 *  a dotted IPv4 quad in place of the last two), or NULL. The end is the
 *  first byte that cannot continue it; the caller says what may follow.
 */
PRIVATE const char *ipv6_text_end(const char *s, const char *e)
{
    int groups = 0;
    BOOL elided = FALSE;
    const char *q = s;
    if(e - q >= 2 && q[0] == ':' && q[1] == ':') {
        elided = TRUE;
        q += 2;
    }
    while(q < e && hex_digit_value(*q) >= 0) {
        const char *g = q;
        while(q < e && q - g < 4 && hex_digit_value(*q) >= 0) {
            q++;
        }
        if(q < e && *q == '.') {
            q = ipv4_text_end(g, e);
            if(!q) {
                return NULL;
            }
            groups += 2;
            break;
        }
        groups++;
        if(q == e || *q != ':') {
            break;
        }
        q++;
        if(q < e && *q == ':') {
            if(elided) {
                return NULL;
            }
            elided = TRUE;
            q++;
        } else if(q == e || hex_digit_value(*q) < 0) {
            return NULL;    // a single ':' that ends it
        }
    }
    if(elided? groups > 7 : groups != 8) {
        return NULL;
    }
    return q;
}

/*
 *  The longest zone of an IP literal: an interface name (15 bytes on Linux)
 *  %-encoded whole fits. Unbounded, a long "zone" put the '[' of a userinfo
 *  that only looks like a literal and its "]:port" farther apart than the
 *  bytes judged past a cap (MASK_CAP_MARGIN), and the password showed. The
 *  bound holds for the bytes as WRITTEN: a literal read in the decoded view
 *  of a capped line takes up to three times its bytes per decoding, so
 *  there a literal is granted at the cut only when its '[' was written
 *  raw and its bytes as written, to the cut, fit in IP_LITERAL_WRITTEN_MAX
 *  (written_literal_fits()).
 */
#define IP_LITERAL_ZONE_MAX 64

/*
 *  The longest literal followed by a port: '[', the longest IPv6 text
 *  ("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"), '%' and its zone,
 *  "]:" and five digits
 */
#define IP_LITERAL_WRITTEN_MAX  (1 + 45 + 1 + IP_LITERAL_ZONE_MAX + 2 + 5)

/*
 *  Where each byte of a decoded view was written: the text as it came
 *  (`text`, `len` bytes, its end the cut of a capped line) and, for each
 *  byte of the view, the offset in it of the first byte it came from
 *  (`at`, one entry more for the end of the view)
 */
typedef struct {
    const char *text;
    size_t len;
    const size_t *at;
} written_map_t;

/*
 *  TRUE when the IP literal whose '[' is the byte `i` of a decoded view
 *  was written raw, and its bytes as written, from that '[' to the cut,
 *  are no more than those of the longest one
 */
PRIVATE BOOL written_literal_fits(const written_map_t *written, size_t i)
{
    size_t w = written->at[i];
    if(w >= written->len || written->text[w] != '[') {
        return FALSE;
    }
    return (written->len - w <= IP_LITERAL_WRITTEN_MAX)? TRUE : FALSE;
}

/*
 *  The ':' after the IP literal that begins the authority at `host` when
 *  all the rest of the authority, up to `e`, is its port
 *  ("[2001:db8::1]:8080", "[fe80::1%25eth0]:80"); else the first ':' of
 *  the authority. The literal is an IPv6 address (ipv6_text_end()) with an
 *  optional zone ('%' and 1 to IP_LITERAL_ZONE_MAX unreserved bytes, no
 *  ':'), and the port is one to five digits: a userinfo can begin with '['
 *  ("[a%x:pass]:z@h"), and anything else after a "]:" may be its password.
 *  A literal that reaches `e` may still be the start of a userinfo.
 */
PRIVATE const char *colon_after_ip_literal(const char *host, const char *e)
{
    const char *first_colon = memchr(host, ':', (size_t)(e - host));
    if(host >= e || *host != '[') {
        return first_colon;
    }
    const char *q = ipv6_text_end(host + 1, e);
    if(!q) {
        return first_colon;
    }
    if(q < e && *q == '%') {
        const char *zone = ++q;
        while(q < e && q - zone < IP_LITERAL_ZONE_MAX && (is_ascii_alnum(*q) || *q == '%' ||
                *q == '-' || *q == '.' || *q == '_' || *q == '~')) {
            q++;
        }
        if(q == zone) {
            return first_colon;
        }
    }
    if(e - q <= 2 || *q != ']' || q[1] != ':' || e - (q + 2) > 5) {
        return first_colon;
    }
    for(const char *d = q + 2; d < e; d++) {
        if(*d < '0' || *d > '9') {
            return first_colon;
        }
    }
    return q + 1;
}

PRIVATE void mark_url_passwords(
    const char *bf,
    const char *end,
    BOOL end_is_cut,
    const written_map_t *written,   // NULL: the bytes are the ones written
    uint8_t *marks
)
{
    for(const char *p = bf + 1; p + 2 < end; p++) {
        if(p[0] != ':' || p[1] != '/' || p[2] != '/' || !is_ascii_alnum(p[-1])) {
            continue;
        }
        const char *host = p + 3;
        const char *colon = NULL;
        const char *at = NULL;
        const char *e = host;
        while(e < end && *e && *e != '\r' && *e != '\n' && !strchr(URL_AUTHORITY_STOPS, *e)) {
            if(*e == '@') {
                at = e;
            } else if(*e == ':' && !colon) {
                colon = e;
            }
            e++;
        }
        if(colon && end_is_cut && e >= end) {
            const char *from = colon_after_ip_literal(host, e);
            if(from != colon && written && !written_literal_fits(written, (size_t)(host - bf))) {
                from = colon;
            }
            mark_range(marks, (size_t)(from + 1 - bf), (size_t)(end - bf));
        } else if(colon && at && colon < at) {
            mark_range(marks, (size_t)(colon + 1 - bf), (size_t)(at - bf));
        }
        p = e - 2;  // the ':' of the next "://" may be the last byte scanned
    }
}

/*
 *  A JWT, the credential after "Bearer" or "Basic" (shape_len()), wherever
 *  they begin in the `len` bytes of `str`, marked
 */
PRIVATE void mark_shapes(const char *str, size_t len, BOOL end_is_cut, uint8_t *marks)
{
    const char *str_end = str + len;
    for(const char *p = str; p < str_end; p++) {
        if(*p != 'e' && *p != 'b' && *p != 'B') {
            continue;
        }
        size_t offset = 0;
        size_t n = shape_len(p, str, str_end, end_is_cut, &offset);
        if(n) {
            mark_range(marks, (size_t)(p + offset - str), (size_t)(p + offset + n - str));
        }
    }
}

/*
 *  The "name: value" secrets of the `len` bytes of `str` (a json member, a
 *  header, a label: "password": x, X-Auth-Token: x, bearer: tok), marked.
 *  The name is the word before the ':' since a blank, a '=' or a ':',
 *  judged by its last MASK_MAX_NAME bytes; the value is colon_value_end()'s.
 *  One pass, of its own: a value marked is jumped over.
 */
PRIVATE void mark_labels(const char *str, size_t len, uint8_t *marks)
{
    const char *str_end = str + len;
    const char *word = str;     // where the name of a "name: value" begins
    const char *p = str;
    while(p < str_end) {
        if(*p == ' ' || *p == '\t' || *p == 0 || *p == '=') {
            p++;
            word = p;
            continue;
        }
        if(*p != ':') {
            p++;
            continue;
        }
        const char *name = ((size_t)(p - word) > MASK_MAX_NAME)? p - MASK_MAX_NAME : word;
        if(!is_param_name_secret(name, (size_t)(p - name), NAMES_NOW)) {
            p++;
            word = p;
            continue;
        }
        const char *v = p + 1;
        while(v < str_end && (*v == ' ' || *v == '\t')) {
            v++;
        }
        const char *e = colon_value_end(v, str, str_end);
        mark_range(marks, (size_t)(v - str), (size_t)(e - str));
        size_t n_scheme = scheme_word_len(v, str, str_end);
        if(n_scheme) {
            p = v + n_scheme;   // a scheme taken whole hides no name after it
        } else {
            p = (e > v)? e : p + 1;
        }
        word = p;
    }
}

/*
 *  The rules of now of mask_secrets_inline() -- not the readings of
 *  released versions -- on the `len` bytes of `str`: what a decoded view
 *  of the text is judged by (mark_decoded_view())
 */
PRIVATE void mark_inline_rules_of_now(
    const char *str,
    size_t len,
    BOOL end_is_cut,
    const written_map_t *written,
    uint8_t *marks
)
{
    BOOL value_is_secret = inline_value_is_secret(str, len, NAMES_NOW);
    mark_params(str, len, value_is_secret, READ_NOW, marks);
    mark_query_params(str, str + len, NAME_PLAIN, marks);
    mark_query_params(str, str + len, NAME_WRITTEN, marks);
    mark_url_passwords(str, str + len, end_is_cut, written, marks);    // see IP_LITERAL_ZONE_MAX
    mark_shapes(str, len, end_is_cut, marks);
    mark_labels(str, len, marks);
}

typedef enum {
    RULES_OF_INLINE = 0,
    RULES_OF_DUMP,
} view_rules_t;

PRIVATE void mark_decoded_view(
    const char *str,
    size_t len,
    view_rules_t rules,
    BOOL end_is_cut,
    int decodings,
    const written_map_t *written,
    uint8_t *marks
);

/*
 *  The bytes of the text that mask_secrets_inline() masks, marked: the
 *  `len` bytes of `str`, of a text of `text_len` bytes (a window of it,
 *  when `end_is_cut`) -- its "attribute=" is looked for in all of it
 */
PRIVATE void mark_inline_secrets(
    const char *str,
    size_t len,
    size_t text_len,
    BOOL end_is_cut,
    uint8_t *marks
)
{
    BOOL value_is_secret = inline_value_is_secret(str, text_len, NAMES_NOW);
    BOOL value_is_secret_7_26_9 = inline_value_is_secret(str, text_len, NAMES_7_26_9);
    BOOL not_7_26_9 = mark_params(str, len, value_is_secret, READ_NOW, marks);
    if(not_7_26_9 || value_is_secret != value_is_secret_7_26_9) {
        mark_params(str, len, value_is_secret_7_26_9, READ_AS_7_26_9, marks);
    }
    if(has_shape_js_7_26_20(str, len)) {
        mark_params(str, len, value_is_secret, READ_AS_GOBJ_JS_7_26_20, marks);
    }
    mark_query_params(str, str + len, NAME_PLAIN, marks);
    mark_query_params(str, str + len, NAME_WRITTEN, marks);
    mark_url_passwords(str, str + len, end_is_cut, NULL, marks);
    mark_shapes(str, len, end_is_cut, marks);
    mark_labels(str, len, marks);
    mark_decoded_view(str, len, RULES_OF_INLINE, end_is_cut, 0, NULL, marks);
}

/*
 *  The `len` bytes of `str` with each run of bytes to mask written as
 *  "********". A gbmem string, NULL if there is nothing to mask.
 */
PRIVATE char *mask_inline_secrets(const char *str, size_t len, size_t text_len, BOOL end_is_cut)
{
    size_t marks_size = (len + 7)/8;
    uint8_t *marks = gbmem_malloc(marks_size);
    if(!marks) {
        // Error already logged
        return gbmem_strdup(MASK_TOO_LARGE);   // fails safe: never "nothing to mask"
    }
    memset(marks, 0, marks_size);
    mark_inline_secrets(str, len, text_len, end_is_cut, marks);

    size_t n_marked = 0;
    size_t n_runs = 0;
    for(size_t i = 0; i < len; i++) {
        if(is_marked(marks, i)) {
            n_marked++;
            if(i == 0 || !is_marked(marks, i - 1)) {
                n_runs++;
            }
        }
    }
    if(n_runs == 0) {
        GBMEM_FREE(marks)
        return NULL;
    }

    const char *mask = "********";
    size_t mask_len = strlen(mask);
    char *masked = gbmem_malloc(len - n_marked + n_runs*mask_len + 1);
    if(!masked) {
        // Error already logged
        GBMEM_FREE(marks)
        return gbmem_strdup(MASK_TOO_LARGE);   // fails safe
    }
    char *out = masked;
    for(size_t i = 0; i < len; i++) {
        if(!is_marked(marks, i)) {
            *out++ = str[i];
        } else if(i == 0 || !is_marked(marks, i - 1)) {
            memcpy(out, mask, mask_len);
            out += mask_len;
        }
    }
    *out = 0;
    GBMEM_FREE(marks)
    return masked;
}

/***************************************************************************
 *  A text (a command line) with the secrets that can be told written as
 *  "********" (each run of them as one), the rest kept as it is:
 *    - the value of every "name=value" whose name is a secret's
 *      (is_secret_name()), quoted or not; an unquoted one runs to the next
 *      "word=" (inline_secret_value_end()). The name is the word before
 *      the '=', and also the run of name bytes before it, as in a query
 *      string, and the name as a form writes it (mark_query_params(),
 *      name_reading_t: its value to a byte of &;,"'<> or a blank);
 *    - the password of the userinfo of a URL (mark_url_passwords());
 *    - in a URL or a query %-encoded inside a value, what these rules mask
 *      in it read decoded (mark_decoded_view());
 *    - the value of every "name: value" whose name is a secret's, its name
 *      in quotes or not (a json, a header, a label: "password": hunter2,
 *      Authorization: Basic x, bearer: tok);
 *    - a JWT, or the credential after "Bearer " or "Basic "
 *      (scheme_credential()), wherever they are, whatever names them.
 *  Each rule judges the text as it came (mark_inline_secrets()), and the
 *  readings of the parameters of 7.26.9 and of gobj-js 7.26.20 are rules
 *  of their own, whole (param_reading_t): what they masked is masked, but
 *  the word after a bare "Bearer" that cannot be a credential, which
 *  gobj-js 7.26.20 masked whatever it was. A gbmem string, or NULL when
 *  there was nothing to mask.
 ***************************************************************************/
PUBLIC char *mask_secrets_inline(const char *str)
{
    if(!str) {
        return NULL;
    }
    size_t len = 0;
    BOOL may_mask = FALSE;
    for(const char *p = str; *p; p++) {
        len++;
        if(!may_mask && (*p == '=' || *p == ':' || *p == '%' || may_begin_shape(p))) {
            may_mask = TRUE;
        }
    }
    if(!may_mask) {
        return NULL;
    }
    if(len > MASK_MAX_BYTES) {
        return gbmem_strdup(MASK_TOO_LARGE);
    }
    return mask_inline_secrets(str, len, len, FALSE);
}

/***************************************************************************
 *  See helpers.h
 ***************************************************************************/
PUBLIC size_t mark_secrets_inline(const char *text, size_t len, uint8_t *marks)
{
    if(!text || len == 0) {
        return 0;
    }
    if(!marks) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "marks NULL",
            NULL
        );
        return 0;
    }
    mark_inline_secrets(text, len, len, FALSE, marks);
    size_t n_marked = 0;
    for(size_t i = 0; i < len; i++) {
        if(is_marked(marks, i)) {
            n_marked++;
        }
    }
    return n_marked;
}

/***************************************************************************
 *  See helpers.h
 ***************************************************************************/
#define MASK_CAP_MARGIN     256     // bytes past the cap that are judged too

PUBLIC size_t mask_secrets_inline_capped(char *bf, size_t bfsize, const char *text, size_t len)
{
    if(!bf || bfsize == 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "bf NULL or of no size",
            NULL
        );
        return 0;
    }
    *bf = 0;
    if(!text || len == 0) {
        return 0;
    }
    size_t window = MIN(len, bfsize - 1 + MASK_CAP_MARGIN);
    while(window > 0 && window < len && (((unsigned char)text[window]) & 0xC0) == 0x80) {
        window--;   // a copy that fits in bf ends where the window ends
    }
    char *masked;
    if(window > MASK_MAX_BYTES) {
        masked = gbmem_strdup(MASK_TOO_LARGE);
    } else {
        masked = mask_inline_secrets(text, window, len, (window < len)? TRUE : FALSE);
    }
    const char *shown = masked? masked : text;
    size_t shown_len = masked? strlen(masked) : window;
    size_t n = MIN(shown_len, bfsize - 1);
    while(n > 0 && n < shown_len && (((unsigned char)shown[n]) & 0xC0) == 0x80) {
        n--;    // a whole UTF-8 character
    }
    memcpy(bf, shown, n);
    bf[n] = 0;
    GBMEM_FREE(masked)
    return n;
}

/***************************************************************************
 *  The rules of a dump (mask_secrets_in_text()), each one a pass of its
 *  own over the bytes as they came, marking what it masks
 ***************************************************************************/
PRIVATE BOOL is_name_char(char c)
{
    return (is_ascii_alnum(c) || c == '_' || c == '-' || c == '.')? TRUE : FALSE;
}

/*
 *  The value at `v` of a "name": value or a name: value (its blanks
 *  skipped), marked: a quoted text inside its quotes, an object or a list
 *  inside its brackets (to the one that closes it, the texts in it
 *  skipped, the ends of line kept), a scheme and its credential whole, or
 *  a run to a byte of `stops`. Return where it ends.
 */
PRIVATE const char *mark_colon_value(const char *bf, const char *v, const char *end, const char *stops, uint8_t *marks)
{
    if(v >= end) {
        return v;
    }
    if(*v == '"' || *v == '\'') {
        char quote = *v;
        const char *s = v + 1;
        while(s < end && *s != quote) {
            if(*s == '\\' && s + 1 < end) {
                s++;
            }
            s++;
        }
        mark_range(marks, (size_t)(v + 1 - bf), (size_t)(s - bf));
        return s;
    }
    if(*v == '{' || *v == '[') {
        int depth = 0;
        BOOL in_string = FALSE;
        const char *s = v;
        for(; s < end; s++) {
            if(in_string) {
                if(*s == '\\' && s + 1 < end) {
                    mark_range(marks, (size_t)(s - bf), (size_t)(s + 1 - bf));
                    s++;
                } else if(*s == '"') {
                    in_string = FALSE;
                }
            } else if(*s == '"') {
                in_string = TRUE;
            } else if(*s == '{' || *s == '[') {
                depth++;
                if(depth == 1) {
                    continue;
                }
            } else if(*s == '}' || *s == ']') {
                depth--;
                if(depth == 0) {
                    break;
                }
            }
            if(*s != '\r' && *s != '\n') {
                mark_range(marks, (size_t)(s - bf), (size_t)(s + 1 - bf));
            }
        }
        return s;
    }
    const char *e = mark_scheme_value(bf, v, end, stops, marks);
    return e? e : mark_run(bf, v, end, stops, marks);
}

/*
 *  The colon of an HTTP header whose value is a credential (Cookie:,
 *  Set-Cookie:, Authorization:, Proxy-Authorization:, any case) at the
 *  start of a line `ls`, a byte of value after it; NULL if none
 */
PRIVATE const char *credential_header_colon(const char *ls, const char *end)
{
    static const char *headers[] = {
        "cookie:", "set-cookie:", "authorization:", "proxy-authorization:", 0
    };
    for(int h=0; headers[h]; h++) {
        size_t hl = strlen(headers[h]);
        if((size_t)(end - ls) > hl && ascii_case_equal(ls, headers[h], hl)) {
            return ls + hl - 1;
        }
    }
    return NULL;
}

/*
 *  Mask IN PLACE from `p` up to a byte of `stops`, a CR/LF, a NUL or
 *  `end`, as 7.26.9 masked: return where it stopped
 */
PRIVATE char *mask_run_in_place(char *p, const char *end, const char *stops)
{
    while(p < end && *p != '\r' && *p != '\n' && !strchr(stops, *p)) {
        *p++ = '*';
    }
    return p;
}

/*
 *  The rules of a dump as one loop, each one masking IN PLACE -- on `copy`,
 *  the bytes as they came -- so each sees what the ones before it left, as
 *  7.26.9 masked a dump. It is read twice (param_reading_t), each reading
 *  whole: READ_AS_7_26_9, 7.26.9's dump, its names (NAMES_7_26_9) and no
 *  shape; and READ_AS_GOBJ_JS_7_26_20, the names of now and the shapes of
 *  gobj-js 7.26.20. The rules:
 *    - the value of a Cookie:, Set-Cookie:, Authorization: or
 *      Proxy-Authorization: header at the start of a line (after the
 *      scheme for the last two), to the end of the line;
 *    - with the shapes, a JWT of three parts, the word after a "bearer"
 *      word (shape_len_js_7_26_20());
 *    - the value of a name=value whose name is a secret's, to a byte of
 *      PARAM_VALUE_STOPS, or inside its quotes;
 *    - the value of a "name": value whose name is a secret's: inside its
 *      quotes, inside its brackets, or to a byte of JSON_VALUE_STOPS.
 *  What it masks is marked, but the word after "bearer": masked only when
 *  it can be a credential (mark_dump_shapes()), it is hidden here from the
 *  rules after it, and not marked (a byte 1 in the copy, no '*'). Return
 *  TRUE if it met a shape or a secret's name that is none of 7.26.9's: up
 *  to the first one, READ_AS_7_26_9 reads as the other reading does.
 */
#define HIDDEN_BYTE     '\x01'

PRIVATE BOOL mark_dump_in_place(
    const char *bf,
    char *copy,
    size_t len,
    param_reading_t reading,
    uint8_t *marks
)
{
    secret_names_t names = (reading == READ_AS_7_26_9)? NAMES_7_26_9 : NAMES_NOW;
    BOOL not_7_26_9 = FALSE;
    char *end = copy + len;
    memcpy(copy, bf, len);

    for(char *p = copy; p < end; p++) {
        if(p == copy || p[-1] == '\n') {
            const char *colon = credential_header_colon(p, end);
            if(colon) {
                char *v = (char *)colon + 1;
                while(v < end && (*v == ' ' || *v == '\t')) {
                    v++;
                }
                if(name_lower(*p) == 'a' || name_lower(*p) == 'p') {
                    char *sc = v;   // the scheme, if a value follows it
                    while(sc < end && *sc != ' ' && *sc != '\r' && *sc != '\n') {
                        sc++;
                    }
                    if(sc < end && *sc == ' ') {
                        v = sc + 1;
                    }
                }
                mask_run_in_place(v, end, "");
            }
        }

        if(reading == READ_AS_GOBJ_JS_7_26_20 && (*p == 'e' || *p == 'b' || *p == 'B')) {
            size_t offset = 0;
            size_t n = shape_len_js_7_26_20(p, copy, end, &offset);
            if(n) {
                not_7_26_9 = TRUE;
                char *t = p + offset;
                for(size_t i = 0; i < n; i++) {
                    if(t[i] != '*') {
                        t[i] = (*p == 'e')? '*' : HIDDEN_BYTE;  // a '*' of an earlier rule stays
                    }
                }
                continue;   // its bytes are masked now: nothing else begins there
            }
        }

        if(*p == '=' && p > copy) {
            const char *n = name_before(copy, p);
            if(n < p && is_secret_name_by(n, (size_t)(p - n), names)) {
                if(names == NAMES_NOW && !is_secret_name_by(n, (size_t)(p - n), NAMES_7_26_9)) {
                    not_7_26_9 = TRUE;
                }
                char *v = p + 1;
                if(v < end && (*v == '"' || *v == '\'')) {
                    char q[2] = {*v, 0};
                    mask_run_in_place(v + 1, end, q);
                } else {
                    mask_run_in_place(v, end, PARAM_VALUE_STOPS);
                }
            }
        }

        if(*p == '"') {
            char *n = p + 1;
            char *q = n;
            while(q < end && *q != '"' && *q != '\r' && *q != '\n') {
                q++;
            }
            if(q < end && *q == '"' && q > n && is_secret_name_tail(n, (size_t)(q - n), names)) {
                if(names == NAMES_NOW && !is_secret_name_tail(n, (size_t)(q - n), NAMES_7_26_9)) {
                    not_7_26_9 = TRUE;
                }
                char *v = q + 1;
                while(v < end && (*v == ' ' || *v == '\t')) {
                    v++;
                }
                if(v < end && *v == ':') {
                    v++;
                    while(v < end && (*v == ' ' || *v == '\t')) {
                        v++;
                    }
                    if(v < end && *v == '"') {
                        char *sv = v + 1;
                        while(sv < end && *sv != '"') {
                            if(*sv == '\\' && sv + 1 < end) {
                                *sv++ = '*';
                            }
                            *sv++ = '*';
                        }
                    } else if(v < end && (*v == '{' || *v == '[')) {
                        int depth = 0;
                        BOOL in_string = FALSE;
                        for(char *sv = v; sv < end; sv++) {
                            if(in_string) {
                                if(*sv == '\\' && sv + 1 < end) {
                                    *sv++ = '*';
                                } else if(*sv == '"') {
                                    in_string = FALSE;
                                }
                            } else if(*sv == '"') {
                                in_string = TRUE;
                            } else if(*sv == '{' || *sv == '[') {
                                depth++;
                                if(depth == 1) {
                                    continue;
                                }
                            } else if(*sv == '}' || *sv == ']') {
                                depth--;
                                if(depth == 0) {
                                    break;
                                }
                            }
                            if(*sv != '\r' && *sv != '\n') {
                                *sv = '*';
                            }
                        }
                    } else {
                        mask_run_in_place(v, end, JSON_VALUE_STOPS);
                    }
                    p = q;  // the closing quote of the name is not an opening one
                }
            }
        }
    }

    for(size_t i = 0; i < len; i++) {
        if(copy[i] == '*' && bf[i] != '*') {
            mark_range(marks, i, i + 1);
        }
    }
    return not_7_26_9;
}

/*
 *  A JWT, the credential after "Bearer" or "Basic", wherever they are --
 *  and at the end of the bytes whatever its shape: a dump is the first
 *  bytes of a frame, its end may cut a credential
 */
PRIVATE void mark_dump_shapes(const char *bf, const char *end, uint8_t *marks)
{
    mark_shapes(bf, (size_t)(end - bf), TRUE, marks);
}

/*
 *  The value of a "name": value -- or 'name': value, a Python dict, a
 *  log of one -- whose name is a secret's (its last MASK_MAX_NAME bytes)
 */
PRIVATE void mark_dump_json_members(const char *bf, const char *end, uint8_t *marks)
{
    const char *frontier = bf;
    for(const char *p = bf; p < end; p++) {
        if(p < frontier || (*p != '"' && *p != '\'')) {
            continue;
        }
        char quote = *p;
        const char *n = p + 1;
        const char *q = n;
        while(q < end && *q != quote && *q != '\r' && *q != '\n') {
            q++;
        }
        if(!(q < end && *q == quote && q > n && is_secret_name_tail(n, (size_t)(q - n), NAMES_NOW))) {
            continue;
        }
        const char *v = q + 1;
        while(v < end && (*v == ' ' || *v == '\t')) {
            v++;
        }
        if(v >= end || *v != ':') {
            continue;
        }
        v++;
        while(v < end && (*v == ' ' || *v == '\t')) {
            v++;
        }
        const char *e = mark_colon_value(bf, v, end, JSON_VALUE_STOPS, marks);
        if(v < end && (*v == '"' || *v == '{' || *v == '[')) {
            frontier = e;
        } else {
            // and as 7.26.9's dump read it ('it''s', "Bearer"): a scheme taken whole hides no name after it
            frontier = mark_run(bf, v, end, JSON_VALUE_STOPS, marks);
        }
        p = q;  // the closing quote of the name is not an opening one
    }
}

/*
 *  The value of a name: value whose name (a run of name bytes before the
 *  ':', or the name as a form writes it: name_reading_t) is a secret's:
 *  another header, a label (X-Auth-Token: x, bearer: tok, user[password]:
 *  x). Not the colon of a header that mark_dump_in_place() masks: its
 *  scheme is kept.
 */
PRIVATE void mark_dump_labels(const char *bf, const char *end, name_reading_t reading, uint8_t *marks)
{
    if(reading == NAME_WRITTEN && !may_hold_written_name(bf, end)) {
        return;
    }
    const char *frontier = bf;
    const char *header_colon = credential_header_colon(bf, end);
    for(const char *p = bf; p < end; p++) {
        if(p > bf && p[-1] == '\n') {
            header_colon = credential_header_colon(p, end);
        }
        if(*p != ':' || p == bf || p < frontier || p == header_colon) {
            continue;
        }
        if(!name_at_is_secret(bf, p, reading)) {
            continue;
        }
        const char *v = p + 1;
        while(v < end && (*v == ' ' || *v == '\t')) {
            v++;
        }
        const char *e = mark_colon_value(bf, v, end, LABEL_VALUE_STOPS, marks);
        size_t n_scheme = scheme_word_len(v, bf, end);
        frontier = n_scheme? v + n_scheme : e;  // a scheme taken whole hides no name after it
    }
}

/*
 *  The rules of now of mask_secrets_in_text() -- not the dump of released
 *  versions -- on the bytes from `bf` to `end`
 */
PRIVATE void mark_dump_rules_of_now(const char *bf, const char *end, uint8_t *marks)
{
    mark_dump_shapes(bf, end, marks);
    mark_query_params(bf, end, NAME_PLAIN, marks);
    mark_query_params(bf, end, NAME_WRITTEN, marks);
    mark_url_passwords(bf, end, TRUE, NULL, marks);     // the end of a dump may cut a frame
    mark_dump_json_members(bf, end, marks);
    mark_dump_labels(bf, end, NAME_PLAIN, marks);
    mark_dump_labels(bf, end, NAME_WRITTEN, marks);
}

/*
 *  A URL or a query %-encoded inside the value of a parameter
 *  (redirect_uri=, next=, url=: "next=%2Flogin%3Fpassword%3Dx") is read
 *  decoded too: a view of the `len` bytes of `str` with each %XX of an
 *  ASCII byte (no NUL) decoded is judged by the rules of now (`rules`),
 *  and what they mark in it is marked on the bytes it came from, a %XX
 *  whole. Decoded MASK_MAX_DECODINGS times at most -- once, and once more
 *  for a value encoded inside an encoded value ("%253D") --: each view is
 *  a pass of the rules, so a bound keeps it linear ("%252525...41" would
 *  ask for a view per "25").
 */
#define MASK_MAX_DECODINGS  2

PRIVATE int encoded_ascii_at(const char *p, const char *end)
{
    if(end - p < 3 || p[0] != '%') {
        return -1;
    }
    int hi = hex_digit_value(p[1]);
    int lo = (hi >= 0)? hex_digit_value(p[2]) : -1;
    if(lo < 0) {
        return -1;
    }
    int c = hi*16 + lo;
    return (c > 0 && c < 0x80)? c : -1;
}

PRIVATE void mark_decoded_view(
    const char *str,
    size_t len,
    view_rules_t rules,
    BOOL end_is_cut,
    int decodings,
    const written_map_t *written,   // NULL: `str` is the text as written
    uint8_t *marks
)
{
    if(decodings >= MASK_MAX_DECODINGS) {
        return;
    }
    const char *end = str + len;
    size_t view_len = len;
    for(const char *p = str; p < end; p++) {
        if(*p == '%' && encoded_ascii_at(p, end) >= 0) {
            view_len -= 2;
            p += 2;
        }
    }
    if(view_len == len) {
        return;     // nothing encoded
    }

    char *view = gbmem_malloc(view_len + 1);
    size_t view_marks_size = (view_len + 7)/8;
    uint8_t *view_marks = gbmem_malloc(view_marks_size + 1);
    /*  Only the cut of a capped line reads the map (8 bytes a byte): a dump
     *  or a whole text of a few MiB would fail the allocation, and all of
     *  it would be masked  */
    BOOL map = (rules == RULES_OF_INLINE && end_is_cut)? TRUE : FALSE;
    size_t *view_at = map? gbmem_malloc((view_len + 1) * sizeof(size_t)) : NULL;
    if(!view || !view_marks || (map && !view_at)) {
        // Error already logged. Fails safe: all of it
        GBMEM_FREE(view)
        GBMEM_FREE(view_marks)
        GBMEM_FREE(view_at)
        mark_range(marks, 0, len);
        return;
    }
    memset(view_marks, 0, view_marks_size + 1);
    char *v = view;
    for(const char *p = str; p < end; p++) {
        if(map) {
            size_t s = (size_t)(p - str);
            view_at[v - view] = written? written->at[s] : s;
        }
        int c = (*p == '%')? encoded_ascii_at(p, end) : -1;
        if(c >= 0) {
            *v++ = (char)c;
            p += 2;
        } else {
            *v++ = *p;
        }
    }
    *v = 0;
    if(map) {
        view_at[view_len] = written? written->at[len] : len;
    }
    written_map_t view_written = {
        .text = written? written->text : str,
        .len = written? written->len : len,
        .at = view_at
    };

    if(rules == RULES_OF_INLINE) {
        mark_inline_rules_of_now(view, view_len, end_is_cut, map? &view_written : NULL, view_marks);
    } else {
        mark_dump_rules_of_now(view, view + view_len, view_marks);
    }
    mark_decoded_view(
        view, view_len, rules, end_is_cut, decodings + 1, map? &view_written : NULL, view_marks
    );

    const char *p = str;
    for(size_t i = 0; i < view_len; i++) {
        size_t n = (*p == '%' && encoded_ascii_at(p, end) >= 0)? 3 : 1;
        if(is_marked(view_marks, i)) {
            mark_range(marks, (size_t)(p - str), (size_t)(p - str) + n);
        }
        p += n;
    }
    GBMEM_FREE(view_at)
    GBMEM_FREE(view_marks)
    GBMEM_FREE(view)
}

/***************************************************************************
 *  The bytes of a traffic dump with the credentials that can be told
 *  soundly written as '*', in place, the length kept:
 *    - the value of an HTTP "Cookie:", "Set-Cookie:", "Authorization:" or
 *      "Proxy-Authorization:" header at the start of a line (for the last
 *      two, after the scheme: "Authorization: Bearer ******");
 *    - the value of "name=value" whose name is a secret's (is_secret_name()),
 *      as in a query string, a form body or a command line, its name read
 *      as a run of name bytes and as a form writes it (name_reading_t);
 *    - the password of the userinfo of a URL (mark_url_passwords()), and
 *      from its first ':' an authority that reaches the end of the bytes;
 *    - in a URL or a query %-encoded inside a value, what these rules mask
 *      in it read decoded (mark_decoded_view());
 *    - the value of a json "name": value (or 'name': value), and of a
 *      name: value (another header, a label: X-Auth-Token: x, bearer:
 *      tok), whose name is a secret's;
 *      a value that is a scheme and its credential is masked WHOLE
 *      (token=Bearer abc, "password": Basic xyz);
 *    - a JWT, and the credential after "Bearer" or "Basic", wherever they
 *      are (jwt_len(), scheme_credential()), and at the end of the bytes
 *      whatever its shape: a dump is the first bytes of a frame.
 *  Each rule is a pass of its own over the bytes as they came, marking
 *  what it masks (mark_range()), and the dump of 7.26.9 is one pass,
 *  whole (mark_dump_in_place()): what 7.26.9 masked is masked. Return the
 *  bytes masked.
 ***************************************************************************/
PUBLIC size_t mask_secrets_in_text(char *bf, size_t len)
{
    if(!bf || len == 0) {
        return 0;
    }
    const char *end = bf + len;
    size_t marks_size = (len + 7)/8;
    uint8_t *marks = gbmem_malloc(marks_size);
    char *copy = gbmem_malloc(len);
    size_t masked = 0;
    if(!marks || !copy) {
        /*
         *  Error already logged. Fails safe: all of it, its lines kept
         */
        GBMEM_FREE(marks)
        GBMEM_FREE(copy)
        for(size_t i = 0; i < len; i++) {
            if(bf[i] != '*' && bf[i] != '\r' && bf[i] != '\n') {
                bf[i] = '*';
                masked++;
            }
        }
        return masked;
    }
    memset(marks, 0, marks_size);

    if(mark_dump_in_place(bf, copy, len, READ_AS_GOBJ_JS_7_26_20, marks)) {
        mark_dump_in_place(bf, copy, len, READ_AS_7_26_9, marks);
    }
    mark_dump_rules_of_now(bf, end, marks);
    mark_decoded_view(bf, len, RULES_OF_DUMP, TRUE, 0, NULL, marks);

    for(size_t i = 0; i < len; i++) {
        if(is_marked(marks, i) && bf[i] != '*') {
            bf[i] = '*';
            masked++;
        }
    }
    GBMEM_FREE(copy)
    GBMEM_FREE(marks)
    return masked;
}

/***************************************************************************
 *  A secret is shown masked unless it is absent, null or an empty string
 ***************************************************************************/
PRIVATE BOOL secret_value_is_set(json_t *value)
{
    if(!value || json_is_null(value)) {
        return FALSE;
    }
    if(json_is_string(value) && json_string_length(value) == 0) {
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************
 *  A json as a log or a trace may show it: at any depth, the value of a
 *  key with a secret's name (is_secret_name()) is "********", whatever its
 *  json type; so is the "value" of a dict whose "attribute" names a
 *  secret (write-attr); and in a string, under any key, a secret
 *  "name=value", a JWT and the token after "Bearer " are masked
 *  (mask_secrets_inline()). A dict or a list shared is masked once, the
 *  same everywhere; a cycle (json_object_set() can build one) is "<cycle>",
 *  and below MASK_MAX_DEPTH levels "<deeper not shown>": a kw of a peer
 *  nests as it likes. The walk is linear and bounded: each node costs one
 *  unit, a key or a string its bytes too, and once MASK_MAX_BYTES units are
 *  spent (json_mask_secrets_capped(): max_bytes) the walk stops, one
 *  "<not shown: too large to mask>" for the rest. Return a new reference:
 *  a masked copy, or jn itself when there was nothing to mask.
 ***************************************************************************/
#define MASK_MAX_DEPTH  64     // the same in gobj-js

PRIVATE json_t *json_mask_secrets_depth(json_t *jn, json_t *memo, int depth, size_t *budget);

/*
 *  Every node walked costs at least one unit of the budget, a key or a
 *  string its bytes too: what is walked is bounded by the cap, nodes
 *  included (a frame of "[{},{},...]" costs nothing in bytes)
 */
PRIVATE void charge(size_t *budget, size_t n)
{
    *budget = (n < *budget)? *budget - n : 0;
}

/*
 *  A dict or a list masked once: a shared one is masked the same
 *  everywhere, and a cycle is "<cycle>" (with a fan-out of two, a cycle
 *  walked again would be exponential before any depth limit)
 */

PRIVATE json_t *json_mask_container(json_t *jn, json_t *memo, int depth, size_t *budget)
{
    if(*budget == 0) {
        return json_string(MASK_TOO_LARGE);
    }
    char key_[32];
    snprintf(key_, sizeof(key_), "%p", (void *)jn);
    json_t *seen = json_object_get(memo, key_);
    if(seen) {
        return json_is_true(seen)? json_string("<cycle>") : json_incref(seen);
    }
    if(depth >= MASK_MAX_DEPTH) {
        return json_string("<deeper not shown>");  // a kw of a peer can nest at will
    }
    charge(budget, 1);
    json_object_set_new(memo, key_, json_true());  // in progress

    // built as the children are walked: once the budget is spent the walk stops
    BOOL changed = FALSE;
    BOOL truncated = FALSE;
    json_t *result;
    if(json_is_object(jn)) {
        result = json_object();
        const char *attribute = json_string_value(json_object_get(jn, "attribute"));
        BOOL value_is_secret = attribute && is_secret_name_tail(attribute, strlen(attribute), NAMES_NOW);
        const char *key;    // judged and copied by all its bytes, a NUL in it too
        size_t key_len;
        json_t *value;
        json_object_keylen_foreach(jn, key, key_len, value) {
            if(*budget == 0) {
                truncated = TRUE;
                break;
            }
            charge(budget, 1 + key_len);
            json_t *shown;
            BOOL secret = is_secret_name_tail(key, key_len, NAMES_NOW) ||
                (value_is_secret && key_len == 5 && memcmp(key, "value", 5)==0);
            if(secret && secret_value_is_set(value)) {
                shown = json_string("********");
            } else {
                shown = json_mask_secrets_depth(value, memo, depth+1, budget);
            }
            if(shown != value) {
                changed = TRUE;
            }
            json_object_setn_new(result, key, key_len, shown);
        }
        if(truncated) {
            json_object_set_new(result, "<more>", json_string(MASK_TOO_LARGE));
        }
    } else {
        result = json_array();
        size_t idx;
        json_t *value;
        json_array_foreach(jn, idx, value) {
            if(*budget == 0) {
                truncated = TRUE;
                break;
            }
            charge(budget, 1);
            json_t *shown = json_mask_secrets_depth(value, memo, depth+1, budget);
            if(shown != value) {
                changed = TRUE;
            }
            json_array_append_new(result, shown);
        }
        if(truncated) {
            json_array_append_new(result, json_string(MASK_TOO_LARGE));
        }
    }

    if(!changed && !truncated) {
        JSON_DECREF(result)
        result = json_incref(jn);
    }
    json_object_set(memo, key_, result);
    return result;
}

PRIVATE json_t *json_mask_secrets_depth(json_t *jn, json_t *memo, int depth, size_t *budget)
{
    if(!jn) {
        return NULL;
    }
    if(json_is_object(jn) || json_is_array(jn)) {
        return json_mask_container(jn, memo, depth, budget);
    }
    if(json_is_string(jn)) {
        size_t len = json_string_length(jn);
        if(len > *budget) {
            *budget = 0;
            return json_string(MASK_TOO_LARGE);    // fails safe: not shown, never in clear
        }
        *budget -= len;
        const char *s = json_string_value(jn);
        char *masked;
        if(strlen(s) != len) {
            // a NUL inside (a peer's frame, JSON_ALLOW_NUL) is read, and shown, as a blank
            char *copy = gbmem_malloc(len + 1);
            if(!copy) {
                // Error already logged
                return json_string(MASK_TOO_LARGE);    // fails safe
            }
            for(size_t i = 0; i < len; i++) {
                copy[i] = s[i]? s[i] : ' ';
            }
            copy[len] = 0;
            masked = mask_secrets_inline(copy);
            GBMEM_FREE(copy)
        } else {
            masked = mask_secrets_inline(s);
        }
        if(masked) {
            json_t *jn_shown = json_string(masked);
            GBMEM_FREE(masked)
            return jn_shown;
        }
    }
    return json_incref(jn);
}

PUBLIC json_t *json_mask_secrets_capped(json_t *jn, size_t max_bytes)
{
    json_t *memo = json_object();
    size_t budget = max_bytes;
    json_t *jn_shown = json_mask_secrets_depth(jn, memo, 0, &budget);
    JSON_DECREF(memo)
    return jn_shown;
}

PUBLIC json_t *json_mask_secrets(json_t *jn)
{
    return json_mask_secrets_capped(jn, MASK_MAX_BYTES);
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
 *  Turn a dotted version into a comparable number
 ***************************************************************************/
PUBLIC int version_cmp(const char *version1, const char *version2)
{
    int size1 = 0;
    int size2 = 0;
    const char **segments1 = 0;
    const char **segments2 = 0;

    if(!empty_string(version1)) {
        segments1 = split2(version1, ".-", &size1);
        if(!segments1) {
            // Error already logged
            size1 = 0;
        }
    }
    if(!empty_string(version2)) {
        segments2 = split2(version2, ".-", &size2);
        if(!segments2) {
            // Error already logged
            size2 = 0;
        }
    }

    /*
     *  Segment by segment, widest first: a missing segment is a zero, so
     *  "7.11" and "7.11.0" compare equal. Nothing is accumulated into a
     *  single number on the way -- that is the whole point, see helpers.h.
     */
    int ret = 0;
    int segments = (size1 > size2)? size1:size2;
    for(int i = 0; i < segments; i++) {
        long v1 = (i < size1)? atol(segments1[i]):0;
        long v2 = (i < size2)? atol(segments2[i]):0;
        if(v1 != v2) {
            ret = (v1 < v2)? -1:1;
            break;
        }
    }

    if(segments1) {
        split_free2(segments1);
    }
    if(segments2) {
        split_free2(segments2);
    }

    return ret;
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
    char *saveptr = NULL;

    if(plist_size) {
        *plist_size = 0; // error case
    }
    char *buffer = gbmem_strdup(str);
    if(!buffer) {
        return 0;
    }

    // Get list size
    int list_size = 0;
    for (ptr = strtok_r(buffer, delim, &saveptr); ptr != NULL; ptr = strtok_r(NULL, delim, &saveptr)) {
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
    for (ptr = strtok_r(buffer, delim, &saveptr); ptr != NULL; ptr = strtok_r(NULL, delim, &saveptr)) {
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

/***************************************************************************
 *  Does str match the regular expression pattern?
 *  Compiles, matches and frees on each call: nothing is kept.
 ***************************************************************************/
PUBLIC BOOL str_match_regex(const char *str, const char *pattern, int cflags)
{
    if(!str || !pattern) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "str_match_regex() without string or pattern",
            NULL
        );
        return FALSE;
    }

    regex_t r;
    int ret = regcomp(&r, pattern, cflags | REG_NOSUB);
    if(ret != 0) {
        char err[256];
        regerror(ret, &r, err, sizeof(err));
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "regcomp() FAILED, the pattern matches nothing",
            "pattern",      "%s", pattern,
            "error",        "%s", err,
            NULL
        );
        return FALSE;
    }

    BOOL matched = (regexec(&r, str, 0, NULL, 0) == 0)? TRUE: FALSE;
    regfree(&r);
    return matched;
}




                    /*------------------------------------*
                     *  ### Json
                     *------------------------------------*/




/***************************************************************************
 *  The json of an open file, read WHOLE and then parsed. json_loadfd()
 *  reads ONE byte per read() (it must not read past the value), and a
 *  file here is all one value: a schema of 60 KB was 60000 system calls,
 *  15 ms, every time a treedb opened. A read or parse failure is logged
 *  (critical, with `on_critical_error`) and answers NULL.
 ***************************************************************************/
PRIVATE json_t *load_json_from_fd(
    hgobj gobj,
    int fd,
    const char *path,
    log_opt_t on_critical_error
)
{
    struct stat st;
    size_t size = (fstat(fd, &st) == 0 && st.st_size > 0)? (size_t)st.st_size : 0;
    size_t bfsize = size + 1;   /*  one more: a file that grew is read to its end  */
    size_t len = 0;
    char *bf = gbmem_malloc(bfsize);
    while(bf) {
        if(len == bfsize) {
            char *bf2 = gbmem_realloc(bf, bfsize * 2);
            if(!bf2) {
                gbmem_free(bf);
                bf = NULL;
                break;  /*  logged below  */
            }
            bf = bf2;
            bfsize *= 2;
        }
        ssize_t n = read(fd, bf + len, bfsize - len);
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            gobj_log_critical(gobj, on_critical_error,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot read json file",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            gbmem_free(bf);
            return 0;
        }
        if(n == 0) {
            break;
        }
        len += (size_t)n;
    }
    if(!bf) {
        gobj_log_critical(gobj, on_critical_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "No memory to read json file",
            "path",         "%s", path,
            "size",         "%lu", (unsigned long)bfsize,
            NULL
        );
        return 0;
    }

    json_error_t error;
    json_t *jn = json_loadb(bf, len, 0, &error);
    gbmem_free(bf);
    if(!jn) {
        char error_shown[JSON_ERROR_TEXT_LENGTH];
        gobj_log_critical(gobj, on_critical_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON,
            "msg",          "%s", "Cannot load json file, bad json",
            "path",         "%s", path,
            "error",        "%s", json_error_for_log(&error, error_shown, sizeof(error_shown)),
            "line",         "%d", error.line,
            "position",     "%d", error.position,
            NULL
        );
    }
    return jn;
}

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
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Cannot load json, file not exist",
                "path",         "%s", full_path,
                NULL
            );
        }
        return 0;
    }

    int fd;
    if(exclusive) {
        fd = open_exclusive(full_path, O_RDONLY|O_NOFOLLOW|O_CLOEXEC, 0);
    } else {
        fd = open(full_path, O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    }
    if(fd<0) {
        if(!(silence && on_critical_error == LOG_NONE)) {
            gobj_log_critical(gobj, on_critical_error|LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
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

    json_t *jn = load_json_from_fd(gobj, fd, full_path, on_critical_error);
    if(!jn) {
        close(fd);
        return 0;   // Error already logged
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

    int fd = open(full_path, O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0) {
        gobj_log_critical(gobj, on_critical_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open json file",
            "path",         "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return 0;
    }

    json_t *jn = load_json_from_fd(gobj, fd, full_path, on_critical_error);  // Error already logged
    close(fd);
    return jn;
}

/***************************************************************************
 *  Write `jn_data` to directory/filename, IN PLACE: the file is truncated
 *  and rewritten, so a crash or a full disk halfway leaves it short or
 *  empty. It is NOT atomic, on purpose -- every caller would pay a
 *  temporary file and a rename it does not need. A caller whose file must
 *  survive a crash writes a temporary, flushes it and renames it over the
 *  old one (C_TREEDB's write_schema_tmp / commit_schema_file do).
 *
 *  Every failure is logged: a missing directory with `create` FALSE as an
 *  error, the rest (directory, open, write, close) as critical with
 *  `on_critical_error`. The close is checked because a write the kernel
 *  delayed fails THERE (EIO, ENOSPC on NFS or a thin volume): ignored, the
 *  file was reported saved while it was not.
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
            "msgset",       "%s", MSGSET_SYSTEM,
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
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot save json file, directory not found and not to be created",
                "directory",    "%s", directory,
                "filename",     "%s", filename,
                NULL
            );
            JSON_DECREF(jn_data)
            return -1;
        }
        if(mkrdir(directory, xpermission)<0) {
            gobj_log_critical(gobj, on_critical_error,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
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
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot create json file",
            "filename",     "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        JSON_DECREF(jn_data)
        return -1;
    }

    if(json_dumpfd(jn_data, fp, JSON_INDENT(4))<0) {
        gobj_log_critical(gobj, on_critical_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON,
            "msg",          "%s", "Cannot write in json file",
            "filename",     "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        close(fp);
        JSON_DECREF(jn_data)
        return -1;
    }
    if(close(fp) < 0) {
        gobj_log_critical(gobj, on_critical_error,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot close json file, what was written may be lost",
            "filename",     "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        JSON_DECREF(jn_data)
        return -1;
    }
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
            "msgset",       "%s", MSGSET_PARAMETER,
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
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "DESC without key field",
                NULL
            );
            break;
        }
        if(empty_string(json_desc->type)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
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
                json_int_t v=0;
                if(*defaults == '0') {
                    v = strtol(defaults, 0, 8);
                } else if(*defaults == 'x') {
                    v = strtol(defaults, 0, 16);
                } else if(strcasecmp(defaults, "now")==0) {
                    v = time(NULL);
                } else {
                    v = strtol(defaults, 0, 10);
                }
                json_object_set_new(jn, name, json_integer(v));
                break;
            CASES("int")
            CASES("integer")
                json_int_t v=0;
                if(*defaults == '0') {
                    v = strtol(defaults, 0, 8);
                } else if(*defaults == 'x') {
                    v = strtol(defaults, 0, 16);
                } else {
                    v = strtol(defaults, 0, 10);
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
                    "msgset",       "%s", MSGSET_PARAMETER,
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
            "msgset",       "%s", MSGSET_PARAMETER,
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
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "list MUST BE a json array",
            NULL
        );
        return 0;
    }

    jn_int = json_array_get(jn_list, idx);
    if(jn_int && !json_is_integer(jn_int)) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
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
            "msgset",       "%s", MSGSET_PARAMETER,
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
PUBLIC int json_list_find(json_t *list, json_t *value)
{
    int idx_found = -1;
    int index;
    json_t *_value;

    json_array_foreach(list, index, _value) {
        if(json_equal(_value, value)) {  // FAST: direct comparison
            idx_found = index;
            break;
        }
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
    if(pfirst) {
        *pfirst = 0;
    }
    if(psecond) {
        *psecond = 0;
    }
    if(json_array_size(list) != 2) {
        return FALSE;
    }

    json_int_t first = json_integer_value(json_array_get(list, 0));
    json_int_t second = json_integer_value(json_array_get(list, 1));
    if(first <= second) {
        if(pfirst) {
            *pfirst = first;
        }
        if(psecond) {
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
            "msgset",       "%s", MSGSET_PARAMETER,
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
PUBLIC json_t *json_listsrange2set( // WARNING function TOO SLOW, use for short ranges
    json_t *listsrange // not owned
) {
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
            "msgset",       "%s", MSGSET_PARAMETER,
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
 *  verbose: the failure of a json of OUR OWN, an ERROR with a stack; its
 *  bytes are never logged as they came: a masked dump, and the error
 *  without jansson's quote (json_error_for_log()). For bytes of a peer use
 *  anystring2json_from_peer().
 ***************************************************************************/
PUBLIC json_t *anystring2json(const char *bf, size_t len, BOOL verbose)
{
    if(!bf) {
        if(verbose) {
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "bf NULL",
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
            char error_shown[JSON_ERROR_TEXT_LENGTH];
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_JSON,
                "msg",          "%s", "json_loadb() FAILED",
                "error",        "%s", json_error_for_log(&error, error_shown, sizeof(error_shown)),
                "line",         "%d", error.line,
                "column",       "%d", error.column,
                "position",     "%d", error.position,
                "len",          "%lu", (unsigned long)len,
                NULL
            );
            gobj_trace_dump(0, bf, len, "json_loadb() FAILED");
        }
    }
    return jn;
}

/***************************************************************************
 *  Convert any json string to json binary.
 ***************************************************************************/
PUBLIC json_t *anyfile2json(const char *path, BOOL verbose)
{
    size_t flags = JSON_DECODE_ANY;
    json_error_t error;

    json_t *jn = json_load_file(path, flags, &error);
    if(!jn) {
        if(verbose) {
            char error_shown[JSON_ERROR_TEXT_LENGTH];
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "json_load_file() FAILED",
                "path",         "%s", path,
                "error",        "%s", json_error_for_log(&error, error_shown, sizeof(error_shown)),
                "line",         "%d", error.line,
                "column",       "%d", error.column,
                "position",     "%d", error.position,
                NULL
            );
        }
    }
    return jn;
}

/***************************************************************************
 *  Convert a legal json string to json binary.
 *  legal json string: MUST BE an array [] or object {}
 *  Old legalstring2json()
 *  verbose: as anystring2json()
 ***************************************************************************/
PUBLIC json_t *string2json(const char *str, BOOL verbose)
{
    size_t flags = 0;
    json_error_t error;

    json_t *jn = json_loads(str, flags, &error);
    if(!jn) {
        if(verbose) {
            char error_shown[JSON_ERROR_TEXT_LENGTH];
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "json_loads() FAILED",
                "error",        "%s", json_error_for_log(&error, error_shown, sizeof(error_shown)),
                "line",         "%d", error.line,
                "column",       "%d", error.column,
                "position",     "%d", error.position,
                NULL
            );
            if(str) {
                gobj_trace_dump(0, str, strlen(str), "json_loads() FAILED");
            }
        }
    }
    return jn;
}

/***************************************************************************
 *  Convert a legal json string to json binary removing comments.
 *  verbose: as anystring2json()
 ***************************************************************************/
PUBLIC json_t *json_config_string2json(const char *bf, BOOL verbose)
{
    size_t flags = 0;
    json_error_t error;

    if(!bf) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "bf NULL",
            NULL
        );
        return NULL;
    }

    /*
     *  Memory needed, double for src,dst
     */
    size_t len = strlen(bf);

    /*
     *  Create source and load the string
     */
    gbuffer_t *gbuf_src = gbuffer_create(len, len);
    if(!gbuf_src) {
        // Error already logged
        return NULL;
    }
    gbuffer_append_string(gbuf_src, bf);

    /*
     *  Create destination
     */
    gbuffer_t *gbuf_dst = gbuffer_create(len, len);
    if(!gbuf_dst) {
        // Error already logged
        gbuffer_decref(gbuf_src);
        return NULL;
    }

    /*
     *  Filter the comments and save to destination
     */
    char *s;
    while((s=gbuffer_getline(gbuf_src, '\n'))) {
        char *f = strstr(s, INLINE_COMMENT);
        if(f) {
            /*
             *  Remove comments
             */
            *f = 0;
        }
        if(!empty_string(s)) {
            gbuffer_append_string(gbuf_dst, s);
        }
    }

    /*
     *  Convert to jansson
     */
    s = gbuffer_cur_rd_pointer(gbuf_dst);

    json_t *jn_msg = json_loads(s, flags, &error);
    if(!jn_msg) {
        if(verbose) {
            char error_shown[JSON_ERROR_TEXT_LENGTH];
            gobj_log_error(0, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "json_loads() FAILED",
                "error",        "%s", json_error_for_log(&error, error_shown, sizeof(error_shown)),
                "line",         "%d", error.line,
                "column",       "%d", error.column,
                "position",     "%d", error.position,
                NULL
            );
            gobj_trace_dump(0, s, strlen(s), "json_loads() FAILED, comments removed");
        }
        gbuffer_decref(gbuf_src);
        gbuffer_decref(gbuf_dst);
        return NULL;
    }

    /*
     *  Free
     */
    gbuffer_decref(gbuf_src);
    gbuffer_decref(gbuf_dst);

    return jn_msg;
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
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "jn NULL",
            NULL
        );
        if(result) {
            (*result)--;
        }
        /*
         *  AND RETURN. It used to fall through to json_typeof(jn) below and
         *  segfault on the very NULL it had just reported -- a checker that
         *  crashes on the thing it exists to detect.
         */
        return -1;
    }

    switch(json_typeof(jn)) {
    case JSON_ARRAY:
        {
            if(jn->refcount <= 0) {
                gobj_log_error(0,0,
                    "gobj",         "%s", __FILE__,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
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
                    "msgset",       "%s", MSGSET_INTERNAL,
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
                    "msgset",       "%s", MSGSET_INTERNAL,
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
                    "msgset",       "%s", MSGSET_INTERNAL,
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
                "msgset",       "%s", MSGSET_INTERNAL,
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
                "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
 *  A dict or a list met by json_refcounts_report(), and where it is: its
 *  key in the dict of the frame `parent`, or its index in the list. Only a
 *  dict or a list gets a frame (32 bytes): a scalar is counted where its
 *  container is walked. Up to 7.26.9 a frame was reserved for EVERY child
 *  of the container being walked, scalars too, so a list of a million
 *  integers asked one block of 32 MB and the walk stopped at its root
 *  (REV-CLOUD V24.4.5).
 *
 *  The frames are kept in chunks of REFCOUNT_CHUNK_FRAMES: no block grows
 *  with the json, MAX_BLOCK bounds none of them.
 ***************************************************************************/
typedef struct {
    json_t *jn;
    size_t parent;          // (size_t)-1: the root
    const char *key;        // NULL: the item `idx` of a list
    size_t idx;
} refcount_frame_t;

#define REFCOUNT_CHUNK_FRAMES   (16*1024)   // 512 KB of frames
#define REFCOUNT_FIRST_FRAMES   64          // the first chunk doubles from here

typedef struct {
    refcount_frame_t **chunks;
    size_t n_chunks;
    size_t capacity;    // the frames of the chunks: the first one doubles up to a chunk
    size_t count;
} refcount_frames_t;

PRIVATE refcount_frame_t *refcount_frame_at(const refcount_frames_t *frames, size_t i)
{
    return &frames->chunks[i / REFCOUNT_CHUNK_FRAMES][i % REFCOUNT_CHUNK_FRAMES];
}

/*
 *  One more frame. 0, or -1 with no memory for it (Error already logged).
 */
PRIVATE int refcount_frames_push(refcount_frames_t *frames, refcount_frame_t frame)
{
    if(frames->count == frames->capacity) {
        if(frames->capacity < REFCOUNT_CHUNK_FRAMES) {
            /*
             *  The first chunk doubles, as a small json needs few frames:
             *  REFCOUNT_FIRST_FRAMES * 2^n reaches REFCOUNT_CHUNK_FRAMES
             */
            size_t new_capacity = frames->capacity? frames->capacity * 2 : REFCOUNT_FIRST_FRAMES;
            if(!frames->chunks) {
                frames->chunks = gbmem_malloc(sizeof(refcount_frame_t *));
                if(!frames->chunks) {
                    return -1;  // Error already logged
                }
                frames->chunks[0] = NULL;
                frames->n_chunks = 1;
            }
            refcount_frame_t *first = frames->chunks[0]?
                gbmem_realloc(frames->chunks[0], new_capacity * sizeof(refcount_frame_t)) :
                gbmem_malloc(new_capacity * sizeof(refcount_frame_t));
            if(!first) {
                return -1;  // Error already logged
            }
            frames->chunks[0] = first;
            frames->capacity = new_capacity;
        } else {
            refcount_frame_t **chunks = gbmem_realloc(frames->chunks,
                (frames->n_chunks + 1) * sizeof(refcount_frame_t *)
            );
            if(!chunks) {
                return -1;  // Error already logged
            }
            frames->chunks = chunks;
            frames->chunks[frames->n_chunks] = gbmem_malloc(REFCOUNT_CHUNK_FRAMES * sizeof(refcount_frame_t));
            if(!frames->chunks[frames->n_chunks]) {
                return -1;  // Error already logged
            }
            frames->n_chunks++;
            frames->capacity += REFCOUNT_CHUNK_FRAMES;
        }
    }
    *refcount_frame_at(frames, frames->count) = frame;
    frames->count++;
    return 0;
}

PRIVATE void refcount_frames_free(refcount_frames_t *frames)
{
    for(size_t i = 0; i < frames->n_chunks; i++) {
        GBMEM_FREE(frames->chunks[i])
    }
    GBMEM_FREE(frames->chunks)
    frames->n_chunks = 0;
    frames->capacity = 0;
    frames->count = 0;
}

/***************************************************************************
 *  The kw path of what is at (key or idx) in the frame `frame`
 ***************************************************************************/
PRIVATE json_t *refcount_path(
    const refcount_frames_t *frames,
    size_t frame,
    const char *key,
    size_t idx
)
{
    json_t *jn_segments = json_array();
    if(frame != (size_t)-1) {
        json_array_append_new(jn_segments, key? json_string(key) : json_sprintf("%zu", idx));
    }
    for(size_t i = frame; i != (size_t)-1 && refcount_frame_at(frames, i)->parent != (size_t)-1;
            i = refcount_frame_at(frames, i)->parent) {
        const refcount_frame_t *f = refcount_frame_at(frames, i);
        json_array_append_new(jn_segments, f->key? json_string(f->key) : json_sprintf("%zu", f->idx));
    }

    size_t ln = 1;
    size_t n = json_array_size(jn_segments);
    for(size_t i = 0; i < n; i++) {
        ln += json_string_length(json_array_get(jn_segments, i)) + 1;
    }
    char *bf = gbmem_malloc(ln);
    if(!bf) {
        JSON_DECREF(jn_segments)
        return json_string("");   // Error already logged
    }
    char *p = bf;
    for(size_t i = n; i > 0; i--) {
        json_t *jn_segment = json_array_get(jn_segments, i - 1);
        size_t l = json_string_length(jn_segment);
        if(p != bf) {
            *p++ = '`';
        }
        memcpy(p, json_string_value(jn_segment), l);
        p += l;
    }
    *p = 0;
    json_t *jn_path = json_string(bf);
    GBMEM_FREE(bf)
    JSON_DECREF(jn_segments)
    return jn_path? jn_path : json_string("");
}

/***************************************************************************
 *  What json_refcounts_report() has met, and how many times it reached
 *  each one: a set of pointers (open addressing, a load of 1/2 at most),
 *  with a count beside each slot. A slot is 8 bytes of pointer and 4 of
 *  count, and the slots double as they fill: 24 to 48 bytes per json
 *  walked, and while they double the old slots and the new ones are held
 *  together, 72 per json at that moment. The slots are kept in chunks of
 *  REFCOUNT_CHUNK_SLOTS (768 KB), so no block grows with the json: in one
 *  block, a million jsons asked 16 MB of pointers, MAX_BLOCK by default.
 ***************************************************************************/
#define REFCOUNT_CHUNK_SLOTS    (64*1024)

typedef struct {
    const void **slots;
    uint32_t *reached;  // the times the json of each slot was reached, saturated
} refcount_seen_chunk_t;

typedef struct {
    refcount_seen_chunk_t *chunks;
    size_t n_chunks;
    unsigned chunk_shift;   // the slots of a chunk: 1 << chunk_shift, REFCOUNT_CHUNK_SLOTS at most
    size_t capacity;    // n_chunks << chunk_shift, a power of 2; 0: nothing yet
    size_t count;
} refcount_seen_t;

PRIVATE void refcount_seen_free_chunks(refcount_seen_chunk_t *chunks, size_t n_chunks)
{
    for(size_t i = 0; chunks && i < n_chunks; i++) {
        GBMEM_FREE(chunks[i].slots)
        GBMEM_FREE(chunks[i].reached)
    }
    GBMEM_FREE(chunks)
}

PRIVATE size_t refcount_seen_slot(
    const refcount_seen_chunk_t *chunks,
    unsigned chunk_shift,
    size_t capacity,
    const void *p
)
{
    size_t chunk_mask = ((size_t)1 << chunk_shift) - 1;
    uint64_t h = ((uint64_t)(uintptr_t)p >> 4) * 0x9E3779B97F4A7C15ULL;
    size_t i = (size_t)(h >> 16) & (capacity - 1);
    while(1) {
        const void *slot = chunks[i >> chunk_shift].slots[i & chunk_mask];
        if(!slot || slot == p) {
            return i;
        }
        i = (i + 1) & (capacity - 1);
    }
}

/*
 *  One more reach of `p`: *reached is how many times it has been reached,
 *  this one included (1: the first). 0, or -1 with no memory to remember it
 *  (Error already logged).
 */
PRIVATE int refcount_seen_reach(refcount_seen_t *seen, const void *p, uint32_t *reached)
{
    if((seen->count + 1) * 2 > seen->capacity) {
        size_t new_capacity = seen->capacity? seen->capacity * 2 : 1024;
        size_t new_chunk_slots = MIN(new_capacity, (size_t)REFCOUNT_CHUNK_SLOTS);
        unsigned new_chunk_shift = 0;
        while(((size_t)1 << new_chunk_shift) < new_chunk_slots) {
            new_chunk_shift++;
        }
        size_t new_n_chunks = new_capacity / new_chunk_slots;
        refcount_seen_chunk_t *new_chunks = gbmem_malloc(new_n_chunks * sizeof(refcount_seen_chunk_t));
        if(!new_chunks) {
            return -1;  // Error already logged
        }
        memset(new_chunks, 0, new_n_chunks * sizeof(refcount_seen_chunk_t));
        for(size_t c = 0; c < new_n_chunks; c++) {
            new_chunks[c].slots = gbmem_malloc(new_chunk_slots * sizeof(void *));
            new_chunks[c].reached = new_chunks[c].slots?
                gbmem_malloc(new_chunk_slots * sizeof(uint32_t)) : NULL;
            if(!new_chunks[c].reached) {
                refcount_seen_free_chunks(new_chunks, new_n_chunks);
                return -1;  // Error already logged
            }
            memset(new_chunks[c].slots, 0, new_chunk_slots * sizeof(void *));
            memset(new_chunks[c].reached, 0, new_chunk_slots * sizeof(uint32_t));
        }
        size_t old_mask = ((size_t)1 << seen->chunk_shift) - 1;
        size_t new_mask = new_chunk_slots - 1;
        for(size_t i = 0; i < seen->capacity; i++) {
            const refcount_seen_chunk_t *old = &seen->chunks[i >> seen->chunk_shift];
            const void *slot = old->slots[i & old_mask];
            if(slot) {
                size_t j = refcount_seen_slot(new_chunks, new_chunk_shift, new_capacity, slot);
                new_chunks[j >> new_chunk_shift].slots[j & new_mask] = slot;
                new_chunks[j >> new_chunk_shift].reached[j & new_mask] = old->reached[i & old_mask];
            }
        }
        refcount_seen_free_chunks(seen->chunks, seen->n_chunks);
        seen->chunks = new_chunks;
        seen->n_chunks = new_n_chunks;
        seen->chunk_shift = new_chunk_shift;
        seen->capacity = new_capacity;
    }
    size_t i = refcount_seen_slot(seen->chunks, seen->chunk_shift, seen->capacity, p);
    refcount_seen_chunk_t *chunk = &seen->chunks[i >> seen->chunk_shift];
    size_t k = i & (((size_t)1 << seen->chunk_shift) - 1);
    if(!chunk->slots[k]) {
        chunk->slots[k] = p;
        seen->count++;
    }
    if(chunk->reached[k] < UINT32_MAX) {
        chunk->reached[k]++;
    }
    *reached = chunk->reached[k];
    return 0;
}

/***************************************************************************
 *  What json_refcounts_report() counts as it walks
 ***************************************************************************/
typedef struct {
    json_int_t max_refcount;
    int max_shown;
    json_int_t jsons;
    json_int_t corrupted;
    json_int_t over_max;
    BOOL truncated;         // stopped short: no memory for its frames or its set
    refcount_seen_t seen;   // every json met (but the singletons), and its reaches
    json_t *jn_shown;
} refcount_walk_t;

/***************************************************************************
 *  Check one reach of a json by the walk; TRUE when the walk goes into it
 *  (a dict or a list, sound, reached for the first time)
 ***************************************************************************/
PRIVATE BOOL refcount_check_one(
    json_t *jn,
    const refcount_frames_t *frames,
    size_t frame,
    const char *key,
    size_t idx,
    refcount_walk_t *walk
)
{
    if(walk->truncated) {
        return FALSE;   // stopped short: no memory to remember more
    }
    BOOL bad_type = FALSE;
    switch(json_typeof(jn)) { // json_typeof PROTECTED
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
            return FALSE;   // singletons, of no refcount
        case JSON_OBJECT:
        case JSON_ARRAY:
        case JSON_STRING:
        case JSON_INTEGER:
        case JSON_REAL:
            break;
        default:
            bad_type = TRUE;
            break;
    }

    /*
     *  EVERY json is counted each time it is reached, and a dict or a list
     *  is walked into the first time only, so a cycle ends. A json reached
     *  more times than its refcount says it is held has a refcount BELOW
     *  the truth -- what check-json exists to find: a cycle whose refcounts
     *  are corrupted, a json json_object_set_new()'d into two places. Such a
     *  cycle was walked round until its frames passed MAX_BLOCK (REV-CLOUD
     *  V22.5.3), and then ended without saying it (V23.4.4). The root is
     *  reached once by the caller, who holds it.
     */
    uint32_t reached = 0;
    if(refcount_seen_reach(&walk->seen, jn, &reached) < 0) {
        walk->truncated = TRUE;     // Error already logged
        return FALSE;
    }
    if(reached == 1) {
        walk->jsons++;
    }

    const char *fault = NULL;
    if(bad_type) {
        if(reached == 1) {
            fault = "corrupted";    // a type jansson does not have
        }
    } else if((size_t)reached == jn->refcount + 1) {
        fault = "corrupted";        // refcount 0 (freed) at once, or one holder too many
    } else if(reached == 1 && walk->max_refcount > 0 && (json_int_t)jn->refcount > walk->max_refcount) {
        fault = "over_max";
    }
    if(fault) {
        if(strcmp(fault, "corrupted")==0) {
            walk->corrupted++;
        } else {
            walk->over_max++;
        }
        if((int)json_array_size(walk->jn_shown) < walk->max_shown) {
            json_array_append_new(walk->jn_shown, json_pack("{s:o, s:I, s:s}",
                "path", refcount_path(frames, frame, key, idx),
                "refcount", (json_int_t)jn->refcount,
                "fault", fault
            ));
        }
    }
    return (!bad_type && reached == 1 && jn->refcount > 0 &&
        (json_is_object(jn) || json_is_array(jn)))? TRUE : FALSE;
}

/***************************************************************************
 *  What the refcounts of a json say (see helpers.h). A LOOP over the dicts
 *  and lists met, each kept as a frame (32 bytes per dict or list), with
 *  the reaches of every json counted (24 to 48 bytes per json, up to 72
 *  while the set doubles, see refcount_seen_t): json_check_refcounts()
 *  recursed, logged an ERROR for each fault, and went round a cycle for
 *  ever.
 ***************************************************************************/
PUBLIC json_t *json_refcounts_report(
    json_t *jn, // not owned
    json_int_t max_refcount,
    int max_shown
)
{
    if(!jn) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "jn NULL",
            NULL
        );
        return NULL;
    }

    refcount_walk_t walk = {
        .max_refcount = max_refcount,
        .max_shown = max_shown,
        .jn_shown = json_array()
    };
    refcount_frames_t frames = {0};

    if(refcount_check_one(jn, &frames, (size_t)-1, NULL, 0, &walk)) {
        if(refcount_frames_push(&frames, (refcount_frame_t){jn, (size_t)-1, NULL, 0}) < 0) {
            walk.truncated = TRUE;  // Error already logged
        }
    }

    for(size_t i = 0; i < frames.count && !walk.truncated; i++) {
        json_t *jn_container = refcount_frame_at(&frames, i)->jn;
        if(json_is_object(jn_container)) {
            const char *key; json_t *jn_child;
            json_object_foreach(jn_container, key, jn_child) {
                if(refcount_check_one(jn_child, &frames, i, key, 0, &walk)) {
                    if(refcount_frames_push(&frames, (refcount_frame_t){jn_child, i, key, 0}) < 0) {
                        walk.truncated = TRUE;  // Error already logged: the report says so
                        break;
                    }
                }
            }
        } else {
            size_t idx; json_t *jn_child;
            json_array_foreach(jn_container, idx, jn_child) {
                if(refcount_check_one(jn_child, &frames, i, NULL, idx, &walk)) {
                    if(refcount_frames_push(&frames, (refcount_frame_t){jn_child, i, NULL, idx}) < 0) {
                        walk.truncated = TRUE;  // Error already logged: the report says so
                        break;
                    }
                }
            }
        }
    }
    refcount_frames_free(&frames);
    refcount_seen_free_chunks(walk.seen.chunks, walk.seen.n_chunks);

    json_t *jn_report = json_pack("{s:I, s:I, s:I, s:I, s:b, s:o}",
        "max_refcount", max_refcount,
        "jsons", walk.jsons,
        "corrupted", walk.corrupted,
        "over_max", walk.over_max,
        "truncated", walk.truncated,
        "shown", walk.jn_shown
    );
    return jn_report;
}

/***************************************************************************
 *  See helpers.h. Recursive, at most JSON_PARSER_MAX_DEPTH levels deep (the
 *  depth a parser builds, and the one jansson dumps or copies): deeper is a
 *  fault of its own, a cycle among them.
 ***************************************************************************/
PRIVATE const char *json_nul_fault_depth(json_t *jn, int depth)
{
    if(json_is_string(jn)) {
        return (json_string_length(jn) != strlen(json_string_value(jn)))?
            "a string with a NUL in it" : NULL;
    }
    if(!json_is_object(jn) && !json_is_array(jn)) {
        return NULL;
    }
    if(depth >= JSON_PARSER_MAX_DEPTH) {
        return "a json nested deeper than a parser builds";
    }
    if(json_is_object(jn)) {
        const char *key;
        size_t key_len;
        json_t *value;
        json_object_keylen_foreach(jn, key, key_len, value) {
            if(key_len != strlen(key)) {
                return "a key with a NUL in it";
            }
            const char *fault = json_nul_fault_depth(value, depth + 1);
            if(fault) {
                return fault;
            }
        }
        return NULL;
    }
    size_t idx;
    json_t *value;
    json_array_foreach(jn, idx, value) {
        const char *fault = json_nul_fault_depth(value, depth + 1);
        if(fault) {
            return fault;
        }
    }
    return NULL;
}

PUBLIC const char *json_nul_fault(
    json_t *jn  // not owned
)
{
    return json_nul_fault_depth(jn, 0);
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "list MUST BE a json array",
            NULL
        );
        gobj_trace_json(gobj, jn_list, "list MUST BE a json array");
        return FALSE;
    }
    if(!str) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
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
 *  What a level of _walk_tree() answers
 ****************************************************************************/
#define WALK_DONE       0   // the directory was walked (or skipped, logged)
#define WALK_STOPPED    1   // a callback returned FALSE: the whole walk stops
#define WALK_FAILED     -1  // the walk fails (logged)

/****************************************************************************
 *  The path of an entry of a walk: `directory` + "/" + `name`, in `bf`
 *  (PATH_MAX). Refused with a log when it does not fit: up to 7.25.4
 *  build_path() dropped the name, the path was the directory itself, and
 *  the walk went into the SAME directory again, forever (a crash on a
 *  tree deeper than PATH_MAX), or a file was stat'ed as its directory and
 *  dropped from a listing.
 ****************************************************************************/
PRIVATE int walk_entry_path(
    hgobj gobj,
    char *bf,
    const char *directory,
    const char *name,
    const char *function
)
{
    size_t dir_len = strlen(directory);
    size_t name_len = strlen(name);
    if(dir_len + 1 + name_len >= PATH_MAX) {
        gobj_log_error(gobj, 0,
            "function",     "%s", function,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Path too long, the directory cannot be walked",
            "path",         "%.256s", directory,
            "name",         "%s", name,
            "path_len",     "%d", (int)dir_len,
            "PATH_MAX",     "%d", (int)PATH_MAX,
            NULL
        );
        return -1;
    }
    build_path(bf, PATH_MAX, directory, name, NULL);
    return 0;
}

/****************************************************************************
 *  A SUBdirectory of a walk that cannot be opened for one of these causes
 *  is skipped, with a warning: it is not readable by this user (EACCES),
 *  it went away (ENOENT), or it is no longer a directory (ENOTDIR, ELOOP).
 *  Any other cause is transient (EMFILE, ENFILE, ENOMEM, EIO): the walk
 *  fails, because a listing that lost a subdirectory is not the listing.
 *  Up to 7.25.4 it was skipped too, and the walk answered 0, short.
 ****************************************************************************/
PRIVATE BOOL walk_skips_subdirectory(int err)
{
    return (err == EACCES || err == ENOENT || err == ENOTDIR || err == ELOOP)? TRUE: FALSE;
}

/****************************************************************************
 *  Walk a directory, and its subdirectories with WD_RECURSIVE.
 *  Return WALK_DONE, WALK_STOPPED (a callback returned FALSE: every level
 *  above stops too; up to 7.25.4 only the directory of the callback
 *  stopped, and the walk went on with the next one), or WALK_FAILED.
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

    /*
     *  Each level keeps its directory open and a path of PATH_MAX on the
     *  stack: a deeper tree is refused, never walked into
     */
    if(level > MAX_TREE_DEPTH) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Tree too deep, the directory cannot be walked",
            "path",         "%.256s", root_dir,
            "max_depth",    "%d", MAX_TREE_DEPTH,
            NULL
        );
        return WALK_FAILED;
    }

    if (!(dir = opendir(root_dir))) {
        int last_errno = errno;
        if(level > 1 && walk_skips_subdirectory(last_errno)) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Cannot open subdirectory, it is skipped",
                "path",         "%s", root_dir,
                "errno",        "%d", last_errno,
                "serrno",       "%s", strerror(last_errno),
                NULL
            );
            return WALK_DONE;
        }
        // The ROOT that cannot be opened is a walk that fails: always logged
        // (up to 7.25.4 an EACCES root answered -1 with nothing logged).
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open directory",
            "path",         "%s", root_dir,
            "errno",        "%d", last_errno,
            "serrno",       "%s", strerror(last_errno),
            NULL
        );
        return WALK_FAILED;
    }

    /*
     *  errno tells the end of the directory from a failure of readdir()
     *  (EIO, ESTALE): up to 7.25.4 a failure was taken as the end, and the
     *  walk answered 0 with the entries it had not read yet missing.
     */
    while ((errno = 0, dent = readdir(dir))) {
        char *dname = dent->d_name;
        if(!strcmp(dname, ".") || !strcmp(dname, "..")) {
            continue;
        }
        if(!(opt & WD_HIDDENFILES) && dname[0] == '.') {
            continue;
        }

        char path[PATH_MAX];
        if(walk_entry_path(gobj, path, root_dir, dname, __FUNCTION__) < 0) {
            closedir(dir);
            return WALK_FAILED; // Error already logged
        }

#ifdef __linux__
        if(lstat(path, &st) == -1) {
#elif ESP_PLATFORM
        if(stat(path, &st) == -1) {
#else
    #error "What S.O.?"
#endif
            /*
             *  An entry that went away (ENOENT) or that this user cannot
             *  see (EACCES) is not listed. Any other failure is an entry
             *  lost: the walk fails (up to 7.25.4 it was logged and the
             *  entry dropped, and the walk answered 0).
             */
            if(errno == EACCES || errno == ENOENT) {
                continue;
            }
            int last_errno = errno;
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "stat() FAILED, the directory cannot be walked",
                "path",         "%s", path,
                "errno",        "%d", last_errno,
                "serrno",       "%s", strerror(last_errno),
                NULL
            );
            closedir(dir);
            return WALK_FAILED;
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
                    closedir(dir);
                    return WALK_STOPPED;
                }
            }
        }

        /* recursively follow dirs */
        if(S_ISDIR(st.st_mode)) {
            if ((opt & WD_RECURSIVE)) {
                int ret = _walk_tree(gobj, path, reg, user_data, opt, level, cb);
                if(ret != WALK_DONE) {
                    closedir(dir);
                    return ret; // WALK_STOPPED, or WALK_FAILED with the error already logged
                }
            }
        }
    }
    if(errno != 0) {
        int last_errno = errno;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read directory, readdir() FAILED",
            "path",         "%s", root_dir,
            "errno",        "%d", last_errno,
            "serrno",       "%s", strerror(last_errno),
            NULL
        );
        closedir(dir);
        return WALK_FAILED;
    }
    closedir(dir);
    return WALK_DONE;
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
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
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
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "regcomp() FAILED",
            "error",        "%d", ret,
           NULL
        );
        return -1;
    }

    /*
     *  A root that cannot be opened or read is logged by _walk_tree()
     */
    ret = _walk_tree(gobj, root_dir, &r, user_data, opt, 0, cb);
    regfree(&r);
    return ret < 0? -1: 0;  // Error already logged
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
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "No memory to dir_array",
                "sizeof",       "%d", new_capacity * sizeof(char *),
                NULL
            );
            return -1;
        }
        da->items = new_items;
        da->capacity = new_capacity;
    }

    char *item = gbmem_strdup(name);
    if(!item) {
        return -1;  // Error already logged
    }
    da->items[da->count] = item;
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
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot open directory",
            "path",         "%s", directory,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    while((errno = 0, entry = readdir(dir)) != NULL) {   // errno tells the end from a failure
        if(entry->d_name[0] == '.' &&
          (entry->d_name[1] == '\0' ||
           (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        /*
         *  Regular files only, a symbolic link is never listed: the d_type
         *  path and the lstat() path must agree (up to 7.25.4 the DT_UNKNOWN
         *  path used stat() and listed a link to a file).
         */
        int is_file = 0;

        #ifdef DT_REG
        BOOL ask_stat = (entry->d_type == DT_UNKNOWN)? TRUE: FALSE;
        if(entry->d_type == DT_REG) {
            is_file = 1;
        }
        #else
        BOOL ask_stat = TRUE;
        #endif
        if(ask_stat) {
            struct stat st;
            char path[PATH_MAX];

            if(walk_entry_path(gobj, path, directory, entry->d_name, __FUNCTION__) < 0) {
                closedir(dir);
                dir_array_free(da);
                return -1;  // Error already logged
            }
            if(stat_no_follow(path, &st) == 0) {
                if(S_ISREG(st.st_mode)) {
                    is_file = 1;
                }
            } else if(errno == EACCES) {
                /*
                 *  A directory that can be read and not searched: no entry
                 *  can be asked its type. It is listed, as the entry type
                 *  lists a file there, and whoever opens it meets the EACCES
                 *  and says it. Up to 7.25.4 it was skipped with no log:
                 *  timeranger2 read the key EMPTY and unflagged, where the
                 *  entry type flags it.
                 */
                is_file = 1;
            } else if(errno != ENOENT) {
                /*
                 *  An entry lost, not an entry gone: the listing fails
                 */
                int last_errno = errno;
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "Cannot list directory, stat() FAILED",
                    "path",         "%s", path,
                    "errno",        "%d", last_errno,
                    "serrno",       "%s", strerror(last_errno),
                    NULL
                );
                closedir(dir);
                dir_array_free(da);
                return -1;
            }
        }

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

        /*
         *  A listing that lost an entry is not the listing of the
         *  directory: up to 7.25.4 the entry was dropped and the listing
         *  answered 0, and timeranger2 read a key without the file.
         */
        if(dir_array_add(da, entry->d_name) < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "Cannot list directory, no memory for an entry",
                "path",         "%s", directory,
                "entries",      "%ld", (long)da->count,
                NULL
            );
            closedir(dir);
            dir_array_free(da);
            return -1;
        }
    }

    /*
     *  A readdir() that fails (EIO, ESTALE) is not the end of the
     *  directory: up to 7.25.4 it answered 0 with a short listing, and
     *  timeranger2 read a key without the md2 files it had not listed.
     */
    if(errno != 0) {
        int last_errno = errno;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot list directory, readdir() FAILED",
            "path",         "%s", directory,
            "entries",      "%ld", (long)da->count,
            "errno",        "%d", last_errno,
            "serrno",       "%s", strerror(last_errno),
            NULL
        );
        closedir(dir);
        dir_array_free(da);
        return -1;
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
typedef struct {
    dir_array_t *da;
    BOOL failed;
} fill_array_t;

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
    fill_array_t *fill = user_data;
    if(fill->failed) {
        return FALSE;   // Error already logged
    }

    if(dir_array_add(fill->da, (opt & WD_ONLY_NAMES)? filename: fullpath) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "Cannot list directory tree, no memory for an entry",
            "path",         "%s", fullpath,
            "entries",      "%ld", (long)fill->da->count,
            NULL
        );
        fill->failed = TRUE;
        return FALSE;   // stop traversing tree
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
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "Cannot open directory",
            "path",         "%s", root_dir,
           NULL
        );
        return -1;
    }

    if(!re) {
        re = ".*";  // NULL is every entry, as documented (up to 7.25.4 it crashed in regcomp)
    }
    int ret = regcomp(&r, re, REG_EXTENDED | REG_NOSUB);
    if(ret!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "regcomp() FAILED",
            "error",        "%d", ret,
           NULL
        );
        return -1;
    }

    /*
     *  Fill the array
     */
    fill_array_t fill = {.da = da, .failed = FALSE};
    int ret_walk = _walk_tree(gobj, root_dir, &r, &fill, opt, 0, fill_array_cb);
    regfree(&r);

    /*
     *  The root that cannot be opened is a listing that failed, not an
     *  empty one (up to 7.25.4 it answered 0). A subdirectory that cannot
     *  be opened is skipped (logged) only for EACCES, ENOENT, ENOTDIR,
     *  ELOOP; for a transient cause (EMFILE, EIO) it fails the listing (up
     *  to 7.25.4 it was skipped). A directory that cannot be READ
     *  (readdir() fails), the root or a subdirectory, fails the listing:
     *  up to 7.25.4 it was taken as the end of the directory. So do a path
     *  longer than PATH_MAX and a tree too deep. The cause is logged by
     *  _walk_tree().
     */
    if(ret_walk < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot list directory tree, the directory cannot be opened or read",
            "path",         "%s", root_dir,
            NULL
        );
        dir_array_free(da);
        return -1;
    }

    if(fill.failed) {
        dir_array_free(da);  // Error already logged
        return -1;
    }

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




/***************************************************************************
 *              Dates as text: read and write
 *
 *  approxidate_careful() reads a date the way a person types it on a
 *  command line ("2 days ago", "yesterday noon", "2026-10-01 10:00",
 *  "@1700000000 +0000"); parse_date_basic() reads only a whole date and
 *  time, or epoch seconds; show_date_relative() writes how long ago one
 *  was. The grammar, with examples, is in helpers.h.
 *
 *  A text is read in two steps: read_date_text() notes what each piece
 *  of it says (a date, a clock time, a zone, a duration back, ...) and
 *  resolve_reading() turns that into a time, against NOW (time(), so a
 *  test can fake it). A piece nobody understands makes the whole text
 *  unreadable: it is not skipped.
 *
 *  Written for Yuneta in 2026 from a description of the behaviour (the
 *  cases of tests/c/helpers/test_date_text.c). It replaces the port of
 *  git's date.c that came from yuneta v6 (see CHANGELOG.md).
 ***************************************************************************/
#define DATE_FUTURE_SLACK   (10*86400)  // a date with no year may be this far ahead
#define DATE_MAX_COUNT      100000000   // a count of units larger than this is no count
#define DATE_EPOCH_MIN      100000000   // a bare number from here on is epoch seconds
#define DATE_EPOCH_MAX      253402300799LL  // 9999-12-31 23:59:59 UTC: epoch seconds past it are
                                            // no date, but for "@<seconds> <offset>"

PRIVATE const char *month_full_names[12] = {
    "january", "february", "march", "april", "may", "june",
    "july", "august", "september", "october", "november", "december"
};
PRIVATE const char *weekday_full_names[7] = {
    "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"
};
PRIVATE const char *number_words[11] = {
    "zero", "one", "two", "three", "four", "five",
    "six", "seven", "eight", "nine", "ten"
};

/*
 *  Zone abbreviations, in minutes east of UTC. An abbreviation names one
 *  offset: CEST is +02:00 in any season.
 */
PRIVATE const struct {
    const char *name;
    int minutes;
} zone_names[] = {
    {"z",       0},
    {"ut",      0},
    {"utc",     0},
    {"gmt",     0},
    {"wet",     0},
    {"west",    60},
    {"bst",     60},
    {"cet",     60},
    {"cest",    120},
    {"met",     60},
    {"mest",    120},
    {"eet",     120},
    {"eest",    180},
    {"msk",     180},
    {"est",     -300},
    {"edt",     -240},
    {"cst",     -360},
    {"cdt",     -300},
    {"mst",     -420},
    {"mdt",     -360},
    {"pst",     -480},
    {"pdt",     -420},
    {"akst",    -540},
    {"akdt",    -480},
    {"hst",     -600},
    {"jst",     540},
    {"aest",    600},
    {"aedt",    660},
    {"nzst",    720},
    {"nzdt",    780},
};

typedef enum {
    DATE_UNIT_NONE = 0,
    DATE_UNIT_SECOND,
    DATE_UNIT_MINUTE,
    DATE_UNIT_HOUR,
    DATE_UNIT_DAY,
    DATE_UNIT_WEEK,
    DATE_UNIT_MONTH,
    DATE_UNIT_YEAR,
} date_unit_t;

/*
 *  What a text says. 0 (or -1 where 0 is a value) is "not said".
 */
typedef struct {
    time_t now;
    long now_offset;        // seconds east of UTC at now

    int64_t year;
    int mon;                // 1..12
    int mday;
    int hour;               // -1: no clock time
    int min;
    int sec;
    int ampm;               // 'a', 'p' or 0
    int special_hour;       // -1, or the hour of noon (12), midnight (0), tea (17)

    BOOL zone_said;
    long zone_offset;       // seconds east of UTC
    BOOL zone_utc_word;     // "GMT", "UTC" or "UT": an offset may follow ("GMT+0200")
    BOOL zone_numeric;      // the zone was said as an offset ("+0200")
    BOOL comment_zone_said; // a zone name in a comment ("(CEST)"): the zone when
    long comment_zone_offset;   // no other is said

    BOOL order_taken_back;  // M/D/Y (D.M.Y) taken in the other order because it is far ahead:
    int written_mon;        // a clock time after it undoes that, as the port did
    int written_mday;

    BOOL epoch_said;
    BOOL epoch_at;          // written as "@<seconds>"
    uintmax_t epoch;

    BOOL never;

    int64_t back_seconds;   // a duration back from now, in three parts:
    int64_t back_days;      // seconds, whole days,
    int64_t back_months;    // and calendar months
    int weekday_target;     // "last friday", "2 friday": -1 none, 0..6
    int64_t weekday_count;

    BOOL vague;             // a piece that is not an absolute date (refused by parse_date_basic)
    int understood;         // pieces that say something
    BOOL unreadable;        // a piece nobody understands
} date_reading_t;

/***************************************************************************
 *  The calendar: proleptic Gregorian
 ***************************************************************************/
PRIVATE BOOL leap_year(int64_t year)
{
    if(year % 400 == 0) {
        return TRUE;
    }
    if(year % 100 == 0) {
        return FALSE;
    }
    return (year % 4 == 0)? TRUE : FALSE;
}

PRIVATE int month_days(int64_t year, int mon)
{
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if(mon == 2 && leap_year(year)) {
        return 29;
    }
    return days[mon - 1];
}

/***************************************************************************
 *  Days from 1970-01-01 to the first day of `mon` (1..12) of `year` (>= 1)
 ***************************************************************************/
PRIVATE int64_t days_to_month(int64_t year, int mon)
{
    static const int days_before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int64_t whole_years = year - 1;
    int64_t days = whole_years*365 + whole_years/4 - whole_years/100 + whole_years/400;
    days -= 719162;     // the same count up to 1970-01-01
    days += days_before[mon - 1];
    if(mon > 2 && leap_year(year)) {
        days++;
    }
    return days;
}

/***************************************************************************
 *  Seconds from the epoch to a date and time of UTC. `mday`, `hour`,
 *  `min` and `sec` may pass their range: they are added as they are.
 ***************************************************************************/
PRIVATE int64_t utc_seconds(int64_t year, int mon, int64_t mday, int64_t hour, int64_t min, int64_t sec)
{
    return (days_to_month(year, mon) + mday - 1)*86400 + hour*3600 + min*60 + sec;
}

/***************************************************************************
 *  The offset of the local zone at `t`, in seconds east of UTC
 ***************************************************************************/
PRIVATE long local_offset_at(time_t t)
{
    struct tm lt;
    if(!localtime_r(&t, &lt)) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "localtime_r() FAILED, offset taken as 0",
            "t",            "%lld", (long long)t,
            NULL
        );
        return 0;
    }
    return (long)(utc_seconds(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
        lt.tm_hour, lt.tm_min, lt.tm_sec) - t);
}

/***************************************************************************
 *  Words: a month, a weekday (3 letters or more of the name), a unit, a
 *  number word, a zone
 ***************************************************************************/
PRIVATE int month_of_word(const char *word)
{
    size_t len = strlen(word);
    if(len < 3) {
        return 0;
    }
    for(int i = 0; i < 12; i++) {
        if(strncmp(month_full_names[i], word, len) == 0) {
            return i + 1;
        }
    }
    return 0;
}

PRIVATE int weekday_of_word(const char *word)
{
    size_t len = strlen(word);
    if(len < 3) {
        return -1;
    }
    for(int i = 0; i < 7; i++) {
        if(strncmp(weekday_full_names[i], word, len) == 0) {
            return i;
        }
    }
    return -1;
}

/*
 *  A weekday in the plural after its count: "2 fridays ago"
 */
PRIVATE int weekday_of_plural(const char *word)
{
    size_t len = strlen(word);
    for(int i = 0; i < 7; i++) {
        size_t name_len = strlen(weekday_full_names[i]);
        if(len == name_len + 1 && word[name_len] == 's' &&
                strncmp(weekday_full_names[i], word, name_len) == 0) {
            return i;
        }
    }
    return -1;
}

/*
 *  `raw` as written, `word` in lower case: a lone 'm' is a minute and a
 *  lone 'M' a month; any other unit is read in any case
 */
PRIVATE date_unit_t unit_of_word(const char *raw, const char *word)
{
    static const struct {
        const char *name;
        date_unit_t unit;
    } units[] = {
        {"s", DATE_UNIT_SECOND}, {"sec", DATE_UNIT_SECOND}, {"secs", DATE_UNIT_SECOND},
        {"second", DATE_UNIT_SECOND}, {"seconds", DATE_UNIT_SECOND},
        {"mi", DATE_UNIT_MINUTE}, {"min", DATE_UNIT_MINUTE}, {"mins", DATE_UNIT_MINUTE},
        {"minute", DATE_UNIT_MINUTE}, {"minutes", DATE_UNIT_MINUTE},
        {"h", DATE_UNIT_HOUR}, {"hr", DATE_UNIT_HOUR}, {"hrs", DATE_UNIT_HOUR},
        {"hour", DATE_UNIT_HOUR}, {"hours", DATE_UNIT_HOUR},
        {"d", DATE_UNIT_DAY}, {"day", DATE_UNIT_DAY}, {"days", DATE_UNIT_DAY},
        {"w", DATE_UNIT_WEEK}, {"wk", DATE_UNIT_WEEK}, {"wks", DATE_UNIT_WEEK},
        {"week", DATE_UNIT_WEEK}, {"weeks", DATE_UNIT_WEEK},
        {"mo", DATE_UNIT_MONTH}, {"mos", DATE_UNIT_MONTH},
        {"month", DATE_UNIT_MONTH}, {"months", DATE_UNIT_MONTH},
        {"y", DATE_UNIT_YEAR}, {"yr", DATE_UNIT_YEAR}, {"yrs", DATE_UNIT_YEAR},
        {"year", DATE_UNIT_YEAR}, {"years", DATE_UNIT_YEAR},
    };

    if(strcmp(raw, "m") == 0) {
        return DATE_UNIT_MINUTE;
    }
    if(strcmp(raw, "M") == 0) {
        return DATE_UNIT_MONTH;
    }
    for(size_t i = 0; i < sizeof(units)/sizeof(units[0]); i++) {
        if(strcmp(units[i].name, word) == 0) {
            return units[i].unit;
        }
    }
    return DATE_UNIT_NONE;
}

PRIVATE int number_of_word(const char *word)
{
    for(int i = 0; i < 11; i++) {
        if(strcmp(number_words[i], word) == 0) {
            return i;
        }
    }
    return -1;
}

PRIVATE BOOL zone_of_word(const char *word, long *offset_seconds)
{
    for(size_t i = 0; i < sizeof(zone_names)/sizeof(zone_names[0]); i++) {
        if(strcmp(zone_names[i].name, word) == 0) {
            *offset_seconds = (long)zone_names[i].minutes*60;
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  The letters at `p`, as written and in lower case. A word too long to
 *  be any of ours comes back empty. Return the end of the word.
 ***************************************************************************/
PRIVATE const char *take_word(const char *p, char *raw, char *word, size_t size)
{
    size_t len = 0;
    while(isalpha((unsigned char)p[len])) {
        len++;
    }
    if(len >= size) {
        raw[0] = 0;
        word[0] = 0;
        return p + len;
    }
    for(size_t i = 0; i < len; i++) {
        raw[i] = p[i];
        word[i] = (char)tolower((unsigned char)p[i]);
    }
    raw[len] = 0;
    word[len] = 0;
    return p + len;
}

/***************************************************************************
 *  A year of two digits: 00-37 is 20xx, 70-99 is 19xx, the rest is none
 ***************************************************************************/
PRIVATE int64_t year_of_two_digits(uintmax_t value)
{
    if(value <= 37) {
        return 2000 + (int64_t)value;
    }
    if(value >= 70 && value <= 99) {
        return 1900 + (int64_t)value;
    }
    return 0;
}

/***************************************************************************
 *  A date of the calendar, with years 1970-9999
 ***************************************************************************/
PRIVATE BOOL valid_date(int64_t year, int64_t mon, int64_t mday)
{
    if(year < 1970 || year > 9999 || mon < 1 || mon > 12 || mday < 1) {
        return FALSE;
    }
    return (mday <= month_days(year, (int)mon))? TRUE : FALSE;
}

/***************************************************************************
 *  TRUE when a date, at the time of day of now, is more than
 *  DATE_FUTURE_SLACK ahead of now
 ***************************************************************************/
PRIVATE BOOL far_ahead(date_reading_t *rd, int64_t year, int mon, int mday)
{
    time_t shifted = rd->now + rd->now_offset;
    struct tm nf;
    gmtime_r(&shifted, &nf);
    int64_t t = utc_seconds(year, mon, mday, nf.tm_hour, nf.tm_min, nf.tm_sec) - rd->now_offset;
    return (t > (int64_t)rd->now + DATE_FUTURE_SLACK)? TRUE : FALSE;
}

/***************************************************************************
 *  A date was read: only one per text
 ***************************************************************************/
PRIVATE void note_date(date_reading_t *rd, int64_t year, int mon, int mday)
{
    if((year && rd->year) || (mon && rd->mon) || (mday && rd->mday)) {
        rd->unreadable = TRUE;
        return;
    }
    if(year) {
        rd->year = year;
    }
    if(mon) {
        rd->mon = mon;
    }
    if(mday) {
        rd->mday = mday;
    }
    rd->understood++;
}

PRIVATE void note_clock(date_reading_t *rd, int64_t hour, int64_t min, int64_t sec)
{
    if(rd->hour >= 0 || rd->special_hour >= 0) {
        rd->unreadable = TRUE;
        return;
    }
    if(hour < 0 || hour > 24 || min < 0 || min > 59 || sec < 0 || sec > 60 ||
            (hour == 24 && (min || sec))) {
        rd->unreadable = TRUE;
        return;
    }
    rd->hour = (int)hour;
    rd->min = (int)min;
    rd->sec = (int)sec;
    rd->understood++;
}

PRIVATE void note_duration(date_reading_t *rd, uintmax_t count, date_unit_t unit)
{
    if(count > DATE_MAX_COUNT) {
        rd->unreadable = TRUE;
        return;
    }
    int64_t n = (int64_t)count;
    switch(unit) {
        case DATE_UNIT_SECOND:
            rd->back_seconds += n;
            break;
        case DATE_UNIT_MINUTE:
            rd->back_seconds += n*60;
            break;
        case DATE_UNIT_HOUR:
            rd->back_seconds += n*3600;
            break;
        case DATE_UNIT_DAY:
            rd->back_days += n;
            break;
        case DATE_UNIT_WEEK:
            rd->back_days += n*7;
            break;
        case DATE_UNIT_MONTH:
            rd->back_months += n;
            break;
        case DATE_UNIT_YEAR:
            rd->back_months += n*12;
            break;
        case DATE_UNIT_NONE:
        default:
            rd->unreadable = TRUE;
            return;
    }
    rd->vague = TRUE;
    rd->understood++;
}

PRIVATE void note_weekday_back(date_reading_t *rd, uintmax_t count, int weekday)
{
    if(rd->weekday_target >= 0 || count < 1 || count > DATE_MAX_COUNT) {
        rd->unreadable = TRUE;
        return;
    }
    rd->weekday_target = weekday;
    rd->weekday_count = (int64_t)count;
    rd->vague = TRUE;
    rd->understood++;
}

PRIVATE void note_ampm(date_reading_t *rd, int ampm)
{
    if(rd->ampm) {
        rd->unreadable = TRUE;
        return;
    }
    rd->ampm = ampm;
    rd->understood++;
}

/***************************************************************************
 *  What follows a count: a unit, a weekday or am/pm, attached ("2h") or
 *  after spaces, dots, commas or underscores ("2 hours", "1.day").
 *  Return the end of what it took, or NULL when nothing follows that a
 *  count goes with.
 ***************************************************************************/
PRIVATE const char *read_after_count(date_reading_t *rd, uintmax_t count, const char *p)
{
    const char *q = p;
    while(*q == ' ' || *q == '\t' || *q == '.' || *q == ',' || *q == '_') {
        q++;
    }
    if(!isalpha((unsigned char)*q)) {
        return NULL;
    }
    char raw[16];
    char word[16];
    const char *end = take_word(q, raw, word, sizeof(word));

    date_unit_t unit = unit_of_word(raw, word);
    if(unit != DATE_UNIT_NONE) {
        note_duration(rd, count, unit);
        return end;
    }
    int weekday = weekday_of_word(word);
    if(weekday < 0) {
        weekday = weekday_of_plural(word);
    }
    if(weekday >= 0) {
        note_weekday_back(rd, count, weekday);
        return end;
    }
    if(strcmp(word, "am") == 0 || strcmp(word, "pm") == 0) {
        if(count > 23) {
            rd->unreadable = TRUE;
            return end;
        }
        note_clock(rd, (int64_t)count, 0, 0);
        note_ampm(rd, word[0]);
        return end;
    }
    return NULL;
}

/***************************************************************************
 *  A clock time after its hour: mm[:ss[.fraction]]
 ***************************************************************************/
PRIVATE const char *read_clock(date_reading_t *rd, uintmax_t hour, const char *p)
{
    int64_t min = 0;
    int64_t sec = 0;
    int digits = 0;
    while(isdigit((unsigned char)*p) && digits < 3) {
        min = min*10 + (*p - '0');
        p++;
        digits++;
    }
    if(digits != 2) {
        rd->unreadable = TRUE;
        return p;
    }
    if(*p == ':' && isdigit((unsigned char)p[1])) {
        p++;
        digits = 0;
        while(isdigit((unsigned char)*p) && digits < 3) {
            sec = sec*10 + (*p - '0');
            p++;
            digits++;
        }
        if(digits != 2) {
            rd->unreadable = TRUE;
            return p;
        }
        if((*p == '.' || *p == ',') && isdigit((unsigned char)p[1])) {
            p++;
            while(isdigit((unsigned char)*p)) {
                p++;     // a fraction of a second is not kept
            }
        }
    }
    note_clock(rd, hour > 99? 99 : (int64_t)hour, min, sec);
    if(rd->order_taken_back && !rd->unreadable) {
        /*
         *  A whole date and time is read as written: the port read it
         *  with parse_date_basic(), which never took the other order
         */
        rd->mon = rd->written_mon;
        rd->mday = rd->written_mday;
        rd->order_taken_back = FALSE;
    }
    return p;
}

/***************************************************************************
 *  A date of numbers: Y-M-D, M/D/Y, D.M.Y, M/D, D.M (the separator is
 *  '-', '/' or '.', the same one all along). With the year first it is
 *  Y-M-D, or Y-D-M when the month cannot be one. Otherwise M/D is tried
 *  first with '-' and '/', D.M with '.', and the other order is taken
 *  when the first is no date or is more than 10 days ahead -- with '.'
 *  only when the other order is not more than 10 days ahead too
 *  ("5.4.2027" is April 5), and never for a date with its year and a
 *  clock time after it ("1.12.2026 10:00"), which is read as written.
 ***************************************************************************/
PRIVATE const char *read_numeric_date(date_reading_t *rd, const char *p)
{
    uintmax_t n[3] = {0, 0, 0};
    int len[3] = {0, 0, 0};
    int count = 0;
    char sep = 0;

    while(count < 3) {
        while(isdigit((unsigned char)*p) && len[count] < 9) {
            n[count] = n[count]*10 + (uintmax_t)(*p - '0');
            len[count]++;
            p++;
        }
        if(len[count] == 0 || isdigit((unsigned char)*p)) {
            rd->unreadable = TRUE;
            return p;
        }
        count++;
        if(count == 3) {
            break;
        }
        if(!sep && (*p == '-' || *p == '/' || *p == '.')) {
            sep = *p;
        }
        if(*p != sep || !isdigit((unsigned char)p[1])) {
            break;
        }
        p++;
    }

    int64_t year = 0;
    int64_t first_mon, first_mday, second_mon, second_mday;
    if(len[0] == 4) {
        /*
         *  Y-M-D, or Y-M; Y-D-M when the month cannot be one
         */
        year = (int64_t)n[0];
        if(count == 2) {
            if(!valid_date(year, (int64_t)n[1], 1)) {
                rd->unreadable = TRUE;
                return p;
            }
            note_date(rd, year, (int)n[1], 0);
            return p;
        }
        first_mon = (int64_t)n[1];
        first_mday = (int64_t)n[2];
        second_mon = (int64_t)n[2];
        second_mday = (int64_t)n[1];
    } else {
        if(count == 3) {
            if(len[2] == 4) {
                year = (int64_t)n[2];
            } else if(len[2] <= 2) {
                year = year_of_two_digits(n[2]);
            }
            if(!year) {
                rd->unreadable = TRUE;
                return p;
            }
        }
        if(sep == '.') {
            first_mday = (int64_t)n[0];
            first_mon = (int64_t)n[1];
        } else {
            first_mon = (int64_t)n[0];
            first_mday = (int64_t)n[1];
        }
        second_mon = first_mday;
        second_mday = first_mon;
    }

    int64_t check_year = year;
    if(!check_year) {
        time_t shifted = rd->now + rd->now_offset;
        struct tm nf;
        gmtime_r(&shifted, &nf);
        check_year = nf.tm_year + 1900;
    }
    BOOL first_valid = valid_date(check_year, first_mon, first_mday);
    BOOL second_valid = valid_date(check_year, second_mon, second_mday);

    BOOL first_taken = first_valid;
    if(first_valid && !year && far_ahead(rd, check_year, (int)first_mon, (int)first_mday)) {
        first_taken = FALSE;    // with no year, a date far ahead may be the other order
    }
    BOOL taken_back_with_year = FALSE;
    if(first_valid && year && len[0] != 4 &&
            far_ahead(rd, check_year, (int)first_mon, (int)first_mday)) {
        first_taken = FALSE;    // M/D/Y far ahead: D/M/Y, if it is a date
        taken_back_with_year = TRUE;
    }
    if(!first_taken && first_valid && sep == '.' && second_valid &&
            far_ahead(rd, check_year, (int)second_mon, (int)second_mday)) {
        first_taken = TRUE;     // D.M.Y: the other order only when it is not far ahead too
    }
    if(first_taken) {
        note_date(rd, year, (int)first_mon, (int)first_mday);
    } else if(second_valid) {
        note_date(rd, year, (int)second_mon, (int)second_mday);
        if(taken_back_with_year && rd->hour < 0) {
            rd->order_taken_back = TRUE;    // until a clock time follows (read_clock)
            rd->written_mon = (int)first_mon;
            rd->written_mday = (int)first_mday;
        }
    } else if(first_valid) {
        note_date(rd, year, (int)first_mon, (int)first_mday);
    } else {
        rd->unreadable = TRUE;
    }
    if((*p == 'T' || *p == 't') && isdigit((unsigned char)p[1])) {
        p++;    // ISO 8601: the clock time follows the date
    }
    return p;
}

/***************************************************************************
 *  The rest of a date with a month name between dashes or slashes, after
 *  its month and day: "-2026" ("01-Oct-2026", "Oct-01-2026"), and after
 *  a year with '/' the clock time of the common log format of web
 *  servers ("01/Oct/2026:10:00:00 +0200")
 ***************************************************************************/
PRIVATE const char *read_named_month_rest(date_reading_t *rd, int mon, uintmax_t mday, char sep, const char *p)
{
    if(mday < 1 || mday > 31) {
        rd->unreadable = TRUE;
        return p;
    }
    int64_t year = 0;
    if(*p == sep && isdigit((unsigned char)p[1])) {
        p++;
        uintmax_t value = 0;
        int digits = 0;
        while(isdigit((unsigned char)*p) && digits < 5) {
            value = value*10 + (uintmax_t)(*p - '0');
            digits++;
            p++;
        }
        if(digits == 4) {
            year = (int64_t)value;
        } else if(digits <= 2) {
            year = year_of_two_digits(value);
        }
        if(!year || isdigit((unsigned char)*p) || !valid_date(year, mon, (int64_t)mday)) {
            rd->unreadable = TRUE;
            return p;
        }
    }
    note_date(rd, year, mon, (int)mday);
    if(year && sep == '/' && *p == ':' && isdigit((unsigned char)p[1])) {
        p++;
        uintmax_t hour = 0;
        int digits = 0;
        while(isdigit((unsigned char)*p) && digits < 3) {
            hour = hour*10 + (uintmax_t)(*p - '0');
            digits++;
            p++;
        }
        if(digits > 2 || *p != ':' || !isdigit((unsigned char)p[1])) {
            rd->unreadable = TRUE;
            return p;
        }
        return read_clock(rd, hour, p + 1);
    }
    return p;
}

/***************************************************************************
 *  The day after a year and its month name: "2026-Oct-01", "2026/Oct/01"
 *  (the same separator), and the clock time of ISO 8601 after a 'T'
 ***************************************************************************/
PRIVATE const char *read_year_month_name_day(date_reading_t *rd, int64_t year, int mon, char sep, const char *p)
{
    if(*p != sep || !isdigit((unsigned char)p[1])) {
        rd->unreadable = TRUE;
        return p;
    }
    p++;
    int64_t mday = 0;
    int digits = 0;
    while(isdigit((unsigned char)*p) && digits < 3) {
        mday = mday*10 + (*p - '0');
        digits++;
        p++;
    }
    if(digits > 2 || !valid_date(year, mon, mday)) {
        rd->unreadable = TRUE;
        return p;
    }
    note_date(rd, year, mon, (int)mday);
    if((*p == 'T' || *p == 't') && isdigit((unsigned char)p[1])) {
        p++;    // ISO 8601: the clock time follows the date
    }
    return p;
}

/***************************************************************************
 *  A day of the month and its month name, between dashes or slashes:
 *  "01-Oct-2026", "01/Oct/2026:10:00:00"; or a year: "2026-Oct-01"
 ***************************************************************************/
PRIVATE const char *read_day_and_month_name(date_reading_t *rd, uintmax_t mday, int digits, const char *p)
{
    char sep = *p;
    char raw[16];
    char word[16];
    const char *end = take_word(p + 1, raw, word, sizeof(word));
    int mon = month_of_word(word);
    if(mon && digits == 4) {
        if(!valid_date((int64_t)mday, mon, 1)) {
            rd->unreadable = TRUE;
            return end;
        }
        return read_year_month_name_day(rd, (int64_t)mday, mon, sep, end);
    }
    if(!mon || digits > 2) {
        rd->unreadable = TRUE;
        return end;
    }
    return read_named_month_rest(rd, mon, mday, sep, end);
}

/***************************************************************************
 *  A number with nothing that makes it a count, a clock time or part of
 *  a date of numbers: epoch seconds, a year, a day of the month, a month
 ***************************************************************************/
PRIVATE void note_number(date_reading_t *rd, uintmax_t value, int digits)
{
    if(value >= DATE_EPOCH_MIN) {
        if(rd->epoch_said || value > (uintmax_t)INT64_MAX) {
            rd->unreadable = TRUE;
            return;
        }
        rd->epoch_said = TRUE;
        rd->epoch = value;
        rd->understood++;
        return;
    }
    if(digits == 4 && value >= 1970 && value <= 9999) {
        note_date(rd, (int64_t)value, 0, 0);
        return;
    }
    if(value >= 1 && value <= 31) {
        if(!rd->mday) {
            note_date(rd, 0, 0, (int)value);
            return;
        }
        if(!rd->mon && value <= 12) {
            note_date(rd, 0, (int)value, 0);
            return;
        }
    }
    if(digits <= 2 && value > 0 && !rd->year) {
        int64_t year = year_of_two_digits(value);
        if(year) {
            note_date(rd, year, 0, 0);
            return;
        }
    }
    rd->unreadable = TRUE;
}

/***************************************************************************
 *  A piece that starts with a digit
 ***************************************************************************/
PRIVATE const char *read_number(date_reading_t *rd, const char *p)
{
    const char *start = p;
    uintmax_t value = 0;
    int digits = 0;
    while(isdigit((unsigned char)*p)) {
        if(value > (UINTMAX_MAX - 9)/10) {
            rd->unreadable = TRUE;
            while(isdigit((unsigned char)*p)) {
                p++;
            }
            return p;
        }
        value = value*10 + (uintmax_t)(*p - '0');
        digits++;
        p++;
    }

    if(*p == ':' && isdigit((unsigned char)p[1])) {
        return read_clock(rd, value, p + 1);
    }
    if((*p == '-' || *p == '/' || *p == '.') && isdigit((unsigned char)p[1])) {
        return read_numeric_date(rd, start);
    }
    if((*p == '-' || *p == '/') && isalpha((unsigned char)p[1])) {
        return read_day_and_month_name(rd, value, digits, p);
    }
    if(isalpha((unsigned char)p[0]) && isalpha((unsigned char)p[1]) &&
            !isalpha((unsigned char)p[2])) {
        /*
         *  An ordinal: "1st", "2nd", "3rd", "5th" is a day of the month
         */
        char o0 = (char)tolower((unsigned char)p[0]);
        char o1 = (char)tolower((unsigned char)p[1]);
        if((o0 == 's' && o1 == 't') || (o0 == 'n' && o1 == 'd') ||
                (o0 == 'r' && o1 == 'd') || (o0 == 't' && o1 == 'h')) {
            if(value < 1 || value > 31 || digits > 2) {
                rd->unreadable = TRUE;
            } else {
                note_date(rd, 0, 0, (int)value);
            }
            return p + 2;
        }
    }
    if((*p == 'T' || *p == 't') && digits == 8 && isdigit((unsigned char)p[1])) {
        /*
         *  The basic form of ISO 8601: YYYYMMDDThhmm[ss]
         */
        int64_t year = (int64_t)(value/10000);
        int64_t mon = (int64_t)((value/100)%100);
        int64_t mday = (int64_t)(value%100);
        if(!valid_date(year, mon, mday)) {
            rd->unreadable = TRUE;
            return p;
        }
        note_date(rd, year, (int)mon, (int)mday);
        p++;
        int64_t clock = 0;
        int clock_digits = 0;
        while(isdigit((unsigned char)*p)) {
            if(clock_digits < 6) {
                clock = clock*10 + (*p - '0');
            }
            clock_digits++;
            p++;
        }
        if(clock_digits == 4) {
            note_clock(rd, clock/100, clock%100, 0);
        } else if(clock_digits == 6) {
            note_clock(rd, clock/10000, (clock/100)%100, clock%100);
        } else {
            rd->unreadable = TRUE;
        }
        return p;
    }

    const char *end = read_after_count(rd, value, p);
    if(end) {
        return end;
    }
    note_number(rd, value, digits);
    return p;
}

/***************************************************************************
 *  A zone as +hh, +hhmm or +hh:mm (or with '-'). Only after a clock time
 *  (or noon, midnight, tea) or epoch seconds: anywhere else a sign is no
 *  date. After GMT, UTC or UT it is the zone ("GMT+0200", as
 *  Date.toString() of javascript writes).
 ***************************************************************************/
PRIVATE const char *read_zone_offset(date_reading_t *rd, const char *p)
{
    int sign = (*p == '-')? -1 : 1;
    p++;
    if((rd->zone_said && !rd->zone_utc_word) ||
            (rd->hour < 0 && rd->special_hour < 0 && !rd->epoch_said)) {
        rd->unreadable = TRUE;
        return p;
    }
    rd->zone_utc_word = FALSE;  // "GMT+0200": the offset says the zone
    int64_t value = 0;
    int digits = 0;
    while(isdigit((unsigned char)*p) && digits < 5) {
        value = value*10 + (*p - '0');
        digits++;
        p++;
    }
    int64_t hours, minutes;
    if(digits == 2 && *p == ':' && isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2])) {
        hours = value;
        minutes = (p[1] - '0')*10 + (p[2] - '0');
        p += 3;
    } else if(digits == 2) {
        hours = value;
        minutes = 0;
    } else if(digits == 4) {
        hours = value/100;
        minutes = value%100;
    } else {
        rd->unreadable = TRUE;
        return p;
    }
    if(hours > 23 || minutes > 59 || isdigit((unsigned char)*p)) {
        rd->unreadable = TRUE;
        return p;
    }
    if(!rd->zone_said) {
        rd->understood++;
    }
    rd->zone_said = TRUE;
    rd->zone_numeric = TRUE;
    rd->zone_offset = sign*(long)(hours*3600 + minutes*60);
    return p;
}

/***************************************************************************
 *  TRUE when the count at `p` has a unit after it: after noon (midnight,
 *  tea) "-1d" is a duration and "-0500" a zone
 ***************************************************************************/
PRIVATE BOOL unit_follows_count(const char *p)
{
    while(isdigit((unsigned char)*p)) {
        p++;
    }
    while(*p == ' ' || *p == '\t' || *p == '.' || *p == ',' || *p == '_') {
        p++;
    }
    if(!isalpha((unsigned char)*p)) {
        return FALSE;
    }
    char raw[16];
    char word[16];
    take_word(p, raw, word, sizeof(word));
    return (unit_of_word(raw, word) != DATE_UNIT_NONE)? TRUE : FALSE;
}

/***************************************************************************
 *  A duration with a minus sign: "-1d", "-2 hours", "-1w ago". The port of
 *  git's date.c skipped the sign, so a count back from now was typed that
 *  way for years. Only a count with its unit: "-5" or "-2 friday" is no
 *  duration.
 ***************************************************************************/
PRIVATE const char *read_signed_duration(date_reading_t *rd, const char *p)
{
    p++;
    uintmax_t count = 0;
    while(isdigit((unsigned char)*p)) {
        if(count > (UINTMAX_MAX - 9)/10) {
            rd->unreadable = TRUE;
            return p;
        }
        count = count*10 + (uintmax_t)(*p - '0');
        p++;
    }
    const char *q = p;
    while(*q == ' ' || *q == '\t' || *q == '.' || *q == ',' || *q == '_') {
        q++;
    }
    if(!isalpha((unsigned char)*q)) {
        rd->unreadable = TRUE;
        return p;
    }
    char raw[16];
    char word[16];
    const char *end = take_word(q, raw, word, sizeof(word));
    date_unit_t unit = unit_of_word(raw, word);
    if(unit == DATE_UNIT_NONE) {
        rd->unreadable = TRUE;
        return end;
    }
    note_duration(rd, count, unit);
    return end;
}

/***************************************************************************
 *  A piece that starts with a letter
 ***************************************************************************/
PRIVATE const char *read_word(date_reading_t *rd, const char *p)
{
    char raw[16];
    char word[16];
    const char *end = take_word(p, raw, word, sizeof(word));
    if(!word[0]) {
        rd->unreadable = TRUE;
        return end;
    }

    long offset;
    if(zone_of_word(word, &offset)) {
        if(rd->zone_said) {
            rd->unreadable = TRUE;
            return end;
        }
        rd->zone_said = TRUE;
        rd->zone_offset = offset;
        if(strcmp(word, "gmt") == 0 || strcmp(word, "utc") == 0 || strcmp(word, "ut") == 0) {
            rd->zone_utc_word = TRUE;
        }
        rd->understood++;
        return end;
    }

    int mon = month_of_word(word);
    if(mon) {
        if((*end == '-' || *end == '/') && isdigit((unsigned char)end[1])) {
            /*
             *  "Oct-01-2026"
             */
            char sep = *end;
            const char *p_day = end + 1;
            uintmax_t mday = 0;
            int digits = 0;
            while(isdigit((unsigned char)*p_day) && digits < 3) {
                mday = mday*10 + (uintmax_t)(*p_day - '0');
                digits++;
                p_day++;
            }
            if(digits > 2) {
                rd->unreadable = TRUE;
                return p_day;
            }
            return read_named_month_rest(rd, mon, mday, sep, p_day);
        }
        note_date(rd, 0, mon, 0);
        return end;
    }

    if(weekday_of_word(word) >= 0) {
        return end;     // a weekday alone says nothing a date does not ("Thu, 1 Oct 2026")
    }

    if(strcmp(word, "now") == 0 || strcmp(word, "today") == 0) {
        rd->vague = TRUE;
        rd->understood++;
        return end;
    }
    if(strcmp(word, "yesterday") == 0) {
        rd->back_days++;
        rd->vague = TRUE;
        rd->understood++;
        return end;
    }
    if(strcmp(word, "noon") == 0 || strcmp(word, "midnight") == 0 || strcmp(word, "tea") == 0) {
        if(rd->special_hour >= 0 || rd->hour >= 0) {
            rd->unreadable = TRUE;
            return end;
        }
        rd->special_hour = (word[0] == 'n')? 12 : (word[0] == 'm')? 0 : 17;
        rd->vague = TRUE;
        rd->understood++;
        return end;
    }
    if(strcmp(word, "am") == 0 || strcmp(word, "pm") == 0) {
        note_ampm(rd, word[0]);
        return end;
    }
    if(strcmp(word, "never") == 0) {
        rd->never = TRUE;
        rd->vague = TRUE;
        rd->understood++;
        return end;
    }
    if(strcmp(word, "ago") == 0 || strcmp(word, "at") == 0 || strcmp(word, "on") == 0 ||
            strcmp(word, "the") == 0 || strcmp(word, "of") == 0) {
        return end;
    }
    if(strcmp(word, "last") == 0) {
        const char *after = read_after_count(rd, 1, end);
        if(!after) {
            rd->unreadable = TRUE;
            return end;
        }
        return after;
    }
    int number = number_of_word(word);
    if(number >= 0) {
        const char *after = read_after_count(rd, (uintmax_t)number, end);
        if(!after) {
            rd->unreadable = TRUE;
            return end;
        }
        return after;
    }

    rd->unreadable = TRUE;  // a unit with no count among them
    return end;
}

/***************************************************************************
 *  Note what each piece of `text` says
 ***************************************************************************/
PRIVATE void read_date_text(date_reading_t *rd, const char *text)
{
    memset(rd, 0, sizeof(*rd));
    rd->now = time(NULL);
    rd->now_offset = local_offset_at(rd->now);
    rd->hour = -1;
    rd->special_hour = -1;
    rd->weekday_target = -1;

    const char *p = text? text : "";
    while(*p && !rd->unreadable) {
        unsigned char c = (unsigned char)*p;
        if(isspace(c) || c == ',' || c == '_' || c == '.') {
            p++;
        } else if(c == '(') {
            /*
             *  A comment, as RFC 2822 writes the name of a zone: "(CEST)".
             *  Its first zone name is the zone when no other is said.
             */
            const char *close = strchr(p, ')');
            if(!close) {
                rd->unreadable = TRUE;
                break;
            }
            for(const char *q = p + 1; q < close; ) {
                if(!isalpha((unsigned char)*q)) {
                    q++;
                    continue;
                }
                const char *word_start = q;
                char raw[16];
                char word[16];
                q = take_word(q, raw, word, sizeof(word));
                if((unsigned char)word_start[-1] >= 0x80 || (unsigned char)*q >= 0x80) {
                    continue;   // part of a word of another alphabet ("estándar" is no EST)
                }
                long offset;
                if(!rd->comment_zone_said && word[0] && zone_of_word(word, &offset)) {
                    rd->comment_zone_said = TRUE;
                    rd->comment_zone_offset = offset;
                }
            }
            p = close + 1;
        } else if(isdigit(c)) {
            p = read_number(rd, p);
        } else if(isalpha(c)) {
            p = read_word(rd, p);
        } else if(c == '-' && isdigit((unsigned char)p[1]) &&
                rd->hour < 0 && !rd->epoch_said &&
                (rd->special_hour < 0 || unit_follows_count(p + 1))) {
            p = read_signed_duration(rd, p);
        } else if(c == '+' || c == '-') {
            p = read_zone_offset(rd, p);
        } else if(c == '@') {
            p++;
            if(!isdigit((unsigned char)*p)) {
                rd->unreadable = TRUE;
                break;
            }
            uintmax_t value = 0;
            while(isdigit((unsigned char)*p)) {
                if(value > (UINTMAX_MAX - 9)/10) {
                    rd->unreadable = TRUE;
                    break;
                }
                value = value*10 + (uintmax_t)(*p - '0');
                p++;
            }
            if(rd->unreadable || rd->epoch_said || value > (uintmax_t)INT64_MAX) {
                rd->unreadable = TRUE;
                break;
            }
            rd->epoch_said = TRUE;
            rd->epoch_at = TRUE;
            rd->epoch = value;
            rd->understood++;
        } else {
            rd->unreadable = TRUE;
        }
    }
    if(!rd->zone_said && rd->comment_zone_said) {
        rd->zone_said = TRUE;
        rd->zone_offset = rd->comment_zone_offset;
    }
}

/***************************************************************************
 *  The time that a reading says, and the offset to show it with (the one
 *  given, else the local one at that time), in minutes. Return -1 when
 *  the text says no time.
 *
 *  Without a clock time the date moves in the UTC offset of now: whole
 *  days are 24 hours, and a date keeps the time of day of now. A clock
 *  time (or noon, midnight, tea) is a local one, of the date reached.
 ***************************************************************************/
PRIVATE int resolve_reading(date_reading_t *rd, timestamp_t *result, int *offset_minutes)
{
    if(rd->unreadable || !rd->understood) {
        return -1;
    }
    if(rd->never) {
        *result = 0;
        *offset_minutes = 0;
        return 0;
    }
    if(rd->epoch_said) {
        if(rd->year || rd->mon || rd->mday || rd->hour >= 0 || rd->vague || rd->ampm) {
            return -1;
        }
        if(rd->epoch > (uintmax_t)DATE_EPOCH_MAX && !(rd->epoch_at && rd->zone_numeric)) {
            return -1;  // past the year 9999: no date ("1761000000000" is milliseconds)
        }
        time_t epoch = (time_t)rd->epoch;
        struct tm epoch_tm;
        if(!gmtime_r(&epoch, &epoch_tm) || !localtime_r(&epoch, &epoch_tm)) {
            return -1;  // no calendar holds it: no date
        }
        *result = rd->epoch;
        *offset_minutes = (int)((rd->zone_said? rd->zone_offset : local_offset_at((time_t)rd->epoch))/60);
        return 0;
    }

    long frame_offset = rd->zone_said? rd->zone_offset : rd->now_offset;
    time_t shifted = rd->now + frame_offset;
    struct tm nf;
    gmtime_r(&shifted, &nf);
    int64_t now_clock = nf.tm_hour*3600 + nf.tm_min*60 + nf.tm_sec;
    BOOL date_said = (rd->year || rd->mon || rd->mday)? TRUE : FALSE;

    int64_t year = rd->year? rd->year : nf.tm_year + 1900;
    int mon = rd->mon? rd->mon : nf.tm_mon + 1;
    int64_t mday = rd->mday? rd->mday : nf.tm_mday;
    if(!rd->mday && mday > month_days(year, mon)) {
        mday = month_days(year, mon);
    }
    if(mday > month_days(year, mon)) {
        return -1;  // "Feb 29" of a year that has none
    }
    if(date_said && !rd->year && far_ahead(rd, year, mon, (int)mday)) {
        year--;
        if(mday > month_days(year, mon)) {
            return -1;
        }
    }

    if(rd->back_months) {
        int64_t months = year*12 + (mon - 1) - rd->back_months;
        if(months < 12) {
            return -1;
        }
        year = months/12;
        mon = (int)(months%12) + 1;
        if(mday > month_days(year, mon)) {
            mday = month_days(year, mon);
        }
    }
    if(year < 1) {
        return -1;
    }

    int64_t day = days_to_month(year, mon) + mday - 1 - rd->back_days;
    if(rd->weekday_target >= 0) {
        int weekday = (int)(((day + 4)%7 + 7)%7);   // 1970-01-01 was a Thursday
        int64_t back = (weekday - rd->weekday_target + 7)%7;
        if(back == 0) {
            back = 7;
        }
        day -= back + 7*(rd->weekday_count - 1);
    }
    if(rd->special_hour >= 0 && !date_said && now_clock < rd->special_hour*3600) {
        day--;      // the last noon (midnight, tea) that passed
    }

    int64_t t;
    if(rd->hour >= 0 || rd->special_hour >= 0) {
        int64_t hour = (rd->hour >= 0)? rd->hour : rd->special_hour;
        int64_t min = (rd->hour >= 0)? rd->min : 0;
        int64_t sec = (rd->hour >= 0)? rd->sec : 0;
        if(rd->ampm == 'p' && hour < 12) {
            hour += 12;
        } else if(rd->ampm == 'a' && hour == 12) {
            hour = 0;
        }
        if(rd->zone_said) {
            t = day*86400 + hour*3600 + min*60 + sec - rd->zone_offset;
        } else {
            time_t midnight = (time_t)(day*86400);
            struct tm lt;
            gmtime_r(&midnight, &lt);
            lt.tm_hour = (int)hour;
            lt.tm_min = (int)min;
            lt.tm_sec = (int)sec;
            lt.tm_isdst = -1;
            t = (int64_t)mktime(&lt);
        }
    } else {
        int64_t clock = now_clock;
        if(rd->ampm == 'p' && clock < 12*3600) {
            clock += 12*3600;
        } else if(rd->ampm == 'a' && clock >= 12*3600) {
            clock -= 12*3600;
        }
        t = day*86400 + clock - frame_offset;
    }
    t -= rd->back_seconds;
    if(t < 0) {
        return -1;
    }

    *result = (timestamp_t)t;
    *offset_minutes = (int)((rd->zone_said? rd->zone_offset : local_offset_at((time_t)t))/60);
    return 0;
}

/***************************************************************************
 *  How long ago `t` was, in the unit that fits: seconds up to 89,
 *  minutes up to 89, hours up to 35, days up to 13, weeks up to 69 days,
 *  months up to 364 days, years and months up to 5 years, years. Each
 *  unit is rounded from the one below it.
 ***************************************************************************/
PUBLIC void show_date_relative(timestamp_t t, char *timebuf, int timebufsize)
{
    if(!timebuf || timebufsize <= 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "no buffer",
            NULL
        );
        return;
    }
    timestamp_t now = (timestamp_t)time(NULL);
    if(t > now) {
        snprintf(timebuf, (size_t)timebufsize, "in the future");
        return;
    }

    uintmax_t seconds = now - t;
    if(seconds < 90) {
        snprintf(timebuf, (size_t)timebufsize, "%ju %s ago", seconds, seconds == 1? "second" : "seconds");
        return;
    }
    uintmax_t minutes = (seconds + 30)/60;
    if(minutes < 90) {
        snprintf(timebuf, (size_t)timebufsize, "%ju minutes ago", minutes);
        return;
    }
    uintmax_t hours = (minutes + 30)/60;
    if(hours < 36) {
        snprintf(timebuf, (size_t)timebufsize, "%ju hours ago", hours);
        return;
    }
    uintmax_t days = (hours + 12)/24;
    if(days < 14) {
        snprintf(timebuf, (size_t)timebufsize, "%ju days ago", days);
        return;
    }
    if(days < 70) {
        snprintf(timebuf, (size_t)timebufsize, "%ju weeks ago", (days + 3)/7);
        return;
    }
    if(days < 365) {
        snprintf(timebuf, (size_t)timebufsize, "%ju months ago", (days + 15)/30);
        return;
    }
    if(days < 1825) {
        uintmax_t total_months = (days*24 + 365)/730;   // days*12/365, rounded
        uintmax_t years = total_months/12;
        uintmax_t months = total_months%12;
        if(months) {
            snprintf(timebuf, (size_t)timebufsize, "%ju %s, %ju %s ago",
                years, years == 1? "year" : "years",
                months, months == 1? "month" : "months");
        } else {
            snprintf(timebuf, (size_t)timebufsize, "%ju %s ago", years, years == 1? "year" : "years");
        }
        return;
    }
    snprintf(timebuf, (size_t)timebufsize, "%ju years ago", (days + 183)/365);
}

/***************************************************************************
 *  Only a whole date and time, or epoch seconds: the time and the
 *  offset it was written in (the local one at that time when none), in
 *  minutes. Return -1 for anything else.
 ***************************************************************************/
PUBLIC int parse_date_basic(const char *date, timestamp_t *timestamp, int *offset)
{
    date_reading_t rd;
    read_date_text(&rd, date);
    if(rd.vague || rd.special_hour >= 0) {
        return -1;
    }
    if(!rd.epoch_said && (!rd.year || !rd.mon || !rd.mday || rd.hour < 0)) {
        return -1;
    }
    timestamp_t t;
    int off;
    if(resolve_reading(&rd, &t, &off) < 0) {
        return -1;
    }
    *timestamp = t;
    *offset = off;
    return 0;
}

/***************************************************************************
 *  A date as a person types it; NOW, with *error_ret set, when the text
 *  says no date
 ***************************************************************************/
PUBLIC timestamp_t approxidate_careful(const char *date, int *error_ret)
{
    date_reading_t rd;
    timestamp_t t;
    int offset;
    read_date_text(&rd, date);
    if(resolve_reading(&rd, &t, &offset) < 0) {
        if(error_ret) {
            *error_ret = 1;
        }
        return (timestamp_t)rd.now;
    }
    if(error_ret) {
        *error_ret = 0;
    }
    return t;
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
    signed char val;
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
PUBLIC int print_json(const char *label, json_t *jn)
{
    if(!label) {
        label = "";
    }
    if(!jn || jn->refcount <= 0) {
        fprintf(stdout, "%s (%p) ERROR print_json(): json %s %d, type %d\n",
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
            if(strcmp(key, "gbuffer")==0) {
                gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)json_integer_value(jn_value);
                _trace_json(0, verbose, ", gbuf->refcount(%d)",
                    (int)(gbuf?gbuf->refcount:0)
                );
            }
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
        _trace_json(inside_list?deep:0, verbose, "\"%s\" (%d)",
            json_string_value(jn), (int)(jn->refcount)
        );
    } else if(json_is_integer(jn)) {
        _trace_json(inside_list?deep:0, verbose, "%"JSON_INTEGER_FORMAT" (%d)",
            json_integer_value(jn), (int)(jn->refcount)
        );
    } else if(json_is_real(jn)) {
        _trace_json(inside_list?deep:0, verbose, "%.2f (%d)",
            json_real_value(jn), (int)(jn->refcount)
        );
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
        fprintf(stdout, "%s%sERROR debug_json()%s: json NULL or refcount is 0\n",
            label,
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

/***************************************************************************
 *  Print json with refcounts
 ***************************************************************************/
PUBLIC int debug_json2(json_t *jn, const char *fmt, ...)
{
    va_list ap;
    char prefix[1024];

    if(!fmt) {
        fmt = "";
    }

    va_start(ap, fmt);
    vsnprintf(prefix, sizeof(prefix), fmt, ap);
    va_end(ap);

    int verbose = TRUE;
    if(!jn || jn->refcount <= 0) {
        fprintf(stdout, "%s%sERROR debug_json()%s: json NULL or refcount is 0\n",
            prefix,
            On_Red BWhite, Color_Off);
        return -1;
    }
    if(verbose) {
        fprintf(stdout, "%s\n", prefix);
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
PUBLIC time_t time_in_seconds(void)
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
#ifdef __linux__
    FILE *fp = fopen("/proc/self/stat", "r");
    if(!fp) {
        return 0;
    }

    unsigned long utime = 0;
    unsigned long stime = 0;

    int res = fscanf(fp,
        "%*d %*s %*c %*d %*d %*d %*d %*d "
        "%*u %*u %*u %*u %*u "
        "%lu %lu",
        &utime, &stime
    );
    if(res < 0) {}
    fclose(fp);

    return utime + stime;
#else
    return 0;
#endif
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
#ifdef __linux__
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
#else
    return 0;
#endif
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

/***************************************************************************
 *  Convert a 64-bit integer to host byte order
 ***************************************************************************/
PUBLIC void list_open_files(void)
{
#ifdef __linux__
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
            "msgset",       "%s", MSGSET_SYSTEM,
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
#endif
}

/*****************************************************************
 *  Convert GMT time `t` (seconds from Epoch) to Time Zone `tz`
 *  filling `ltm` and `offset`.
 *  Return t plus the offset of search time zone
 *  (use gmtime with the returned value)
 *  tz values in
 *      https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
 *  Example: time_t offset = gmtime2timezone(0, ":America/Panama", 0, 0);
 *  WARNING use ':' in TZ name
 *****************************************************************/
PUBLIC time_t gmtime2timezone(time_t t, const char *tz, struct tm *ltm, time_t *offset)
{
    /*
     *  Save current TZ of env
     */
    char *cur_tz = getenv("TZ");
    if (cur_tz) {
        cur_tz = strdup(cur_tz);
    }

    /*
     *  Set searched TZ in env
     */
    setenv("TZ", tz, 1);
    tzset();

    /*
     *  Get localtime of t with searched TZ
     */
    if(ltm) {
        localtime_r(&t, ltm);
    }

    /*
     *  Return offset of the searched TZ
     */
    time_t t_epoch = time(NULL);
    struct tm mt_epoch = {0};

    localtime_r(&t_epoch, &mt_epoch);
    if(offset) {
        *offset = mt_epoch.tm_gmtoff;
    }

    /*
     *  Restore current TZ of env
     */
    if (cur_tz) {
        setenv("TZ", cur_tz, 1);
        free(cur_tz);
    } else {
        unsetenv("TZ");
    }
    tzset();

    /*
     *  Return t plus the offset of searched TZ
     */
    t += mt_epoch.tm_gmtoff;
    return t;
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
            "msgset",       "%s", MSGSET_PARAMETER,
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
            "msgset",   "%s", MSGSET_RUNTIME,
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

    // Fallback only: on a packaged node, yuneta-node-uuid (packages/templates)
    // has already written uuid.json from the hardware, as root
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
    size_t len = strlen(key);
    for(i = 0; i < len; i++) {
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
    size_t len = strlen(key);
    for(i = 0; i < len; i++) {
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
        "msgset",       "%s", MSGSET_INTERNAL,
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
        int fd = open("/dev/null", O_RDWR|O_CLOEXEC);
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
 *  Helper: copy up to dst_size-1 chars from src (length src_len) into dst
 ***************************************************************************/
PRIVATE void copy_field(char *dst, size_t dst_size, const char *src, size_t src_len)
{
    if(!dst || dst_size == 0) return;
    if(src_len >= dst_size) {
        src_len = dst_size - 1;
    }
    memcpy(dst, src, src_len);
    dst[src_len] = 0;
}

/***************************************************************************
 *  Parse a URI into its components.
 *
 *  Supports:
 *      schema://host:port/path?query       (full URL)
 *      schema://host/path?query            (no port)
 *      host:port                           (no_schema mode)
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
    if(host) host[0] = 0;
    if(schema) schema[0] = 0;
    if(port) port[0] = 0;
    if(path) path[0] = 0;
    if(query) query[0] = 0;

    if(empty_string(uri)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_INTERNAL,
            "msg",                  "%s", "uri EMPTY",
            "url",                  "%s", uri,
            NULL
        );
        return -1;
    }

    const char *p = uri;

    if(no_schema) {
        /*
         *  no_schema mode: expect host:port (or just host)
         */
        const char *colon = strchr(p, ':');
        if(colon) {
            copy_field(host, host_size, p, (size_t)(colon - p));
            copy_field(port, port_size, colon + 1, strlen(colon + 1));
        } else {
            copy_field(host, host_size, p, strlen(p));
        }
        return 0;
    }

    /*
     *  Schema: look for "://"
     */
    const char *schema_end = strstr(p, "://");
    if(!schema_end) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_INTERNAL,
            "msg",                  "%s", "parse_url() FAILED, no schema",
            "url",                  "%s", uri,
            NULL
        );
        return -1;
    }
    copy_field(schema, schema_size, p, (size_t)(schema_end - p));
    p = schema_end + 3; // skip "://"

    /*
     *  Host (and optional port)
     *  Find the end of the authority section (delimited by '/' or '?' or end of string)
     */
    const char *authority_end = p;
    while(*authority_end && *authority_end != '/' && *authority_end != '?') {
        authority_end++;
    }

    /*
     *  Check for port in authority (host:port)
     *  Handle IPv6 addresses: [::1]:port
     */
    const char *host_start = p;
    const char *port_start = NULL;

    if(*p == '[') {
        /* IPv6 address */
        const char *bracket_end = memchr(p, ']', (size_t)(authority_end - p));
        if(bracket_end) {
            copy_field(host, host_size, p, (size_t)(bracket_end + 1 - p));
            if(bracket_end + 1 < authority_end && *(bracket_end + 1) == ':') {
                port_start = bracket_end + 2;
            }
        } else {
            copy_field(host, host_size, p, (size_t)(authority_end - p));
        }
    } else {
        /* IPv4 or hostname — find last colon in the authority */
        const char *colon = memchr(p, ':', (size_t)(authority_end - p));
        if(colon) {
            copy_field(host, host_size, host_start, (size_t)(colon - host_start));
            port_start = colon + 1;
        } else {
            copy_field(host, host_size, host_start, (size_t)(authority_end - host_start));
        }
    }

    if(port_start && port_start < authority_end) {
        copy_field(port, port_size, port_start, (size_t)(authority_end - port_start));
    }

    p = authority_end;

    /*
     *  Path
     */
    if(*p == '/') {
        const char *path_end = p;
        while(*path_end && *path_end != '?') {
            path_end++;
        }
        copy_field(path, path_size, p, (size_t)(path_end - p));
        p = path_end;
    }

    /*
     *  Query
     */
    if(*p == '?') {
        p++; // skip '?'
        copy_field(query, query_size, p, strlen(p));
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
    if(schema) schema[0] = 0;

    if(empty_string(uri)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "uri EMPTY",
            "url",          "%s", uri,
            NULL
        );
        return -1;
    }

    const char *schema_end = strstr(uri, "://");
    if(!schema_end) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "no schema found",
            "url",          "%s", uri,
            NULL
        );
        return -1;
    }

    if(schema && schema_size > 0) {
        copy_field(schema, schema_size, uri, (size_t)(schema_end - uri));
        return 0;
    }

    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "no schema found",
        "url",          "%s", uri,
        NULL
    );
    return -1;
}




                    /*------------------------------------*
                     *  ### Debug
                     *------------------------------------*/




#ifdef __linux__
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
#endif /* CONFIG_DEBUG_WITH_BACKTRACE */
#endif /* __linux__ */

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void show_backtrace_with_backtrace(loghandler_fwrite_fn_t fwrite_fn, void *h)
{
#ifdef __linux__
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
#endif /* CONFIG_DEBUG_WITH_BACKTRACE */
#endif /* __linux__ */
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int init_backtrace_with_backtrace(const char *program)
{
#ifdef __linux__
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
#endif /* CONFIG_DEBUG_WITH_BACKTRACE */
#endif /* __linux__ */
    return 0;
}

/***************************************************************
 *  Return the free RAM in kilobytes
 ***************************************************************/
PUBLIC unsigned long free_ram_in_kb(void)
{
#ifdef __linux__
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
#elif ESP_PLATFORM
    return 0; // TODO review
#else
#error "What S.O.?"
#endif
}

/***************************************************************
 *  Return the total RAM in kilobytes
 ***************************************************************/
PUBLIC unsigned long total_ram_in_kb(void)
{
#ifdef __linux__
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
#elif ESP_PLATFORM
    return 0; // TODO review
#else
#error "What S.O.?"
#endif
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
#ifdef __linux__
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

#endif
    return 0;
}

/***************************************************************************
 *  Copy the file `source` to `destination`, made by newfile() with
 *  `permission` (and refused when it exists and not `overwrite`).
 *  Return 0, or -1 (logged).
 *
 *  On Linux the kernel copies it (sendfile(2), as many calls as it takes:
 *  one call copies at most 2 GB); elsewhere a read/write loop.
 ***************************************************************************/
PUBLIC int copyfile(
    const char* source,
    const char* destination,
    int permission,
    BOOL overwrite
) {
    int input = open(source, O_RDONLY|O_CLOEXEC);
    if(input < 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open the file to copy",
            "path",         "%s", source,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    int output = newfile(destination, permission, overwrite);
    if(output < 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot create the copy",
            "path",         "%s", destination,
            "overwrite",    "%d", (int)overwrite,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        close(input);
        return -1;
    }

    int ret = 0;
    const char *failed = NULL;

#ifdef __linux__
    struct stat st;
    if(fstat(input, &st) < 0) {
        failed = "fstat() FAILED";
        ret = -1;
    } else {
        off_t offset = 0;
        while(offset < st.st_size) {
            ssize_t n = sendfile(output, input, &offset, (size_t)(st.st_size - offset));
            if(n < 0) {
                if(errno == EINTR) {
                    continue;
                }
                failed = "sendfile() FAILED";
                ret = -1;
                break;
            }
            if(n == 0) {
                failed = "sendfile() copied nothing: the file shrank";
                ret = -1;
                break;
            }
        }
    }
#else
    char buf[4096];
    while(1) {
        ssize_t nread = read(input, buf, sizeof(buf));
        if(nread < 0) {
            if(errno == EINTR) {
                continue;
            }
            failed = "read() FAILED";
            ret = -1;
            break;
        }
        if(nread == 0) {
            break;
        }
        ssize_t done = 0;
        while(done < nread) {
            ssize_t nwritten = write(output, buf + done, (size_t)(nread - done));
            if(nwritten < 0) {
                if(errno == EINTR) {
                    continue;
                }
                failed = "write() FAILED";
                ret = -1;
                break;
            }
            done += nwritten;
        }
        if(ret < 0) {
            break;
        }
    }
#endif

    if(ret < 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", failed,
            "source",       "%s", source,
            "destination",  "%s", destination,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
    }

    close(input);
    if(close(output) < 0 && ret == 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "close() of the copy FAILED",
            "destination",  "%s", destination,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        ret = -1;
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags < 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
             "function",     "%s", __FUNCTION__,
             "msgset",       "%s", MSGSET_SYSTEM,
             "msg",          "%s", "fcntl() FAILED",
             "serrno",       "%s", strerror(errno),
             NULL
         );

        print_error(0, "fcntl() FAILED: %s\n", strerror(errno));
        return -1;
    }
    flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if(flags < 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
             "function",     "%s", __FUNCTION__,
             "msgset",       "%s", MSGSET_SYSTEM,
             "msg",          "%s", "fcntl() FAILED",
             "serrno",       "%s", strerror(errno),
             NULL
        );
        print_error(0, "fcntl() FAILED: %s\n", strerror(errno));
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int set_cloexec(int fd) // children must not inherit
{
    if(fd < 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
             "function",     "%s", __FUNCTION__,
             "msgset",       "%s", MSGSET_SYSTEM,
             "msg",          "%s", "fd negative",
             NULL
         );
        return -1;
    }

    int flags = fcntl(fd, F_GETFD, 0);
    if(flags < 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
             "function",     "%s", __FUNCTION__,
             "msgset",       "%s", MSGSET_SYSTEM,
             "msg",          "%s", "fcntl() FAILED",
             "serrno",       "%s", strerror(errno),
             NULL
         );
        print_error(0, "fcntl() FAILED: %s\n", strerror(errno));
        return -1;
    }
    flags = fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    if(flags < 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
             "function",     "%s", __FUNCTION__,
             "msgset",       "%s", MSGSET_SYSTEM,
             "msg",          "%s", "fcntl() FAILED",
             "serrno",       "%s", strerror(errno),
             NULL
        );
        print_error(0, "fcntl() FAILED: %s\n", strerror(errno));
    }

    return 0;
}

/***************************************************************************
 *  Upper
 ***************************************************************************/
PUBLIC char *upper(char *s)
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
PUBLIC char *lower(char *s)
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
PUBLIC char *capitalize(char *s)
{
    lower(s);
    if(*s)
        *s = toupper(*s);
    return s;
}

/***************************************************************************
 *  Set TCP_NODELAY, SO_KEEPALIVE and SO_LINGER options to socket
 ***************************************************************************/
PUBLIC int set_tcp_socket_options(int fd, int delay)
{
    int ret = 0;
    int on = 1;

    // Always sent as soon as possible, even if there is only a small amount of data.
    ret += setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&on, sizeof(on));

    ret += setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
#ifdef TCP_KEEPIDLE
    if(!delay) {
        delay = 60; /* seconds */
    }
    int intvl = 1;  /*  1 second; same as default on Win32 */
    int cnt = 10;  /* 10 retries; same as hardcoded on Win32 */
    ret += setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &delay, sizeof(delay));
    ret += setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
    ret += setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#endif
    struct linger lg;
    lg.l_onoff = 1;        /* non-zero value enables linger option in kernel */
    lg.l_linger = 0;    /* timeout interval in seconds 0: close immediately discarding any unsent data  */
    ret += setsockopt( fd, SOL_SOCKET, SO_LINGER, (void *)&lg, sizeof(lg));
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL is_tcp_socket(int fd)
{
    int type;
    socklen_t optionLen = sizeof(type);

    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &optionLen)<0) {
        return FALSE;
    }
    if (type == SOCK_STREAM) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL is_udp_socket(int fd)
{
    int type;
    socklen_t optionLen = sizeof(type);

    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &optionLen)<0) {
        return FALSE;
    }
    if (type == SOCK_DGRAM) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int print_socket_address(char *buf, size_t buflen, const struct sockaddr *sa)
{
    if(!buf) {
        return -1;
    }
    if (sa->sa_family == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)sa;
        char ip[INET_ADDRSTRLEN];
        if(inet_ntop(AF_INET, &in->sin_addr, ip, sizeof(ip)) == NULL) {
            return -1;
        }
        unsigned port = (unsigned)ntohs(in->sin_port);
        (void)snprintf(buf, buflen, "%s:%u", ip, port);

    } else if (sa->sa_family == AF_INET6) {
        const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)sa;
        char ip6[INET6_ADDRSTRLEN];
        if(inet_ntop(AF_INET6, &in6->sin6_addr, ip6, sizeof(ip6)) == NULL) {
            return -1;
        }
        unsigned port = (unsigned)ntohs(in6->sin6_port);
        if(in6->sin6_scope_id) {
            (void)snprintf(buf, buflen, "[%s%%%u]:%u", ip6, (unsigned)in6->sin6_scope_id, port);
        } else {
            (void)snprintf(buf, buflen, "[%s]:%u", ip6, port);
        }

    } else {
        *buf = 0;
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int get_peername(char *bf, size_t bfsize, int fd)
{
    struct sockaddr_storage remoteAddr;
    socklen_t addrLen = sizeof(struct sockaddr_storage);

    // Get the remote socket address
    if (getpeername(fd, (struct sockaddr*)&remoteAddr, &addrLen) == -1) {
        if(bf && bfsize) {
            *bf = 0;
        }
        return -1;
    }
    print_socket_address(bf, bfsize, (struct sockaddr*)&remoteAddr);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int get_sockname(char *bf, size_t bfsize, int fd)
{
    struct sockaddr_storage localAddr;
    socklen_t addrLen = sizeof(struct sockaddr_storage);

    // Get the local socket address
    if (getsockname(fd, (struct sockaddr*)&localAddr, &addrLen) == -1) {
        if(bf && bfsize) {
            *bf = 0;
        }
        return -1;
    }
    print_socket_address(bf, bfsize, (struct sockaddr*)&localAddr);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int check_open_fds(void) // return # opened fd's
{
#ifdef __linux__
    DIR *dir = opendir("/proc/self/fd");
    if(!dir) {
        return 0;
    }

    int n = 0;

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.') {
            continue;
        }

        char path[300];
        snprintf(path, sizeof(path), "/proc/self/fd/%s", entry->d_name);

        char link[256];
        ssize_t len = readlink(path, link, sizeof(link)-1);
        if(len != -1) {
            n++;
        }
    }
    closedir(dir);

    return n;
#else
    return 0;
#endif
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int print_open_fds(const char *fmt, ...)
{
#ifdef __linux__
    va_list ap;
    char prefix[512];

    if(!fmt) {
        fmt = "";
    }

    va_start(ap, fmt);
    vsnprintf(prefix, sizeof(prefix), fmt, ap);
    va_end(ap);

    DIR *dir = opendir("/proc/self/fd");
    if(!dir) {
        return 0;
    }

    int n = 0;

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.') {
            continue;
        }

        char path[300];
        snprintf(path, sizeof(path), "/proc/self/fd/%s", entry->d_name);

        char link[256];
        ssize_t len = readlink(path, link, sizeof(link)-1);
        if(len != -1) {
            link[len] = '\0';
            trace_msg0("INHERIT %s: fd %s -> %s", prefix, entry->d_name, link);

            int fd = atoi(entry->d_name);
            if(is_tcp_socket(fd) || is_udp_socket(fd)) {
                char peername[64];
                char sockname[64];
                get_peername(peername, sizeof(peername), fd);
                get_sockname(sockname, sizeof(sockname), fd);
                trace_msg0("INHERIT %s: fd %s -> %s %s",
                    prefix,
                    entry->d_name,
                    peername,
                    sockname
                );
            }

            n++;
        }
    }
    closedir(dir);

    return n;
#else
    return 0;
#endif
}

/***************************************************************************
 *  Count the active inotify watches held by one inotify instance,
 *  reading /proc/self/fdinfo/<fd> and counting "inotify wd:" lines.
 ***************************************************************************/
PRIVATE int count_inotify_watches(const char *fd_name)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), "/proc/self/fdinfo", fd_name, NULL);

    FILE *file = fopen(path, "r");
    if(!file) {
        return 0;
    }
    int watches = 0;
    char line[256];
    while(fgets(line, sizeof(line), file)) {
        if(strncmp(line, "inotify wd:", 11) == 0) {
            watches++;
        }
    }
    fclose(file);
    return watches;
}

/***************************************************************************
 *  This process's own inotify usage: number of inotify instances (fds
 *  linking to "anon_inode:inotify" under /proc/self/fd) and the total
 *  number of watches across them.
 *
 *  There is no kernel API for the per-UID counters (they only surface as
 *  EMFILE on exhaustion), so this reports THIS process only.
 *
 *  Returns 0 on success, -1 if /proc/self/fd cannot be read.
 ***************************************************************************/
PUBLIC int get_inotify_self_usage(int *instances, int *watches)
{
    if(instances) {
        *instances = 0;
    }
    if(watches) {
        *watches = 0;
    }
#ifdef __linux__
    DIR *dir = opendir("/proc/self/fd");
    if(!dir) {
        return -1;
    }

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        if(entry->d_name[0] == '.') {
            continue;
        }
        char link_path[PATH_MAX];
        build_path(link_path, sizeof(link_path), "/proc/self/fd", entry->d_name, NULL);

        char target[64] = {0};
        ssize_t len = readlink(link_path, target, sizeof(target) - 1);
        if(len <= 0) {
            continue;
        }
        target[len] = 0;
        if(strcmp(target, "anon_inode:inotify") == 0) {
            if(instances) {
                (*instances)++;
            }
            if(watches) {
                *watches += count_inotify_watches(entry->d_name);
            }
        }
    }
    closedir(dir);

    return 0;
#else
    return -1;
#endif
}

/***************************************************************************
 * Static NSS replacements for CONFIG_FULLY_STATIC builds.
 * Read /etc/passwd and /etc/group directly — no dlopen/NSS needed.
 ***************************************************************************/
#ifdef CONFIG_FULLY_STATIC

/* Internal /etc/passwd line parser — fills *pw and buf on success */
PRIVATE int parse_passwd_line(char *line, struct passwd *pw, char *buf, size_t bufsz)
{
    /* Format: name:password:uid:gid:gecos:dir:shell */
    char *fields[7];
    char *p = line;
    for(int i = 0; i < 7; i++) {
        fields[i] = p;
        if(i < 6) {
            p = strchr(p, ':');
            if(!p) return -1;
            *p++ = '\0';
        }
    }
    /* Strip trailing newline / carriage-return from shell field */
    char *nl = strchr(fields[6], '\n');
    if(nl) *nl = '\0';
    char *cr = strchr(fields[6], '\r');
    if(cr) *cr = '\0';

    /* Copy strings into caller-supplied buf */
    size_t need = strlen(fields[0]) + strlen(fields[1]) +
                  strlen(fields[4]) + strlen(fields[5]) + strlen(fields[6]) + 5;
    if(need > bufsz) return -1;

    char *pos = buf;
    pw->pw_name   = pos; strcpy(pos, fields[0]); pos += strlen(fields[0]) + 1;
    pw->pw_passwd = pos; strcpy(pos, fields[1]); pos += strlen(fields[1]) + 1;
    pw->pw_uid    = (uid_t)atol(fields[2]);
    pw->pw_gid    = (gid_t)atol(fields[3]);
    pw->pw_gecos  = pos; strcpy(pos, fields[4]); pos += strlen(fields[4]) + 1;
    pw->pw_dir    = pos; strcpy(pos, fields[5]); pos += strlen(fields[5]) + 1;
    pw->pw_shell  = pos; strcpy(pos, fields[6]); pos += strlen(fields[6]) + 1;
    (void)pos;
    return 0;
}

PRIVATE struct passwd *static_getpwuid(uid_t uid)
{
    static struct passwd result;
    static char buf[2048];

    FILE *f = fopen("/etc/passwd", "r");
    if(!f) return NULL;

    char line[2048];
    while(fgets(line, sizeof(line), f)) {
        if(line[0] == '#' || line[0] == '\n') continue;
        char tmp[2048];
        strncpy(tmp, line, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        struct passwd pw;
        if(parse_passwd_line(tmp, &pw, buf, sizeof(buf)) == 0) {
            if(pw.pw_uid == uid) {
                result = pw;
                fclose(f);
                return &result;
            }
        }
    }
    fclose(f);
    return NULL;
}

PRIVATE struct passwd *static_getpwnam(const char *name)
{
    static struct passwd result;
    static char buf[2048];

    if(!name) return NULL;

    FILE *f = fopen("/etc/passwd", "r");
    if(!f) return NULL;

    char line[2048];
    while(fgets(line, sizeof(line), f)) {
        if(line[0] == '#' || line[0] == '\n') continue;
        char tmp[2048];
        strncpy(tmp, line, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        struct passwd pw;
        if(parse_passwd_line(tmp, &pw, buf, sizeof(buf)) == 0) {
            if(strcmp(pw.pw_name, name) == 0) {
                result = pw;
                fclose(f);
                return &result;
            }
        }
    }
    fclose(f);
    return NULL;
}

/* Internal /etc/group line parser */
PRIVATE int parse_group_line(char *line, struct group *gr, char *buf, size_t bufsz)
{
    /* Format: name:password:gid:member1,member2,... */
    char *fields[4];
    char *p = line;
    for(int i = 0; i < 4; i++) {
        fields[i] = p;
        if(i < 3) {
            p = strchr(p, ':');
            if(!p) return -1;
            *p++ = '\0';
        }
    }
    char *nl = strchr(fields[3], '\n');
    if(nl) *nl = '\0';
    char *cr = strchr(fields[3], '\r');
    if(cr) *cr = '\0';

    size_t need = strlen(fields[0]) + strlen(fields[1]) + strlen(fields[3]) + 3;
    if(need > bufsz) return -1;

    char *pos = buf;
    gr->gr_name   = pos; strcpy(pos, fields[0]); pos += strlen(fields[0]) + 1;
    gr->gr_passwd = pos; strcpy(pos, fields[1]); pos += strlen(fields[1]) + 1;
    gr->gr_gid    = (gid_t)atol(fields[2]);
    gr->gr_mem    = NULL; /* caller accesses gr_gid only */
    (void)pos;
    return 0;
}

PRIVATE struct group *static_getgrnam(const char *name)
{
    static struct group result;
    static char buf[2048];

    if(!name) return NULL;

    FILE *f = fopen("/etc/group", "r");
    if(!f) return NULL;

    char line[2048];
    while(fgets(line, sizeof(line), f)) {
        if(line[0] == '#' || line[0] == '\n') continue;
        char tmp[2048];
        strncpy(tmp, line, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        struct group gr;
        if(parse_group_line(tmp, &gr, buf, sizeof(buf)) == 0) {
            if(strcmp(gr.gr_name, name) == 0) {
                result = gr;
                fclose(f);
                return &result;
            }
        }
    }
    fclose(f);
    return NULL;
}

PRIVATE int static_getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups)
{
    int max = *ngroups;
    int count = 0;
    int truncated = 0;

    /* Primary group is always first */
    if(count < max) {
        groups[count++] = group;
    } else {
        truncated = 1;
    }

    FILE *f = fopen("/etc/group", "r");
    if(!f) {
        *ngroups = count;
        return truncated ? -1 : count;
    }

    char line[2048];
    while(fgets(line, sizeof(line), f)) {
        if(line[0] == '#' || line[0] == '\n') continue;

        char tmp[2048];
        strncpy(tmp, line, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';

        /* Find the 4th field (members) */
        char *p = tmp;
        int colon = 0;
        while(*p && colon < 3) {
            if(*p == ':') colon++;
            p++;
        }
        /* p now points to members field, format: user1,user2,... */
        char *nl = strchr(p, '\n');
        if(nl) *nl = '\0';

        /* Get gid from 3rd field */
        char *gid_start = tmp;
        int c = 0;
        while(*gid_start && c < 2) {
            if(*gid_start == ':') c++;
            gid_start++;
        }
        char *gid_end = strchr(gid_start, ':');
        if(!gid_end) continue;
        *gid_end = '\0';
        gid_t gr_gid = (gid_t)atol(gid_start);

        /* Skip if this is the primary group (already added) */
        if(gr_gid == group) continue;

        /* Check if user is in the member list */
        char *members = p;
        char *tok = members;
        char *next;
        BOOL found = FALSE;
        while(tok && *tok) {
            next = strchr(tok, ',');
            if(next) *next = '\0';
            if(strcmp(tok, user) == 0) {
                found = TRUE;
                break;
            }
            tok = next ? next + 1 : NULL;
        }

        if(found) {
            if(count < max) {
                groups[count++] = gr_gid;
            } else {
                truncated = 1;
            }
        }
    }
    fclose(f);

    *ngroups = count;
    return truncated ? -1 : count;
}

#endif /* CONFIG_FULLY_STATIC */

/***************************************************************************
 * Portable wrappers — callers use these; #ifdef stays here only.
 ***************************************************************************/
struct passwd *yuneta_getpwuid(uid_t uid)
{
#ifdef CONFIG_FULLY_STATIC
    return static_getpwuid(uid);
#else
    return getpwuid(uid);
#endif
}

struct passwd *yuneta_getpwnam(const char *name)
{
#ifdef CONFIG_FULLY_STATIC
    return static_getpwnam(name);
#else
    return getpwnam(name);
#endif
}

struct group *yuneta_getgrnam(const char *name)
{
#ifdef CONFIG_FULLY_STATIC
    return static_getgrnam(name);
#else
    return getgrnam(name);
#endif
}

int yuneta_getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups)
{
#ifdef CONFIG_FULLY_STATIC
    return static_getgrouplist(user, group, groups, ngroups);
#else
    return getgrouplist(user, group, groups, ngroups);
#endif
}

/***************************************************************************
 * Check if a given username belongs to the "yuneta" group
 * or is exactly the "yuneta" user.
 *
 * Return:
 *      TRUE -> user is "yuneta" or belongs to "yuneta" group
 *      FALSE -> otherwise
 ***************************************************************************/
PUBLIC int is_yuneta_user(const char *username)
{
#ifdef ESP_PLATFORM
    return TRUE;
#elif __linux__
    if(!username) {
        return FALSE;
    }

    /* Check if user is exactly "yuneta" */
    if(strcmp(username, "yuneta") == 0) {
        return TRUE;
    }

    /* Get passwd entry for user */
    struct passwd *pw = yuneta_getpwnam(username);
    if(!pw) {
        return FALSE;
    }

    /* Get group entry for "yuneta" */
    struct group *gr = yuneta_getgrnam("yuneta");
    if(!gr) {
        return FALSE;
    }

    /* Check if user's primary group is yuneta */
    if(pw->pw_gid == gr->gr_gid) {
        return TRUE;
    }

    /* Check supplementary groups */
    #define NGROUPS 256
    gid_t groups[NGROUPS];
    int ngroups = NGROUPS;

    if(yuneta_getgrouplist(username, pw->pw_gid, groups, &ngroups) == -1) {
        /* In case of error, fallback: user likely has more groups than NGROUPS */
        return FALSE;
    }

    for(int i = 0; i < ngroups; i++) {
        if(groups[i] == gr->gr_gid) {
            return TRUE;
        }
    }

    return FALSE;
#else
    #error "What S.O.?"
#endif
}

/***************************************************************************
 *  esp-idf has not basename
 ***************************************************************************/
PUBLIC const char *path_basename(const char *path)
{
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

/***************************************************************************
 *  Resolve YUNETAS_BASE with this precedence:
 *      1) $YUNETAS_BASE if it exists and is a directory
 *      2) /yuneta/development/yunetas
 *      3) /yuneta/development
 ***************************************************************************/
PUBLIC const char *get_yunetas_base(void)
{
    const char *env = getenv("YUNETAS_BASE");

    if(env) {
        if(!is_directory(env)) {
            print_error(0,
                "Warning: YUNETAS_BASE is set to '%s' but is not a directory."
                " Falling back...\n",
                env
            );
        } else {
            return env;
        }
    }

    if(is_directory("/yuneta/development/yunetas")) {
        return "/yuneta/development/yunetas";
    }
    if(is_directory("/yuneta/development")) {
        return "/yuneta/development";
    }

    print_error(PEF_EXIT,
        "Error: Could not determine YUNETAS_BASE. "
        "Set the environment variable to a valid directory, "
        "or ensure /yuneta/development[/yunetas] exists.\n");
    return NULL;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC gbuffer_t *source2base64_for_yunetas(
    const char *source,
    char *comment,
    int commentlen
) {
    /*------------------------------------------------*
     *          Check source
     *  Frequently, You want install install the output
     *  of your yuno's make install command.
     *------------------------------------------------*/
    if(empty_string(source)) {
        snprintf(comment, commentlen, "%s", "source empty");
        return 0;
    }

    char path[PATH_MAX];
    if(access(source, 0)==0 && is_regular_file(source)) {
        snprintf(path, sizeof(path), "%s", source);
    } else {
        const char *yunetas_base = get_yunetas_base();
        build_path(path, sizeof(path), yunetas_base, "outputs/yunos", source, NULL);
    }

    if(access(path, 0)!=0) {
        snprintf(comment, commentlen, "source '%s' not found", source);
        return 0;
    }
    if(!is_regular_file(path)) {
        snprintf(comment, commentlen, "source '%s' is not a regular file", path);
        return 0;
    }
    gbuffer_t *gbuf_b64 = gbuffer_file2base64(path);
    if(!gbuf_b64) {
        snprintf(comment, commentlen, "conversion '%s' to base64 failed", path);
    }
    return gbuf_b64;
}

/***************************************************************************
 *  $$ interfere with bash, use ^^ as alternative
 ***************************************************************************/
PUBLIC gbuffer_t *replace_cli_vars(
    const char *command,
    char *comment,
    int commentlen
) {
    gbuffer_t *gbuf = gbuffer_create(4*1024, gbmem_get_maximum_block());
    char *command_ = gbmem_strdup(command);
    char *p = command_;

    const char *prefix = "$$";  // default
    if(strstr(p, "^^")) {
        prefix = "^^";
    }

    char *n, *f;
    while((n=strstr(p, prefix))) {
        *n = 0;
        gbuffer_append(gbuf, p, strlen(p));

        n += 2;
        if(*n == '(') {
            f = strchr(n, ')');
        } else {
            gbuffer_decref(gbuf);
            gbmem_free(command_);
            snprintf(comment, commentlen, "%s", "Bad format of $$: use $$(...) or ^^(...)");
            return NULL;
        }
        if(!f) {
            gbuffer_decref(gbuf);
            gbmem_free(command_);
            snprintf(comment, commentlen, "%s", "Bad format of $$: use $$(...) or ^^(...)");
            return NULL;
        }
        *n = 0;
        n++;
        *f = 0;
        f++;

        gbuffer_t *gbuf_b64 = source2base64_for_yunetas(n, comment, commentlen);
        if(!gbuf_b64) {
            gbuffer_decref(gbuf);
            gbmem_free(command_);
            return NULL;
        }

        gbuffer_append(gbuf, "'", 1);
        gbuffer_append_gbuf(gbuf, gbuf_b64);
        gbuffer_append(gbuf, "'", 1);
        gbuffer_decref(gbuf_b64);

        p = f;
    }
    if(!empty_string(p)) {
        gbuffer_append(gbuf, p, strlen(p));
    }

    gbmem_free(command_);
    return gbuf;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int get_number_from_nn_table(const number_name_table_t *table, const char *name)
{
    if(!table || !name) {
        return -1;
    }

    const number_name_table_t *p = table;

    while(p->name) {                    // table ends with name == NULL
        if(strcmp(p->name, name)==0) {
            return p->number;
        }
        p++;                            // advance pointer
    }

    return -1;  // not found
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char *get_name_from_nn_table(const number_name_table_t *table, int number)
{
    if(!table) {
        return NULL;
    }

    const number_name_table_t *p = table;

    while(p->name) {
        if(p->number == number) {
            return p->name;
        }
        p++;
    }

    return NULL;  // not found
}

/***************************************************************************
 *  Return in gmt time range in hours of time t, TZ optional
 ***************************************************************************/
PUBLIC time_range_t get_hours_range(time_t t, int range, const char *TZ)
{
    time_range_t time_range;

    struct tm tm;
    if(!empty_string(TZ)) {
        gmtime2timezone(t, TZ, &tm, 0);
    } else {
#ifdef WIN32
        struct tm *tm_ = gmtime(&t);
        memmove(&tm, tm_, sizeof(struct tm));
#else
        gmtime_r(&t, &tm);
#endif
    }

    struct tm tm_x = {0};
    tm_x.tm_year = tm.tm_year;
    tm_x.tm_mon = tm.tm_mon;
    tm_x.tm_mday = tm.tm_mday;
    tm_x.tm_hour = tm.tm_hour;

    time_range.start = mktime(&tm_x);
    tm_x.tm_hour += range;
    time_range.end = mktime(&tm_x);

    return time_range;
}

/***************************************************************************
 *  Return in gmt time range in days of time t, TZ optional
 ***************************************************************************/
PUBLIC time_range_t get_days_range(time_t t, int range, const char *TZ)
{
    time_range_t time_range;

    struct tm tm;
    if(!empty_string(TZ)) {
        gmtime2timezone(t, TZ, &tm, 0);
    } else {
#ifdef WIN32
        struct tm *tm_ = gmtime(&t);
        memmove(&tm, tm_, sizeof(struct tm));
#else
        gmtime_r(&t, &tm);
#endif
    }

    struct tm tm_x = {0};
    tm_x.tm_year = tm.tm_year;
    tm_x.tm_mon = tm.tm_mon;
    tm_x.tm_mday = tm.tm_mday;

    time_range.start = mktime(&tm_x);
    tm_x.tm_mday += range;
    time_range.end = mktime(&tm_x);

    return time_range;
}

/***************************************************************************
 *  Return in gmt time range in weeks of time t, TZ optional
 ***************************************************************************/
PUBLIC time_range_t get_weeks_range(time_t t, int range, const char *TZ)
{
    time_range_t time_range;

    struct tm tm;
    if(!empty_string(TZ)) {
        gmtime2timezone(t, TZ, &tm, 0);

    } else {
#ifdef WIN32
        struct tm *tm_ = gmtime(&t);
        memmove(&tm, tm_, sizeof(struct tm));
#else
        gmtime_r(&t, &tm);
#endif
    }

    int day = tm.tm_mday;
    int wday= tm.tm_wday;
    if(wday == 1) {
        // Estamos en lunes
        day = tm.tm_mday;
    } else if(wday == 0) {
        // Domingo
        day += -6;
    } else { //if(wday > 1) {
        // Dentro de la semana
        day += (1 - wday);
    }

    struct tm tm_x = {0};
    tm_x.tm_year = tm.tm_year;
    tm_x.tm_mon = tm.tm_mon;
    tm_x.tm_mday = day;

    time_range.start = mktime(&tm_x);
    tm_x.tm_mday += range*7;
    time_range.end = mktime(&tm_x);

    return time_range;
}

/***************************************************************************
 *  Return in gmt time range in month of time t, TZ optional
 ***************************************************************************/
PUBLIC time_range_t get_months_range(time_t t, int range, const char *TZ)
{
    time_range_t time_range;

    struct tm tm;
    if(!empty_string(TZ)) {
        gmtime2timezone(t, TZ, &tm, 0);
    } else {
#ifdef WIN32
        struct tm *tm_ = gmtime(&t);
        memmove(&tm, tm_, sizeof(struct tm));
#else
        gmtime_r(&t, &tm);
#endif
    }

    struct tm tm_x = {0};
    tm_x.tm_year = tm.tm_year;
    tm_x.tm_mon = tm.tm_mon;
    tm_x.tm_mday = 1;

    time_range.start = mktime(&tm_x);
    tm_x.tm_mon += range;
    time_range.end = mktime(&tm_x);

    return time_range;
}

/***************************************************************************
 *  Return in gmt time range in year of time t, TZ optional
 ***************************************************************************/
PUBLIC time_range_t get_years_range(time_t t, int range, const char *TZ)
{
    time_range_t time_range;

    struct tm tm;
    if(!empty_string(TZ)) {
        gmtime2timezone(t, TZ, &tm, 0);
    } else {
#ifdef WIN32
        struct tm *tm_ = gmtime(&t);
        memmove(&tm, tm_, sizeof(struct tm));
#else
        gmtime_r(&t, &tm);
#endif
    }

    struct tm tm_x = {0};
    tm_x.tm_year = tm.tm_year;
    tm_x.tm_mday = 1;

    time_range.start = mktime(&tm_x);
    tm_x.tm_year += range;
    time_range.end = mktime(&tm_x);

    return time_range;
}




                    /*------------------------------------*
                     *      Hashing
                     *------------------------------------*/




/***************************************************************************
 *  sha256, FIPS 180-4, one block at a time. Standalone on purpose: the
 *  persistence layer hashes the files it stores and links no TLS backend.
 ***************************************************************************/
PRIVATE const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define SHA256_ROTR(x, n)   (((x) >> (n)) | ((x) << (32 - (n))))

PRIVATE void sha256_block(uint32_t h[8], const uint8_t block[64])
{
    uint32_t w[64];
    for(int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | ((uint32_t)block[i*4+3]);
    }
    for(int i = 16; i < 64; i++) {
        uint32_t s0 = SHA256_ROTR(w[i-15], 7) ^ SHA256_ROTR(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = SHA256_ROTR(w[i-2], 17) ^ SHA256_ROTR(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

    for(int i = 0; i < 64; i++) {
        uint32_t S1 = SHA256_ROTR(e, 6) ^ SHA256_ROTR(e, 11) ^ SHA256_ROTR(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + sha256_k[i] + w[i];
        uint32_t S0 = SHA256_ROTR(a, 2) ^ SHA256_ROTR(a, 13) ^ SHA256_ROTR(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        hh = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void sha256_init(sha256_ctx_t *ctx)
{
    static const uint32_t h0[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    memcpy(ctx->h, h0, sizeof(ctx->h));
    ctx->len = 0;
    ctx->used = 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void sha256_update(sha256_ctx_t *ctx, const void *data, size_t len)
{
    if(len == 0) {
        return;
    }
    const uint8_t *p = (const uint8_t *)data;
    ctx->len += len;

    if(ctx->used > 0) {
        size_t take = MIN(len, 64 - ctx->used);
        memcpy(ctx->block + ctx->used, p, take);
        ctx->used += take;
        p += take;
        len -= take;
        if(ctx->used < 64) {
            return;
        }
        sha256_block(ctx->h, ctx->block);
        ctx->used = 0;
    }
    while(len >= 64) {
        sha256_block(ctx->h, p);
        p += 64;
        len -= 64;
    }
    if(len > 0) {
        memcpy(ctx->block, p, len);
        ctx->used = len;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_LEN])
{
    uint8_t tail[128];
    memset(tail, 0, sizeof(tail));
    if(ctx->used > 0) {
        memcpy(tail, ctx->block, ctx->used);
    }
    tail[ctx->used] = 0x80;
    size_t tail_len = (ctx->used < 56)? 64: 128;
    uint64_t bit_len = ctx->len * 8;
    for(int i = 0; i < 8; i++) {
        tail[tail_len - 1 - i] = (uint8_t)(bit_len >> (8 * i));
    }
    sha256_block(ctx->h, tail);
    if(tail_len == 128) {
        sha256_block(ctx->h, tail + 64);
    }

    for(int i = 0; i < 8; i++) {
        digest[i*4]   = (uint8_t)(ctx->h[i] >> 24);
        digest[i*4+1] = (uint8_t)(ctx->h[i] >> 16);
        digest[i*4+2] = (uint8_t)(ctx->h[i] >> 8);
        digest[i*4+3] = (uint8_t)(ctx->h[i]);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void sha256_digest(const void *data, size_t len, uint8_t digest[SHA256_DIGEST_LEN])
{
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int check_sha256_hex_buffer(char *bf, size_t bflen)
{
    if(!bf || bflen < SHA256_HEX_LEN + 1) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "buffer too small for a sha256",
            "bflen",        "%d", (int)bflen,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  64 lowercase hex chars and the nul
 ***************************************************************************/
PRIVATE void sha256_digest_to_hex(const uint8_t digest[SHA256_DIGEST_LEN], char *bf)
{
    static const char hex[] = "0123456789abcdef";
    for(int i = 0; i < SHA256_DIGEST_LEN; i++) {
        bf[i*2] = hex[digest[i] >> 4];
        bf[i*2+1] = hex[digest[i] & 0x0f];
    }
    bf[SHA256_HEX_LEN] = 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int sha256_hex(const void *data, size_t len, char *bf, size_t bflen)
{
    if(check_sha256_hex_buffer(bf, bflen) < 0) {
        return -1;  // Error already logged
    }
    uint8_t digest[SHA256_DIGEST_LEN];
    sha256_digest(data, len, digest);
    sha256_digest_to_hex(digest, bf);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int sha256_final_hex(sha256_ctx_t *ctx, char *bf, size_t bflen)
{
    if(check_sha256_hex_buffer(bf, bflen) < 0) {
        return -1;  // Error already logged
    }
    uint8_t digest[SHA256_DIGEST_LEN];
    sha256_final(ctx, digest);
    sha256_digest_to_hex(digest, bf);
    return 0;
}
