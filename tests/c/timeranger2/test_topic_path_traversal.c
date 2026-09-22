/****************************************************************************
 *          test_topic_path_traversal.c
 *
 *  Regression coverage for the topic-name confinement and the master guard
 *  of the destructive topic calls at the timeranger2 boundary. A topic name
 *  becomes ONE directory component under the database, like a key under
 *  keys/ (see test_pkey_path_traversal.c for the key side):
 *
 *      - tranger2_create_topic / open_topic / delete_topic / backup_topic /
 *        topic_path / write_topic_var / write_topic_cols REJECT an empty
 *        name, ".", "..", and any name holding '/' or '`', with one error
 *        log and no filesystem effect: "../<other db>/<topic>" must not
 *        delete another database's topic.
 *      - A leading '.' is NOT refused: MQTT queues are "<client_id>-IN/-OUT"
 *        and the broker accepts a client_id such as ".foo". "a..b" is one
 *        component and cannot escape either.
 *      - tranger2_open_topic on a directory that is not a topic (no
 *        topic_desc.json) answers NULL with an error, not a critical: with
 *        on_critical_error=2 that critical was an exit(0) of the yuno.
 *      - A replica (master=0) cannot delete or back up a topic.
 *      - The id of a disk feed (tranger2_open_rt_disk, the rt_id of a
 *        follower's open-rt / open-list) is a directory component too,
 *        `<topic>/disks/<id>/`, that the follower rmrdir()s before creating
 *        it: the same rule refuses it, and "../../<dir>" removes nothing.
 *        The follower runs WITH a loop (without one it never touches the
 *        directory) and the victim is where "../../" lands, the database
 *        directory: against the unguarded library the victim is removed.
 *        An id longer than NAME_MAX is refused too (the mkdir failed and
 *        the feed answered "opened"). A refused id is a peer's input: a
 *        WARNING, no stack.
 *
 *  The negative assertions FAIL against the unguarded library: the traversal
 *  name deletes the other database's topic, "." / ".." / the non-topic dir
 *  reach load_persistent_json() as a critical, and the replica deletes the
 *  master's topic.
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

#define APP             "test_topic_path_traversal"
#define DATABASE        "tr_topic_path_traversal"
#define OTHER_DATABASE  "tr_topic_path_traversal_other"
#define TOPIC_NAME      "topic_path_traversal"
#define OTHER_TOPIC     "secrets"
#define NOT_A_TOPIC     "not_a_topic"
#define DOT_TOPIC       ".dot_topic"
#define DOTDOT_TOPIC    "a..b"
#define ESCAPE_TOPIC    "../" OTHER_DATABASE "/" OTHER_TOPIC

#define INVALID_TOPIC_MSG   "Invalid topic name (path metacharacters not allowed)"
#define INVALID_RT_ID_MSG   "Invalid rt id (path metacharacters not allowed)"
#define LONG_RT_ID_MSG      "Invalid rt id (longer than NAME_MAX)"
#define VICTIM_DIR          "victim_of_rt_id"
#define ESCAPE_RT_ID        "../../" VICTIM_DIR

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int rt_disk_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    json_t *jn_record
)
{
    JSON_DECREF(jn_record)
    return 0;
}

PRIVATE json_t *startup_tranger(const char *path_root, const char *database, BOOL master)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i, s:I}",
        "path", path_root,
        "database", database,
        "master", master,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600,
        "yev_loop", (json_int_t)0
    );
    return tranger2_startup(0, jn_tranger, 0);
}

/*
 *  A follower that watches the disk: only one with a loop creates (and
 *  first removes) the directory of a disk feed.
 */
PRIVATE json_t *startup_follower_with_loop(const char *path_root, const char *database)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i}",
        "path", path_root,
        "database", database,
        "master", 0,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600
    );
    return tranger2_startup(0, jn_tranger, yev_loop);
}

PRIVATE void drain(int turns)
{
    for(int i = 0; i < turns; i++) {
        yev_loop_run_once(yev_loop);
    }
}

