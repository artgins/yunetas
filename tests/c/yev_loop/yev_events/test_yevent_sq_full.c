/****************************************************************************
 *          test_yevent_sq_full.c
 *
 *          A FULL submission queue.
 *
 *          io_uring_get_sqe() answers NULL when every entry of the
 *          submission queue holds something not handed to the kernel yet,
 *          and yev_loop used that NULL as an entry (a segfault). It
 *          flushes the queue (io_uring_submit) and asks once more; when the
 *          kernel takes nothing (io_uring_enter() fails: a CQ overflow on
 *          an older kernel answers EBUSY until completions are reaped), the
 *          submission is KEPT and made at the next cycle of the loop. It
 *          used to be logged and lost.
 *
 *          Setup
 *          -----
 *          A loop of 8 entries. Before each call, the queue is filled with
 *          NOPs that are NOT submitted (IOSQE_CQE_SKIP_SUCCESS: they
 *          complete with no CQE). To make the kernel take nothing, the fd
 *          of the ring is made invalid for the call (io_uring_enter()
 *          answers EBADF), and restored after it.
 *
 *          Process
 *          -------
 *          1. Start a timer of 100 ms with the queue full: it starts, and
 *             fires.
 *          2. Start a timer of 10 s, fill the queue, stop the timer while
 *             it RUNS: the cancel is submitted, the timer is stopped.
 *          3. Queue full and not flushable: a timer of 100 ms starts (kept),
 *             and fires once the kernel takes submissions again.
 *          4. Queue full and not flushable: a timer of 10 s starts (kept)
 *             and is stopped before its read reached the kernel: the stop
 *             reaches the callback as a cancel (STOPPED, -ECANCELED).
 *          5. Fill the queue and stop the loop: it stops.
 *          6. Queue full and not flushable, in a second loop: the loop
 *             is stopped (kept), and stops once the kernel takes
 *             submissions again.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <liburing.h>
#include <gobj.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <yev_loop.h>
#include <helpers.h>

#define APP "test_yevent_sq_full"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);
PRIVATE int yev_callback(yev_event_h yev_event);
PRIVATE int loop_timeout_callback(yev_event_h yev_event);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
int times_counter = 0;
int stopped_counter = 0;
int stopped_result = 0;
int result = 0;
int loop_timeouts = 0;
int saved_ring_fd = -1;
int saved_enter_ring_fd = -1;

/***************************************************************************
 *  Fill the submission queue of the loop with NOPs, not submitted (the
 *  NOPs an earlier step left there are submitted first).
 *  HACK the ring is the first member of the loop (yev_loop.c): a test
 *  reaches it to leave the queue full, nothing else touches it.
 *  Return how many entries were taken.
 ***************************************************************************/
PRIVATE int fill_submission_queue(void)
{
    struct io_uring *ring = (struct io_uring *)yev_loop;
    io_uring_submit(ring);  /*  what an earlier step left in the queue  */
    int n = 0;
    struct io_uring_sqe *sqe;
    while((sqe = io_uring_get_sqe(ring)) != NULL) {
        io_uring_prep_nop(sqe);
        io_uring_sqe_set_flags(sqe, IOSQE_CQE_SKIP_SUCCESS);
        io_uring_sqe_set_data(sqe, NULL);
        n++;
    }
    return n;
}

/***************************************************************************
 *  Make the kernel take NOTHING from the queue: io_uring_enter() on an
 *  invalid fd answers EBADF, so a flush leaves the queue full. HACK the
 *  ring is the first member of the loop.
 ***************************************************************************/
PRIVATE void block_submissions(void)
{
    struct io_uring *ring = (struct io_uring *)yev_loop;
    saved_ring_fd = ring->ring_fd;
    saved_enter_ring_fd = ring->enter_ring_fd;
    ring->ring_fd = -1;
    ring->enter_ring_fd = -1;
}

PRIVATE void unblock_submissions(void)
{
    struct io_uring *ring = (struct io_uring *)yev_loop;
    ring->ring_fd = saved_ring_fd;
    ring->enter_ring_fd = saved_enter_ring_fd;
}

/***************************************************************************
 *  The timeout of a loop run: the loop was not stopped
 ***************************************************************************/
