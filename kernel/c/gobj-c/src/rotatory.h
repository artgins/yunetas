/****************************************************************************
 *              ROTATORY.H
 *              Log by week's days or or month's days or year's days
 *              Copyright (c) 2013 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gobj.h"

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
typedef void * hrotatory_t;

/*****************************************************************
 *     Prototypes
 *****************************************************************/
PUBLIC int rotatory_start_up(void);
PUBLIC void rotatory_end(void); // close all

// Return NULL on error
// Available mask for filename: "DD/MM/CCYY-W-ZZZ"
PUBLIC hrotatory_t rotatory_open(
    const char* path,
    size_t bf_size,                     // 0 = default 64K
    size_t max_megas_rotatoryfile_size, // 0 = default 8, In Megas!!
    size_t min_free_disk_percentage,    // 0 = default 10 %
    int xpermission,
    int rpermission,
    BOOL exit_on_fail
);
PUBLIC void rotatory_close(hrotatory_t hr);

PUBLIC int rotatory_subscribe2newfile(
    hrotatory_t hr,
    int (*cb_newfile)(void *user_data, const char *old_filename, const char *new_filename),
    void *user_data
);

// Return -1 if error
PUBLIC int rotatory_write(hrotatory_t hr, int priority, const char *bf, size_t len);

// Max size defined by bf_size in rotatory_open()
PUBLIC int rotatory_fwrite(hrotatory_t hr_, int priority, const char *format, ...);

// if hr is null trunk all files
PUBLIC void rotatory_trunk(hrotatory_t hr);
 // if hr is null flush all files
PUBLIC void rotatory_flush(hrotatory_t hr);

PUBLIC const char *rotatory_path(hrotatory_t hr);

#ifdef __cplusplus
}
#endif
