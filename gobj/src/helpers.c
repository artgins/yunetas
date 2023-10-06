/****************************************************************************
 *              helpers.c
 *
 *              Several helpers
 *
 *              Copyright (c) 2014,2023 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#ifdef __linux__
    #include <fcntl.h>
    #include <syslog.h>
#endif

#ifdef ESP_PLATFORM
    #include <esp_log.h>
    #define O_LARGEFILE 0
    #define fstat64 fstat
    #define stat64 stat
    #define syslog(priority, format, ... ) ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, "yuneta", format, ##__VA_ARGS__)
#endif

#include "helpers.h"

/*****************************************************************
 *     Data
 *****************************************************************/
static BOOL umask_cleared = FALSE;




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
    struct stat64 st;
    int ret = stat64(path, &st);
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
    struct stat64 st;
    int ret = fstat64(fd, &st);
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
 *  Make recursive dirs
 *  index va apuntando los segmentos del path en temp
 ***************************************************************************/
PUBLIC int mkrdir(const char *path, int index, int permission)
{
    char bf[NAME_MAX];
    if(*path == '/')
        index++;
    char *p = strchr(path + index, '/');
    int len;
    if(p) {
        len = MIN((size_t)(p-path), sizeof(bf)-1);
        strncpy(bf, path, len);
        bf[len]=0;
    } else {
        len = MIN(strlen(path)+1, sizeof(bf)-1);
        strncpy(bf, path, len);
        bf[len]=0;
    }

    if(access(bf, 0)!=0) {
        if(newdir(bf, permission)<0) {
            if(errno != EEXIST) {
                gobj_log_error(0, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                    "msg",          "%s", "newdir() FAILED",
                    "path",         "%s", bf,
                    "errno",        "%s", strerror(errno),
                    NULL
                );
                return -1;
            }
        }
    }
    if(p) {
        // Have you got permissions to read next directory?
        return mkrdir(path, p-path+1, permission);
    }
    return 0;
}

/****************************************************************************
 *  Recursively remove a directory
 ****************************************************************************/
