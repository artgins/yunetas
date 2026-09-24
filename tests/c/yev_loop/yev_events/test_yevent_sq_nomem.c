/****************************************************************************
 *          test_yevent_sq_nomem.c
 *
 *          No memory to keep a submission.
 *
 *          When the kernel takes no submission, the loop KEEPS them
 *          (test_yevent_sq_full), in a list that grows with
 *          gbmem_realloc(). When that list cannot grow, the caller answers
 *          -1 with an ERROR that says what did not happen ("No memory to
 *          keep a submission: event NOT started"). The start of a write
 *          and of a sendmsg aborted the process instead.
 *
 *          And a gbmem_realloc() that fails must leave the old block valid
 *          AND tracked: 7.25.4 took it out of the tracking before the size
 *          check, so the free of the kept list at the end logged "Wrong
 *          dl_item_t, WITHOUT link" and the memory counter wrapped.
 *
 *          Setup
 *          -----
 *          The largest block is 100000 bytes: the kept list holds 1024
 *          entries (65536 bytes) and cannot grow to 2048. The queue is
 *          filled with NOPs and io_uring_submit() fails
 *          (-Wl,--wrap=io_uring_submit) while the writes start.
 *
 *          Process
 *          -------
 *          1. 1025 writes to a pipe start: 1024 are kept, the last one
 *             answers -1 (no abort).
 *          2. The kernel takes submissions again: the 1024 writes complete.
 *          3. Everything is freed: the memory tracking is whole.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <liburing.h>
#include <gobj.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <yev_loop.h>
#include <helpers.h>

#define APP "test_yevent_sq_nomem"

#define MAX_BLOCK   100000
#define N_KEPT      1024
#define N_WRITES    (N_KEPT + 1)

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int yev_callback(yev_event_h yev_event);
PRIVATE int loop_timeout_callback(yev_event_h yev_event);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
int result = 0;
int loop_timeouts = 0;
int writes_done = 0;
yev_event_h writes[N_WRITES];

int submit_fail_count = 0;

int __real_io_uring_submit(struct io_uring *ring);
int __wrap_io_uring_submit(struct io_uring *ring);

int __wrap_io_uring_submit(struct io_uring *ring)
{
    if(submit_fail_count > 0) {
        submit_fail_count--;
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
 *
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
    if(yev_get_state(yev_event) == YEV_ST_IDLE) {
        writes_done++;
    }
    return writes_done == N_KEPT? -1 : 0;
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int fds[2];
    if(pipe(fds) < 0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "no pipe");
        return -1;
    }

    yev_loop_create(
        0,
        8,
        10,
        loop_timeout_callback,
        &yev_loop
    );

    /*------------------------------------------*
     *  1. The kept list cannot grow
     *------------------------------------------*/
    set_expected_results(
        "1. no memory to keep a write",
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Submission queue full and the kernel takes nothing: kept for the next cycle",
            "msg", "SIZE GREATER THAN MAX_BLOCK",
            "msg", "No memory to keep a submission",
            "msg", "No memory to keep a submission: event NOT started"
        ),
        NULL, NULL, 1
    );
    fill_submission_queue();
    submit_fail_count = 1000000;
    int refused = 0;
    for(int i = 0; i < N_WRITES; i++) {
        gbuffer_t *gbuf = gbuffer_create(16, 16);
        gbuffer_append_string(gbuf, "0123456789abcde");
        writes[i] = yev_create_write_event(yev_loop, yev_callback, NULL, fds[1], gbuf);
        if(yev_start_event(writes[i]) < 0) {
            refused++;
        }
    }
    if(refused != 1) {
        printf("%sERROR%s <-- 1. %d writes refused, expected 1\n", On_Red BWhite, Color_Off, refused);
        result += -1;
    }
    result += test_json(NULL);

    /*------------------------------------------*
     *  2. The kernel takes them again
     *------------------------------------------*/
    set_expected_results("2. the kept writes complete", NULL, NULL, NULL, 1);
    submit_fail_count = 0;
    yev_loop_run(yev_loop, 2);
    if(writes_done != N_KEPT || loop_timeouts != 0) {
        printf("%sERROR%s <-- 2. %d writes done, expected %d (loop timeouts %d)\n",
            On_Red BWhite, Color_Off, writes_done, N_KEPT, loop_timeouts);
        result += -1;
    }
    result += test_json(NULL);

    /*------------------------------------------*
     *  3. Free everything
     *------------------------------------------*/
    set_expected_results("3. destroy", NULL, NULL, NULL, 1);
    for(int i = 0; i < N_WRITES; i++) {
        yev_destroy_event(writes[i]);
    }
    yev_loop_destroy(yev_loop);
    close(fds[0]);
    close(fds[1]);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    gbmem_setup(
        MAX_BLOCK,  // mem_max_block
        0,          // mem_max_system_memory, the default
        FALSE,      // use_own_system_memory
        0,          // mem_min_block
        0           // mem_superblock
    );

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

    signal(SIGPIPE, SIG_IGN);

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    result += do_test();

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