PRIVATE int loop_timeout_callback(yev_event_h yev_event)
{
    if(yev_event) {
        return 0;
    }
    loop_timeouts++;
    return -1;  // break the loop
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_callback(yev_event_h yev_event)
{
    if(!yev_event) {
        /*
         *  It's the timeout
         */
        return -1;  // break the loop
    }

    char msg[80] = "";

    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_get_type(yev_event)) {
        case YEV_TIMER_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    times_counter++;
                    snprintf(msg, sizeof(msg), "timeout got %d", times_counter);
                } else if(yev_state == YEV_ST_STOPPED) {
                    stopped_counter++;
                    stopped_result = yev_get_result(yev_event);
                    snprintf(msg, sizeof(msg), "timeout stopped");
                } else {
                    snprintf(msg, sizeof(msg), "BAD state %s", yev_get_state_name(yev_event));
                }
            }
            break;
        default:
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING,
                "msg",          "%s", "yev_event not implemented",
                NULL
            );
            return 0;
    }

    gobj_log_warning(0, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_YEV_LOOP,
        "msg",          "%s", msg,
        "type",         "%s", yev_event_type_name(yev_event),
        "state",        "%s", yev_get_state_name(yev_event),
        NULL
    );

    return -1;  // break the loop: each step checks what happened
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        8,
        10,
        loop_timeout_callback,  // a run that times out: what it waited for did not happen
        &yev_loop
    );

    /*--------------------------------*
     *  1. Start a timer, queue full
     *--------------------------------*/
    yev_event_h yev_event_once = yev_create_timer_event(yev_loop, yev_callback, NULL);

    if(fill_submission_queue() <= 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "the queue could not be filled");
        result += -1;
    }
    if(yev_start_timer_event(yev_event_once, 100, FALSE) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "timer not started with the queue full");
        result += -1;
    }
    yev_loop_run(yev_loop, 2);
    if(times_counter != 1) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "the timer did not fire");
        result += -1;
    }

    /*--------------------------------*
     *  2. Stop a RUNNING timer,
     *     queue full
     *--------------------------------*/
    if(yev_start_timer_event(yev_event_once, 10*1000, FALSE) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "timer of 10 s not started");
        result += -1;
    }
    if(fill_submission_queue() <= 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "the queue could not be filled again");
        result += -1;
    }
    if(yev_stop_event(yev_event_once) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "running timer not stopped with the queue full");
        result += -1;
    }
    yev_loop_run(yev_loop, 2);
    if(stopped_counter != 1) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "the stop did not reach the callback");
        result += -1;
    }

    /*--------------------------------*
     *  3. Start a timer, queue full
     *     and NOT flushable: kept,
     *     submitted at the next cycle
     *--------------------------------*/
    yev_event_h yev_event_kept = yev_create_timer_event(yev_loop, yev_callback, NULL);
    fill_submission_queue();
    block_submissions();
    if(yev_start_timer_event(yev_event_kept, 100, FALSE) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "timer not started with the queue full and not flushable");
        result += -1;
    }
    unblock_submissions();
    yev_loop_run(yev_loop, 2);
    if(times_counter != 2) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "the kept timer did not fire");
        result += -1;
    }

    /*--------------------------------*
     *  4. Stop a timer whose start
     *     is still kept: a cancel
     *--------------------------------*/
    fill_submission_queue();
    block_submissions();
    if(yev_start_timer_event(yev_event_kept, 10*1000, FALSE) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "timer of 10 s not started with the queue not flushable");
        result += -1;
    }
    if(yev_stop_event(yev_event_kept) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "kept timer not stopped");
        result += -1;
    }
    unblock_submissions();
    yev_loop_run(yev_loop, 2);
    if(stopped_counter != 2 || stopped_result != -ECANCELED) {
        printf("%sERROR%s <-- %s (stopped %d, result %d)\n", On_Red BWhite, Color_Off,
            "the stop of the kept timer did not reach the callback as a cancel",
            stopped_counter, stopped_result);
        result += -1;
    }

    /*--------------------------------*
     *  5. Stop the loop, queue full
     *--------------------------------*/
    if(fill_submission_queue() <= 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "the queue could not be filled a third time");
        result += -1;
    }
    if(yev_loop_stop(yev_loop) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "loop not stopped with the queue full");
        result += -1;
    }
    yev_loop_run_once(yev_loop);

    /*--------------------------------*
     *  6. Stop a loop, queue full and
     *     NOT flushable: kept, and the
     *     loop stops (a timeout of its
     *     run says it did not)
     *--------------------------------*/
    yev_loop_h yev_loop_main = yev_loop;
    yev_loop_create(
        0,
        8,
        10,
        loop_timeout_callback,
        &yev_loop
    );
    fill_submission_queue();
    block_submissions();
    if(yev_loop_stop(yev_loop) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "loop not stopped with the queue not flushable");
        result += -1;
    }
    unblock_submissions();
    int timeouts_before = loop_timeouts;
    yev_loop_run(yev_loop, 1);
    if(loop_timeouts != timeouts_before) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "the kept stop did not stop the loop");
        result += -1;
    }
    yev_loop_destroy(yev_loop);
    yev_loop = yev_loop_main;

    /*--------------------------------*
     *  Destroy the events
     *--------------------------------*/
    yev_destroy_event(yev_event_once);
    yev_destroy_event(yev_event_kept);

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

    gbmem_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL    // global_authentication_parser
    );

    yuno_catch_signals();

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
    const char *test = APP;
    json_t *error_list = json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",  // error_list
        "msg", "timeout got 1",
        "msg", "timeout stopped",
        "msg", "Submission queue full and the kernel takes nothing: kept for the next cycle",
        "msg", "timeout got 2",
        "msg", "Submission queue full and the kernel takes nothing: kept for the next cycle",
        "msg", "timeout stopped",
        "msg", "Submission queue full and the kernel takes nothing: kept for the next cycle"
    );

    set_expected_results( // Check that no other logs happen
        test,   // test name
        error_list,  // error_list
        NULL,  // expected
        NULL,   // ignore_keys
        1       // verbose
    );

    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    result += do_test();

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)

    result += test_json(NULL);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "system memory not free");
        print_track_mem();
        result += -1;
    }

    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result<0?-1:0;
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
