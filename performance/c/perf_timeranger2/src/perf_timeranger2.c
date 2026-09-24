/****************************************************************************
 *          perf_timeranger2.c
 *
 *          Benchmark of timeranger2 alone: the open of a store, the create
 *          of topics, and the cost of a tm query before and after
 *          tranger2_mark_tm_order().
 *
 *          Usage:  perf_timeranger2 [--small] [phase ...]
 *
 *          Phases (all of them when none is given):
 *            open     build a topic of KEYS x FILES md2 files x RECS rows,
 *                     then open it REOPEN times as a master and as a
 *                     replica (mean time of one open)
 *            create   create 10 topics, then raise the topic_version of
 *                     each of them (10 version changes)
 *            tm       1 key x 30 daily files x 20000 rows; its topic_desc
 *                     is made to look like a topic of 7.25.4 or earlier (no
 *                     "marks_tm_unordered"); a tm query of one minute in the
 *                     middle file before the migration, the migration
 *                     (tranger2_mark_tm_order), the same query after it
 *          --small divides every size by 10 (for ctest: it checks that the
 *          benchmark builds and runs, its figures are not the reference).
 *
 *          Every result is one line of JSON on stdout:
 *            {"bench": "perf_timeranger2", "case": "<case>",
 *             "seconds": <s>, "ops": <n>, "us_per_op": <us>, ...}
 *          The first line says the version, the build and the sizes.
 *          The store lives in ~/tests_yuneta/perf_timeranger2 and is
 *          removed at the end.
 *
 *          The figures of a release are taken on the same machine with this
 *          benchmark linked against the timeranger2 of both releases, run
 *          alternated, and the medians compared (see README.md). Linked
 *          against 7.25.4, "tm_query_unmigrated" is the tm query of 7.25.4,
 *          and the migration is not run.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include <yuneta_version.h>
#include <yuneta_config.h>
#include <gobj.h>
#include <kwid.h>
#include <helpers.h>
#include <timeranger2.h>

/*
 *  Weak: the benchmark also links against a timeranger2 without it (7.25.4
 *  and earlier), to compare the two. There the tm phase runs no migration.
 */
#pragma weak tranger2_mark_tm_order

/***************************************************************
 *              Constants
 ***************************************************************/
#define BENCH       "perf_timeranger2"
#define DATABASE    "perf_timeranger2"
#define DAY         86400
#define TM0         1700006400      // 2023-11-15 00:00:00 UTC

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int KEYS = 2000;
PRIVATE int FILES = 10;
PRIVATE int RECS = 20;
PRIVATE int REOPEN = 10;
PRIVATE int TM_FILES = 30;
PRIVATE int TM_RECS = 20000;

PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];
PRIVATE int records_seen = 0;

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec/1e9;
}

PRIVATE void result_line(const char *name, double seconds, int ops, const char *extra)
{
    printf("{\"bench\": \"%s\", \"case\": \"%s\", \"seconds\": %.6f, \"ops\": %d, \"us_per_op\": %.3f%s%s}\n",
        BENCH,
        name,
        seconds,
        ops,
        ops > 0? seconds*1e6/ops : 0.0,
        extra? ", " : "",
        extra? extra : ""
    );
    fflush(stdout);
}

PRIVATE json_t *start_tranger(BOOL master)
{
    json_t *jn = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", master,
        "on_critical_error", 0
    );
    return tranger2_startup(0, jn, 0);
}

PRIVATE json_t *create_topic(json_t *tranger, const char *name, int version)
{
    char v[32];
    snprintf(v, sizeof(v), "%d", version);
    return tranger2_create_topic(
        tranger,
        name,
        "id",
        "tm",
        json_pack("{s:s, s:i, s:i}",
            "filename_mask", "%Y-%m-%d",
            "xpermission", 02770,
            "rpermission", 0660
        ),
        sf_string_key,
        json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
        json_pack("{s:s}", "topic_version", v)
    );
}

PRIVATE int append(json_t *tranger, const char *topic, const char *key, json_int_t tm)
{
    md2_record_ex_t md;
    return tranger2_append_record(
        tranger,
        topic,
        (uint64_t)tm,   // __t__: the files are cut by it, one per day
        0,
        &md,
        json_pack("{s:s, s:I, s:s}", "id", key, "tm", tm, "content", "hello world")
    );
}

