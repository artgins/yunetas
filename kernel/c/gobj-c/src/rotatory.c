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
#define MAX_OLD_PIECES          9999    // .OLD.<n> of one day, see rotatory_keep_all_old_files()

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
    BOOL keep_all_old;          // size rotation to .OLD.<n>, see rotatory_keep_all_old_files()
    BOOL disk_full;             // below min_free_disk_percentage: records dropped
    uint64_t dropped_records;   // while disk_full
    BOOL name_recurs;           // the mask has no year: a name is used again (W, DD, ZZZ, ...)
    BOOL write_failed;          // the last piece could not be written: see _rotatory_fwrite()
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
PRIVATE BOOL is_open_handle(rotatory_log_t *hr);
PRIVATE void _rotatory_free(rotatory_log_t *hr);
PRIVATE BOOL disk_is_full(rotatory_log_t *hr);
PRIVATE BOOL must_be_emptied(rotatory_log_t *hr, const char *path);
PRIVATE int _rotatory_open_file(rotatory_log_t *hr, BOOL empty_it);
PRIVATE void check_write_again(rotatory_log_t *hr);


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
            _rotatory_free(hr);
            return 0;
        }
    }
    /*-----------------------------*
     *  Make initial filename
     *-----------------------------*/
    hr->name_recurs = strchr(hr->filenamemask, 'Y')? FALSE: TRUE;
    _translate_mask(hr);
    snprintf(hr->path, sizeof(hr->path), "%s/%s", hr->log_directory, hr->filename);

    /*
     *  The same rule as at a new name (see must_be_emptied()): a yuno that
     *  starts on a Monday does not go on with the file of LAST Monday.
     *  Up to 7.25.4 the file was always opened with "a", and one "W" file
     *  held the records of 8 days.
     */
    BOOL empty_it = must_be_emptied(hr, hr->path);

    if(access(hr->path, 0)!=0) {
        int fd = newfile(hr->path, hr->rpermission, FALSE);
        if(fd < 0) {
            print_error(
                hr->pe_flag,
                "rotatory_open(): Cannot create '%s' file, %s",
                hr->path,
                strerror(errno)
            );
            _rotatory_free(hr);
            return 0;
        }
        close(fd);
    }
    hr->flog = fopen(hr->path, empty_it? "w": "a");
    if(!hr->flog) {
        print_error(
            hr->pe_flag,
            "rotatory_open(): Cannot open '%s' file, %s",
            hr->path,
            strerror(errno)
        );
        _rotatory_free(hr);
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

    if(!is_open_handle(hr)) {
        return; // Already closed (rotatory_end(), or a second close): see is_open_handle()
    }

    dl_delete(&dl_clients, hr, 0);
    _rotatory_free(hr);
}

/*****************************************************************
 *  Free a handle that is not (or no longer) in the list
 *****************************************************************/
PRIVATE void _rotatory_free(rotatory_log_t *hr)
{
    if(hr->flog) {
        _rotatory_flush(hr);
        fclose(hr->flog);
        hr->flog = 0;
    }
    free(hr->buffer);   // System memory, see rotatory_open()
    free(hr);
}

/*****************************************************************
 *  TRUE if hr is a handle that is open now.
 *
 *  A handle outlives its rotatory_close() in the hands of others: the
 *  file log handler of glogger keeps it, and entry_point logs after
 *  rotatory_end(). Every public function asks this BEFORE touching the
 *  handle, so a closed one is never read or freed again. It compares
 *  pointers only (a closed handle is freed memory). The list holds one
 *  entry for each file the process writes: a few.
 *****************************************************************/
PRIVATE BOOL is_open_handle(rotatory_log_t *hr)
{
    if(!__initialized__ || !hr) {
        return FALSE;
    }
    return dl_find(&dl_clients, hr)? TRUE: FALSE;
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
    if(!is_open_handle(hr)) {
        print_error(PEF_SYSLOG, "rotatory_subscribe2newfile(): handle not open");
        return -1;
    }
    hr->cb_newfile = cb_newfile;
    hr->user_data = user_data;

    return 0;
}

/*****************************************************************
 *  See rotatory.h
 *****************************************************************/
PUBLIC int rotatory_keep_all_old_files(hrotatory_h hr_, BOOL keep_all)
{
    rotatory_log_t *hr = hr_;
    if(!is_open_handle(hr)) {
        print_error(PEF_SYSLOG, "rotatory_keep_all_old_files(): handle not open");
        return -1;
    }
    hr->keep_all_old = keep_all;
    return 0;
}

/*****************************************************************
 *  Return 0, also when nothing is written (see rotatory.h),
 *  -1 only if hr or bf is NULL
 *****************************************************************/
