/****************************************************************************
 *          perf_yev_timer
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <gobj.h>
#include <ansi_escape_codes.h>
#include <stacktrace_with_bfd.h>
#include <yunetas_ev_loop.h>

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);
PRIVATE int yev_callback(yev_event_t *event);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_t *yev_loop;
yev_event_t *yev_event_once;
yev_event_t *yev_event_periodic;
int wait_time = 1;
int times_once = 0;
int times_periodic = 0;
int result = 0;
/***************************************************************************
 *              Test
 ***************************************************************************/
int do_test(void)
{
    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        &yev_loop
    );

    /*--------------------------------*
     *      Create timer
     *--------------------------------*/
    yev_event_once = yev_create_timer_event(yev_loop, yev_callback, NULL);

    yev_event_periodic = yev_create_timer_event(yev_loop, yev_callback, NULL);

    gobj_trace_msg(0, "start time-once %d seconds", wait_time);
    yev_start_timer_event(yev_event_once, wait_time*1000, FALSE);

    gobj_trace_msg(0, "start time-periodic %d seconds", 1);
    yev_start_timer_event(yev_event_periodic, 1*1000, TRUE);

    yev_loop_run(yev_loop);
    gobj_trace_msg(0, "Quiting of main yev_loop_run()");

    yev_loop_run_once(yev_loop);

    yev_destroy_event(yev_event_once);
    yev_destroy_event(yev_event_periodic);
    yev_loop_destroy(yev_loop);

    return 0;
}

/***************************************************************************
 *  Callback that will be executed when the timer period lapses.
 *  Posts the timer expiry event to the default event loop.
 ***************************************************************************/
PRIVATE int yev_callback(yev_event_t *yev_event)
{
    if(yev_event->result < 0) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_event->flag);
        gobj_log_info(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "timeout got",
            "msg2",         "%s", "⏰⏰ ✅✅ timeout got",
            "type",         "%s", yev_event_type_name(yev_event),
            "fd",           "%d", yev_event->fd,
            "result",       "%d", yev_event->result,
            "sres",         "%s", (yev_event->result<0)? strerror(-yev_event->result):"",
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            "periodic",     "%d", (yev_event->flag & YEV_FLAG_TIMER_PERIODIC)?1:0,
            NULL
        );
        json_decref(jn_flags);

        if(yev_event->result == -EAGAIN) {
            return 0;
        }
        // cancel timer-once and stop loop, next cancels ignored
        if(yev_loop->running) {
            if(!(yev_event->flag & YEV_FLAG_TIMER_PERIODIC) && yev_event->result == -ECANCELED) {
                yev_stop_event(yev_event_periodic);
                yev_loop_stop(yev_loop);
            }
        }
        return 0;
    }

    if(yev_event->flag & YEV_FLAG_TIMER_PERIODIC) {
        times_periodic++;
        if(times_periodic == 2) {
            gobj_trace_msg(0, "re-start time-periodic %d seconds, will provoke -11 error", 1);
            yev_start_timer_event(yev_event_periodic, 1*1000, TRUE); // Will provoke EAGAIN error
        }
        if(times_periodic == 4) {
            gobj_trace_msg(0, "return no-realm and re-start time-periodic %d seconds", 1);
            yev_start_timer_event(yev_event_periodic, 1*1000, TRUE);
            return -1; // On return -1 the timer will be not rearmed
        }
        if(times_once > 5) {
            printf("got timer-periodic, STOP timer ONCE\n");
            if(yev_event_in_ring(yev_event_once)) {
                yev_stop_event(yev_event_once);
            }
        }
    } else {
        times_once++;
        printf("got timer-once %d, set in %d seconds\n", times_once, (int)wait_time*times_once);
        yev_start_timer_event(yev_event, times_once*wait_time*1000, FALSE);
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

//    gobj_set_deep_tracing(2);           // TODO TEST
//    gobj_set_global_trace(0, TRUE);     // TODO TEST

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

    //gobj_set_gobj_trace(0, "libuv", TRUE, 0);

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    result += do_test();

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "system memory not free");
        result += -1;
    }

    return result;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop->running = 0;
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
