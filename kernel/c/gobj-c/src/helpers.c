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
#include <unistd.h>
#include <sys/file.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <syslog.h>

#ifdef __linux__
#include <openssl/rand.h>
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

#ifdef ESP_PLATFORM
    #include <esp_log.h>
    #define O_LARGEFILE 0
    #define fstat64 fstat
    #define stat64 stat
    #define syslog(priority, format, ... ) ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, "yuneta", format, ##__VA_ARGS__)
#endif

#include "kwid.h"
#include "ansi_escape_codes.h"
#include "helpers.h"

/***************************************************
 *              Structures
 **************************************************/
#define _

#ifdef __linux__

#define MAX_COMM_LEN    128
#define MAX_CMDLINE_LEN 128

#define F_NO_PID_IO 0x01
#define F_NO_PID_FD 0x02

#define STAT            "/proc/stat"
#define UPTIME          "/proc/uptime"
#define DISKSTATS       "/proc/diskstats"
#define INTERRUPTS      "/proc/interrupts"
#define MEMINFO         "/proc/meminfo"

#define PID_STAT    "/proc/%u/stat"
#define PID_STATUS  "/proc/%u/status"
#define PID_IO      "/proc/%u/io"
#define PID_CMDLINE "/proc/%u/cmdline"
#define PID_SMAP    "/proc/%u/smaps"
#define PID_FD      "/proc/%u/fd"

#define PROC_TASK   "/proc/%u/task"
#define TASK_STAT   "/proc/%u/task/%u/stat"
#define TASK_STATUS "/proc/%u/task/%u/status"
#define TASK_IO     "/proc/%u/task/%u/io"
#define TASK_CMDLINE    "/proc/%u/task/%u/cmdline"
#define TASK_SMAP   "/proc/%u/task/%u/smaps"
#define TASK_FD     "/proc/%u/task/%u/fd"

/*
 * kB <-> number of pages.
 * Page size depends on machine architecture (4 kB, 8 kB, 16 kB, 64 kB...)
 */
// #define KB_TO_PG(k) ((k) >> kb_shift)
#define PG_TO_KB(k) ((k) << get_kb_shift())

/* Structure for memory and swap space utilization statistics */
struct stats_memory {
    unsigned long frmkb __attribute__ ((aligned (8)));
    unsigned long bufkb __attribute__ ((aligned (8)));
    unsigned long camkb __attribute__ ((aligned (8)));
    unsigned long tlmkb __attribute__ ((aligned (8)));
    unsigned long frskb __attribute__ ((aligned (8)));
    unsigned long tlskb __attribute__ ((aligned (8)));
    unsigned long caskb __attribute__ ((aligned (8)));
    unsigned long comkb __attribute__ ((aligned (8)));
    unsigned long activekb  __attribute__ ((aligned (8)));
    unsigned long inactkb   __attribute__ ((aligned (8)));
    unsigned long dirtykb   __attribute__ ((aligned (8)));
    unsigned long anonpgkb  __attribute__ ((aligned (8)));
    unsigned long slabkb    __attribute__ ((aligned (8)));
    unsigned long kstackkb  __attribute__ ((aligned (8)));
    unsigned long pgtblkb   __attribute__ ((aligned (8)));
    unsigned long vmusedkb  __attribute__ ((aligned (8)));
};

#define STATS_MEMORY_SIZE   (sizeof(struct stats_memory))

