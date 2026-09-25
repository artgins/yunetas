/****************************************************************************
 *          test_yevent_stop_stale_sqe.c
 *
 *          A stop of a RUNNING timer or connect event does not take back a
 *          submission that is not there.
 *
 *          The stop of a RUNNING event asks for a submission queue entry
 *          (get_sqe) for its cancel. The stop of a timer or a connect also
 *          closes the fd of the event, and before the close it takes back
 *          the submissions on that fd that the kernel has not taken
 *          (take_back_submissions_on_fd), which scans the queue from its
 *          head to its tail. io_uring_get_sqe() moves the tail first, and
 *          io_uring_initialize_sqe() does not clear the fd or the
 *          user_data of the entry. Up to this fix the scan ran BETWEEN the
 *          two, so it read the entry of the cancel with the fd and the
 *          event of the operation that used that slot the last time. A
 *          wrapped ring (every long-running yuno wraps it) and a reused fd
 *          number were enough: the old event was "taken back", and got a
 *          completion it never asked for -- an extra STOPPED callback, a
 *          wrong in_flight, and, with the old event destroyed, a completion
 *          on freed memory.
 *
 *          Cases (a ring of 4 entries, so that it wraps at once)
 *          -----
 *          A.  A timer armed 4 times (every slot of the ring holds its fd
 *              and its pointer), armed again, and stopped RUNNING: exactly
 *              one STOPPED callback, where it is destroyed. No warning, no
 *              completion after that.
 *          B.  A read event reads 4 times on the socket of a connect event
 *              (every slot holds that fd and the READ event), the socket
 *              is closed, a new connect event takes its number and is
 *              stopped RUNNING: the read event is not touched, and the
 *              connect event gets exactly one STOPPED callback.
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

#define APP "test_yevent_stop_stale_sqe"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
int result = 0;

yev_event_h yev_timer = 0;
int timer_callbacks = 0;
int timer_stopped_callbacks = 0;

int connect_callbacks = 0;
int connect_stopped_callbacks = 0;

int read_callbacks = 0;

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
 *  The timer is destroyed in its STOPPED callback: a completion after it
 *  would land on freed memory
 ***************************************************************************/
