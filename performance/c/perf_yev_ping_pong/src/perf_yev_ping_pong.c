/****************************************************************************
 *          perf_yev_ping_pong
 *

    - Performance, in my machine, 17-Nov-2024:
        Msg/sec    : 173.3 Thousands
        Bytes/sec  : 177.5 Millions

 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <gobj.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <yev_loop.h>
#include <helpers.h>

/***************************************************************
 *              Constants
 ***************************************************************/
BOOL dump = FALSE;
int time2exit = 5;

const char *server_url = "tcp://localhost:2222";
//const char *server_url = "tcp://[::]:2222"; // in ipv6 cannot put the hostname as string TODO find some who does

int drop_in_seconds = 5;

int who_drop = 0; // 0 client, 1 srv_cli, 2 server

#define BUFFER_SIZE (1*1024) // TODO si aumento se muere, el buffer transmitido es la mitad

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);
PRIVATE int yev_server_callback(yev_event_h event);
PRIVATE int yev_client_callback(yev_event_h event);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;

#ifdef LIKE_LIBUV_PING_PONG
static char PING[] = "PING\n";
#endif

int fd_connect;
int srv_cli_fd;
int fd_listen;

gbuffer_t *gbuf_server_tx = 0;
yev_event_h yev_server_tx = 0;

gbuffer_t *gbuf_server_rx = 0;
yev_event_h yev_server_rx = 0;

gbuffer_t *gbuf_client_tx = 0;
yev_event_h yev_client_tx = 0;

gbuffer_t *gbuf_client_rx = 0;
yev_event_h yev_client_rx = 0;

uint64_t t;
uint64_t msg_per_second = 0;
uint64_t bytes_per_second = 0;
int seconds_count;

/***************************************************************************
 *  yev_loop callback
 ***************************************************************************/
