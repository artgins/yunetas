/****************************************************************************
 *          test_stop_reopen.c
 *
 *  A tranger stopped with tranger2_stop() and used again (C_TRANGER's
 *  stop + start: the tranger lives from mt_create to mt_destroy):
 *
 *      - the stop closes the fds of fd_opened_files and marks them -1, so a
 *        later stop or shutdown never closes those NUMBERS again: by then
 *        they may belong to a socket, a log or an io_uring of somebody else.
 *      - a topic opened again after the stop revives the tranger: its
 *        shutdown closes what it opens from now on.
 *      - a master gave its single-master lock back at the stop, and takes it
 *        again at the revival: a second master of the same store is refused
 *        once more.
 *      - a lookup of a topic that is ALREADY open does not revive anything:
 *        the flag was cleared by every tranger2_topic() call, on the hot
 *        path of the append.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_stop_reopen"
#define DATABASE    "tr_stop_reopen"
#define TOPIC_NAME  "topic_stop_reopen"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *startup_master(const char *path_root)
{
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

PRIVATE int append_one(json_t *tranger, json_int_t t)
{
    md2_record_ex_t md = {0};
    json_t *jn_record = json_pack("{s:s, s:I, s:s}",
        "id", "key1",
        "tm", t,
        "content", "payload"
    );
    return tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)t, 0, &md, jn_record);
}

PRIVATE int lock_fd(json_t *tranger)
{
    return (int)kw_get_int(0, tranger, "fd_opened_files`__timeranger2__.json", -1, 0);
}

PRIVATE BOOL fd_is_open(int fd)
{
    return (fd >= 0 && fcntl(fd, F_GETFD) >= 0)? TRUE: FALSE;
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
    if(!tranger) {
        return -1;
    }
    json_t *topic = tranger2_create_topic(
        tranger,
        TOPIC_NAME,
        "id",
        "tm",
        0,
        sf_string_key,
        json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
        0
    );
    if(!topic || append_one(tranger, BASE_T) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A lookup of an open topic revives
     *  nothing
     *-------------------------------------*/
    set_expected_results("a lookup of an open topic does not touch __closed__", NULL, NULL, NULL, 1);
    json_object_set_new(tranger, "__closed__", json_true());
    tranger2_topic(tranger, TOPIC_NAME);
    if(!kw_get_bool(0, tranger, "__closed__", 0, 0)) {
        printf("%sERROR%s --> tranger2_topic() of an open topic cleared __closed__\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    json_object_del(tranger, "__closed__");
    result += test_json(NULL);

    /*-------------------------------------*
     *  Stop: the lock fd is closed and
     *  marked, its number goes to somebody
     *  else
     *-------------------------------------*/
    set_expected_results("the stop marks the fds it closes", NULL, NULL, NULL, 1);
    int old_lock_fd = lock_fd(tranger);
    tranger2_stop(tranger);
    if(lock_fd(tranger) != -1) {
        printf("%sERROR%s --> the stop left the closed lock fd %d in fd_opened_files\n",
            On_Red BWhite, Color_Off, lock_fd(tranger));
        result += -1;
    }
    int other = open("/dev/null", O_RDONLY|O_CLOEXEC);
    if(other != old_lock_fd) {
        /*  Not an error of the library: the check below is weaker, not wrong  */
        printf("  (the unrelated fd got %d, the lock was %d)\n", other, old_lock_fd);
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Reopen: revived, the lock taken
     *  again
     *-------------------------------------*/
    set_expected_results(
        "a topic opened again revives the tranger and retakes the lock",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot open an exclusive json file",
            "msg", "Open as not master, __timeranger2__.json locked"
        ),
        NULL, NULL, 1
    );
    if(!tranger2_topic(tranger, TOPIC_NAME)) {
        printf("%sERROR%s --> the topic does not open again\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(kw_get_bool(0, tranger, "__closed__", 0, 0)) {
        printf("%sERROR%s --> a topic opened again left __closed__ set\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!kw_get_bool(0, tranger, "master", 0, 0) || !fd_is_open(lock_fd(tranger)) ||
            lock_fd(tranger) == other) {
        printf("%sERROR%s --> the revived master holds no lock (fd %d)\n",
            On_Red BWhite, Color_Off, lock_fd(tranger));
        result += -1;
    }
    json_t *tranger2 = startup_master(path_root);
    if(!tranger2 || kw_get_bool(0, tranger2, "master", 0, 0)) {
        printf("%sERROR%s --> a second master took the store of a revived one\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(tranger2) {
        tranger2_shutdown(tranger2);
    }
    if(append_one(tranger, BASE_T + 1) < 0) {
        printf("%sERROR%s --> the revived master cannot append\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Shutdown: closes what the revival
     *  opened, and nothing of anybody else
     *-------------------------------------*/
    set_expected_results("shutdown closes no fd of anybody else", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    if(!fd_is_open(other)) {
        printf("%sERROR%s --> the shutdown closed fd %d, which it did not own\n",
            On_Red BWhite, Color_Off, other);
        result += -1;
    } else {
        close(other);
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Stop + shutdown with no revival
     *-------------------------------------*/
    set_expected_results("stop then shutdown closes nothing twice", NULL, NULL, NULL, 1);
    tranger = startup_master(path_root);
    tranger2_open_topic(tranger, TOPIC_NAME, TRUE);
    tranger2_stop(tranger);
    other = open("/dev/null", O_RDONLY|O_CLOEXEC);
    tranger2_shutdown(tranger);
    if(!fd_is_open(other)) {
        printf("%sERROR%s --> the shutdown after a stop closed fd %d\n",
            On_Red BWhite, Color_Off, other);
        result += -1;
    } else {
        close(other);
    }
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
