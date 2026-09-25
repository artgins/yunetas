/****************************************************************************
 *          test_yevent_close_fd_kept.c
 *
 *          An fd closed by the loop takes back the submissions of OTHER
 *          events on it that the kernel has not taken yet.
 *
 *          A connect event owns its socket: the loop closes it when the
 *          event is stopped (release_on_stop) or freed
 *          (really_free_yev_event). The writes of C_TCP are other events on
 *          the same fd. When io_uring_enter() refuses submissions (EBUSY,
 *          EAGAIN: the case kept submissions exist for), a write can be
 *          still untaken -- kept by the loop, or in the submission queue --
 *          when the socket is closed. Handed to the kernel at the next
 *          cycle, it ran on whatever file had taken the number by then: the
 *          bytes of the old connection went to a NEW peer, and the write
 *          callback said it was sent.
 *
 *          Now every untaken submission on the fd is taken back before the
 *          close, and its event completes as canceled (STOPPED,
 *          -ECANCELED) at the next cycle of the loop.
 *
 *          Cases
 *          -----
 *          A.  The write is in the submission queue (io_uring_submit()
 *              failed), and the connect event is STOPPED (IDLE: its fd is
 *              closed by the stop).
 *          B.  The write is KEPT by the loop (the queue is full), and the
 *              connect event is DESTROYED (IDLE: its fd is closed by the
 *              free).
 *
 *          In both, the number is taken at once by a new socketpair, and
 *          the loop runs: the peer of the new socketpair must read
 *          nothing, and the write callback must be told -ECANCELED.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
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

#define APP "test_yevent_close_fd_kept"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
int result = 0;
int saved_ring_fd = -1;
int saved_enter_ring_fd = -1;

int connect_callbacks = 0;
int write_callbacks = 0;
int write_result = 0;
int write_state = 0;

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
 *
 ***************************************************************************/
