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


/***************************************************************************
 *  Measurement of times
 ***************************************************************************/
typedef struct {
    struct timespec start, end;
    uint64_t count;
} time_measure_t;

#define MT_START_TIME(time_measure) \
    clock_gettime(CLOCK_MONOTONIC, &time_measure.start); \
    time_measure.end = time_measure.start; \
    time_measure.count = 0;

#define MT_INCREMENT_COUNT(time_measure, cnt) \
    time_measure.count += (cnt);

#define MT_PRINT_TIME(time_measure, prefix) \
    clock_gettime(CLOCK_MONOTONIC, &time_measure.end); \
    mt_print_time(&time_measure, prefix);

static inline void mt_print_time(time_measure_t *time_measure, const char *prefix)
{
    register uint64_t s, e;
    s = ((uint64_t)time_measure->start.tv_sec)*1000000 + ((uint64_t)time_measure->start.tv_nsec)/1000;
    e = ((uint64_t)time_measure->end.tv_sec)*1000000 + ((uint64_t)time_measure->end.tv_nsec)/1000;
    double dt =  ((double)(e-s))/1000000;

    printf("%s# TIME %s (count: %"PRIu64"): %f seconds, %'ld op/sec%s\n",
           On_Black RGreen,
           prefix?prefix:"",
           time_measure->count,
           dt,
           (long)(((double)time_measure->count)/dt),
           Color_Off
    );
}

#ifdef __cplusplus
}
#endif