PUBLIC int rotatory_write(hrotatory_h hr_, int priority, const char* bf, size_t len)
{
    rotatory_log_t *hr = hr_;

    if(!hr || !bf) {
        return -1;
    }
    if(!is_open_handle(hr)) {
        /*
         *  A closed handle: nothing is written. Not -1: glogger stops
         *  calling the next handlers when one answers a negative value.
         */
        return 0;
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
    check_write_again(hr);
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
    if(!is_open_handle(hr)) {
        return 0;   // A closed handle, see rotatory_write()
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
        if(is_open_handle(hr)) {
            _rotatory_truncate(hr);
        }
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
        if(is_open_handle(hr)) {
            _rotatory_flush(hr);
        }
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
    /*
     *  The free space is checked every MAX_COUNTER_STATVFS records, also
     *  while the disk is full: that is how the handle writes again.
     */
    hr->counter_statvfs = (uint16_t)((hr->counter_statvfs + 1) % MAX_COUNTER_STATVFS);
    if(hr->counter_statvfs == 0) {
        disk_is_full(hr);
    }

    BOOL change_file = _get_rotatory_filename(hr);

    if(hr->disk_full) {
        /*
         *  A new name is taken also while the disk is full: the callback
         *  of a new file is where a user applies its retention (the agent
         *  audit), and the retention is what frees the space. Before,
         *  nothing was done while the disk was full, so the retention
         *  never ran and the handle never wrote again.
         */
        if(change_file) {
            if(_rotatory_open_file(hr, must_be_emptied(hr, NULL)) == 0) {
                disk_is_full(hr);   // the retention may have freed the space
            }
            // else error already printed
        }
        if(hr->disk_full || !hr->flog) {
            hr->dropped_records++;
            return -1;
        }
        return 0;
    }

    BOOL empty_it = FALSE;  // see must_be_emptied()

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

                char filename_old[2*NAME_MAX+32];
                snprintf(filename_old, sizeof(filename_old), "%s.OLD",
                    hr->path
                );
                if(hr->keep_all_old) {
                    /*
                     *  The first free .OLD.<n>: no piece of the day is removed
                     */
                    int n;
                    for(n=1; n<=MAX_OLD_PIECES; n++) {
                        snprintf(filename_old, sizeof(filename_old), "%s.OLD.%d",
                            hr->path, n
                        );
                        if(access(filename_old, 0)!=0) {
                            break;
                        }
                    }
                    if(n > MAX_OLD_PIECES) {
                        print_error(
                            PEF_SYSLOG,
                            "_rotatory(): %d pieces of '%s' in one day, the last one is replaced",
                            MAX_OLD_PIECES,
                            hr->path
                        );
                    }
                }
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
                    if(!hr->keep_all_old) {
                        empty_it = TRUE;    // as up to 7.25.4: the size stays bounded
                    }
                }
                change_file = 1;
            }
        }
    } else {
        /*
         *  No file open: none there, or a write failed (see
         *  _rotatory_fwrite()). Open it again: up to 7.25.4 a failed write
         *  (or a piece of 0 bytes) closed the file until the next name.
         */
        change_file = 1;
    }

    if(change_file) {
        if(!empty_it) {
            empty_it = must_be_emptied(hr, NULL);
        }
        if(_rotatory_open_file(hr, empty_it) < 0) {
            return -1;  // Error already printed
        }
    }

    if(!hr->flog) {
        return -1;
    }
    return 0;
}

/*****************************************************************
 *  TRUE if the existing file of the current name must be emptied
 *  before it is written: it was last written before the day that its
 *  name is used for, and its name is used again (the mask has no year):
 *  the file of LAST week of a "W" mask (its name is its retention).
 *  A file written in this day or later is appended to: up to 7.25.4 it
 *  was opened with "w" at a new name, and a clock set back across
 *  midnight emptied the file of the day before, then the one of today.
 *  A handle that keeps all old files never empties one.
 *  `path` NULL: the path of the current name.
 *****************************************************************/
PRIVATE BOOL must_be_emptied(rotatory_log_t *hr, const char *path)
{
    if(hr->keep_all_old || !hr->name_recurs) {
        return FALSE;
    }
    char bf[2*NAME_MAX+2];
    if(!path) {
        snprintf(bf, sizeof(bf), "%s/%s", hr->log_directory, hr->filename);
        path = bf;
    }
    struct stat st;
    if(stat(path, &st) == 0 && st.st_size > 0 && st.st_mtime < hr->day_start) {
        return TRUE;
    }
    return FALSE;
}

/*****************************************************************
 *  Close the file open now (if any) and open the one of the current
 *  name, then call the callback of a new file. Return -1 on error
 *  (printed: this is the sink of the log, it cannot log through itself).
 *****************************************************************/
PRIVATE int _rotatory_open_file(rotatory_log_t *hr, BOOL empty_it)
{
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
    lastpath[sizeof(lastpath)-1] = 0;
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
    hr->flog = fopen(hr->path, empty_it? "w": "a");
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
    return 0;
}

/*****************************************************************
 *  Check the free space of the disk of this handle, and change its
 *  state: below min_free_disk_percentage it stops writing, above it
 *  writes again. One line (stdout and syslog: this is the sink of the
 *  log, it cannot log through itself) when it stops, one when it
 *  writes again. Up to 7.25.4 the state was ONE flag for every handle
 *  of the process, and nothing cleared it.
 *****************************************************************/
