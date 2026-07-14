/****************************************************************************
 *          test_rt_disk_multi_feed.c
 *
 *  Regression coverage for publish_new_rt_disk_records() with SEVERAL
 *  rt_disk feeds alive on the same key of a FOLLOWER (a non-master reader —
 *  the gui_treedb Live-cards topology: a per-key card and a whole-topic
 *  card open at the same time).
 *
 *  The master hard-links each new md2 into the directory of EVERY feed that
 *  wants the key, so the follower is woken once per feed and each wake-up
 *  serves only the feed whose directory fired, from its own `published`
 *  watermark. The regression: the watermark was initialized lazily from the
 *  shared key cache, which the FIRST wake-up of the batch had already
 *  advanced — so the feed whose directory fired SECOND computed "nothing
 *  new" and permanently lost the first records after it opened. The fix
 *  seeds the watermark of every feed of the key at the batch start, while
 *  it is still known.
 *
 *      - do_test: master + in-process follower; a keyed feed and a keyless
 *        feed over the same key; each append must reach EACH of them
 *        EXACTLY once (the second-fired feed used to get 0 on the first
 *        append, and the pre-fix fan-out used to deliver N copies).
 *        A brand-new key (its batch starts at rowid 1, so the seeded
 *        watermark is legitimately 0) is covered with a third feed.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_rt_disk_multi_feed"

#define DATABASE    "tr_rt_disk_multi_feed"
#define TOPIC_NAME  "topic_rt_disk_multi_feed"
#define KEY_A       "0000000000000000001"
#define KEY_B       "0000000000000000002"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

PRIVATE int count_keyed_a = 0;      /*  feed "rtA"   (key=KEY_A)    */
PRIVATE int count_keyed_b = 0;      /*  feed "rtB"   (key=KEY_B)    */
PRIVATE int count_keyless = 0;      /*  feed "rtALL" (every key)    */

PRIVATE int my_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    const char *id = json_string_value(json_object_get(list, "id"));
    if(id && strcmp(id, "rtA")==0) {
        count_keyed_a++;
    } else if(id && strcmp(id, "rtB")==0) {
        count_keyed_b++;
    } else if(id && strcmp(id, "rtALL")==0) {
        count_keyless++;
    }
    JSON_DECREF(record)
    return 0;
}

PRIVATE void reset_counts(void)
{
    count_keyed_a = 0;
    count_keyed_b = 0;
    count_keyless = 0;
}

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *startup_tranger(BOOL master)
{
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", master?1:0,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600
    );
    return tranger2_startup(0, jn_tranger, yev_loop);
}

PRIVATE int append_one(json_t *tranger, json_int_t id, uint64_t t)
{
    json_t *jn_record = json_pack("{s:I, s:I, s:s}",
        "id", id,
        "tm", (json_int_t)t,
        "content", "payload"
    );
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, t, 0, &md, jn_record);
}

/*
 *  Drain the loop until the expected counts arrive (or a cap), plus a tail
 *  of extra turns: the DUPLICATE half of the regression only shows up in
 *  the wake-ups that come after the expected ones.
 */
