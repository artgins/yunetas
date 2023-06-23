/****************************************************************************
 *          test_yev_ping_pong
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <gobj.h>
#include <ansi_escape_codes.h>
#include <stacktrace_with_bfd.h>
#include <yunetas_ev_loop.h>

/***************************************************************
 *              Constants
 ***************************************************************/
BOOL dump = FALSE;

const char *server_url = "tcp://localhost:2222";
//const char *server_url = "tcp://[::]:2222"; // in ipv6 cannot put the hostname as string TODO find some who does

int drop_in_seconds = 5;

int who_drop = 0; // 0 client, 1 srv_cli, 2 server

#define BUFFER_SIZE (1*1024) // TODO si aumento se muere, el buffer transmitido es la mitad

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);
PRIVATE int yev_server_callback(yev_event_t *event);
PRIVATE int yev_client_callback(yev_event_t *event);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_t *yev_loop;

#ifdef LIKE_LIBUV_PING_PONG
static char PING[] = "PING\n";
#endif

int fd_connect;
int srv_cli_fd;
int fd_listen;

gbuffer_t *gbuf_server_tx = 0;
yev_event_t *yev_server_tx = 0;

gbuffer_t *gbuf_server_rx = 0;
yev_event_t *yev_server_rx = 0;

gbuffer_t *gbuf_client_tx = 0;
yev_event_t *yev_client_tx = 0;

gbuffer_t *gbuf_client_rx = 0;
yev_event_t *yev_client_rx = 0;

uint64_t t;
uint64_t msg_per_second = 0;
uint64_t bytes_per_second = 0;
int seconds_count;

/***************************************************************************
 *              Test
 ***************************************************************************/
