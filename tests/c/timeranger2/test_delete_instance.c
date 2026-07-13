/****************************************************************************
 *          test_delete_instance.c
 *
 *  Regression coverage for tranger2_delete_instance:
 *      - do_test_history_skip:  history iterator skips dead rows (cold reload too).
 *      - do_test_paged_skip:    iterator_get_page skips dead rows; a FILTERED
 *                               iterator (one with a match_cond) indexes only
 *                               the live rows, so total_rows agrees with what
 *                               the pages return.
 *      - do_test_zero_payload:  __size__ bytes at __offset__ are zeroed when opt-in.
 *      - do_test_idempotent:    second delete of the same instance is a no-op (0, no log error).
 *      - do_test_non_master:    non-master callers are refused with a logged error.
 *
 *  Gate 3 (publish_new_rt_disk_records, rt_by_disk follower) is mechanically
 *  identical to the gate covered by do_test_paged_skip and is not exercised
 *  here — would require a second process + inotify.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_delete_instance"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE    "tr_delete_instance"
#define TOPIC_NAME  "topic_delete_instance"
#define KEY_ID      1
#define KEY_STR     "0000000000000000001"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;

PRIVATE size_t history_seen = 0;
PRIVATE json_int_t history_rowids[8];

PRIVATE int history_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    if(history_seen < sizeof(history_rowids)/sizeof(history_rowids[0])) {
        history_rowids[history_seen] = (json_int_t)md_record->rowid;
    }
    history_seen++;
    JSON_DECREF(record)
    return 0;
}

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE void build_paths(
    char *path_root, size_t root_sz,
    char *path_database, size_t db_sz,
    char *path_topic, size_t topic_sz
)
{
    const char *home = getenv("HOME");
    build_path(path_root, root_sz, home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, db_sz, path_root, DATABASE, NULL);
    build_path(path_topic, topic_sz, path_database, TOPIC_NAME, NULL);
}

PRIVATE json_t *startup_master(const char *path_root)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600
    );
    return tranger2_startup(0, jn_tranger, 0);
}

PRIVATE json_t *startup_nonmaster(const char *path_root)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 0,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600
    );
    return tranger2_startup(0, jn_tranger, 0);
}

PRIVATE int create_topic(json_t *tranger)
{
    json_t *topic = tranger2_create_topic(
        tranger,
        TOPIC_NAME,
        "id",
        "tm",
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
    return topic? 0 : -1;
}

PRIVATE int append_n(json_t *tranger, int n, md2_record_ex_t *out_md_records)
{
    for(int j=0; j<n; j++) {
        json_t *jn_record = json_pack("{s:I, s:I, s:s}",
            "id", (json_int_t)KEY_ID,
            "tm", (json_int_t)(BASE_T + j),
            "content", "payload-XXXXXXXX"   // fixed-ish size, easy to find in raw file
        );
        md2_record_ex_t md = {0};
        if(tranger2_append_record(tranger, TOPIC_NAME, BASE_T + j, 0, &md, jn_record) < 0) {
            return -1;
        }
        if(out_md_records) {
            out_md_records[j] = md;
        }
    }
    return 0;
}

/***************************************************************************
 *  do_test_history_skip
 *  Append 3 instances of one key, delete instance #2, then iterate via
 *  tranger2_open_iterator (history loop). Expect rowids 1 and 3 only.
 *  Reopen the tranger fresh (cold reload) and re-iterate: same result.
 ***************************************************************************/