PRIVATE int timer_callback(yev_event_h yev_event)
{
    timer_callbacks++;
    if(yev_get_state(yev_event) == YEV_ST_STOPPED) {
        timer_stopped_callbacks++;
        if(yev_event == yev_timer) {
            yev_destroy_event(yev_event);
            yev_timer = 0;
        }
    }
    return -1;  // break the loop
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int connect_callback(yev_event_h yev_event)
{
    connect_callbacks++;
    if(yev_get_state(yev_event) == YEV_ST_STOPPED) {
        connect_stopped_callbacks++;
    }
    return -1;  // break the loop
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int read_callback(yev_event_h yev_event)
{
    read_callbacks++;
    return -1;  // break the loop
}

/***************************************************************************
 *  Case A: a timer stopped RUNNING in a wrapped ring
 ***************************************************************************/
PRIVATE void case_timer(void)
{
    char temp[256];

    yev_timer = yev_create_timer_event(yev_loop, timer_callback, 0);

    /*
     *  Every slot of the ring gets the fd and the pointer of the timer
     */
    for(int i = 0; i < 4; i++) {
        yev_start_timer_event(yev_timer, 1, FALSE);
        yev_loop_run(yev_loop, 1);
    }
    if(timer_callbacks != 4 || timer_stopped_callbacks != 0) {
        snprintf(temp, sizeof(temp),
            "A: timer callbacks %d (stopped %d), expected 4 (0)",
            timer_callbacks, timer_stopped_callbacks
        );
        fail(temp);
    }

    timer_callbacks = 0;
    yev_start_timer_event(yev_timer, 10000, FALSE);
    if(yev_get_state(yev_timer) != YEV_ST_RUNNING) {
        fail("A: the timer is not RUNNING");
    }
    yev_stop_event(yev_timer);

    /*
     *  The STOPPED callback, where it is destroyed; then one more run, where
     *  a false completion would come
     */
    yev_loop_run(yev_loop, 1);
    yev_loop_run(yev_loop, 1);

    if(timer_callbacks != 1 || timer_stopped_callbacks != 1) {
        snprintf(temp, sizeof(temp),
            "A: timer callbacks %d (stopped %d), expected 1 (1)",
            timer_callbacks, timer_stopped_callbacks
        );
        fail(temp);
    }
    if(yev_timer) {
        fail("A: the timer was not destroyed in its STOPPED callback");
        yev_destroy_event(yev_timer);
        yev_timer = 0;
    }
}

/***************************************************************************
 *  Case B: a connect event stopped RUNNING on an fd number whose old
 *  submissions belong to a read event
 ***************************************************************************/
PRIVATE void case_connect(int fd_listen, int port)
{
    char temp[256];
    char url[64];
    snprintf(url, sizeof(url), "tcp://127.0.0.1:%d", port);

    yev_event_h yev_connect = yev_create_connect_event(
        yev_loop,
        connect_callback,
        url,
        NULL,       // src_url
        AF_INET,    // ai_family
        0,          // ai_flags
        0           // gobj
    );
    yev_start_event(yev_connect);
    yev_loop_run(yev_loop, 2);
    if(connect_callbacks != 1 || !(yev_get_flag(yev_connect) & YEV_FLAG_CONNECTED)) {
        fail("B: the connect event did not connect");
        return;
    }
    int fd = yev_get_fd(yev_connect);
    int fd_peer = accept(fd_listen, NULL, NULL);
    if(fd_peer < 0) {
        fail("B: accept() FAILED");
        return;
    }

    /*
     *  Every slot of the ring gets the fd of the connect and the READ event
     */
    yev_event_h yev_read = yev_create_read_event(
        yev_loop, read_callback, 0, fd, gbuffer_create(64, 64)
    );
    for(int i = 0; i < 4; i++) {
        gbuffer_clear(yev_get_gbuf(yev_read));
        yev_start_event(yev_read);
        if(write(fd_peer, "x", 1) != 1) {
            fail("B: write() FAILED");
        }
        yev_loop_run(yev_loop, 1);
    }
    if(read_callbacks != 4) {
        snprintf(temp, sizeof(temp), "B: read callbacks %d, expected 4", read_callbacks);
        fail(temp);
    }

    /*
     *  The socket closed; a new connect event takes its number
     */
    yev_stop_event(yev_connect);    // IDLE: the stop closes the socket
    yev_destroy_event(yev_connect);

    connect_callbacks = 0;
    read_callbacks = 0;
    yev_connect = yev_create_connect_event(
        yev_loop,
        connect_callback,
        url,
        NULL,       // src_url
        AF_INET,    // ai_family
        0,          // ai_flags
        0           // gobj
    );
    if(yev_get_fd(yev_connect) != fd) {
        snprintf(temp, sizeof(temp),
            "B: the new connect did not take the number %d (%d): the case shows nothing",
            fd, yev_get_fd(yev_connect)
        );
        fail(temp);
    }
    yev_start_event(yev_connect);
    if(yev_get_state(yev_connect) != YEV_ST_RUNNING) {
        fail("B: the connect is not RUNNING");
    }
    yev_stop_event(yev_connect);

    yev_loop_run(yev_loop, 1);
    yev_loop_run(yev_loop, 1);

    if(read_callbacks != 0) {
        snprintf(temp, sizeof(temp),
            "B: the read event, idle, got %d callbacks from the stop of another event",
            read_callbacks
        );
        fail(temp);
    }
    if(connect_callbacks != 1 || connect_stopped_callbacks != 1) {
        snprintf(temp, sizeof(temp),
            "B: connect callbacks %d (stopped %d), expected 1 (1)",
            connect_callbacks, connect_stopped_callbacks
        );
        fail(temp);
    }

    yev_destroy_event(yev_connect);
    yev_destroy_event(yev_read);
    int fd_accepted = accept(fd_listen, NULL, NULL);
    if(fd_accepted >= 0) {
        close(fd_accepted);
    }
    close(fd_peer);
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int fd_listen = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t addrlen = sizeof(addr);
    if(bind(fd_listen, (struct sockaddr *)&addr, sizeof(addr))<0 ||
            listen(fd_listen, 4)<0 ||
            getsockname(fd_listen, (struct sockaddr *)&addr, &addrlen)<0) {
        fail("cannot create the listener");
        return result;
    }
    int port = ntohs(addr.sin_port);

    yev_loop_create(
        0,
        4,      // a small ring: it wraps at once
        10,
        yev_loop_callback,
        &yev_loop
    );

    case_timer();
    case_connect(fd_listen, port);

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);
    close(fd_listen);

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

    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);

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
        json_array(), // error_list: nothing may be taken back, nothing may be warned
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
