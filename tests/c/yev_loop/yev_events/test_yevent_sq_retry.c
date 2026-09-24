/****************************************************************************
 *          test_yevent_sq_retry.c
 *
 *          Submissions the kernel did not take, when the queue is NOT the
 *          problem: io_uring_submit() failed (a CQ overflow answers EBUSY
 *          on an older kernel until the completions are reaped).
 *
 *          test_yevent_sq_full covers the entries the LOOP keeps, when the
 *          queue is full. This test covers the entries that stay in the
 *          queue itself, not taken by the kernel:
 *
 *          - A submit that fails with free room in the queue leaves its
 *            entry there. In 7.25.4 nothing submitted it again: the loop
 *            waited without a timeout for a completion that could not come.
 *          - The loop hands its kept entries to the queue and the submit
 *            after that fails: the same, and the kept list is empty.
 *          - A stop of an event whose entry is still in the queue closes
 *            its fd (a timer, a connect). The stale entry ran later on
 *            whatever fd took that number: it took the expiration of a
 *            new timer, which never fired.
 *          - The kernel takes nothing for many cycles: the loop says so,
 *            once, and says so again when the kernel takes them.
 *
 *          Setup
 *          -----
 *          io_uring_submit() is wrapped (-Wl,--wrap=io_uring_submit): it
 *          answers -EBUSY, without touching the ring, as many times as the
 *          test asks, and then calls the real one.
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

#define APP "test_yevent_sq_retry"

/*
 *  The cycles after which the loop says that the kernel still takes
 *  nothing (YEV_PENDING_CYCLES_ALARM in yev_loop.c)
 */
#define PENDING_CYCLES_ALARM    100

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
int result = 0;
int loop_timeouts = 0;

yev_event_h ev_a = NULL;
yev_event_h ev_c = NULL;
int fired_a = 0;
int stopped_a = 0;
int stopped_a_result = 0;
int fired_c = 0;
int fired_other = 0;

/*
 *  The wrap: how many of the next calls fail, and how many pass before
 *  the failures start
 */
int submit_pass_first = 0;
int submit_fail_count = 0;
int submit_failed = 0;

int __real_io_uring_submit(struct io_uring *ring);
int __wrap_io_uring_submit(struct io_uring *ring);

int __wrap_io_uring_submit(struct io_uring *ring)
{
    if(submit_pass_first > 0) {
        submit_pass_first--;
        return __real_io_uring_submit(ring);
    }
    if(submit_fail_count > 0) {
        submit_fail_count--;
        submit_failed++;
        return -EBUSY;
    }
    return __real_io_uring_submit(ring);
}

/***************************************************************************
 *  Fill the submission queue with NOPs, not submitted.
 *  HACK the ring is the first member of the loop (yev_loop.c).
 ***************************************************************************/
