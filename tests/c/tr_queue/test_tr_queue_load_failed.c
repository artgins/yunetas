/****************************************************************************
 *          test_tr_queue_load_failed.c
 *
 *  A queue whose key cannot be loaded must not move its `first_rowid`.
 *
 *  trq_load() (tr_queue) and tr2q_load() (the mqtt queues) take the rowid
 *  of the first pending message they load as the queue's `first_rowid`,
 *  and SAVE it to topic_var.json: the next load starts there. When the
 *  load failed, no message was loaded, first_rowid stayed 0, was set to
 *  the size of the topic and saved: the pending messages were skipped for
 *  ever, even after the store was repaired (independent review of the
 *  third fix round, pre-existing).
 *
 *  The md2 of the queue's key is cut behind the running tranger's back,
 *  the queue is loaded (the load fails), the md2 is put back, and the
 *  queue is loaded again after a restart: every pending message must be
 *  there.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <tr_queue.h>
#include <tr2q_mqtt.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_tr_queue_load_failed"
#define DATABASE    "tr_queue_load_failed"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *startup(void)
{
    return tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    ), 0);
}

PRIVATE void md2_path(char *bf, size_t bfsize, const char *topic_name)
{
    build_path(bf, bfsize, path_database, topic_name, "keys", "__rowid__", "queue.md2", NULL);
}

/*
 *  Read the md2 whole, to put it back later
 */
PRIVATE gbuffer_t *save_md2(const char *topic_name)
{
    char path[PATH_MAX];
    md2_path(path, sizeof(path), topic_name);
    off_t size = filesize(path);
    gbuffer_t *gbuf = gbuffer_create((size_t)size, (size_t)size);
    int fd = open(path, O_RDONLY);
    if(fd < 0 || read(fd, gbuffer_cur_wr_pointer(gbuf), (size_t)size) != size) {
        printf("%sERROR%s --> cannot read %s\n", On_Red BWhite, Color_Off, path);
    } else {
        gbuffer_set_wr(gbuf, (size_t)size);
    }
    if(fd >= 0) {
        close(fd);
    }
    return gbuf;
}

PRIVATE void restore_md2(const char *topic_name, gbuffer_t *gbuf)
{
    char path[PATH_MAX];
    md2_path(path, sizeof(path), topic_name);
    size_t size = gbuffer_leftbytes(gbuf);
    int fd = open(path, O_WRONLY|O_TRUNC);
    if(fd < 0 || write(fd, gbuffer_cur_rd_pointer(gbuf), size) != (ssize_t)size) {
        printf("%sERROR%s --> cannot restore %s\n", On_Red BWhite, Color_Off, path);
    }
    if(fd >= 0) {
        close(fd);
    }
}

PRIVATE void cut_md2(const char *topic_name)
{
    char path[PATH_MAX];
    md2_path(path, sizeof(path), topic_name);
    if(truncate(path, 0) < 0) {
        printf("%sERROR%s --> cannot cut %s\n", On_Red BWhite, Color_Off, path);
    }
}

PRIVATE json_int_t saved_first_rowid(json_t *tranger, const char *topic_name)
{
    json_t *topic = tranger2_topic(tranger, topic_name);
    return kw_get_int(0, topic, "first_rowid", 0, 0);
}

PRIVATE int expect_int(const char *what, json_int_t found, json_int_t expected)
{
    if(found != expected) {
        printf("%sERROR%s --> %s: %d, expected %d\n",
            On_Red BWhite, Color_Off, what, (int)found, (int)expected);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  tr_queue
 ***************************************************************************/
PRIVATE int test_trq(void)
{
    int result = 0;
    const char *topic_name = "trq";
    rmrdir(path_database);

    set_expected_results("trq: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    tr_queue_t *trq = trq_open(tranger, topic_name, "tm", 0, 0);
    trq_load(trq);
    for(int i = 1; i <= 3; i++) {
        trq_append2(trq, 946684800 + i, json_pack("{s:i, s:I}", "n", i, "tm", (json_int_t)(946684800 + i)), 0);
    }
    trq_close(trq);
    gbuffer_t *saved = save_md2(topic_name);
    test_json(NULL);    // the setup logs are not what is tested

    /*-------------------------------------*
     *  The load fails: first_rowid is not
     *  moved, nor saved
     *-------------------------------------*/
    set_expected_results("trq: a load that fails moves nothing",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Cannot read record metadata, read FAILED",
            "msg", "Cannot load the whole history of a key of the list: the records read before the failure "
                   "were handed, the list goes on with the next key",
            "msg", "Queue loaded without some of its messages: its first_rowid is not moved nor saved"
        ),
        NULL, NULL, 1
    );
    cut_md2(topic_name);
    trq = trq_open(tranger, topic_name, "tm", 0, 0);
    if(trq_load(trq) == 0) {
        printf("%sERROR%s --> trq_load() answered 0 with a load that failed\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("trq: first_rowid saved after a failed load", saved_first_rowid(tranger, topic_name), 0);
    trq_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    /*-------------------------------------*
     *  Repaired: every pending message
     *  is loaded
     *-------------------------------------*/
    set_expected_results("trq: repaired, every pending message", NULL, NULL, NULL, 1);
    restore_md2(topic_name, saved);
    GBUFFER_DECREF(saved)
    tranger = startup();
    trq = trq_open(tranger, topic_name, "tm", 0, 0);
    trq_load(trq);
    result += expect_int("trq: pending messages after the repair", (json_int_t)trq_size(trq), 3);
    trq_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  tr2q (the mqtt queues)
 ***************************************************************************/
PRIVATE int test_tr2q(void)
{
    int result = 0;
    const char *topic_name = "tr2q";
    rmrdir(path_database);

    set_expected_results("tr2q: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    tr2_queue_t *trq = tr2q_open(tranger, topic_name, "tm", 0, 10, 0);
    tr2q_load(trq);
    for(int i = 1; i <= 3; i++) {
        tr2q_append(trq, 946684800 + i,
            json_pack("{s:i, s:I}", "mid", i, "tm", (json_int_t)(946684800 + i)), 0);
    }
    tr2q_close(trq);
    gbuffer_t *saved = save_md2(topic_name);
    test_json(NULL);    // the setup logs are not what is tested

    set_expected_results("tr2q: a load that fails moves nothing",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Cannot read record metadata, read FAILED",
            "msg", "Cannot load the whole history of a key of the list: the records read before the failure "
                   "were handed, the list goes on with the next key",
            "msg", "Queue loaded without some of its messages: its first_rowid is not moved nor saved"
        ),
        NULL, NULL, 1
    );
    cut_md2(topic_name);
    trq = tr2q_open(tranger, topic_name, "tm", 0, 10, 0);
    if(tr2q_load(trq) == 0) {
        printf("%sERROR%s --> tr2q_load() answered 0 with a load that failed\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("tr2q: first_rowid saved after a failed load", saved_first_rowid(tranger, topic_name), 0);
    tr2q_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    set_expected_results("tr2q: repaired, every pending message", NULL, NULL, NULL, 1);
    restore_md2(topic_name, saved);
    GBUFFER_DECREF(saved)
    tranger = startup();
    trq = tr2q_open(tranger, topic_name, "tm", 0, 10, 0);
    tr2q_load(trq);
    result += expect_int("tr2q: pending messages after the repair", (json_int_t)(tr2q_queued_size(trq) + tr2q_inflight_size(trq)), 3);
    tr2q_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);

    result += test_trq();
    result += test_tr2q();
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
