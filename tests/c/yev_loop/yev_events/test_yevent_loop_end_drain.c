/****************************************************************************
 *          test_yevent_loop_end_drain.c
 *
 *          An event destroyed with completions still to come is freed, even
 *          when the loop ends before they arrive.
 *
 *          An event destroyed while the kernel has one of its operations is
 *          not freed at once: the loop frees it at its last completion. Up
 *          to 7.25.4 two things went wrong at the end of a loop:
 *            - an event destroyed while the loop ran, whose completion did
 *              not arrive before the loop ended, was never freed (a leak
 *              of the event, its gbuffer and its buffers),
 *            - an event destroyed after the loop ended was freed at once,
 *              while the kernel could still have its read or its write.
 *
 *          The rule now: every destroyed event with completions to come
 *          waits for them, and yev_loop_destroy() cancels what the kernel
 *          still has, reaps the completions (for 1 second at most), and
 *          frees what is left, with an error in the log.
 *
 *          How the test sees it (no ASan needed): the test keeps its own
 *          reference to the gbuffer of the event. While the event lives, the
 *          refcount is 2; when the loop frees the event, it is 1. And the
 *          memory is checked at the end (CONFIG_DEBUG_TRACK_MEMORY).
 *
 *          Cases (a new loop for each one)
 *          -----
 *          A.  A read destroyed while the loop runs (from a timer), and the
 *              loop ends before the completion of its cancel.
 *          B.  A zero-copy send destroyed in its callback, and the loop ends
 *              before its notification (only when the kernel has it).
 *          C.  A read destroyed after the loop ended: it is not freed while
 *              the kernel has the read, but by yev_loop_destroy().
 *          D.  An event whose completion never comes (the test counts one
 *              more operation than the kernel has): yev_loop_destroy()
 *              waits 1 second, frees it, and logs an error.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <liburing.h>
#include <gobj.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <yev_loop.h>
#include <helpers.h>

#define APP "test_yevent_loop_end_drain"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
int result = 0;

yev_event_h yev_to_destroy = NULL;  // destroyed by the timer
gbuffer_t *gbuf_probe = NULL;       // The test's own reference

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void fail(const char *what)
{
    printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, what);
    result += -1;
}

/***************************************************************************
 *  The loop callback: its timeout ends a run
 ***************************************************************************/
PRIVATE int yev_loop_callback(yev_event_h yev_event)
{
    if(!yev_event) {
        return -1;  // break the loop
    }
    return 0;
}

/***************************************************************************
 *  The read callback: it must not be called after the destroy
 ***************************************************************************/
PRIVATE int read_callback(yev_event_h yev_event)
{
    fail("the callback of a destroyed event was called");
    return -1;
}

/***************************************************************************
 *  The timer destroys the read while the loop runs, and ends the run
 ***************************************************************************/
PRIVATE int timer_callback(yev_event_h yev_event)
{
    if(yev_to_destroy) {
        yev_stop_event(yev_to_destroy);     // the cancel is submitted
        yev_destroy_event(yev_to_destroy);  // the free waits for the completions
        yev_to_destroy = NULL;
    }
    return -1;  // break the loop: the completion of the cancel is not reaped
}

/***************************************************************************
 *  The send destroys itself at its first completion, and ends the run
 ***************************************************************************/
PRIVATE int send_callback(yev_event_h yev_event)
{
    yev_destroy_event(yev_event);
    return -1;  // break the loop: the notification is not reaped
}

/***************************************************************************
 *  A read event on a socket with no data, with the test's own reference
 *  to its gbuffer
 ***************************************************************************/
PRIVATE yev_event_h create_read(int fd)
{
    gbuffer_t *gbuf = gbuffer_create(1024, 1024);
    gbuf_probe = gbuffer_incref(gbuf);
    return yev_create_read_event(
        yev_loop,
        read_callback,
        NULL,   // gobj
        fd,
        gbuf    // owned by the event
    );
}

/***************************************************************************
 *  The gbuffer is held by the event (2) or the event is freed (1)
 ***************************************************************************/
PRIVATE void check_refcount(const char *what, int expected)
{
    if((int)gbuf_probe->refcount != expected) {
        char temp[256];
        snprintf(temp, sizeof(temp), "%s: gbuffer refcount %d, expected %d (%s)",
            what, (int)gbuf_probe->refcount, expected,
            expected == 2? "the event is freed too soon":"the event is not freed"
        );
        fail(temp);
    }
}

/***************************************************************************
 *  A new loop
 ***************************************************************************/
PRIVATE void new_loop(void)
{
    yev_loop_create(
        0,
        2024,
        10,
        yev_loop_callback,
        &yev_loop
    );
}

/***************************************************************************
 *  The loop ends without reaping what is left: stop, and destroy
 ***************************************************************************/
