/****************************************************************************
 *          test_yevent_kept_after_post.c
 *
 *          A completion the loop makes itself, from a POSTED action, is
 *          delivered at the next cycle -- not when some other completion
 *          happens to arrive.
 *
 *          A stop of an event whose submission the kernel has not taken
 *          (kept by the loop, test_yevent_sq_full) takes it back and makes
 *          its completion itself (-ECANCELED). The loop delivers those
 *          completions at the top of each cycle, BEFORE the posted events
 *          (gobj_post_event). So a posted action that stops such an event
 *          leaves a completion that was not there when the loop decided how
 *          to wait: up to 7.25.4 the loop then blocked on the ring, and the
 *          STOPPED waited for an unrelated completion -- or, as here, for
 *          the timeout of the loop run.
 *
 *          Setup
 *          -----
 *          A loop of 8 entries. The queue is filled with NOPs that are not
 *          submitted, and the fd of the ring is made invalid (the kernel
 *          takes nothing): a timer of 10 s starts KEPT. A posted event is
 *          left for a gobj.
 *
 *          Process
 *          -------
 *          The loop runs (timeout 3 s). Its first cycle cannot submit the
 *          kept timer, and delivers the posted event. The action gives the
 *          kernel its fd back, flushes the queue, and stops the timer: its
 *          submission is taken back. The STOPPED must reach the callback in
 *          the next cycle, before the timeout of the run.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <liburing.h>
#include <gobj.h>
#include <g_st_kernel.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <yev_loop.h>
#include <helpers.h>

#define APP "test_yevent_kept_after_post"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int yev_callback(yev_event_h yev_event);
PRIVATE int loop_timeout_callback(yev_event_h yev_event);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
yev_event_h yev_timer;
int result = 0;
int loop_timeouts = 0;
int stopped_counter = 0;
int stopped_result = 0;
int posted_delivered = 0;
int saved_ring_fd = -1;
int saved_enter_ring_fd = -1;

/***************************************************************************
 *  Fill the submission queue of the loop with NOPs, not submitted.
 *  HACK the ring is the first member of the loop (yev_loop.c).
 ***************************************************************************/
PRIVATE int fill_submission_queue(void)
{
    struct io_uring *ring = (struct io_uring *)yev_loop;
    io_uring_submit(ring);
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
 *  The kernel takes NOTHING: io_uring_enter() on an invalid fd answers
 *  EBADF. HACK the ring is the first member of the loop.
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

/***************************************************************
 *              GClass of the gobj that gets the posted event
 ***************************************************************/
#define C_KEPT_POSTER "C_KEPT_POSTER"
GOBJ_DECLARE_EVENT(EV_STOP_KEPT);
GOBJ_DEFINE_EVENT(EV_STOP_KEPT);

/*
 *  The posted action: the kernel takes submissions again, and the kept
 *  timer is stopped -- taken back, its completion made by the loop
 */
PRIVATE int ac_stop_kept(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    posted_delivered++;
    unblock_submissions();
    io_uring_submit((struct io_uring *)yev_loop);   // the NOPs: no completion
    if(yev_stop_event(yev_timer) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "the kept timer was not stopped");
        result += -1;
    }
    KW_DECREF(kw)
    return 0;
}

PRIVATE sdata_desc_t attrs_table[] = {
SDATA_END()
};

PRIVATE const GMETHODS gmt = {0};

PRIVATE int register_kept_poster(void)
{
    ev_action_t st_idle[] = {
        {EV_STOP_KEPT,  ac_stop_kept,   0},
        {0, 0, 0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };
    event_type_t event_types[] = {
        {EV_STOP_KEPT,  0},
        {0, 0}
    };
    hgclass gc = gclass_create(
        C_KEPT_POSTER,
        event_types,
        states,
        &gmt,
        0,              // lmt
        attrs_table,
        0,              // priv_size
        0,              // authz_table
        0,              // command_table
        0,              // trace_level
        0               // gclass_flag
    );
    return gc? 0: -1;
}

/***************************************************************************
 *  The timeout of the loop run: the STOPPED did not come in time
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
    if(yev_get_state(yev_event) == YEV_ST_STOPPED) {
        stopped_counter++;
        stopped_result = yev_get_result(yev_event);
    }
    return -1;  // break the loop: the test checks what happened
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(hgobj gobj)
{
    yev_loop_create(
        0,
        8,
        10,
        loop_timeout_callback,
        &yev_loop
    );

    yev_timer = yev_create_timer_event(yev_loop, yev_callback, NULL);

    fill_submission_queue();
    block_submissions();
    if(yev_start_timer_event(yev_timer, 10*1000, FALSE) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "timer not started (kept)");
        result += -1;
    }
    gobj_post_event(gobj, EV_STOP_KEPT, json_object(), gobj);

    uint64_t t0 = time_in_milliseconds_monotonic();
    yev_loop_run(yev_loop, 3);
    double tm = (double)(time_in_milliseconds_monotonic() - t0) / 1000.0;

    if(posted_delivered != 1) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "the posted event was not delivered");
        result += -1;
    }
    if(stopped_counter != 1 || stopped_result != -ECANCELED || loop_timeouts != 0) {
        printf("%sERROR%s <-- the STOPPED of the taken-back timer waited for the timeout of the run"
            " (stopped %d, result %d, loop timeouts %d, %.3f s)\n",
            On_Red BWhite, Color_Off, stopped_counter, stopped_result, loop_timeouts, tm);
        result += -1;
    }
    if(tm > 1.0) {
        printf("%sERROR%s <-- the STOPPED took %.3f s\n", On_Red BWhite, Color_Off, tm);
        result += -1;
    }

    yev_destroy_event(yev_timer);
    yev_loop_destroy(yev_loop);

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
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

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
    signal(SIGPIPE, SIG_IGN);

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    if(register_kept_poster() < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "gclass not created");
        gobj_end();
        return -1;
    }
    hgobj gobj = gobj_create_yuno("kept_poster", C_KEPT_POSTER, 0);

    set_expected_results(
        APP,
        json_pack("[{s:s}]",
            "msg", "Submission queue full and the kernel takes nothing: kept for the next cycle"
        ),
        NULL,
        NULL,
        1
    );

    result += do_test(gobj);
    result += test_json(NULL);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "system memory not free");
        print_track_mem();
        result += -1;
    }

    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    } else {
        printf("<-- %sTEST OK%s: %s\n", On_Green BWhite, Color_Off, APP);
    }
    return result < 0? -1: 0;
}