PRIVATE int connect_callback(yev_event_h yev_event)
{
    connect_callbacks++;
    return -1;  // break the loop
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int write_callback(yev_event_h yev_event)
{
    write_callbacks++;
    write_result = yev_get_result(yev_event);
    write_state = yev_get_state(yev_event);
    return -1;  // break the loop
}

/***************************************************************************
 *  Fill the submission queue with NOPs that are not submitted.
 *  HACK the ring is the first member of the loop (yev_loop.c).
 ***************************************************************************/
PRIVATE void fill_submission_queue(void)
{
    struct io_uring *ring = (struct io_uring *)yev_loop;
    io_uring_submit(ring);
    struct io_uring_sqe *sqe;
    while((sqe = io_uring_get_sqe(ring)) != NULL) {
        io_uring_prep_nop(sqe);
        io_uring_sqe_set_flags(sqe, IOSQE_CQE_SKIP_SUCCESS);
        io_uring_sqe_set_data(sqe, NULL);
    }
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

/***************************************************************************
 *  A connect event connected to the listener, and the peer accepted
 ***************************************************************************/
PRIVATE yev_event_h connect_to(int fd_listen, int port, int *fd_peer)
{
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
    connect_callbacks = 0;
    yev_start_event(yev_connect);
    yev_loop_run(yev_loop, 2);
    if(connect_callbacks != 1 || !(yev_get_flag(yev_connect) & YEV_FLAG_CONNECTED)) {
        fail("the connect event did not connect");
    }
    *fd_peer = accept(fd_listen, NULL, NULL);
    if(*fd_peer < 0) {
        fail("accept() FAILED");
    }
    return yev_connect;
}

/***************************************************************************
 *  What the peer of a socket has to read, without waiting
 ***************************************************************************/
PRIVATE ssize_t read_nowait(int fd, char *bf, size_t bfsize)
{
    memset(bf, 0, bfsize);
    fcntl(fd, F_SETFL, O_NONBLOCK);
    ssize_t n = read(fd, bf, bfsize-1);
    return n;
}

/***************************************************************************
 *  After the close: the number reused, the loop run, and the checks
 ***************************************************************************/
PRIVATE void check_case(const char *name, int old_fd, yev_event_h yev_write)
{
    char temp[256];

    /*
     *  A new socketpair takes the number of the closed socket
     */
    int sv[2];
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        fail("cannot create a socketpair");
        return;
    }
    if(sv[0] != old_fd && sv[1] != old_fd) {
        snprintf(temp, sizeof(temp),
            "%s: the new socketpair did not take the number %d (%d, %d): the case shows nothing",
            name, old_fd, sv[0], sv[1]
        );
        fail(temp);
    }
    int fd_new_peer = (sv[0] == old_fd)? sv[1] : sv[0];

    unblock_submissions();
    write_callbacks = 0;
    write_result = 0;
    yev_loop_run(yev_loop, 1);

    char bf[128];
    ssize_t n = read_nowait(fd_new_peer, bf, sizeof(bf));
    if(n > 0) {
        snprintf(temp, sizeof(temp), "%s: the NEW peer read the old connection's bytes: '%s'", name, bf);
        fail(temp);
    }
    if(write_callbacks != 1 || write_result != -ECANCELED || write_state != YEV_ST_STOPPED) {
        snprintf(temp, sizeof(temp),
            "%s: write callback %d times, result %d, state %d; expected 1, %d (ECANCELED), STOPPED",
            name, write_callbacks, write_result, write_state, -ECANCELED
        );
        fail(temp);
    }

    yev_destroy_event(yev_write);
    close(sv[0]);
    close(sv[1]);
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    /*
     *  A listener of the test (plain socket)
     */
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
        8,
        10,
        yev_loop_callback,
        &yev_loop
    );

    /*------------------------------------------------------------*
     *  A.  The write in the submission queue, the connect event
     *      STOPPED
     *------------------------------------------------------------*/
    int fd_peer;
    yev_event_h yev_connect = connect_to(fd_listen, port, &fd_peer);
    int old_fd = yev_get_fd(yev_connect);

    gbuffer_t *gbuf = gbuffer_create(64, 64);
    gbuffer_append_string(gbuf, "SECRET-OF-THE-OLD-CONNECTION-A");
    yev_event_h yev_write = yev_create_write_event(yev_loop, write_callback, 0, old_fd, gbuf);

    block_submissions();
    yev_start_event(yev_write);
    if(io_uring_sq_ready((struct io_uring *)yev_loop) == 0) {
        fail("A: the write is not in the submission queue");
    }
    yev_stop_event(yev_connect);    // IDLE: the stop closes the socket
    check_case("A", old_fd, yev_write);
    yev_destroy_event(yev_connect);
    close(fd_peer);

    /*------------------------------------------------------------*
     *  B.  The write KEPT by the loop (queue full), the connect
     *      event DESTROYED
     *------------------------------------------------------------*/
    yev_connect = connect_to(fd_listen, port, &fd_peer);
    old_fd = yev_get_fd(yev_connect);

    gbuf = gbuffer_create(64, 64);
    gbuffer_append_string(gbuf, "SECRET-OF-THE-OLD-CONNECTION-B");
    yev_write = yev_create_write_event(yev_loop, write_callback, 0, old_fd, gbuf);

    fill_submission_queue();
    block_submissions();
    yev_start_event(yev_write);     // kept: the queue is full and the kernel takes nothing
    yev_destroy_event(yev_connect); // IDLE: freed at once, the socket closed
    check_case("B", old_fd, yev_write);
    close(fd_peer);

    /*--------------------------------*
     *  End
     *--------------------------------*/
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
        json_pack("[{s:s},{s:s},{s:s},{s:s}]",  // error_list
            "msg", "An fd is closed with submissions of other events on it that the kernel did not take: taken back, completed as canceled",
            "msg", "Submissions the kernel did not take: submitted again at each cycle",
            "msg", "Submission queue full and the kernel takes nothing: kept for the next cycle",
            "msg", "An fd is closed with submissions of other events on it that the kernel did not take: taken back, completed as canceled"
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
