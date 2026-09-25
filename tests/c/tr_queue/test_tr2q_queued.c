/****************************************************************************
 *          test_tr2q_queued.c
 *
 *  The QUEUED messages of an mqtt queue (tr2q_mqtt): the messages above
 *  max_inflight_messages, kept on disk only, without their content.
 *
 *  1.  tr2q_move_from_queued_to_inflight() of a message whose content
 *      cannot be read: it answers -1 and the message STAYS QUEUED. Up to
 *      7.25.4 it was moved in flight before its content was read: on a
 *      failed read it was left in flight with no content, and the caller
 *      (c_prot_mqtt2, an incoming QoS 2 message) broke without sending its
 *      PUBREC -- a message in flight that nobody would ever complete.
 *
 *  2.  tr2q_check_backup() with nothing in flight and messages QUEUED:
 *      the backup is not made. c_prot_mqtt2 calls it every second when
 *      nothing is in flight, and a backup re-creates the topic empty: the
 *      queued messages point to records of the backed-up topic, and their
 *      content could not be read any more (lost). Up to 7.25.4 it was
 *      made (an in-flight message that expired left the queued ones
 *      waiting, with nothing in flight).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <tr2q_mqtt.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_tr2q_queued"
#define DATABASE    "tr2q_queued"

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

PRIVATE json_t *tr2q_kw(int mid, json_int_t tm)
{
    return json_pack("{s:i, s:I, s:s}", "mid", mid, "tm", tm, "topic", "t/q");
}

/*
 *  Cut the content files of the queue (not its md2): the content of a
 *  message not in memory cannot be read any more
 */
PRIVATE int cut_contents(const char *topic_name)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, topic_name, "keys", "__rowid__", NULL);
    DIR *dir = opendir(path);
    if(!dir) {
        printf("%sERROR%s --> cannot open %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    int cut = 0;
    struct dirent *de;
    while((de = readdir(dir)) != NULL) {
        size_t ln = strlen(de->d_name);
        if(ln > 5 && strcmp(de->d_name + ln - 5, ".json") == 0) {
            char file[PATH_MAX];
            build_path(file, sizeof(file), path, de->d_name, NULL);
            if(truncate(file, 0) == 0) {
                cut++;
            }
        }
    }
    closedir(dir);
    if(cut == 0) {
        printf("%sERROR%s --> no content file cut in %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    return 0;
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
 *  1.  A queued message whose content cannot be read stays queued
 ***************************************************************************/
PRIVATE int test_move_unreadable(void)
{
    int result = 0;
    const char *topic_name = "tr2q_move";
    rmrdir(path_database);

    set_expected_results("move: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    tr2_queue_t *trq = tr2q_open(tranger, topic_name, "tm", 0, 1 /* max_inflight */, 0);
    tr2q_load(trq);
    tr2q_append(trq, 946684801, tr2q_kw(1, 946684801), 0);             // in flight
    q2_msg_t *queued = tr2q_append(trq, 946684802, tr2q_kw(2, 946684802), 0);  // queued
    test_json(NULL);    // the setup logs are not what is tested

    result += expect_int("move: in flight after the appends", (json_int_t)tr2q_inflight_size(trq), 1);
    result += expect_int("move: queued after the appends", (json_int_t)tr2q_queued_size(trq), 1);

    set_expected_results("move: a queued message whose content cannot be read",
        json_pack("[{s:s},{s:s}]",
            "msg", "Bad on-disk record: __offset__/__size__ out of range",
            "msg", "Cannot load the content of a queued message: it stays queued"
        ),
        NULL, NULL, 1
    );
    if(cut_contents(topic_name) < 0) {
        result += -1;
    }
    if(queued) {
        int ret = tr2q_move_from_queued_to_inflight(queued);
        result += expect_int("move: tr2q_move_from_queued_to_inflight() of an unreadable message", ret, -1);
        result += expect_int("move: the message is not in flight", queued->inflight? 1: 0, 0);
    } else {
        printf("%sERROR%s --> move: the second message was not appended\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("move: in flight after the failed move", (json_int_t)tr2q_inflight_size(trq), 1);
    result += expect_int("move: queued after the failed move", (json_int_t)tr2q_queued_size(trq), 1);
    result += test_json(NULL);

    set_expected_results("move: shutdown", NULL, NULL, NULL, 1);
    tr2q_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  2.  No backup while messages are queued
 ***************************************************************************/
PRIVATE int test_backup_with_queued(void)
{
    int result = 0;
    const char *topic_name = "tr2q_backup";
    rmrdir(path_database);

    set_expected_results("backup: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    tr2_queue_t *trq = tr2q_open(tranger, topic_name, "tm", 0, 1 /* max_inflight */, 1 /* backup_queue_size */);
    tr2q_load(trq);
    q2_msg_t *inflight = tr2q_append(trq, 946684801, tr2q_kw(1, 946684801), 0);
    q2_msg_t *queued = tr2q_append(trq, 946684802, tr2q_kw(2, 946684802), 0);
    tr2q_unload_msg(inflight, 0);   // done: nothing in flight, one queued
    test_json(NULL);    // the setup logs are not what is tested

    result += expect_int("backup: in flight", (json_int_t)tr2q_inflight_size(trq), 0);
    result += expect_int("backup: queued", (json_int_t)tr2q_queued_size(trq), 1);

    set_expected_results("backup: refused while a message is queued", NULL, NULL, NULL, 1);
    int ret = tr2q_check_backup(trq);
    result += expect_int("backup: tr2q_check_backup() with a queued message", ret, 0);
    result += expect_int("backup: the topic is not backed up",
        (json_int_t)tranger2_topic_size(tranger, topic_name), 2);
    if(!queued || !tr2q_msg_json(queued)) {
        printf("%sERROR%s --> backup: the content of the queued message is lost\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("backup: shutdown", NULL, NULL, NULL, 1);
    tr2q_close(trq);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL    // global_authentication_parser
    );

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
        NULL,
        &yev_loop
    );

    /*--------------------------------*
     *      Paths
     *--------------------------------*/
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    mkrdir(path_root, 02770);

    /*--------------------------------*
     *      Tests
     *--------------------------------*/
    int result = 0;
    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    result += test_move_unreadable();
    result += test_backup_with_queued();

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP)

    rmrdir(path_database);

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);
    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result<0?-1:0;
}
