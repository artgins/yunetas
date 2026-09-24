/****************************************************************************
 *          test_yevent_udp_ipv6.c
 *
 *          IPv6 peers: the address of a peer is kept with its real length.
 *
 *          Up to 7.25.4 the loop and the gbuffer kept a peer address in a
 *          struct sockaddr (16 bytes), which holds an IPv4 address only. An
 *          IPv6 address (struct sockaddr_in6) needs 28 bytes:
 *            - yev_create_accept_event() and yev_create_connect_event()
 *              refused an IPv6 address (sock_info_t too small),
 *            - a recvmsg event received the peer address truncated,
 *            - the gbuffer kept 16 bytes of it (gbuffer_setaddr()),
 *            - a sendmsg event passed msg_namelen = 16, and the kernel
 *              refused the reply (EINVAL).
 *          So a C_UDP_S could not answer an IPv6 peer.
 *
 *          Cases
 *          -----
 *          A.  A UDP listener on udp://[::1]: yev_create_accept_event()
 *              keeps the IPv6 address (sizeof(struct sockaddr_in6)).
 *          B.  An echo, as C_UDP_S does it: a recvmsg event receives a
 *              datagram from an IPv6 client, the peer address is stored in
 *              the gbuffer with its length, and the gbuffer is sent back
 *              with a sendmsg event to that address. The client receives it.
 *          C.  A TCP connect to tcp://[::1]: yev_create_connect_event()
 *              keeps the IPv6 address and connects.
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

#define APP "test_yevent_udp_ipv6"

#define MESSAGE     "Hello over IPv6"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
int result = 0;

struct sockaddr_in6 client_addr;    // The client, as it is bound
int recv_callbacks = 0;
int send_callbacks = 0;
int send_result = 0;
yev_state_t send_state = YEV_ST_IDLE;
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
 *  The reply is sent: the event is destroyed, the run ends
 ***************************************************************************/
PRIVATE int send_callback(yev_event_h yev_event)
{
    send_callbacks++;
    send_result = yev_get_result(yev_event);
    send_state = yev_get_state(yev_event);
    yev_destroy_event(yev_event);
    return -1;  // break the loop
}

/***************************************************************************
 *  A datagram from the client: echo it to the address it came from
 ***************************************************************************/
