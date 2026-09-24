/****************************************************************************
 *          test_tr_queue_backup_failed.c
 *
 *  A queue backup that FAILS leaves the queue working in its topic.
 *
 *  trq_check_backup() (tr_queue) and tr2q_check_backup() (the mqtt queues)
 *  took the return of tranger2_backup_topic() as the queue's topic without
 *  looking at it. The backup closes the topic first, so when it failed
 *  after that (here: the rename, the backup name is taken by a FILE) the
 *  queue was left with no topic and the call answered 0: every read logged
 *  "What topic?", every ack answered -1, and no backup ever happened again.
 *  Now the backup opens the topic again, the queue takes it, and the call
 *  answers -1.
 *
 *  tranger2_backup_topic() itself also leaked the topic_var it had loaded
 *  when the rename failed: the memory check at the end catches it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
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

#define APP         "test_tr_queue_backup_failed"
#define DATABASE    "tr_queue_backup_failed"

#define MSG_MOVING      "Backup timeranger topic, moving"
#define MSG_RENAME      "cannot backup topic"
#define MSG_REOPENED    "Backup of topic failed: the topic is opened again as it was, not backed up"
#define MSG_QUEUE       "Queue backup failed: the queue goes on in its topic, not backed up"

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

/*
 *  The backup's name taken by a regular file: the rename fails (ENOTDIR)
 */
PRIVATE void take_backup_name(const char *topic_name)
{
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "%s.bak", topic_name);
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, name, NULL);
    int fd = open(path, O_CREAT|O_WRONLY, 0660);
    if(fd < 0) {
        printf("%sERROR%s --> cannot create %s\n", On_Red BWhite, Color_Off, path);
        return;
    }
    close(fd);
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

/*
 *  A message of the mqtt queues: its payload travels as a gbuffer
 */
PRIVATE json_t *tr2q_kw(int mid, json_int_t tm)
{
    gbuffer_t *gbuf = gbuffer_create(16, 16);
    gbuffer_append_string(gbuf, "payload");
    return json_pack("{s:i, s:I, s:I}",
        "mid", mid,
        "tm", tm,
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
}

PRIVATE json_t *expected_backup_failure(void)
{
    return json_pack("[{s:s},{s:s},{s:s},{s:s}]",
        "msg", MSG_MOVING,
        "msg", MSG_RENAME,
        "msg", MSG_REOPENED,
        "msg", MSG_QUEUE
    );
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
    tr_queue_t *trq = trq_open(tranger, topic_name, "tm", 0, 1 /* backup_queue_size */);
    trq_load(trq);
    q_msg_t *msg = trq_append2(trq, 946684801, json_pack("{s:i, s:I}", "n", 1, "tm", (json_int_t)946684801), 0);
    trq_unload_msg(msg, 0);
    take_backup_name(topic_name);
    test_json(NULL);    // the setup logs are not what is tested

    set_expected_results("trq: the backup fails", expected_backup_failure(), NULL, NULL, 1);
    int ret = trq_check_backup(trq);
    result += expect_int("trq: trq_check_backup() of a failed backup", ret, -1);
    if(!trq->topic || trq->topic != tranger2_topic(tranger, topic_name)) {
        printf("%sERROR%s --> trq: the queue has no topic after the failed backup\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("trq: topic size after the failed backup",
        (json_int_t)tranger2_topic_size(tranger, topic_name), 1);
    result += test_json(NULL);

    set_expected_results("trq: the queue works after the failed backup", NULL, NULL, NULL, 1);
    if(trq->topic) {
        msg = trq_append2(trq, 946684802, json_pack("{s:i, s:I}", "n", 2, "tm", (json_int_t)946684802), 0);
        if(!msg) {
            printf("%sERROR%s --> trq: append after the failed backup\n", On_Red BWhite, Color_Off);
            result += -1;
        } else {
            json_t *jn = trq_msg_json(msg);
            if(!jn) {
                printf("%sERROR%s --> trq: the message cannot be read\n", On_Red BWhite, Color_Off);
                result += -1;
            }
            JSON_DECREF(jn)
            result += expect_int("trq: ack after the failed backup",
                trq_set_hard_flag(msg, TRQ_MSG_PENDING, 0), 0);
        }
        result += expect_int("trq: topic size", (json_int_t)tranger2_topic_size(tranger, topic_name), 2);
    }
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
    tr2_queue_t *trq = tr2q_open(tranger, topic_name, "tm", 0, 10, 1 /* backup_queue_size */);
    tr2q_load(trq);
    tr2q_append(trq, 946684801, tr2q_kw(1, 946684801), 0);
    take_backup_name(topic_name);
    test_json(NULL);    // the setup logs are not what is tested

    set_expected_results("tr2q: the backup fails", expected_backup_failure(), NULL, NULL, 1);
    int ret = tr2q_check_backup(trq);
    result += expect_int("tr2q: tr2q_check_backup() of a failed backup", ret, -1);
    if(!trq->topic || trq->topic != tranger2_topic(tranger, topic_name)) {
        printf("%sERROR%s --> tr2q: the queue has no topic after the failed backup\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("tr2q: topic size after the failed backup",
        (json_int_t)tranger2_topic_size(tranger, topic_name), 1);
    result += test_json(NULL);

    set_expected_results("tr2q: the queue works after the failed backup", NULL, NULL, NULL, 1);
    if(trq->topic) {
        q2_msg_t *msg = tr2q_append(trq, 946684802, tr2q_kw(2, 946684802), 0);
        if(!msg) {
            printf("%sERROR%s --> tr2q: append after the failed backup\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        result += expect_int("tr2q: topic size", (json_int_t)tranger2_topic_size(tranger, topic_name), 2);
    }
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

    rmrdir(path_database);
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
