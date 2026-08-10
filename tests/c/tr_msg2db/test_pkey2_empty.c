/****************************************************************************
 *          test_pkey2_empty.c
 *
 *  A store must not take what it cannot give back.
 *
 *  msg2db_append_message() used to accept a record whose pkey2 value was
 *  empty -- it only asked whether the KEY was there -- while the load side
 *  asks for a non-empty VALUE. So `"alarm": ""` was written, stored, and
 *  dropped at every load for the life of the store. A client node was
 *  carrying 4447 of them, written across a fortnight and unreadable ever
 *  since, and saying so 4447 times in its log at every start.
 *
 *  The write side refuses them now, which is what this test pins: the record
 *  is rejected where the caller can still be told, the log says so, and
 *  nothing reaches the disk. The reload then finds only what it can read.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <signal.h>
#include <yunetas.h>

#define APP             "test_pkey2_empty"
#define DATABASE        "tr_msg2db_test"
#define MSG2DB_NAME     "msg2db_test"
#define TOPIC_NAME      "alarms"

#define GOOD_RECORDS    3
#define BAD_RECORDS     5   /* > 1, so the difference between "one line each"
                               and "one line plus a tally" is visible */

PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;

PRIVATE char msg2db_schema[]= "\
{                                                                   \n\
    'id': '"MSG2DB_NAME"',                                          \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': '"TOPIC_NAME"',                                   \n\
            'pkey': 'id',                                           \n\
            'tkey': 'tm',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'pkey2': 'alarm',                                       \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Device Id',                          \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'tm': {                                             \n\
                    'header': 'Time',                               \n\
                    'type': 'integer',                              \n\
                    'flag': ['persistent','required','time']        \n\
                },                                                  \n\
                'alarm': {                                          \n\
                    'header': 'Alarm',                              \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'Description',                        \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *  Append GOOD_RECORDS with a pkey2, and BAD_RECORDS with an empty one.
 *
 *  The index is {id: {pkey2: node}} -- one node per pair -- so the good ones
 *  carry a distinct alarm each, to land as distinct nodes.
 ***************************************************************************/
PRIVATE int fill_topic(json_t *tranger)
{
    for(int i = 0; i < GOOD_RECORDS; i++) {
        char alarm_name[32];
        snprintf(alarm_name, sizeof(alarm_name), "temperature-%d", i);
        json_t *msg = json_pack("{s:s, s:I, s:s, s:s}",
            "id", "device-1",
            "tm", (json_int_t)(1700000000 + i),
            "alarm", alarm_name,
            "description", "a record that can be loaded back"
        );
        msg2db_append_message(tranger, MSG2DB_NAME, TOPIC_NAME, msg, "");
    }

    for(int i = 0; i < BAD_RECORDS; i++) {
        json_t *msg = json_pack("{s:s, s:I, s:s, s:s}",
            "id", "device-2",
            "tm", (json_int_t)(1700001000 + i),
            "alarm", "",
            "description", "refused: a store must not take what it cannot give back"
        );
        msg2db_append_message(tranger, MSG2DB_NAME, TOPIC_NAME, msg, "");
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];

    if(!(getenv("YUNETA_STORE") && strlen(getenv("YUNETA_STORE")) > 0)) {
        snprintf(path_root, sizeof(path_root), "/tmp");
    } else {
        snprintf(path_root, sizeof(path_root), "%s", getenv("YUNETA_STORE"));
    }
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    /*
     *  tranger2_startup() OWNS the config, so it is built fresh for each of
     *  the two runs. Handing it the same object twice with an incref leaks
     *  it: startup keeps a reference that shutdown does not give back.
     */
    #define TRANGER_CONFIG() json_pack("{s:s, s:s, s:b, s:i}", \
        "path", path_root, \
        "database", DATABASE, \
        "master", 1, \
        "on_critical_error", 0 \
    )

    /*-------------------------------------------------------*
     *  First run: create the topic and fill it.
     *
     *  The empty pkey2 is accepted here without a word, which
     *  is the asymmetry this test documents.
     *-------------------------------------------------------*/
    /*
     *  Creating the database says so, and then every bad record is refused
     *  out loud -- one line each, because each one is a caller being told
     *  that its message did not make it.
     */
    json_t *expected_fill = json_pack("[{s:s}, {s:s}]",
        "msg", "Creating __timeranger2__.json",
        "msg", "Creating topic"
    );
    for(int i = 0; i < BAD_RECORDS; i++) {
        json_array_append_new(expected_fill,
            json_pack("{s:s}", "msg", "Field 'pkey2' required, message NOT saved")
        );
    }
    set_expected_results("fill", expected_fill, NULL, NULL, TRUE);

    json_t *tranger = tranger2_startup(0, TRANGER_CONFIG(), yev_loop);
    helper_quote2doublequote(msg2db_schema);
    json_t *jn_schema = legalstring2json(msg2db_schema, TRUE);
    msg2db_open_db(tranger, MSG2DB_NAME, jn_schema, "");

    fill_topic(tranger);

    msg2db_close_db(tranger, MSG2DB_NAME);
    tranger2_shutdown(tranger);

    result += test_json(NULL);

    /*-------------------------------------------------------*
     *  Second run: load it back.
     *
     *  Nothing to complain about: what could not be read was
     *  never written.
     *-------------------------------------------------------*/
    set_expected_results("load", json_array(), NULL, NULL, TRUE);

    tranger = tranger2_startup(0, TRANGER_CONFIG(), yev_loop);
    helper_quote2doublequote(msg2db_schema);
    json_t *jn_schema2 = legalstring2json(msg2db_schema, TRUE);
    msg2db_open_db(tranger, MSG2DB_NAME, jn_schema2, "");

    /*
     *  The good ones are there, and only the good ones: the refused records
     *  left nothing behind to load.
     */
    json_t *messages = msg2db_list_messages(
        tranger,
        MSG2DB_NAME,
        TOPIC_NAME,
        0,      // jn_ids, owned: all ids
        0,      // jn_filter, owned
        0       // match_fn
    );
    size_t loaded = json_array_size(messages);
    if(loaded != GOOD_RECORDS) {
        printf("%sERROR --> %s loaded %d messages, expected %d\n",
            On_Red BWhite, Color_Off, (int)loaded, GOOD_RECORDS
        );
        result += -1;
    }
    JSON_DECREF(messages)

    msg2db_close_db(tranger, MSG2DB_NAME);
    tranger2_shutdown(tranger);

    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    yev_loop_reset_running(yev_loop);
}

PRIVATE void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;
    signal(SIGPIPE, SIG_IGN);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigaction(SIGALRM, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gbmem_setup(
        256*1024L,
        1024*1024*1024L,
        FALSE,
        0,
        0
    );
    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    yuno_catch_signals();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    yev_loop_create(0, 2024, 10, NULL, &yev_loop);

    int result = do_test();
    result += global_result;

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }
    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result < 0 ? -1 : 0;
}