struct pid_stats {
    unsigned long long read_bytes           __attribute__ ((aligned (16)));
    unsigned long long write_bytes          __attribute__ ((packed));
    unsigned long long cancelled_write_bytes    __attribute__ ((packed));
    unsigned long long total_vsz            __attribute__ ((packed));
    unsigned long long total_rss            __attribute__ ((packed));
    unsigned long long total_stack_size     __attribute__ ((packed));
    unsigned long long total_stack_ref      __attribute__ ((packed));
    unsigned long long total_threads        __attribute__ ((packed));
    unsigned long long total_fd_nr          __attribute__ ((packed));
    unsigned long long blkio_swapin_delays      __attribute__ ((packed));
    unsigned long long minflt           __attribute__ ((packed));
    unsigned long long cminflt          __attribute__ ((packed));
    unsigned long long majflt           __attribute__ ((packed));
    unsigned long long cmajflt          __attribute__ ((packed));
    unsigned long long utime            __attribute__ ((packed));
    long long          cutime           __attribute__ ((packed));
    unsigned long long stime            __attribute__ ((packed));
    long long          cstime           __attribute__ ((packed));
    unsigned long long gtime            __attribute__ ((packed));
    long long          cgtime           __attribute__ ((packed));
    unsigned long long vsz              __attribute__ ((packed));
    unsigned long long rss              __attribute__ ((packed));
    unsigned long      nvcsw            __attribute__ ((packed));
    unsigned long      nivcsw           __attribute__ ((packed));
    unsigned long      stack_size           __attribute__ ((packed));
    unsigned long      stack_ref            __attribute__ ((packed));
    /* If pid is null, the process has terminated */
    unsigned int       pid              __attribute__ ((packed));
    /* If tgid is not null, then this PID is in fact a TID */
    unsigned int       tgid             __attribute__ ((packed));
    unsigned int       rt_asum_count        __attribute__ ((packed));
    unsigned int       rc_asum_count        __attribute__ ((packed));
    unsigned int       uc_asum_count        __attribute__ ((packed));
    unsigned int       tf_asum_count        __attribute__ ((packed));
    unsigned int       sk_asum_count        __attribute__ ((packed));
    unsigned int       delay_asum_count     __attribute__ ((packed));
    unsigned int       processor            __attribute__ ((packed));
    unsigned int       priority         __attribute__ ((packed));
    unsigned int       policy           __attribute__ ((packed));
    unsigned int       flags            __attribute__ ((packed));
    unsigned int       uid              __attribute__ ((packed));
    unsigned int       threads          __attribute__ ((packed));
    unsigned int       fd_nr            __attribute__ ((packed));
    char               comm[MAX_COMM_LEN];
    char               cmdline[MAX_CMDLINE_LEN];
};

#define PID_STATS_SIZE  (sizeof(struct pid_stats))

/*
 * Structure for CPU statistics.
 * In activity buffer: First structure is for global CPU utilisation ("all").
 * Following structures are for each individual CPU (0, 1, etc.)
 */
struct stats_cpu {
    unsigned long long cpu_user     __attribute__ ((aligned (16)));
    unsigned long long cpu_nice     __attribute__ ((aligned (16)));
    unsigned long long cpu_sys      __attribute__ ((aligned (16)));
    unsigned long long cpu_idle     __attribute__ ((aligned (16)));
    unsigned long long cpu_iowait       __attribute__ ((aligned (16)));
    unsigned long long cpu_steal        __attribute__ ((aligned (16)));
    unsigned long long cpu_hardirq      __attribute__ ((aligned (16)));
    unsigned long long cpu_softirq      __attribute__ ((aligned (16)));
    unsigned long long cpu_guest        __attribute__ ((aligned (16)));
    unsigned long long cpu_guest_nice   __attribute__ ((aligned (16)));
};

#define STATS_CPU_SIZE  (sizeof(struct stats_cpu))

/*
 * Structure for interrupts statistics.
 * In activity buffer: First structure is for total number of interrupts ("SUM").
 * Following structures are for each individual interrupt (0, 1, etc.)
 *
 * NOTE: The total number of interrupts is saved as a %llu by the kernel,
 * whereas individual interrupts are saved as %u.
 */
struct stats_irq {
    unsigned long long irq_nr   __attribute__ ((aligned (16)));
};

#define STATS_IRQ_SIZE  (sizeof(struct stats_irq))

#endif

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
static BOOL umask_cleared = FALSE;
static char _node_uuid[64] = {0}; // uuid of the node




                    /*------------------------------------*
                     *      Directory/Files
                     *------------------------------------*/




/***************************************************************************
 *  Create a new directory
 *  The use of this functions implies the use of 00_security.h's permission system:
 *  umask will be set to 0 and we control all permission mode.
 ***************************************************************************/
PUBLIC int newdir(const char *path, int permission)
{
    if(!umask_cleared) {
        umask(0);
        umask_cleared = TRUE;
    }
    return mkdir(path, permission);
}

/***************************************************************************
 *  Create a new file (only to write)
 *  The use of this functions implies the use of 00_security.h's permission system:
 *  umask will be set to 0 and we control all permission mode.
 ***************************************************************************/
PUBLIC int newfile(const char *path, int permission, BOOL overwrite)
{
    int flags = O_CREAT|O_WRONLY|O_LARGEFILE;

    if(!umask_cleared) {
        umask(0);
        umask_cleared = TRUE;
    }

    if(overwrite)
        flags |= O_TRUNC;
    else
        flags |= O_EXCL;

    return open(path, flags, permission);
}

/***************************************************************************
 *  Open a file as exclusive
 ***************************************************************************/
