/****************************************************************************
 *          test_timer1.c
 *
 *          Create a timer of 2 second
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <gobj.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <stacktrace_with_bfd.h>
#include <yunetas_ev_loop.h>

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_t *yev_loop;
yev_event_t *yev_event_once;
yev_event_t *yev_event_periodic;
int times_counter_periodic = 0;
int times_counter_once = 0;
int result = 0;

/***************************************************************************
 *  Callback that will be executed when the timer period lapses.
 *  Posts the timer expiry event to the default event loop.
 ***************************************************************************/
PRIVATE int yev_callback_once(yev_event_t *yev_event)
{
    yev_state_t yev_state = yev_get_state(yev_event);

    times_counter_once++;

    char msg[80];
    if(yev_event->result<0) {
        snprintf(msg, sizeof(msg), "%s", strerror(-yev_event->result));
    } else {
        if(yev_state == YEV_ST_IDLE) {
            snprintf(msg, sizeof(msg), "timeout once got %d", times_counter_once);
        } else if(yev_state == YEV_ST_STOPPED) {
            snprintf(msg, sizeof(msg), "timeout once stopped");
        } else {
            snprintf(msg, sizeof(msg), "BAD state timer once %s", yev_get_state_name(yev_event));
        }
    }
    json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_event->flag);
    gobj_log_warning(0, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_YEV_LOOP,
        "msg",          "%s", msg,
        "msg2",         "%s", "⏰⏰ ✅✅ timeout once got",
        "type",         "%s", yev_event_type_name(yev_event),
        "state",        "%s", yev_get_state_name(yev_event),
        "fd",           "%d", yev_event->fd,
        "result",       "%d", yev_event->result,
        "sres",         "%s", (yev_event->result<0)? strerror(-yev_event->result):"",
        "p",            "%p", yev_event,
        "flag",         "%j", jn_flags,
        "periodic",     "%d", (yev_event->flag & YEV_FLAG_TIMER_PERIODIC)?1:0,
        NULL
    );
    json_decref(jn_flags);

    yev_stop_event(yev_event_periodic);

    return 0;
}

/***************************************************************************
 *  Callback that will be executed when the timer period lapses.
 *  Posts the timer expiry event to the default event loop.
 ***************************************************************************/
PRIVATE int yev_callback_periodic(yev_event_t *yev_event)
{
    yev_state_t yev_state = yev_get_state(yev_event);

    times_counter_periodic++;

    char msg[80];
    if(yev_event->result<0) {
        snprintf(msg, sizeof(msg), "%s", strerror(-yev_event->result));
    } else {
        if(yev_state == YEV_ST_IDLE) {
            snprintf(msg, sizeof(msg), "timeout periodic got %d", times_counter_periodic);
        } else if(yev_state == YEV_ST_STOPPED) {
            snprintf(msg, sizeof(msg), "timeout periodic stopped");
        } else {
            snprintf(msg, sizeof(msg), "BAD state timer periodic %s", yev_get_state_name(yev_event));
        }
    }
    json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_event->flag);
    gobj_log_warning(0, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_YEV_LOOP,
        "msg",          "%s", msg,
        "msg2",         "%s", "⏰⏰ ✅✅ timeout periodic got",
        "type",         "%s", yev_event_type_name(yev_event),
        "state",        "%s", yev_get_state_name(yev_event),
        "fd",           "%d", yev_event->fd,
        "result",       "%d", yev_event->result,
        "sres",         "%s", (yev_event->result<0)? strerror(-yev_event->result):"",
        "p",            "%p", yev_event,
        "flag",         "%j", jn_flags,
        "periodic",     "%d", (yev_event->flag & YEV_FLAG_TIMER_PERIODIC)?1:0,
        NULL
    );
    json_decref(jn_flags);

    if(times_counter_periodic == 1) {
        gobj_trace_msg(0, "re-start timer once");
        yev_start_timer_event(yev_event_once, 1*1000, FALSE);
    }

    if(times_counter_periodic < 3) {
        if(yev_state != YEV_ST_IDLE) {
            printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "state must be idle");
            result += -1;
        }
    } else if(times_counter_periodic == 3) {
        if(yev_state != YEV_ST_STOPPED) {
            printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "state must be stopped");
            result += -1;
        }
        /*
         *  Here the two timers are stopped or idle, quit the loop
         */
        yev_loop_stop(yev_loop);

    } else {
        gobj_trace_msg(0, "stop timer periodic");
        yev_stop_event(yev_event_periodic);
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "reached 4 times in periodic timer");
        result += -1;
    }

    return 0;
}

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
    yev_event_once = yev_create_timer_event(yev_loop, yev_callback_once, NULL);
    yev_event_periodic = yev_create_timer_event(yev_loop, yev_callback_periodic, NULL);

    gobj_trace_msg(0, "start time once %d seconds", 4);
    yev_start_timer_event(yev_event_once, 4*1000, FALSE);

    gobj_trace_msg(0, "start time periodic %d seconds", 1);
    yev_start_timer_event(yev_event_periodic, 1*1000, TRUE);

    yev_loop_run(yev_loop);
    gobj_trace_msg(0, "Quiting of main yev_loop_run()");

    yev_loop_run_once(yev_loop);
    yev_destroy_event(yev_event_periodic);
    yev_destroy_event(yev_event_once);

    yev_loop_destroy(yev_loop);

    return result;
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

    init_backtrace_with_bfd(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_bfd);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // startup_persistent_attrs
        NULL,   // end_persistent_attrs
        NULL,   // load_persistent_attrs
        NULL,   // save_persistent_attrs
        NULL,   // remove_persistent_attrs
        NULL,   // list_persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL,   // global_authenticate_parser
        0,      // max_block, largest memory block
        0       // max_system_memory, maximum system memory
    );

    yuno_catch_signals();

    gobj_set_gobj_trace(0, "libuv", TRUE, 0);

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    const char *test = "test_timer2";
    set_expected_results( // Check that no logs happen
        test,   // test name
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",  // error_list
            "msg", "timeout periodic got 1",
            "msg", "timeout periodic got 2",
            "msg", "timeout once got 1",
            "msg", "timeout periodic stopped"
        ),
        NULL,  // expected
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    result += do_test();

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)

    double tm = mt_get_time(&time_measure);
    if(!(tm >= 2 && tm < 2.1)) {
        printf("%sERROR --> %s time %f (must be tm >= 2 && tm < 2.1)\n", On_Red BWhite, Color_Off, tm);
        result += -1;
    }

    result += test_json(NULL, result);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "system memory not free");
        print_track_mem();
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
