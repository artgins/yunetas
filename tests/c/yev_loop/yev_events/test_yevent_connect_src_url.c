/****************************************************************************
 *          test_yevent_connect_src_url.c
 *
 *          A connect with a local address to bind (src_url).
 *
 *          yev_create_connect_event() and yev_rearm_connect_event() take a
 *          src_url: the local host and port that the socket binds before it
 *          connects. Up to 7.25.4 the src_url was never parsed (a check on a
 *          buffer that was always empty): the socket was bound to an empty
 *          host and port, that is a port of the kernel's choice, and the
 *          src_url was ignored without a word. A bad src_url was ignored
 *          too.
 *
 *          Cases
 *          -----
 *          A.  IPv4: connect to 127.0.0.1:<port> from 127.0.0.1:<src port>.
 *              The listener sees the peer at the src port.
 *          B.  IPv6: connect to [::1]:<port> from [::1]:<src port>.
 *          C.  With a schema: src_url "tcp://127.0.0.1:<src port>".
 *          D.  A bad src_url ("[::1:5000", no closing bracket): the connect
 *              event has no socket, and the error is logged.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <gobj.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <yev_loop.h>
#include <helpers.h>

#define APP "test_yevent_connect_src_url"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
int result = 0;

int connect_callbacks = 0;
BOOL connected = FALSE;

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
 *  The connect is done
 ***************************************************************************/
PRIVATE int connect_callback(yev_event_h yev_event)
{
    connect_callbacks++;
    connected = (yev_get_flag(yev_event) & YEV_FLAG_CONNECTED)? TRUE:FALSE;
    return -1;  // break the loop
}

/***************************************************************************
 *  A socket bound to the loopback of the family, any port.
 *  -1 if the family is not available.
 ***************************************************************************/
PRIVATE int bind_loopback(int family, int type, struct sockaddr_storage *addr)
{
    int fd = socket(family, type, 0);
    if(fd < 0) {
        return -1;
    }
    memset(addr, 0, sizeof(*addr));
    socklen_t addrlen;
    if(family == AF_INET6) {
        struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)addr;
        a6->sin6_family = AF_INET6;
        a6->sin6_addr = in6addr_loopback;
        addrlen = sizeof(*a6);
    } else {
        struct sockaddr_in *a4 = (struct sockaddr_in *)addr;
        a4->sin_family = AF_INET;
        a4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addrlen = sizeof(*a4);
    }
    if(bind(fd, (struct sockaddr *)addr, addrlen)<0 ||
            getsockname(fd, (struct sockaddr *)addr, &addrlen)<0) {
        close(fd);
        return -1;
    }
    return fd;
}

/***************************************************************************
 *  The port of an address
 ***************************************************************************/
PRIVATE int port_of(struct sockaddr_storage *addr)
{
    if(addr->ss_family == AF_INET6) {
        return ntohs(((struct sockaddr_in6 *)addr)->sin6_port);
    }
    return ntohs(((struct sockaddr_in *)addr)->sin_port);
}

/***************************************************************************
 *  A free local port of the family (bound and closed)
 ***************************************************************************/
PRIVATE int free_port(int family)
{
    struct sockaddr_storage addr;
    int fd = bind_loopback(family, SOCK_STREAM, &addr);
    if(fd < 0) {
        return -1;
    }
    close(fd);
    return port_of(&addr);
}

/***************************************************************************
 *  Connect to a listener of the family, binding src_url, and check that
 *  the listener sees the peer at src_port
 ***************************************************************************/
