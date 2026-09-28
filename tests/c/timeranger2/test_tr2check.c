/****************************************************************************
 *          test_tr2check.c
 *
 *  tr2check (utils/c/tr2check) checks a topic filled by a load test: count,
 *  duplicates, gaps, out-of-range sequences, checksums, storage rate and
 *  latency. The test writes a topic whose defects it knows, runs the tool
 *  on it and reads its json result and its exit code:
 *
 *  - key A: seq 1..10, every checksum right, latency 10..100 ms. Clean.
 *  - key B: seq 5 twice, seq 8 never, seq 3 with a wrong checksum.
 *  - key C: seq 1, 3, 2, 4, 5: out of order, and still clean.
 *  - key D: seq 0 (out of range), and one record with no seq at all.
 *
 *  The topic is sf_t_ms|sf_tm_ms, so the latency is exact to the ms.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_tr2check"
#define DATABASE    "tr_tr2check"
#define TOPIC_NAME  "tracks"

#ifndef TR2CHECK_BIN
#define TR2CHECK_BIN "/yuneta/bin/tr2check"
#endif

#define T0_MS       1790000000000LL

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int expect_int(const char *what, json_int_t got, json_int_t expected)
{
    if(got != expected) {
        printf("%sERROR%s --> %s: %lld, expected %lld\n",
            On_Red BWhite, Color_Off, what, (long long)got, (long long)expected);
        return -1;
    }
    return 0;
}

PRIVATE int expect_str(const char *what, const char *got, const char *expected)
{
    if(strcmp(got? got : "", expected) != 0) {
        printf("%sERROR%s --> %s: '%s', expected '%s'\n",
            On_Red BWhite, Color_Off, what, got? got : "", expected);
        return -1;
    }
    return 0;
}

/*
 *  The checksum the generator writes: sha256 of the record without the
 *  field, compact json with sorted keys.
 */
PRIVATE void set_checksum(json_t *record, BOOL wrong)
{
    char *s = json_dumps(record, JSON_COMPACT|JSON_SORT_KEYS);
    char hex[SHA256_HEX_LEN + 1];
    sha256_hex(s, strlen(s), hex, sizeof(hex));
    GBMEM_FREE(s)
    if(wrong) {
        hex[0] = (hex[0] == '0')? '1' : '0';
    }
    json_object_set_new(record, "checksum", json_string(hex));
}

PRIVATE int append(json_t *tranger, const char *key, json_t *jn_seq, json_int_t latency_ms, BOOL wrong_checksum)
{
    static json_int_t n = 0;
    n++;
    json_int_t tm = T0_MS + n * 1000;
    json_t *record = json_pack("{s:s, s:I, s:f}",
        "id", key,
        "tm", tm,
        "value", 123.45
    );
    if(jn_seq) {
        json_object_set_new(record, "seq", jn_seq);
    }
    set_checksum(record, wrong_checksum);

    md2_record_ex_t md;
    return tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)(tm + latency_ms), 0, &md, record);
}

/*
 *  Run tr2check, return its json result (yours) and its exit code
 */
