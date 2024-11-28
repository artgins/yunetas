/****************************************************************************
 *          perf_yev_ping_pong2
 *
 *  Same as perf_yev_ping_pong plus save messages in tranger2 (test_topic_pkey_integer_iterator6)
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
#include <kwid.h>
#include <timeranger2.h>
#include <testing.h>
#include <ansi_escape_codes.h>
#include <stacktrace_with_backtrace.h>
#include <yev_loop.h>

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE    "tr_topic_pkey_integer"
#define TOPIC_NAME  "topic_pkey_integer_ping_pong"

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

PRIVATE int global_result = 0;
PRIVATE json_t *tranger2 = 0;
PRIVATE json_t *list = 0;

/***************************************************************************
 *  TIMERANGER
 ***************************************************************************/
PRIVATE int rt_disk_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record      // must be owned
)
{
    system_flag2_t system_flag = md_record->system_flag;
    if(system_flag & sf_loading_from_disk) {
    }

    JSON_DECREF(record)
    return 0;
}

/***************************************************************************
 *  yev_loop callback
 ***************************************************************************/
PRIVATE int yev_loop_callback(yev_event_t *yev_event) {
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
PRIVATE int yev_server_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;
    int ret = 0;

    if(dump) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_event->flag);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "💥 yev server callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "result",       "%d", yev_event->result,
            "sres",         "%s", (yev_event->result<0)? strerror(-yev_event->result):"",
            "flag",         "%j", jn_flags,
            "p",            "%p", yev_event,
            NULL
        );
        json_decref(jn_flags);
    }

    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_event->type) {
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
                yev_set_gbuffer(yev_server_tx, gbuf_server_tx);
                yev_start_event(yev_server_tx);

                /*
                 *  Clear buffer
                 *  Re-arm read
                 */
                gbuffer_clear(yev_event->gbuf);
                yev_set_gbuffer(yev_server_rx, yev_event->gbuf);
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
                        yev_event->yev_loop,
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
PRIVATE int yev_client_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;
    int ret = 0;

    if(dump) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_event->flag);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "💥 yev client callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "result",       "%d", yev_event->result,
            "sres",         "%s", (yev_event->result<0)? strerror(-yev_event->result):"",
            "flag",         "%j", jn_flags,
            "p",            "%p", yev_event,
            NULL
        );
        json_decref(jn_flags);
    }

    yev_state_t yev_state = yev_get_state(yev_event);
    switch(yev_event->type) {
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
                    gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "Client receiving");
                }

                json_t *jn_record = gbuf2json(gbuffer_incref(yev_event->gbuf), TRUE);
                gbuffer_reset_rd(yev_event->gbuf);
                md2_record_ex_t md_record;
                tranger2_append_record(tranger2, TOPIC_NAME, 0, 0, &md_record, jn_record);

                gbuffer_clear(gbuf_client_tx);
                gbuffer_append_gbuf(gbuf_client_tx, yev_event->gbuf);

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
                gbuffer_clear(yev_event->gbuf);
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
                        yev_event->fd,
                        0
                    );
                }
                yev_set_gbuffer(yev_client_rx, gbuf_client_rx);
                yev_start_event(yev_client_rx);

                /*
                 *  Transmit
                 */
                if(!gbuf_client_tx) {
                    json_t *jn_tx = json_pack("{s:s}",
                        "hello", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    );
                    gbuf_client_tx = json2gbuf(0, jn_tx, 0);
                    gbuffer_setlabel(gbuf_client_tx, "client-tx");
                }

                if(!yev_client_tx) {
                    yev_client_tx = yev_create_write_event(
                        yev_event->yev_loop,
                        yev_client_callback,
                        NULL,
                        yev_event->fd,
                        0
                    );
                }

                /*
                 *  Transmit
                 */
                if(dump) {
                    gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "Client transmitting");
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
 *
 ***************************************************************************/
