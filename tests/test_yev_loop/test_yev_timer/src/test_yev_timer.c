/****************************************************************************
 *          test_yev_timer
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <gobj.h>
#include <stacktrace_with_bfd.h>
#include <yunetas_ev_loop.h>

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);
PRIVATE int yev_callback(hgobj gobj, yev_event_t *event, void *data, int result, BOOL stopped);

/***************************************************************
 *              Data
 ***************************************************************/
int wait_time = 2;
int times = 0;

/***************************************************************************
 *              Test
 ***************************************************************************/
int do_test(void)
{
    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_t *yev_loop;
    yev_loop_create(
        0,
        2024,
        &yev_loop
    );

    /*--------------------------------*
     *      Create timer
     *--------------------------------*/
    yev_event_t *yev_event = yev_create_timer_event(yev_loop, yev_callback, NULL);

    gobj_trace_msg(0, "yev_start_timer_event %d seconds", wait_time);
    yev_start_timer_event(yev_event, wait_time*1000, FALSE);

    yev_loop_run(yev_loop);
    gobj_trace_msg(0, "Quiting of yev_loop_run()");

    yev_destroy_event(yev_event);
    yev_loop_destroy(yev_loop);

    return 0;
}

/***************************************************************************
 *  Callback that will be executed when the timer period lapses.
 *  Posts the timer expiry event to the default event loop.
 ***************************************************************************/
PRIVATE int yev_callback(hgobj gobj, yev_event_t *yev_event, void *data, int result, BOOL stopped)
{
    if(stopped) {
        gobj_trace_msg(0, "yev_start_timer_event STOPPED, stop loop");
        yev_loop_stop(yev_event->yev_loop);
    } else {
        times++;
        if(times < 3) {
            gobj_trace_msg(0, "timer_event of %d seconds DONE %d time", wait_time, times);
            yev_start_timer_event(yev_event, wait_time*1000, FALSE);
        } else {
            gobj_trace_msg(0, "timer_event of %d seconds DONE %d time, stopping", wait_time, times);
            yev_stop_event(yev_event);
        }
    }
    return 0;
}

/***************************************************************************
 *              Main
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

    gobj_set_deep_tracing(2);           // TODO TEST
    gobj_set_global_trace(0, TRUE);     // TODO TEST

#ifdef DEBUG
    init_backtrace_with_bfd(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_bfd);
#endif

    gobj_start_up(
        argc,
        argv,
        NULL, // jn_global_settings
        NULL, // startup_persistent_attrs
        NULL, // end_persistent_attrs
        0,  // load_persistent_attrs
        0,  // save_persistent_attrs
        0,  // remove_persistent_attrs
        0,  // list_persistent_attrs
        NULL, // global_command_parser
        NULL, // global_stats_parser
        NULL, // global_authz_checker
        NULL, // global_authenticate_parser
        8*1024L,    // max_block, largest memory block
        100*1024L   // max_system_memory, maximum system memory
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);


    /*--------------------------------*
     *      Test
     *--------------------------------*/
    do_test();

    return gobj_get_exit_code();
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int tries = 0;

    /*
     *  __yuno_gobj__ is 0 for watcher fork, if we are running --stop
     */
    hgobj gobj = gobj_yuno();

    if(gobj) {
        tries++;
        gobj_set_yuno_must_die();
        if(tries > 1) {
            _exit(-1);
        }
    }
}

PRIVATE void debug_sighandler(int sig)
{
    /*
     *  __yuno_gobj__ is 0 for watcher fork, if we are running --stop
     */
    hgobj gobj = gobj_yuno();

    if(gobj) {
        gobj_set_deep_tracing(2);
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

    sigIntHandler.sa_handler = debug_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGUSR1, &sigIntHandler, NULL);
}