PRIVATE json_t *run_tr2check(const char *topic_path, const char *options, int *exit_code)
{
    char cmd[PATH_MAX * 2];
    snprintf(cmd, sizeof(cmd), "%s %s %s", TR2CHECK_BIN, topic_path, options);

    FILE *f = popen(cmd, "r");
    if(!f) {
        printf("%sERROR%s --> cannot run '%s'\n", On_Red BWhite, Color_Off, cmd);
        *exit_code = -1;
        return NULL;
    }
    size_t size = 256 * 1024;
    char *bf = gbmem_malloc(size);
    size_t len = bf? fread(bf, 1, size - 1, f) : 0;
    int status = pclose(f);
    *exit_code = WIFEXITED(status)? WEXITSTATUS(status) : -1;
    if(!bf) {
        return NULL;
    }
    bf[len] = 0;
    json_error_t error;
    json_t *jn = json_loads(bf, 0, &error);
    if(!jn && *exit_code != 2) {   // 2: the tool could not check, and says so on stderr
        printf("%sERROR%s --> '%s' did not answer json: %s\n%s\n",
            On_Red BWhite, Color_Off, cmd, error.text, bf);
    }
    GBMEM_FREE(bf)
    return jn;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    char path_topic[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    build_path(path_topic, sizeof(path_topic), path_database, TOPIC_NAME, NULL);
    rmrdir(path_database);

    /*-------------------------------------*
     *  A topic with known defects
     *-------------------------------------*/
    set_expected_results(
        "tr2check: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    ), 0);
    json_t *topic = tranger? tranger2_create_topic(
        tranger, TOPIC_NAME, "id", "tm", NULL, sf_string_key|sf_t_ms|sf_tm_ms,
        NULL, NULL
    ) : NULL;
    if(!topic) {
        printf("%sERROR%s --> cannot create the topic\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    result += test_json(NULL);

    set_expected_results("tr2check: records", NULL, NULL, NULL, 1);
    for(int i = 1; i <= 10; i++) {
        result += append(tranger, "A", json_integer(i), i * 10, FALSE);
    }
    int b_seqs[] = {1, 2, 3, 4, 5, 5, 6, 7, 9, 10};
    for(size_t i = 0; i < ARRAY_SIZE(b_seqs); i++) {
        result += append(tranger, "B", json_integer(b_seqs[i]), 20, b_seqs[i] == 3);
    }
    int c_seqs[] = {1, 3, 2, 4, 5};
    for(size_t i = 0; i < ARRAY_SIZE(c_seqs); i++) {
        result += append(tranger, "C", json_integer(c_seqs[i]), 30, FALSE);
    }
    result += append(tranger, "D", json_integer(0), 40, FALSE);
    result += append(tranger, "D", json_integer(1), 40, FALSE);
    result += append(tranger, "D", NULL, 40, FALSE);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    /*-------------------------------------*
     *  The whole topic: every defect
     *-------------------------------------*/
    set_expected_results("tr2check: the whole topic", NULL, NULL, NULL, 1);
    int exit_code = 0;
    json_t *jn = run_tr2check(path_topic, "--checksum-field=checksum 2>/dev/null", &exit_code);
    result += expect_int("exit code of a topic with defects", exit_code, 1);
    result += expect_str("result", kw_get_str(0, jn, "result", "", 0), "FAIL");
    result += expect_int("keys", kw_get_int(0, jn, "keys", -1, 0), 4);
    result += expect_int("records", kw_get_int(0, jn, "records", -1, 0), 28);
    result += expect_int("duplicated", kw_get_int(0, jn, "duplicated", -1, 0), 1);
    result += expect_int("gaps", kw_get_int(0, jn, "gaps", -1, 0), 1);
    result += expect_int("out_of_range", kw_get_int(0, jn, "out_of_range", -1, 0), 1);
    result += expect_int("no_seq", kw_get_int(0, jn, "no_seq", -1, 0), 1);
    result += expect_int("corrupted", kw_get_int(0, jn, "corrupted", -1, 0), 1);
    result += expect_int("unique", kw_get_int(0, jn, "unique", -1, 0), 10 + 9 + 5 + 1);
    result += expect_int("latency resolution", kw_get_int(0, jn, "latency_resolution_ms", -1, 0), 1);
    result += expect_int("latency max", kw_get_int(0, jn, "latency_ms`max", -1, 0), 100);
    json_t *gap = json_array_get(kw_get_list(0, jn, "examples`gaps", 0, 0), 0);
    result += expect_str("the gap is in B", kw_get_str(0, gap, "key", "", 0), "B");
    result += expect_int("the gap is seq 8", kw_get_int(0, gap, "from", -1, 0), 8);
    json_t *dup = json_array_get(kw_get_list(0, jn, "examples`duplicated", 0, 0), 0);
    result += expect_int("the duplicate is seq 5", kw_get_int(0, dup, "seq", -1, 0), 5);
    JSON_DECREF(jn)
    result += test_json(NULL);

    /*-------------------------------------*
     *  A clean key: latency exact to the ms
     *-------------------------------------*/
    set_expected_results("tr2check: a clean key", NULL, NULL, NULL, 1);
    jn = run_tr2check(path_topic, "--key=A --checksum-field=checksum --expected=10 2>/dev/null", &exit_code);
    result += expect_int("exit code of a clean key", exit_code, 0);
    result += expect_str("result", kw_get_str(0, jn, "result", "", 0), "PASS");
    result += expect_int("records", kw_get_int(0, jn, "records", -1, 0), 10);
    result += expect_int("missing", kw_get_int(0, jn, "missing", -1, 0), 0);
    result += expect_int("seq_min", kw_get_int(0, jn, "seq_min", -1, 0), 1);
    result += expect_int("seq_max", kw_get_int(0, jn, "seq_max", -1, 0), 10);
    result += expect_int("p50", kw_get_int(0, jn, "latency_ms`p50", -1, 0), 50);
    result += expect_int("p90", kw_get_int(0, jn, "latency_ms`p90", -1, 0), 90);
    result += expect_int("p99", kw_get_int(0, jn, "latency_ms`p99", -1, 0), 100);
    result += expect_int("mean", (json_int_t)kw_get_real(0, jn, "latency_ms`mean", -1, 0), 55);
    JSON_DECREF(jn)
    result += test_json(NULL);

    /*-------------------------------------*
     *  Out of order is not a duplicate
     *-------------------------------------*/
    set_expected_results("tr2check: out of order", NULL, NULL, NULL, 1);
    jn = run_tr2check(path_topic, "--key=C 2>/dev/null", &exit_code);
    result += expect_int("exit code of an out-of-order key", exit_code, 0);
    result += expect_int("duplicated", kw_get_int(0, jn, "duplicated", -1, 0), 0);
    result += expect_int("gaps", kw_get_int(0, jn, "gaps", -1, 0), 0);
    JSON_DECREF(jn)
    result += test_json(NULL);

    /*-------------------------------------*
     *  --expected sees the losses at the
     *  end, which a gap cannot see
     *-------------------------------------*/
    set_expected_results("tr2check: losses at the end", NULL, NULL, NULL, 1);
    jn = run_tr2check(path_topic, "--key=A --expected=12 2>/dev/null", &exit_code);
    result += expect_int("exit code with 2 records missing at the end", exit_code, 1);
    result += expect_int("gaps", kw_get_int(0, jn, "gaps", -1, 0), 0);
    result += expect_int("missing", kw_get_int(0, jn, "missing", -1, 0), 2);
    JSON_DECREF(jn)

    jn = run_tr2check(path_topic, "--key=A --seq-to=12 2>/dev/null", &exit_code);
    result += expect_int("--seq-to: gaps at the end", kw_get_int(0, jn, "gaps", -1, 0), 2);
    JSON_DECREF(jn)
    result += test_json(NULL);

    /*-------------------------------------*
     *  One sequence for the whole topic
     *-------------------------------------*/
    set_expected_results("tr2check: topic scope", NULL, NULL, NULL, 1);
    jn = run_tr2check(path_topic, "--rkey='^(A|B)$' --seq-scope=topic 2>/dev/null", &exit_code);
    result += expect_int("keys", kw_get_int(0, jn, "keys", -1, 0), 2);
    result += expect_int("the seqs of A and B collide", kw_get_int(0, jn, "duplicated", -1, 0), 10);
    JSON_DECREF(jn)
    result += test_json(NULL);

    /*-------------------------------------*
     *  Not a topic
     *-------------------------------------*/
    set_expected_results("tr2check: not a topic", NULL, NULL, NULL, 1);
    jn = run_tr2check(path_database, "2>/dev/null", &exit_code);
    result += expect_int("exit code of a path that is not a topic", exit_code, 2);
    JSON_DECREF(jn)
    result += test_json(NULL);

    return result;
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

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    yev_loop_create(0, 2024, 10, NULL, &yev_loop);

    int result = do_test();

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    } else {
        printf("<-- %sTEST OK%s: %s\n", On_Green BWhite, Color_Off, APP);
    }
    return result < 0? -1 : 0;
}
