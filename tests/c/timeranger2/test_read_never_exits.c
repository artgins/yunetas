/****************************************************************************
 *          test_read_never_exits.c
 *
 *  A failed READ never makes timeranger2 leave the process, whatever
 *  on_critical_error says. It still logs a critical, but it answers an
 *  error and the process goes on:
 *
 *      - a filtered iterator whose key was deleted under it: its next page
 *        opens an md2 file that is gone (get_topic_rd_fd). On a master with
 *        on_critical_error=2 that was an exit(0), not relaunched.
 *      - tranger2_read_user_flag() on a (key, __t__) whose md2 file does not
 *        exist: a master CREATED an empty md2 there (the read went through
 *        the write fd), then failed to read it with on_critical_error.
 *
 *  The tranger runs with on_critical_error = LOG_OPT_EXIT_NEGATIVE: against
 *  the old library the process exits with -1 in the middle of the test,
 *  which ctest sees as a FAIL. (With 2, the gclass default, it would exit 0,
 *  and ctest would read that as a pass.)
 *
 *  What must keep exiting is not here: a short write of an md2 row
 *  misaligns every later append, and that critical keeps on_critical_error.
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

#define APP         "test_read_never_exits"
#define DATABASE    "tr_read_never_exits"
#define TOPIC_NAME  "topic_read_never_exits"
#define KEY_GONE    "key_gone"
#define KEY_KEPT    "key_kept"
#define N_RECORDS   5
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000
#define OTHER_DAY_T (BASE_T + 10*86400)
#define OTHER_DAY_MD2   "2000-01-11.md2"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *startup_master(const char *path_root)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i, s:I}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_EXIT_NEGATIVE,
        "filename_mask", "%Y-%m-%d",
        "xpermission" , 02770,
        "rpermission", 0600,
        "yev_loop", (json_int_t)0
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
        0,
        sf_string_key,
        json_pack("{s:s, s:I, s:s}",
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
    return topic? 0 : -1;
}

PRIVATE int append_records(json_t *tranger, const char *key)
{
    for(int i = 0; i < N_RECORDS; i++) {
        md2_record_ex_t md = {0};
        json_t *jn_record = json_pack("{s:s, s:I, s:s}",
            "id", key,
            "tm", (json_int_t)(BASE_T + i),
            "content", "payload"
        );
        if(tranger2_append_record(tranger, TOPIC_NAME, BASE_T + i, 0, &md, jn_record) < 0) {
            return -1;
        }
    }
    return 0;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_other_day[PATH_MAX];

    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    build_path(path_other_day, sizeof(path_other_day),
        path_database, TOPIC_NAME, "keys", KEY_KEPT, OTHER_DAY_MD2, NULL);
    rmrdir(path_database);

    /*-------------------------------------*
     *  Setup
     *-------------------------------------*/
    set_expected_results(
        "setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup_master(path_root);
    if(!tranger || create_topic(tranger) < 0 ||
            append_records(tranger, KEY_GONE) < 0 ||
            append_records(tranger, KEY_KEPT) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A filtered iterator, then its key
     *  deleted under it
     *-------------------------------------*/
    set_expected_results(
        "a page of an iterator whose key was deleted is an error, not an exit",
        json_pack("[{s:s}]",
            "msg", "Cannot open file to read"
        ),
        NULL, NULL, 1
    );
    json_t *iterator = tranger2_open_iterator(
        tranger,
        TOPIC_NAME,
        KEY_GONE,
        json_pack("{s:I}", "from_rowid", (json_int_t)2),
        NULL,
        "it_gone",
        APP,
        NULL,
        NULL
    );
    if(!iterator) {
        printf("%sERROR%s --> cannot open the iterator\n", On_Red BWhite, Color_Off);
        result += -1;
    } else {
        if(tranger2_delete_key(tranger, TOPIC_NAME, KEY_GONE) < 0) {
            printf("%sERROR%s --> cannot delete the key\n", On_Red BWhite, Color_Off);
            result += -1;
        }
        json_t *page = tranger2_iterator_get_page(tranger, iterator, 1, 10, FALSE);
        JSON_DECREF(page)
        tranger2_close_iterator(tranger, iterator);
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  read_user_flag on a __t__ with no
     *  md2 file: an error, and no file
     *-------------------------------------*/
    set_expected_results(
        "read_user_flag of a record that does not exist creates nothing",
        json_pack("[{s:s}]",
            "msg", "Record metadata file not found"
        ),
        NULL, NULL, 1
    );
    tranger2_read_user_flag(tranger, TOPIC_NAME, KEY_KEPT, OTHER_DAY_T, 1);
    if(is_regular_file(path_other_day)) {
        printf("%sERROR%s --> a read created an md2 file: %s\n",
            On_Red BWhite, Color_Off, path_other_day);
        result += -1;
    }
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
