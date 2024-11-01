/****************************************************************************
 *          test_yevent_traffic1.c
 *
 *          Setup
 *          -----
 *          Create listen (re-arm)
 *          Create connect
 *
 *          Process
 *          -------
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <gobj.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <stacktrace_with_bfd.h>
#include <yunetas_ev_loop.h>

/***************************************************************
 *              Constants
 ***************************************************************/
const char *server_url = "tcp://localhost:3333";

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);
PRIVATE int yev_callback(yev_event_t *event);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_t *yev_loop;
yev_event_t *yev_event_accept;
yev_event_t *yev_event_connect;
int result = 0;

/***************************************************************************
 *  yev_loop callback
 ***************************************************************************/
PRIVATE int yev_callback(yev_event_t *yev_event)
{
    if(!yev_event) {
        /*
         *  It's the timeout
         */
        return -1;  // break the loop
    }

    char *msg = "???";
    int ret = 0;
    switch(yev_event->type) {
        case YEV_ACCEPT_TYPE:
            {
                yev_state_t yev_state = yev_get_state(yev_event);
                if(yev_state == YEV_ST_IDLE) {
                    msg = "Listen Connection Accepted";
                } else if(yev_state == YEV_ST_STOPPED) {
                    msg = "Listen socket failed or stopped";
                } else {
                    msg ="What?";
                }
                ret = 0; // re-arm
            }
            break;
        case YEV_CONNECT_TYPE:
            {
                yev_state_t yev_state = yev_get_state(yev_event);
                if(yev_state == YEV_ST_IDLE) {
                    msg = "Connection Accepted";
                } else if(yev_state == YEV_ST_STOPPED) {
                    if(yev_event->result == -125) {
                        msg = "Connect canceled";
                    } else {
                        msg = "Connection Refused";
                    }
                } else {
                    msg ="What?=";
                }
                ret = -1; // break the loop
            }
            break;
        default:
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBUV_ERROR,
                "msg",          "%s", "yev_event not implemented",
                NULL
            );
            break;
    }
    json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_event->flag);
    gobj_log_warning(0, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", msg,
        "type",         "%s", yev_event_type_name(yev_event),
        "state",        "%s", yev_get_state_name(yev_event),
        "fd",           "%d", yev_event->fd,
        "result",       "%d", yev_event->result,
        "sres",         "%s", (yev_event->result<0)? strerror(-yev_event->result):"",
        "p",            "%p", yev_event,
        "flag",         "%j", jn_flags,
        NULL
    );
    json_decref(jn_flags);

    return ret;
}

/***************************************************************************
 *              Test
 ***************************************************************************/
int do_test(void)
{
    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
        yev_callback,
        &yev_loop
    );

    /*--------------------------------*
     *      Create listen
     *--------------------------------*/
    yev_event_accept = yev_create_accept_event(
        yev_loop,
        yev_callback,
        0
    );
    yev_setup_accept_event( // create the socket listening in yev_event->fd
        yev_event_accept,
        server_url, // listen_url,
        0, //backlog,
        FALSE, // shared
        AF_INET,  // ai_family AF_UNSPEC
        AI_ADDRCONFIG   // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
    );
    yev_start_event(yev_event_accept);
    yev_loop_run(yev_loop, 1);

    /*--------------------------------*
     *      Create connect
     *--------------------------------*/
    yev_event_connect = yev_create_connect_event(
        yev_loop,
        yev_callback,
        0
    );
    yev_setup_connect_event( // create the socket listening in yev_event->fd
        yev_event_connect,
        server_url, // listen_url,
        NULL,   // src_url, only host:port
        AF_INET,      // ai_family AF_UNSPEC
        AI_ADDRCONFIG       // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
    );
    yev_start_event(yev_event_connect);

    /*--------------------------------*
     *  Process ring queue
     *  Server accept the connection - re-arm
     *  Client connected, break the loop
     *--------------------------------*/
    yev_loop_run(yev_loop, 1);

    /*--------------------------------*
     *  Stop connect event: disconnected
     *  Stop accept event:
     *--------------------------------*/
    yev_stop_event(yev_event_connect);
    yev_loop_run(yev_loop, 1);
    yev_stop_event(yev_event_accept);
    yev_loop_run(yev_loop, 1);

    yev_destroy_event(yev_event_accept);
    yev_destroy_event(yev_event_connect);

    yev_loop_stop(yev_loop);
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

    init_backtrace_with_bfd(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_bfd);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // startup_persistent_attrs
        NULL,   // end_persistent_attrs
        NULL,   // load_persistent_attrs
        NULL,   // save_persistent_attrs
        NULL,   // remove_persistent_attrs
        NULL,   // list_persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL,   // global_authenticate_parser
        0,      // max_block, largest memory block
        0       // max_system_memory, maximum system memory
    );

    yuno_catch_signals();

    gobj_set_gobj_trace(0, "liburing", TRUE, 0);

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
    const char *test = "test_yevent_listen1";
//    json_t *error_list = json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",  // error_list
//        "msg", "addrinfo on listen",
//        "msg", "Connection Accepted",
//        "msg", "Connect canceled",
//        "msg", "Listen Connection Accepted",
//        "msg", "Listen socket failed or stopped"
//    );

    set_expected_results( // Check that no logs happen
        test,   // test name
        0, //error_list,  // error_list
        NULL,  // expected
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    result += do_test();

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)

    result += test_json(NULL, result);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "system memory not free");
        print_track_mem();
        result += -1;
    }

    return result;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop->running = 0;
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
