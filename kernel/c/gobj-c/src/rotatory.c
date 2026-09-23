/****************************************************************************
 *              ROTATORY.C
 *              Log by week's days or or month's days or year's days
 *              Copyright (c) 1996-2014 Niyamaka.
 *              Copyright (c) 2026, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE  /* get gnu version of basename() */
#endif

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <libgen.h>
#include <dirent.h>
#ifdef __linux__
#include <sys/statvfs.h>
#endif
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#include "helpers.h"
#include "rotatory.h"

/*****************************************************************
 *          Constants
 *****************************************************************/
#define MAX_COUNTER_STATVFS     100
#define DEFAULT_BUFFER_SIZE     (64*1024)
#define DATE_MASK               "DD/MM/CCYY-W-ZZZ"  // the mask of formatdate()
#define DIGIT_MARK              '\001'
#define OLD_SUFFIX              ".OLD"

/*****************************************************************
 *          Structures
 *****************************************************************/
typedef struct rotatory_log_s {
    DL_ITEM_FIELDS
    char path[2*NAME_MAX+1];
    size_t buffer_size;
    uint64_t max_megas_rotatoryfile_size;
    size_t min_free_disk_percentage;
    int xpermission;            // permission for directories and executable files.
    int rpermission;            // permission for regular files.
    pe_flag_t pe_flag;          // Exit if cannot create rotatory file

    uint16_t counter_statvfs;
    char log_directory[NAME_MAX];   // from path
    char filenamemask[NAME_MAX];    // from path
    char filename[NAME_MAX];        // current filename
    time_t day_start;               // the name of the file is valid while the time
    time_t next_day_start;          //   is in [day_start, next_day_start): one local day
    int (*cb_newfile)(void *user_data, const char *old_filename, const char *new_filename);
    void *user_data;
    FILE *flog;
    char *buffer;

} rotatory_log_t;

/*****************************************************************
 *          Data
 *****************************************************************/
PRIVATE char __initialized__ = 0;
PRIVATE char disk_full_informed = 0;
PRIVATE int atexit_registered = 0; /* Register atexit just 1 time. */
PRIVATE dl_list_t dl_clients;
PRIVATE const char *priority_names[]={
    "EMERG",
    "ALERT",
    "CRITICAL",
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG",
    "STATS",
    "MONITOR",
    "AUDIT",
    0
};


/*****************************************************************
 *          Prototypes
 *****************************************************************/
PRIVATE void _rotatory_truncate(rotatory_log_t *rotatory_log);
PRIVATE void _rotatory_flush(rotatory_log_t* hr);
PRIVATE BOOL _get_rotatory_filename(rotatory_log_t *rotatory_log);
PRIVATE int _rotatory_prepare(rotatory_log_t *hr);
PRIVATE int _rotatory_fwrite(rotatory_log_t *hr, const char *bf, size_t len);
PRIVATE int _translate_mask(rotatory_log_t *hr);


/*****************************************************************
 *
 *****************************************************************/
PUBLIC int rotatory_start_up(void)
{
    if(__initialized__) {
        return -1;
    }
    if (!atexit_registered) {
        atexit(rotatory_end);
        atexit_registered = 1;
    }

    dl_init(&dl_clients, 0);
    __initialized__ = TRUE;
    return 0;
}

/*****************************************************************
 *  Close all
 *****************************************************************/
PUBLIC void rotatory_end(void)
{
    rotatory_log_t *hr;

    while((hr=dl_first(&dl_clients))) {
        rotatory_close(hr);
    }
    __initialized__ = FALSE;
}

/*****************************************************************
 *  Return NULL on error
 *****************************************************************/
