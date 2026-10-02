/****************************************************************************
 *          test_rt_disk_unwatched_key.c
 *
 *  A follower's rt_disk feed whose KEY directory cannot be watched: the
 *  watch fails with ENOSPC (fs.inotify.max_user_watches reached; here
 *  __wrap_inotify_add_watch(), for the directory of KEY_C in the feed).
 *  Its records are linked there by the master, and nothing of them is
 *  heard while there is no watch.
 *
 *  Once watches can be made again, the next batch of the feed (a record of
 *  another key) watches the directory and hands it as created: the feed
 *  reads the records made in it meanwhile, and hears the next ones. Up to
 *  7.25.21 the directory was taken for gone and never watched: every
 *  record of the key was lost to the feed, with one ERROR.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/inotify.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_rt_disk_unwatched_key"

#define DATABASE    "tr_rt_disk_unwatched_key"
#define TOPIC_NAME  "topic_rt_disk_unwatched_key"
#define KEY_C       "0000000000000000003"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/***************************************************************
 *              Out of watches, for one directory
 ***************************************************************/
int __real_inotify_add_watch(int fd, const char *pathname, uint32_t mask);
int __wrap_inotify_add_watch(int fd, const char *pathname, uint32_t mask);
PRIVATE const char *watch_enospc_suffix = NULL;    // a watch of a path ending so fails

int __wrap_inotify_add_watch(int fd, const char *pathname, uint32_t mask)
{
    if(watch_enospc_suffix) {
        char real[PATH_MAX];
        const char *path = pathname;
        if(strncmp(pathname, "/proc/self/fd/", 14) == 0) {
            ssize_t n = readlink(pathname, real, sizeof(real) - 1);    // FS_FLAG_DIR_FDS
            if(n > 0) {
                real[n] = 0;
                path = real;
            }
        }
        size_t lp = strlen(path), ls = strlen(watch_enospc_suffix);
        if(lp >= ls && strcmp(path + lp - ls, watch_enospc_suffix) == 0) {
            errno = ENOSPC;
            return -1;
        }
    }
    return __real_inotify_add_watch(fd, pathname, mask);
}

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int count_c = 0;
PRIVATE int count_other = 0;

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
    if(strcmp(key, KEY_C) == 0) {
        count_c++;
    } else {
        count_other++;
    }
    JSON_DECREF(record)
    return 0;
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

PRIVATE void drain(int expected_c, int expected_other)
{
    for(int i = 0; i < 100; i++) {
        if(count_c >= expected_c && count_other >= expected_other) {
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
     *  Master and follower, one feed
     *-------------------------------------*/
    set_expected_results(
        "unwatched key: setup",
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
    json_t *tf = topic? startup_tranger(FALSE) : NULL;
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        if(tf) {
            tranger2_shutdown(tf);
        }
        tranger2_shutdown(tm);
        return -1;
    }
    json_t *rt_all = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL
    );
    if(!rt_all) {
        result += -1;
    }
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Out of watches: two records of a
     *  new key, not heard
     *-------------------------------------*/
    set_expected_results(
        "unwatched key: its records are read once it is watched",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot watch a directory, out of inotify watches or memory: tried again at each batch (and the next ones that fail, counted)",
            "msg", "Directories watched again: every one that could not be is watched now"
        ),
        NULL, NULL, 1
    );
    watch_enospc_suffix = "/rtALL/" KEY_C;
    append_one(tm, 3, BASE_T + 1);
    append_one(tm, 3, BASE_T + 2);
    drain(2, 0);
    if(count_c != 0) {
        printf("%sERROR%s --> unwatched key: %d records heard without a watch, expected 0\n",
            On_Red BWhite, Color_Off, count_c);
        result += -1;
    }

    /*-------------------------------------*
     *  Watches again: a record of another
     *  key is a batch, which watches it
     *-------------------------------------*/
    watch_enospc_suffix = NULL;
    append_one(tm, 4, BASE_T + 3);
    drain(2, 1);
    append_one(tm, 3, BASE_T + 4);
    drain(3, 1);
    if(count_c != 3 || count_other != 1) {
        printf("%sERROR%s --> unwatched key: %d records of the key heard (3), %d of the other (1)\n",
            On_Red BWhite, Color_Off, count_c, count_other);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Shutdown
     *-------------------------------------*/
    set_expected_results("unwatched key: shutdown", NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tf, rt_all);
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);
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
    }
    return result < 0? -1 : 0;
}