PRIVATE json_t *open_tranger(void)
{
    global_result = 0;

    /*
     *  Write the tests in ~/tests_yuneta/
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    char path_topic[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    build_path(path_topic, sizeof(path_topic), path_database, TOPIC_NAME, NULL);

    /*-------------------------------------------------*
     *      Startup the timeranger db
     *-------------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_startup", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", 0
    );
    tranger2 = tranger2_startup(0, jn_tranger, yev_loop);
    global_result += test_json(NULL);  // NULL: we want to check only the logs

    json_t *match_cond = json_pack("{s:b, s:i, s:I}",
        "backward", 0,
        "from_rowid", -10,
        "load_record_callback", (json_int_t)(size_t)rt_disk_record_callback
    );

    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%s/%s",
        kw_get_str(0, tranger2, "directory", "", KW_REQUIRED),
        TOPIC_NAME    // topic name
    );
    if(is_directory(directory)) {
        rmrdir(directory);
    }

    /*-------------------------------------------------*
     *      Create a topic
     *-------------------------------------------------*/
    set_expected_results(
        "create topic", // test name
        json_pack("[{s:s},{s:s},{s:s},{s:s}]", // error's list
            "msg", "Creating topic",
            "msg", "Creating topic_desc.json",
            "msg", "Creating topic_cols.json",
            "msg", "Creating topic_var.json"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    tranger2_create_topic(
        tranger2,
        TOPIC_NAME,     // topic name
        "id",           // pkey
        "tm",           // tkey
        json_pack("{s:i, s:s, s:i, s:i}", // jn_topic_desc
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        sf_int_key,  // system_flag
        json_pack("{s:s, s:I, s:s}", // jn_cols, owned
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
    global_result += test_json(NULL);  // NULL: we want to check only the logs

    list = tranger2_open_list(
        tranger2,
        TOPIC_NAME,
        match_cond,             // match_cond, owned
        NULL,                   // extra
        "",                     // rt_id
        FALSE,                  // rt_by_disk
        NULL                    // creator
    );

    return tranger2;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_tranger(json_t *tranger)
{
    int result = 0;
    /*-------------------------*
     *  close
     *-------------------------*/
    printf("\n\n\n\n");
    set_expected_results( // Check that no logs happen
        "tranger2_close_iterator", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    tranger2_close_list(tranger, list);
    result += test_json(NULL);  // NULL: we want to check only the logs

    /*-------------------------------*
     *      Shutdown timeranger
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger_shutdown", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    result += debug_json("tranger", tranger, FALSE);

    tranger2_shutdown(tranger);
    result += test_json(NULL);  // NULL: we want to check only the logs

    return result;
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
    yev_event_t *yev_server_accept = yev_create_accept_event(
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
    yev_event_t *yev_client_connect = yev_create_connect_event(
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
     *      TRANGER
     *--------------------------------*/
    open_tranger();

    /*--------------------------------*
     *      Begin run loop
     *--------------------------------*/
    t = start_msectimer(1000);
    result += yev_loop_run(yev_loop, 10);

    /*--------------------------------*
     *      TRANGER
     *--------------------------------*/
    close_tranger(tranger2);

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

    /*-------------------------------------*
     *      Check memory loss
     *-------------------------------------*/
    unsigned long memory_check_list[] = {0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    /*-------------------------------------*
     *  Captura salida logger very early
     *-------------------------------------*/
    glog_init();
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    //gobj_set_deep_tracing(2);
    //gobj_set_global_trace(0, TRUE);

    // gobj_set_gobj_trace(0, "liburing", TRUE, 0);
    // gobj_set_gobj_trace(0, "fs", TRUE, 0);

    /*--------------------------------*
     *      Startup gobj
     *--------------------------------*/
    result += gobj_start_up(
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

    //gobj_set_gobj_trace(0, "liburing", TRUE, 0);

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    const char *test = "yev_ping_pong2";
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

    gobj_end();

    result += global_result;

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
    yev_loop->running = 0;
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
