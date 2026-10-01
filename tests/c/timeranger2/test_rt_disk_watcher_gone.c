/****************************************************************************
 *          test_rt_disk_watcher_gone.c
 *
 *  The watcher of a follower's rt_disk feed GOES on its own: its read fails
 *  (here the inotify fd is replaced, under the watcher, by a directory: the
 *  next read answers EISDIR), and fs_watcher destroys it. Up to 7.25.20
 *  nobody was told: the feed kept the pointer, and closing the feed
 *  stopped the freed watcher again; with the deletes owed counted, the next
 *  delete heard by another feed of the topic also asked that freed watcher
 *  where its events end.
 *
 *  Freed memory is poisoned (a small allocator below, the technique of
 *  tests/c/emailsender/poison_alloc.c): a read after the free reads the
 *  poison every run, instead of a block given out again that still looks
 *  right.
 *
 *  Now the owner is told (FS_WATCHER_GONE_TYPE) before the watcher goes:
 *  the feed drops its watcher, says it is deaf, and the readers skip it.
 *
 *  And the read of a watcher canceled by another than its owner
 *  (do_test_read_canceled_by_another): here yev_stop_event() on the
 *  watcher's read event, from outside fs_watcher. In a yuno no such cancel
 *  comes: yev_loop_stop() cancels every operation, but it is called after
 *  every owner stopped its watcher (gobj_end()), and a loop run after it
 *  delivers nothing behind its own completion (the loop breaks there and
 *  leaves it at the head of the ring). Coming anyway it is an order
 *  broken: logged as an error, and the owner is told -- closing the feed
 *  and the shutdown afterwards touch no freed watcher. Up to 7.25.20 the
 *  watcher went silently there too.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>
#include <fs_watcher.h>

#define APP "test_rt_disk_watcher_gone"

#define DATABASE    "tr_rt_disk_watcher_gone"
#define TOPIC_NAME  "topic_rt_disk_watcher_gone"
#define SEED_KEY    "0000000000000000000"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/***************************************************************
 *              Poisoned free
 ***************************************************************/
#define POISON_BYTE     0x5A
#define QUARANTINE_SIZE 4096

typedef struct {
    size_t size;
    size_t pad;     // keeps the block 16-aligned
} poison_hdr_t;

PRIVATE sys_malloc_fn_t base_malloc;
PRIVATE sys_realloc_fn_t base_realloc;
PRIVATE sys_calloc_fn_t base_calloc;
PRIVATE sys_free_fn_t base_free;
PRIVATE poison_hdr_t *quarantine[QUARANTINE_SIZE];
PRIVATE size_t quarantine_idx = 0;
PRIVATE BOOL quarantine_closed = FALSE;

PRIVATE void *poison_malloc(size_t size)
{
    poison_hdr_t *hdr = base_malloc(sizeof(poison_hdr_t) + size);
    if(!hdr) {
        return NULL;
    }
    memset(hdr + 1, 0, size);
    hdr->size = size;
    return hdr + 1;
}

PRIVATE void poison_free(void *p)
{
    if(!p) {
        return;
    }
    poison_hdr_t *hdr = ((poison_hdr_t *)p) - 1;
    memset(p, POISON_BYTE, hdr->size);
    if(quarantine_closed) {
        base_free(hdr);
        return;
    }
    poison_hdr_t *old = quarantine[quarantine_idx];
    quarantine[quarantine_idx] = hdr;
    quarantine_idx = (quarantine_idx + 1) % QUARANTINE_SIZE;
    if(old) {
        base_free(old);
    }
}

PRIVATE void *poison_realloc(void *p, size_t size)
{
    if(!p) {
        return poison_malloc(size);
    }
    poison_hdr_t *hdr = ((poison_hdr_t *)p) - 1;
    void *np = poison_malloc(size);
    if(!np) {
        return NULL;
    }
    memcpy(np, p, hdr->size < size? hdr->size : size);
    poison_free(p);
    return np;
}

PRIVATE void *poison_calloc(size_t n, size_t size)
{
    return poison_malloc(n * size);
}

PRIVATE void release_quarantine(void)
{
    quarantine_closed = TRUE;
    for(size_t i=0; i<QUARANTINE_SIZE; i++) {
        if(quarantine[i]) {
            base_free(quarantine[i]);
            quarantine[i] = NULL;
        }
    }
}

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int records = 0;
PRIVATE int deleted_all = 0;

PRIVATE int record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    records++;
    JSON_DECREF(record)
    return 0;
}

PRIVATE int key_deleted_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    void *user_data
)
{
    deleted_all++;
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

PRIVATE int append_one(json_t *tranger, uint64_t t)
{
    json_t *jn_record = json_pack("{s:I, s:I, s:s}",
        "id", (json_int_t)0,
        "tm", (json_int_t)t,
        "content", "payload"
    );
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, t, 0, &md, jn_record);
}

