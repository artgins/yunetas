/****************************************************************************
 *          test_yev_ping_pong
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <gobj.h>
#include <stacktrace_with_bfd.h>
#include <yunetas_ev_loop.h>

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);
PRIVATE int yev_server_callback(yev_event_t *event);
PRIVATE int yev_client_callback(yev_event_t *event);

/***************************************************************
 *              Data
 ***************************************************************/
const char *server_url = "tcp://localhost:2222";

/***************************************************************************
 *              Test
 ***************************************************************************/
int do_test(void)
{
    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_t *yev_loop;
    yev_loop_create(
        0,
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
    int fd_listen = yev_setup_accept_event(
        yev_server_accept,
        server_url,     // server_url,
        0,              // backlog, default 512
        FALSE           // shared
    );
    if(fd_listen < 0) {
        gobj_trace_msg(0, "Error setup listen on %s", server_url);
        exit(0);
    }

    yev_event_t *yev_server_rx = yev_create_read_event(
        yev_loop,
        yev_server_callback,
        NULL,
        fd_listen
    );
    yev_event_t *yev_server_tx = yev_create_write_event(
        yev_loop,
        yev_server_callback,
        NULL,
        fd_listen
    );

    gbuffer *gbuf_server_rx = gbuffer_create(4*1024, 4*1024);
    gbuffer *gbuf_server_tx = gbuffer_create(4*1024, 4*1024);
    yev_start_event(yev_server_accept, NULL);
    yev_start_event(yev_server_rx, gbuf_server_rx);
//    yev_start_event(yev_server_tx, gbuf_server_tx);

    /*--------------------------------*
     *      Setup client
     *--------------------------------*/
    yev_event_t *yev_client_connect = yev_create_connect_event(
        yev_loop,
        yev_client_callback,
        NULL
    );
    int fd_connect = yev_setup_connect_event(
        yev_client_connect,
        server_url,     // client_url
        NULL            // local bind
    );
    if(fd_connect < 0) {
        gobj_trace_msg(0, "Error setup connect to %s", server_url);
        exit(0);
    }

    yev_event_t *yev_client_rx = yev_create_read_event(
        yev_loop,
        yev_client_callback,
        NULL,
        fd_connect
    );
    yev_event_t *yev_client_tx = yev_create_write_event(
        yev_loop,
        yev_client_callback,
        NULL,
        fd_connect
    );

    gbuffer *gbuf_client_rx = gbuffer_create(4*1024, 4*1024);
    gbuffer *gbuf_client_tx = gbuffer_create(4*1024, 4*1024);
    yev_start_event(yev_client_connect, NULL);
    yev_start_event(yev_client_rx, gbuf_client_rx);
//    yev_start_event(yev_client_tx, gbuf_client_tx);

    /*--------------------------------*
     *      Begin run loop
     *--------------------------------*/
    yev_loop_run(yev_loop);

    /*--------------------------------*
     *      Stop
     *--------------------------------*/
    yev_stop_event(yev_server_accept);
    yev_stop_event(yev_server_rx);
    yev_stop_event(yev_server_tx);

    yev_stop_event(yev_client_connect);
    yev_stop_event(yev_client_rx);
    yev_stop_event(yev_client_tx);

    yev_destroy_event(yev_server_accept);
    yev_destroy_event(yev_server_rx);
    yev_destroy_event(yev_server_tx);

    yev_destroy_event(yev_client_connect);
    yev_destroy_event(yev_client_rx);
    yev_destroy_event(yev_client_tx);

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

    gobj_trace_msg(gobj, "yev server callback %s%s", yev_event_type_name(yev_event), stopped?", STOPPED":"");
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_client_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;
    BOOL stopped = (yev_event->flag & YEV_STOPPED_FLAG)?TRUE:FALSE;

    gobj_trace_msg(gobj, "yev client callback %s%s", yev_event_type_name(yev_event), stopped?", STOPPED":"");
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

    gobj_set_deep_tracing(2);           // TODO TEST
    gobj_set_global_trace(0, TRUE);     // TODO TEST

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
        8*1024L,    // max_block, largest memory block
        100*1024L   // max_system_memory, maximum system memory
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

    return gobj_get_exit_code();
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    _exit(0);
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