PRIVATE void connect_from(const char *what, int family, const char *src_fmt)
{
    char temp[256];
    struct sockaddr_storage listen_addr;
    int fd_listen = bind_loopback(family, SOCK_STREAM, &listen_addr);
    if(fd_listen < 0 || listen(fd_listen, 1) < 0) {
        if(family == AF_INET6) {
            printf("  %s: IPv6 loopback not available, case skipped\n", what);
        } else {
            fail("cannot listen");
        }
        if(fd_listen >= 0) {
            close(fd_listen);
        }
        return;
    }

    int src_port = free_port(family);
    char dst_url[80];
    char src_url[80];
    snprintf(dst_url, sizeof(dst_url), "tcp://%s:%d",
        family == AF_INET6? "[::1]":"127.0.0.1", port_of(&listen_addr)
    );
    snprintf(src_url, sizeof(src_url), src_fmt, src_port);

    connect_callbacks = 0;
    connected = FALSE;
    yev_event_h yev_connect = yev_create_connect_event(
        yev_loop,
        connect_callback,
        dst_url,
        src_url,
        family,     // ai_family
        0,          // ai_flags
        0           // gobj
    );
    if(!yev_connect || yev_get_fd(yev_connect) < 0) {
        snprintf(temp, sizeof(temp), "%s: connect %s from %s: no socket", what, dst_url, src_url);
        fail(temp);
        if(yev_connect) {
            yev_destroy_event(yev_connect);
        }
        close(fd_listen);
        return;
    }

    yev_start_event(yev_connect);
    yev_loop_run(yev_loop, 1);  // Broken by the connect callback
    if(connect_callbacks != 1 || !connected) {
        snprintf(temp, sizeof(temp), "%s: %d connect callbacks, connected %d, expected 1 1",
            what, connect_callbacks, connected
        );
        fail(temp);
    }

    /*
     *  The listener sees the peer at the src port
     */
    struct pollfd pfd = {.fd = fd_listen, .events = POLLIN};
    struct sockaddr_storage peer;
    socklen_t peerlen = sizeof(peer);
    int fd_peer = -1;
    if(poll(&pfd, 1, 1000) > 0) {
        fd_peer = accept(fd_listen, (struct sockaddr *)&peer, &peerlen);
    }
    if(fd_peer < 0) {
        snprintf(temp, sizeof(temp), "%s: the listener accepted nothing", what);
        fail(temp);
    } else {
        if(peer.ss_family != family || port_of(&peer) != src_port) {
            char peername[80];
            print_socket_address(peername, sizeof(peername), (struct sockaddr *)&peer);
            snprintf(temp, sizeof(temp), "%s: the peer is %s, expected the port %d of %s",
                what, peername, src_port, src_url
            );
            fail(temp);
        }
        close(fd_peer);
    }

    yev_stop_event(yev_connect);
    yev_destroy_event(yev_connect);
    close(fd_listen);
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
        2024,
        10,
        yev_loop_callback,
        &yev_loop
    );

    /*------------------------------------------------------------*
     *  A.  IPv4
     *------------------------------------------------------------*/
    connect_from("A IPv4", AF_INET, "127.0.0.1:%d");

    /*------------------------------------------------------------*
     *  B.  IPv6
     *------------------------------------------------------------*/
    connect_from("B IPv6", AF_INET6, "[::1]:%d");

    /*------------------------------------------------------------*
     *  C.  With a schema
     *------------------------------------------------------------*/
    connect_from("C schema", AF_INET, "tcp://127.0.0.1:%d");

    /*------------------------------------------------------------*
     *  D.  A bad src_url
     *------------------------------------------------------------*/
    yev_event_h yev_connect = yev_create_connect_event(
        yev_loop,
        connect_callback,
        "tcp://127.0.0.1:5000",
        "[::1:5000",    // no closing bracket
        AF_INET,        // ai_family
        0,              // ai_flags
        0               // gobj
    );
    if(!yev_connect || yev_get_fd(yev_connect) >= 0) {
        fail("D: a bad src_url gives a socket");
    }
    if(yev_connect) {
        yev_destroy_event(yev_connect);
    }

    /*--------------------------------*
     *  End
     *--------------------------------*/
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
    set_expected_results(
        test,       // test name
        json_pack("[{s:s}]",  // error_list: only case D logs
            "msg", "Bad src_url: cannot bind the connect"
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
