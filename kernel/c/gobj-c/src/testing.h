/****************************************************************************
 *          testing.h
 *
 *          Auxiliary functions to do testing
 *
 *          Copyright (c) 2019, Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <sys/stat.h>
#include "ansi_escape_codes.h"
#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif

PUBLIC int capture_log_write(void* v, int priority, const char* bf, size_t len);

/*
 *  Firstly, use set_expected_results()
 *  Then use test_json_file() or test_json() that return 0 if ok, -1 if error
 */

PUBLIC void set_expected_results(
    const char *name,
    json_t *errors_list,
    json_t *expected,
    const char **ignore_keys,
    BOOL verbose
);

/*
 *  These functions free JSONs set by set_expected_results()
 */
PUBLIC int test_json_file(const char *file); // Compare JSON of the file with JSON in expected
PUBLIC int test_json( // Compare JSON in jn_found with JSON in expected
    json_t *jn_found  // owned
);

/*
 *  These functions don't use expected JSONs
 */
PUBLIC int test_directory_permission(const char *path, mode_t permission);
PUBLIC int test_file_permission_and_size(const char *path, mode_t permission, off_t size);

#ifdef __cplusplus
}
#endif
