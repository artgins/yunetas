/****************************************************************************
 *          test_yevent_stop_nomem.c
 *
 *          A stop that fails for lack of memory gives up nothing, and a
 *          loop that ends without room for its cancel says so.
 *
 *          yev_stop_event() of a RUNNING event needs a submission entry
 *          for the cancel. Without one (the queue full, the kernel taking
 *          nothing, and no memory to keep the submission) it answers -1
 *          and the event stays RUNNING: its operation is in the kernel,
 *          uncanceled. Up to 7.25.4 the stop had already given up the
 *          gbuffer (released at the next completion) and closed the fd of
 *          a timer or a connect: the read then completed normally with no
 *          gbuffer, and C_TCP called gbuffer_leftbytes(NULL).
 *
 *          And yev_loop_destroy() cancels what the kernel still has of the
 *          events destroyed with completions to come (free_dying_events).
 *          When it has no entry for that cancel, it now says so; up to
 *          7.25.4 only the final error was logged, blaming the accounting
 *          of the completions for a cancel never sent.
 *
 *          Setup
 *          -----
 *          The largest block is 100000 bytes: the kept list holds 1024
 *          entries (65536 bytes) and cannot grow to 2048. The queue is
 *          filled with NOPs and io_uring_submit() fails
 *          (-Wl,--wrap=io_uring_submit) while 1024 writes are kept.
 *
 *          Process
 *          -------
 *          1. A read of a pipe (with its gbuffer) and a timer of 10 s run
 *             in the kernel. The kept list is filled.
 *          2. Both stops answer -1: the timer keeps its fd, the read its
 *             gbuffer (no release pending).
 *          3. The kernel takes submissions again, the pipe gets data: the
 *             read completes with its gbuffer and the data in it.
 *          4. Queue full and nothing taken again: the timer is destroyed
 *             RUNNING (its cancel is kept), and the loop is destroyed with
 *             no entry for the cancel of the events left: that is logged
 *             before the final error.
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

#define APP "test_yevent_stop_nomem"

#define MAX_BLOCK   100000
#define N_KEPT      1024

#define MSG_KEPT        "Submission queue full and the kernel takes nothing: kept for the next cycle"
#define MSG_MAX_BLOCK   "SIZE GREATER THAN MAX_BLOCK"
#define MSG_NO_KEEP     "No memory to keep a submission"
#define MSG_NOT_CANCEL  "No memory to keep a submission: event NOT canceled"

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
int reads_done = 0;
size_t read_bytes_in_gbuffer = 0;
BOOL read_had_gbuffer = FALSE;
yev_event_h writes[N_KEPT];
yev_event_h yev_read;
yev_event_h yev_timer;

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
    if(yev_get_state(yev_event) != YEV_ST_IDLE) {
        return 0;
    }
    switch(yev_get_type(yev_event)) {
        case YEV_WRITE_TYPE:
            writes_done++;
            break;
        case YEV_READ_TYPE:
            reads_done++;
            read_had_gbuffer = yev_get_gbuf(yev_event)? TRUE: FALSE;
            if(read_had_gbuffer) {
                read_bytes_in_gbuffer = gbuffer_leftbytes(yev_get_gbuf(yev_event));
            }
            break;
        default:
            break;
    }
    return (writes_done == N_KEPT && reads_done == 1)? -1 : 0;
}

/***************************************************************************
 *  Queue full, the kernel takes nothing, and the kept list full
 ***************************************************************************/