PUBLIC hrotatory_h rotatory_open(
    const char* path,
    size_t bf_size,
    size_t max_megas_rotatoryfile_size,
    size_t min_free_disk_percentage,
    int xpermission,
    int rpermission,
    BOOL exit_on_fail)
{
    rotatory_log_t *hr = 0;

    if(!__initialized__) {
        print_error(
            PEF_SYSLOG,
            "rotatory_open(): rotatory not initialized"
        );
        return 0;
    }

    /*-------------------------------------*
     *          Check parameters
     *-------------------------------------*/
    if(!path || !*path) {
        print_error(
            PEF_SYSLOG,
            "path EMPTY"
        );
        return 0;
    }

    if(bf_size <=0) {
        bf_size = DEFAULT_BUFFER_SIZE;
    }
    if(max_megas_rotatoryfile_size <= 0) {
        max_megas_rotatoryfile_size = 8;
    }
    if(min_free_disk_percentage <= 0) {
        min_free_disk_percentage = 10;
    }
    if(!xpermission) {
        xpermission = 02775;
    }
    if(!rpermission) {
        rpermission = 0664;
    }

    /*-------------------------------------*
     *          Alloc memory
     *
     *  HACK use system memory, not gbmem_*, as glogger does for its
     *  handlers: a rotatory is the sink of the file log handler, and it
     *  must live until the end. The leak report of gbmem
     *  (print_track_mem(), after gobj_end()) is written THROUGH it, so it
     *  cannot be one of the blocks that report counts, and it cannot be
     *  freed before it (see yuneta_entry_point(): rotatory_end() goes
     *  after print_track_mem()).
     *-------------------------------------*/
    hr = calloc(1, sizeof(rotatory_log_t));
    if(!hr) {
        print_error(
            PEF_ABORT,
            "rotatory_open(): No MEMORY for %d",
            (int)sizeof(rotatory_log_t)
        );
        return 0;
    }
    strncpy(hr->path, path, sizeof(hr->path) -1);
    hr->buffer_size = bf_size;
    hr->max_megas_rotatoryfile_size = max_megas_rotatoryfile_size;
    hr->min_free_disk_percentage = min_free_disk_percentage;
    hr->xpermission = xpermission;
    hr->rpermission = rpermission;
    hr->pe_flag = exit_on_fail?PEF_EXIT:PEF_SYSLOG;

    hr->buffer = calloc(1, hr->buffer_size);
    if(!hr->buffer) {
        print_error(
            PEF_ABORT,
            "rotatory_open(): No MEMORY for %d",
            (int)bf_size
        );
        free(hr);
        return 0;
    }

    /*------------------------------------------------*
     *  Split path in log_directory and filenamemask
     *------------------------------------------------*/
    char *filenamemask = "";
    char *log_directory = "";

#ifdef WIN32
    char drive[_MAX_DRIVE] = {0};
    char dir[_MAX_DIR] = {0};
    char fname[_MAX_FNAME] = {0};
    char ext[_MAX_EXT] = {0};
    _splitpath( hr->path, drive, dir, fname, ext );
    filenamemask = fname;
    log_directory = dir;
#else
    filenamemask = basename(hr->path);
    strncpy(hr->filenamemask, filenamemask, sizeof(hr->filenamemask) -1);
    log_directory = dirname(hr->path);
    strncpy(hr->log_directory, log_directory, sizeof(hr->log_directory) -1);
#endif

    /*-----------------------------*
     *  Create the log directory
     *-----------------------------*/
    if(access(hr->log_directory, 0)!=0) {
        if(mkrdir(hr->log_directory, hr->xpermission)<0) {
            print_error(
                hr->pe_flag,
                "rotatory_open(): Cannot create '%s' directory, %s",
                hr->log_directory,
                strerror(errno)
            );
            rotatory_close(hr);
            return 0;
        }
    }
    /*-----------------------------*
     *  Make initial filename
     *-----------------------------*/
    _translate_mask(hr);
    snprintf(hr->path, sizeof(hr->path), "%s/%s", hr->log_directory, hr->filename);
    if(access(hr->path, 0)!=0) {
        int fd = newfile(hr->path, hr->rpermission, FALSE);
        if(fd < 0) {
            print_error(
                hr->pe_flag,
                "rotatory_open(): Cannot create '%s' file, %s",
                hr->path,
                strerror(errno)
            );
            rotatory_close(hr);
            return 0;
        }
        close(fd);
    }
    hr->flog = fopen(hr->path, "a");
    if(!hr->flog) {
        print_error(
            hr->pe_flag,
            "rotatory_open(): Cannot open '%s' file, %s",
            hr->path,
            strerror(errno)
        );
        rotatory_close(hr);
        return 0;
    }

    int fd = fileno(hr->flog);
    set_cloexec(fd);

    /*-------------------------------------*
     *  Add to list and create rotatory
     *-------------------------------------*/
    dl_add(&dl_clients, hr);

    return hr;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC void rotatory_close(hrotatory_h hr_)
{
    rotatory_log_t *hr = hr_;

    if(!__initialized__ || !hr) {
        return;
    }

    if(hr->flog) {
        _rotatory_flush(hr);
        fclose(hr->flog);
        hr->flog = 0;
    }
    if(dl_find(&dl_clients, hr)) {
        dl_delete(&dl_clients, hr, 0);
    }
    free(hr->buffer);   // System memory, see rotatory_open()
    free(hr);
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int rotatory_subscribe2newfile(
    hrotatory_h hr_,
    int (*cb_newfile)(void *user_data, const char *old_filename, const char *new_filename),
    void *user_data)
{
    rotatory_log_t *hr = hr_;
    hr->cb_newfile = cb_newfile;
    hr->user_data = user_data;

    return 0;
}

/*****************************************************************
 *   Return bytes written
 *****************************************************************/
PUBLIC int rotatory_write(hrotatory_h hr_, int priority, const char* bf, size_t len)
{
    rotatory_log_t *hr = hr_;

    if(!hr || !bf) {
        return -1;
    }
    if(priority < 0 || priority > LOG_AUDIT) {
        priority = LOG_DEBUG;
    }

    /*
     *  The file is checked (name, size, still there) once for the record,
     *  never between its pieces: a record is not split between two files.
     */
    if(_rotatory_prepare(hr) < 0) {
        return 0;   // Nothing written: disk full, or the file cannot be opened
    }

    if(priority == LOG_AUDIT) {
        // without header
        _rotatory_fwrite(hr, bf, len);
    } else {
        char spriority[64];
        snprintf(
            spriority,
            sizeof(spriority),
            "%s: ",
            priority_names[priority]
        );
        _rotatory_fwrite(hr, spriority, strlen(spriority));
        _rotatory_fwrite(hr, bf, len);
    }
    #define END_LOG "\n"
    _rotatory_fwrite(hr, END_LOG, strlen(END_LOG));
    return 0;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int rotatory_fwrite(hrotatory_h hr_, int priority, const char *format, ...)
{
    rotatory_log_t *hr = hr_;
    va_list ap;

    if(!hr) {
        // silence
        return -1;
    }
    va_start(ap, format);
    vsnprintf(
        hr->buffer,
        hr->buffer_size,
        format,
        ap
    );
    va_end(ap);

    return rotatory_write(hr, priority, hr->buffer, strlen(hr->buffer));
}

/*****************************************************************
 *  if hr is null truncate all files
 *****************************************************************/
PUBLIC void rotatory_truncate(hrotatory_h hr)
{
    if(hr) {
        _rotatory_truncate(hr);
        return;
    }

    hr = dl_first(&dl_clients);
    while(hr) {
        _rotatory_truncate(hr);
        hr = dl_next(hr);
    }
}

/*****************************************************************
 *  if hr is null flush all files
 *****************************************************************/
PUBLIC void rotatory_flush(hrotatory_h hr)
{
    if(hr) {
        _rotatory_flush(hr);
        return;
    }

    hr = dl_first(&dl_clients);
    while(hr) {
        _rotatory_flush(hr);
        hr = dl_next(hr);
    }
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE void _rotatory_truncate(rotatory_log_t *hr)
{
    if(hr->flog) {
        rotatory_flush(hr);
        fclose(hr->flog);
        hr->flog = fopen(hr->path, "w");
        if(!hr->flog) {
            print_error(
                hr->pe_flag,
                "_rotatory_truncate(): Cannot open '%s' file, %s",
                hr->path,
                strerror(errno)
            );
            return;
        }

        int fd = fileno(hr->flog);
        set_cloexec(fd);
    }
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE void _rotatory_flush(rotatory_log_t *hr)
{
    if(hr->flog) {
#ifdef USE_LOCK_FILE
        lock_file(fileno(hr->flog));
#endif
        fflush(hr->flog);
#ifdef USE_LOCK_FILE
        unlock_file(fileno(hr->flog));
#endif
    }
}

/*****************************************************************
 *  Return TRUE is filename has changed
 *
 *  The name is made from the local DATE only (every letter of the
 *  mask is a part of the date), so it cannot change inside one local
 *  day: it is made again only when the time leaves the day of the
 *  current name (midnight, or the clock set to another day).
 *  Up to 7.25.4 it was made again for every piece written, with a
 *  localtime() that stat()s the zone file each time.
 *****************************************************************/
PRIVATE BOOL _get_rotatory_filename(rotatory_log_t *hr)
{
    time_t now = time(NULL);
    if(now >= hr->day_start && now < hr->next_day_start) {
        return FALSE;
    }

    char path[2*NAME_MAX+1];
    _translate_mask(hr);

    snprintf(path, sizeof(path), "%s/%s", hr->log_directory, hr->filename);

    if(strcmp(path, hr->path)!=0) {
        return TRUE;
    }
    return FALSE;
}

/*****************************************************************
 *  Make sure the file to write the next record is open:
 *  a new name (a new day), the size limit, or the file removed from
 *  the directory. Once for each record, see rotatory_write().
 *  Return -1 if the record cannot be written.
 *****************************************************************/
PRIVATE int _rotatory_prepare(rotatory_log_t *hr)
{
    if(disk_full_informed) {
        return -1;
    }
    BOOL change_file = _get_rotatory_filename(hr);

    if(hr->flog) {
        /*
         *  One fstat() gives the size and whether the file is still in the
         *  directory (st_nlink 0 = removed: a new one is created, as the
         *  access() of each piece did up to 7.25.4).
         */
        struct stat st;
        if(fstat(fileno(hr->flog), &st) == 0) {
            if(st.st_nlink == 0) {
                change_file = 1;
            } else if(hr->max_megas_rotatoryfile_size &&
                    ((uint64_t)st.st_size)/(1024*1024) > hr->max_megas_rotatoryfile_size) {
                /*
                 *  Rename to OLD and create a new file
                 */
                rotatory_flush(hr);
                fclose(hr->flog);
                hr->flog = 0;

                char filename_old[2*NAME_MAX+5];
                snprintf(filename_old, sizeof(filename_old), "%s.OLD",
                    hr->path
                );
                if(access(filename_old, 0)==0) {
                    if(unlink(filename_old)<0) {
                        print_error(
                            PEF_SYSLOG,
                            "_rotatory(): Cannot remove '%s', %s",
                            filename_old,
                            strerror(errno)
                        );
                    }
                }
                if(rename(hr->path, filename_old) < 0) {
                    print_error(
                        PEF_SYSLOG,
                        "_rotatory(): Cannot rename '%s' to '%s', %s",
                        hr->path,
                        filename_old,
                        strerror(errno)
                    );
                }
                change_file = 1;
            }
        }
    } else if(!change_file) {
        if(access(hr->path, 0)!=0) {
            change_file = 1;    // No file open and no file there: create it
        }
    }

    if(change_file) {
        if(hr->flog) {
            rotatory_flush(hr);
            fclose(hr->flog);
            hr->flog = 0;
        }
        if(access(hr->log_directory, 0)!=0) {
            // Creat the directory
            if(mkrdir(hr->log_directory, hr->xpermission)<0) {
                print_error(
                    hr->pe_flag,
                    "_rotatory(): Cannot create '%s' directory, %s",
                    hr->log_directory,
                    strerror(errno)
                );
                return -1;
            }
        }

        char lastpath[2*NAME_MAX+2];
        strncpy(lastpath, hr->path, sizeof(lastpath)-1);
        snprintf(hr->path, sizeof(hr->path), "%s/%s", hr->log_directory, hr->filename);
        if(access(hr->path, 0)!=0) {
            int fd = newfile(hr->path, hr->rpermission, FALSE);
            if(fd < 0) {
                print_error(
                    hr->pe_flag,
                    "_rotatory(): Cannot create '%s' file, %s",
                    hr->path,
                    strerror(errno)
                );
                return -1;
            }
            close(fd);
        }
        hr->flog = fopen(hr->path, "w");
        if(!hr->flog) {
            print_error(
                hr->pe_flag,
                "_rotatory(): Cannot open '%s' file, %s",
                hr->path,
                strerror(errno)
            );
            return -1;
        }

        int fd = fileno(hr->flog);
        set_cloexec(fd);

        if(hr->cb_newfile) {
            (hr->cb_newfile)(hr->user_data, lastpath, hr->path);
        }
    }

    hr->counter_statvfs++;
    hr->counter_statvfs = hr->counter_statvfs % MAX_COUNTER_STATVFS;
    if(hr->flog && hr->counter_statvfs==0) {
        /*
         *  Check filesystem free only periodically,
         *  each MAX_COUNTER_STATVFS times.
         */
#ifdef __linux__
        struct statvfs fiData;
        if(fstatvfs(fileno(hr->flog), &fiData) == 0) {
            int free_percent = (fiData.f_bavail * 100)/fiData.f_blocks;
            if(free_percent < hr->min_free_disk_percentage) {
                // No escribo nada con %free menor que x
                if(!disk_full_informed) {
                    print_error(
                        PEF_SYSLOG,
                        "rotatory(): stop logging because full disk: %d%% free (<%d%%)",
                        free_percent,
                        (int)hr->min_free_disk_percentage
                    );
                    rotatory_flush(hr);
                    disk_full_informed = 1;
                }
                return -1;
            }
        }
#endif /* __linux__ */
    }

    if(!hr->flog) {
        return -1;
    }
    return 0;
}

/*****************************************************************
 *  Write one piece of a record to the file opened by
 *  _rotatory_prepare()
 *****************************************************************/
PRIVATE int _rotatory_fwrite(rotatory_log_t *hr, const char *bf, size_t len)
{
    // TODO perhaps I would remove lock file in order to get more speed.
    // block to write
    if(hr->flog) {
#ifdef USE_LOCK_FILE
        lock_file(fileno(hr->flog));
#endif
        int ret = fwrite(bf, 1, len,hr->flog);
#ifdef USE_LOCK_FILE
        unlock_file(fileno(hr->flog));
#endif
        if(ret <= 0) {
            print_error(
                PEF_SYSLOG,
                "_rotatory(): vfprintf() FAILED, %s",
                strerror(errno)
            );
            fclose(hr->flog);
            hr->flog = 0;
            return ret;
        }
    }

    return 0;
}

/*****************************************************************
 *
 *****************************************************************/
PRIVATE int _translate_mask(rotatory_log_t *hr)
{
    time_t t;
    time(&t);

    formatdate(
        t,
        hr->filename,
        sizeof(hr->filename),
        hr->filenamemask
    );

    /*
     *  The local day of this name: [today 00:00, tomorrow 00:00)
     */
    struct tm tm;
    localtime_r(&t, &tm);
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    time_t day_start = mktime(&tm);
    tm.tm_mday += 1;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    time_t next_day_start = mktime(&tm);

    if(day_start == (time_t)-1 || next_day_start == (time_t)-1 ||
            t < day_start || t >= next_day_start) {
        // The day cannot be told: make the name again in the next second
        hr->day_start = t;
        hr->next_day_start = t + 1;
    } else {
        hr->day_start = day_start;
        hr->next_day_start = next_day_start;
    }

    return 0;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC const char *rotatory_path(hrotatory_h hr_)
{
    rotatory_log_t *hr = hr_;
    return hr->path;
}

/*****************************************************************
 *  TRUE if `name` has the shape of the files of this rotatory:
 *  where formatdate() writes a digit there must be a digit, the rest
 *  of the mask literal, and an optional ".OLD" at the end.
 *****************************************************************/
PRIVATE BOOL name_has_the_shape_of_the_mask(rotatory_log_t *hr, const char *name)
{
    char shape[NAME_MAX+1];

    /*
     *  Same call as formatdate(), with a mark in place of every digit
     */
    translate_string(
        shape,
        sizeof(shape),
        "\001\001/\001\001/\001\001\001\001-\001-\001\001\001",
        hr->filenamemask,
        DATE_MASK
    );

    size_t shape_len = strlen(shape);
    size_t name_len = strlen(name);
    if(name_len == shape_len + strlen(OLD_SUFFIX)) {
        if(strcmp(name + shape_len, OLD_SUFFIX) != 0) {
            return FALSE;
        }
    } else if(name_len != shape_len) {
        return FALSE;
    }

    for(size_t i=0; i<shape_len; i++) {
        if(shape[i] == DIGIT_MARK) {
            if(!isdigit((unsigned char)name[i])) {
                return FALSE;
            }
        } else if(shape[i] != name[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

/*****************************************************************
 *  Retention: remove the files of this rotatory older than keep_days.
 *  See rotatory.h. Nothing of this runs on the write path.
 *****************************************************************/
PUBLIC int rotatory_remove_old_files(
    hrotatory_h hr_,
    unsigned keep_days,
    json_t *jn_removed,
    uint64_t *removed_bytes
)
{
    rotatory_log_t *hr = hr_;

    if(removed_bytes) {
        *removed_bytes = 0;
    }
    if(!hr) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "hr NULL",
            NULL
        );
        return -1;
    }
    if(keep_days == 0) {
        return 0;
    }

    DIR *dir = opendir(hr->log_directory);
    if(!dir) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot open directory",
            "path",         "%s", hr->log_directory,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    char current_old[NAME_MAX+sizeof(OLD_SUFFIX)];
    snprintf(current_old, sizeof(current_old), "%s%s", hr->filename, OLD_SUFFIX);

    time_t limit = time(NULL) - (time_t)keep_days * 24 * 60 * 60;
    int removed = 0;
    struct dirent *de;
    while((de = readdir(dir)) != NULL) {
        const char *name = de->d_name;
        if(strcmp(name, hr->filename) == 0 || strcmp(name, current_old) == 0) {
            continue;
        }
        if(!name_has_the_shape_of_the_mask(hr, name)) {
            continue;
        }

        char path[PATH_MAX];
        build_path(path, sizeof(path), hr->log_directory, name, NULL);

        struct stat st;
#ifdef __linux__
        if(lstat(path, &st) != 0) {
#else
        if(stat(path, &st) != 0) {
#endif
            if(errno != ENOENT) {
                gobj_log_error(0, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "lstat() FAILED",
                    "path",         "%s", path,
                    "errno",        "%d", errno,
                    "serrno",       "%s", strerror(errno),
                    NULL
                );
            }
            continue;
        }
        if(!S_ISREG(st.st_mode)) {
            continue;   // never a symbolic link, a directory, ...
        }
        if(st.st_mtime >= limit) {
            continue;
        }

        if(unlink(path) != 0) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "unlink() FAILED",
                "path",         "%s", path,
                "errno",        "%d", errno,
                "serrno",       "%s", strerror(errno),
                NULL
            );
            continue;
        }
        removed++;
        if(removed_bytes) {
            *removed_bytes += (uint64_t)st.st_size;
        }
        if(jn_removed) {
            json_array_append_new(jn_removed, json_string(name));
        }
    }
    closedir(dir);

    return removed;
}
