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
    int verbose
);

PUBLIC BOOL match_record(
    json_t *record, // NOT owned
    json_t *expected, // NOT owned
    int verbose,
    gbuffer_t *gbuf_path
);
PUBLIC BOOL match_list(
    json_t *list, // NOT owned
    json_t *expected, // NOT owned
    int verbose,
    gbuffer_t *gbuf_path
);

PUBLIC BOOL check_log_result(const char *test, int verbose);

PUBLIC BOOL match_tranger_record(
    json_t *tranger,
    const char *topic_name,
    json_int_t rowid,
    uint32_t uflag,
    uint32_t sflag,
    const char *key,
    json_t *record
);

#ifdef __cplusplus
}
#endif
