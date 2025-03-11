/****************************************************************************
 *          test_yevent_traffic_secure1.c
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
#define APP "test_yevent_traffic_secure1"

#include <string.h>
#include <signal.h>
#include <kwid.h>
#include <gobj.h>
#include <ytls.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <yev_loop.h>

/***************************************************************
 *              Constants
 ***************************************************************/
const char *server_url = "tcps://localhost:3333";
#define MESSAGE "AaaaaaaaaaaaaaaaBbbbbbbbbbbbbbb2"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);
PRIVATE int yev_client_callback(yev_event_h yev_event);
PRIVATE int yev_server_callback(yev_event_h yev_event);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
yev_event_h yev_event_accept;
yev_event_h yev_event_connect;

yev_event_h yev_server_reader_msg = 0;
yev_event_h yev_client_reader_msg = 0;

int result = 0;

hytls ytls_server;
hsskt sskt_server;

hytls ytls_client;
hsskt sskt_client;

BOOL server_secure_connected = false;
BOOL client_secure_connected = false;

/***************************************************************************
 *              Test
 ***************************************************************************/
int send_clear_data(hytls ytls, hsskt sskt, gbuffer_t *gbuf)
{
    if(ytls_encrypt_data(ytls, sskt, gbuf)<0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "ytls_encrypt_data() FAILED",
            "error",        "%s", ytls_get_last_error(ytls, sskt),
            NULL
        );
        return -1;
    }
    if(gbuffer_leftbytes(gbuf) > 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "NEED a queue, NOT ALL DATA being encrypted",
            NULL
        );
    }
    return 0;
}

/***************************************************************************
 *  YTLS callbacks
 ***************************************************************************/
int ytls_server_on_handshake_done_callback(void *user_data, int error)
{
    // int fd = (int)user_data;

    server_secure_connected = true;
    gobj_info_msg(0, "Server: secure connected");

//    priv->inform_disconnection = true;
//    priv->secure_connected = true;
//
//    gobj_change_state(gobj, "ST_CONNECTED");
//
//    json_t *kw_ev = json_pack("{s:s, s:s}",
//        "peername", priv->peername,
//        "sockname", priv->sockname
//    );
//    gobj_publish_event(gobj, priv->connected_event_name, kw_ev);

    ytls_flush(ytls_server, sskt_server);

    return 0;
}

int ytls_client_on_handshake_done_callback(void *user_data, int error)
{
    // int fd = (int)user_data;

    client_secure_connected = true;
    gobj_info_msg(0, "Client: secure connected");

//    priv->inform_disconnection = true;
//    priv->secure_connected = true;
//
//    gobj_change_state(gobj, "ST_CONNECTED");
//
//    json_t *kw_ev = json_pack("{s:s, s:s}",
//        "peername", priv->peername,
//        "sockname", priv->sockname
//    );
//    gobj_publish_event(gobj, priv->connected_event_name, kw_ev);

    ytls_flush(ytls_client, sskt_client);

    return 0;
}

int ytls_server_on_clear_data_callback(void *user_data, gbuffer_t *gbuf)
{
    // int fd = (int)user_data;

//    json_t *kw = json_pack("{s:I}",
//        "gbuffer", (json_int_t)(size_t)gbuf
//    );
//    gobj_publish_event(gobj, priv->rx_data_event_name, kw);
    gobj_info_msg(0, "Server: query from the client");
    gobj_trace_dump_gbuf(0, gbuf, "Server: query from the client");
    // gbuffer_decref(gbuf); re-use


    /*
     *  Do ECHO
     */
    send_clear_data(ytls_server, sskt_server, gbuf);

    return 0;
}

int ytls_server_on_encrypted_data_callback(void *user_data, gbuffer_t *gbuf)
{
    int fd = (int)(size_t)user_data;

//    json_t *kw = json_pack("{s:I}",
//        "gbuffer", (json_int_t)(size_t)gbuf
//    );
//    gobj_send_event(gobj, "EV_SEND_ENCRYPTED_DATA", kw, gobj);

    yev_event_h yev_tx_msg = yev_create_write_event(
        yev_loop,
        yev_server_callback,
        NULL,   // gobj
        fd,
        gbuf  // owned
    );
    yev_start_event(yev_tx_msg);

    return 0;
}

int ytls_client_on_clear_data_callback(void *user_data, gbuffer_t *gbuf)
{
    // int fd = (int)user_data;

    //    json_t *kw = json_pack("{s:I}",
//        "gbuffer", (json_int_t)(size_t)gbuf
//    );
//    gobj_publish_event(gobj, priv->rx_data_event_name, kw);

    gobj_info_msg(0, "Client: response from the server");
    gobj_trace_dump_gbuf(0, gbuf, "Client: response from the server");

    /*---------------------------------------------------------*
     *  The client matchs the received message with the sent.
     *---------------------------------------------------------*/
    json_t *msg = gbuf2json(gbuffer_incref(gbuf), true);
    const char *text = json_string_value(msg);

    if(strcmp(text, MESSAGE)!=0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "Messages tx and rx don't macthc");
        print_track_mem();
        result += -1;
    } else {
        gobj_info_msg(0, "Client and Server messages MATCH");
    }
    json_decref(msg);

    gbuffer_decref(gbuf);
    return 0;
}