PRIVATE void drain(int expected_a, int expected_b, int expected_all)
{
    for(int i = 0; i < 50; i++) {
        if(count_keyed_a >= expected_a
            && count_keyed_b >= expected_b
            && count_keyless >= expected_all
        ) {
            break;
        }
        yev_loop_run_once(yev_loop);
    }
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE, NULL);
    rmrdir(path_database);

    /*-------------------------------------*
     *  Master with a seeded key
     *-------------------------------------*/
    set_expected_results(
        "multi_feed: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );

    json_t *tm = startup_tranger(TRUE);
    if(!tm) {
        return -1;
    }
    json_t *topic = tranger2_create_topic(
        tm, TOPIC_NAME, "id", "tm",
        json_pack("{s:i, s:s, s:i, s:i}",
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        sf_int_key,
        json_pack("{s:s, s:I, s:s}",
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
    if(!topic) {
        tranger2_shutdown(tm);
        return -1;
    }
    if(append_one(tm, 1, BASE_T)<0) {   /*  KEY_A exists before the feeds  */
        result += -1;
    }
    for(int i = 0; i < 5; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Follower with three feeds:
     *  keyed on A, keyed on B (brand-new), keyless
     *-------------------------------------*/
    set_expected_results(
        "multi_feed: follower + feeds, appends reach each feed once",
        NULL, NULL, NULL, 1
    );

    json_t *tf = startup_tranger(FALSE);
    if(!tf) {
        tranger2_shutdown(tm);
        return -1;
    }
    if(!tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        return -1;
    }

    json_t *rt_a = tranger2_open_rt_disk(
        tf, TOPIC_NAME, KEY_A, NULL, my_record_callback, "rtA", "", NULL
    );
    json_t *rt_b = tranger2_open_rt_disk(
        tf, TOPIC_NAME, KEY_B, NULL, my_record_callback, "rtB", "", NULL
    );
    json_t *rt_all = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL
    );
    if(!rt_a || !rt_b || !rt_all) {
        result += -1;
    }
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }

    /*
     *  FIRST append with both feeds of KEY_A open — the regression: the
     *  shared cache advanced with the first wake-up and the feed whose
     *  directory fired second (unseeded watermark) got NOTHING, forever.
     */
    reset_counts();
    if(append_one(tm, 1, BASE_T + 1)<0) {
        result += -1;
    }
    drain(1, 0, 1);
    if(count_keyed_a != 1 || count_keyless != 1) {
        printf("%sERROR%s --> first append: keyed=%d keyless=%d, expected 1/1\n",
            On_Red BWhite, Color_Off, count_keyed_a, count_keyless);
        result += -1;
    }
    if(count_keyed_b != 0) {
        printf("%sERROR%s --> first append: keyed B got %d, expected 0\n",
            On_Red BWhite, Color_Off, count_keyed_b);
        result += -1;
    }

    /*
     *  Steady state: still exactly once each (the pre-fix fan-out
     *  delivered N copies here).
     */
    reset_counts();
    if(append_one(tm, 1, BASE_T + 2)<0) {
        result += -1;
    }
    drain(1, 0, 1);
    if(count_keyed_a != 1 || count_keyless != 1 || count_keyed_b != 0) {
        printf("%sERROR%s --> second append: keyed=%d keyless=%d keyedB=%d, expected 1/1/0\n",
            On_Red BWhite, Color_Off, count_keyed_a, count_keyless, count_keyed_b);
        result += -1;
    }

    /*
     *  BRAND-NEW key: its batch starts at rowid 1, so the seeded watermark
     *  is legitimately 0 — presence in `published`, not value>0, is what
     *  must say "seeded". Both feeds of KEY_B get it exactly once.
     */
    reset_counts();
    if(append_one(tm, 2, BASE_T + 3)<0) {
        result += -1;
    }
    drain(0, 1, 1);
    if(count_keyed_b != 1 || count_keyless != 1) {
        printf("%sERROR%s --> new key: keyedB=%d keyless=%d, expected 1/1\n",
            On_Red BWhite, Color_Off, count_keyed_b, count_keyless);
        result += -1;
    }
    if(count_keyed_a != 0) {
        printf("%sERROR%s --> new key: keyed A got %d, expected 0\n",
            On_Red BWhite, Color_Off, count_keyed_a);
        result += -1;
    }

    tranger2_close_rt_disk(tf, rt_a);
    tranger2_close_rt_disk(tf, rt_b);
    tranger2_close_rt_disk(tf, rt_all);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    set_expected_results("multi_feed: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    /*
     *  The watcher stops queued by the shutdowns complete asynchronously
     *  (their io_uring CQEs free each fs_event): give the loop the turns to
     *  process them, or their memory is still allocated at the leak check.
     */
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
    }
    return result < 0? -1 : 0;
}
