/****************************************************************************
 *          main_fs_watcher
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>

#include <gobj.h>
#include <helpers.h>
#include <yev_loop.h>
#include <ansi_escape_codes.h>
#include <fs_watcher.h>

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);
PRIVATE int fs_event_callback(fs_event_t *fs_event);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
yev_event_h yev_event_periodic;
fs_event_t *fs_event_h;

/***************************************************************************
 *              Test
 ***************************************************************************/
int do_test(char *path)
{
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
     *      Create fs event
     *--------------------------------*/
    fs_event_h = fs_create_watcher_event(
        yev_loop,
        path,
        FS_FLAG_RECURSIVE_PATHS,
        fs_event_callback,
        NULL,
        NULL,
        NULL
    );

    fs_start_watcher_event(fs_event_h);

    yev_loop_run(yev_loop, -1);
    gobj_trace_msg(0, "Quiting of main yev_loop_run()");

    print_json2("tracked_paths", fs_event_h->jn_tracked_paths);

    fs_stop_watcher_event(fs_event_h);
    yev_loop_run_once(yev_loop);

    yev_loop_destroy(yev_loop);

    return 0;
}

/***************************************************************************
 *  Callback that will be executed when the timer period lapses.
 *  Posts the timer expiry event to the default event loop.
 ***************************************************************************/
PRIVATE int fs_event_callback(fs_event_t *fs_event)
{
    char full_path[PATH_MAX];
    snprintf(full_path, PATH_MAX, "%s/%s", fs_event->directory, fs_event->filename);

    if (fs_event->fs_type & (FS_SUBDIR_CREATED_TYPE)) {
        printf("  %sDire created :%s %s\n", On_Green BWhite, Color_Off, full_path);
    }
    if (fs_event->fs_type & (FS_SUBDIR_DELETED_TYPE)) {
        printf("  %sDire deleted :%s %s\n", On_Green BWhite, Color_Off, full_path);
    }
    if (fs_event->fs_type & (FS_FILE_CREATED_TYPE)) {
        printf("  %sFile created :%s %s\n", On_Green BWhite, Color_Off, full_path);
    }
    if (fs_event->fs_type & (FS_FILE_DELETED_TYPE)) {
        printf("  %sFile deleted :%s %s\n", On_Green BWhite, Color_Off, full_path);
    }
    if (fs_event->fs_type & (FS_FILE_MODIFIED_TYPE)) {
        printf("  %sFile modified:%s %s\n", On_Green BWhite, Color_Off, full_path);
    }

    return 0;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    char path[PATH_MAX];
    build_path(path, sizeof(path), argv[1], NULL);
    if(!is_directory(path)) {
        printf("%sERROR%s path not found '%s'\n", On_Red BWhite, Color_Off, path);
        exit(EXIT_FAILURE);
    }
    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gobj_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

//    gobj_set_deep_tracing(2);           // TODO TEST
//    gobj_set_global_trace(0, true);     // TODO TEST

#ifdef DEBUG
    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);
#endif

    gobj_start_up(
        argc,
        argv,
        NULL, // jn_global_settings
        NULL, // persistent_attrs
        NULL, // global_command_parser
        NULL, // global_stats_parser
        NULL, // global_authz_checker
        NULL  // global_authenticate_parser
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    do_test(path);

    gobj_end();

    return gobj_get_exit_code();
}

/***************************************************************************
 *      Signal handlers
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

PUBLIC void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);   // to debug in kdevelop
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);    // ctrl+c
}