int ytls_client_on_encrypted_data_callback(void *user_data, gbuffer_t *gbuf)
{
    int fd = (int)(size_t)user_data;

//    json_t *kw = json_pack("{s:I}",
//        "gbuffer", (json_int_t)(size_t)gbuf
//    );
//    gobj_send_event(gobj, "EV_SEND_ENCRYPTED_DATA", kw, gobj);

    yev_event_h yev_tx_msg = yev_create_write_event(
        yev_loop,
        yev_client_callback,
        NULL,   // gobj
        fd,
        gbuf //owned
    );
    yev_start_event(yev_tx_msg);

    return 0;
}

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

    int ret = 0;
    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_get_type(yev_event)) {
        case YEV_ACCEPT_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    /*---------------------------------------*
                     *  SERVER: setup read event
                     *---------------------------------------*/
                    gbuffer_t *gbuf = gbuffer_create(1024, 1024);
                    yev_server_reader_msg = yev_create_read_event(
                        yev_loop,
                        yev_server_callback,
                        NULL,   // gobj
                        yev_get_result(yev_event_accept), //srv_cli_fd,
                        gbuf
                    );
                    yev_start_event(yev_server_reader_msg);

                    // Set connected, but not secure-connected in ytls_server_on_handshake_done_callback
                    sskt_server = ytls_new_secure_filter(
                        ytls_server,
                        ytls_server_on_handshake_done_callback,
                        ytls_server_on_clear_data_callback,
                        ytls_server_on_encrypted_data_callback,
                        (void *)(size_t)yev_get_result(yev_event_accept)
                    );
                    if(!sskt_server) {
                        //msg = "Server: Bad ytls";
                        //log_error()
                        ret = -1; // break the loop
                    }

                } else {
                    // Listening stopped
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
                    //msg = "Server: Tx ready";
                } else {
                    /*
                     *  Cannot send, something went bad
                     *  Disconnected
                     */
                    //msg = strerror(yev_get_result(yev_event));
                    //msg = "Server: Server's client disconnected writing";
                    ret = -1; // break the loop
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
                     *  Data from the client
                     */
                    //msg = "Server: Message from the client";
                    gbuffer_t *gbuf_rx = yev_get_gbuf(yev_event);

                    gobj_trace_dump_gbuf(0, gbuf_rx, "Server: Query encrypted from the client");

                    /*
                     *  Server: Process the message
                     *  First decrypt if is SSL
                     *  The clear data will be returned in on_clear_data_callback callback!! TODO
                     */
                    if(ytls_decrypt_data(ytls_server, sskt_server, gbuffer_incref(gbuf_rx))<0) {
                        gobj_log_error(0, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_LIBURING_ERROR,
                            "msg",          "%s", "ytls_decrypt_data() FAILED",
                            NULL
                        );
                    }

                    /*
                     *  Re-arm the read event
                     */
                    gbuffer_clear(gbuf_rx); // Empty the buffer
                    yev_start_event(yev_event);

                } else {
                    /*
                     *  Bad read
                     *  Disconnected
                     */
                    //msg = strerror(yev_get_result(yev_event));
                    //msg = "Server: Server's client disconnected reading";
                    /*
                     *  Free the message
                     */
                    yev_set_gbuffer(yev_event, NULL);

                    ret = -1; // break the loop
                }
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

    int ret = 0;
    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_get_type(yev_event)) {
        case YEV_CONNECT_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    /*----------------------------------------------------------*
                     *  CLIENT: setup reader event
                     *---------------------------------------------------------*/
                    gbuffer_t *gbuf_rx = gbuffer_create(1024, 1024);
                    yev_client_reader_msg = yev_create_read_event(
                        yev_loop,
                        yev_client_callback,
                        NULL,   // gobj
                        yev_get_fd(yev_event_connect),
                        gbuf_rx
                    );
                    yev_start_event(yev_client_reader_msg);

                    //msg = "Client: Connection Accepted";
                    // Set connected
                    sskt_client = ytls_new_secure_filter(
                        ytls_client,
                        ytls_client_on_handshake_done_callback,
                        ytls_client_on_clear_data_callback,
                        ytls_client_on_encrypted_data_callback,
                        (void *)(size_t)yev_get_fd(yev_event_connect)
                    );
                    if(!sskt_client) {
                        //msg = "Client: Bad ytls";
                        //log_error()
                        ret = -1; // break the loop
                    }

                } else {
                    if(yev_get_result(yev_event) == -125) {
                        //msg = "Client: Connect canceled";
                    } else {
                        //msg = "Client: Connection Refused";
                    }
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
                    //msg = "Client: Tx ready";
                } else {
                    /*
                     *  Cannot send, something went bad
                     *  Disconnected
                     */
                    //msg = strerror(yev_get_result(yev_event));
                    //msg = "Client: Client disconnected writing";
                    ret = -1; // break the loop
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
                    //msg = "Client: Response from the server";
                    gbuffer_t *gbuf_rx = yev_get_gbuf(yev_event);

                    gobj_trace_dump_gbuf(0, gbuf_rx, "Client: Response encrypted from the server");

                    /*
                     *  Client: Process the response
                     *  First decrypt if is SSL
                     *  The clear data will be returned in on_clear_data_callback callback!! TODO
                     */
                    if(ytls_decrypt_data(ytls_client, sskt_client, gbuffer_incref(gbuf_rx))<0) {
                        gobj_log_error(0, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_LIBURING_ERROR,
                            "msg",          "%s", "ytls_decrypt_data() FAILED",
                            NULL
                        );
                    }

                    /*
                     *  Re-arm the read event
                     */
                    gbuffer_clear(gbuf_rx); // Empty the buffer
                    yev_start_event(yev_event);

                } else {
                    /*
                     *  Bad read
                     *  Disconnected
                     */
                    //msg = strerror(yev_get_result(yev_event));
                    //msg = "Client: Client disconnected reading";
                    /*
                     *  Free the message
                     */
                    yev_set_gbuffer(yev_event, NULL);

                    ret = -1; // break the loop
                }
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

    return ret;
}

