/****************************************************************************
 *          test_date_bound_units.c
 *
 *  A time bound given as a DATE ("2000-01-02T00:00:00+0000") is read in
 *  seconds and taken to the unit of its axis: milliseconds for `t` on an
 *  sf_t_ms topic, for `tm` on an sf_tm_ms one. Up to 7.27.4 get_segments()
 *  asked sf_tm_ms for the `t` bounds too: on a topic with t in ms and tm in
 *  seconds a date in from_t/to_t was compared as seconds against ms (it
 *  selected every row, or none), and the reverse on a topic with tm in ms.
 *  An END bound takes its whole second: `to_t` 23:59:59 holds a record at
 *  23:59:59.500 of an axis in ms (it was 23:59:59.000, and lost it).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_date_bound_units"

#define DATABASE    "tr_date_bound_units"
#define TOPIC_T_MS  "topic_t_ms"    // t in ms, tm in seconds
#define TOPIC_TM_MS "topic_tm_ms"   // t in seconds, tm in ms
#define DAY1        946684800       // 2000-01-01
#define DAY_SECS    86400

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int expect(const char *what, const char *got, const char *expected);

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *create_topic(json_t *tranger, const char *topic_name, system_flag2_t system_flag)
{
    return tranger2_create_topic(
        tranger, topic_name, "id", "tm",
        json_pack("{s:s}", "filename_mask", "%Y-%m-%d"),
        system_flag,
        json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
        0
    );
}

/*
 *  One record of day `d` (0, 1, 2), its t and its tm in the units of the topic
 */
PRIVATE void append_day(json_t *tranger, const char *topic_name, BOOL t_ms, BOOL tm_ms, int d, const char *content)
{
    uint64_t secs = DAY1 + (uint64_t)d*DAY_SECS + 3600;
    json_t *record = json_pack("{s:I, s:I, s:s}",
        "id", (json_int_t)1,
        "tm", (json_int_t)(tm_ms? secs*1000 : secs),
        "content", content
    );
    md2_record_ex_t md = {0};
    tranger2_append_record(tranger, topic_name, t_ms? secs*1000 : secs, 0, &md, record);
}

/*
 *  What a load of the key serves with `cond` (owned), as "<content> ..."
 */
PRIVATE void served(json_t *tranger, const char *topic_name, json_t *cond, char *bf, size_t bfsize)
{
    bf[0] = 0;
    json_t *data = json_array();
    json_t *it = tranger2_open_iterator(
        tranger, topic_name, "0000000000000000001", cond, NULL, "served", "", data, NULL
    );
    int idx; json_t *record;
    json_array_foreach(data, idx, record) {
        if(bf[0]) {
            strncat(bf, " ", bfsize - strlen(bf) - 1);
        }
        strncat(bf, kw_get_str(0, record, "content", "?", 0), bfsize - strlen(bf) - 1);
    }
    if(it) {
        tranger2_close_iterator(tranger, it);
    }
    JSON_DECREF(data)
}

PRIVATE int expect(const char *what, const char *got, const char *expected)
{
    if(strcmp(got, expected) != 0) {
        printf("%sERROR%s --> %s: '%s', expected '%s'\n",
            On_Red BWhite, Color_Off, what, got, expected);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char bf[256];
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);
    mkrdir(path_root, 02770);

    set_expected_results(
        "date bounds: setup",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = tranger2_startup(0,
        json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        ),
        yev_loop
    );
    if(!tranger ||
            !create_topic(tranger, TOPIC_T_MS, sf_int_key|sf_t_ms) ||
            !create_topic(tranger, TOPIC_TM_MS, sf_int_key|sf_tm_ms)) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    const char *contents[] = {"D1", "D2", "D3"};
    for(int d = 0; d < 3; d++) {
        append_day(tranger, TOPIC_T_MS, TRUE, FALSE, d, contents[d]);
        append_day(tranger, TOPIC_TM_MS, FALSE, TRUE, d, contents[d]);
        if(d == 1) {
            /*  The last half second of day 2, on the axis in ms  */
            uint64_t last = DAY1 + 2*DAY_SECS - 1;
            md2_record_ex_t md = {0};
            tranger2_append_record(tranger, TOPIC_T_MS, last*1000 + 500, 0, &md,
                json_pack("{s:I, s:I, s:s}", "id", (json_int_t)1, "tm", (json_int_t)last, "content", "D2b")
            );
            tranger2_append_record(tranger, TOPIC_TM_MS, last, 0, &md,
                json_pack("{s:I, s:I, s:s}", "id", (json_int_t)1, "tm", (json_int_t)(last*1000 + 500), "content", "D2b")
            );
        }
    }
    result += test_json(NULL);

    set_expected_results("date bounds: each axis in its own unit", NULL, NULL, NULL, 1);
    const char *topics[] = {TOPIC_T_MS, TOPIC_TM_MS, NULL};
    for(int i = 0; topics[i]; i++) {
        char what[128];
        served(tranger, topics[i],
            json_pack("{s:s, s:s}",
                "from_t", "2000-01-02T00:00:00+0000",
                "to_t", "2000-01-02T23:59:59+0000"
            ),
            bf, sizeof(bf)
        );
        snprintf(what, sizeof(what), "%s: a day by t", topics[i]);
        result += expect(what, bf, "D2 D2b");

        served(tranger, topics[i],
            json_pack("{s:s, s:s}",
                "from_tm", "2000-01-02T00:00:00+0000",
                "to_tm", "2000-01-02T23:59:59+0000"
            ),
            bf, sizeof(bf)
        );
        snprintf(what, sizeof(what), "%s: a day by tm", topics[i]);
        result += expect(what, bf, "D2 D2b");

        served(tranger, topics[i],
            json_pack("{s:s}", "from_t", "2000-01-02T00:00:00+0000"),
            bf, sizeof(bf)
        );
        snprintf(what, sizeof(what), "%s: from a day by t", topics[i]);
        result += expect(what, bf, "D2 D2b D3");
    }
    result += test_json(NULL);

    set_expected_results("date bounds: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;
    gbmem_get_json_allocators(&malloc_func, &free_func);
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