PRIVATE void drain(int turns)
{
    for(int i = 0; i < turns; i++) {
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
    build_path(path_database, sizeof(path_database), getenv("HOME"), "tests_yuneta", DATABASE, NULL);
    rmrdir(path_database);

    set_expected_results(
        "watcher gone: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(TRUE);
    if(!tm || !tranger2_create_topic(
        tm, TOPIC_NAME, "id", "tm",
        json_pack("{s:i, s:s, s:i, s:i}",
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        sf_int_key,
        json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
        0)) {
        tranger2_shutdown(tm);
        return -1;
    }
    if(append_one(tm, BASE_T)<0) {
        result += -1;
    }
    json_t *tf = startup_tranger(FALSE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        return -1;
    }
    json_t *rt_all = tranger2_open_rt_disk(tf, TOPIC_NAME, "", NULL, record_callback, "rtALL", "", NULL);
    json_t *rt_deaf = tranger2_open_rt_disk(tf, TOPIC_NAME, "", NULL, record_callback, "rtDEAF", "", NULL);
    if(!rt_all || !rt_deaf) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        return -1;
    }
    tranger2_set_rt_key_deleted_callback(rt_all, key_deleted_callback, NULL);
    drain(10);
    if(append_one(tm, BASE_T + 1)<0) {  // the key's directory in disks/rtDEAF/, watched
        result += -1;
    }
    drain(30);
    result += test_json(NULL);

    /*
     *  The fd of rtDEAF's watcher becomes a directory: the read in flight
     *  holds the inotify file and completes with the record below, the
     *  next one fails
     */
    set_expected_results_unordered(
        "watcher gone: the feed is told, and nobody reads the dead watcher",
        json_pack("[{s:s},{s:s}]",
            "msg", "inotify read FAILED: the watcher is gone",
            "msg", "rt_disk feed deaf: its watcher is gone"
        ),
        NULL, NULL, 1
    );
    fs_event_t *fs_deaf = (fs_event_t *)(uintptr_t)json_integer_value(
        json_object_get(rt_deaf, "fs_event_client")
    );
    int dfd = open(path_database, O_RDONLY|O_DIRECTORY);
    if(!fs_deaf || dfd < 0 || dup2(dfd, fs_deaf->fd) < 0) {
        printf("%sERROR%s --> cannot replace the fd of the watcher: %s\n",
            On_Red BWhite, Color_Off, strerror(errno));
        result += -1;
    }
    if(dfd >= 0) {
        close(dfd);
    }
    if(append_one(tm, BASE_T + 2)<0) {
        result += -1;
    }
    /*
     *  The read of a directory fails in a worker of io_uring: by time,
     *  not by turns
     */
    uint64_t t0 = time_in_milliseconds_monotonic();
    while(time_in_milliseconds_monotonic() - t0 < 5000 &&
            json_integer_value(json_object_get(rt_deaf, "fs_event_client")) != 0) {
        yev_loop_run_once(yev_loop);
    }
    drain(10);
    if(json_integer_value(json_object_get(rt_deaf, "fs_event_client")) != 0) {
        printf("%sERROR%s --> the feed still holds the watcher that went\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*
     *  A delete: rtALL hears it first and makes the debts of the others
     */
    if(tranger2_delete_key(tm, TOPIC_NAME, SEED_KEY)<0) {
        result += -1;
    }
    drain(30);
    if(deleted_all != 1) {
        printf("%sERROR%s --> rtALL heard the delete %d times, expected 1\n",
            On_Red BWhite, Color_Off, deleted_all);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("watcher gone: shutdown", NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tf, rt_all);
    tranger2_close_rt_disk(tf, rt_deaf);
    drain(10);
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    drain(10);
    result += test_json(NULL);

    rmrdir(path_database);
    return result;
}

/***************************************************************************
 *  The read of a feed's watcher canceled by another than its owner
 ***************************************************************************/
PRIVATE int do_test_read_canceled_by_another(void)
{
    int result = 0;
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), getenv("HOME"), "tests_yuneta", DATABASE, NULL);
    rmrdir(path_database);

    set_expected_results(
        "read canceled by another: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(TRUE);
    if(!tm || !tranger2_create_topic(
        tm, TOPIC_NAME, "id", "tm",
        json_pack("{s:i, s:s, s:i, s:i}",
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        sf_int_key,
        json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
        0)) {
        tranger2_shutdown(tm);
        return -1;
    }
    json_t *tf = startup_tranger(FALSE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        return -1;
    }
    json_t *rt_live = tranger2_open_rt_disk(tf, TOPIC_NAME, "", NULL, record_callback, "rtLIVE", "", NULL);
    if(!rt_live) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        return -1;
    }
    drain(10);
    result += test_json(NULL);

    set_expected_results(
        "read canceled by another: the owner is told, nobody touches a freed watcher",
        json_pack("[{s:s},{s:s}]",
            "msg", "inotify read canceled, and not by its owner: the watcher is gone",
            "msg", "rt_disk feed deaf: its watcher is gone"
        ),
        NULL, NULL, 1
    );
    fs_event_t *fs_live = (fs_event_t *)(uintptr_t)json_integer_value(
        json_object_get(rt_live, "fs_event_client")
    );
    if(!fs_live || yev_stop_event(fs_live->yev_event) < 0) {
        printf("%sERROR%s --> cannot cancel the read of the watcher\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    uint64_t t0 = time_in_milliseconds_monotonic();
    while(time_in_milliseconds_monotonic() - t0 < 5000 &&
            json_integer_value(json_object_get(rt_live, "fs_event_client")) != 0) {
        yev_loop_run_once(yev_loop);
    }
    drain(10);
    if(json_integer_value(json_object_get(rt_live, "fs_event_client")) != 0) {
        printf("%sERROR%s --> the feed still holds the watcher whose read was canceled\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    tranger2_close_rt_disk(tf, rt_live);
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    drain(10);
    result += test_json(NULL);

    rmrdir(path_database);
    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    gbmem_get_allocators(&base_malloc, &base_realloc, &base_calloc, &base_free);
    gbmem_set_allocators(poison_malloc, poison_realloc, poison_calloc, poison_free);

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
    result += do_test_read_canceled_by_another();

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();
    release_quarantine();

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
