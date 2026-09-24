/****************************************************************************
 *          test_yevent_udp_zerocopy.c
 *
 *          The completions of a zero-copy UDP send (YEV_SENDMSG_TYPE).
 *
 *          A zero-copy send (io_uring_prep_sendmsg_zc) completes TWICE:
 *          first the result of the send, flagged IORING_CQE_F_MORE, then a
 *          notification (IORING_CQE_F_NOTIF, res 0) when the kernel no longer
 *          uses the buffer. The loop counted one completion per submission,
 *          so a callback that destroyed its event at the first completion
 *          (what C_UDP_S does when all the data is sent) freed it, and the
 *          notification read the freed event (use-after-free, 7.25.4).
 *
 *          The rule now: the callback is told once, at the first completion.
 *          The notification is not delivered: it only ends the operation. An
 *          event destroyed before its notification keeps its buffer, and is
 *          freed when the notification arrives.
 *
 *          How the test sees it (no ASan needed): the test keeps its own
 *          reference to the gbuffer of the event. While the event lives, the
 *          refcount is 2; when the loop frees the event, it is 1. The event
 *          itself is never read after its destroy.
 *
 *          Cases
 *          -----
 *          A.  Destroy at the first completion (the C_UDP_S case). The
 *              callback is called once, with the bytes sent. The event is
 *              still alive after the callback (the notification is pending),
 *              and it is freed when the notification arrives.
 *          B.  The same event is sent again from its callback, before the
 *              notification of the first send. The notification of the first
 *              send arrives while the second send runs: it is not delivered
 *              as a completion of the second send.
 *          C.  A send that fails (a datagram too big, EMSGSIZE). The callback
 *              gets it STOPPED with -EMSGSIZE, once, and no warning is logged
 *              for its notification.
 *
 *          When the kernel has no zero-copy sendmsg, the loop uses a plain
 *          sendmsg: one completion, and the event is freed at once.
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

#define APP "test_yevent_udp_zerocopy"

#define MESSAGE1    "Zero-copy message one"
#define MESSAGE2    "Zero-copy message two"
#define BIG_SIZE    70000   // More than a UDP datagram can carry

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
int result = 0;
BOOL zerocopy = FALSE;

int send_callbacks = 0;
int send_result[4];
yev_state_t send_state[4];
BOOL send_again = FALSE;
gbuffer_t *gbuf_sent = NULL;       // The test's own reference

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
 *  The timer ends a run
 ***************************************************************************/
PRIVATE int timer_callback(yev_event_h yev_event)
{
    return -1;  // break the loop
}

/***************************************************************************
 *  The send callback
 ***************************************************************************/
PRIVATE int send_callback(yev_event_h yev_event)
{
    if(send_callbacks < 4) {
        send_result[send_callbacks] = yev_get_result(yev_event);
        send_state[send_callbacks] = yev_get_state(yev_event);
    }
    send_callbacks++;

    if(send_again && yev_get_state(yev_event) == YEV_ST_IDLE) {
        /*
         *  Case B: the same event again, before the notification of the
         *  first send
         */
        send_again = FALSE;
        gbuffer_append_string(yev_get_gbuf(yev_event), MESSAGE2);
        yev_start_event(yev_event);
        return 0;
    }

    yev_destroy_event(yev_event);
    return -1;  // break the loop: the notification is not reaped yet
}

/***************************************************************************
 *  Run the loop until a timer of 100 ms fires
 ***************************************************************************/
PRIVATE void run_loop_a_while(void)
{
    yev_event_h timer = yev_create_timer_event(yev_loop, timer_callback, 0);
    yev_start_timer_event(timer, 100, FALSE);
    yev_loop_run(yev_loop, 1);
    yev_destroy_event(timer);
}

/***************************************************************************
 *  Receive one datagram, without waiting
 ***************************************************************************/
PRIVATE int receive(int fd, char *bf, size_t bfsize)
{
    memset(bf, 0, bfsize);
    ssize_t n = recv(fd, bf, bfsize - 1, MSG_DONTWAIT);
    return (int)n;
}

/***************************************************************************
 *  Create a send event, with the test's own reference to its gbuffer
 ***************************************************************************/
PRIVATE yev_event_h create_send(int fd, struct sockaddr_in *addr, size_t size, const char *text)
{
    gbuffer_t *gbuf = gbuffer_create(size, size);
    if(text) {
        gbuffer_append_string(gbuf, text);
    } else {
        char *p = gbuffer_cur_wr_pointer(gbuf);
        memset(p, 'x', size);
        gbuffer_set_wr(gbuf, size);
    }
    gbuf_sent = gbuffer_incref(gbuf);

    return yev_create_sendmsg_event(
        yev_loop,
        send_callback,
        NULL,   // gobj
        fd,
        gbuf,   // owned by the event
        (struct sockaddr *)addr
    );
}

/***************************************************************************
 *  After the last run: the event is freed, only our reference is left
 ***************************************************************************/
