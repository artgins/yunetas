/****************************************************************************
 *          test_lost_lock.c
 *
 *  A master gives its single-master lock back at tranger2_stop(). While it
 *  is stopped another master may take the store, and then the first one is
 *  not the master any more. It noticed that only at its next
 *  tranger2_open_topic(): tranger2_create_topic() (the restart path of
 *  C_TRANGER and C_TREEDB) read the stale `master` and wrote the topic
 *  directories, topic_cols.json and topic_var.json into the other master's
 *  store first, and tranger2_write_topic_var() / _cols() wrote with no
 *  check at all (M3 of the 2026-09-23 independent review of 7.25.4).
 *
 *  Now every write path takes the lock again FIRST. One that cannot take
 *  it writes nothing: the tranger goes on as a replica, and says so in its
 *  json, `master` false and `master_lost` true. Only another process
 *  holding the lock is the startup's conflict (a CRITICAL at
 *  on_critical_error, exit(0) with a yuno's default); any other failure to
 *  take it is an ERROR and the process goes on, a replica.
 *
 *  The three writes of a record's md2 row in place --
 *  tranger2_write_user_flag(), tranger2_set_user_flag() and
 *  tranger2_set_system_flag() -- asked nothing (M-B of the independent
 *  review of the second fix round): on a demoted master, and on any
 *  replica, they reached the write of a read-only fd, logged a CRITICAL,
 *  and with the default on_critical_error the process exited(0).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_lost_lock"
#define DATABASE    "tr_lost_lock"
#define TOPIC_NAME  "topic_lost_lock"
#define DAY1        946684800   // 2000-01-01

extern void jsonp_free(void *ptr);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char path_database[PATH_MAX];

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *startup_master(void)
{
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y-%m-%d",
        "xpermission" , 02770,
        "rpermission", 0600
    );
    return tranger2_startup(0, jn_tranger, 0);
}

PRIVATE json_t *create_topic(json_t *tranger, const char *topic_name, json_int_t version)
{
    return tranger2_create_topic(
        tranger, topic_name, "id", "tm", NULL, sf_string_key,
        json_pack("{s:s, s:I, s:s}",
            "id", "",
            "tm", (json_int_t)0,
            version > 1? "new_col": "content", ""
        ),
        json_pack("{s:I}", "topic_version", version)
    );
}

/*
 *  The file of the topic, as a compact json string (static buffer)
 */
PRIVATE const char *file_of(const char *topic_name, const char *filename)
{
    static char bf[512];
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, topic_name, filename, NULL);
    json_t *jn = json_load_file(path, 0, 0);
    char *s = jn? json_dumps(jn, JSON_COMPACT|JSON_SORT_KEYS): NULL;
    snprintf(bf, sizeof(bf), "%s", s? s: "(none)");
    jsonp_free(s);
    JSON_DECREF(jn)
    return bf;
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

PRIVATE int expect_bool(const char *what, BOOL got, BOOL expected)
{
    return expect(what, got? "true": "false", expected? "true": "false");
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
/***************************************************************************
 *  A revive that finds the store taken: the startup's single-master guard
 *  (the level and the exit are the same knob, on_critical_error), and a
 *  demoted master that stays a replica
 ***************************************************************************/
PRIVATE int test_revive_conflict(void)
{
    int result = 0;
    rmrdir(path_database);
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);

    /*-------------------------------------*
     *  With the default of a yuno (exit(0)
     *  on a critical) the revive ends the
     *  process, as the startup does
     *-------------------------------------*/
    set_expected_results("lost lock: a revive with exit on critical", NULL, NULL, NULL, 1);
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        gobj_log_del_handler("test_capture");
        json_t *jn = json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_EXIT_ZERO
        );
        json_t *x = tranger2_startup(0, json_incref(jn), 0);
        if(!x || !create_topic(x, TOPIC_NAME, 1)) {
            _exit(4);
        }
        tranger2_stop(x);
        json_t *y = tranger2_startup(0, jn, 0);   // takes the store
        if(!y) {
            _exit(5);
        }
        tranger2_write_topic_var(x, TOPIC_NAME, json_pack("{s:I}", "last_rowid_id", (json_int_t)1));
        _exit(3);   // survived the revive
    }
    int status = 0;
    waitpid(pid, &status, 0);
    char how[32];
    snprintf(how, sizeof(how), "%s %d",
        WIFEXITED(status)? "exit": "signal",
        WIFEXITED(status)? WEXITSTATUS(status): WTERMSIG(status));
    result += expect("a revive that finds the store taken exits like the startup", how, "exit 0");
    result += test_json(NULL);

    /*-------------------------------------*
     *  Without exit: a demoted master stays
     *  a replica, even once the lock is
     *  free again
     *-------------------------------------*/
    rmrdir(path_database);
    set_expected_results(
        "lost lock: a demoted master stays a replica",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic",
            "msg", "Master lock NOT retaken after a stop: another process holds it, go on as not master",
            "msg", "Only master can write",
            "msg", "Only master can write"
        ),
        NULL, NULL, 1
    );
    json_t *a = startup_master();
    create_topic(a, TOPIC_NAME, 1);
    tranger2_stop(a);
    json_t *b = startup_master();
    tranger2_write_topic_var(a, TOPIC_NAME, json_pack("{s:I}", "last_rowid_id", (json_int_t)1));
    tranger2_shutdown(b);           // the lock is free
    tranger2_stop(a);
    tranger2_write_topic_var(a, TOPIC_NAME, json_pack("{s:I}", "last_rowid_id", (json_int_t)2));
    result += expect_bool("the demoted master is still not the master",
        kw_get_bool(0, a, "master", 0, 0), FALSE);
    tranger2_shutdown(a);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  A revive that fails for another reason than the store being TAKEN:
 *  the lock file cannot be opened (here it is gone; EMFILE, ENOLCK, EINTR
 *  are the same case). Nobody else owns the store, so it is not the
 *  startup's single-master conflict: an ERROR, the tranger goes on as a
 *  replica, and the process does NOT exit -- with a yuno's default
 *  (LOG_OPT_EXIT_ZERO) it exited(0) and was not relaunched (independent
 *  review of the third fix round).
 ***************************************************************************/
