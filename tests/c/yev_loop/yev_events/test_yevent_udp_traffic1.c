/****************************************************************************
 *          test_yevent_udp_traffic1.c
 *
 *          Setup
 *          -----
 *          Create listen (re-arm)
 *          Create connect
 *
 *          Process
 *          -------
 *          On client connected, it transmits a message
 *          The server echo the message
 *          The client matchs the received message with the sent.
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_yevent_udp_traffic1"

#include <string.h>
#include <signal.h>
#include <gobj.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <yev_loop.h>
#include <helpers.h>

/***************************************************************
 *              Constants
 ***************************************************************/
const char *server_url = "udp://localhost:3333";
#define MESSAGE "AaaaaaaaaaaaaaaaBbbbbbbbbbbbbbb2"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
yev_event_h yev_event_accept;
yev_event_h yev_event_connect;
int result = 0;

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
 *  yev_loop callback   SERVER
 ***************************************************************************/
PRIVATE int yev_server_callback(yev_event_h yev_event)
{
    if(!yev_event) {
        /*
         *  It's the timeout
         */
        return -1;  // break the loop
    }

    char *msg = "???";
    int ret = 0;
    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_get_type(yev_event)) {
        case YEV_ACCEPT_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    msg = "Server: Listen Connection Accepted";
                    ret = 0; // re-arm
                } else if(yev_state == YEV_ST_STOPPED) {
                    msg = "Server: Listen socket failed or stopped";
                    ret = -1; // break the loop
                } else {
                    msg = "Server: What?";
                    ret = -1; // break the loop
                }
            }
            break;
        case YEV_READ_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    /*
                     *  Data from the client
                     */
                    msg = "Server: Message from the client";
                    gbuffer_t *gbuf_rx = yev_get_gbuf(yev_event);
                    /*
                     *  Server: Process the message
                     */
                    gobj_trace_dump_gbuf(0, gbuf_rx, "Server: Message from the client");

                    /*
                     *  Response to the client
                     *  Get their callback and fd
                     */
                    yev_event_h yev_response = yev_create_write_event(
                        yev_loop,
                        yev_get_callback(yev_event),
                        NULL,   // gobj
                        yev_get_fd(yev_event),
                        gbuffer_incref(gbuf_rx)
                    );
                    yev_start_event(yev_response);

                    /*
                     *  Re-arm the read event
                     */
//                    gbuffer_clear(gbuf_rx); // Empty the buffer
//                    yev_start_event(yev_event);

                } else if(yev_state == YEV_ST_STOPPED) {
                    /*
                     *  Bad read
                     *  Disconnected
                     */
                    msg = "Server: Server's client disconnected reading";
                    /*
                     *  Free the message
                     */
                    yev_set_gbuffer(yev_event, NULL);
                    // TODO inform disconnection

                } else {
                    msg = "Server: What?";
                    /*
                     *  Free the message
                     */
                    yev_set_gbuffer(yev_event, NULL);
                }
            }
            break;

        case YEV_WRITE_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    /*
                     *  Write going well
                     *  You can advise to someone, ready to more writes.
                     */
                    msg = "Server: Tx ready";
                } else if(yev_state == YEV_ST_STOPPED) {
                    /*
                     *  Cannot send, something went bad
                     *  Disconnected
                     */
                    msg = "Server: Server's client disconnected writing";
                    // TODO
                } else {
                    msg = "Server: What?";
                }

                /*
                 *  Destroy the write event
                 */
                yev_destroy_event(yev_event);
                yev_event = NULL;
            }
            break;

        default:
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING_ERROR,
                "msg",          "%s", "yev_event not implemented",
                "event_type",   "%s", yev_event_type_name(yev_event),
                NULL
            );
            break;
    }

    if(yev_event) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(yev_event));
        gobj_log_warning(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", msg,
            "type",         "%s", yev_event_type_name(yev_event),
            "state",        "%s", yev_get_state_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "result",       "%d", yev_get_result(yev_event),
            "sres",         "%s", (yev_get_result(yev_event)<0)? strerror(-yev_get_result(yev_event)):"",
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

    return ret;
}