PRIVATE json_t *create_topic(json_t *tranger, const char *topic_name)
{
    return tranger2_create_topic(
        tranger,
        topic_name,
        "id",
        "tm",
        json_pack("{s:i, s:s, s:i, s:i}",
            "on_critical_error", LOG_OPT_TRACE_STACK,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        sf_string_key,
        json_pack("{s:s, s:I, s:s}",
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
}

PRIVATE json_t *expected_invalid(int n)
{
    json_t *list = json_array();
    for(int i = 0; i < n; i++) {
        json_array_append_new(list, json_pack("{s:s}", "msg", INVALID_TOPIC_MSG));
    }
    return list;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    char path_other_database[PATH_MAX], path_other_topic[PATH_MAX];
    char path_not_a_topic[PATH_MAX];

    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    build_path(path_topic, sizeof(path_topic), path_database, TOPIC_NAME, NULL);
    build_path(path_other_database, sizeof(path_other_database), path_root, OTHER_DATABASE, NULL);
    build_path(path_other_topic, sizeof(path_other_topic), path_other_database, OTHER_TOPIC, NULL);
    build_path(path_not_a_topic, sizeof(path_not_a_topic), path_database, NOT_A_TOPIC, NULL);
    rmrdir(path_database);
    rmrdir(path_other_database);

    /*-------------------------------------*
     *  Setup: another database with a real
     *  topic, the one the traversal aims at
     *-------------------------------------*/
    set_expected_results(
        "setup other database",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *other = startup_tranger(path_root, OTHER_DATABASE, TRUE);
    if(!other || !create_topic(other, OTHER_TOPIC)) {
        if(other) {
            tranger2_shutdown(other);
        }
        return -1;
    }
    tranger2_shutdown(other);
    result += test_json(NULL);

    /*-------------------------------------*
     *  Setup: the database under test
     *-------------------------------------*/
    set_expected_results(
        "setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup_tranger(path_root, DATABASE, TRUE);
    if(!tranger || !create_topic(tranger, TOPIC_NAME)) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    mkrdir(path_not_a_topic, 02770);
    result += test_json(NULL);

    const char *bad_names[] = {"", ".", "..", ESCAPE_TOPIC, "a/b", TOPIC_NAME "/keys", "a`b", NULL};
    int n_bad = 0;
    while(bad_names[n_bad]) {
        n_bad++;
    }

    /*-------------------------------------*
     *  Negative: delete_topic must not reach
     *  the other database
     *-------------------------------------*/
    set_expected_results(
        "negative: delete_topic rejects path-traversal names",
        expected_invalid(n_bad),
        NULL, NULL, 1
    );
    for(int i = 0; bad_names[i]; i++) {
        if(tranger2_delete_topic(tranger, bad_names[i]) >= 0) {
            printf("%sERROR%s --> delete_topic accepted topic name '%s'\n",
                On_Red BWhite, Color_Off, bad_names[i]);
            result += -1;
        }
    }
    if(!is_directory(path_other_topic)) {
        printf("%sERROR%s --> the other database's topic was deleted: %s\n",
            On_Red BWhite, Color_Off, path_other_topic);
        result += -1;
    }
    if(!is_directory(path_topic)) {
        printf("%sERROR%s --> the topic under test was deleted: %s\n",
            On_Red BWhite, Color_Off, path_topic);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Negative: open_topic rejects them,
     *  with no critical on the way
     *-------------------------------------*/
    set_expected_results(
        "negative: open_topic rejects path-traversal names",
        expected_invalid(n_bad),
        NULL, NULL, 1
    );
    for(int i = 0; bad_names[i]; i++) {
        if(tranger2_open_topic(tranger, bad_names[i], TRUE)) {
            printf("%sERROR%s --> open_topic accepted topic name '%s'\n",
                On_Red BWhite, Color_Off, bad_names[i]);
            result += -1;
        }
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Negative: create_topic plants nothing
     *  outside the database
     *-------------------------------------*/
    set_expected_results(
        "negative: create_topic rejects path-traversal names",
        expected_invalid(n_bad),
        NULL, NULL, 1
    );
    for(int i = 0; bad_names[i]; i++) {
        if(create_topic(tranger, bad_names[i])) {
            printf("%sERROR%s --> create_topic accepted topic name '%s'\n",
                On_Red BWhite, Color_Off, bad_names[i]);
            result += -1;
        }
    }
    char path_planted[PATH_MAX];
    build_path(path_planted, sizeof(path_planted), path_other_database, "a`b", NULL);
    if(is_directory(path_planted)) {
        printf("%sERROR%s --> create_topic planted a directory: %s\n",
            On_Red BWhite, Color_Off, path_planted);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Negative: the rest of the boundary
     *-------------------------------------*/
    set_expected_results(
        "negative: topic_path, write_topic_var/cols, backup_topic reject them",
        expected_invalid(4),
        NULL, NULL, 1
    );
    char bf[PATH_MAX];
    if(tranger2_topic_path(bf, sizeof(bf), tranger, ESCAPE_TOPIC) >= 0) {
        printf("%sERROR%s --> topic_path accepted '%s'\n", On_Red BWhite, Color_Off, ESCAPE_TOPIC);
        result += -1;
    }
    if(tranger2_write_topic_var(tranger, ESCAPE_TOPIC, json_object()) >= 0) {
        printf("%sERROR%s --> write_topic_var accepted '%s'\n", On_Red BWhite, Color_Off, ESCAPE_TOPIC);
        result += -1;
    }
    if(tranger2_write_topic_cols(tranger, ESCAPE_TOPIC, json_object()) >= 0) {
        printf("%sERROR%s --> write_topic_cols accepted '%s'\n", On_Red BWhite, Color_Off, ESCAPE_TOPIC);
        result += -1;
    }
    if(tranger2_backup_topic(tranger, ESCAPE_TOPIC, "", "", TRUE, 0)) {
        printf("%sERROR%s --> backup_topic accepted '%s'\n", On_Red BWhite, Color_Off, ESCAPE_TOPIC);
        result += -1;
    }
    if(!is_directory(path_other_topic)) {
        printf("%sERROR%s --> the other database's topic was moved: %s\n",
            On_Red BWhite, Color_Off, path_other_topic);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Negative: a directory that is not a
     *  topic is an error, not a critical
     *-------------------------------------*/
    set_expected_results(
        "negative: open_topic on a directory that is not a topic",
        json_pack("[{s:s}]",
            "msg", "Not a topic: topic_desc.json not found"
        ),
        NULL, NULL, 1
    );
    if(tranger2_open_topic(tranger, NOT_A_TOPIC, TRUE)) {
        printf("%sERROR%s --> open_topic opened a directory that is not a topic\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Positive: a leading '.' and an inner
     *  ".." are single components
     *-------------------------------------*/
    set_expected_results(
        "positive: '.dot_topic' and 'a..b' are legit topic names",
        json_pack("[{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Deleting topic",
            "msg", "Deleting topic"
        ),
        NULL, NULL, 1
    );
    const char *good_names[] = {DOT_TOPIC, DOTDOT_TOPIC, NULL};
    for(int i = 0; good_names[i]; i++) {
        if(!create_topic(tranger, good_names[i])) {
            printf("%sERROR%s --> create_topic rejected legit topic name '%s'\n",
                On_Red BWhite, Color_Off, good_names[i]);
            result += -1;
        }
    }
    for(int i = 0; good_names[i]; i++) {
        if(tranger2_delete_topic(tranger, good_names[i]) < 0) {
            printf("%sERROR%s --> delete_topic rejected legit topic name '%s'\n",
                On_Red BWhite, Color_Off, good_names[i]);
            result += -1;
        }
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Negative: a replica cannot delete or
     *  back up the master's topic
     *-------------------------------------*/
    set_expected_results(
        "negative: a replica cannot delete or back up a topic",
        json_pack("[{s:s},{s:s}]",
            "msg", "Only master can delete",
            "msg", "Only master can back up"
        ),
        NULL, NULL, 1
    );
    json_t *replica = startup_tranger(path_root, DATABASE, FALSE);
    if(!replica) {
        printf("%sERROR%s --> cannot start the replica\n", On_Red BWhite, Color_Off);
        result += -1;
    } else {
        if(tranger2_delete_topic(replica, TOPIC_NAME) >= 0) {
            printf("%sERROR%s --> a replica deleted topic '%s'\n",
                On_Red BWhite, Color_Off, TOPIC_NAME);
            result += -1;
        }
        if(tranger2_backup_topic(replica, TOPIC_NAME, "", "", TRUE, 0)) {
            printf("%sERROR%s --> a replica backed up topic '%s'\n",
                On_Red BWhite, Color_Off, TOPIC_NAME);
            result += -1;
        }
        tranger2_shutdown(replica);
    }
    if(!is_directory(path_topic)) {
        printf("%sERROR%s --> the replica removed the master's topic: %s\n",
            On_Red BWhite, Color_Off, path_topic);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Negative: the rt id of a disk feed is
     *  a directory component too, and the
     *  follower rmrdir()s it before creating
     *  it: "../../<dir>" must not leave the
     *  topic's disks/
     *-------------------------------------*/
    set_expected_results(
        "negative: a follower's rt id cannot escape the topic",
        json_pack("[{s:s},{s:s}]",
            "msg", INVALID_RT_ID_MSG,
            "msg", LONG_RT_ID_MSG
        ),
        NULL, NULL, 1
    );
    /*
     *  "../../" from <database>/<topic>/disks/ is <database>/: the victim
     *  is put where the traversal lands.
     */
    char path_victim[PATH_MAX], path_victim_file[PATH_MAX];
    build_path(path_victim, sizeof(path_victim), path_database, VICTIM_DIR, NULL);
    build_path(path_victim_file, sizeof(path_victim_file), path_victim, "keep.txt", NULL);
    rmrdir(path_victim);
    mkrdir(path_victim, 02770);
    save_json_to_file(0, path_victim, "keep.txt", 0660, 0, 0, TRUE, TRUE, json_object());
    char long_rt_id[NAME_MAX + 2];
    memset(long_rt_id, 'x', sizeof(long_rt_id) - 1);
    long_rt_id[sizeof(long_rt_id) - 1] = 0;

    json_t *follower = startup_follower_with_loop(path_root, DATABASE);
    if(!follower) {
        printf("%sERROR%s --> cannot start the follower\n", On_Red BWhite, Color_Off);
        result += -1;
    } else {
        const char *bad_ids[] = {ESCAPE_RT_ID, long_rt_id, NULL};
        gobj_log_clear_counters();
        for(int i = 0; bad_ids[i]; i++) {
            json_t *rt = tranger2_open_rt_disk(
                follower,
                TOPIC_NAME,
                "",
                NULL,
                rt_disk_callback,
                bad_ids[i],
                "",
                NULL
            );
            drain(10);
            if(rt) {
                printf("%sERROR%s --> a follower opened a disk feed with rt id '%.40s...'\n",
                    On_Red BWhite, Color_Off, bad_ids[i]);
                tranger2_close_rt_disk(follower, rt);
                drain(10);
                result += -1;
            }
        }
        json_t *counters = gobj_get_log_data();
        if(kw_get_int(0, counters, "warning", 0, 0) != 2 ||
                kw_get_int(0, counters, "error", 0, 0) != 0) {
            printf("%sERROR%s --> a refused rt id must be ONE warning each, no error: %s\n",
                On_Red BWhite, Color_Off, json_dumps(counters, JSON_COMPACT));
            result += -1;
        }
        JSON_DECREF(counters)
        tranger2_shutdown(follower);
        drain(10);
    }
    if(!is_regular_file(path_victim_file)) {
        printf("%sERROR%s --> the rt id escaped the topic and removed: %s\n",
            On_Red BWhite, Color_Off, path_victim);
        result += -1;
    }
    rmrdir(path_victim);
    result += test_json(NULL);

    /*-------------------------------------*
     *  Shutdown
     *-------------------------------------*/
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