PRIVATE int recv_callback(yev_event_h yev_event)
{
    char temp[256];
    recv_callbacks++;

    if(yev_get_state(yev_event) != YEV_ST_IDLE) {
        snprintf(temp, sizeof(temp), "B: recvmsg in state %s, result %d",
            yev_get_state_name(yev_event), yev_get_result(yev_event)
        );
        fail(temp);
        return -1;
    }

    /*
     *  The peer address, as the kernel wrote it
     */
    struct msghdr *msghdr = yev_event->msghdr;
    if(msghdr->msg_namelen != sizeof(struct sockaddr_in6)) {
        snprintf(temp, sizeof(temp), "B: recvmsg peer address of %d bytes, expected %d",
            (int)msghdr->msg_namelen, (int)sizeof(struct sockaddr_in6)
        );
        fail(temp);
    }

    if(yev_get_sock_info(yev_event)->addrlen != sizeof(struct sockaddr_in6)) {
        snprintf(temp, sizeof(temp), "B: sock_info peer address of %d bytes, expected %d",
            (int)yev_get_sock_info(yev_event)->addrlen, (int)sizeof(struct sockaddr_in6)
        );
        fail(temp);
    }

    gbuffer_t *gbuf = yev_get_gbuf(yev_event);
    gbuffer_setaddr(gbuf, msghdr->msg_name, msghdr->msg_namelen);

    const struct sockaddr_in6 *peer = (const struct sockaddr_in6 *)gbuffer_getaddr(gbuf);
    if(gbuffer_getaddrlen(gbuf) != sizeof(struct sockaddr_in6) ||
            peer->sin6_family != AF_INET6 ||
            peer->sin6_port != client_addr.sin6_port ||
            memcmp(&peer->sin6_addr, &in6addr_loopback, sizeof(struct in6_addr))!=0) {
        char peername[80];
        print_socket_address(peername, sizeof(peername), gbuffer_getaddr(gbuf));
        snprintf(temp, sizeof(temp),
            "B: the gbuffer keeps the peer '%s' (%d bytes), expected [::1]:%d (%d bytes)",
            peername, (int)gbuffer_getaddrlen(gbuf),
            (int)ntohs(client_addr.sin6_port), (int)sizeof(struct sockaddr_in6)
        );
        fail(temp);
    }

    /*
     *  The echo, as C_UDP_S writes it: the address lives in the gbuffer,
     *  which the event holds
     */
    yev_event_h yev_reply = yev_create_sendmsg_event(
        yev_loop,
        send_callback,
        NULL,   // gobj
        yev_get_fd(yev_event),
        gbuffer_incref(gbuf),
        gbuffer_getaddr(gbuf),
        gbuffer_getaddrlen(gbuf)
    );
    yev_start_event(yev_reply);
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
 *  Wait up to 1 second for a datagram
 ***************************************************************************/
PRIVATE int receive(int fd, char *bf, size_t bfsize)
{
    memset(bf, 0, bfsize);
    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    if(poll(&pfd, 1, 1000) <= 0) {
        return -1;
    }
    return (int)recv(fd, bf, bfsize - 1, MSG_DONTWAIT);
}

/***************************************************************************
 *  A socket bound to [::1], any port. -1 if IPv6 is not available.
 ***************************************************************************/
PRIVATE int bind_loopback6(int type, struct sockaddr_in6 *addr)
{
    int fd = socket(AF_INET6, type, 0);
    if(fd < 0) {
        return -1;
    }
    memset(addr, 0, sizeof(*addr));
    addr->sin6_family = AF_INET6;
    addr->sin6_addr = in6addr_loopback;
    socklen_t addrlen = sizeof(*addr);
    if(bind(fd, (struct sockaddr *)addr, sizeof(*addr))<0 ||
            getsockname(fd, (struct sockaddr *)addr, &addrlen)<0) {
        close(fd);
        return -1;
    }
    return fd;
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
     *  The client: an IPv6 UDP socket on [::1]
     */
    int fd_client = bind_loopback6(SOCK_DGRAM, &client_addr);
    if(fd_client < 0) {
        printf("  IPv6 loopback not available: test skipped\n");
        yev_loop_destroy(yev_loop);
        return 0;
    }

    /*------------------------------------------------------------*
     *  A.  A UDP listener on [::1]
     *------------------------------------------------------------*/
    yev_event_h yev_server = yev_create_accept_event(
        yev_loop,
        yev_loop_callback,
        "udp://[::1]:0",
        0,          // backlog
        FALSE,      // shared
        AF_INET6,   // ai_family
        0,          // ai_flags
        0           // gobj
    );
    if(!yev_server) {
        fail("A: yev_create_accept_event(udp://[::1]) FAILED");
        close(fd_client);
        yev_loop_destroy(yev_loop);
        return result;
    }
    sock_info_t *sock_info = yev_get_sock_info(yev_server);
    if(sock_info->addrlen != sizeof(struct sockaddr_in6) ||
            ((struct sockaddr *)&sock_info->addr)->sa_family != AF_INET6) {
        snprintf(temp, sizeof(temp), "A: listener address of %d bytes, expected %d",
            (int)sock_info->addrlen, (int)sizeof(struct sockaddr_in6)
        );
        fail(temp);
    }
    struct sockaddr_in6 server_addr;
    socklen_t server_addrlen = sizeof(server_addr);
    getsockname(yev_get_fd(yev_server), (struct sockaddr *)&server_addr, &server_addrlen);

    /*------------------------------------------------------------*
     *  B.  Echo: recvmsg, the peer in the gbuffer, sendmsg back
     *------------------------------------------------------------*/
    yev_event_h yev_reader = yev_create_recvmsg_event(
        yev_loop,
        recv_callback,
        NULL,   // gobj
        yev_get_fd(yev_server),
        gbuffer_create(1024, 1024)
    );
    yev_start_event(yev_reader);

    if(sendto(fd_client, MESSAGE, strlen(MESSAGE), 0,
            (struct sockaddr *)&server_addr, sizeof(server_addr)) != (ssize_t)strlen(MESSAGE)) {
        fail("B: the client cannot send");
    }

    yev_loop_run(yev_loop, 1);  // Broken by the send callback

    if(recv_callbacks != 1) {
        snprintf(temp, sizeof(temp), "B: %d recvmsg callbacks, expected 1", recv_callbacks);
        fail(temp);
    }
    if(send_callbacks != 1 || send_state != YEV_ST_IDLE || send_result != (int)strlen(MESSAGE)) {
        snprintf(temp, sizeof(temp),
            "B: %d sendmsg callbacks, result %d (%s) state %d, expected 1 %d IDLE",
            send_callbacks, send_result, send_result<0? strerror(-send_result):"",
            (int)send_state, (int)strlen(MESSAGE)
        );
        fail(temp);
    }
    if(receive(fd_client, bf, sizeof(bf)) != (int)strlen(MESSAGE) || strcmp(bf, MESSAGE)!=0) {
        fail("B: the client did not receive the echo");
    }

    /*------------------------------------------------------------*
     *  C.  A TCP connect to [::1]
     *------------------------------------------------------------*/
    struct sockaddr_in6 listen_addr;
    int fd_listen = bind_loopback6(SOCK_STREAM, &listen_addr);
    if(fd_listen < 0 || listen(fd_listen, 1) < 0) {
        fail("C: cannot listen on [::1]");
    } else {
        char url[80];
        snprintf(url, sizeof(url), "tcp://[::1]:%d", (int)ntohs(listen_addr.sin6_port));
        yev_event_h yev_connect = yev_create_connect_event(
            yev_loop,
            connect_callback,
            url,
            NULL,       // src_url
            AF_INET6,   // ai_family
            0,          // ai_flags
            0           // gobj
        );
        if(!yev_connect) {
            snprintf(temp, sizeof(temp), "C: yev_create_connect_event(%s) FAILED", url);
            fail(temp);
        } else {
            sock_info = yev_get_sock_info(yev_connect);
            if(sock_info->addrlen != sizeof(struct sockaddr_in6)) {
                snprintf(temp, sizeof(temp), "C: connect address of %d bytes, expected %d",
                    (int)sock_info->addrlen, (int)sizeof(struct sockaddr_in6)
                );
                fail(temp);
            }
            yev_start_event(yev_connect);
            yev_loop_run(yev_loop, 1);  // Broken by the connect callback
            if(connect_callbacks != 1 || !connected) {
                snprintf(temp, sizeof(temp), "C: %d connect callbacks, connected %d, expected 1 1",
                    connect_callbacks, connected
                );
                fail(temp);
            }
            yev_stop_event(yev_connect);
            yev_destroy_event(yev_connect);
        }
    }
    if(fd_listen >= 0) {
        close(fd_listen);
    }

    /*--------------------------------*
     *  End
     *--------------------------------*/
    yev_stop_event(yev_reader);
    yev_stop_event(yev_server);
    yev_destroy_event(yev_reader);
    yev_destroy_event(yev_server);
    close(fd_client);

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