/***************************************************************************
 *              Test
 ***************************************************************************/
int do_test(void)
{
    /*--------------------------------*
     *  Create ssl SERVER
     *--------------------------------*/
    json_t *jn_crypto_s = json_pack("{s:s, s:s, s:s, s:b}",
        "library", "openssl",
        "ssl_certificate", "/yuneta/agent/certs/localhost.crt",
        "ssl_certificate_key", "/yuneta/agent/certs/localhost.key",
        "trace", 0
    );
    ytls_server = ytls_init(
        0,
        jn_crypto_s, // not owned
        true
    );
    JSON_DECREF(jn_crypto_s)

    /*--------------------------------*
     *  Create ssl CLIENT
     *--------------------------------*/
    json_t *jn_crypto_c = json_pack("{s:s, s:b}",
        "library", "openssl",
        "trace", 0
    );
    ytls_client = ytls_init(
        0,
        jn_crypto_c, // not owned
        false
    );
    JSON_DECREF(jn_crypto_c)

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
        0
    );
    yev_setup_accept_event(
        yev_event_accept,
        server_url, // listen_url,
        0, //backlog,
        false, // shared
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
        yev_client_callback,
        0
    );
    yev_setup_connect_event(
        yev_event_connect,
        server_url, // listen_url,
        NULL,   // src_url, only host:port
        AF_INET,      // ai_family AF_UNSPEC
        AI_ADDRCONFIG       // ai_flags AI_V4MAPPED | AI_ADDRCONFIG
    );
    yev_start_event(yev_event_connect);

    /*--------------------------------*
     *  Process ring queue
     *--------------------------------*/
    yev_loop_run(yev_loop, 1);

    /*----------------------------------------------------------*
     *  CLIENT: On client connected, it transmits a message
     *---------------------------------------------------------*/
    if(server_secure_connected && client_secure_connected) {
        /*
         *  If both secure connected, send the message
         */
        gobj_info_msg(0, "client: send request");
        json_t *message = json_string(MESSAGE);
        gbuffer_t *gbuf_tx = json2gbuf(0, message, JSON_ENCODE_ANY);
        send_clear_data(ytls_client, sskt_client, gbuf_tx);

        /*--------------------------------*
         *  Process ring queue
         *--------------------------------*/
        yev_loop_run(yev_loop, 1);
    }

    /*--------------------------------*
     *  Stop connect event:
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

    if(sskt_server) {
        ytls_free_secure_filter(ytls_server, sskt_server);
    }
    if(sskt_client) {
        ytls_free_secure_filter(ytls_client, sskt_client);
    }

    EXEC_AND_RESET(ytls_cleanup, ytls_server)
    EXEC_AND_RESET(ytls_cleanup, ytls_client)

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
        NULL,   // global_authenticate_parser
        0,      // max_block, largest memory block
        0,      // max_system_memory, maximum system memory
        false,
        0,
        0
    );

    yuno_catch_signals();

    gobj_set_gobj_trace(0, "liburing", true, 0);

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
    unsigned long memory_check_list[] = {0,0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    const char *test = APP;
    json_t *error_list = json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",  // error_list
          "msg", "addrinfo on listen",
          "msg", "Client: secure connected",
          "msg", "Server: secure connected",
          "msg", "client: send request",
          "msg", "Server: query from the client",
          "msg", "Client: response from the server",
          "msg", "Client and Server messages MATCH"
    );

    set_expected_results( // Check that no logs happen
        test,   // test name
        error_list,  // error_list
        NULL,  // expected
        NULL,   // ignore_keys
        true    // verbose
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
