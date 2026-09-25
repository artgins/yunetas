/****************************************************************************
 *          test_iterator_index.c
 *
 *  The id index of a topic's iterators.
 *
 *  tranger2_get_iterator_by_id() walked the topic's "iterators" array, and
 *  tranger2_open_iterator() calls it to refuse a duplicate: a multi-key
 *  iterator of N keys (C_TRANGER's rkey) opened N iterators in O(N^2). The
 *  topic now keeps "iterators_by_id" {creator: {id: iterator}} beside the
 *  array, and the lookup is one hash access. What this test holds:
 *
 *      - every open iterator is found by (id, creator), and a closed one is
 *        not;
 *      - the same id under another creator is another iterator; an empty
 *        creator matches only a creatorless iterator;
 *      - a duplicate (id, creator) is still refused;
 *      - closing every iterator leaves the index empty (no reference kept).
 *
 *  And the pages of an unfiltered iterator: its segments are taken again
 *  when the key's cache moved (an append must be in the next page), and
 *  ONLY then -- they used to be deep-copied from
 *  the cache on every page of an idle key.
 *
 *  It also PRINTS the time of the two halves of the opens: with a linear
 *  lookup the second half costs more than the first; with the index they
 *  cost the same. Printed, not asserted: a timing is not a test.
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

#define APP         "test_iterator_index"
#define DATABASE    "tr_iterator_index"
#define TOPIC_NAME  "topic_iterator_index"
#define N_KEYS      2000
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000
#define CREATOR     "creator_1"
#define CREATOR2    "creator_2"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE json_t *iterators[N_KEYS];

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *startup_master(const char *path_root)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i, s:I}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600,
        "yev_loop", (json_int_t)0
    );
    return tranger2_startup(0, jn_tranger, 0);
}

PRIVATE void build_key(char *bf, size_t bfsize, int i)
{
    snprintf(bf, bfsize, "k%05d", i);
}

PRIVATE void build_id(char *bf, size_t bfsize, int i)
{
    snprintf(bf, bfsize, "it^k%05d", i);
}

PRIVATE int fill_topic(json_t *tranger)
{
    json_t *topic = tranger2_create_topic(
        tranger,
        TOPIC_NAME,
        "id",
        "tm",
        0,
        sf_string_key,
        json_pack("{s:s, s:I}", "id", "", "tm", (json_int_t)0),
        0
    );
    if(!topic) {
        return -1;
    }
    for(int i = 0; i < N_KEYS; i++) {
        char key[NAME_MAX];
        build_key(key, sizeof(key), i);
        md2_record_ex_t md = {0};
        json_t *jn_record = json_pack("{s:s, s:I}", "id", key, "tm", (json_int_t)BASE_T);
        if(tranger2_append_record(tranger, TOPIC_NAME, BASE_T, 0, &md, jn_record) < 0) {
            return -1;
        }
    }
    return 0;
}

PRIVATE json_t *open_one(json_t *tranger, int i, const char *creator)
{
    char key[NAME_MAX], id[NAME_MAX];
    build_key(key, sizeof(key), i);
    build_id(id, sizeof(id), i);
    return tranger2_open_iterator(
        tranger, TOPIC_NAME, key, json_object(), NULL, id, creator, NULL, NULL
    );
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    set_expected_results(
        "setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup_master(path_root);
    if(!tranger || fill_topic(tranger) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Open N iterators, timing the halves
     *-------------------------------------*/
    set_expected_results("open every key", NULL, NULL, NULL, 1);
    uint64_t t0 = time_in_milliseconds_monotonic();
    for(int i = 0; i < N_KEYS; i++) {
        if(i == N_KEYS/2) {
            uint64_t t1 = time_in_milliseconds_monotonic();
            printf("  first  %d opens: %llu ms\n", N_KEYS/2, (unsigned long long)(t1 - t0));
            t0 = t1;
        }
        iterators[i] = open_one(tranger, i, CREATOR);
        if(!iterators[i]) {
            printf("%sERROR%s --> cannot open iterator %d\n", On_Red BWhite, Color_Off, i);
            result += -1;
            break;
        }
    }
    printf("  second %d opens: %llu ms\n", N_KEYS/2,
        (unsigned long long)(time_in_milliseconds_monotonic() - t0));
    result += test_json(NULL);

    /*-------------------------------------*
     *  Lookups: each one found, by creator
     *-------------------------------------*/
    set_expected_results("each iterator is found by (id, creator)", NULL, NULL, NULL, 1);
    for(int i = 0; i < N_KEYS; i++) {
        char id[NAME_MAX];
        build_id(id, sizeof(id), i);
        if(tranger2_get_iterator_by_id(tranger, TOPIC_NAME, id, CREATOR) != iterators[i]) {
            printf("%sERROR%s --> iterator '%s' not found\n", On_Red BWhite, Color_Off, id);
            result += -1;
            break;
        }
    }
    if(tranger2_get_iterator_by_id(tranger, TOPIC_NAME, "it^k00000", CREATOR2)) {
        printf("%sERROR%s --> found under a creator that never opened it\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(tranger2_get_iterator_by_id(tranger, TOPIC_NAME, "it^k00000", "")) {
        printf("%sERROR%s --> an empty creator matched a creator's iterator\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Same id, other creator: another one.
     *  Same id, same creator: refused.
     *-------------------------------------*/
    set_expected_results(
        "same id: another creator is another iterator, the same one is refused",
        json_pack("[{s:s}]", "msg", "Iterator already exists"),
        NULL, NULL, 1
    );
    json_t *other = open_one(tranger, 0, CREATOR2);
    if(!other || other == iterators[0]) {
        printf("%sERROR%s --> same id under another creator was not a new iterator\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(tranger2_get_iterator_by_id(tranger, TOPIC_NAME, "it^k00000", CREATOR2) != other) {
        printf("%sERROR%s --> the second creator's iterator is not found\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(open_one(tranger, 0, CREATOR)) {
        printf("%sERROR%s --> a duplicate (id, creator) was accepted\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Close half: closed ones are gone,
     *  the rest are still there
     *-------------------------------------*/
    set_expected_results("a closed iterator is not found, an open one is", NULL, NULL, NULL, 1);
    for(int i = 0; i < N_KEYS; i += 2) {
        tranger2_close_iterator(tranger, iterators[i]);
    }
    for(int i = 0; i < N_KEYS; i++) {
        char id[NAME_MAX];
        build_id(id, sizeof(id), i);
        json_t *found = tranger2_get_iterator_by_id(tranger, TOPIC_NAME, id, CREATOR);
        if((i % 2 == 0 && found) || (i % 2 == 1 && found != iterators[i])) {
            printf("%sERROR%s --> after closing half, '%s' answers wrong\n",
                On_Red BWhite, Color_Off, id);
            result += -1;
            break;
        }
    }
    if(tranger2_get_iterator_by_id(tranger, TOPIC_NAME, "it^k00000", CREATOR2) != other) {
        printf("%sERROR%s --> closing creator_1's it^k00000 took creator_2's\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Close the rest: the index is empty
     *-------------------------------------*/
    set_expected_results("closing every iterator empties the index", NULL, NULL, NULL, 1);
    for(int i = 1; i < N_KEYS; i += 2) {
        tranger2_close_iterator(tranger, iterators[i]);
    }
    tranger2_close_iterator(tranger, other);
    json_t *topic = tranger2_topic(tranger, TOPIC_NAME);
    if(json_object_size(json_object_get(topic, "iterators_by_id")) != 0 ||
            json_array_size(json_object_get(topic, "iterators")) != 0) {
        printf("%sERROR%s --> iterators left after closing all: index %d, array %d\n",
            On_Red BWhite, Color_Off,
            (int)json_object_size(json_object_get(topic, "iterators_by_id")),
            (int)json_array_size(json_object_get(topic, "iterators")));
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  The segments of an unfiltered
     *  iterator: taken again when the key
     *  moved, and only then
     *-------------------------------------*/
    set_expected_results("pages take the segments again only when the key moved",
        NULL, NULL, NULL, 1);
    json_t *pager = open_one(tranger, 0, "pager");
    json_t *page = tranger2_iterator_get_page(tranger, pager, 1, 10, TRUE);
    json_t *segments_1 = json_object_get(pager, "segments");
    JSON_DECREF(page)
    page = tranger2_iterator_get_page(tranger, pager, 1, 10, TRUE);
    if(json_object_get(pager, "segments") != segments_1) {
        printf("%sERROR%s --> a page of an idle key took the segments again\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    JSON_DECREF(page)

    char key0[NAME_MAX];
    build_key(key0, sizeof(key0), 0);
    md2_record_ex_t md = {0};
    tranger2_append_record(tranger, TOPIC_NAME, BASE_T + 1, 0, &md,
        json_pack("{s:s, s:I}", "id", key0, "tm", (json_int_t)(BASE_T + 1))
    );
    page = tranger2_iterator_get_page(tranger, pager, 1, 10, TRUE);
    json_t *newest = json_array_get(json_object_get(page, "data"), 0);
    if(kw_get_int(0, page, "total_rows", 0, 0) != 2 ||
            kw_get_int(0, newest, "tm", 0, 0) != BASE_T + 1) {
        printf("%sERROR%s --> the page after an append does not have it: total_rows %d\n",
            On_Red BWhite, Color_Off, (int)kw_get_int(0, page, "total_rows", 0, 0));
        result += -1;
    }
    JSON_DECREF(page)
    tranger2_close_iterator(tranger, pager);
    result += test_json(NULL);

    set_expected_results("shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop_reset_running(yev_loop);
    if(xtimes_once > 1) {
        exit(-1);
    }
}

PRIVATE void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);
}

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
    yuno_catch_signals();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    yev_loop_create(0, 2024, 10, NULL, &yev_loop);

    int result = raise_open_files_limit();  // one file per key, and there are more than 1024
    result += do_test();

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