PRIVATE void end_loop(void)
{
    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);
    yev_loop = NULL;
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int sv[2];
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        fail("cannot create a socketpair");
        return result;
    }

    /*------------------------------------------------------------*
     *  A.  A read destroyed while the loop runs, the loop ends
     *      before the completion of its cancel
     *------------------------------------------------------------*/
    new_loop();
    yev_event_h yev_read = create_read(sv[0]);
    yev_start_event(yev_read);
    yev_to_destroy = yev_read;
    yev_event_h timer = yev_create_timer_event(yev_loop, timer_callback, 0);
    yev_start_timer_event(timer, 10, FALSE);
    yev_loop_run(yev_loop, 1);      // Broken by the timer
    yev_destroy_event(timer);

    check_refcount("A: after the destroy, while the kernel has the read", 2);
    end_loop();
    check_refcount("A: after yev_loop_destroy()", 1);
    GBUFFER_DECREF(gbuf_probe)

    /*------------------------------------------------------------*
     *  B.  A zero-copy send destroyed in its callback, the loop
     *      ends before its notification
     *------------------------------------------------------------*/
    new_loop();
    BOOL zerocopy = FALSE;
    struct io_uring_probe *probe = io_uring_get_probe_ring((struct io_uring *)yev_loop);
    if(probe) {
        zerocopy = io_uring_opcode_supported(probe, IORING_OP_SENDMSG_ZC)? TRUE:FALSE;
        io_uring_free_probe(probe);
    }
    int fd_rx = socket(AF_INET, SOCK_DGRAM, 0);
    int fd_tx = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t addrlen = sizeof(addr);
    if(bind(fd_rx, (struct sockaddr *)&addr, sizeof(addr))<0 ||
            getsockname(fd_rx, (struct sockaddr *)&addr, &addrlen)<0) {
        fail("B: cannot bind the receiver");
    }
    gbuffer_t *gbuf = gbuffer_create(256, 256);
    gbuffer_append_string(gbuf, "the last datagram");
    gbuf_probe = gbuffer_incref(gbuf);
    yev_event_h yev_send = yev_create_sendmsg_event(
        yev_loop,
        send_callback,
        NULL,   // gobj
        fd_tx,
        gbuf,   // owned by the event
        (struct sockaddr *)&addr,
        sizeof(addr)
    );
    yev_start_event(yev_send);
    yev_loop_run(yev_loop, 1);      // Broken by the send callback

    if(zerocopy) {
        check_refcount("B: after the destroy, before the notification", 2);
    }
    end_loop();
    check_refcount("B: after yev_loop_destroy()", 1);
    GBUFFER_DECREF(gbuf_probe)
    close(fd_rx);
    close(fd_tx);

    /*------------------------------------------------------------*
     *  C.  A read destroyed after the loop ended
     *------------------------------------------------------------*/
    new_loop();
    yev_read = create_read(sv[0]);
    yev_start_event(yev_read);      // The loop does not run: the read is in the kernel
    yev_stop_event(yev_read);       // the cancel is submitted
    yev_destroy_event(yev_read);    // the free waits for the completions

    check_refcount("C: after the destroy, with the loop not running", 2);
    end_loop();
    check_refcount("C: after yev_loop_destroy()", 1);
    GBUFFER_DECREF(gbuf_probe)

    /*------------------------------------------------------------*
     *  D.  A completion that never comes
     *------------------------------------------------------------*/
    new_loop();
    yev_read = create_read(sv[0]);
    yev_start_event(yev_read);
    yev_read->in_flight++;          // One operation more than the kernel has
    yev_stop_event(yev_read);
    yev_destroy_event(yev_read);

    uint64_t t0 = time_in_milliseconds_monotonic();
    end_loop();
    uint64_t waited = time_in_milliseconds_monotonic() - t0;

    check_refcount("D: after yev_loop_destroy()", 1);
    GBUFFER_DECREF(gbuf_probe)
    if(waited < 900 || waited > 3000) {
        char temp[256];
        snprintf(temp, sizeof(temp), "D: yev_loop_destroy() waited %d ms, expected about 1000",
            (int)waited
        );
        fail(temp);
    }

    /*--------------------------------*
     *  End
     *--------------------------------*/
    close(sv[0]);
    close(sv[1]);

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
        NULL   // global_authentication_parser
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

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    const char *test = APP;
    set_expected_results(
        test,       // test name
        json_pack("[{s:s}]",  // error_list: only case D logs
            "msg", "Loop destroyed with events whose completions did not come: freed"
        ),
        NULL,       // expected
        NULL,       // ignore_keys
        1           // verbose
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
    if(yev_loop) {
        yev_loop_reset_running(yev_loop);
    }
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