/***************************************************************************
 *  yev_loop callback   CLIENT
 ***************************************************************************/
PRIVATE int yev_client_callback(yev_event_h yev_event)
{
    if(!yev_event) {
        /*
         *  It's the timeout
         */
        return -1;  // break the loop
    }

    char *msg = "???";
    int ret = 0;
    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_get_type(yev_event)) {
        case YEV_CONNECT_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    msg = "Client: Connection Accepted";
                } else if(yev_state == YEV_ST_STOPPED) {
                    if(yev_get_result(yev_event) == -125) {
                        msg = "Client: Connect canceled";
                    } else {
                        msg = "Client: Connection Refused";
                    }
                    ret = -1; // break the loop
                } else {
                    msg = "Client: What?";
                    ret = -1; // break the loop
                }
            }
            break;

        case YEV_WRITE_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    /*
                     *  Write going well
                     *  You can advise to someone, ready to more writes.
                     */
                    msg = "Client: Tx ready";
                } else if(yev_state == YEV_ST_STOPPED) {
                    /*
                     *  Cannot send, something went bad
                     *  Disconnected
                     */
                    msg = "Client: Client disconnected writing";
                    // TODO
                } else {
                    msg = "Client: What?";
                }

                /*
                 *  Destroy the write event
                 */
                yev_destroy_event(yev_event);
                yev_event = NULL;
            }
            break;

        case YEV_READ_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    /*
                     *  Data from the server
                     */
                    msg = "Client: Response from the server";
                    gbuffer_t *gbuf = yev_get_gbuf(yev_event);
                    gobj_trace_dump_gbuf(0, gbuf, "Client: Response from the server");

                } else if(yev_state == YEV_ST_STOPPED) {
                    /*
                     *  Bad read
                     *  Disconnected
                     */
                    msg = "Client: Client disconnected reading";
                    // TODO
                } else {
                    msg = "Server: What?";
                }
                ret = -1; // break the loop when the client get their response
            }
            break;

        default:
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBURING_ERROR,
                "msg",          "%s", "yev_event not implemented",
                "event_type",   "%s", yev_event_type_name(yev_event),
                NULL
            );
            break;
    }

    if(yev_event) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(yev_event));
        gobj_log_warning(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", msg,
            "type",         "%s", yev_event_type_name(yev_event),
            "state",        "%s", yev_get_state_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "result",       "%d", yev_get_result(yev_event),
            "sres",         "%s", (yev_get_result(yev_event)<0)? strerror(-yev_get_result(yev_event)):"",
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            NULL
        );
        json_decref(jn_flags);
    }

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
        yev_loop_callback,  // process timeouts of loop
        &yev_loop
    );

    /*--------------------------------*
     *      Create listen
     *--------------------------------*/
    yev_event_accept = yev_create_accept_event(
        yev_loop,
        yev_server_callback,
        server_url,     // listen_url,
        0,              // backlog,
        FALSE,          // shared
        AF_INET,        // ai_family AF_UNSPEC
        AI_ADDRCONFIG,  // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
        0
    );
    yev_start_event(yev_event_accept);
    yev_loop_run(yev_loop, 1);

    /*--------------------------------*
     *      Create connect
     *--------------------------------*/
    yev_event_connect = yev_create_connect_event(
        yev_loop,
        yev_client_callback,
        server_url,     // listen_url,
        NULL,           // src_url, only host:port
        AF_INET,        // ai_family AF_UNSPEC
        AI_ADDRCONFIG,  // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
        0
    );
    yev_start_event(yev_event_connect);

    /*--------------------------------*
     *  Process ring queue
     *  Server accept the connection - re-arm
     *  Client connected, break the loop
     *--------------------------------*/
    yev_loop_run(yev_loop, 1);

    /*----------------------------------------------------------*
     *  CLIENT: On client connected, it transmits a message
     *---------------------------------------------------------*/
    yev_event_h yev_client_reader_msg = 0;
    if(yev_get_state(yev_event_connect) == YEV_ST_IDLE) {
        /*
         *  If connected, create the message to send.
         *  And set a read event to receive the response.
         */
        gobj_info_msg(0, "client: send request");
        json_t *message = json_string(MESSAGE);
        yev_event_h yev_client_msg = 0;
        gbuffer_t *gbuf = json2gbuf(0, message, JSON_ENCODE_ANY);
        yev_client_msg = yev_create_write_event(
            yev_loop,
            yev_client_callback,
            NULL,   // gobj
            yev_get_fd(yev_event_connect),
            gbuf
        );
        yev_start_event(yev_client_msg);

        /*
         *  Setup a reader yevent
         */
        gbuf = gbuffer_create(1024, 1024);
        yev_client_reader_msg = yev_create_read_event(
            yev_loop,
            yev_client_callback,
            NULL,   // gobj
            yev_get_fd(yev_event_connect),
            gbuf
        );
        yev_start_event(yev_client_reader_msg);
    }

    /*---------------------------------------*
     *  SERVER: The server echo the message
     *---------------------------------------*/
    yev_event_h yev_server_reader_msg = 0;
    if(yev_get_state(yev_event_accept) == YEV_ST_IDLE ||
            yev_get_state(yev_event_accept) == YEV_ST_RUNNING  // Can be RUNNING if re-armed
        ) {
        /*
         *  Server connected: create and setup a read event to receive the messages of client.
         */
        /*
         *  Setup a reader yevent
         */
        gbuffer_t *gbuf = gbuffer_create(1024, 1024);
        yev_server_reader_msg = yev_create_read_event(
            yev_loop,
            yev_server_callback,
            NULL,   // gobj
            yev_get_result(yev_event_accept), //srv_cli_fd,
            gbuf
        );
        yev_start_event(yev_server_reader_msg);
    }

    /*--------------------------------*
     *  Process ring queue
     *--------------------------------*/
    yev_loop_run(yev_loop, 1);

    /*---------------------------------------------------------*
     *  The client matchs the received message with the sent.
     *---------------------------------------------------------*/
    gbuffer_t *gbuf = yev_get_gbuf(yev_client_reader_msg);
    json_t *msg = gbuf2json(gbuffer_incref(gbuf), TRUE);
    const char *text = json_string_value(msg);

    if(strcmp(text, MESSAGE)!=0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "Messages tx and rx don't macthc");
        print_track_mem();
        result += -1;
    }
    json_decref(msg);

    /*--------------------------------*
     *  Stop connect event: disconnected
     *  Stop accept event:
     *--------------------------------*/
    yev_stop_event(yev_client_reader_msg);
    yev_stop_event(yev_server_reader_msg);
    yev_stop_event(yev_event_connect);
    yev_stop_event(yev_event_accept);
    yev_loop_run(yev_loop, 1);

    yev_destroy_event(yev_client_reader_msg);
    yev_destroy_event(yev_server_reader_msg);
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
        NULL   // global_authenticate_parser
    );

    yuno_catch_signals();

    // gobj_set_gobj_trace(0, "liburing", true, 0);

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
    json_t *error_list = json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",  // error_list
        "msg", "Client: Connection Accepted",
        "msg", "Server: Listen Connection Accepted",
        "msg", "client: send request",
        "msg", "Server: Message from the client",
        "msg", "Client: Response from the server",
        "msg", "Server: Listen socket failed or stopped"
    );

    set_expected_results( // Check that no logs happen
        test,   // test name
        error_list,  // error_list
        NULL,  // expected
        NULL,   // ignore_keys
        TRUE    // verbose
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
