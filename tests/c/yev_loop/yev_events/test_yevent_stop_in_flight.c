/****************************************************************************
 *          test_yevent_stop_in_flight.c
 *
 *          A stop does not release the gbuffer of an operation that the
 *          kernel still has.
 *
 *          yev_stop_event() of a RUNNING read, write or recvmsg submits a
 *          cancel. The cancel is not done when it is submitted: until the
 *          completion of the operation arrives, the kernel can still write
 *          into the gbuffer (a read) or read from it (a write). Up to 7.25.4
 *          the stop released the gbuffer at once, so its memory could be
 *          freed and used again while the kernel still had it.
 *
 *          The rule now: the event keeps the gbuffer while it has a
 *          completion to come, and the loop releases it at the last
 *          completion, before the callback. The callback sees the event as
 *          before: STOPPED, with -ECANCELED, and without gbuffer.
 *
 *          How the test sees it (no ASan needed): the test keeps its own
 *          reference to the gbuffer of the event. While the event holds it,
 *          the refcount is 2; when the event releases it, it is 1.
 *
 *          Cases
 *          -----
 *          A.  A read with no data to read, stopped.
 *          B.  A write to a full socket, stopped.
 *          C.  A recvmsg with no datagram to receive, stopped.
 *          D.  The read event of A started again with a new gbuffer, as
 *              C_TCP does it when it connects again: it reads.
 *          E.  yev_set_gbuffer(NULL) on an event with its read in the
 *              kernel: the gbuffer is released at the last completion.
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
#include <gobj.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <yev_loop.h>
#include <helpers.h>

#define APP "test_yevent_stop_in_flight"

#define MESSAGE     "Data after the stop"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
int result = 0;

int callbacks = 0;
int cb_result = 0;
yev_state_t cb_state = YEV_ST_IDLE;
BOOL cb_had_gbuf = FALSE;
int cb_refcount = 0;
gbuffer_t *gbuf_probe = NULL;      // The test's own reference

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
 *  The callback of the event under test: what it sees, and the run ends
 ***************************************************************************/
PRIVATE int event_callback(yev_event_h yev_event)
{
    callbacks++;
    cb_result = yev_get_result(yev_event);
    cb_state = yev_get_state(yev_event);
    cb_had_gbuf = yev_get_gbuf(yev_event)? TRUE:FALSE;
    cb_refcount = gbuf_probe? (int)gbuf_probe->refcount : 0;
    return -1;  // break the loop
}

/***************************************************************************
 *  A new gbuffer for an event, with the test's own reference to it:
 *  empty with text "", full of data with text NULL
 ***************************************************************************/
PRIVATE gbuffer_t *probe_gbuffer(size_t size, const char *text)
{
    gbuffer_t *gbuf = gbuffer_create(size, size);
    if(text) {
        if(*text) {
            gbuffer_append_string(gbuf, text);
        }
    } else {
        char *p = gbuffer_cur_wr_pointer(gbuf);
        memset(p, 'x', size);
        gbuffer_set_wr(gbuf, size);
    }
    gbuf_probe = gbuffer_incref(gbuf);
    return gbuf;    // owned by the event
}

/***************************************************************************
 *  Stop the event, with its operation in the kernel, and check the rule
 ***************************************************************************/