PRIVATE int fill_submission_queue(void)
{
    struct io_uring *ring = (struct io_uring *)yev_loop;
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
 *  The timeout of a loop run: what the run waited for did not happen
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
        return -1;  // break the loop
    }

    yev_state_t yev_state = yev_get_state(yev_event);
    if(yev_event == ev_a) {
        if(yev_state == YEV_ST_IDLE) {
            fired_a++;
        } else if(yev_state == YEV_ST_STOPPED) {
            stopped_a++;
            stopped_a_result = yev_get_result(yev_event);
            return 0;   // the run goes on: it waits for the other timer
        }
    } else if(yev_event == ev_c) {
        if(yev_state == YEV_ST_IDLE) {
            fired_c++;
        }
    } else {
        if(yev_state == YEV_ST_IDLE) {
            fired_other++;
        }
    }

    return -1;  // break the loop: each step checks what happened
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    yev_loop_create(
        0,
        8,
        10,
        loop_timeout_callback,  // a run that times out: what it waited for did not happen
        &yev_loop
    );

    /*------------------------------------------*
     *  1. A start whose submit fails, the queue
     *     NOT full: its entry stays in the
     *     queue, and the loop submits it
     *------------------------------------------*/
    set_expected_results(
        "1. a failed submit, the queue not full",
        json_pack("[{s:s}]",
            "msg", "Submissions the kernel did not take: submitted again at each cycle"
        ),
        NULL, NULL, 1
    );
    yev_event_h ev1 = yev_create_timer_event(yev_loop, yev_callback, NULL);
    submit_fail_count = 1;
    if(yev_start_timer_event(ev1, 100, FALSE) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "1. timer not started");
        result += -1;
    }
    loop_timeouts = 0;
    yev_loop_run(yev_loop, 2);
    if(fired_other != 1 || loop_timeouts != 0) {
        printf("%sERROR%s <-- 1. the timer whose submit failed did not fire (fired %d, loop timeouts %d)\n",
            On_Red BWhite, Color_Off, fired_other, loop_timeouts);
        result += -1;
    }
    result += test_json(NULL);

    /*------------------------------------------*
     *  2. A kept start, and the submit that
     *     hands it to the kernel fails: the
     *     loop submits it at the next cycle
     *------------------------------------------*/
    set_expected_results(
        "2. a kept entry whose hand-over fails",
        json_pack("[{s:s}]",
            "msg", "Submission queue full and the kernel takes nothing: kept for the next cycle"
        ),
        NULL, NULL, 1
    );
    fill_submission_queue();
    submit_fail_count = 2;          // the flush of get_sqe(), the submit of the caller
    if(yev_start_timer_event(ev1, 100, FALSE) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "2. timer not started");
        result += -1;
    }
    submit_pass_first = 1;          // the flush of the full queue passes...
    submit_fail_count = 1;          // ...and the submit of the kept entry fails
    loop_timeouts = 0;
    yev_loop_run(yev_loop, 2);
    if(fired_other != 2 || loop_timeouts != 0) {
        printf("%sERROR%s <-- 2. the kept timer did not fire (fired %d, loop timeouts %d)\n",
            On_Red BWhite, Color_Off, fired_other, loop_timeouts);
        result += -1;
    }
    result += test_json(NULL);

    /*------------------------------------------*
     *  3. A stop of a timer whose entry is in
     *     the queue: its fd is closed, a new
     *     timer takes the same fd number. The
     *     stale entry does not run on it.
     *------------------------------------------*/
    set_expected_results(
        "3. a stop takes its entry back from the queue",
        json_pack("[{s:s}]",
            "msg", "Submissions the kernel did not take: submitted again at each cycle"
        ),
        NULL, NULL, 1
    );
    ev_a = yev_create_timer_event(yev_loop, yev_callback, NULL);
    submit_fail_count = 1;
    yev_start_timer_event(ev_a, 10*1000, FALSE);    // its read stays in the queue
    int fd_a = yev_get_fd(ev_a);
    submit_fail_count = 100;    // nothing is taken until the loop runs
    if(yev_stop_event(ev_a) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "3. timer A not stopped");
        result += -1;
    }
    ev_c = yev_create_timer_event(yev_loop, yev_callback, NULL);
    yev_start_timer_event(ev_c, 50, FALSE);
    int fd_c = yev_get_fd(ev_c);
    if(fd_c != fd_a) {
        printf("%sERROR%s <-- 3. timer C did not reuse the fd of timer A (%d, %d): the case is not tested\n",
            On_Red BWhite, Color_Off, fd_c, fd_a);
        result += -1;
    }
    usleep(100*1000);           // C has expired before any entry reaches the kernel
    submit_fail_count = 0;
    loop_timeouts = 0;
    yev_loop_run(yev_loop, 2);
    if(fired_c != 1 || loop_timeouts != 0) {
        printf("%sERROR%s <-- 3. timer C did not fire: the stale read of A took it (fired %d, loop timeouts %d)\n",
            On_Red BWhite, Color_Off, fired_c, loop_timeouts);
        result += -1;
    }
    if(stopped_a != 1 || stopped_a_result != -ECANCELED || fired_a != 0) {
        printf("%sERROR%s <-- 3. timer A did not reach its callback as a cancel (stopped %d, result %d, fired %d)\n",
            On_Red BWhite, Color_Off, stopped_a, stopped_a_result, fired_a);
        result += -1;
    }
    result += test_json(NULL);

    /*------------------------------------------*
     *  4. The kernel takes nothing for many
     *     cycles: said once, and said again
     *     when it takes them
     *------------------------------------------*/
    set_expected_results(
        "4. the kernel takes nothing for many cycles",
        json_pack("[{s:s}, {s:s}, {s:s}]",
            "msg", "Submissions the kernel did not take: submitted again at each cycle",
            "msg", "Submissions not taken by the kernel for many cycles of the loop: their operations wait",
            "msg", "Submissions taken by the kernel again"
        ),
        NULL, NULL, 1
    );
    submit_fail_count = PENDING_CYCLES_ALARM + 10;
    submit_failed = 0;
    yev_start_timer_event(ev1, 10, FALSE);
    loop_timeouts = 0;
    yev_loop_run(yev_loop, 4);
    if(fired_other != 3 || loop_timeouts != 0 || submit_failed != PENDING_CYCLES_ALARM + 10) {
        printf("%sERROR%s <-- 4. the timer did not fire after the kernel took it (fired %d, loop timeouts %d, failed submits %d)\n",
            On_Red BWhite, Color_Off, fired_other, loop_timeouts, submit_failed);
        result += -1;
    }
    result += test_json(NULL);

    /*--------------------------------*
     *  Destroy
     *--------------------------------*/
    set_expected_results("destroy", NULL, NULL, NULL, 1);
    yev_destroy_event(ev1);
    yev_destroy_event(ev_a);
    yev_destroy_event(ev_c);
    yev_loop_destroy(yev_loop);
    result += test_json(NULL);

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

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    result += do_test();

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP)

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
