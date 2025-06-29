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
    json_t *expected,  // owned
    const char **ignore_keys,
    BOOL verbose
);

/*
 *  These functions free JSONs set by set_expected_results()
 */
PUBLIC int test_json_file( // Compare JSON of the file with JSON in expected
    const char *file
);
PUBLIC int test_json( // Compare JSON in jn_found with JSON in expected
    json_t *jn_found  // owned
);

/*
 *  These functions don't use expected JSONs
 */
PUBLIC int test_directory_permission(const char *path, mode_t permission);
PUBLIC int test_file_permission_and_size(const char *path, mode_t permission, off_t size);

/*
 *  list and match must be two json arrays of objects
 *  sizes of both must match
 *  keys in 'expected' must match with keys in 'found'
 */
PUBLIC int test_list(json_t *found, json_t *expected, const char *msg, ...) JANSSON_ATTRS((format(printf, 3, 4)));


/***************************************************************************
 *  Measurement of Times (MT)
 *  Example:

    ...
    time_measure_t time_measure;
    MT_START_TIME(time_measure)
    ...
    MT_INCREMENT_COUNT(time_measure, MAX_KEYS*MAX_RECORDS)
    MT_PRINT_TIME(time_measure, "tranger2_append_record")
    ...

 ***************************************************************************/
typedef struct {
    struct timespec start, end;
    uint64_t count;
} time_measure_t;

extern PUBLIC time_measure_t yev_time_measure; // to measure yev times

#define MT_START_TIME(time_measure) \
    clock_gettime(CLOCK_MONOTONIC, &time_measure.start); \
    time_measure.end = time_measure.start; \
    time_measure.count = 0;

#define MT_INCREMENT_COUNT(time_measure, cnt) \
    time_measure.count += (cnt);

#define MT_SET_COUNT(time_measure, cnt) \
    time_measure.count = (cnt);

#define MT_PRINT_TIME(time_measure, prefix) \
    clock_gettime(CLOCK_MONOTONIC, &time_measure.end); \
    mt_print_time(&time_measure, prefix);

PUBLIC void mt_print_time(time_measure_t *time_measure, const char *prefix);

PUBLIC double mt_get_time(time_measure_t *time_measure);

PUBLIC void set_measure_times(int types); // Set the measure of times of types (-1 all)
PUBLIC int get_measure_times(void);

#ifdef CONFIG_DEBUG_PRINT_YEV_LOOP_TIMES
extern int measuring_cur_type;
#endif


#ifdef __cplusplus
}
#endif
