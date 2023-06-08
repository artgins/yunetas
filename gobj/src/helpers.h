/****************************************************************************
 *              helpers.h
 *              Copyright (c) 2014,2023 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

/*
 *  Dependencies
 */
#include <stdio.h>
#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif

/*****************************************************************
 *     Prototypes
 *****************************************************************/
/*
 *  Wrap mkdir and creat
 *  The use of this functions implies the use of 00_security.h's permission system:
 *  umask will be set to 0 and we control all permission mode.
 */

PUBLIC int newdir(const char *path, int permission);
PUBLIC int newfile(const char *path, int permission, BOOL overwrite);
PUBLIC int open_exclusive(const char *path, int flags, int permission);  // open exclusive

PUBLIC uint64_t filesize(const char *path);
PUBLIC uint64_t filesize2(int fd);

PUBLIC int lock_file(int fd);
PUBLIC int unlock_file(int fd);

PUBLIC BOOL is_regular_file(const char *path);
PUBLIC BOOL is_directory(const char *path);
PUBLIC BOOL file_exists(const char *directory, const char *filename);
PUBLIC BOOL subdir_exists(const char *directory, const char *subdir);
PUBLIC int file_remove(const char *directory, const char *filename);

PUBLIC int mkrdir(const char *path, int index, int permission);
PUBLIC int rmrdir(const char *root_dir);
PUBLIC int rmrcontentdir(const char *root_dir);
PUBLIC int copyfile(
    const char* source,
    const char* destination,
    int permission,
    BOOL overwrite
);

PUBLIC char *build_path(char *bf, size_t bfsize, ...);
PUBLIC char *delete_right_char(char *s, char x);
PUBLIC char *delete_left_char(char *s, char x);

PUBLIC json_t *load_json_from_file(
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error
);

PUBLIC int save_json_to_file(
    const char *directory,
    const char *filename,
    int xpermission,
    int rpermission,
    log_opt_t on_critical_error,
    BOOL create,        // Create file if not exists or overwrite.
    BOOL only_read,
    json_t *jn_data     // owned
);

#ifdef __cplusplus
}
#endif