PRIVATE int do_test_history_skip(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);

    set_expected_results(
        "history: startup + create + append + delete",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );

    json_t *tranger = startup_master(path_root);
    if(!tranger) {
        return -1;
    }
    if(create_topic(tranger) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    md2_record_ex_t md[3];
    if(append_n(tranger, 3, md) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }

    /*-------------------------------------*
     *  Delete instance #2
     *-------------------------------------*/
    if(tranger2_delete_instance(
            tranger, TOPIC_NAME, KEY_STR,
            md[1].__t__, md[1].rowid, FALSE) < 0) {
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Iterate (warm: same tranger handle)
     *-------------------------------------*/
    set_expected_results(
        "history: warm iterator skips dead row",
        NULL, NULL, NULL, 1
    );

    history_seen = 0;
    memset(history_rowids, 0, sizeof(history_rowids));

    json_t *match_cond = json_pack("{s:i}", "from_rowid", 1);
    json_t *iterator = tranger2_open_iterator(
        tranger, TOPIC_NAME, KEY_STR,
        match_cond,
        history_record_callback,
        "warm",
        NULL, NULL, NULL
    );
    if(!iterator) {
        result += -1;
    }
    if(history_seen != 2) {
        printf("%sERROR%s --> warm: expected 2 records, got %zu\n",
            On_Red BWhite, Color_Off, history_seen);
        result += -1;
    }
    if(history_rowids[0] != 1 || history_rowids[1] != 3) {
        printf("%sERROR%s --> warm: rowids renumbered, got [%lld, %lld]\n",
            On_Red BWhite, Color_Off,
            (long long)history_rowids[0], (long long)history_rowids[1]);
        result += -1;
    }
    tranger2_close_iterator(tranger, iterator);
    result += test_json(NULL);

    /*-------------------------------------*
     *  Cold reload
     *-------------------------------------*/
    tranger2_shutdown(tranger);

    set_expected_results(
        "history: cold reload still skips dead row",
        NULL, NULL, NULL, 1
    );

    json_t *tranger2 = startup_master(path_root);
    if(!tranger2) {
        return -1;
    }
    /* Re-open the topic on the fresh tranger to populate cache */
    if(create_topic(tranger2) < 0) {
        tranger2_shutdown(tranger2);
        return -1;
    }

    history_seen = 0;
    memset(history_rowids, 0, sizeof(history_rowids));

    json_t *match_cond2 = json_pack("{s:i}", "from_rowid", 1);
    json_t *iterator2 = tranger2_open_iterator(
        tranger2, TOPIC_NAME, KEY_STR,
        match_cond2,
        history_record_callback,
        "cold",
        NULL, NULL, NULL
    );
    if(!iterator2) {
        result += -1;
    }
    if(history_seen != 2) {
        printf("%sERROR%s --> cold: expected 2 records, got %zu\n",
            On_Red BWhite, Color_Off, history_seen);
        result += -1;
    }
    if(history_rowids[0] != 1 || history_rowids[1] != 3) {
        printf("%sERROR%s --> cold: rowids renumbered, got [%lld, %lld]\n",
            On_Red BWhite, Color_Off,
            (long long)history_rowids[0], (long long)history_rowids[1]);
        result += -1;
    }
    tranger2_close_iterator(tranger2, iterator2);
    result += test_json(NULL);

    set_expected_results("history: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger2);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_paged_skip
 *  Append 3 instances, delete #2, then page via tranger2_iterator_get_page.
 *  Expect data.length==2, total_rows==3 (slot count NOT renumbered).
 ***************************************************************************/
PRIVATE int do_test_paged_skip(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);

    set_expected_results(
        "paged: startup + create + append + delete",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );

    json_t *tranger = startup_master(path_root);
    if(!tranger) {
        return -1;
    }
    if(create_topic(tranger) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    md2_record_ex_t md[3];
    if(append_n(tranger, 3, md) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    if(tranger2_delete_instance(
            tranger, TOPIC_NAME, KEY_STR,
            md[1].__t__, md[1].rowid, FALSE) < 0) {
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Open iterator and page
     *-------------------------------------*/
    set_expected_results(
        "paged: get_page skips dead row, total_rows counts the live ones",
        NULL, NULL, NULL, 1
    );

    json_t *match_cond = json_pack("{s:i}", "from_rowid", 1);
    json_t *iterator = tranger2_open_iterator(
        tranger, TOPIC_NAME, KEY_STR,
        match_cond,
        NULL,           // no history callback this time
        "paged",
        NULL, NULL, NULL
    );
    if(!iterator) {
        result += -1;
    }

    json_t *page = tranger2_iterator_get_page(
        tranger, iterator, 1, 10, FALSE
    );
    json_int_t total_rows = json_integer_value(json_object_get(page, "total_rows"));
    json_t *data = json_object_get(page, "data");
    size_t data_len = json_array_size(data);

    if(data_len != 2) {
        printf("%sERROR%s --> paged: expected 2 records, got %zu\n",
            On_Red BWhite, Color_Off, data_len);
        result += -1;
    }
    /*
     *  `from_rowid` is a condition, so this iterator is FILTERED and builds
     *  its row index: the index holds only rows that survive the metadata
     *  match, and a deleted instance never enters it. total_rows therefore
     *  counts the rows the pages actually return (2), instead of the old
     *  slot count (3) that promised a record the page then skipped.
     */
    if(total_rows != 2) {
        printf("%sERROR%s --> paged: expected total_rows=2 (live rows), got %lld\n",
            On_Red BWhite, Color_Off, (long long)total_rows);
        result += -1;
    }

    JSON_DECREF(page)
    tranger2_close_iterator(tranger, iterator);
    result += test_json(NULL);

    set_expected_results("paged: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_zero_payload
 *  Delete with zero_payload=TRUE, then open the .json data file and verify
 *  the [__offset__, __offset__+__size__) byte range is all 0x00.
 ***************************************************************************/
PRIVATE int do_test_zero_payload(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);

    set_expected_results(
        "zero: startup + create + append + delete(zero_payload)",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );

    json_t *tranger = startup_master(path_root);
    if(!tranger) {
        return -1;
    }
    if(create_topic(tranger) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    md2_record_ex_t md[3];
    if(append_n(tranger, 3, md) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    uint64_t target_offset = md[1].__offset__;
    uint64_t target_size   = md[1].__size__;
    if(tranger2_delete_instance(
            tranger, TOPIC_NAME, KEY_STR,
            md[1].__t__, md[1].rowid, TRUE /* zero_payload */) < 0) {
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Check raw bytes
     *-------------------------------------*/
    char data_path[PATH_MAX];
    build_path(data_path, sizeof(data_path),
        path_topic, "keys", KEY_STR, "2000-01-01.json", NULL);

    int fd = open(data_path, O_RDONLY|O_CLOEXEC);
    if(fd < 0) {
        printf("%sERROR%s --> zero: cannot open %s: %s\n",
            On_Red BWhite, Color_Off, data_path, strerror(errno));
        result += -1;
    } else {
        char *buf = gbmem_malloc(target_size);
        if(pread(fd, buf, target_size, (off_t)target_offset) != (ssize_t)target_size) {
            printf("%sERROR%s --> zero: short pread\n", On_Red BWhite, Color_Off);
            result += -1;
        } else {
            for(uint64_t i = 0; i < target_size; i++) {
                if(buf[i] != 0) {
                    printf("%sERROR%s --> zero: byte %llu of payload = 0x%02x (expected 0x00)\n",
                        On_Red BWhite, Color_Off,
                        (unsigned long long)i, (unsigned char)buf[i]);
                    result += -1;
                    break;
                }
            }
        }
        gbmem_free(buf);
        close(fd);
    }

    set_expected_results("zero: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_idempotent
 *  Second delete of the same instance: returns 0, no log entry.
 ***************************************************************************/
PRIVATE int do_test_idempotent(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);

    set_expected_results(
        "idempotent: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );

    json_t *tranger = startup_master(path_root);
    if(!tranger || create_topic(tranger) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    md2_record_ex_t md[3];
    if(append_n(tranger, 3, md) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    if(tranger2_delete_instance(
            tranger, TOPIC_NAME, KEY_STR,
            md[1].__t__, md[1].rowid, FALSE) < 0) {
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Second delete: no log expected
     *-------------------------------------*/
    set_expected_results(
        "idempotent: second delete is silent no-op",
        NULL, NULL, NULL, 1
    );
    int ret = tranger2_delete_instance(
        tranger, TOPIC_NAME, KEY_STR,
        md[1].__t__, md[1].rowid, FALSE
    );
    if(ret != 0) {
        printf("%sERROR%s --> idempotent: expected 0, got %d\n",
            On_Red BWhite, Color_Off, ret);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("idempotent: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_non_master
 *  Non-master caller: tranger2_delete_instance returns -1 and logs.
 ***************************************************************************/
PRIVATE int do_test_non_master(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);

    /*-------------------------------------*
     *  Setup via master, then close
     *-------------------------------------*/
    set_expected_results(
        "non_master: master setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );

    json_t *m = startup_master(path_root);
    if(!m || create_topic(m) < 0) {
        if(m) {
            tranger2_shutdown(m);
        }
        return -1;
    }
    md2_record_ex_t md[3];
    if(append_n(m, 3, md) < 0) {
        tranger2_shutdown(m);
        return -1;
    }
    tranger2_shutdown(m);
    result += test_json(NULL);

    /*-------------------------------------*
     *  Reopen as non-master and try to delete
     *-------------------------------------*/
    set_expected_results(
        "non_master: delete is refused with logged error",
        json_pack("[{s:s}]", "msg", "Only master can delete"),
        NULL, NULL, 1
    );

    json_t *r = startup_nonmaster(path_root);
    if(!r) {
        return -1;
    }
    /* Just in case topic isn't memoized yet for the reader */
    create_topic(r);

    int ret = tranger2_delete_instance(
        r, TOPIC_NAME, KEY_STR,
        md[1].__t__, md[1].rowid, FALSE
    );
    if(ret != -1) {
        printf("%sERROR%s --> non_master: expected -1, got %d\n",
            On_Red BWhite, Color_Off, ret);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("non_master: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(r);
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

    gobj_start_up(
        argc, argv,
        NULL, NULL, NULL, NULL, NULL, NULL
    );

    yuno_catch_signals();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler(
        "testing", 0, capture_log_write, 0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    yev_loop_create(0, 2024, 10, NULL, &yev_loop);

    int result = 0;
    result += do_test_history_skip();
    result += do_test_paged_skip();
    result += do_test_zero_payload();
    result += do_test_idempotent();
    result += do_test_non_master();
    result += global_result;

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