PRIVATE int fill_kept_list(int fd)
{
    fill_submission_queue();
    submit_fail_count = 1000000;
    int refused = 0;
    for(int i = 0; i < N_KEPT; i++) {
        gbuffer_t *gbuf = gbuffer_create(16, 16);
        gbuffer_append_string(gbuf, "0123456789abcde");
        writes[i] = yev_create_write_event(yev_loop, yev_callback, NULL, fd, gbuf);
        if(yev_start_event(writes[i]) < 0) {
            refused++;
        }
    }
    return refused;
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int fds_read[2];
    int fds_write[2];
    if(pipe(fds_read) < 0 || pipe(fds_write) < 0) {
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
     *  1. A read and a timer in the kernel,
     *     and the kept list full
     *------------------------------------------*/
    set_expected_results(
        "1. a read and a timer run, the kept list is full",
        json_pack("[{s:s}]",
            "msg", MSG_KEPT
        ),
        NULL, NULL, 1
    );
    yev_read = yev_create_read_event(
        yev_loop, yev_callback, NULL, fds_read[0], gbuffer_create(64, 64)
    );
    yev_timer = yev_create_timer_event(yev_loop, yev_callback, NULL);
    if(yev_start_event(yev_read) < 0 || yev_start_timer_event(yev_timer, 10*1000, FALSE) < 0) {
        printf("%sERROR%s <-- 1. the read or the timer did not start\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    int refused = fill_kept_list(fds_write[1]);
    if(refused != 0) {
        printf("%sERROR%s <-- 1. %d writes refused, expected 0\n", On_Red BWhite, Color_Off, refused);
        result += -1;
    }
    result += test_json(NULL);

    /*------------------------------------------*
     *  2. The stops fail, and give up nothing
     *------------------------------------------*/
    set_expected_results(
        "2. the stops fail for lack of memory, and give up nothing",
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", MSG_MAX_BLOCK,
            "msg", MSG_NO_KEEP,
            "msg", MSG_NOT_CANCEL,
            "msg", MSG_MAX_BLOCK,
            "msg", MSG_NO_KEEP,
            "msg", MSG_NOT_CANCEL
        ),
        NULL, NULL, 1
    );
    if(yev_stop_event(yev_read) != -1) {
        printf("%sERROR%s <-- 2. the stop of the read did not answer -1\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(yev_stop_event(yev_timer) != -1) {
        printf("%sERROR%s <-- 2. the stop of the timer did not answer -1\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!yev_event_is_running(yev_read) || !yev_event_is_running(yev_timer)) {
        printf("%sERROR%s <-- 2. an event whose stop failed is not RUNNING\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(yev_read->gbuf_release_pending || !yev_get_gbuf(yev_read)) {
        printf("%sERROR%s <-- 2. the read whose stop failed gave up its gbuffer\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(yev_get_fd(yev_timer) < 0) {
        printf("%sERROR%s <-- 2. the timer whose stop failed has its fd closed\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*------------------------------------------*
     *  3. The read completes with its data
     *------------------------------------------*/
    set_expected_results("3. the read completes with its gbuffer", NULL, NULL, NULL, 1);
    submit_fail_count = 0;
    if(write(fds_read[1], "hello", 5) != 5) {
        printf("%sERROR%s <-- 3. cannot write the pipe\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    yev_loop_run(yev_loop, 2);
    if(reads_done != 1 || !read_had_gbuffer || read_bytes_in_gbuffer != 5) {
        printf("%sERROR%s <-- 3. the read completed %d time(s), with gbuffer %d, %d bytes in it\n",
            On_Red BWhite, Color_Off, reads_done, read_had_gbuffer, (int)read_bytes_in_gbuffer);
        result += -1;
    }
    if(writes_done != N_KEPT || loop_timeouts != 0) {
        printf("%sERROR%s <-- 3. %d writes done, expected %d (loop timeouts %d)\n",
            On_Red BWhite, Color_Off, writes_done, N_KEPT, loop_timeouts);
        result += -1;
    }
    for(int i = 0; i < N_KEPT; i++) {
        yev_destroy_event(writes[i]);
        writes[i] = 0;
    }
    yev_destroy_event(yev_read);
    result += test_json(NULL);

    /*------------------------------------------*
     *  4. The timer destroyed RUNNING, its
     *     cancel kept, and the loop destroyed
     *     with no room for its own cancel
     *------------------------------------------*/
    set_expected_results(
        "4. the loop ends with no room for the cancel of the events left",
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Destroying a running event, stop it",
            "msg", MSG_KEPT,
            "msg", "Submission queue full: the cancel of the events left is NOT submitted, their completions may not come",
            "msg", "Loop destroyed with events whose completions did not come: freed"
        ),
        NULL, NULL, 1
    );
    fill_submission_queue();
    submit_fail_count = 1000000;
    yev_destroy_event(yev_timer);   // RUNNING: its cancel is kept, and it is dying
    yev_loop_destroy(yev_loop);
    submit_fail_count = 0;
    result += test_json(NULL);

    close(fds_read[0]);
    close(fds_read[1]);
    close(fds_write[0]);
    close(fds_write[1]);

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

    result += do_test();

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
