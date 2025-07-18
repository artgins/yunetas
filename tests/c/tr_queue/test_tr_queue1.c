/****************************************************************************
 *          test_tr_queue1.c
 *          Copyright (c) 2018 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <locale.h>
#include <signal.h>
#include <time.h>
#include <yunetas.h>

#define APP "test_tr_queue1"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define DATABASE    "tr_queue"
#define TOPIC_NAME  "topic_queue1"

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;

/***************************************************************************
 *
 ***************************************************************************/
static int test(tr_queue_t *trq_msgs, int caso)
{
    int result = 0;

    /*-------------------------------------*
     *  Loop
     *-------------------------------------*/
    switch(caso) {
    case 1:
        {
            const char *test_name = "case 1";
            int cnt = 3600*24*2 + 60;    // 172860 = two day + 1 minute
            json_int_t initial_time = 946684799; // 1999-12-31T23:59:59+0000
            const char *key = "00001";

            json_t *error_list = json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s}]", // error's list
                "msg", "Backup timeranger topic, moving",
                "msg", "Creating topic",
                "msg", "Creating topic_desc.json",
                "msg", "Creating topic_cols.json",
                "msg", "Creating topic_var.json"
            );

            set_expected_results( // Check that no logs happen
                test_name, // test name
                error_list,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            for(int i=0; i<cnt; i++) {
                json_int_t t = i + initial_time;
                json_t *kw = json_pack("{s:s, s:I, s:s}",
                    "id", key,
                    "tm", t,
                    "event", "trace"
                );

                /*
                 *  Enqueue
                 */
                q_msg_t *msg = trq_append2(
                    trq_msgs,
                    t,
                    kw
                );

                /*
                 *  Dequeue
                 */
                uint64_t rowid = trq_msg_rowid(msg);
                uint64_t __t__ = trq_msg_time(msg);
                md2_record_ex_t *md_record_ex = trq_msg_md(msg);
                uint64_t md_rowid = md_record_ex->rowid;

                q_msg_t *msg2 = trq_get_by_rowid(trq_msgs, rowid);
                trq_unload_msg(msg2, 0);

                if(trq_check_pending_rowid(
                    trq_msgs,
                    __t__,
                    md_rowid
                )!=0) {
                    gobj_log_error(0, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "Message not found in the queue",
                        "rowid",        "%ld", (unsigned long)rowid,
                        "md_rowid",     "%ld", (unsigned long)md_rowid,
                        NULL
                    );
                }

                if(trq_size(trq_msgs)==0) {
                    // Check and do backup only when no message
                    trq_check_backup(trq_msgs);
                }
            }

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL);  // NULL: we want to check only the logs
        }
        break;

    default:
        printf("MIERDA\n");
        result += -1;
    }

    return result;
}

/***************************************************************************
 *              Test
 *  Open as master, check main files, add records, open rt lists
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
int do_test(void)
{
    int result = 0;

    /*
     *  Write the tests in ~/tests_yuneta/
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    char path_topic[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    build_path(path_topic, sizeof(path_topic), path_database, TOPIC_NAME, NULL);

    /*-------------------------------------------------*
     *      Startup the timeranger db
     *-------------------------------------------------*/
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", 0
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);

    /*------------------------------*
     *  Open the queue
     *------------------------------*/
    tr_queue_t *trq_msgs = trq_open(
        tranger,
        "gate_events",
        "tm",
        0,
        100000
    );

    /*------------------------------*
     *  Ejecuta los tests
     *------------------------------*/
    result += test(trq_msgs, 1);

    /*------------------------------*
     *  Close the queue
     *------------------------------*/
    trq_close(trq_msgs);

    /*------------------------------*
     *  Check the topic size
     *------------------------------*/
    uint64_t qsize = tranger2_topic_size(
        tranger,
        "gate_events"
    );
    if(qsize != 72860) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "topic_size not 72860", Color_Off);
        result += -1;
    }

    /*-------------------------------*
     *      Shutdown timeranger
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_shutdown", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    tranger2_shutdown(tranger);
    result += test_json(NULL);  // NULL: we want to check only the logs

    return result;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

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

//    gobj_set_deep_tracing(2);           // TODO TEST
//    gobj_set_global_trace(0, true);     // TODO TEST

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gbmem_setup(
        256*1024L,          // max_block, largest memory block
        1024*1024*1024L,    // max_system_memory, maximum system memory
        FALSE,
        0,
        0
    );
    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL    // global_authenticate_parser
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

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
        NULL,
        &yev_loop
    );

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    int result = do_test();
    result += global_result;

    /*--------------------------------*
     *  Stop the event loop
     *--------------------------------*/
    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
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
