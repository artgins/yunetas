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

PUBLIC void set_expected_results(
    const char *name,
    json_t *errors_list,
    json_t *expected,
    BOOL verbose
);

PUBLIC int test_file(const char *file);


#ifdef __cplusplus
}
#endif
