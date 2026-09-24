/****************************************************************************
 *          perf_rotatory.c
 *
 *          Benchmark of the rotatory (the file log of every yuno and the
 *          agent audit): what one record costs.
 *
 *          Usage:  perf_rotatory [--small] [rounds]
 *
 *          Cases (RECORDS records each, `rounds` times, 1 by default):
 *            audit_record            a record of 300 bytes written as the
 *                                    agent audit writes it: two
 *                                    rotatory_write() calls (the text, then
 *                                    "\n"), LOG_AUDIT, no header
 *            audit_record_retention  the same, with the newfile callback of
 *                                    the audit subscribed (its retention
 *                                    runs at the open)
 *            audit_record_flush      the same, and rotatory_flush() after
 *                                    each record (one write() to the file
 *                                    for each record)
 *            log_record              a record of 300 bytes with its
 *                                    priority header (LOG_INFO), one call
 *          --small: RECORDS / 10 (for ctest: it checks that the benchmark
 *          builds and runs, its figures are not the reference).
 *
 *          Every result is one line of JSON on stdout:
 *            {"bench": "perf_rotatory", "case": "<case>", "round": <n>,
 *             "seconds": <s>, "records": <n>, "ns_per_record": <ns>}
 *          The first line says the version, the build and the sizes.
 *          The files live in ~/tests_yuneta/perf_rotatory and are removed
 *          at the end.
 *
 *          The figures of a release are taken on the same machine with this
 *          benchmark linked against the rotatory.c of both releases, run
 *          alternated (see README.md). Linked against 7.25.4 (no
 *          retention in its rotatory), audit_record_retention prints
 *          "retention_not_linked".
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>

#include <yuneta_version.h>
#include <yuneta_config.h>
#include <gobj.h>
#include <helpers.h>
#include <rotatory.h>

/*
 *  Weak: the benchmark also links against a rotatory without them (7.25.4
 *  and earlier), to compare the two. There the retention case is not run.
 */
#pragma weak rotatory_keep_all_old_files
#pragma weak rotatory_remove_old_files

/***************************************************************
 *              Constants
 ***************************************************************/
#define BENCH       "perf_rotatory"
#define MASK        "ZZZ-DD_MM_CCYY.log"    // the mask of the agent audit
#define LINE_LEN    300

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int RECORDS = 300000;

PRIVATE char path_root[PATH_MAX];
PRIVATE char path_bench[PATH_MAX];

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec/1e9;
}

PRIVATE void result_line(const char *name, int round, double seconds, int records)
{
    printf("{\"bench\": \"%s\", \"case\": \"%s\", \"round\": %d, \"seconds\": %.6f, "
        "\"records\": %d, \"ns_per_record\": %.1f}\n",
        BENCH,
        name,
        round,
        seconds,
        records,
        records > 0? seconds*1e9/records : 0.0
    );
    fflush(stdout);
}

/*
 *  The newfile callback of the agent audit: its retention (7 days)
 */
PRIVATE int newfile_cb(void *user_data, const char *old_filename, const char *new_filename)
{
    rotatory_remove_old_files(user_data, 7, NULL, NULL);
    return 0;
}

/***************************************************************
 *  One case: RECORDS records into a fresh directory
 ***************************************************************/
typedef enum {
    CASE_AUDIT = 0,
    CASE_AUDIT_RETENTION,
    CASE_AUDIT_FLUSH,
    CASE_LOG,
} case_t;

PRIVATE int run_case(case_t c, const char *name, int round)
{
    if(c == CASE_AUDIT_RETENTION && (!rotatory_keep_all_old_files || !rotatory_remove_old_files)) {
        printf("{\"bench\": \"%s\", \"case\": \"%s\", \"round\": %d, \"retention_not_linked\": true}\n",
            BENCH, name, round);
        return 0;
    }
    rmrdir(path_bench);
    if(mkrdir(path_bench, 02770) < 0) {
        return -1;  // Error already logged
    }
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_bench, MASK, NULL);

    hrotatory_h hr = rotatory_open(
        path,
        0,          // default buffer
        500,        // megas: no size rotation during the run
        1,          // min free disk percentage: do not stop on a full disk
        02775,
        0660,
        FALSE
    );
    if(!hr) {
        printf("{\"bench\": \"%s\", \"error\": \"cannot open %s\"}\n", BENCH, path);
        return -1;
    }
    if(c == CASE_AUDIT_RETENTION) {
        rotatory_keep_all_old_files(hr, TRUE);
        rotatory_subscribe2newfile(hr, newfile_cb, hr);
        rotatory_remove_old_files(hr, 7, NULL, NULL);
    }

    char line[LINE_LEN + 1];
    memset(line, 'a', LINE_LEN);
    line[LINE_LEN] = 0;

    double t0 = now_s();
    for(int i=0; i<RECORDS; i++) {
        if(c == CASE_LOG) {
            rotatory_write(hr, LOG_INFO, line, LINE_LEN);
        } else {
            rotatory_write(hr, LOG_AUDIT, line, LINE_LEN);
            rotatory_write(hr, LOG_AUDIT, "\n", 1);
            if(c == CASE_AUDIT_FLUSH) {
                rotatory_flush(hr);
            }
        }
    }
    double t1 = now_s();

    rotatory_close(hr);
    result_line(name, round, t1 - t0, RECORDS);
    return 0;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);
    rotatory_start_up();

    int rounds = 1;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--small") == 0) {
            RECORDS /= 10;
        } else if(atoi(argv[i]) > 0) {
            rounds = atoi(argv[i]);
        } else {
            printf("Usage: %s [--small] [rounds]\n", argv[0]);
            rotatory_end();
            gobj_end();
            return -1;
        }
    }

    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_bench, sizeof(path_bench), path_root, BENCH, NULL);

#ifdef CONFIG_DEBUG_TRACK_MEMORY
    int track_memory = 1;
#else
    int track_memory = 0;
#endif
    printf("{\"bench\": \"%s\", \"yuneta_version\": \"%s\", \"track_memory\": %d, "
        "\"records\": %d, \"line_len\": %d, \"rounds\": %d}\n",
        BENCH, YUNETA_VERSION, track_memory,
        RECORDS, LINE_LEN, rounds
    );

    int ret = 0;
    for(int r=1; r<=rounds && ret == 0; r++) {
        ret += run_case(CASE_AUDIT, "audit_record", r);
        ret += run_case(CASE_AUDIT_RETENTION, "audit_record_retention", r);
        ret += run_case(CASE_AUDIT_FLUSH, "audit_record_flush", r);
        ret += run_case(CASE_LOG, "log_record", r);
    }
    rmrdir(path_bench);

    rotatory_end();
    gobj_end();

    if(ret < 0) {
        printf("{\"bench\": \"%s\", \"result\": \"FAILED\"}\n", BENCH);
        return -1;
    }
    return 0;
}