PUBLIC int open_exclusive(const char *path, int flags, int permission)
{
    if(!flags) {
        flags = O_RDWR|O_LARGEFILE|O_NOFOLLOW;
    }

    int fp = open(path, flags, permission);
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
PUBLIC int mkrdir(const char *path, int permission)
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
                if(newdir(tmp, permission)<0) {
                    if(errno != EEXIST) {
                        gobj_log_error(0, 0,
                           "function",     "%s", __FUNCTION__,
                           "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                           "msg",          "%s", "newdir() FAILED",
                           "path",         "%s", tmp,
                           "errno",        "%s", strerror(errno),
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
                   "errno",        "%s", strerror(errno),
                   NULL
                );
                return -1;
            }
            *p = '/';
        }
    }

    // Create the final directory component
    if(access(tmp, F_OK) != 0) {
        if(newdir(tmp, permission)<0) {
            if(errno != EEXIST) {
                gobj_log_error(0, 0,
                   "function",     "%s", __FUNCTION__,
                   "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                   "msg",          "%s", "newdir() FAILED",
                   "path",         "%s", tmp,
                   "errno",        "%s", strerror(errno),
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
           "errno",        "%s", strerror(errno),
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
                "errno",        "%s", strerror(errno),
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
                "msg",          "%s", "opendir() FAILED",
                "errno",        "%s", strerror(errno),
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
                "errno",        "%s", strerror(errno),
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
                     *          Strings
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
PUBLIC char *pop_last_segment(char *path) // WARNING path modified
{
    char *p = strrchr(path, '/');
    if(!p) {
        return path;
    }
    *p = 0;
    return p+1;
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
    char *buffer = GBMEM_STRDUP(str);
    if(!buffer) {
        return 0;
    }

    // Get list size
    int list_size = 0;
    for (ptr = strtok(buffer, delim); ptr != NULL; ptr = strtok(NULL, delim)) {
        list_size++;
    }
    GBMEM_FREE(buffer)

    buffer = GBMEM_STRDUP(str);   // Prev buffer is destroyed!
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
            list[i++] = GBMEM_STRDUP(ptr);
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
    char *buffer = GBMEM_STRDUP(str);
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

    buffer = GBMEM_STRDUP(str);   // Prev buffer is destroyed!
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
            list[i] = GBMEM_STRDUP(ptr);
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
                     *          Json
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
                "errno",        "%s", strerror(errno),
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
            "errno",        "%s", strerror(errno),
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
                "errno",        "%s", strerror(errno),
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
            "errno",        "%s", strerror(errno),
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
                if(strcasecmp(defaults, "true")==0) {
                    json_object_set_new(jn, name, json_true());
                } else if(strcasecmp(defaults, "false")==0) {
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
PUBLIC json_t *json_record_to_schema(const json_desc_t *json_desc)
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

    return GBMEM_STRDUP(s);
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
     *  Discard complex types, done as matched
     */
    if(json_is_object(jn_var1) ||
            json_is_object(jn_var2) ||
            json_is_array(jn_var1) ||
            json_is_array(jn_var2)) {
        return 0;
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
    if(!kw1 || !kw2) {
        return FALSE;
    }
    char *kw1_ = json2uglystr(kw1);
    char *kw2_ = json2uglystr(kw2);
    int ret = strcmp(kw1_, kw2_);
    GBMEM_FREE(kw1_)
    GBMEM_FREE(kw2_)
    return ret==0?TRUE:FALSE;
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




                    /*---------------------------------*
                     *      Walkdir functions
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
                "msg",          "%s", "opendir() FAILED",
                "path",         "%s", root_dir,
                "error",        "%d", errno,
                "strerror",     "%s", strerror(errno),
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
                    "strerror",     "%s", strerror(errno),
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

    if(access(root_dir, 0)!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "directory not found",
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

/****************************************************************************
 *  Get number of files
 ****************************************************************************/
PRIVATE BOOL _nfiles_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *filename, // dname[256]
    int level,
    wd_option opt)
{
    int *nfiles = user_data;
    (*nfiles)++;
    return TRUE; // continue traverse tree
}

PUBLIC int get_number_of_files(
    hgobj gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt)
{
    regex_t r;

    if(access(root_dir, 0)!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "directory not found",
            "path",         "%s", root_dir,
           NULL
        );
        return 0;
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
    int nfiles = 0;
    _walk_tree(gobj, root_dir, &r, &nfiles, opt, 0, _nfiles_cb);
    regfree(&r);
    return nfiles;
}

/****************************************************************************
 *  Compare function to sort
 ****************************************************************************/
PRIVATE int cmpstringp(const void *p1, const void *p2) {
/*  The actual arguments to this function are "pointers to
 *  pointers to char", but strcmp(3) arguments are "pointers
 *  to char", hence the following cast plus dereference
 */
    return strcmp(* (char * const *) p1, * (char * const *) p2);
}

/****************************************************************************
 *  Return the ordered full tree filenames of root_dir
 *  WARNING free return array with free_ordered_filename_array()
 *  WARNING: here I don't use gbmem functions
 ****************************************************************************/
struct myfiles_s {
    char **files;
    int *idx;
};
PRIVATE BOOL _fill_array_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *filename, // dname[256]
    int level,
    wd_option opt)
{
    struct myfiles_s *myfiles = user_data;
    char **files = myfiles->files;
    int idx = *(myfiles->idx);

    size_t ln;
    char *ptr;
    if(opt & WD_ONLY_NAMES) {
        ln = strlen(filename);
        ptr = GBMEM_MALLOC(ln+1);
        if(!ptr) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "malloc() FAILED",
                "error",        "%d", errno,
                "strerror",     "%s", strerror(errno),
               NULL
            );
            return FALSE; // don't continue traverse tree
        }
        memcpy(ptr, filename, ln);
        ptr[ln] = 0;

    } else {
        ln = strlen(fullpath);
        ptr = GBMEM_MALLOC(ln+1);
        if(!ptr) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "malloc() FAILED",
                "error",        "%d", errno,
                "strerror",     "%s", strerror(errno),
               NULL
            );
            return FALSE; // don't continue traverse tree
        }
        memcpy(ptr, fullpath, ln);
        ptr[ln] = 0;
    }

    *(files+idx) = ptr;
    (*myfiles->idx)++;
    return TRUE; // continue traversing tree
}
PUBLIC char **get_ordered_filename_array(
    hgobj gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    int *size)
{
    regex_t r;
    if(size) {
        *size = 0; // initial 0
    }

    if(access(root_dir, 0)!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "directory not found",
            "path",         "%s", root_dir,
           NULL
        );
        return 0;
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
        return 0;
    }
    /*--------------------------------------------------------*
     *  Order required, do some preprocessing:
     *  - get nfiles: number of files
     *  - alloc *system* memory for array of nfiles pointers
     *  - fill array with file names
     *  - order the array
     *  - return the array
     *  - the user must free with free_ordered_filename_array()
     *--------------------------------------------------------*/
    int nfiles = get_number_of_files(gobj, root_dir, pattern, opt);
    if(!nfiles) {
        regfree(&r);
        return 0;
    }

    int ln = sizeof(char *) * (nfiles+1);
    // TODO check if too much memory is required . Avoid use of swap memory.

    char **files = GBMEM_MALLOC(ln);
    if(!files) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "malloc() FAILED",
            "error",        "%d", errno,
            "strerror",     "%s", strerror(errno),
           NULL
        );
        regfree(&r);
        return 0;
    }
    memset(files, 0, ln);

    /*
     *  Fill the array
     */
    struct myfiles_s myfiles;
    int idx = 0;
    myfiles.files = files;
    myfiles.idx = &idx;

    _walk_tree(gobj, root_dir, &r, &myfiles, opt, 0, _fill_array_cb);
    regfree(&r);

    /*
     *  Order the array
     */
    qsort(files, nfiles, sizeof(char *), cmpstringp);

    if(size) {
        *size = nfiles;
    }
    return files;
}