PRIVATE int count_record(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    json_t *jn_record
)
{
    records_seen++;
    JSON_DECREF(jn_record)
    return 0;
}

/***************************************************************************
 *  The topic as 7.25.4 or earlier wrote it: no "marks_tm_unordered"
 ***************************************************************************/
PRIVATE int make_it_legacy(const char *topic_name)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, topic_name, "topic_desc.json", NULL);
    json_t *desc = load_json_from_file(0, path_database, "tm/topic_desc.json", 0);
    if(!desc) {
        printf("{\"bench\": \"%s\", \"error\": \"cannot read %s\"}\n", BENCH, path);
        return -1;
    }
    json_object_del(desc, "marks_tm_unordered");
    chmod(path, 0660);
    int ret = json_dump_file(desc, path, JSON_INDENT(4));
    JSON_DECREF(desc)
    if(ret < 0) {
        printf("{\"bench\": \"%s\", \"error\": \"cannot write %s\"}\n", BENCH, path);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  A tm query of one minute in the middle file: the records it loads
 ***************************************************************************/
PRIVATE int tm_query(json_t *tranger, double *seconds)
{
    json_int_t middle = TM0 + (json_int_t)(TM_FILES/2)*DAY + DAY/2;
    records_seen = 0;
    double t0 = now_s();
    json_t *iterator = tranger2_open_iterator(
        tranger,
        "tm",
        "key0",
        json_pack("{s:I, s:I}", "from_tm", middle, "to_tm", middle + 59),
        count_record,
        "tm_query",
        BENCH,
        NULL,
        NULL
    );
    if(!iterator) {
        return -1;  // Error already logged
    }
    tranger2_close_iterator(tranger, iterator);
    *seconds = now_s() - t0;
    return records_seen;
}

/***************************************************************
 *              Phases
 ***************************************************************/
PRIVATE int phase_open(void)
{
    json_t *tranger = start_tranger(TRUE);
    if(!create_topic(tranger, "many", 1)) {
        tranger2_shutdown(tranger);
        return -1;  // Error already logged
    }
    char key[64];
    double t0 = now_s();
    for(int k = 0; k < KEYS; k++) {
        snprintf(key, sizeof(key), "key%05d", k);
        for(int f = 0; f < FILES; f++) {
            for(int r = 0; r < RECS; r++) {
                if(append(tranger, "many", key, TM0 + (json_int_t)f*DAY + r*60) < 0) {
                    tranger2_shutdown(tranger);
                    return -1;  // Error already logged
                }
            }
        }
    }
    int n = KEYS*FILES*RECS;
    result_line("build_appends", now_s() - t0, n, NULL);
    tranger2_shutdown(tranger);

    for(int master = 1; master >= 0; master--) {
        double sum = 0;
        for(int i = 0; i < REOPEN; i++) {
            t0 = now_s();
            tranger = start_tranger(master);
            if(!tranger2_open_topic(tranger, "many", TRUE)) {
                tranger2_shutdown(tranger);
                return -1;  // Error already logged
            }
            sum += now_s() - t0;
            tranger2_shutdown(tranger);
        }
        char extra[128];
        snprintf(extra, sizeof(extra), "\"keys\": %d, \"md2_files\": %d", KEYS, KEYS*FILES);
        result_line(master? "open_master" : "open_replica", sum/REOPEN, 1, extra);
    }
    return 0;
}

PRIVATE int phase_create(void)
{
    json_t *tranger = start_tranger(TRUE);
    char name[64];
    double t0 = now_s();
    for(int i = 0; i < 10; i++) {
        snprintf(name, sizeof(name), "topic%02d", i);
        if(!create_topic(tranger, name, 1)) {
            tranger2_shutdown(tranger);
            return -1;  // Error already logged
        }
    }
    result_line("create_topic", now_s() - t0, 10, NULL);
    tranger2_shutdown(tranger);

    tranger = start_tranger(TRUE);
    t0 = now_s();
    for(int i = 0; i < 10; i++) {
        snprintf(name, sizeof(name), "topic%02d", i);
        if(!create_topic(tranger, name, 2)) {
            tranger2_shutdown(tranger);
            return -1;  // Error already logged
        }
    }
    result_line("topic_version_change", now_s() - t0, 10, NULL);
    tranger2_shutdown(tranger);
    return 0;
}

PRIVATE int phase_tm(void)
{
    json_t *tranger = start_tranger(TRUE);
    if(!create_topic(tranger, "tm", 1)) {
        tranger2_shutdown(tranger);
        return -1;  // Error already logged
    }
    double t0 = now_s();
    for(int f = 0; f < TM_FILES; f++) {
        for(int r = 0; r < TM_RECS; r++) {
            if(append(tranger, "tm", "key0", TM0 + (json_int_t)f*DAY + r*(DAY/TM_RECS)) < 0) {
                tranger2_shutdown(tranger);
                return -1;  // Error already logged
            }
        }
    }
    result_line("tm_build_appends", now_s() - t0, TM_FILES*TM_RECS, NULL);
    tranger2_shutdown(tranger);

    if(make_it_legacy("tm") < 0) {
        return -1;
    }

    char extra[128];
    double seconds;
    tranger = start_tranger(TRUE);
    if(!tranger2_open_topic(tranger, "tm", TRUE)) {
        tranger2_shutdown(tranger);
        return -1;  // Error already logged
    }
    int n = tm_query(tranger, &seconds);
    snprintf(extra, sizeof(extra), "\"records\": %d, \"md2_rows\": %d", n, TM_FILES*TM_RECS);
    result_line("tm_query_unmigrated", seconds, 1, extra);

    if(!tranger2_mark_tm_order) {
        result_line("mark_tm_order_not_linked", 0, 0, NULL);
        tranger2_shutdown(tranger);
        return 0;
    }
    t0 = now_s();
    json_t *report = tranger2_mark_tm_order(tranger, "tm");
    seconds = now_s() - t0;
    if(!report) {
        tranger2_shutdown(tranger);
        return -1;  // Error already logged
    }
    JSON_DECREF(report)
    snprintf(extra, sizeof(extra), "\"md2_files\": %d, \"md2_rows\": %d", TM_FILES, TM_FILES*TM_RECS);
    result_line("mark_tm_order", seconds, 1, extra);

    n = tm_query(tranger, &seconds);
    snprintf(extra, sizeof(extra), "\"records\": %d, \"md2_rows\": %d", n, TM_FILES*TM_RECS);
    result_line("tm_query_migrated", seconds, 1, extra);

    tranger2_shutdown(tranger);
    return 0;
}

/***************************************************************************
 *              Main
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

    BOOL do_open = FALSE, do_create = FALSE, do_tm = FALSE, any = FALSE;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--small") == 0) {
            KEYS /= 10;
            TM_RECS /= 10;
            REOPEN = 3;
        } else if(strcmp(argv[i], "open") == 0) {
            do_open = TRUE;
            any = TRUE;
        } else if(strcmp(argv[i], "create") == 0) {
            do_create = TRUE;
            any = TRUE;
        } else if(strcmp(argv[i], "tm") == 0) {
            do_tm = TRUE;
            any = TRUE;
        } else {
            printf("Usage: %s [--small] [open] [create] [tm]\n", argv[0]);
            gobj_end();
            return -1;
        }
    }
    if(!any) {
        do_open = do_create = do_tm = TRUE;
    }

    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

#ifdef CONFIG_DEBUG_TRACK_MEMORY
    int track_memory = 1;
#else
    int track_memory = 0;
#endif
    printf("{\"bench\": \"%s\", \"yuneta_version\": \"%s\", \"track_memory\": %d, "
        "\"keys\": %d, \"files\": %d, \"recs\": %d, \"reopen\": %d, "
        "\"tm_files\": %d, \"tm_recs\": %d}\n",
        BENCH, YUNETA_VERSION, track_memory,
        KEYS, FILES, RECS, REOPEN, TM_FILES, TM_RECS
    );

    int ret = 0;
    if(do_open && ret == 0) {
        ret = phase_open();
    }
    if(do_create && ret == 0) {
        ret = phase_create();
    }
    if(do_tm && ret == 0) {
        ret = phase_tm();
    }
    rmrdir(path_database);

    gobj_end();

    if(ret < 0) {
        printf("{\"bench\": \"%s\", \"result\": \"FAILED\"}\n", BENCH);
        return -1;
    }
    return 0;
}