PUBLIC int rmrdir(const char *root_dir)
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
        char *path = malloc(strlen(root_dir) + strlen(dname) + 2);
        if(!path) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY_ERROR,
                "msg",          "%s", "no memory to path",
                NULL
            );
            closedir(dir);
            return -1;
        }
        strcpy(path, root_dir);
        strcat(path, "/");
        strcat(path, dname);

        if(stat(path, &st) == -1) {
            closedir(dir);
            free(path);
            return -1;
        }

        if(S_ISDIR(st.st_mode)) {
            /* recursively follow dirs */
            if(rmrdir(path)<0) {
                closedir(dir);
                free(path);
                return -1;
            }
        } else {
            if(unlink(path) < 0) {
                closedir(dir);
                free(path);
                return -1;
            }
        }
        free(path);
    }
    closedir(dir);
    if(rmdir(root_dir) < 0) {
        return -1;
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
        char *path = malloc(strlen(root_dir) + strlen(dname) + 2);
        if(!path) {
            closedir(dir);
            return -1;
        }
        strcpy(path, root_dir);
        strcat(path, "/");
        strcat(path, dname);

        if(stat(path, &st) == -1) {
            closedir(dir);
            free(path);
            return -1;
        }

        if(S_ISDIR(st.st_mode)) {
            /* recursively follow dirs */
            if(rmrdir(path)<0) {
                closedir(dir);
                free(path);
                return -1;
            }
        } else {
            if(unlink(path) < 0) {
                closedir(dir);
                free(path);
                return -1;
            }
        }
        free(path);
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
 *
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
PUBLIC json_t *bits2jn_strlist(
    const char **string_table,
    uint64_t bits
) {
    json_t *jn_list = json_array();

    for(uint64_t i=0; string_table[i]!=NULL && i<sizeof(i); i++) {
        uint64_t bitmask = 1 << i;
        if (bits & bitmask) {
            json_array_append_new(jn_list, json_string(string_table[i]));
        }
    }

    return jn_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC gbuffer_t *bits2gbuffer(
    const char **string_table,
    uint64_t bits
) {
    gbuffer_t *gbuf = gbuffer_create(256, 1024);

    if(!gbuf) {
        return 0;
    }
    BOOL add_sep = FALSE;

    for(uint64_t i=0; string_table[i]!=NULL && i<sizeof(i); i++) {
        uint64_t bitmask = 1 << i;
        if (bits & bitmask) {
            if(add_sep) {
                gbuffer_append(gbuf, "|", 1);
            }
            gbuffer_append_string(gbuf, string_table[i]);
            add_sep = TRUE;
        }
    }

    return gbuf;
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
                view("%s%04X: %s  %s\n", prefix, i-15, bf, asci);
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
            view("%s%04X: %s  %s\n", prefix, i - j, bf, asci);
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

    size_t flags = JSON_INDENT(2)|JSON_ENCODE_ANY;
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
    uint64_t ms = time_in_miliseconds();
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

    uint64_t ms = time_in_miliseconds();

    return (ms>value)? TRUE:FALSE;
}

/****************************************************************************
 *  Current time in milisecons
 ****************************************************************************/
PUBLIC uint64_t time_in_miliseconds(void)
{
    struct timespec spec;

    //clock_gettime(CLOCK_MONOTONIC, &spec); //Este no da el time from Epoch
    clock_gettime(CLOCK_REALTIME, &spec);

    // Convert to milliseconds
    return ((uint64_t)spec.tv_sec)*1000 + ((uint64_t)spec.tv_nsec)/1000000;
}

/***************************************************************************
 *  Return current time in seconds (standart time(&t))
 ***************************************************************************/
PUBLIC uint64_t time_in_seconds(void)
{
    time_t __t__;
    time(&__t__);
    return (uint64_t) __t__;
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
 *  Prints to the provided buffer a nice number of bytes (KB, MB, GB, etc)
 *  https://www.mbeckler.org/blog/?p=114
 ***************************************************************************/
PUBLIC void nice_size(char *bf, size_t bfsize, uint64_t bytes)
{
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
    while (count >= 1000 && s < 7)
    {
        s++;
        count /= 1000;
    }
    if (count - floor(count) == 0.0)
        snprintf(bf, bfsize, "%d %s", (int)count, suffixes[s]);
    else
        snprintf(bf, bfsize, "%.1f %s", count, suffixes[s]);
}

/***************************************************************************
 *  Print a byte count in a human readable format
 *  https://programming.guide/worlds-most-copied-so-snippet.html
 ***************************************************************************/
PUBLIC void nice_size2(char *bf, size_t bfsize, size_t bytes, BOOL si)
{
    size_t unit = si ? 1000 : 1024;
    size_t absBytes = bytes;

    if (absBytes < unit) {
        snprintf(bf, bfsize, "%zu B", bytes);
        return; // bytes + " B";
    }

    int exp = (int) (log((double)absBytes) / log((double)unit));
    size_t th = (size_t) ceil(pow((double)unit, exp) * ((double)unit - 0.05));

    if (exp < 6 && absBytes >= th - ((th & 0xFFF) == 0xD00 ? 51 : 0)) {
        exp++;
    }

    char *sufijos = (si ? "kMGTPE" : "KMGTPE");
    char pre = sufijos[exp - 1];

    if (exp > 4) {
        bytes /= unit;
        exp -= 1;
    }
    snprintf(bf, bfsize, "%.1f %c%sB", (double)bytes / pow((double)unit, exp), pre, (si ? "" : "i"));
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
    GBMEM_FREE(val1);
    GBMEM_FREE(val2);
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
    int max_items = 0;

    if(plist_size) {
        max_items = *plist_size;
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
    GBMEM_FREE(buffer);

    buffer = GBMEM_STRDUP(str);   // Prev buffer is destroyed!
    if(!buffer) {
        return 0;
    }

    // Limit list
    if(max_items > 0) {
        list_size = MIN(max_items, list_size);
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
    GBMEM_FREE(buffer);

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
            GBMEM_FREE(*p);
            *p = 0;
            p++;
        }
        GBMEM_FREE(list);
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
    int max_items = 0;

    if(plist_size) {
        max_items = *plist_size;
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
    GBMEM_FREE(buffer);

    // Limit list
    if(max_items > 0) {
        list_size = MIN(max_items, list_size);
    }

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
    GBMEM_FREE(buffer);

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
            GBMEM_FREE(*p);
            *p = 0;
            p++;
        }
        GBMEM_FREE(list);
    }
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