/****************************************************************************
 *  WARNING: here I don't use gbmem functions
 ****************************************************************************/
PUBLIC void free_ordered_filename_array(char **array, int size)
{
    if(!array) {
        return;
    }
    for(int i=0; i<size; i++) {
        char *ptr = *(array+i);
        if(ptr) {
            GBMEM_FREE(ptr)
        }
    }
    GBMEM_FREE(array)
}

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
time_t tm_to_time_t(const struct tm *tm)
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
void show_date_relative(
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

struct date_mode *date_mode_from_type(enum date_mode_type type)
{
    static struct date_mode mode;
    if (type == DATE_STRFTIME)
        gobj_trace_msg(0, "cannot create anonymous strftime date_mode struct");
    mode.type = type;
    return &mode;
}

const char *show_date(timestamp_t tim, int tz, const struct date_mode *mode)
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
int parse_date_basic(const char *date, timestamp_t *timestamp, int *offset)
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

int parse_expiry_date(const char *date, timestamp_t *timestamp)
{
    int errors = 0;

    if (!strcmp(date, "never") || !strcmp(date, "false"))
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

int parse_date(const char *date, char *result, int resultsize)
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

void parse_date_format(const char *format, struct date_mode *mode)
{
    const char *p;

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

void datestamp(char *out, int outsize)
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

int date_overflows(timestamp_t t)
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
                     *      Utilities functions
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
static const char tbhexa[]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
static inline char * byte_to_strhex(char *s, char w)
{
    *s = tbhexa[ ((w >> 4) & 0x0f) ];
    s++;
    *s = tbhexa[ ((w) & 0x0f) ];
    s++;
    return s;
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
        _trace_json(inside_list?deep:0, verbose, "true");
    } else if(json_is_false(jn)) {
        _trace_json(inside_list?deep:0, verbose, "false");
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
        // No value no test true
        return FALSE;
    }
    time(&timer_actual);
    return (timer_actual>=value)? TRUE:FALSE;
}

/****************************************************************************
 *   Arranca un timer de 'miliseconds' mili-segundos.
 *   El valor retornado es el que hay que usar en la funcion test_msectimer()
 *   para ver si el timer ha cumplido.
 ****************************************************************************/
PUBLIC uint64_t start_msectimer(uint64_t miliseconds)
{
    uint64_t ms = time_in_miliseconds_monotonic();
    ms += miliseconds;
    return ms;
}

/****************************************************************************
 *   Retorna TRUE si ha cumplido el timer 'value', FALSE si no.
 ****************************************************************************/
PUBLIC BOOL test_msectimer(uint64_t value)
{
    if(value == 0) {
        // No value no test true
        return FALSE;
    }

    uint64_t ms = time_in_miliseconds_monotonic();

    return (ms>=value)? TRUE:FALSE;
}

/****************************************************************************
 *  Return MONOTONIC time in miliseconds
 ****************************************************************************/
PUBLIC uint64_t time_in_miliseconds_monotonic(void)
{
    struct timespec spec;

    clock_gettime(CLOCK_MONOTONIC, &spec); //Este no da el time from Epoch

    // Convert to milliseconds
    return ((uint64_t)spec.tv_sec)*1000 + ((uint64_t)spec.tv_nsec)/1000000;
}

/****************************************************************************
 *  Current **real** time in milliseconds
 ****************************************************************************/
PUBLIC uint64_t time_in_miliseconds(void)
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
        perror("opendir");
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
            fgets(hostname, sizeof(hostname), file);
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
 *  Create a random uuid
 ***************************************************************************/
PUBLIC int create_uuid(char *bf, int bfsize)
{
    if(bfsize > 0) {
        *bf = 0;
    }
    if(bfsize < 37) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "buffer TOO small",
            NULL
        );
        return -1;
    }

#ifdef __linux__
    unsigned char uuid[16];
    RAND_bytes(uuid, 16);       // depends of openssl

    // Set the version to 4 (randomly generated UUID)
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    // Set the variant to DCE 1.1
    uuid[8] = (uuid[8] & 0x3F) | 0x80;

    snprintf(bf, bfsize,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5],
        uuid[6], uuid[7],
        uuid[8], uuid[9],
        uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]
    );