PRIVATE void check_freed(const char *what)
{
    if(gbuf_sent->refcount != 1) {
        char temp[256];
        snprintf(temp, sizeof(temp), "%s: the event is not freed (gbuffer refcount %d, expected 1)",
            what, (int)gbuf_sent->refcount
        );
        fail(temp);
    }
    GBUFFER_DECREF(gbuf_sent)
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    char temp[256];
    char bf[256];

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
     *  Does this kernel have a zero-copy sendmsg? (the loop asks the same)
     */
    struct io_uring_probe *probe = io_uring_get_probe_ring((struct io_uring *)yev_loop);
    if(probe) {
        zerocopy = io_uring_opcode_supported(probe, IORING_OP_SENDMSG_ZC)? TRUE:FALSE;
        io_uring_free_probe(probe);
    }
    printf("  zero-copy sendmsg: %s\n", zerocopy? "yes":"no (plain sendmsg)");

    /*
     *  The receiver: a UDP socket on loopback, any port
     */
    int fd_rx = socket(AF_INET, SOCK_DGRAM, 0);
    int fd_tx = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t addrlen = sizeof(addr);
    if(bind(fd_rx, (struct sockaddr *)&addr, sizeof(addr))<0 ||
            getsockname(fd_rx, (struct sockaddr *)&addr, &addrlen)<0) {
        fail("cannot bind the receiver");
        close(fd_rx);
        close(fd_tx);
        yev_loop_destroy(yev_loop);
        return result;
    }

    /*------------------------------------------------------------*
     *  A.  Destroy at the first completion (C_UDP_S)
     *------------------------------------------------------------*/
    send_callbacks = 0;
    yev_event_h ev = create_send(fd_tx, &addr, 256, MESSAGE1);
    yev_start_event(ev);
    yev_loop_run(yev_loop, 1);  // Broken by the callback

    if(send_callbacks != 1) {
        snprintf(temp, sizeof(temp), "A: %d send callbacks, expected 1", send_callbacks);
        fail(temp);
    }
    if(send_result[0] != (int)strlen(MESSAGE1) || send_state[0] != YEV_ST_IDLE) {
        snprintf(temp, sizeof(temp), "A: callback result %d state %d, expected %d IDLE",
            send_result[0], (int)send_state[0], (int)strlen(MESSAGE1)
        );
        fail(temp);
    }
    /*
     *  With zero-copy the kernel still uses the buffer: the event is alive,
     *  holding it, until the notification arrives. Without it, the event is
     *  freed when its callback returns.
     */
    int alive_refcount = zerocopy? 2:1;
    if((int)gbuf_sent->refcount != alive_refcount) {
        snprintf(temp, sizeof(temp),
            "A: after the callback destroyed the event, gbuffer refcount %d, expected %d%s",
            (int)gbuf_sent->refcount, alive_refcount,
            zerocopy? " (the event was freed before its notification)":""
        );
        fail(temp);
    }

    run_loop_a_while();         // The notification arrives here

    if(send_callbacks != 1) {
        snprintf(temp, sizeof(temp), "A: %d send callbacks after the notification, expected 1",
            send_callbacks
        );
        fail(temp);
    }
    check_freed("A");

    if(receive(fd_rx, bf, sizeof(bf)) != (int)strlen(MESSAGE1) || strcmp(bf, MESSAGE1)!=0) {
        fail("A: the message was not received");
    }

    /*------------------------------------------------------------*
     *  B.  The same event again, before the first notification
     *------------------------------------------------------------*/
    send_callbacks = 0;
    send_again = TRUE;
    ev = create_send(fd_tx, &addr, 256, MESSAGE1);
    yev_start_event(ev);
    yev_loop_run(yev_loop, 1);  // Broken by the second callback

    if(send_callbacks != 2) {
        snprintf(temp, sizeof(temp), "B: %d send callbacks, expected 2", send_callbacks);
        fail(temp);
    }
    if(send_result[0] != (int)strlen(MESSAGE1) || send_state[0] != YEV_ST_IDLE ||
            send_result[1] != (int)strlen(MESSAGE2) || send_state[1] != YEV_ST_IDLE) {
        snprintf(temp, sizeof(temp),
            "B: callback results %d %d states %d %d, expected %d %d IDLE IDLE",
            send_result[0], send_result[1], (int)send_state[0], (int)send_state[1],
            (int)strlen(MESSAGE1), (int)strlen(MESSAGE2)
        );
        fail(temp);
    }

    run_loop_a_while();

    if(send_callbacks != 2) {
        snprintf(temp, sizeof(temp), "B: %d send callbacks after the notifications, expected 2",
            send_callbacks
        );
        fail(temp);
    }
    check_freed("B");

    if(receive(fd_rx, bf, sizeof(bf)) != (int)strlen(MESSAGE1) || strcmp(bf, MESSAGE1)!=0) {
        fail("B: the first message was not received");
    }
    if(receive(fd_rx, bf, sizeof(bf)) != (int)strlen(MESSAGE2) || strcmp(bf, MESSAGE2)!=0) {
        fail("B: the second message was not received");
    }

    /*------------------------------------------------------------*
     *  C.  A send that fails: EMSGSIZE
     *------------------------------------------------------------*/
    send_callbacks = 0;
    ev = create_send(fd_tx, &addr, BIG_SIZE, NULL);
    yev_start_event(ev);
    yev_loop_run(yev_loop, 1);  // Broken by the callback

    if(send_callbacks != 1 || send_result[0] != -EMSGSIZE || send_state[0] != YEV_ST_STOPPED) {
        snprintf(temp, sizeof(temp),
            "C: %d send callbacks, result %d state %d, expected 1 %d STOPPED",
            send_callbacks, send_result[0], (int)send_state[0], -EMSGSIZE
        );
        fail(temp);
    }

    run_loop_a_while();

    if(send_callbacks != 1) {
        snprintf(temp, sizeof(temp), "C: %d send callbacks after the notification, expected 1",
            send_callbacks
        );
        fail(temp);
    }
    check_freed("C");

    /*--------------------------------*
     *  End
     *--------------------------------*/
    close(fd_rx);
    close(fd_tx);

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
    set_expected_results( // No log at all: a notification must not be seen as a completion
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
