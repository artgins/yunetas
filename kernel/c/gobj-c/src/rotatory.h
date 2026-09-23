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

PUBLIC int rotatory_subscribe2newfile(
    hrotatory_h hr,
    int (*cb_newfile)(void *user_data, const char *old_filename, const char *new_filename),
    void *user_data
);

// Return -1 if error
PUBLIC int rotatory_write(hrotatory_h hr, int priority, const char *bf, size_t len);

// Max size defined by bf_size in rotatory_open()
PUBLIC int rotatory_fwrite(hrotatory_h hr_, int priority, const char *format, ...);

// if hr is null truncate all files
PUBLIC void rotatory_truncate(hrotatory_h hr);
 // if hr is null flush all files
PUBLIC void rotatory_flush(hrotatory_h hr);

PUBLIC const char *rotatory_path(hrotatory_h hr);

/*
 *  Keep every piece of a day. By default a size rotation renames the
 *  file to "<name>.OLD" and removes the previous .OLD, so a day keeps at
 *  most two pieces (the yuno logs: bounded by design). With keep_all,
 *  each size rotation renames the file to the first free "<name>.OLD.<n>"
 *  (n = 1, 2, ...) and nothing is removed: use it with a retention
 *  (rotatory_remove_old_files()), as the agent audit does.
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
 *  Nothing is done on the write path: call it when the rotatory is opened
 *  and from the callback of rotatory_subscribe2newfile().
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