PRIVATE int test_revive_other_failure(void)
{
    int result = 0;
    rmrdir(path_database);
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    char lock_file[PATH_MAX];
    char lock_away[PATH_MAX];
    build_path(lock_file, sizeof(lock_file), path_database, "__timeranger2__.json", NULL);
    build_path(lock_away, sizeof(lock_away), path_database, "__timeranger2__.json.away", NULL);

    /*-------------------------------------*
     *  With the default of a yuno: it
     *  must survive
     *-------------------------------------*/
    set_expected_results("lost lock: a revive that cannot open the lock, exit on critical", NULL, NULL, NULL, 1);
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        gobj_log_del_handler("test_capture");
        json_t *x = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_EXIT_ZERO
        ), 0);
        if(!x || !create_topic(x, TOPIC_NAME, 1)) {
            _exit(4);
        }
        tranger2_stop(x);
        if(rename(lock_file, lock_away) < 0) {
            _exit(5);
        }
        tranger2_write_topic_var(x, TOPIC_NAME, json_pack("{s:I}", "last_rowid_id", (json_int_t)1));
        _exit(kw_get_bool(0, x, "master_lost", 0, 0)? 3: 6);   // survived, and demoted
    }
    int status = 0;
    waitpid(pid, &status, 0);
    char how[32];
    snprintf(how, sizeof(how), "%s %d",
        WIFEXITED(status)? "exit": "signal",
        WIFEXITED(status)? WEXITSTATUS(status): WTERMSIG(status));
    result += expect("a revive that cannot open the lock survives, demoted", how, "exit 3");
    if(rename(lock_away, lock_file) < 0) {
        printf("%sERROR%s --> cannot put the lock file back\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Without exit: an ERROR, not the
     *  critical of the conflict, and a
     *  replica
     *-------------------------------------*/
    rmrdir(path_database);
    set_expected_results(
        "lost lock: a revive that cannot open the lock",
        json_pack("[{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic",
            "msg", "Master lock NOT retaken after a stop: cannot open the lock file, go on as not master",
            "msg", "Only master can write"
        ),
        NULL, NULL, 1
    );
    json_t *a = startup_master();
    create_topic(a, TOPIC_NAME, 1);
    tranger2_stop(a);
    if(rename(lock_file, lock_away) < 0) {
        printf("%sERROR%s --> cannot move the lock file away\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    tranger2_write_topic_var(a, TOPIC_NAME, json_pack("{s:I}", "last_rowid_id", (json_int_t)1));
    result += expect_bool("it is not the master", kw_get_bool(0, a, "master", 0, 0), FALSE);
    result += expect_bool("it says it lost the lock", kw_get_bool(0, a, "master_lost", 0, 0), TRUE);
    if(rename(lock_away, lock_file) < 0) {
        printf("%sERROR%s --> cannot put the lock file back\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    tranger2_shutdown(a);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  The three rewrites of a md2 row, by one that is not the master
 ***************************************************************************/
PRIVATE int try_md2_rewrites(json_t *tranger, const char *who)
{
    int result = 0;
    char what[128];
    snprintf(what, sizeof(what), "%s: tranger2_set_user_flag() is refused", who);
    result += expect(what,
        tranger2_set_user_flag(tranger, TOPIC_NAME, "k", DAY1, 1, 0x1, TRUE) < 0? "refused": "written",
        "refused"
    );
    snprintf(what, sizeof(what), "%s: tranger2_write_user_flag() is refused", who);
    result += expect(what,
        tranger2_write_user_flag(tranger, TOPIC_NAME, "k", DAY1, 1, 0x7) < 0? "refused": "written",
        "refused"
    );
    snprintf(what, sizeof(what), "%s: tranger2_set_system_flag() is refused", who);
    result += expect(what,
        tranger2_set_system_flag(tranger, TOPIC_NAME, "k", DAY1, 1, sf_immutable_record, TRUE) < 0?
            "refused": "written",
        "refused"
    );
    return result;
}

PRIVATE int test_md2_rewrites(void)
{
    int result = 0;
    rmrdir(path_database);

    /*-------------------------------------*
     *  D writes a record, stops, E takes
     *  the store
     *-------------------------------------*/
    set_expected_results(
        "lost lock: md2 rewrites, setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *d = startup_master();
    if(!d || !create_topic(d, TOPIC_NAME, 1)) {
        printf("%sERROR%s --> cannot create the store\n", On_Red BWhite, Color_Off);
        return -1;
    }
    md2_record_ex_t md = {0};
    tranger2_append_record(d, TOPIC_NAME, DAY1, 0, &md,
        json_pack("{s:s, s:I, s:s}", "id", "k", "tm", (json_int_t)DAY1, "content", "R1")
    );
    tranger2_stop(d);
    json_t *e = startup_master();
    result += expect_bool("E is the master", kw_get_bool(0, e, "master", 0, 0), TRUE);
    result += test_json(NULL);

    /*-------------------------------------*
     *  D, demoted: every rewrite refused
     *-------------------------------------*/
    set_expected_results(
        "lost lock: md2 rewrites of a demoted master",
        json_pack("[{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Master lock NOT retaken after a stop: another process holds it, go on as not master",
            "msg", "Only master can write",
            "msg", "Only master can write",
            "msg", "Only master can write"
        ),
        NULL, NULL, 1
    );
    result += try_md2_rewrites(d, "demoted master");
    tranger2_shutdown(d);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A replica of E's store: the same
     *-------------------------------------*/
    set_expected_results(
        "lost lock: md2 rewrites of a replica",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Only master can write",
            "msg", "Only master can write",
            "msg", "Only master can write"
        ),
        NULL, NULL, 1
    );
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    json_t *r = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 0,
        "on_critical_error", LOG_OPT_TRACE_STACK
    ), 0);
    if(!r || !tranger2_open_topic(r, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> cannot open the replica\n", On_Red BWhite, Color_Off);
        result += -1;
    } else {
        result += try_md2_rewrites(r, "replica");
    }
    char flag[16];
    snprintf(flag, sizeof(flag), "%u",
        (unsigned)tranger2_read_user_flag(e, TOPIC_NAME, "k", DAY1, 1));
    result += expect("the user_flag on disk is untouched", flag, "0");
    result += test_json(NULL);

    /*-------------------------------------*
     *  And a replica configured with the
     *  default of a yuno, exit(0) on a
     *  critical: it must survive the call
     *-------------------------------------*/
    set_expected_results("lost lock: a replica with exit on critical", NULL, NULL, NULL, 1);
    fflush(stdout);
    pid_t pid = fork();
    if(pid == 0) {
        gobj_log_del_handler("test_capture");
        json_t *x = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 0,
            "on_critical_error", LOG_OPT_EXIT_ZERO
        ), 0);
        if(x && tranger2_open_topic(x, TOPIC_NAME, FALSE)) {
            tranger2_set_user_flag(x, TOPIC_NAME, "k", DAY1, 1, 0x1, TRUE);
            _exit(3);   // survived the call
        }
        _exit(4);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    char how[32];
    snprintf(how, sizeof(how), "%s %d",
        WIFEXITED(status)? "exit": "signal",
        WIFEXITED(status)? WEXITSTATUS(status): WTERMSIG(status));
    result += expect("a replica with exit on critical survives set_user_flag", how, "exit 3");
    result += test_json(NULL);

    set_expected_results("lost lock: md2 rewrites, shutdown", NULL, NULL, NULL, 1);
    if(r) {
        tranger2_shutdown(r);
    }
    tranger2_shutdown(e);
    result += test_json(NULL);

    return result;
}

PRIVATE int do_test(void)
{
    int result = 0;
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE, NULL);
    rmrdir(path_database);

    /*-------------------------------------*
     *  A is the master, it stops,
     *  B takes the store
     *-------------------------------------*/
    set_expected_results(
        "lost lock: A stops, B takes the store",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *a = startup_master();
    if(!a || !create_topic(a, TOPIC_NAME, 1)) {
        printf("%sERROR%s --> cannot create the store\n", On_Red BWhite, Color_Off);
        return -1;
    }
    tranger2_stop(a);
    json_t *b = startup_master();
    result += expect_bool("B is the master", kw_get_bool(0, b, "master", 0, 0), TRUE);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A's first call after the stop is a
     *  write: it writes nothing
     *-------------------------------------*/
    set_expected_results(
        "lost lock: a write is the first call after the stop",
        json_pack("[{s:s},{s:s}]",
            "msg", "Master lock NOT retaken after a stop: another process holds it, go on as not master",
            "msg", "Only master can write"
        ),
        NULL, NULL, 1
    );
    result += expect("tranger2_write_topic_var() is refused",
        tranger2_write_topic_var(a, TOPIC_NAME, json_pack("{s:I}", "last_rowid_id", (json_int_t)99))<0?
            "refused": "written",
        "refused"
    );
    result += expect("topic_var.json is B's",
        file_of(TOPIC_NAME, "topic_var.json"), "{\"topic_version\":1}");
    result += expect_bool("A is not the master", kw_get_bool(0, a, "master", 0, 0), FALSE);
    result += expect_bool("A says it lost the lock", kw_get_bool(0, a, "master_lost", 0, 0), TRUE);
    result += test_json(NULL);

    /*-------------------------------------*
     *  ... and create_topic writes nothing
     *  either, and opens what exists as a
     *  replica would
     *-------------------------------------*/
    set_expected_results(
        "lost lock: create_topic after it",
        json_pack("[{s:s}]",
            "msg", "Cannot open TimeRanger topic. Not found and no master"
        ),
        NULL, NULL, 1
    );
    json_t *topic = create_topic(a, TOPIC_NAME, 2);
    result += expect_bool("the existing topic opens, read only", topic? TRUE: FALSE, TRUE);
    result += expect("topic_var.json is still B's",
        file_of(TOPIC_NAME, "topic_var.json"), "{\"topic_version\":1}");
    result += expect("topic_cols.json is still B's",
        file_of(TOPIC_NAME, "topic_cols.json"), "{\"content\":\"\",\"id\":\"\",\"tm\":0}");
    result += expect_bool("no new topic in B's store",
        create_topic(a, "topic_of_a", 1)? TRUE: FALSE, FALSE);
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, "topic_of_a", NULL);
    result += expect_bool("no directory of a new topic", is_directory(path), FALSE);
    tranger2_shutdown(a);
    result += test_json(NULL);

    /*-------------------------------------*
     *  The same with create_topic as the
     *  FIRST call after the stop, the
     *  restart path of C_TRANGER
     *-------------------------------------*/
    set_expected_results(
        "lost lock: create_topic is the first call after the stop",
        json_pack("[{s:s},{s:s}]",
            "msg", "Master lock NOT retaken after a stop: another process holds it, go on as not master",
            "msg", "Cannot open TimeRanger topic. Not found and no master"
        ),
        NULL, NULL, 1
    );
    tranger2_stop(b);
    json_t *c = startup_master();
    result += expect_bool("C is the master", kw_get_bool(0, c, "master", 0, 0), TRUE);
    result += expect_bool("B creates no topic in C's store",
        create_topic(b, "topic_of_b", 1)? TRUE: FALSE, FALSE);
    build_path(path, sizeof(path), path_database, "topic_of_b", NULL);
    result += expect_bool("no directory of B's topic", is_directory(path), FALSE);
    result += expect_bool("B is not the master", kw_get_bool(0, b, "master", 0, 0), FALSE);
    tranger2_shutdown(b);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A master that gets its lock back is
     *  the master again, as before
     *-------------------------------------*/
    set_expected_results(
        "lost lock: the lock taken back",
        json_pack("[{s:s},{s:s}]",
            "msg", "Re-Creating topic_var.json",
            "msg", "Re-Creating topic_cols.json"
        ),
        NULL, NULL, 1
    );
    tranger2_stop(c);
    topic = create_topic(c, TOPIC_NAME, 3);
    result += expect_bool("C is the master again", kw_get_bool(0, c, "master", 0, 0), TRUE);
    result += expect_bool("C did not lose it", kw_get_bool(0, c, "master_lost", 0, 0), FALSE);
    result += expect("C's version is written",
        file_of(TOPIC_NAME, "topic_var.json"), "{\"topic_version\":3}");
    tranger2_shutdown(c);
    result += test_json(NULL);

    result += test_md2_rewrites();
    result += test_revive_conflict();
    result += test_revive_other_failure();

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