PRIVATE int yev_loop_callback(yev_event_h yev_event) {
    if (!yev_event) {
        /*
         *  It's the timeout
         */
        return -1;  // break the loop
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_server_callback(yev_event_h yev_event)
{
    hgobj gobj = yev_get_gobj(yev_event);
    int ret = 0;

    if(dump) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(yev_event));
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "💥 yev server callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "result",       "%d", yev_get_result(yev_event),
            "sres",         "%s", (yev_get_result(yev_event)<0)? strerror(-yev_get_result(yev_event)):"",
            "flag",         "%j", jn_flags,
            "p",            "%p", yev_event,
            NULL
        );
        json_decref(jn_flags);
    }

    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_get_type(yev_event)) {
        case YEV_READ_TYPE:
            {
                if(yev_state != YEV_ST_IDLE) {
                    /*
                     *  Disconnected or Disconnecting
                     */
                    ret = -1;
                    break;
                }

                msg_per_second++;
                bytes_per_second += gbuffer_leftbytes(yev_get_gbuf(yev_event));
                if(test_msectimer(t)) {
                    seconds_count++;
                    if(seconds_count && (seconds_count % drop_in_seconds)==0) {
                        if(who_drop) {
                            printf(Cursor_Down, 3);
                            printf(Move_Horizontal, 1);
                            switch(who_drop) {
                                case 1:
                                    close(fd_connect);
                                    break;
                                case 2:
                                    close(srv_cli_fd);
                                    break;
                                case 3:
                                    close(fd_listen);
                                    break;
                            }
                        }
                    }

                    char nice[64];
                    nice_size(nice, sizeof(nice), msg_per_second, FALSE);
                    printf("\n" Erase_Whole_Line Move_Horizontal, 1);
                    printf("Msg/sec    : %s\n", nice);
                    printf(Erase_Whole_Line Move_Horizontal, 1);
                    nice_size(nice, sizeof(nice), bytes_per_second, FALSE);
                    printf("Bytes/sec  : %s\n", nice);
                    printf(Cursor_Up, 3);
                    printf(Move_Horizontal, 1);

                    fflush(stdout);
                    msg_per_second = 0;
                    bytes_per_second = 0;
                    t = start_msectimer(1000);
                }

                /*
                 *  Save received data to transmit: do echo
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, yev_get_gbuf(yev_event), "Server receiving");
                }
                gbuffer_clear(gbuf_server_tx);
                gbuffer_append_gbuf(gbuf_server_tx, yev_get_gbuf(yev_event));

                /*
                 *  Transmit
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, gbuf_server_tx, "Server transmitting");
                }
                yev_set_gbuffer(yev_server_tx, gbuf_server_tx);
                yev_start_event(yev_server_tx);

                /*
                 *  Clear buffer
                 *  Re-arm read
                 */
                gbuffer_clear(yev_get_gbuf(yev_event));
                yev_set_gbuffer(yev_server_rx, yev_get_gbuf(yev_event));
                yev_start_event(yev_server_rx);
            }
            break;

        case YEV_WRITE_TYPE:
            {
                if(yev_state != YEV_ST_IDLE) {
                    /*
                     *  Disconnected
                     */
                    ret = -1;
                    break;
                }
            }
            break;

        case YEV_ACCEPT_TYPE:
            {
                if(yev_state != YEV_ST_IDLE) {
                    ret = -1;
                    break;
                }

                srv_cli_fd = yev_get_result(yev_event);

                char sockname[80], peername[80];
                get_peername(peername, sizeof(peername), srv_cli_fd);
                get_sockname(sockname, sizeof(sockname), srv_cli_fd);
                printf("ACCEPTED  sockname %s <- peername %s \n", sockname, peername);

                /*
                 *  Ready to receive
                 */
                if(!gbuf_server_rx) {
                    gbuf_server_rx = gbuffer_create(BUFFER_SIZE, BUFFER_SIZE);
                    gbuffer_setlabel(gbuf_server_rx, "server-rx");
                }
                if(!yev_server_rx) {
                    yev_server_rx = yev_create_read_event(
                        yev_get_loop(yev_event),
                        yev_server_callback,
                        NULL,
                        srv_cli_fd,
                        0
                    );
                }

                /*
                 *  Read to Transmit
                 */
                if(!gbuf_server_tx) {
                    gbuf_server_tx = gbuffer_create(BUFFER_SIZE, BUFFER_SIZE);
                    gbuffer_setlabel(gbuf_server_tx, "server-tx");
                }
                if(!yev_server_tx) {
                    yev_server_tx = yev_create_write_event(
                        yev_get_loop(yev_event),
                        yev_server_callback,
                        NULL,
                        srv_cli_fd,
                        0
                    );
                }
                yev_set_gbuffer(yev_server_rx, gbuf_server_rx);
                yev_start_event(yev_server_rx);
            }
            break;

        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "event type NOT IMPLEMENTED",
                "event_type",   "%s", yev_event_type_name(yev_event),
                NULL
            );
            break;
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_client_callback(yev_event_h yev_event)
{
    hgobj gobj = yev_get_gobj(yev_event);
    int ret = 0;

    if(dump) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(yev_event));
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "💥 yev client callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "result",       "%d", yev_get_result(yev_event),
            "sres",         "%s", (yev_get_result(yev_event)<0)? strerror(-yev_get_result(yev_event)):"",
            "flag",         "%j", jn_flags,
            "p",            "%p", yev_event,
            NULL
        );
        json_decref(jn_flags);
    }

    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_get_type(yev_event)) {
        case YEV_READ_TYPE:
            {
                if(yev_state != YEV_ST_IDLE) {
                    /*
                     *  Disconnected or Disconnecting
                     */
                    ret = -1;
                    break;
                }

                /*
                 *  Save received data to transmit: do echo
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, yev_get_gbuf(yev_event), "Client receiving");
                }
                gbuffer_clear(gbuf_client_tx);
                gbuffer_append_gbuf(gbuf_client_tx, yev_get_gbuf(yev_event));

                /*
                 *  Transmit
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, gbuf_client_tx, "Client transmitting");
                }
                yev_set_gbuffer(yev_client_tx, gbuf_client_tx);
                yev_start_event(yev_client_tx);

                /*
                 *  Clear buffer
                 *  Re-arm read
                 */
                gbuffer_clear(yev_get_gbuf(yev_event));
                yev_start_event(yev_client_rx);
            }
            break;

        case YEV_WRITE_TYPE:
            {
                if(yev_state != YEV_ST_IDLE) {
                    /*
                     *  Disconnected
                     */
                    ret = -1;
                    break;
                }
                // Write ended
            }
            break;

        case YEV_CONNECT_TYPE:
            {
                if(yev_state != YEV_ST_IDLE) {
                    /*
                     *  Error on connection
                     */
                    break;
                }

                char sockname[80], peername[80];
                get_peername(peername, sizeof(peername), yev_get_fd(yev_event));
                get_sockname(sockname, sizeof(sockname), yev_get_fd(yev_event));
                printf("CONNECTED sockname %s -> peername %s \n", sockname, peername);

                /*
                 *  Ready to receive
                 */
                if(!gbuf_client_rx) {
                    gbuf_client_rx = gbuffer_create(BUFFER_SIZE, BUFFER_SIZE);
                    gbuffer_setlabel(gbuf_client_rx, "client-rx");
                }
                if(!yev_client_rx) {
                    yev_client_rx = yev_create_read_event(
                        yev_get_loop(yev_event),
                        yev_client_callback,
                        NULL,
                        yev_get_fd(yev_event),
                        0
                    );
                }
                yev_set_gbuffer(yev_client_rx, gbuf_client_rx);
                yev_start_event(yev_client_rx);

                /*
                 *  Transmit
                 */
                if(!gbuf_client_tx) {
                    gbuf_client_tx = gbuffer_create(BUFFER_SIZE, BUFFER_SIZE);
                    gbuffer_setlabel(gbuf_client_tx, "client-tx");
                    #ifdef LIKE_LIBUV_PING_PONG
                    gbuffer_append_string(gbuf_client_tx, PING);
                    #else
                    for(int i= 0; i<BUFFER_SIZE; i++) {
                        gbuffer_append_char(gbuf_client_tx, 'A');
                    }
                    #endif
                }

                if(!yev_client_tx) {
                    yev_client_tx = yev_create_write_event(
                        yev_get_loop(yev_event),
                        yev_client_callback,
                        NULL,
                        yev_get_fd(yev_event),
                        0
                    );
                }

                /*
                 *  Transmit
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, yev_get_gbuf(yev_event), "Client transmitting");
                }
                yev_set_gbuffer(yev_client_tx, gbuf_client_tx);
                yev_start_event(yev_client_tx);
            }
            break;
        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "event type NOT IMPLEMENTED",
                "event_type",   "%s", yev_event_type_name(yev_event),
                NULL
            );
            break;
    }

    return ret;
}

/***************************************************************************
 *              Test
 ***************************************************************************/
int do_test(void)
{
    int result = 0;

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    result += yev_loop_create(
        NULL,
        2024,
        10,
        yev_loop_callback,  // process timeouts of loop
        &yev_loop
    );

    /*--------------------------------*
     *      Setup server
     *--------------------------------*/
    yev_event_h yev_server_accept = yev_create_accept_event(
        yev_loop,
        yev_server_callback,
        NULL
    );
    fd_listen = yev_setup_accept_event(
        yev_server_accept,
        server_url,     // server_url,
        0,              // backlog, default 512
        FALSE,          // shared
        AF_INET,        // ai_family AF_UNSPEC
        AI_ADDRCONFIG   // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
    );
    if(fd_listen < 0) {
        gobj_trace_msg(0, "Error setup listen on %s", server_url);
        return -1;
    }

    result += yev_start_event(yev_server_accept);

    /*--------------------------------*
     *      Setup client
     *--------------------------------*/
    yev_event_h yev_client_connect = yev_create_connect_event(
        yev_loop,
        yev_client_callback,
        NULL
    );
    fd_connect = yev_setup_connect_event(
        yev_client_connect,
        server_url,     // client_url
        NULL,   // local bind src_url, only host:port
        AF_INET,        // ai_family AF_UNSPEC
        AI_ADDRCONFIG   // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
    );
    if(fd_connect < 0) {
        gobj_trace_msg(0, "Error setup connect to %s", server_url);
        return -1;
    }

    result += yev_start_event(yev_client_connect);

    printf("\n----------------> Quit in %d seconds <-----------------\n\n", time2exit);

    /*--------------------------------*
     *      Begin run loop
     *--------------------------------*/
    t = start_msectimer(1000);
    result += yev_loop_run(yev_loop, 10);

    /*--------------------------------*
     *      Stop
     *--------------------------------*/
    yev_stop_event(yev_server_tx);
    yev_stop_event(yev_server_rx);
    yev_stop_event(yev_client_tx);
    yev_stop_event(yev_client_rx);

    yev_stop_event(yev_server_accept);
    yev_stop_event(yev_client_connect);

    yev_loop_run(yev_loop, 2);
    yev_loop_run_once(yev_loop);

    yev_destroy_event(yev_server_tx);
    yev_destroy_event(yev_server_rx);
    yev_destroy_event(yev_client_tx);
    yev_destroy_event(yev_client_rx);
    yev_destroy_event(yev_server_accept);
    yev_destroy_event(yev_client_connect);

    result += yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    int result = 0;

    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gobj_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

    //dump = TRUE;                        // TODO TEST
    //gobj_set_deep_tracing(2);           // TODO TEST
    //gobj_set_global_trace(0, TRUE);     // TODO TEST

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    result += gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL,   // global_authenticate_parser
        0,      // max_block, largest memory block
        0,      // max_system_memory, maximum system memory
        FALSE,
        0,
        0
    );

    yuno_catch_signals();

    //gobj_set_gobj_trace(0, "liburing", TRUE, 0);

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

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    const char *test = "yev_ping_pong";
    json_t *error_list = json_pack("[{s:s}]",  // error_list
        "msg", "addrinfo on listen"
    );
    set_expected_results( // Check that no logs happen
        test,   // test name
        error_list,  // error_list
        NULL,  // expected
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    result += do_test();
    printf(Cursor_Down "\n", 4);

    result += test_json(NULL);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "system memory not free");
        print_track_mem();
        result += -1;
    }

    result += gobj_get_exit_code();
    return result;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int times = 0;
    times++;
    yev_loop_reset_running(yev_loop);
    if(times > 1) {
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

    alarm(time2exit);
}