int do_test(void)
{
    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        NULL,
        2024,
        &yev_loop
    );

    /*--------------------------------*
     *      Setup server
     *--------------------------------*/
    yev_event_t *yev_server_accept = yev_create_accept_event(
        yev_loop,
        yev_server_callback,
        NULL
    );
    fd_listen = yev_setup_accept_event(
        yev_server_accept,
        server_url,     // server_url,
        0,              // backlog, default 512
        FALSE           // shared
    );
    if(fd_listen < 0) {
        gobj_trace_msg(0, "Error setup listen on %s", server_url);
        exit(0);
    }

    yev_start_event(yev_server_accept, NULL);

    /*--------------------------------*
     *      Setup client
     *--------------------------------*/
    yev_event_t *yev_client_connect = yev_create_connect_event(
        yev_loop,
        yev_client_callback,
        NULL
    );
    fd_connect = yev_setup_connect_event(
        yev_client_connect,
        server_url,     // client_url
        NULL            // local bind
    );
    if(fd_connect < 0) {
        gobj_trace_msg(0, "Error setup connect to %s", server_url);
        exit(0);
    }

    yev_start_event(yev_client_connect, NULL);

    /*--------------------------------*
     *      Begin run loop
     *--------------------------------*/
    t = start_msectimer(1000);
    yev_loop_run(yev_loop);

    /*--------------------------------*
     *      Stop
     *--------------------------------*/
    yev_stop_event(yev_client_connect);
    yev_stop_event(yev_client_rx);
    yev_stop_event(yev_client_tx);

    yev_destroy_event(yev_client_connect);
    yev_destroy_event(yev_client_rx);
    yev_destroy_event(yev_client_tx);

    yev_stop_event(yev_server_accept);
    yev_stop_event(yev_server_rx);
    yev_stop_event(yev_server_tx);

    yev_destroy_event(yev_server_accept);
    yev_destroy_event(yev_server_rx);
    yev_destroy_event(yev_server_tx);

    yev_loop_destroy(yev_loop);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_server_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;
    BOOL stopped = (yev_event->flag & YEV_STOPPED_FLAG)?TRUE:FALSE;

    if(dump) {
        gobj_trace_msg(
            gobj, "yev server callback %s%s", yev_event_type_name(yev_event), stopped ? ", STOPPED" : ""
        );
    }


    switch(yev_event->type) {
        case YEV_READ_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    yev_loop_stop(yev_loop);
                    break;
                }

                msg_per_second++;
                bytes_per_second += gbuffer_leftbytes(yev_event->gbuf);
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
                    nice_size(nice, sizeof(nice), msg_per_second);
                    printf("\n" Erase_Whole_Line Move_Horizontal, 1);
                    printf("Msg/sec    : %s\n", nice);
                    printf(Erase_Whole_Line Move_Horizontal, 1);
                    nice_size(nice, sizeof(nice), bytes_per_second);
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
                    gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "Server receiving");
                }
                gbuffer_clear(gbuf_server_tx);
                gbuffer_append_gbuf(gbuf_server_tx, yev_event->gbuf);

                /*
                 *  Transmit
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, gbuf_server_tx, "Server transmitting");
                }
                yev_start_event(yev_server_tx, gbuf_server_tx);

                /*
                 *  Clear buffer
                 *  Re-arm read
                 */
                gbuffer_clear(yev_event->gbuf);
                yev_start_event(yev_server_rx, yev_event->gbuf);
            }
            break;

        case YEV_WRITE_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    yev_loop_stop(yev_loop);
                    break;
                }
            }
            break;

        case YEV_ACCEPT_TYPE:
            {
                // TODO Create a srv_cli structure
                srv_cli_fd = yev_event->result;

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
                        yev_event->yev_loop,
                        yev_server_callback,
                        NULL,
                        srv_cli_fd
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
                        yev_event->yev_loop,
                        yev_server_callback,
                        NULL,
                        srv_cli_fd
                    );
                }

                yev_start_event(yev_server_rx, gbuf_server_rx);
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

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_client_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;
    BOOL stopped = (yev_event->flag & YEV_STOPPED_FLAG)?TRUE:FALSE;

    if(dump) {
        gobj_trace_msg(
            gobj, "yev client callback %s%s", yev_event_type_name(yev_event), stopped ? ", STOPPED" : ""
        );
    }
    switch(yev_event->type) {
        case YEV_READ_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    yev_loop_stop(yev_loop);
                    break;
                }

                /*
                 *  Save received data to transmit: do echo
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "Client receiving");
                }
                gbuffer_clear(gbuf_client_tx);
                gbuffer_append_gbuf(gbuf_client_tx, yev_event->gbuf);

                /*
                 *  Transmit
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, gbuf_client_tx, "Client transmitting");
                }
                yev_start_event(yev_client_tx, gbuf_client_tx);

                /*
                 *  Clear buffer
                 *  Re-arm read
                 */
                gbuffer_clear(yev_event->gbuf);
                yev_start_event(yev_client_rx, yev_event->gbuf);
            }
            break;

        case YEV_WRITE_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    yev_loop_stop(yev_loop);
                    break;
                }
                // Write ended
            }
            break;

        case YEV_CONNECT_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Error on connection
                     */
                    // TODO
                    break;
                }

                char sockname[80], peername[80];
                get_peername(peername, sizeof(peername), yev_event->fd);
                get_sockname(sockname, sizeof(sockname), yev_event->fd);
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
                        yev_event->yev_loop,
                        yev_client_callback,
                        NULL,
                        yev_event->fd
                    );
                }
                yev_start_event(yev_client_rx, gbuf_client_rx);

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
                        yev_event->yev_loop,
                        yev_client_callback,
                        NULL,
                        yev_event->fd
                    );
                }

                /*
                 *  Transmit
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "Client transmitting");
                }
                yev_start_event(yev_client_tx, gbuf_client_tx);
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

    return 0;
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

#ifdef DEBUG
    init_backtrace_with_bfd(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_bfd);
#endif

    gobj_start_up(
        argc,
        argv,
        NULL, // jn_global_settings
        NULL, // startup_persistent_attrs
        NULL, // end_persistent_attrs
        0,  // load_persistent_attrs
        0,  // save_persistent_attrs
        0,  // remove_persistent_attrs
        0,  // list_persistent_attrs
        NULL, // global_command_parser
        NULL, // global_stats_parser
        NULL, // global_authz_checker
        NULL, // global_authenticate_parser
        60*1024L,  // max_block, largest memory block
        120*1024L   // max_system_memory, maximum system memory
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    do_test();

    printf(Cursor_Down "\n", 4);

    return gobj_get_exit_code();
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    yev_loop->running = 0;
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