PRIVATE BOOL disk_is_full(rotatory_log_t *hr)
{
#ifdef __linux__
    struct statvfs fiData;
    int ret;
    if(hr->flog) {
        ret = fstatvfs(fileno(hr->flog), &fiData);
    } else {
        ret = statvfs(hr->log_directory, &fiData);
    }
    if(ret != 0 || fiData.f_blocks == 0) {
        return hr->disk_full;   // Cannot tell: keep the state
    }
    int free_percent = (int)((fiData.f_bavail * 100)/fiData.f_blocks);

    if(!hr->disk_full && free_percent < (int)hr->min_free_disk_percentage) {
        print_error(
            PEF_SYSLOG,
            "rotatory(): stop logging to '%s' because full disk: %d%% free (<%d%%)",
            hr->path,
            free_percent,
            (int)hr->min_free_disk_percentage
        );
        _rotatory_flush(hr);
        hr->disk_full = TRUE;
        hr->dropped_records = 0;

    } else if(hr->disk_full && free_percent >= (int)hr->min_free_disk_percentage) {
        print_error(
            PEF_SYSLOG,
            "rotatory(): logging to '%s' again: %d%% free (>=%d%%), %llu records were dropped",
            hr->path,
            free_percent,
            (int)hr->min_free_disk_percentage,
            (unsigned long long)hr->dropped_records
        );
        hr->disk_full = FALSE;
    }
#endif /* __linux__ */
    return hr->disk_full;
}

/*****************************************************************
 *  Write one piece of a record to the file opened by
 *  _rotatory_prepare()
 *
 *  A piece of 0 bytes writes nothing and is not an error: up to 7.25.4
 *  fwrite() of 0 bytes was taken as a failure, and the file was closed
 *  until the next name (the next day). A real failure closes the file,
 *  and the next record opens it again (see _rotatory_prepare()). One line
 *  is printed when the writes fail, one when they work again.
 *****************************************************************/
PRIVATE int _rotatory_fwrite(rotatory_log_t *hr, const char *bf, size_t len)
{
    if(!hr->flog || len == 0) {
        return 0;
    }
#ifdef USE_LOCK_FILE
    lock_file(fileno(hr->flog));
#endif
    size_t ret = fwrite(bf, 1, len, hr->flog);
#ifdef USE_LOCK_FILE
    unlock_file(fileno(hr->flog));
#endif
    if(ret != len) {
        if(!hr->write_failed) {
            print_error(
                PEF_SYSLOG,
                "_rotatory(): fwrite() FAILED, '%s', %s",
                hr->path,
                strerror(errno)
            );
        }
        hr->write_failed = TRUE;
        fclose(hr->flog);
        hr->flog = 0;
        return -1;
    }

    return 0;
}

/*****************************************************************
 *  After a failed write, the first record of the file opened again is
 *  flushed at once: the writes work again only if it reaches the file
 *  (a buffered fwrite() does not tell)
 *****************************************************************/
PRIVATE void check_write_again(rotatory_log_t *hr)
{
    if(!hr->write_failed || !hr->flog) {
        return;
    }
    if(fflush(hr->flog) != 0) {
        fclose(hr->flog);   // still failing: printed once, see _rotatory_fwrite()
        hr->flog = 0;
        return;
    }
    hr->write_failed = FALSE;
    print_error(
        PEF_SYSLOG,
        "_rotatory(): writing to '%s' again",
        hr->path
    );
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
    if(!is_open_handle(hr)) {
        return "";
    }
    return hr->path;
}

/*****************************************************************
 *  TRUE if `suffix` is "", ".OLD" or ".OLD.<n>": the file itself or
 *  one of the pieces of a size rotation
 *****************************************************************/
PRIVATE BOOL is_piece_suffix(const char *suffix)
{
    if(*suffix == 0 || strcmp(suffix, OLD_SUFFIX) == 0) {
        return TRUE;
    }
    size_t old_len = strlen(OLD_SUFFIX);
    if(strncmp(suffix, OLD_SUFFIX, old_len) != 0 || suffix[old_len] != '.') {
        return FALSE;
    }
    const char *n = suffix + old_len + 1;
    if(!*n) {
        return FALSE;
    }
    for(; *n; n++) {
        if(!isdigit((unsigned char)*n)) {
            return FALSE;
        }
    }
    return TRUE;
}

/*****************************************************************
 *  TRUE if `name` has the shape of the files of this rotatory:
 *  where formatdate() writes a digit there must be a digit, the rest
 *  of the mask literal, and an optional ".OLD" or ".OLD.<n>" at the end.
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
    if(name_len < shape_len || !is_piece_suffix(name + shape_len)) {
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
 *  See rotatory.h. The rotatory never calls it by itself.
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
    if(!is_open_handle(hr)) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "hr NULL or not open",
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

    size_t current_len = strlen(hr->filename);

    time_t limit = time(NULL) - (time_t)keep_days * 24 * 60 * 60;
    int removed = 0;
    struct dirent *de;
    while((de = readdir(dir)) != NULL) {
        const char *name = de->d_name;
        if(strncmp(name, hr->filename, current_len) == 0 && is_piece_suffix(name + current_len)) {
            continue;   // The current file, or a piece of the current day
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