#else
    #error "What S.O.?"
#endif

    return 0;
}

/***************************************************************************
 *  Node uuid
 ***************************************************************************/
#ifdef __linux__
PRIVATE void save_node_uuid(void)
{
    char *directory = "/yuneta/store/agent/uuid";
    json_t *jn_uuid = json_object();
    json_object_set_new(jn_uuid, "uuid", json_string(_node_uuid));

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
        "/yuneta/store/agent/uuid",
        "uuid.json",
        0
    );

    if(jn_uuid) {
        const char *uuid_ = kw_get_str(0, jn_uuid, "uuid", "", KW_REQUIRED);
        snprintf(_node_uuid, sizeof(_node_uuid), "%s", uuid_);
        json_decref(jn_uuid);
        return 0;
    }
    return -1;
}
#endif

/***************************************************************************
 *  Node uuid
 ***************************************************************************/
#ifdef __linux__
PRIVATE int create_node_uuid(void)
{
    read_node_uuid(); // get node uuid in _node_uuid
    if(!empty_string(_node_uuid)) {
        return 0;
    }

    // TODO improve, I use node_uuid to identify the machine, be depending of machine
    create_uuid(_node_uuid, sizeof(_node_uuid));
    save_node_uuid();
    return 0;
}
#endif

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char *node_uuid(void)
{
    if(!_node_uuid[0]) {
#ifdef ESP_PLATFORM
        uint8_t mac_addr[6] = {0};
        esp_efuse_mac_get_default(mac_addr);
        snprintf(uuid, sizeof(uuid), "ESP32-%02X-%02X-%02X-%02X-%02X-%02X",
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
            "efuse_mac",    "%s", uuid,
            "uuid",         "%s", uuid,
            NULL
        );
#elif defined(__linux__)
        create_node_uuid();
#else
    #error "What S.O.?"
#endif
    }
    return _node_uuid;
}