PRIVATE void stop_and_check(const char *what, yev_event_h yev_event)
{
    char temp[256];

    if(yev_get_state(yev_event) != YEV_ST_RUNNING) {
        snprintf(temp, sizeof(temp), "%s: the event is %s before the stop, expected RUNNING",
            what, yev_get_state_name(yev_event)
        );
        fail(temp);
    }

    callbacks = 0;
    yev_stop_event(yev_event);

    /*
     *  The cancel is submitted, its completion is not reaped yet: the
     *  kernel may still use the gbuffer, the event must keep it
     */
    if(gbuf_probe->refcount != 2) {
        snprintf(temp, sizeof(temp),
            "%s: after the stop, gbuffer refcount %d, expected 2 (released before the completion)",
            what, (int)gbuf_probe->refcount
        );
        fail(temp);
    }

    yev_loop_run(yev_loop, 1);  // Broken by the callback

    if(callbacks != 1 || cb_state != YEV_ST_STOPPED || cb_result != -ECANCELED) {
        snprintf(temp, sizeof(temp),
            "%s: %d callbacks, state %d, result %d, expected 1 STOPPED %d",
            what, callbacks, (int)cb_state, cb_result, -ECANCELED
        );
        fail(temp);
    }
    if(cb_had_gbuf || cb_refcount != 1) {
        snprintf(temp, sizeof(temp),
            "%s: the callback sees a gbuffer %d, refcount %d, expected no gbuffer and 1",
            what, cb_had_gbuf, cb_refcount
        );
        fail(temp);
    }
    GBUFFER_DECREF(gbuf_probe)
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    char temp[256];

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
        yev_loop_callback,
        &yev_loop
    );

    /*
     *  A stream pair: the reads wait for data, the writes for room
     */
    int sv[2];
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        fail("cannot create a socketpair");
        yev_loop_destroy(yev_loop);
        return result;
    }

    /*------------------------------------------------------------*
     *  A.  A read with no data, stopped
     *------------------------------------------------------------*/
    yev_event_h yev_read = yev_create_read_event(
        yev_loop,
        event_callback,
        NULL,   // gobj
        sv[0],
        probe_gbuffer(1024, "")
    );
    yev_start_event(yev_read);
    stop_and_check("A read", yev_read);

    /*------------------------------------------------------------*
     *  B.  A write to a full socket, stopped
     *------------------------------------------------------------*/
    int sndbuf = 4096;
    setsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    char fill[4096];
    memset(fill, 'f', sizeof(fill));
    while(send(sv[1], fill, sizeof(fill), MSG_DONTWAIT) > 0) {
        // fill the socket
    }
    yev_event_h yev_write = yev_create_write_event(
        yev_loop,
        event_callback,
        NULL,   // gobj
        sv[1],
        probe_gbuffer(64*1024, NULL)
    );
    yev_start_event(yev_write);
    stop_and_check("B write", yev_write);
    yev_destroy_event(yev_write);

    /*------------------------------------------------------------*
     *  C.  A recvmsg with no datagram, stopped
     *------------------------------------------------------------*/
    int fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if(bind(fd_udp, (struct sockaddr *)&addr, sizeof(addr))<0) {
        fail("C: cannot bind the UDP socket");
    }
    yev_event_h yev_recvmsg = yev_create_recvmsg_event(
        yev_loop,
        event_callback,
        NULL,   // gobj
        fd_udp,
        probe_gbuffer(1024, "")
    );
    yev_start_event(yev_recvmsg);
    stop_and_check("C recvmsg", yev_recvmsg);
    yev_destroy_event(yev_recvmsg);
    close(fd_udp);

    /*------------------------------------------------------------*
     *  D.  The read of A again, with a new gbuffer (as C_TCP)
     *------------------------------------------------------------*/
    /*
     *  Empty the socket that B filled, so a write of the peer arrives
     */
    char drain[4096];
    while(recv(sv[0], drain, sizeof(drain), MSG_DONTWAIT) > 0) {
        // drain it
    }

    if(yev_get_gbuf(yev_read)) {
        fail("D: the stopped read still has a gbuffer");
    }
    yev_set_gbuffer(yev_read, gbuffer_create(1024, 1024));
    yev_start_event(yev_read);
    if(send(sv[1], MESSAGE, strlen(MESSAGE), 0) != (ssize_t)strlen(MESSAGE)) {
        fail("D: the peer cannot write");
    }
    callbacks = 0;
    yev_loop_run(yev_loop, 1);  // Broken by the callback
    if(callbacks != 1 || cb_state != YEV_ST_IDLE || cb_result != (int)strlen(MESSAGE) ||
            gbuffer_leftbytes(yev_get_gbuf(yev_read)) != strlen(MESSAGE) ||
            memcmp(gbuffer_cur_rd_pointer(yev_get_gbuf(yev_read)), MESSAGE, strlen(MESSAGE))!=0) {
        snprintf(temp, sizeof(temp), "D: %d callbacks, state %d, result %d, expected 1 IDLE %d",
            callbacks, (int)cb_state, cb_result, (int)strlen(MESSAGE)
        );
        fail(temp);
    }

    /*------------------------------------------------------------*
     *  E.  yev_set_gbuffer(NULL) with the read in the kernel
     *------------------------------------------------------------*/
    gbuffer_clear(yev_get_gbuf(yev_read));
    gbuf_probe = gbuffer_incref(yev_get_gbuf(yev_read));
    yev_start_event(yev_read);
    yev_set_gbuffer(yev_read, NULL);
    if(gbuf_probe->refcount != 2) {
        snprintf(temp, sizeof(temp),
            "E: after yev_set_gbuffer(NULL), gbuffer refcount %d, expected 2 (the read runs)",
            (int)gbuf_probe->refcount
        );
        fail(temp);
    }
    stop_and_check("E set NULL", yev_read);
    yev_destroy_event(yev_read);

    /*--------------------------------*
     *  End
     *--------------------------------*/
    close(sv[0]);
    close(sv[1]);

    yev_loop_stop(yev_loop);
    yev_loop_run_once(yev_loop);
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
    set_expected_results( // No log at all
        test,       // test name
        NULL,       // error_list
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
