/****************************************************************************
 *          testing.h
 *
 *          Auxiliary functions to do testing
 *
 *          Copyright (c) 2019, Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif

/*
 *  Firstly, use set_expected_results()
 *  Then use test_file() or test_json() that return 0 if ok, -1 if error
 */

PUBLIC void set_expected_results(
    const char *name,
    json_t *errors_list,
    json_t *expected,
    const char **ignore_keys,
    BOOL verbose
);

/*
 *  These functions free jsons set by set_expected_results()
 */
PUBLIC int test_file(const char *file);
PUBLIC int test_json(
    json_t *jn_found  // owned
);

PUBLIC int capture_log_write(void* v, int priority, const char* bf, size_t len);

#ifdef __cplusplus
}
#endif
