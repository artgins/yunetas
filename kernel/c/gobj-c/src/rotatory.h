/****************************************************************************
 *              ROTATORY.H
 *              Log by week's days or or month's days or year's days
 *              Copyright (c) 2013 Niyamaka.
 *              Copyright (c) 2026, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gtypes.h"

/*
 *  Dependencies
 */

#ifdef __cplusplus
extern "C"{
#endif

/*****************************************************************
 *     Constants
 *****************************************************************/
/*
 *  Syslog priority definitions
 */
#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */
/*
 *  Extra mine priority definitions
 */
#define LOG_AUDIT       8       // written without header
#define LOG_MONITOR     9

/*****************************************************************
 *     Structures
 *****************************************************************/
typedef void * hrotatory_h;

/*****************************************************************
 *     Prototypes
 *****************************************************************/
PUBLIC int rotatory_start_up(void);
PUBLIC void rotatory_end(void); // close all

// Return NULL on error
// Available mask for filename: "DD/MM/CCYY-W-ZZZ"
// A name with a day letter (DD, W, ZZZ) or the month (MM) and without the
// year (CCYY) is used again (the "W" of the yuno logs: one file for each
// week day). Its file is emptied before it is written when it was last
// written before the period of the name (the day; the month for a mask
// with MM only): at the first record after the open, and at a new name.
// A file written in the period is appended to. A name with the year, a
// fixed name (no date letter: "logcenter.log"), or a handle that keeps all
// old files (rotatory_keep_all_old_files(), called right after the open),
// never empties a file.
PUBLIC hrotatory_h rotatory_open(
    const char* path,
    size_t bf_size,                     // 0 = default 64K
    size_t max_megas_rotatoryfile_size, // 0 = default 8, In Megas!!
    size_t min_free_disk_percentage,    // 0 = default 10 %
    int xpermission,
    int rpermission,
    BOOL exit_on_fail
);
PUBLIC void rotatory_close(hrotatory_h hr);

// The callback runs for a NEW file only: a new name (a new day) or a size
// rotation (then old_filename == new_filename). Never when the same file
// is opened again: after a failed write, a removed file, a failed truncate.
// When the open of a new file fails (a directory that refuses writes, no
// descriptors, a quota), the callback is not lost: it runs once, at the
// next open that works (on a full disk, tried every 100 records), with
// the old_filename of before the failure.
// A new name is never size-rotated: at a new day the file of the day
// before is left as it is, whatever its size.
// Return 0, -1 if the handle is not open (a line is printed)
PUBLIC int rotatory_subscribe2newfile(
    hrotatory_h hr,
    int (*cb_newfile)(void *user_data, const char *old_filename, const char *new_filename),
    void *user_data
);

// Return 0, also when the record is not written (disk full, a closed handle).
// -1 only if hr or bf is NULL. A len of 0 writes the record separator only.
// A write that fails closes the file; the next record opens it again.
PUBLIC int rotatory_write(hrotatory_h hr, int priority, const char *bf, size_t len);

// Max size defined by bf_size in rotatory_open(). Return as rotatory_write().
PUBLIC int rotatory_fwrite(hrotatory_h hr_, int priority, const char *format, ...);

// if hr is null truncate all files
PUBLIC void rotatory_truncate(hrotatory_h hr);
 // if hr is null flush all files
PUBLIC void rotatory_flush(hrotatory_h hr);

// The path of the current file, "" if the handle is not open
PUBLIC const char *rotatory_path(hrotatory_h hr);

/*
 *  Keep every piece of a day. By default a size rotation renames the
 *  file to "<name>.OLD" and removes the previous .OLD, so a day keeps at
 *  most two pieces (the yuno logs: bounded by design). With keep_all,
 *  each size rotation renames the file to the first free "<name>.OLD.<n>"
 *  (n = 1, 2, ...) and nothing is removed: use it with a retention
 *  (rotatory_remove_old_files()), as the agent audit does.
 *  A rename that fails: without keep_all the file is emptied (the size
 *  stays bounded); with keep_all the file is kept and grows, one line is
 *  printed, and the rename is tried again after 60 seconds of the
 *  monotonic clock (a wall clock set back or forward does not move it),
 *  or at the next name, not at every record. One line is printed when a
 *  rename works again.
 *  Call it right after rotatory_open(): it applies to the file that the
 *  open found (see rotatory_open()).
 *  Return 0, -1 if the handle is not open.
 */
PUBLIC int rotatory_keep_all_old_files(hrotatory_h hr, BOOL keep_all);

/*
 *  Retention: remove the files of this rotatory older than keep_days.
 *
 *  Only the files of THIS rotatory are candidates: regular files of its
 *  directory whose name has the shape of its mask (each mask letter of
 *  "DD/MM/CCYY-W-ZZZ" a digit, the rest literal), and their ".OLD" or
 *  ".OLD.<n>" pieces.
 *  Never the current file or its ".OLD", never a symbolic link, never a
 *  directory. The age is the file's mtime.
 *
 *  The rotatory never calls it by itself. Call it when the rotatory is
 *  opened and from the callback of rotatory_subscribe2newfile(): that
 *  callback runs inside the rotatory_write() of the first record of a new
 *  file (once a day, or at a size rotation), before that record is written,
 *  and only then: not when the same file is opened again (a failed write).
 *  A new day calls it also while the disk is below min_free_disk_percentage
 *  (records dropped): the retention is what frees the space, and the
 *  record of that moment is written if it does. If the open of the new
 *  file fails, the callback runs at the next open that works (see
 *  rotatory_subscribe2newfile()).
 *
 *  Return the number of files removed, -1 on error (logged).
 */
PUBLIC int rotatory_remove_old_files(
    hrotatory_h hr,
    unsigned keep_days,         // 0 = remove nothing
    json_t *jn_removed,         // not owned, optional: the removed names are appended
    uint64_t *removed_bytes     // optional: the size of the removed files
);

#ifdef __cplusplus
}
#endif
